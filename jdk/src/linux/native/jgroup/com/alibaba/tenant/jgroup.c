//
// Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
// DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
//
// This code is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License version 2 only, as
// published by the Free Software Foundation. Alibaba designates this
// particular file as subject to the "Classpath" exception as provided
// by Oracle in the LICENSE file that accompanied this code.
//
// This code is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// version 2 for more details (a copy is included in the LICENSE file that
// accompanied this code).
//
// You should have received a copy of the GNU General Public License version
// 2 along with this work; if not, write to the Free Software Foundation,
// Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <mntent.h>
#include "jni.h"
#include "jni_util.h"
#include "com_alibaba_tenant_NativeDispatcher.h"

// enum index of cgroup controllers
enum CGroupController {
    CG_CPU,
    CG_CPUSET,
    CG_CPUACCT,
    CG_MAX_SUPPORTED, /* supported by here */
    /* unsupported controllers */
    CG_BLKIO,
    CG_DEVICES,
    CG_FREEZER,
    CG_MEMORY, /* supported by -XX:+TenantHeapThrottling */
    CG_NET_CLS,
    CG_NET_PRIO,
    CG_NS,
    CG_PERF_EVENT
};

// supported controller names, must be access with index < CG_MAX_SUPPORTED
static const char* cg_controller_names[] = {
    "cpu",
    "cpuset",
    "cpuacct",
};

// mountpoints of each cgroup controller, dynamically allocated in initialization,
// and never reclaimed
static const char* cg_mount_points[CG_MAX_SUPPORTED] = { NULL };

// Array to indicate if a controller is enabled
static unsigned char cg_controller_enabled[CG_MAX_SUPPORTED] = { 0 };

// set controller enabling status at startup
#define ENABLE_CONTROLLER(idx)              \
    if (idx < CG_MAX_SUPPORTED) {           \
        cg_controller_enabled[idx] = 1;     \
    }
// returns zero if controller disabled; otherwise non-zero
#define IS_CONTROLLER_ENABLED(idx)      ((idx < CG_MAX_SUPPORTED) ? cg_controller_enabled[idx] : 0)

// To control if extra debugging msg should be printed
static jboolean jgroup_debug = JNI_FALSE;

// Execute statement only in debugging mode
#define JGROUP_DEBUG(stmt)  do {                \
            if (JNI_TRUE == jgroup_debug) {     \
                stmt;                           \
            }                                   \
        } while(0)

// Environment variable name to turn on debugging output of JGroup native code
#define JGROUP_DEBUG_ENV_KEY    "JGROUP_DEBUG"

// logging support
#define JGROUP_LOG(stream, format, ...)  do {           \
            if (jgroup_debug == JNI_TRUE) {             \
                fprintf(stream, format, __VA_ARGS__);   \
            }                                           \
        } while (0)

#define JGROUP_MSG(msg)             JGROUP_LOG(stdout, "[JGROUP][INFO] %s\n", msg)
#define JGROUP_INFO(format, ...)    JGROUP_LOG(stdout, "[JGROUP][INFO]" format "\n", __VA_ARGS__)
#define JGROUP_WARN(format, ...)    JGROUP_LOG(stderr, "[JGROUP][WARN]" format "\n", __VA_ARGS__)
#define JGROUP_ERROR(format, ...)   JGROUP_LOG(stderr, "[JGROUP][ERROR]" format "\n", __VA_ARGS__)

#define SIG_STRING              "Ljava/lang/String;"
#define MAX_PATH_LEN            2048
#define CG_MAX_CTRL_NAME_LEN    128

//--------------------- Utility methods ----------------------

// Returns thread ID of current thread
static pid_t get_tid() {
    return syscall(SYS_gettid);
}

// returns JNI_TRUE if a 'path' is a file (not a directory); otherwise JNI_FALSE
static jboolean
is_file(const char* path) {
    if (path != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(struct stat));
        if (0 == stat(path, &st)) {
            return S_ISREG(st.st_mode) ? JNI_TRUE : JNI_FALSE;
        }
    }
    return JNI_FALSE;
}

//--------------------- JNI impl ----------------------

// Detect if cgroup is enabled by reading /proc/cgroups
static int
detect_cgroup_enabled() {
    char subsys_name[CG_MAX_CTRL_NAME_LEN];
    int res = 0, err = 0;
    int hierarchy = 0, num_cgroups = 0, enabled = 0;
    const char* proc_cgroups_path = "/proc/cgroups";
    int cg_index = 0;
    FILE* proc_cgroups = fopen(proc_cgroups_path, "re");

    if (proc_cgroups != NULL) {
        while (!feof(proc_cgroups)) {
            err = fscanf(proc_cgroups, "%s %d %d %d", subsys_name,
                    &hierarchy, &num_cgroups, &enabled);
            if (err < 0) {
                break;
            }

            for (cg_index = 0; cg_index < CG_MAX_SUPPORTED; ++cg_index) {
                if (!strcmp(subsys_name, cg_controller_names[cg_index]) && enabled) {
                    ENABLE_CONTROLLER(cg_index);
                    JGROUP_INFO("cgroup controller %s is enabled!", cg_controller_names[cg_index]);
                    break;
                }
            }
        }
        JGROUP_INFO("Opening %s to detect enabled controllers", proc_cgroups_path);
    } else {
        JGROUP_ERROR("Failed to open %s", proc_cgroups_path);
        res = -1;
    }

    return res;
}

