#
# Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation. Alibaba designates this
# particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#

#!/usr/bin/env bash

#
# To initialize global JGroup (Java wrapper of Linux cgroup) configuration:
# 1, mount required cgroup controllers
# 2, create ROOT cgroup 'ajdk_multi_tenant' for JGroup
# 3, save cgroup configurations to /etc/cgconfig.conf
#

# globals
JG_USER=""
JG_GROUP=""
JG_ROOT=""
NOF_CPUS="0" # at least have 1 cpus (which is "0-0" for cpuset.cpus)
IS_IN_DOCKER_CONTAINER="" # if the script is executed inside docker container, "TRUE" if yes, "" otherwise

# check if all prerequisitions are ready
check_prerequisitions() {
    # needed commandline tools
    REQUIRED_TOOLS=("mountpoint" "mount" "mkdir" "cut" "df" "sed" "getopt" "wc" "awk")
    for REQ in ${REQUIRED_TOOLS[*]}; do
        which $REQ > /dev/null 2>&1
        if [ $? -ne 0 ]; then
            echo "Cannot find prerequisition : $REQ, please install!"
            exit 1
        fi
    done
    # init globals
    NOF_CPUS=$(grep processor /proc/cpuinfo | wc -l)
    # check if we are in DOCKER
    if [ -z "$IS_IN_DOCKER_CONTAINER" ]; then
        if [ ! -z "$(cat /proc/1/cgroup | awk -F/ '{print $2}')" ]; then
            IS_IN_DOCKER_CONTAINER="TRUE"
        fi
    fi
}

