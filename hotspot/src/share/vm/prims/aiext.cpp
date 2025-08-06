/*
 * Copyright (c) 2025 Alibaba Group Holding Limited. All Rights Reserved.
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

#include "aiext.h"

#include <string.h>

#include "precompiled.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/dictionary.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "memory/universe.hpp"
#include "oops/oop.inline.hpp"
#include "opto/aiExtension.hpp"
#include "opto/aiExtensionLog.hpp"
#include "runtime/fieldDescriptor.hpp"
#include "runtime/fieldType.hpp"
#include "runtime/globals.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/thread.hpp"

static const unsigned int CURRENT_VERSION = AIEXT_VERSION_2;

// Returns JVM version string.
static aiext_result_t get_jvm_version(char* buf, size_t buf_size) {
  if (buf == NULL || buf_size == 0) {
    log_info(aiext)("No output buffer for return value");
    return AIEXT_ERROR;
  }
  snprintf(buf, buf_size, "%s", Abstract_VM_Version::jre_release_version());
  return AIEXT_OK;
}

// Returns current AI-Extension version.
static unsigned int get_aiext_version() { return CURRENT_VERSION; }

#define DEF_GET_JVM_FLAG(n, t)                                         \
  static aiext_result_t get_jvm_flag_##n(const char* name, t* value) { \
    if (name == NULL) {                                                \
      log_info(aiext)("Invalid flag name");                            \
      return AIEXT_ERROR;                                              \
    }                                                                  \
    Flag* flag = Flag::find_flag(name, strlen(name));                  \
    if (flag == NULL || !flag->is_##n()) {                             \
      log_info(aiext)("Flag %s not found or type mismatch", name);     \
      return AIEXT_ERROR;                                              \
    }                                                                  \
    if (value == NULL) {                                               \
      log_info(aiext)("Invalid value pointer");                        \
      return AIEXT_ERROR;                                              \
    }                                                                  \
    *value = flag->get_##n();                                          \
    return AIEXT_OK;                                                   \
  }
#define DEF_GET_JVM_FLAG_UNAVAILABLE(n, t)                                   \
  static aiext_result_t get_jvm_flag_##n(const char* name, t* value) {       \
    log_info(aiext)("`get_jvm_flag_%s` is not available on the current JVM", \
                    #n);                                                     \
    return AIEXT_ERROR;                                                      \
  }

DEF_GET_JVM_FLAG(bool, int)
DEF_GET_JVM_FLAG_UNAVAILABLE(int, int)
DEF_GET_JVM_FLAG_UNAVAILABLE(uint, unsigned int)
DEF_GET_JVM_FLAG(intx, intptr_t)
DEF_GET_JVM_FLAG(uintx, uintptr_t)
DEF_GET_JVM_FLAG(uint64_t, uint64_t)
DEF_GET_JVM_FLAG_UNAVAILABLE(size_t, size_t)
DEF_GET_JVM_FLAG(double, double)

static aiext_result_t get_jvm_flag_ccstr(const char* name, char* buf,
                                         size_t buf_size) {
  if (name == NULL) {
    log_info(aiext)("Invalid flag name");
    return AIEXT_ERROR;
  }
  Flag* flag = Flag::find_flag(name, strlen(name));
  if (flag == NULL || !flag->is_ccstr()) {
    log_info(aiext)("Flag %s not found or type mismatch", name);
    return AIEXT_ERROR;
  }
  if (buf == NULL || buf_size == 0) {
    log_info(aiext)("No output buffer for return value");
    return AIEXT_ERROR;
  }
  snprintf(buf, buf_size, "%s", flag->get_ccstr());
  return AIEXT_OK;
}

#undef DEF_GET_JVM_FLAG
#undef DEF_GET_JVM_FLAG_UNAVAILABLE

#define DEF_SET_JVM_FLAG(n, t)                                              \
  static aiext_result_t set_jvm_flag_##n(const char* name, t value) {       \
    if (name == NULL) {                                                     \
      log_info(aiext)("Invalid flag name");                                 \
      return AIEXT_ERROR;                                                   \
    }                                                                       \
    bool result = CommandLineFlags::n##AtPut(name, &value, Flag::INTERNAL); \
    return result ? AIEXT_OK : AIEXT_ERROR;                                 \
  }
#define DEF_SET_JVM_FLAG_UNAVAILABLE(n, t)                                   \
  static aiext_result_t set_jvm_flag_##n(const char* name, t value) {        \
    log_info(aiext)("`get_jvm_flag_%s` is not available on the current JVM", \
                    #n);                                                     \
    return AIEXT_ERROR;                                                      \
  }

static aiext_result_t set_jvm_flag_bool(const char* name, int value) {
  if (name == NULL) {
    log_info(aiext)("Invalid flag name");
    return AIEXT_ERROR;
  }
  bool b = !!value;
  bool result = CommandLineFlags::boolAtPut(name, &b, Flag::INTERNAL);
  return result ? AIEXT_OK : AIEXT_ERROR;
}

DEF_SET_JVM_FLAG_UNAVAILABLE(int, int)
DEF_SET_JVM_FLAG_UNAVAILABLE(uint, unsigned int)
DEF_SET_JVM_FLAG(intx, intptr_t)
DEF_SET_JVM_FLAG(uintx, uintptr_t)
DEF_SET_JVM_FLAG(uint64_t, uint64_t)
DEF_SET_JVM_FLAG_UNAVAILABLE(size_t, size_t)
DEF_SET_JVM_FLAG(double, double)

static aiext_result_t set_jvm_flag_ccstr(const char* name, const char* value) {
  if (name == NULL) {
    log_info(aiext)("Invalid flag name");
    return AIEXT_ERROR;
  }
  bool result = CommandLineFlags::ccstrAtPut(name, &value, Flag::INTERNAL);
  FREE_C_HEAP_ARRAY(char, value, mtInternal);
  return result ? AIEXT_OK : AIEXT_ERROR;
}

#undef DEF_SET_JVM_FLAG
#undef DEF_SET_JVM_FLAG_UNAVAILABLE

// Gets the current Java thread.
static JavaThread* get_current_java_thread() {
  Thread* thread = Thread::current();
  if (!thread->is_Java_thread()) {
    log_info(aiext)("Current thread is not a Java thread");
    return NULL;
  }
  return (JavaThread*)thread;
}

// Guard for restoring pending exceptions.
struct PendingExceptionGuard {
  Thread* thread;
  Handle pending_exception;
  const char* exception_file;
  int exception_line;

  PendingExceptionGuard(TRAPS) {
    thread = THREAD;
    pending_exception = Handle(THREAD, PENDING_EXCEPTION);
    exception_file = THREAD->exception_file();
    exception_line = THREAD->exception_line();
    CLEAR_PENDING_EXCEPTION;
  }

  ~PendingExceptionGuard() {
    // Restore pending exception.
    if (pending_exception.not_null()) {
      thread->set_pending_exception(pending_exception(), exception_file,
                                    exception_line);
    }
  }

  void log(const char* op) const {
    if (log_is_enabled(Info, aiext)) {
      oop ex = thread->pending_exception();
      log_info(aiext)(
          "Exception while %s, %s: %s", op, ex->klass()->external_name(),
          java_lang_String::as_utf8_string(java_lang_Throwable::message(ex)));
    }
  }
};

// Registers native acceleration provider for specific Java method.
static aiext_result_t register_naccel_provider(
    const char* klass, const char* method, const char* sig,
    const char* native_func_name, void* func_or_data,
    aiext_naccel_provider_t provider) {
  JavaThread* thread = get_current_java_thread();
  ThreadInVMfromNative state_guard(thread);
  ResetNoHandleMark rnhm;
  HandleMark hm(thread);
  PendingExceptionGuard except_guard(thread);

  bool result = AIExt::add_entry(klass, method, sig, native_func_name,
                                 func_or_data, provider, thread);
  if (thread->has_pending_exception()) {
    except_guard.log("adding native acceleration entry");
  }

  return result ? AIEXT_OK : AIEXT_ERROR;
}

// Gets unit info, including feature name, version and parameter list.
static aiext_result_t get_unit_info(aiext_handle_t handle, char* feature_buf,
                                    size_t feature_buf_size, char* version_buf,
                                    size_t version_buf_size,
                                    char* param_list_buf,
                                    size_t param_list_buf_size) {
  // Find the given unit.
  const AIExtUnit* unit = AIExt::find_unit(handle);
  if (unit == NULL) {
    return AIEXT_ERROR;
  }

  // Copy to buffers.
  if (feature_buf != NULL && feature_buf_size > 0) {
    snprintf(feature_buf, feature_buf_size, "%s", unit->feature());
  }
  if (version_buf != NULL && version_buf_size > 0) {
    snprintf(version_buf, version_buf_size, "%s", unit->version());
  }
  if (param_list_buf != NULL && param_list_buf_size > 0) {
    const char* param_list = unit->param_list();
    if (param_list == NULL) {
      param_list = "";
    }
    snprintf(param_list_buf, param_list_buf_size, "%s", param_list);
  }
  return AIEXT_OK;
}

// Gets JNI environment.
static JNIEnv* get_jni_env() {
  return JavaThread::current()->jni_environment();
}

// Converts `aiext_value_type_t` to `BasicType`.
static aiext_result_t to_basic_type(aiext_value_type_t type, BasicType& bt) {
  switch (type) {
    case AIEXT_TYPE_BOOLEAN:
      bt = T_BOOLEAN;
      break;
    case AIEXT_TYPE_CHAR:
      bt = T_CHAR;
      break;
    case AIEXT_TYPE_FLOAT:
      bt = T_FLOAT;
      break;
    case AIEXT_TYPE_DOUBLE:
      bt = T_DOUBLE;
      break;
    case AIEXT_TYPE_BYTE:
      bt = T_BYTE;
      break;
    case AIEXT_TYPE_SHORT:
      bt = T_SHORT;
      break;
    case AIEXT_TYPE_INT:
      bt = T_INT;
      break;
    case AIEXT_TYPE_LONG:
      bt = T_LONG;
      break;
    case AIEXT_TYPE_OBJECT:
      bt = T_OBJECT;
      break;
    case AIEXT_TYPE_ARRAY:
      bt = T_ARRAY;
      break;
    default:
      log_info(aiext)("Invalid value type %d", type);
      return AIEXT_ERROR;
  }
  return AIEXT_OK;
}

// Gets Java array layout.
static aiext_result_t get_array_layout(aiext_value_type_t elem_type,
                                       size_t* length_offset,
                                       size_t* data_offset, size_t* elem_size) {
  BasicType bt;
  aiext_result_t result = to_basic_type(elem_type, bt);
  if (result != AIEXT_OK) {
    return result;
  }

  if (length_offset != NULL) {
    *length_offset = arrayOopDesc::length_offset_in_bytes();
  }
  if (data_offset != NULL) {
    *data_offset = arrayOopDesc::base_offset_in_bytes(bt);
  }
  if (elem_size != NULL) {
    *elem_size = type2aelembytes(bt);
  }

  return AIEXT_OK;
}

// Gets the layout of narrow oop.
static aiext_result_t get_narrow_oop_layout(uint32_t* null, uintptr_t* base,
                                            size_t* shift) {
  if (null != NULL) {
    *null = 0;
  }
  if (base != NULL) {
    *base = (uintptr_t)Universe::narrow_oop_base();
  }
  if (shift != NULL) {
    *shift = Universe::narrow_oop_shift();
  }
  return AIEXT_OK;
}

// Finds the given class in the given class loader.
static Klass* find_class(Symbol* class_name, Handle class_loader,
                         Handle protection_domain, TRAPS) {
  if (FieldType::is_array(class_name) || FieldType::is_obj(class_name)) {
    return NULL;
  }

  class_loader = Handle(
      THREAD,
      java_lang_ClassLoader::non_reflection_class_loader(class_loader()));
  ClassLoaderData* loader_data =
      class_loader() == NULL
          ? ClassLoaderData::the_null_class_loader_data()
          : ClassLoaderDataGraph::find_or_create(class_loader, THREAD);

  Dictionary* dictionary = SystemDictionary::dictionary();
  unsigned int d_hash = dictionary->compute_hash(class_name, loader_data);
  int d_index = dictionary->hash_to_index(d_hash);
  return dictionary->find(d_index, d_hash, class_name, loader_data,
                          protection_domain, THREAD);
}

// Gets the field descriptor of the given field.
// Returns `false` on failure.
static bool get_field_descriptor(const char* klass, const char* field,
                                 const char* sig, bool is_static,
                                 fieldDescriptor& fd, TRAPS) {
  JavaThread* thread = (JavaThread*)THREAD;

  // Check class name symbol.
  if (klass == NULL || (int)strlen(klass) > Symbol::max_length()) {
    log_info(aiext)("Invalid class name %s", klass == NULL ? "<null>" : klass);
    return false;
  }

  // Extract pending exception.
  PendingExceptionGuard except_guard(THREAD);

  // Get class loader.
  Handle protection_domain;
  Handle loader(THREAD, SystemDictionary::java_system_loader());
  Klass* k = thread->security_get_caller_class(0);
  if (k != NULL) {
    loader = Handle(THREAD, k->class_loader());
  }

  // Find class from the class loader.
  TempNewSymbol class_name = SymbolTable::new_symbol(klass, THREAD);
  k = find_class(class_name, loader, protection_domain, THREAD);
  if (HAS_PENDING_EXCEPTION) {
    except_guard.log("resolving class");
    k = NULL;
  }
  if (k == NULL) {
    log_info(aiext)("Class %s not found", klass);
    return false;
  }
  if (!k->oop_is_instance()) {
    log_info(aiext)("Class %s is not an instance class", klass);
    return false;
  }
  InstanceKlass* ik = InstanceKlass::cast(k);
  if (!ik->is_initialized()) {
    log_info(aiext)("Class %s is not initialized", klass);
    return false;
  }

  // The class should have been loaded, so the field and signature
  // should already be in the symbol table.
  // If they're not there, the field doesn't exist.
  TempNewSymbol field_name = SymbolTable::probe(field, (int)strlen(field));
  TempNewSymbol sig_name = SymbolTable::probe(sig, (int)strlen(sig));
  if (field_name == NULL || sig_name == NULL ||
      ik->find_field(field_name, sig_name, is_static, &fd) == NULL) {
    log_info(aiext)("Field %s.%s not found in class %s", field, sig, klass);
    return false;
  }

  // Done.
  return true;
}

// Gets field offset in a Java class, returns `-1` on failure.
static int get_field_offset(const char* klass, const char* field,
                            const char* sig) {
  // Get the current Java thread.
  JavaThread* thread = get_current_java_thread();
  if (thread == NULL) {
    return -1;
  }

  // Transition thread state to VM.
  ThreadInVMfromNative state_guard(thread);
  ResetNoHandleMark rnhm;
  HandleMark hm(thread);

  fieldDescriptor fd;
  if (!get_field_descriptor(klass, field, sig, false, fd, thread)) {
    return -1;
  }
  return fd.offset();
}

// Gets address of the given static field in a Java class,
// returns `NULL` on failure.
static void* get_static_field_addr(const char* klass, const char* field,
                                   const char* sig) {
  // Get the current Java thread.
  JavaThread* thread = get_current_java_thread();
  if (thread == NULL) {
    return NULL;
  }

  // Transition thread state to VM.
  ThreadInVMfromNative state_guard(thread);
  ResetNoHandleMark rnhm;
  HandleMark hm(thread);

  fieldDescriptor fd;
  if (!get_field_descriptor(klass, field, sig, true, fd, thread)) {
    return NULL;
  }
  return (void*)fd.field_holder()->java_mirror()->address_field_addr(
      fd.offset());
}

extern const aiext_env_t GLOBAL_AIEXT_ENV = {
    // Version.
    get_jvm_version,
    get_aiext_version,

    // JVM flag access.
    get_jvm_flag_bool,
    get_jvm_flag_int,
    get_jvm_flag_uint,
    get_jvm_flag_intx,
    get_jvm_flag_uintx,
    get_jvm_flag_uint64_t,
    get_jvm_flag_size_t,
    get_jvm_flag_double,
    get_jvm_flag_ccstr,
    set_jvm_flag_bool,
    set_jvm_flag_int,
    set_jvm_flag_uint,
    set_jvm_flag_intx,
    set_jvm_flag_uintx,
    set_jvm_flag_uint64_t,
    set_jvm_flag_size_t,
    set_jvm_flag_double,
    set_jvm_flag_ccstr,

    // Native acceleration.
    register_naccel_provider,

    // Unit information.
    get_unit_info,

    // JNI.
    get_jni_env,

    // Object/pointer layout.
    get_array_layout,
    get_narrow_oop_layout,
    get_field_offset,
    get_static_field_addr,
};
