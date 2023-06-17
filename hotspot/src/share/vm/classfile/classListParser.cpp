/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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
#include "jvm.h"
#include "classfile/classListParser.hpp"
#include "classfile/classLoaderExt.hpp"
#include "classfile/sharedClassUtil.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/systemDictionaryShared.hpp"
#include "memory/metaspaceShared.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/fieldType.hpp"
#include "runtime/javaCalls.hpp"
#include "utilities/defaultStream.hpp"
#include "utilities/hashtable.inline.hpp"
#include "utilities/macros.hpp"

ClassListParser* ClassListParser::_instance = NULL;

ClassListParser::ClassListParser(const char* file) {
  assert(_instance == NULL, "must be singleton");
  _instance = this;
  _classlist_file = file;
  _file = fopen(file, "r");
  _line_no = 0;
  _interfaces = new (ResourceObj::C_HEAP, mtClass) GrowableArray<int>(10, true);

  if (_file == NULL) {
    char errmsg[JVM_MAXPATHLEN];
    os::lasterror(errmsg, JVM_MAXPATHLEN);
    vm_exit_during_initialization("Loading classlist failed", errmsg);
  }
}

ClassListParser::~ClassListParser() {
  if (_file) {
    fclose(_file);
  }
  _instance = NULL;
}

bool ClassListParser::parse_one_line() {
  for (;;) {
    if (fgets(_line, sizeof(_line), _file) == NULL) {
      return false;
    }
    ++ _line_no;
    _line_len = (int)strlen(_line);
    if (_line_len > _max_allowed_line_len) {
      error("input line too long (must be no longer than %d chars)", _max_allowed_line_len);
    }
    if (*_line == '#') { // comment
      continue;
    }
    break;
  }

  _id = _unspecified;
  _super = _unspecified;
  _interfaces->clear();
  _loader = _unspecified;
  _defining_loader_hash = _unspecified;
  _initiating_loader_hash = _unspecified;
  _source = NULL;
  _original_source = NULL;
  _interfaces_specified = false;
  _fingerprint = 0;
  _has_error = 0;

  {
    int len = (int)strlen(_line);
    int i;
    // Replace \t\r\n with ' '
    for (i=0; i<len; i++) {
      if (_line[i] == '\t' || _line[i] == '\r' || _line[i] == '\n') {
        _line[i] = ' ';
      }
    }

    // Remove trailing newline/space
    while (len > 0) {
      if (_line[len-1] == ' ') {
        _line[len-1] = '\0';
        len --;
      } else {
        break;
      }
    }
    _line_len = len;
    _class_name = _line;
  }

  if ((_token = strchr(_line, ' ')) == NULL) {
    // No optional arguments are specified.
    return true;
  }

  // Mark the end of the name, and go to the next input char
  *_token++ = '\0';

  while (*_token) {
    skip_whitespaces();

    if (parse_int_option("id:", &_id)) {
      continue;
    } else if (parse_int_option("super:", &_super)) {
      if(!check_already_loaded("Super class", _super)) {
        return true;
      }
      continue;
    } else if (skip_token("interfaces:")) {
      int i;
      while (try_parse_int(&i)) {
        if(!check_already_loaded("Interface", i)) {
          return true;
        }
        _interfaces->append(i);
      }
    } else if (skip_token("source:")) {
      skip_whitespaces();
      _source = _token;
      char* s = strchr(_token, ' ');
      if (s == NULL) {
        break; // end of input line
      } else {
        *s = '\0'; // mark the end of _source
        _token = s+1;
      }
    } else if (skip_token("origin:")) {
      skip_whitespaces();
      _original_source = _token;
      char *s = strchr(_token, ' ');
      if (s == NULL) {
        break; // end of input line
      } else {
        *s = '\0'; // mark the end of _source
        _token = s+1;
      }
    } else if (EagerAppCDS && parse_hex_option("defining_loader_hash:", &_defining_loader_hash)) {
      continue;
    } else if (EagerAppCDS && parse_hex_option("initiating_loader_hash:", &_initiating_loader_hash)) {
      continue;
    } else if (EagerAppCDS && parse_uint64_option("fingerprint:", &_fingerprint)) {
      continue;
    } else if (skip_token("_initiating_loader_name:") || skip_token("_thread_name:")) {
      skip_whitespaces();
      char* s = strchr(_token, ' ');
      if (s == NULL) {
        break; // end of input line
      } else {
        *s = '\0'; // mark the end of _source
        _token = s+1;
      }
    } else {
      error("Unknown input");
    }
  }

  // if src is specified
  //     id super interfaces must all be specified
  //     loader may be specified
  // else
  //     # the class is loaded from classpath
  //     id may be specified
  //     super, interfaces, loader must not be specified
  return true;
}

