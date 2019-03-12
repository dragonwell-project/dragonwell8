#!/bin/bash
#
# Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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

to_stderr() {
    echo "$@" >&2
}

error() {
    to_stderr "ERROR: $1"
    exit ${2:-126}
}

warning() {
    to_stderr "WARNING: $1"
}

subrepos="corba jaxp jaxws langtools jdk hotspot nashorn"

# default options
GITURL="git@gitlab.alibaba-inc.com:dragonwell"
REPO_PREFIX="jdk8u_"
DEPTH=1000
BRANCH="master"

usage() {
      echo "usage: $0 [-h|--help] [-b|--branch branch_name] [-s|--site github|gitlab]"
      exit 1
}

# parse options
while [ $# -gt 0 ]
do
  case $1 in
    -h | --help )
      usage
      ;;

    -b | --branch )
      shift;
      BRANCH=$1
      ;;

    -s | --site )
      shift;
      site=$1
      if [[ $site == "github" ]]; then
        GITURL="git@github.com:alibaba"
        REPO_PREFIX="dragonwell8_"
      elif [[ $site == "gitlab" ]]; then
        # inside alibaba
        GITURL="git@gitlab.alibaba-inc.com:dragonwell"
        REPO_PREFIX="jdk8u_"
      else
        usage
      fi
      ;;

    *)  # unknown option
      ;;
  esac
  shift
done

GIT=`command -v git`
if [ "x$GIT" = "x" ]; then
  error "Could not locate git command"
fi

echo "Check directories"
for repo in ${subrepos}; do
  if [[ -d ${repo} ]]; then
    error "directory ${repo} already exist"
  fi
done

echo "Start clone..."
for repo in ${subrepos}; do
  REPOURL=${GITURL}/${REPO_PREFIX}${repo}.git
  #CLONEOPTS="--depth ${DEPTH}"
  echo ${GIT} clone ${CLONEOPTS} ${REPOURL} ${repo}
  ${GIT} clone ${CLONEOPTS} ${REPOURL} ${repo}
  result=$?
  if [ $result  != 0 ]; then
    error "failed to clone ${repo}"
  fi
  # checkout to specific branch
  if [ $BRANCH != "master" ]; then
    cd ${repo}
    ${GIT} checkout -b ${BRANCH} origin/${BRANCH}
    result=$?
    if [ $result  != 0 ]; then
      error "failed to checkout ${repo} to ${BRANCH}"
    fi
    cd ..
  fi
done
