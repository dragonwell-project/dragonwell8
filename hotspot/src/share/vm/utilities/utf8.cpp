/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "utilities/utf8.hpp"

// Assume the utf8 string is in legal form and has been
// checked in the class file parser/format checker.
char* UTF8::next(const char* str, jchar* value) {
  unsigned const char *ptr = (const unsigned char *)str;
  unsigned char ch, ch2, ch3;
  int length = -1;              /* bad length */
  jchar result;
  switch ((ch = ptr[0]) >> 4) {
    default:
    result = ch;
    length = 1;
    break;

  case 0x8: case 0x9: case 0xA: case 0xB: case 0xF:
    /* Shouldn't happen. */
    break;

  case 0xC: case 0xD:
    /* 110xxxxx  10xxxxxx */
    if (((ch2 = ptr[1]) & 0xC0) == 0x80) {
      unsigned char high_five = ch & 0x1F;
      unsigned char low_six = ch2 & 0x3F;
      result = (high_five << 6) + low_six;
      length = 2;
      break;
    }
    break;

  case 0xE:
    /* 1110xxxx 10xxxxxx 10xxxxxx */
    if (((ch2 = ptr[1]) & 0xC0) == 0x80) {
      if (((ch3 = ptr[2]) & 0xC0) == 0x80) {
        unsigned char high_four = ch & 0x0f;
        unsigned char mid_six = ch2 & 0x3f;
        unsigned char low_six = ch3 & 0x3f;
        result = (((high_four << 6) + mid_six) << 6) + low_six;
        length = 3;
      }
    }
    break;
  } /* end of switch */

  if (length <= 0) {
    *value = ptr[0];    /* default bad result; */
    return (char*)(ptr + 1); // make progress somehow
  }

  *value = result;

  // The assert is correct but the .class file is wrong
  // assert(UNICODE::utf8_size(result) == length, "checking reverse computation");
  return (char *)(ptr + length);
}

char* UTF8::next_character(const char* str, jint* value) {
  unsigned const char *ptr = (const unsigned char *)str;
  /* See if it's legal supplementary character:
     11101101 1010xxxx 10xxxxxx 11101101 1011xxxx 10xxxxxx */
  if (is_supplementary_character(ptr)) {
    *value = get_supplementary_character(ptr);
    return (char *)(ptr + 6);
  }
  jchar result;
  char* next_ch = next(str, &result);
  *value = result;
  return next_ch;
}

// Count bytes of the form 10xxxxxx and deduct this count
// from the total byte count.  The utf8 string must be in
// legal form which has been verified in the format checker.
int UTF8::unicode_length(const char* str, int len) {
  int num_chars = len;
  for (int i = 0; i < len; i++) {
    if ((str[i] & 0xC0) == 0x80) {
      --num_chars;
    }
  }
  return num_chars;
}

// Count bytes of the utf8 string except those in form
// 10xxxxxx which only appear in multibyte characters.
// The utf8 string must be in legal form and has been
// verified in the format checker.
int UTF8::unicode_length(const char* str) {
  int num_chars = 0;
  for (const char* p = str; *p; p++) {
    if (((*p) & 0xC0) != 0x80) {
      num_chars++;
    }
  }
  return num_chars;
}

// Writes a jchar a utf8 and returns the end
static u_char* utf8_write(u_char* base, jchar ch) {
  if ((ch != 0) && (ch <=0x7f)) {
    base[0] = (u_char) ch;
    return base + 1;
  }

  if (ch <= 0x7FF) {
    /* 11 bits or less. */
    unsigned char high_five = ch >> 6;
    unsigned char low_six = ch & 0x3F;
    base[0] = high_five | 0xC0; /* 110xxxxx */
    base[1] = low_six | 0x80;   /* 10xxxxxx */
    return base + 2;
  }
  /* possibly full 16 bits. */
  char high_four = ch >> 12;
  char mid_six = (ch >> 6) & 0x3F;
  char low_six = ch & 0x3f;
  base[0] = high_four | 0xE0; /* 1110xxxx */
  base[1] = mid_six | 0x80;   /* 10xxxxxx */
  base[2] = low_six | 0x80;   /* 10xxxxxx */
  return base + 3;
}

