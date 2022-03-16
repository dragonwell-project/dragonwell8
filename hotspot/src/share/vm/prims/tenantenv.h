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

#ifndef SHARE_VM_PRIMS_TENANT_ENV_H
#define SHARE_VM_PRIMS_TENANT_ENV_H

#include "jni.h"

// 0x00200000 represents tenant module and the last 10 represents version 1.0
#define TENANT_ENV_VERSION_1_0  0x00200010

/*
 * Tenant Native Method Interface.
 */
struct TenantNativeInterface_;
struct TenantEnv_;

#ifdef __cplusplus
typedef TenantEnv_ TenantEnv;
#else
typedef const struct TenantNativeInterface_* TenantEnv;
#endif

/*
 * We use inlined functions for C++ so that programmers can write:
 *   tenantEnv->GetTenantFlags(cls);
 * in C++ rather than:
 *   (*tenantEnv)->GetTenantFlags(tenantEnv, cls);
 * in C.
 */
struct TenantNativeInterface_ {
  jint (JNICALL *GetTenantFlags)(TenantEnv *env, jclass cls);
};

struct TenantEnv_ {
  const struct TenantNativeInterface_ *functions;
#ifdef __cplusplus
  jint GetTenantFlags(jclass cls) {
    return functions->GetTenantFlags(this, cls);
  }
#endif
};

#endif // SHARE_VM_PRIMS_TENANT_ENV_H
