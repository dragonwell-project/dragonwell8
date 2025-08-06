/*
 * Copyright (c) 2025, Alibaba Group Holding Limited. All Rights Reserved.
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

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "aiext.h"

static size_t obj_array_len_offset, obj_array_data_offset, obj_array_elem_size;
static size_t byte_array_len_offset, byte_array_data_offset,
    byte_array_elem_size;
static size_t char_array_len_offset, char_array_data_offset,
    char_array_elem_size;
static size_t int_array_len_offset, int_array_data_offset, int_array_elem_size;

static uint32_t narrow_null;
static uintptr_t narrow_base;
static size_t narrow_shift;

static int offset_x_int, offset_x_double, offset_strs, offset_string_value;

static void *addr_static_int, *addr_static_enum, *addr_test_enum_a,
    *addr_test_enum_b, *addr_test_enum_c;

static void* native_provider(const aiext_env_t* env,
                             const char* native_func_name, void* data) {
  offset_x_int =
      env->get_field_offset("TestAIExtension$Launcher", "x_int", "I");
  offset_x_double =
      env->get_field_offset("TestAIExtension$Launcher", "x_double", "D");
  offset_strs = env->get_field_offset("TestAIExtension$Launcher", "strs",
                                      "[Ljava/lang/String;");
  offset_string_value =
      env->get_field_offset("java/lang/String", "value", "[C");
  addr_static_int =
      env->get_static_field_addr("TestAIExtension$Launcher", "static_int", "I");
  addr_static_enum =
      env->get_static_field_addr("TestAIExtension$Launcher", "static_enum",
                                 "LTestAIExtension$Launcher$TestEnum;");
  addr_test_enum_a =
      env->get_static_field_addr("TestAIExtension$Launcher$TestEnum", "A",
                                 "LTestAIExtension$Launcher$TestEnum;");
  addr_test_enum_b =
      env->get_static_field_addr("TestAIExtension$Launcher$TestEnum", "B",
                                 "LTestAIExtension$Launcher$TestEnum;");
  addr_test_enum_c =
      env->get_static_field_addr("TestAIExtension$Launcher$TestEnum", "C",
                                 "LTestAIExtension$Launcher$TestEnum;");
  printf(
      "Compiling `%s`, offset_x_int=%d, offset_x_double=%d, "
      "offset_strs=%d, offset_string_value=%d, addr_static_int=%p, "
      "addr_static_enum=%p, addr_test_enum_a=%p, addr_test_enum_b=%p, "
      "addr_test_enum_c=%p\n",
      native_func_name, offset_x_int, offset_x_double, offset_strs,
      offset_string_value, addr_static_int, addr_static_enum, addr_test_enum_a,
      addr_test_enum_b, addr_test_enum_c);
  if (offset_x_int < 0 || offset_x_double < 0 || offset_strs < 0 ||
      offset_string_value < 0 || addr_static_int == NULL ||
      addr_static_enum == NULL || addr_test_enum_a == NULL ||
      addr_test_enum_b == NULL || addr_test_enum_c == NULL) {
    return NULL;
  }
  return data;
}

static void* get_raw_pointer(void* oop) {
  if (obj_array_elem_size < sizeof(void*)) {
    // Narrow oop layout.
    uint32_t narrow = *(uint32_t*)oop;
    if (narrow == narrow_null) {
      return NULL;
    }
    return (void*)(narrow_base + ((uintptr_t)narrow << narrow_shift));
  } else {
    return *(void**)oop;
  }
}

static bool is_oop_eq(const void* lhs, const void* rhs) {
  if (obj_array_elem_size < sizeof(void*)) {
    // Narrow oop layout.
    return *(const uint32_t*)lhs == *(const uint32_t*)rhs;
  } else {
    return *(void* const*)lhs == *(void* const*)rhs;
  }
}

static void set_oop(void* dst, const void* oop) {
  if (obj_array_elem_size < sizeof(void*)) {
    // Narrow oop layout.
    *(uint32_t*)dst = *(const uint32_t*)oop;
  } else {
    *(void**)dst = *(void* const*)oop;
  }
}

// For ()V static method.
static void hello() { printf("Hello from native library!\n"); }

// For (I)V static method.
static void hello_int(int32_t i) {
  printf("Hello, I got %" PRId32 " (int)!\n", i);
}

// For (J)V static method.
static void hello_long(int64_t l) {
  printf("Hello, I got %" PRId64 " (long)!\n", l);
}

// For (F)V static method.
static void hello_float(float f) { printf("Hello, I got %.2f (float)!\n", f); }

// For (D)V static method.
static void hello_double(double d) {
  printf("Hello, I got %.2f (double)!\n", d);
}

// For ([B)V static method.
static void hello_bytes(const void* chars) {
  int32_t len = *(const int32_t*)((const char*)chars + byte_array_len_offset);
  char* cs = (char*)chars + byte_array_data_offset;
  assert(byte_array_elem_size == sizeof(char) && "unexpected byte size");
  printf("Hello, I got %.*s (bytes)!\n", len, cs);
}

// For (Ljava/lang/Object;)V static method.
static void hello_object(const void* obj) {
  printf("Hello, I got %p (object)!\n", obj);
}

// For (S)V method (with a `this` pointer).
static void hello_short_method(const void* this, int16_t i) {
  printf("Hello, I got %p (this) and %" PRId16 " (short)!\n", this, i);
}

// Adds two integers.
// For (II)I static method.
static int32_t add_ints(int32_t a, int32_t b) { return a + b; }

// Adds two doubles.
// For (DD)D static method.
static double add_doubles(double a, double b) { return a + b; }

// Adds two integer arrays, updates the first array in-place.
// For ([I[I)V method.
static void add_arrays(const void* this, void* a, const void* b) {
  int32_t a_len = *(int32_t*)((char*)a + int_array_len_offset);
  int32_t b_len = *(int32_t*)((char*)b + int_array_len_offset);
  for (int i = 0; i < a_len && i < b_len; i++) {
    int32_t* pa =
        (int32_t*)((char*)a + int_array_data_offset + i * int_array_elem_size);
    int32_t* pb =
        (int32_t*)((char*)b + int_array_data_offset + i * int_array_elem_size);
    *pa += *pb;
  }
}

// Adds the given integer to object's field.
// For (I)V method.
static void add_to_int(void* this, int32_t i) {
  assert(offset_x_int > 0 && "Invalid field offset");
  int32_t* x_int = (int32_t*)((char*)this + offset_x_int);
  *x_int += i;
}

// Adds the given double to object's field.
// For (D)V method.
static void add_to_double(void* this, double d) {
  assert(offset_x_double > 0 && "Invalid field offset");
  double* x_double = (double*)((char*)this + offset_x_double);
  *x_double += d;
}

// Reads the string array and prints it out.
// For ()V method.
static void read_strs(void* this) {
  assert(offset_strs > 0 && offset_string_value > 0 && "Invalid field offset");

  // Get pointer to the string array.
  void* strs = get_raw_pointer((char*)this + offset_strs);
  assert(strs != NULL && "Invalid string array");

  // Traverse the string array.
  int32_t strs_len = *(int32_t*)((char*)strs + obj_array_len_offset);
  for (int32_t i = 0; i < strs_len; i++) {
    void* str = get_raw_pointer((char*)strs + obj_array_data_offset +
                                i * obj_array_elem_size);
    assert(str != NULL && "Invalid string");

    // Get pointer to the string's byte array.
    void* value = get_raw_pointer((char*)str + offset_string_value);
    assert(value != NULL && "Invalid string's byte array");

    // Print the string's char array.
    int32_t value_len = *(int32_t*)((char*)value + char_array_len_offset);
    for (int32_t j = 0; j < value_len; j++) {
      char c = (char)*(short*)((char*)value + char_array_data_offset +
                               j * char_array_elem_size);
      putchar(c);
    }
    putchar('\n');
  }
}

// Adds the given integer to static field.
// For (I)V method.
static void add_to_static_int(int32_t i) {
  assert(addr_static_int != NULL && "Invalid static field address");
  *(int32_t*)addr_static_int += i;
}

// Checks the static enum field and updates it.
// For ()V method.
static void check_static_enum() {
  assert(addr_static_enum != NULL && "Invalid static field address");
  if (is_oop_eq(addr_static_enum, addr_test_enum_a)) {
    printf("A\n");
  } else if (is_oop_eq(addr_static_enum, addr_test_enum_b)) {
    printf("B\n");
  } else if (is_oop_eq(addr_static_enum, addr_test_enum_c)) {
    printf("C\n");
  } else {
    printf("Unknown\n");
  }
  set_oop(addr_static_enum, addr_test_enum_b);
}

JNIEXPORT aiext_result_t JNICALL aiext_init(const aiext_env_t* env,
                                            aiext_handle_t handle) {
  return AIEXT_OK;
}

JNIEXPORT aiext_result_t JNICALL aiext_post_init(const aiext_env_t* env,
                                                 aiext_handle_t handle) {
  // Get array layout.
  aiext_result_t res;
  res = env->get_array_layout(AIEXT_TYPE_OBJECT, &obj_array_len_offset,
                              &obj_array_data_offset, &obj_array_elem_size);
  if (res != AIEXT_OK) {
    return res;
  }
  res = env->get_array_layout(AIEXT_TYPE_BYTE, &byte_array_len_offset,
                              &byte_array_data_offset, &byte_array_elem_size);
  if (res != AIEXT_OK) {
    return res;
  }
  res = env->get_array_layout(AIEXT_TYPE_CHAR, &char_array_len_offset,
                              &char_array_data_offset, &char_array_elem_size);
  if (res != AIEXT_OK) {
    return res;
  }
  res = env->get_array_layout(AIEXT_TYPE_INT, &int_array_len_offset,
                              &int_array_data_offset, &int_array_elem_size);
  if (res != AIEXT_OK) {
    return res;
  }

  // Get narrow oop layout.
  res = env->get_narrow_oop_layout(&narrow_null, &narrow_base, &narrow_shift);
  if (res != AIEXT_OK) {
    return res;
  }

#define REPLACE_WITH_NATIVE(m, s, f)                                          \
  do {                                                                        \
    aiext_result_t res;                                                       \
    res = env->register_naccel_provider("TestAIExtension$Launcher", m, s, #f, \
                                        f, NULL);                             \
    if (res != AIEXT_OK) {                                                    \
      return res;                                                             \
    }                                                                         \
  } while (0)
#define REPLACE_WITH_PROVIDER(m, s, f)                                        \
  do {                                                                        \
    aiext_result_t res;                                                       \
    res = env->register_naccel_provider("TestAIExtension$Launcher", m, s, #f, \
                                        f, native_provider);                  \
    if (res != AIEXT_OK) {                                                    \
      return res;                                                             \
    }                                                                         \
  } while (0)

  REPLACE_WITH_NATIVE("hello", "()V", hello);
  REPLACE_WITH_NATIVE("hello", "(I)V", hello_int);
  REPLACE_WITH_NATIVE("hello", "(J)V", hello_long);
  REPLACE_WITH_NATIVE("hello", "(F)V", hello_float);
  REPLACE_WITH_NATIVE("hello", "(D)V", hello_double);
  REPLACE_WITH_NATIVE("hello", "([B)V", hello_bytes);
  REPLACE_WITH_NATIVE("hello", "(Ljava/lang/Object;)V", hello_object);
  REPLACE_WITH_NATIVE("hello", "(S)V", hello_short_method);

  REPLACE_WITH_NATIVE("add", "(II)I", add_ints);
  REPLACE_WITH_NATIVE("add", "(DD)D", add_doubles);
  REPLACE_WITH_NATIVE("add", "([I[I)V", add_arrays);

  REPLACE_WITH_PROVIDER("add_to_int", "(I)V", add_to_int);
  REPLACE_WITH_PROVIDER("add_to_double", "(D)V", add_to_double);

  REPLACE_WITH_PROVIDER("should_skip", "()V", NULL);

  REPLACE_WITH_PROVIDER("read_strs", "()V", read_strs);

  REPLACE_WITH_PROVIDER("add_to_static_int", "(I)V", add_to_static_int);
  REPLACE_WITH_PROVIDER("check_static_enum", "()V", check_static_enum);

#undef REPLACE_WITH_NATIVE
#undef REPLACE_WITH_PROVIDER
  return AIEXT_OK;
}

JNIEXPORT void JNICALL aiext_finalize(const aiext_env_t* env,
                                      aiext_handle_t handle) {
  printf("aiext_finalize\n");
}
