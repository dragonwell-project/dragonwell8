/*
 * Copyright (c) 2021, Alibaba Group Holding Limited. All rights reserved.
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
 */

#ifndef SHARE_VM_UTILITIES_DUMPUTIL_HPP
#define SHARE_VM_UTILITIES_DUMPUTIL_HPP

#include "runtime/os.hpp"

class DumpAux : public StackObj {
 friend class DumpPathBuilder;
 private:
  char*       _base_path;
  uint*       _dump_file_seq_ptr;
  const char* _dump_file_name;
  const char* _dump_file_ext;
  const char* _dump_path;

 public:
  DumpAux(char* char_path, uint* dump_file_seq_ptr, const char* dump_file_name,
          const char* dump_file_ext, const char* dump_path) :
    _base_path(char_path),
    _dump_file_seq_ptr(dump_file_seq_ptr),
    _dump_file_name(dump_file_name),
    _dump_file_ext(dump_file_ext),
    _dump_path(dump_path) {}
};

class DumpPathBuilder {
 private:
  DumpAux& _aux;
  const char*    _path;
 public:
  DumpPathBuilder(DumpAux& aux) : _aux(aux), _path(NULL) {}

  ~DumpPathBuilder() {
    if (_path != NULL) {
      os::free((void*)_path);
    }
  }

  const char* build() {
    char* base_path = _aux._base_path;
    uint dump_file_seq = *(_aux._dump_file_seq_ptr);
    char* my_path;
    const int max_digit_chars = 20;

    const char* dump_file_name = _aux._dump_file_name;
    const char* dump_file_ext = _aux._dump_file_ext;

    const char* dump_path = _aux._dump_path;
    // The dump file defaults to <dump_file_name>.<dump_file_ext> in the current working
    // directory. XXXDumpPath=<file> can be used to specify an alternative
    // dump file name or a directory where dump file is created.
    if (dump_file_seq == 0) { // first time in, we initialize base_path
      // Calculate potentially longest base path and check if we have enough
      // allocated statically.
      const size_t total_length =
                        (dump_path == NULL ? 0 : strlen(dump_path)) +
                        strlen(os::file_separator()) + max_digit_chars +
                        strlen(dump_file_name) + strlen(dump_file_ext) + 1;
      if (total_length > JVM_MAXPATHLEN) {
        warning("Cannot create dump file. Path is too long.");
        return NULL;
      }

      bool use_default_filename = true;
      if (dump_path == NULL || dump_path[0] == '\0') {
        // XXXDumpPath=<file> not specified
      } else {
        strncpy(base_path, dump_path, JVM_MAXPATHLEN);
        // check if the path is a directory (must exist)
        DIR* dir = os::opendir(base_path);
        if (dir == NULL) {
          use_default_filename = false;
        } else {
          // XXXDumpPath specified a directory. We append a file separator
          // (if needed).
          os::closedir(dir);
          size_t fs_len = strlen(os::file_separator());
          if (strlen(base_path) >= fs_len) {
            char* end = base_path;
            end += (strlen(base_path) - fs_len);
            if (strcmp(end, os::file_separator()) != 0) {
              strcat(base_path, os::file_separator());
            }
          }
        }
      }
      // If XXXDumpPath wasn't a file name then we append the default name
      if (use_default_filename) {
        const size_t dlen = strlen(base_path);  // if dump dir specified
        jio_snprintf(&base_path[dlen], JVM_MAXPATHLEN - dlen, "%s%d%s",
                     dump_file_name, os::current_process_id(), dump_file_ext);
      }
      const size_t len = strlen(base_path) + 1;
      my_path = (char*)os::malloc(len, mtInternal);
      if (my_path == NULL) {
        warning("Cannot create dump file. Out of system memory.");
        return NULL;
      }
      strncpy(my_path, base_path, len);
    } else {
      // Append a sequence number id for dumps following the first
      const size_t len = strlen(base_path) + max_digit_chars + 2; // for '.' and \0
      my_path = (char*)os::malloc(len, mtInternal);
      if (my_path == NULL) {
        warning("Cannot dump file. Out of system memory.");
        return NULL;
      }
      jio_snprintf(my_path, len, "%s.%d", base_path, dump_file_seq);
    }
    (*(_aux._dump_file_seq_ptr))++;   // increment seq number for next time we dump

    _path = my_path;
    return _path;
  }
};

#endif // SHARE_VM_UTILITIES_DUMPUTIL_HPP
