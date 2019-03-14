/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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

#ifndef SHARED_VM_UTILITIES_SYMBOLMATCHER_HPP
#define SHARED_VM_UTILITIES_SYMBOLMATCHER_HPP

#include "memory/allocation.hpp"
#include "utilities/growableArray.hpp"

class SymbolRegexPattern {
public:
  SymbolRegexPattern() {  }
  SymbolRegexPattern(const char* pattern, int length)
    : _pattern(pattern),
      _length(length) {
  }

  ~SymbolRegexPattern() {  }

  const char* origin_regex() { return _pattern; }
  void  set_origin_regex(char* s) { _pattern = s; }
  int   length() { return _length; }
  void  set_length(int value) { _length = value; }

private:
  const char* _pattern;
  int         _length;
};

template <MEMFLAGS F>
class SymbolMatcher : public ResourceObj {
public:
  SymbolMatcher(const char* regexes);
  virtual ~SymbolMatcher();
  GrowableArray<SymbolRegexPattern>* patterns() { return _patterns; }

  bool match(Symbol* symbol);
  bool match(const char* s);

private:
  void add_pattern(const char* src, int len);
  bool pattern_match(const char* regex, int regex_len, const char* s);

  GrowableArray<SymbolRegexPattern>*  _patterns;
};


#endif //SHARED_VM_UTILITIES_SYMBOLMATCHER_HPP