void UTF8::convert_to_unicode(const char* utf8_str, jchar* unicode_str, int unicode_length) {
  unsigned char ch;
  const char *ptr = utf8_str;
  int index = 0;

  /* ASCII case loop optimization */
  for (; index < unicode_length; index++) {
    if((ch = ptr[0]) > 0x7F) { break; }
    unicode_str[index] = ch;
    ptr = (const char *)(ptr + 1);
  }

  for (; index < unicode_length; index++) {
    ptr = UTF8::next(ptr, &unicode_str[index]);
  }
}

// returns the quoted ascii length of a 0-terminated utf8 string
int UTF8::quoted_ascii_length(const char* utf8_str, int utf8_length) {
  const char *ptr = utf8_str;
  const char* end = ptr + utf8_length;
  int result = 0;
  while (ptr < end) {
    jchar c;
    ptr = UTF8::next(ptr, &c);
    if (c >= 32 && c < 127) {
      result++;
    } else {
      result += 6;
    }
  }
  return result;
}

// converts a utf8 string to quoted ascii
void UTF8::as_quoted_ascii(const char* utf8_str, int utf8_length, char* buf, int buflen) {
  const char *ptr = utf8_str;
  const char *utf8_end = ptr + utf8_length;
  char* p = buf;
  char* end = buf + buflen;
  while (ptr < utf8_end) {
    jchar c;
    ptr = UTF8::next(ptr, &c);
    if (c >= 32 && c < 127) {
      if (p + 1 >= end) break;      // string is truncated
      *p++ = (char)c;
    } else {
      if (p + 6 >= end) break;      // string is truncated
      sprintf(p, "\\u%04x", c);
      p += 6;
    }
  }
  assert(p < end, "sanity");
  *p = '\0';
}


const char* UTF8::from_quoted_ascii(const char* quoted_ascii_str) {
  const char *ptr = quoted_ascii_str;
  char* result = NULL;
  while (*ptr != '\0') {
    char c = *ptr;
    if (c < 32 || c >= 127) break;
  }
  if (*ptr == '\0') {
    // nothing to do so return original string
    return quoted_ascii_str;
  }
  // everything up to this point was ok.
  int length = ptr - quoted_ascii_str;
  char* buffer = NULL;
  for (int round = 0; round < 2; round++) {
    while (*ptr != '\0') {
      if (*ptr != '\\') {
        if (buffer != NULL) {
          buffer[length] = *ptr;
        }
        length++;
      } else {
        switch (ptr[1]) {
          case 'u': {
            ptr += 2;
            jchar value=0;
            for (int i=0; i<4; i++) {
              char c = *ptr++;
              switch (c) {
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                  value = (value << 4) + c - '0';
                  break;
                case 'a': case 'b': case 'c':
                case 'd': case 'e': case 'f':
                  value = (value << 4) + 10 + c - 'a';
                  break;
                case 'A': case 'B': case 'C':
                case 'D': case 'E': case 'F':
                  value = (value << 4) + 10 + c - 'A';
                  break;
                default:
                  ShouldNotReachHere();
              }
            }
            if (buffer == NULL) {
              char utf8_buffer[4];
              char* next = (char*)utf8_write((u_char*)utf8_buffer, value);
              length += next - utf8_buffer;
            } else {
              char* next = (char*)utf8_write((u_char*)&buffer[length], value);
              length += next - &buffer[length];
            }
            break;
          }
          case 't': if (buffer != NULL) buffer[length] = '\t'; ptr += 2; length++; break;
          case 'n': if (buffer != NULL) buffer[length] = '\n'; ptr += 2; length++; break;
          case 'r': if (buffer != NULL) buffer[length] = '\r'; ptr += 2; length++; break;
          case 'f': if (buffer != NULL) buffer[length] = '\f'; ptr += 2; length++; break;
          default:
            ShouldNotReachHere();
        }
      }
    }
    if (round == 0) {
      buffer = NEW_RESOURCE_ARRAY(char, length + 1);
      ptr = quoted_ascii_str;
    } else {
      buffer[length] = '\0';
    }
  }
  return buffer;
}


