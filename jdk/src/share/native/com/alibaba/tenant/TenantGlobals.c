/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "jni.h"
#include "tenantenv.h"
#include "jni_util.h"
#include "jvm.h"
#include "com_alibaba_tenant_TenantGlobals.h"

static TenantEnv*
getTenantEnv(JNIEnv *env)
{
    jint rc               = JNI_ERR;
    JavaVM *jvm           = NULL;
    TenantEnv  *tenantEnv = NULL;
    (*env)->GetJavaVM(env, &jvm);
    if(NULL != jvm) {
        /* Get tenant environment */
        rc = (*jvm)->GetEnv(jvm, (void **)&tenantEnv, TENANT_ENV_VERSION_1_0);
        if (JNI_OK != rc) {
        tenantEnv = NULL;
    }
  }
  return tenantEnv;
}

JNIEXPORT jint JNICALL
Java_com_alibaba_tenant_TenantGlobals_getTenantFlags(JNIEnv *env, jclass cls)
{
    jint rc  =  JNI_ERR;
    TenantEnv* tenantEnv = getTenantEnv(env);
    if(NULL != tenantEnv) {
        rc = (*tenantEnv)->GetTenantFlags(tenantEnv, cls);
    } else {
        //throw exception
        JNU_ThrowByName(env, "java/lang/InternalError", "Can not get tenant environment.");
    }
  return rc;
}
