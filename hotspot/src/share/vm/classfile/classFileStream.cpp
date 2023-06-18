/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "classfile/classFileStream.hpp"
#include "classfile/vmSymbols.hpp"

void ClassFileStream::truncated_file_error(TRAPS) {
  THROW_MSG(vmSymbols::java_lang_ClassFormatError(), "Truncated class file");
}

ClassFileStream::ClassFileStream(u1* buffer, int length, const char* source, bool need_verify) {
  _buffer_start = buffer;
  _buffer_end   = buffer + length;
  _current      = buffer;
  _source       = source;
  _need_verify  = need_verify;
}

u1 ClassFileStream::get_u1(TRAPS) {
  if (_need_verify) {
    guarantee_more(1, CHECK_0);
  } else {
    assert(1 <= _buffer_end - _current, "buffer overflow");
  }
  return *_current++;
}

u2 ClassFileStream::get_u2(TRAPS) {
  if (_need_verify) {
    guarantee_more(2, CHECK_0);
  } else {
    assert(2 <= _buffer_end - _current, "buffer overflow");
  }
  u1* tmp = _current;
  _current += 2;
  return Bytes::get_Java_u2(tmp);
}

u4 ClassFileStream::get_u4(TRAPS) {
  if (_need_verify) {
    guarantee_more(4, CHECK_0);
  } else {
    assert(4 <= _buffer_end - _current, "buffer overflow");
  }
  u1* tmp = _current;
  _current += 4;
  return Bytes::get_Java_u4(tmp);
}

u8 ClassFileStream::get_u8(TRAPS) {
  if (_need_verify) {
    guarantee_more(8, CHECK_0);
  } else {
    assert(8 <= _buffer_end - _current, "buffer overflow");
  }
  u1* tmp = _current;
  _current += 8;
  return Bytes::get_Java_u8(tmp);
}

void ClassFileStream::skip_u1(int length, TRAPS) {
  if (_need_verify) {
    guarantee_more(length, CHECK);
  }
  _current += length;
}

void ClassFileStream::skip_u2(int length, TRAPS) {
  if (_need_verify) {
    guarantee_more(length * 2, CHECK);
  }
  _current += length * 2;
}

void ClassFileStream::skip_u4(int length, TRAPS) {
  if (_need_verify) {
    guarantee_more(length * 4, CHECK);
  }
  _current += length * 4;
}

#if INCLUDE_JFR

u1* ClassFileStream::clone_buffer() const {
  u1* const new_buffer_start = NEW_RESOURCE_ARRAY(u1, length());
  memcpy(new_buffer_start, _buffer_start, length());
  return new_buffer_start;
}

const char* const ClassFileStream::clone_source() const {
  const char* const src = source();
  char* source_copy = NULL;
  if (src != NULL) {
    size_t source_len = strlen(src);
    source_copy = NEW_RESOURCE_ARRAY(char, source_len + 1);
    strncpy(source_copy, src, source_len + 1);
  }
  return source_copy;
}

ClassFileStream* ClassFileStream::clone() const {
  u1* const new_buffer_start = clone_buffer();
  return new ClassFileStream(new_buffer_start,
                             length(),
                             clone_source(),
                             need_verify());
}
#endif // INCLUDE_JFR

uint64_t ClassFileStream::compute_fingerprint() const {
    int classfile_size = length();
    int classfile_crc = ClassLoader::crc32(0, (const char*)buffer(), length());
    uint64_t fingerprint = (uint64_t(classfile_size) << 32) | uint64_t(uint32_t(classfile_crc));
    assert(fingerprint != 0, "must not be zero");

    return fingerprint;
}
