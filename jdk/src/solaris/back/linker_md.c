/*
 * Copyright (c) 1998, 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/*
 * Adapted from JDK 1.2 linker_md.c v1.37. Note that we #define
 * NATIVE here, whether or not we're running solaris native threads.
 * Outside the VM, it's unclear how we can do the locking that is
 * done in the green threads version of the code below.
 */
#define NATIVE

/*
 * Machine Dependent implementation of the dynamic linking support
 * for java.  This routine is Solaris specific.
 */

#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "path_md.h"
#ifndef NATIVE
#include "iomgr.h"
#include "threads_md.h"
#endif

#ifdef __APPLE__
#define LIB_SUFFIX "dylib"
#else
#define LIB_SUFFIX "so"
#endif

static void dll_build_name(char* buffer, size_t buflen,
                           const char* paths, const char* fname) {
    char *path, *paths_copy, *next_token;
    *buffer = '\0';

    paths_copy = jvmtiAllocate((int)strlen(paths) + 1);
    strcpy(paths_copy, paths);
    if (paths_copy == NULL) {
        return;
    }

    next_token = NULL;
    path = strtok_r(paths_copy, PATH_SEPARATOR, &next_token);

    while (path != NULL) {
        size_t result_len = (size_t)snprintf(buffer, buflen, "%s/lib%s." LIB_SUFFIX, path, fname);
        if (result_len >= buflen) {
            EXIT_ERROR(JVMTI_ERROR_INVALID_LOCATION, "One or more of the library paths supplied to jdwp, "
                                                     "likely by sun.boot.library.path, is too long.");
        } else if (access(buffer, F_OK) == 0) {
            break;
        }
        *buffer = '\0';
        path = strtok_r(NULL, PATH_SEPARATOR, &next_token);
    }

    jvmtiDeallocate(paths_copy);
}

/*
 * create a string for the JNI native function name by adding the
 * appropriate decorations.
 */
int
dbgsysBuildFunName(char *name, int nameLen, int args_size, int encodingIndex)
{
  /* On Solaris, there is only one encoding method. */
    if (encodingIndex == 0)
        return 1;
    return 0;
}

/*
 * create a string for the dynamic lib open call by adding the
 * appropriate pre and extensions to a filename and the path
 */
void
dbgsysBuildLibName(char *holder, int holderlen, const char *pname, const char *fname)
{
    const int pnamelen = pname ? strlen(pname) : 0;

    if (pnamelen == 0) {
        if (pnamelen + (int)strlen(fname) + 10 > holderlen) {
            EXIT_ERROR(JVMTI_ERROR_INVALID_LOCATION, "One or more of the library paths supplied to jdwp, "
                                                     "likely by sun.boot.library.path, is too long.");
        }
        (void)snprintf(holder, holderlen, "lib%s." LIB_SUFFIX, fname);
    } else {
      dll_build_name(holder, holderlen, pname, fname);
    }
}

#ifndef NATIVE
extern int thr_main(void);
#endif

void *
dbgsysLoadLibrary(const char *name, char *err_buf, int err_buflen)
{
    void * result;
#ifdef NATIVE
    result = dlopen(name, RTLD_LAZY);
#else
    sysMonitorEnter(greenThreadSelf(), &_dl_lock);
    result = dlopen(name, RTLD_NOW);
    sysMonitorExit(greenThreadSelf(), &_dl_lock);
    /*
     * This is a bit of bulletproofing to catch the commonly occurring
     * problem of people loading a library which depends on libthread into
     * the VM.  thr_main() should always return -1 which means that libthread
     * isn't loaded.
     */
    if (thr_main() != -1) {
         VM_CALL(panic)("libthread loaded into green threads");
    }
#endif
    if (result == NULL) {
        (void)strncpy(err_buf, dlerror(), err_buflen-2);
        err_buf[err_buflen-1] = '\0';
    }
    return result;
}

void dbgsysUnloadLibrary(void *handle)
{
#ifndef NATIVE
    sysMonitorEnter(greenThreadSelf(), &_dl_lock);
#endif
    (void)dlclose(handle);
#ifndef NATIVE
    sysMonitorExit(greenThreadSelf(), &_dl_lock);
#endif
}

void * dbgsysFindLibraryEntry(void *handle, const char *name)
{
    void * sym;
#ifndef NATIVE
    sysMonitorEnter(greenThreadSelf(), &_dl_lock);
#endif
    sym =  dlsym(handle, name);
#ifndef NATIVE
    sysMonitorExit(greenThreadSelf(), &_dl_lock);
#endif
    return sym;
}