// Detect cgroup mountpoints
static int
detect_cgroup_mount_points() {
    int res = 0;
    struct mntent *ent = NULL;
    char mntent_buffer[4 * MAX_PATH_LEN];
    int cg_index = 0;
    FILE* proc_mounts = NULL;
    struct mntent* temp_ent = NULL;

    temp_ent = (struct mntent *) malloc(sizeof(struct mntent));
    if (temp_ent == NULL) {
        JGROUP_MSG("Failed to allocate temp struct mntent");
        res = -1;
    } else {
        proc_mounts = fopen("/proc/mounts", "re");
        if (proc_mounts != NULL) {
            while ((ent = getmntent_r(proc_mounts,
                                      temp_ent,
                                      mntent_buffer,
                                      sizeof(mntent_buffer))) != NULL) {
                // skip non-cgroup mounts
                if (strcmp(ent->mnt_type, "cgroup")) {
                    continue;
                }
                for (cg_index = 0; cg_index < CG_MAX_SUPPORTED; ++cg_index) {
                    if (NULL != hasmntopt(ent, cg_controller_names[cg_index])) {
                        cg_mount_points[cg_index] = strdup(ent->mnt_dir);
                        JGROUP_INFO("found mountpoint for %s at %s",
                                    cg_controller_names[cg_index],
                                    cg_mount_points[cg_index]);
                    }
                }
            }
        } else {
            JGROUP_MSG("failed to open /proc/mounts");
            res = -1;
        }

        free(temp_ent);

        // double check
        for (cg_index = 0; cg_index < CG_MAX_SUPPORTED; ++cg_index) {
            if (IS_CONTROLLER_ENABLED(cg_index) && cg_mount_points[cg_index] == NULL) {
                JGROUP_ERROR("Cannot determine mountpoint for controller %s", cg_controller_names[cg_index]);
                res = -1;
            }
        }
    }

    return res;
}

// initialize static fields of class 'com.alibaba.tenant.NativeDispatcher'
// returns 0 on success; non-zero otherwise
static int
init_Java_globals(JNIEnv* env, jclass clazz) {
    int         res                 = 0;
    char*       mp_field_names[]    = { "CG_MP_CPU", "CG_MP_CPUSET", "CG_MP_CPUACCT" };
    size_t      idx                 = 0;
    jfieldID    fid                 = NULL;

    // reflect to Java level
    // initialize mountpoints
    for (idx = 0; idx < CG_MAX_SUPPORTED; ++idx) {
        fid = (*env)->GetStaticFieldID(env, clazz, mp_field_names[idx], SIG_STRING);
        jstring mp = (*env)->NewStringUTF(env, cg_mount_points[idx]);
        if (mp != NULL) {
            (*env)->SetStaticObjectField(env, clazz, fid, mp);
        }
    }

    // initialize flags of if a controller is enabled
    if (IS_CONTROLLER_ENABLED(CG_CPU)) {
        char buf[MAX_PATH_LEN];
        // cpu.shares
        snprintf(buf, MAX_PATH_LEN, "%s/cpu.shares", cg_mount_points[CG_CPU]);
        if (is_file(buf) == JNI_TRUE) {
            fid = (*env)->GetStaticFieldID(env, clazz, "IS_CPU_SHARES_ENABLED", "Z");
            (*env)->SetStaticBooleanField(env, clazz, fid, JNI_TRUE);
        }

        // cpu.cfs_*_us
        snprintf(buf, MAX_PATH_LEN, "%s/cpu.cfs_period_us", cg_mount_points[CG_CPU]);
        if (is_file(buf) == JNI_TRUE) {
            fid = (*env)->GetStaticFieldID(env, clazz, "IS_CPU_CFS_ENABLED", "Z");
            (*env)->SetStaticBooleanField(env, clazz, fid, JNI_TRUE);
        }
        JGROUP_INFO("%s mountpoint=%s", cg_controller_names[CG_CPU], cg_mount_points[CG_CPU]);
    }

    // cpuset.*
    if (IS_CONTROLLER_ENABLED(CG_CPUSET)) {
        fid = (*env)->GetStaticFieldID(env, clazz, "IS_CPUSET_ENABLED", "Z");
        (*env)->SetStaticBooleanField(env, clazz, fid, JNI_TRUE);
        JGROUP_INFO("%s mountpoint=%s", cg_controller_names[CG_CPUSET], cg_mount_points[CG_CPUSET]);
    }

    // cpuacct.*
    if (IS_CONTROLLER_ENABLED(CG_CPUACCT)) {
        fid = (*env)->GetStaticFieldID(env, clazz, "IS_CPUACCT_ENABLED", "Z");
        (*env)->SetStaticBooleanField(env, clazz, fid, JNI_TRUE);
        JGROUP_INFO("%s mountpoint=%s", cg_controller_names[CG_CPUACCT], cg_mount_points[CG_CPUACCT]);
    }

    return res;
}

