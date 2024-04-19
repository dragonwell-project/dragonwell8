/*
 * Copyright (c) 1997, 2015, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_WINDOWS_VM_OS_WINDOWS_INLINE_HPP
#define OS_WINDOWS_VM_OS_WINDOWS_INLINE_HPP

#include "runtime/atomic.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/os.hpp"

inline const char* os::file_separator()                { return "\\"; }
inline const char* os::line_separator()                { return "\r\n"; }
inline const char* os::path_separator()                { return ";"; }
inline const char* os::dll_file_extension()            { return ".dll"; }

inline const int os::default_file_open_flags() { return O_BINARY | O_NOINHERIT;}

// File names are case-insensitive on windows only
inline int os::file_name_strcmp(const char* s, const char* t) {
  return _stricmp(s, t);
}

inline void  os::dll_unload(void *lib) {
  ::FreeLibrary((HMODULE)lib);
}

inline void* os::dll_lookup(void *lib, const char *name) {
  return (void*)::GetProcAddress((HMODULE)lib, name);
}

// Used to improve time-sharing on some systems
inline void os::loop_breaker(int attempts) {}

inline bool os::obsolete_option(const JavaVMOption *option) {
  return false;
}

inline bool os::uses_stack_guard_pages() {
  return os::win32::is_nt();
}

inline bool os::allocate_stack_guard_pages() {
  assert(uses_stack_guard_pages(), "sanity check");
  return true;
}

// Bang the shadow pages if they need to be touched to be mapped.
inline void os::bang_stack_shadow_pages() {
  // Write to each page of our new frame to force OS mapping.
  // If we decrement stack pointer more than one page
  // the OS may not map an intervening page into our space
  // and may fault on a memory access to interior of our frame.
  address sp = current_stack_pointer();
  for (int pages = 1; pages <= StackShadowPages; pages++) {
    *((int *)(sp - (pages * vm_page_size()))) = 0;
  }
}

inline bool os::numa_has_static_binding()   { return true;   }
inline bool os::numa_has_group_homing()     { return false;  }

inline size_t os::read(int fd, void *buf, unsigned int nBytes) {
  return ::read(fd, buf, nBytes);
}

inline size_t os::restartable_read(int fd, void *buf, unsigned int nBytes) {
  return ::read(fd, buf, nBytes);
}

inline size_t os::write(int fd, const void *buf, unsigned int nBytes) {
  return ::write(fd, buf, nBytes);
}

inline int os::close(int fd) {
  return ::close(fd);
}

#endif // OS_WINDOWS_VM_OS_WINDOWS_INLINE_HPP