#
# reference https://github.com/jpetazzo/dind/
#
cgroup_init() {
    CGROUP=""
    # loop over /proc/mounts, and search for mounted cgroup device
    while read ln; do
        # searching for cgroup record, like:
        # tmpfs /sys/fs/cgroup tmpfs rw,nosuid,nodev,noexec,mode=755 0 0
        DEV=$(echo $ln | awk '{print $3}')
        if [ $DEV = "tmpfs" ]; then
            if [ -n "$(echo $ln | grep cgroup)" ]; then
                CGROUP="$(echo $ln | awk '{print $2}')"
            fi
        fi
    done < /proc/mounts

    # if did not found cgroup mount point, init cgroup mountpoints
    if [ -z "$CGROUP" ]; then
        # use '/cgroup' as DEFAULT mountpoint
        CGROUP="/cgroup"
        # use /sys/fs/cgroup for AliOS7
        uname -r | grep -q alios7 && CGROUP='/sys/fs/cgroup'

        echo "CGroup file system not mounted, trying to mount it to $CGROUP"
        [ -d "$CGROUP" ] || mkdir $CGROUP
        mountpoint -q $CGROUP || {
            echo "Mounting cgroup file system"
            mount -n -t tmpfs -o uid=0,gid=0,mode=0755 cgroup $CGROUP || {
                echo "Could not make a tmpfs mount. Did you use --privileged?"
                exit 1
            }
        }

        # Mount the cgroup hierarchies exactly as they are in the parent system.
        for SUBSYS in $(cut -d: -f2 /proc/1/cgroup)
        do
            [ -d $CGROUP/$SUBSYS ] || mkdir $CGROUP/$SUBSYS
            mountpoint -q $CGROUP/$SUBSYS ||
                    mount -n -t cgroup -o $SUBSYS cgroup $CGROUP/$SUBSYS

            echo $SUBSYS | grep -q ^name= && {
              NAME=$(echo $SUBSYS | sed s/^name=//)
              [ -d $CGROUP/$NAME ] || ln -s $SUBSYS $CGROUP/$NAME
            }

            [ $SUBSYS = cpuacct,cpu ] && [ ! -d $CGROUP/cpu,cpuacct ] && ln -s $SUBSYS $CGROUP/cpu,cpuacct
        done
    fi

    # check if CGROUP file system is writable
    # In kernel 3.10.0-327.13.1.el7.x86_64, `ls` and `-w` returns different results for $CGROUP;
    # but the controller file inside $CGROUP is still writable anyway.
    if [ ! -w $CGROUP ] && [ ! -w $CGROUP/cpu/cpu.shares ]; then
        echo "CGroup file system mounted at $CGROUP is not writable, please check CGroup config!"
        if [ "$IS_IN_DOCKER_CONTAINER" = "TRUE" ]; then
            echo "It looks like to be inside a docker container, did you forget to add '--privileged' option to 'docker run'?"
        fi
        exit 1
    fi

    [ -d $CGROUP/cpu ] || ln -s cpu,cpuacct $CGROUP/cpu
    [ -d $CGROUP/cpuacct ] || ln -s cpu,cpuacct $CGROUP/cpuacct

    # create JDK sub-directories for each controller
    CONTROLLERS=("cpu" "cpuacct" "cpuset")
    for CTRL in ${CONTROLLERS[*]}; do
        if [ ! -z "$JG_ROOT" ] && [ ! -d "$CGROUP/$CTRL/$JG_ROOT" ]; then
            echo "User specified ROOT directory: $CGROUP/$CTRL/$JG_ROOT does not exist"
            exit 1
        fi
        CTRL_DIR="$CGROUP/$CTRL/$JG_ROOT/ajdk_multi_tenant"
        # create directories if needed
        if [ ! -d "$CTRL_DIR" ]; then
            mkdir -p "$CTRL_DIR"
            if [ $? -ne 0 ]; then
                echo "Failed to create JDK group: $CTRL_DIR"
                exit 1
            fi
        fi
        # setting up owners
        chown -R $JG_USER:$JG_GROUP $CTRL_DIR
    done

    # initialize necessary controllers
    cp $CGROUP/cpu/$JG_ROOT/cpu.shares $CGROUP/cpu/$JG_ROOT/ajdk_multi_tenant/cpu.shares
    cp $CGROUP/cpu/$JG_ROOT/cpu.cfs_period_us $CGROUP/cpu/$JG_ROOT/ajdk_multi_tenant/cpu.cfs_period_us
    #
    # https://access.redhat.com/documentation/en-US/Red_Hat_Enterprise_Linux/6/html/Resource_Management_Guide/sec-cpu.html
    # Setting the value in cpu.cfs_quota_us to -1 indicates that the cgroup does not adhere to any CPU time restrictions.
    # This is also the default value for every cgroup
    #
    # No limit on JDK group
    echo "-1" > $CGROUP/cpu/$JG_ROOT/ajdk_multi_tenant/cpu.cfs_quota_us

    #
    # cpuset.cpus and cpuset.mems may be empty, which is normal behavior according to AliOS team,
    # but in that case tasks/threads cannot be attached to that group.
    # Here just check and forcefully initialize it with a default value;
    # see http://aone.alibaba-inc.com/code/D87428 for more details
    #
    # 1) Check if parent group (controller root) has a valid config, initialize parent if needed;
    # 2) Copy parent group's config to /ajdk_multi_tenant
    #
    if [ ! -z "$JG_ROOT" ]; then
        ROOT_CPUSET_CPUS="$CGROUP/cpuset/$JG_ROOT/cpuset.cpus"
    else
        ROOT_CPUSET_CPUS="$CGROUP/cpuset/cpuset.cpus"
    fi
    if [ -z `cat $ROOT_CPUSET_CPUS` ]; then
        echo "0-$NOF_CPUS" > $ROOT_CPUSET_CPUS
        if [ 0 != $? ]; then
            echo "Cannot init ROOT group's config $ROOT_CPUSET_CPUS"
            exit 1
        fi
    fi
    cp $ROOT_CPUSET_CPUS $CGROUP/cpuset/$JG_ROOT/ajdk_multi_tenant/cpuset.cpus

    if [ ! -z "$JG_ROOT" ]; then
        ROOT_CPUSET_MEMS="$CGROUP/cpuset/$JG_ROOT/cpuset.mems"
    else
        ROOT_CPUSET_MEMS="$CGROUP/cpuset/cpuset.mems"
    fi
    if [ -z `cat $ROOT_CPUSET_MEMS` ]; then
        echo "0" > $ROOT_CPUSET_MEMS
        if [ 0 != $? ]; then
            echo "Cannot init ROOT group's config $ROOT_CPUSET_MEMS"
            exit 1
        fi
    fi
    cp $ROOT_CPUSET_MEMS $CGROUP/cpuset/$JG_ROOT/ajdk_multi_tenant/cpuset.mems

    # save current CGroup configuration to /etc/cgconfig.conf
    CONFIG_FILE='/etc/cgconfig.conf'
    which cgsnapshot
    if [ $? -eq 0 ]; then
        cgsnapshot -s -f $CONFIG_FILE
        if [ $? -ne 0 ]; then
            echo "Failed to save cgroup configuration to $CONFIG_FILE"
            exit 1
        fi
    fi
}

# show help information
show_help() {
    echo "Usage: jgroup [-u username] [-g usergroup] [-r root group path] [-h]"
    echo "Initialize the jgroup configuration for multi-tenant JVM (which requires root permission)"
    echo "e.g. jgroup -u admin -g admin"
    echo "e.g. jgroup -u admin -g admin -r /system.slice/dockr-abcdefghijklmnopq123/"
}

# parse arguments
parse_args() {
    while getopts "u:g:r:h" arg; do
        case "$arg"
        in
            u)
                JG_USER="$OPTARG"
                ;;
            g)
                JG_GROUP="$OPTARG"
                ;;
            r)
                JG_ROOT="$OPTARG"
                ;;
            h|?)
                show_help
                exit 1
                ;;
        esac
    done

    # check result of parsing
    if [ -z "$JG_GROUP" ] || [ -z "$JG_USER" ]; then
        echo "Bad parameters: user = $JG_USER, group = $JG_GROUP"
        show_help
        exit 1
    fi

    # check if $JG_USER and $JG_GROUP exists in /etc
    if [ -z "$(cat /etc/passwd | awk -F: '{print $1}' | grep $JG_USER)" ]; then
        echo "User name $JG_USER does not exist in /etc/passwd!"
        exit 1
    fi

    if [ -z "$(cat /etc/group | awk -F: '{print $1}' | grep $JG_GROUP)" ]; then
        echo "Group name $JG_GROUP does not exist in /etc/group!"
        exit 1
    fi
}

# entrypoint
parse_args $*
check_prerequisitions
cgroup_init

# done
exit 0