// invoked once in static initializer
JNIEXPORT jint JNICALL
Java_com_alibaba_tenant_NativeDispatcher_init0(JNIEnv* env, jclass clazz) {
    jint res = 0;

    // Detect and initialize native configuration
    if (0 != (res = detect_cgroup_enabled())) {
        JGROUP_MSG("Failed to detect cgroup status!");
        return res;
    }

    if (0 != (res = detect_cgroup_mount_points())) {
        JGROUP_MSG("Failed to detect cgroup mountpoints!");
        return res;
    }

    // detect debugging options from environment variables
    char* true_str = "TRUE"; // will be compared in uppercase
    char* env_val  = getenv(JGROUP_DEBUG_ENV_KEY);
    if (env_val != NULL && strlen(true_str) == strlen(env_val)) {
        size_t idx = 0;
        jgroup_debug = JNI_TRUE;
        for (; idx < strlen(true_str); ++idx) {
            if (toupper(env_val[idx]) != true_str[idx]) {
                jgroup_debug = JNI_FALSE;
                break;
            }
        }
    }

    JGROUP_INFO("jgroup debugging mode = %s", jgroup_debug == JNI_TRUE ? "true" : "false");

    // initialize cgroup
    if (0 != (res = init_Java_globals(env, clazz))) {
        JGROUP_MSG("Failed to init Java globals!");
    } else {
        JGROUP_MSG("cgroup initialized successfully");
    }

    return res;
}

//
// Create the cgroup dir, and initialize some mandatory values
// @param group_path   relative path of target CGROUP
JNIEXPORT jint JNICALL
Java_com_alibaba_tenant_NativeDispatcher_createCgroup(JNIEnv* env,
                                                       jobject ignored,
                                                       jstring group_path) {
    int res = 0;
    int cg_index = 0;
    struct stat st;
    if (group_path == NULL) {
        JGROUP_MSG("null group path to create");
        res = -1;
    } else {
        const char* path_str = (*env)->GetStringUTFChars(env, group_path, NULL);
        if (path_str != NULL || strlen(path_str) > 0) {
            char path_buf[MAX_PATH_LEN];
            for (cg_index = 0; cg_index < CG_MAX_SUPPORTED; ++cg_index) {
                snprintf(path_buf, MAX_PATH_LEN, "%s/%s", cg_mount_points[cg_index], path_str);

                // if exists, just reuse
                if (stat(path_buf, &st) == 0 && S_ISDIR(st.st_mode)) {
                    JGROUP_INFO("Reuse existing directory %s", path_buf);
                    continue;
                }

                res = mkdir(path_buf, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);  // modes are copied from libcg
                if (res == 0) {
                    JGROUP_INFO("created cgroup dir %s", path_buf);
                } else {
                    JGROUP_ERROR("failed to create dir %s (%s)", path_buf, strerror(errno));
                    break;
                }
            }
            (*env)->ReleaseStringUTFChars(env, group_path, path_str);
        } else {
            JGROUP_MSG("Empty cgroup name");
            res = -1;
        }
    }
    return res;
}

JNIEXPORT jint JNICALL
Java_com_alibaba_tenant_NativeDispatcher_moveToCgroup(JNIEnv* env,
                                                     jobject ignored,
                                                     jstring group_path) {

    int res = 0;
    if (group_path == NULL) {
        JGROUP_MSG("null group path to create");
        res = -1;
    } else {
        const char* path_str = (*env)->GetStringUTFChars(env, group_path, NULL);
        if (path_str != NULL && strlen(path_str) > 0) {
            char path_buf[MAX_PATH_LEN];
            snprintf(path_buf, MAX_PATH_LEN, "%s/%s/tasks", cg_mount_points[CG_CPU], path_str);

            FILE* fp_tasks = fopen(path_buf, "we");
            if (fp_tasks != NULL) {

                JGROUP_INFO("Open file %s (we)", path_buf);

                pid_t tid = get_tid();
                int sz = fprintf(fp_tasks, "%d", (int)tid);
                fclose(fp_tasks);

                if (sz > 0) {
                    JGROUP_INFO("PID %d has been writen to %s", (int)tid, path_buf);
                } else {
                    JGROUP_ERROR("Failed to write to %s", path_buf);
                }

                JGROUP_INFO("Close file %s (we)", path_buf);
            } else {
                JGROUP_ERROR("Failed to open %s (%s)", path_buf, strerror(errno));
                res = -1;
            }

            (*env)->ReleaseStringUTFChars(env, group_path, path_str);
        } else {
            JGROUP_MSG("Empty cgroup name");
            res = -1;
        }
    }
    return res;
}