void ClassListParser::skip_whitespaces() {
  while (*_token == ' ' || *_token == '\t') {
    _token ++;
  }
}

void ClassListParser::skip_non_whitespaces() {
  while (*_token && *_token != ' ' && *_token != '\t') {
    _token ++;
  }
}

void ClassListParser::parse_int(int* value) {
  skip_whitespaces();
  if (sscanf(_token, "%i", value) == 1) {
    skip_non_whitespaces();
    if (*value < 0) {
      error("Error: negative integers not allowed (%d)", *value);
    }
  } else {
    error("Error: expected integer");
  }
}

bool ClassListParser::try_parse_int(int* value) {
  skip_whitespaces();
  if (sscanf(_token, "%i", value) == 1) {
    skip_non_whitespaces();
    return true;
  }
  return false;
}

bool ClassListParser::skip_token(const char* option_name) {
  size_t len = strlen(option_name);
  if (strncmp(_token, option_name, len) == 0) {
    _token += len;
    return true;
  } else {
    return false;
  }
}

bool ClassListParser::parse_int_option(const char* option_name, int* value) {
  if (skip_token(option_name)) {
    if (*value != _unspecified) {
      error("%s specified twice", option_name);
    } else {
      parse_int(value);
      return true;
    }
  }
  return false;
}

bool ClassListParser::parse_hex_option(const char* option_name, int* value) {
  if (!skip_token(option_name)) return false;
  if (*value != _unspecified) {
    error("%s specified twice", option_name);
  }
  skip_whitespaces();
  if (sscanf(_token, "%x", value) == 1) {
    skip_non_whitespaces();
    return true;
  } else {
    error("Error: expected hex");
  }
  return false;
}

bool ClassListParser::parse_uint64_option(const char* option_name, uint64* value) {
  if (!skip_token(option_name)) return false;
  skip_whitespaces();
  if (sscanf(_token, PTR64_FORMAT, value) == 1) {
    skip_non_whitespaces();
    return true;
  } else {
    error("Error: expected hex");
  }
  return false;
}

void ClassListParser::print_specified_interfaces() {
  const int n = _interfaces->length();
  jio_fprintf(defaultStream::error_stream(), "Currently specified interfaces[%d] = {\n", n);
  for (int i=0; i<n; i++) {
    InstanceKlass* k = lookup_class_by_id(_interfaces->at(i));
    jio_fprintf(defaultStream::error_stream(), "  %4d = %s\n", _interfaces->at(i), k->name()->as_klass_external_name());
  }
  jio_fprintf(defaultStream::error_stream(), "}\n");
}

void ClassListParser::print_actual_interfaces(InstanceKlass *ik) {
  int n = ik->local_interfaces()->length();
  jio_fprintf(defaultStream::error_stream(), "Actual interfaces[%d] = {\n", n);
  for (int i = 0; i < n; i++) {
    InstanceKlass* e = InstanceKlass::cast(ik->local_interfaces()->at(i));
    jio_fprintf(defaultStream::error_stream(), "  %s\n", e->name()->as_klass_external_name());
  }
  jio_fprintf(defaultStream::error_stream(), "}\n");
}

void ClassListParser::error(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  int error_index = _token - _line;
  if (error_index >= _line_len) {
    error_index = _line_len - 1;
  }
  if (error_index < 0) {
    error_index = 0;
  }

  jio_fprintf(defaultStream::error_stream(),
              "An error has occurred while processing class list file %s %d:%d.\n",
              _classlist_file, _line_no, (error_index + 1));
  jio_vfprintf(defaultStream::error_stream(), msg, ap);

  if (_line_len <= 0) {
    jio_fprintf(defaultStream::error_stream(), "\n");
  } else {
    jio_fprintf(defaultStream::error_stream(), ":\n");
    for (int i=0; i<_line_len; i++) {
      char c = _line[i];
      if (c == '\0') {
        jio_fprintf(defaultStream::error_stream(), "%s", " ");
      } else {
        jio_fprintf(defaultStream::error_stream(), "%c", c);
      }
    }
    jio_fprintf(defaultStream::error_stream(), "\n");
    for (int i=0; i<error_index; i++) {
      jio_fprintf(defaultStream::error_stream(), "%s", " ");
    }
    jio_fprintf(defaultStream::error_stream(), "^\n");
  }

  vm_exit_during_initialization("class list format error.", NULL);
  va_end(ap);
}

