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
 *
 */

#include "precompiled.hpp"
#include "runtime/arguments_ext.hpp"
#include "runtime/java.hpp"

void ArgumentsExt::set_tenant_flags() {
  // order is critical here, please be careful
#if !(defined(LINUX) && defined(AMD64))
  if (TenantCpuThrottling || TenantCpuAccounting
      || TenantHeapIsolation || TenantHeapThrottling || MultiTenant) {
    vm_exit_during_initialization("MultiTenant only works on Linux x64 platform");
  }
#endif

  // TenantHeapThrottling directly depends on TenantHeapIsolation
  if (TenantHeapThrottling) {
    if (FLAG_IS_DEFAULT(TenantHeapIsolation)) {
      FLAG_SET_ERGO(bool, TenantHeapIsolation, true);
    }
    if (!TenantHeapIsolation) {
      vm_exit_during_initialization("Invalid combination of -XX:+TenantHeapThrottling and -XX:-TenantHeapIsolation");
    }
  }

  // TenantHeapIsolation directly depends on MultiTenant, UseG1GC
  if (TenantHeapIsolation) {
    if (FLAG_IS_DEFAULT(MultiTenant)) {
      FLAG_SET_ERGO(bool, MultiTenant, true);
    }
    if (UseTLAB && FLAG_IS_DEFAULT(UsePerTenantTLAB)) {
      // enable per-tenant TLABs if unspecified and heap isolation is enabled
      FLAG_SET_ERGO(bool, UsePerTenantTLAB, true);
    }

    // check GC policy compatibility
    if (!UseG1GC) {
      vm_exit_during_initialization("-XX:+TenantHeapIsolation only works with -XX:+UseG1GC");
    }
    if (!MultiTenant) {
      vm_exit_during_initialization("Cannot use multi-tenant features if -XX:-MultiTenant specified");
    }
  }

  // UsePerTenantTLAB depends on TenantHeapIsolation and UseTLAB
  if (UsePerTenantTLAB) {
    if (!TenantHeapIsolation || !UseTLAB) {
      vm_exit_during_initialization("-XX:+UsePerTenantTLAB only works with -XX:+TenantHeapIsolation and -XX:+UseTLAB");
    }
  }
}
