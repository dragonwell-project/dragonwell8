/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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
 *
 */

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_G1ALLOCATIONCONTEXT_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_G1ALLOCATIONCONTEXT_HPP

#include "memory/allocation.hpp"
#include "utilities/hashtable.hpp"

class G1TenantAllocationContext;

/*
 * Typical scenario to use AllocationContext_t:
 * <code>_g1h->_allocator->mutator_alloc_buffer(alloc_context)->attempt_allocation(...)</code>
 *
 * Here we just simply make AllocationContext_t to contain a pointer of
 * G1TenantAllocationContext if TenantHeapIsolation enabled
 *
 */
class AllocationContext_t VALUE_OBJ_CLASS_SPEC {
private:
  union {
    volatile G1TenantAllocationContext* _tenant_alloc_context; // Pointer to corresponding tenant allocation context
    unsigned char                       _alloc_context;        // unused, original value type from OpenJDK
  } _value;

public:
  AllocationContext_t(const uint64_t val) {
    _value._tenant_alloc_context = (G1TenantAllocationContext*)val;
  }
  AllocationContext_t() { _value._tenant_alloc_context = NULL; }
  AllocationContext_t(const AllocationContext_t& peer) : _value(peer._value) { }
  AllocationContext_t(G1TenantAllocationContext* ctxt) {
    Atomic::store_ptr(ctxt, &_value._tenant_alloc_context);
  }

  G1TenantAllocationContext* tenant_allocation_context() const {
    assert(TenantHeapIsolation, "pre-condition");
    return (G1TenantAllocationContext*)_value._tenant_alloc_context;
  }

  // This method is useless for now, since the original implementation does not differentiate
  // system & current allocation context.
  // if Oracle makes any changes to AllocationContext_t in future, pls update below method as well
  const unsigned char allocation_context() const { return 0; }

  // Comparing the longest field in union
  // operator ==
  inline bool operator ==(const AllocationContext_t& ctxt) const {
    return _value._tenant_alloc_context == ctxt._value._tenant_alloc_context;
  }
  inline bool operator ==(const G1TenantAllocationContext* tac) const {
    return _value._tenant_alloc_context == tac;
  }
  inline bool operator ==(unsigned char alloc_context) const {
    return _value._alloc_context == alloc_context;
  }

  // operator !=
  inline bool operator !=(const AllocationContext_t& ctxt) const {
    return _value._tenant_alloc_context != ctxt._value._tenant_alloc_context;
  }
  inline bool operator !=(const G1TenantAllocationContext* tac) const {
    return _value._tenant_alloc_context != tac;
  }
  inline bool operator !=(unsigned char alloc_context) const {
    return _value._alloc_context != alloc_context;
  }

  // operator =
  inline AllocationContext_t& operator =(const AllocationContext_t& ctxt) {
    Atomic::store_ptr((void*)ctxt._value._tenant_alloc_context, &_value._tenant_alloc_context);
    return *this;
  }
  inline AllocationContext_t& operator =(const G1TenantAllocationContext* tac) {
    Atomic::store_ptr(const_cast<G1TenantAllocationContext*>(tac), &_value._tenant_alloc_context);
    return *this;
  }
  inline AllocationContext_t& operator =(unsigned char alloc_context) {
    _value._alloc_context = alloc_context;
    return *this;
  }

  inline const bool is_system() const { return NULL == _value._tenant_alloc_context; }

  // to enable AllocationContext_t to be used as key type in HashMap
  unsigned int hash_code() {
    void *p = (void*)_value._tenant_alloc_context;
    return HashMapUtil::hash(p);
  }

  inline G1TenantAllocationContext* operator -> () const {
    return tenant_allocation_context();
  }
};

class AllocationContext : AllStatic {
private:
  static AllocationContext_t          _root_context;

public:
  // Currently used context
  static AllocationContext_t current();

  // System wide default context
  static AllocationContext_t system();
};

class AllocationContextStats: public StackObj {
public:
  inline void clear() { }
  inline void update(bool full_gc) { }
  inline void update_after_mark() { }
  inline bool available() { return false; }
};

// To switch current to target AllocationContext_t during the lifespan of this object
class AllocationContextMark : public StackObj {
private:
  AllocationContext_t _saved_context;
public:
  AllocationContextMark(AllocationContext_t ctxt);
  ~AllocationContextMark();
};

#endif // SHARE_VM_GC_IMPLEMENTATION_G1_G1ALLOCATIONCONTEXT_HPP
