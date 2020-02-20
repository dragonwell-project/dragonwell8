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

#include "prims/tenantenv.h"
#include "runtime/globals.hpp"

/**
 * Be careful: any change to the following constant defintions, you MUST
 * synch up them with ones defined in com.alibaba.tenant.TenantGlobals
 */

#define TENANT_FLAG_MULTI_TENANT_ENABLED             (0x1)    // bit 0 to indicate if the tenant feature is enabled.

static jint tenant_GetTenantFlags(TenantEnv *env, jclass cls);

static struct TenantNativeInterface_ tenantNativeInterface = {
  tenant_GetTenantFlags
};

struct TenantNativeInterface_* tenant_functions()
{
  return &tenantNativeInterface;
}

static jint
tenant_GetTenantFlags(TenantEnv *env, jclass cls)
{
  jint result = 0x0;

  if (MultiTenant) {
    result |= TENANT_FLAG_MULTI_TENANT_ENABLED;
  }

  return result;
}