// This function is used for loading classes for customized class loaders
// during archive dumping.
InstanceKlass* ClassListParser::load_class_from_source(Symbol* class_name, TRAPS) {
#if !((defined(LINUX) && defined(X86) && defined(_LP64)) || \
      (defined(SOLARIS) && defined(_LP64)) || (defined(LINUX) && defined(AARCH64)))
  // The only supported platforms are: (1) Linux/AMD64; (2) Solaris/64-bit; (3)AArch64
  error("AppCDS custom class loaders not supported on this platform");
#endif

  assert(UseAppCDS, "must be");
  if (strncmp(_class_name, "java/", 5) == 0) {
    tty->print_cr("Prohibited package for non-bootstrap classes: %s.class from %s",
                  _class_name, _source);
    return NULL;
  }

  if (!is_super_specified()) {
    error("If source location is specified, super class must be also specified");
  }
  if (!is_id_specified()) {
    error("If source location is specified, id must be also specified");
  }
  InstanceKlass* k = ClassLoaderExt::load_class(class_name, _source,
      _defining_loader_hash == _unspecified ? 0 : _defining_loader_hash,
      _initiating_loader_hash == _unspecified ? 0 : _initiating_loader_hash,
      _fingerprint,
      THREAD);

  if (k != NULL) {
    if (EagerAppCDS) {
      k->set_source_file_path(SymbolTable::new_symbol(_original_source, THREAD));
    }
    if (k->local_interfaces()->length() != _interfaces->length()) {
      print_specified_interfaces();
      print_actual_interfaces(k);
      error("The number of interfaces (%d) specified in class list does not match the class file (%d)",
            _interfaces->length(), k->local_interfaces()->length());
    }

    if (!SystemDictionaryShared::add_non_builtin_klass(class_name, ClassLoaderData::the_null_class_loader_data(),
                                                       k, THREAD)) {
      error("Duplicated class %s", _class_name);
    }

    // This tells JVM_FindLoadedClass to not find this class.
    k->set_shared_classpath_index(UNREGISTERED_INDEX);
  }

  return k;
}