// Returns NULL if 'c' it not found. This only works as long
// as 'c' is an ASCII character
const jbyte* UTF8::strrchr(const jbyte* base, int length, jbyte c) {
  assert(length >= 0, "sanity check");
  assert(c >= 0, "does not work for non-ASCII characters");
  // Skip backwards in string until 'c' is found or end is reached
  while(--length >= 0 && base[length] != c);
  return (length < 0) ? NULL : &base[length];
}

bool UTF8::equal(const jbyte* base1, int length1, const jbyte* base2, int length2) {
  // Length must be the same
  if (length1 != length2) return false;
  for (int i = 0; i < length1; i++) {
    if (base1[i] != base2[i]) return false;
  }
  return true;
}

bool UTF8::is_supplementary_character(const unsigned char* str) {
  return ((str[0] & 0xFF) == 0xED) && ((str[1] & 0xF0) == 0xA0) && ((str[2] & 0xC0) == 0x80)
      && ((str[3] & 0xFF) == 0xED) && ((str[4] & 0xF0) == 0xB0) && ((str[5] & 0xC0) == 0x80);
}

jint UTF8::get_supplementary_character(const unsigned char* str) {
  return 0x10000 + ((str[1] & 0x0f) << 16) + ((str[2] & 0x3f) << 10)
                 + ((str[4] & 0x0f) << 6)  + (str[5] & 0x3f);
}


//-------------------------------------------------------------------------------------


int UNICODE::utf8_size(jchar c) {
  if ((0x0001 <= c) && (c <= 0x007F)) return 1;
  if (c <= 0x07FF) return 2;
  return 3;
}

int UNICODE::utf8_length(jchar* base, int length) {
  size_t result = 0;
  for (int index = 0; index < length; index++) {
    jchar c = base[index];
    int sz = utf8_size(c);
    if (result + sz > INT_MAX-1) {
      break;
    }
    result += sz;
  }
  return static_cast<int>(result);
}

char* UNICODE::as_utf8(jchar* base, int length) {
  int utf8_len = utf8_length(base, length);
  u_char* result = NEW_RESOURCE_ARRAY(u_char, utf8_len + 1);
  u_char* p = result;
  for (int index = 0; index < length; index++) {
    p = utf8_write(p, base[index]);
  }
  *p = '\0';
  assert(p == &result[utf8_len], "length prediction must be correct");
  return (char*) result;
}

char* UNICODE::as_utf8(jchar* base, int length, char* buf, int buflen) {
  u_char* p = (u_char*)buf;
  u_char* end = (u_char*)buf + buflen;
  for (int index = 0; index < length; index++) {
    jchar c = base[index];
    if (p + utf8_size(c) >= end) break;      // string is truncated
    p = utf8_write(p, base[index]);
  }
  *p = '\0';
  return buf;
}

void UNICODE::convert_to_utf8(const jchar* base, int length, char* utf8_buffer) {
  for(int index = 0; index < length; index++) {
    utf8_buffer = (char*)utf8_write((u_char*)utf8_buffer, base[index]);
  }
  *utf8_buffer = '\0';
}

// returns the quoted ascii length of a unicode string
int UNICODE::quoted_ascii_length(jchar* base, int length) {
  int result = 0;
  for (int i = 0; i < length; i++) {
    jchar c = base[i];
    if (c >= 32 && c < 127) {
      result++;
    } else {
      result += 6;
    }
  }
  return result;
}

// converts a utf8 string to quoted ascii
void UNICODE::as_quoted_ascii(const jchar* base, int length, char* buf, int buflen) {
  char* p = buf;
  char* end = buf + buflen;
  for (int index = 0; index < length; index++) {
    jchar c = base[index];
    if (c >= 32 && c < 127) {
      if (p + 1 >= end) break;      // string is truncated
      *p++ = (char)c;
    } else {
      if (p + 6 >= end) break;      // string is truncated
      sprintf(p, "\\u%04x", c);
      p += 6;
    }
  }
  *p = '\0';
}
