/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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

#include <string.h>

#include "jni.h"
#include "jni_util.h"
#include "jlong.h"
#include "jvm.h"
#include "jdk_util.h"

#include "sun_misc_URLClassPath.h"

extern char*
getUTF(JNIEnv *env, jstring str, char* localBuf, int bufSize);


JNIEXPORT jboolean JNICALL
Java_sun_misc_URLClassPath_knownToNotExist0(JNIEnv *env, jclass cls, jobject loader,
                                            jstring classname)
{
    char *clname;
    jboolean result = JNI_FALSE;
    char buf[128];

    if (classname == NULL) {
        JNU_ThrowNullPointerException(env, NULL);
        return result;
    }

    clname = getUTF(env, classname, buf, sizeof(buf));
    if (clname == NULL) {
        // getUTF() throws OOME before returning NULL, no need to throw OOME here
        return result;
    }
    VerifyFixClassname(clname);

    if (!VerifyClassname(clname, JNI_TRUE)) {  /* expects slashed name */
        goto done;
    }

    result = JVM_KnownToNotExist(env, loader, clname);

 done:
    if (clname != buf) {
        free(clname);
    }

    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_sun_misc_URLClassPath_getLookupCacheURLs(JNIEnv *env, jclass cls, jobject loader)
{
    return JVM_GetResourceLookupCacheURLs(env, loader);
}


JNIEXPORT jintArray JNICALL
Java_sun_misc_URLClassPath_getLookupCacheForClassLoader(JNIEnv *env, jclass cls,
                                                        jobject loader,
                                                        jstring resource_name)
{
    char *resname;
    jintArray result = NULL;
    char buf[128];

    if (resource_name == NULL) {
        JNU_ThrowNullPointerException(env, NULL);
        return result;
    }

    resname = getUTF(env, resource_name, buf, sizeof(buf));
    if (resname == NULL) {
        // getUTF() throws OOME before returning NULL, no need to throw OOME here
        return result;
    }
    result = JVM_GetResourceLookupCache(env, loader, resname);

 done:
    if (resname != buf) {
        free(resname);
    }

    return result;
}

