/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_OOPS_INSTANCEKLASS_INLINE_HPP
#define SHARE_VM_OOPS_INSTANCEKLASS_INLINE_HPP

#include "oops/instanceKlass.hpp"
#include "oops/klass.inline.hpp"

template <class ITERATOR>
inline void InstanceKlass::itable_pointers_do(ITERATOR* iter) {
  if (itable_length() > 0) {
    itableOffsetEntry* ioe = (itableOffsetEntry*)start_of_itable();
    int method_table_offset_in_words = ioe->offset()/wordSize;
    int nof_interfaces = (method_table_offset_in_words - itable_offset_in_words())
                         / itableOffsetEntry::size();

    for (int i = 0; i < nof_interfaces; i ++, ioe ++) {
      if (ioe->interface_klass() != NULL) {
        iter->push(ioe->interface_klass_addr());
        itableMethodEntry* ime = ioe->first_method_entry(this);
        int n = klassItable::method_count_for_interface(ioe->interface_klass());
        for (int index = 0; index < n; index ++) {
          iter->push(ime[index].method_addr());
        }
      }
    }
  }
}

template <class ITERATOR>
inline void InstanceKlass::klass_and_method_pointers_do(ITERATOR* iter) {
  Klass::klass_and_method_pointers_do(iter);

  iterate_array(iter, _methods);
  iterate_array(iter, _default_methods);
  iterate_array(iter, _local_interfaces);
  iterate_array(iter, _transitive_interfaces);

  itable_pointers_do(iter);
}

#endif // SHARE_VM_OOPS_INSTANCEKLASS_INLINE_HPP