InstanceKlass* ClassListParser::load_current_class(TRAPS) {
  TempNewSymbol class_name_symbol = SymbolTable::new_symbol(_class_name, THREAD);
  guarantee(!HAS_PENDING_EXCEPTION, "Exception creating a symbol.");

  InstanceKlass *klass = NULL;
  if (!is_loading_from_source()) {
    if (is_super_specified()) {
      error("If source location is not specified, super class must not be specified");
    }
    if (are_interfaces_specified()) {
      error("If source location is not specified, interface(s) must not be specified");
    }

    bool non_array = !FieldType::is_array(class_name_symbol);

    JavaValue result(T_OBJECT);
    if (non_array) {
      // At this point, we are executing in the context of the boot loader. We
      // cannot call Class.forName because that is context dependent and
      // would load only classes for the boot loader.
      //
      // Instead, let's call java_system_loader().loadClass() directly, which will
      // delegate to the correct loader (boot, platform or app) depending on
      // the class name.

      Handle s = java_lang_String::create_from_symbol(class_name_symbol, CHECK_0);
      // ClassLoader.loadClass() wants external class name format, i.e., convert '/' chars to '.'
      Handle ext_class_name = java_lang_String::externalize_classname(s, CHECK_0);
      KlassHandle spec_klass = KlassHandle(THREAD, SystemDictionary::ClassLoader_klass());
      Handle class_loader = Handle(THREAD, SystemDictionary::java_system_loader());
      JavaCalls::call_virtual(&result,
                              class_loader,  // receiver
                              spec_klass,
                              vmSymbols::loadClass_name(),
                              vmSymbols::string_class_signature(),
                              ext_class_name,
                              THREAD);

    } else {
      // array classes are not supported in class list.
      THROW_NULL(vmSymbols::java_lang_ClassNotFoundException());
    }

    assert(result.get_type() == T_OBJECT, "just checking");
    oop obj = (oop) result.get_jobject();
    if (!HAS_PENDING_EXCEPTION && (obj != NULL)) {
      klass = InstanceKlass::cast(java_lang_Class::as_Klass(obj));
    } else { // load classes in bootclasspath/a
      if (HAS_PENDING_EXCEPTION) {
        // tty->print_cr("ClassLoader.loadClass return exception: %s", obj->klass()->external_name());
        CLEAR_PENDING_EXCEPTION;
      }
      if (non_array) {
        Klass* k = SystemDictionary::resolve_or_null(class_name_symbol, CHECK_NULL);
        if (k != NULL) {
          klass = InstanceKlass::cast(k);
        } else {
          if (!HAS_PENDING_EXCEPTION) {
            THROW_NULL(vmSymbols::java_lang_ClassNotFoundException());
          }
        }
      }
    }
  } else {
    // If "source:" tag is specified, all super class and super interfaces must be specified in the
    // class list file.
    if (UseAppCDS) {
      if (NotFoundClassOpt && strstr(_source, NOT_FOUND_CLASS)) {
        SystemDictionaryShared::record_not_found_class(class_name_symbol,
                                                       _initiating_loader_hash == _unspecified ? 0 : _initiating_loader_hash);
        return NULL;
      } else {
        klass = load_class_from_source(class_name_symbol, CHECK_NULL);
      }
    }
  }

  if (klass != NULL && is_id_specified()) {
    int id = this->id();
    SystemDictionaryShared::update_shared_entry(klass, id);
    InstanceKlass* old = table()->lookup(id);
    if (old != NULL && old != klass) {
      error("Duplicated ID %d for class %s", id, _class_name);
    }
    table()->add(id, klass);
    // set the class is shared
    oop loader = klass->class_loader_data()->class_loader();
    ClassLoader::ClassLoaderType loader_type = ClassLoader::BOOT_LOADER;
    // in case app class loaded by app loader
    if (SystemDictionary::is_system_class_loader(loader) ||
        klass->shared_classpath_index() >= ClassLoaderExt::app_paths_start_index()) {
      loader_type = ClassLoader::APP_LOADER;
    } else if (SystemDictionary::is_ext_class_loader(loader)) {
      loader_type = ClassLoader::EXT_LOADER;
    } else if (SystemDictionary::is_cus_class_loader(loader) || klass->shared_classpath_index() == UNREGISTERED_INDEX) {
      loader_type = ClassLoader::CUS_LOADER;
    }
#ifndef PRODUCT
    klass->set_cds_id(_id);
#endif
    klass->set_class_loader_type(loader_type);
  }

  return klass;
}

bool ClassListParser::is_loading_from_source() {
  return (_source != NULL);
}

InstanceKlass* ClassListParser::lookup_class_by_id(int id) {
  InstanceKlass* klass = table()->lookup(id);
  if (klass == NULL) {
    error("Class ID %d has not been defined", id);
  }
  return klass;
}


InstanceKlass* ClassListParser::lookup_super_for_current_class(Symbol* super_name) {
  if (!is_loading_from_source()) {
    return NULL;
  }

  InstanceKlass* k = lookup_class_by_id(super());
  if (super_name != k->name()) {
    error("The specified super class %s (id %d) does not match actual super class %s",
          k->name()->as_klass_external_name(), super(),
          super_name->as_klass_external_name());
  }
  return k;
}

InstanceKlass* ClassListParser::lookup_interface_for_current_class(Symbol* interface_name) {
  if (!is_loading_from_source()) {
    return NULL;
  }

  const int n = _interfaces->length();
  if (n == 0) {
    error("Class %s implements the interface %s, but no interface has been specified in the input line",
          _class_name, interface_name->as_klass_external_name());
    ShouldNotReachHere();
  }

  int i;
  for (i=0; i<n; i++) {
    InstanceKlass* k = lookup_class_by_id(_interfaces->at(i));
    if (interface_name == k->name()) {
      return k;
    }
  }

  // interface_name is not specified by the "interfaces:" keyword.
  print_specified_interfaces();
  error("The interface %s implemented by class %s does not match any of the specified interface IDs",
        interface_name->as_klass_external_name(), _class_name);
  ShouldNotReachHere();
  return NULL;
}
