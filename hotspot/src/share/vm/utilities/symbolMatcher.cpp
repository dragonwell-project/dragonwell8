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

#include "precompiled.hpp"
#include "memory/resourceArea.hpp"
#include "oops/symbol.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/symbolMatcher.hpp"

template <MEMFLAGS F> SymbolMatcher<F>::SymbolMatcher(const char* regexes)
  : _patterns(new (ResourceObj::C_HEAP, F)
                   GrowableArray<SymbolRegexPattern>(4, true, F)) {
  assert(regexes != NULL, "illegal regexes");
  int size = (int)strlen(regexes);
  int pattern_size = 0;
  char* pattern_begin = (char*)&regexes[0];
  for (int i = 0; i < size + 1; i++) {
    if (regexes[i] == ',' || regexes[i] == ';' || i == size) {
      add_pattern(pattern_begin, pattern_size);
      // reset
      pattern_size = -1;
      pattern_begin = (char*)&regexes[i+1];
    }
    pattern_size++;
  }
}

template <MEMFLAGS F> SymbolMatcher<F>::~SymbolMatcher() {
  delete _patterns;
}

template <MEMFLAGS F> void SymbolMatcher<F>::add_pattern(const char* s, int len) {
  if (len == 0) {
    return;
  }
  _patterns->push(SymbolRegexPattern(s, len));
}

template <MEMFLAGS F> bool SymbolMatcher<F>::match(Symbol* symbol) {
  ResourceMark rm;
  char* s = symbol->as_C_string();
  return match(s);
}

template <MEMFLAGS F> bool SymbolMatcher<F>::match(const char* s) {
  int regex_num = _patterns->length();
  for (int i = 0; i < regex_num; i++) {
    const char* regex = _patterns->at(i).origin_regex();
    int regex_len = _patterns->at(i).length();
    if (pattern_match(regex, regex_len, s)) {
      return true;
    }
  }
  return false;
}

template <MEMFLAGS F> bool SymbolMatcher<F>::pattern_match(const char* regex, int regex_len, const char* s) {
  int s_len = (int)strlen(s);
  if (s_len < regex_len - 1) {
    return false;
  }
  for (int i =0; i < regex_len; i++) {
    if (regex[i] == '*') {
     return true;
    }
    if (regex[i] == s[i]) {
      continue;
    }
    if ((regex[i] == '.' && s[i] == '/')
     || (regex[i] == '/' && s[i] == '.')) {
      continue;
    }
    if (regex[i] != '*' && regex[i] != s[i]) {
      return false;
    }
  }
  return (s_len == regex_len);
}

void test_symbol_matcher() {
  const char* str = NULL;
  const char* regexes1 = "com.alibaba.Test,org.*,org.apache.logging*";
  SymbolMatcher<mtClass>* matcher1 = new (ResourceObj::C_HEAP, mtClass) SymbolMatcher<mtClass>(regexes1);
  str = "com/alibaba/Test";
  assert(matcher1->match(str) == true, "");

  str = "com/alibaba/Test2";
  assert(matcher1->match(str) == false, "");

  str = "org/alibaba/Test2";
  assert(matcher1->match(str) == true, "");

  str = "org/A";
  assert(matcher1->match(str) == true, "");

  str = "org.";
  assert(matcher1->match(str) == true, "");

  str = ".";
  assert(matcher1->match(str) == false, "");

  str = "";
  assert(matcher1->match(str) == false, "");

  str = "org/apache/logging/Flag";
  assert(matcher1->match(str) == true, "");

  const char* regexes2 = "com/alibaba.Test,,a.,";
  SymbolMatcher<mtClass>* matcher2 = new (ResourceObj::C_HEAP, mtClass) SymbolMatcher<mtClass>(regexes2);

  str = "";
  assert(matcher2->match(str) == false, "");

  str = "com.alibaba.Test";
  assert(matcher2->match(str) == true, "");

  str = "a.a";
  assert(matcher2->match(str) == false, "");

  const char* regexes3 = "org.apache.logging*";
  SymbolMatcher<mtClass>* matcher3 = new (ResourceObj::C_HEAP, mtClass) SymbolMatcher<mtClass>(regexes3);

  str = "org/apache/logging/x";
  assert(matcher3->match(str) == true, "");

  str = "org/apache/loggingxx";
  assert(matcher3->match(str) == true, "");

  delete matcher1;
  delete matcher2;
}

template class SymbolMatcher<mtClass>;
