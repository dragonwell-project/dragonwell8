/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/classFileParser.hpp"
#include "classfile/classFileStream.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/classLoaderExt.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/sharedClassUtil.hpp"
#include "classfile/sharedPathsMiscInfo.hpp"
#include "classfile/systemDictionaryShared.hpp"
#include "classfile/classListParser.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/filemap.hpp"
#include "memory/oopFactory.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/symbol.hpp"
#include "prims/jvmtiImpl.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "prims/jvmtiRedefineClasses.hpp"
#include "runtime/arguments.hpp"
#include "runtime/fprofiler.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/os.hpp"
#include "services/threadService.hpp"
#include "utilities/stringUtils.hpp"

jshort ClassLoaderExt::_app_paths_start_index = ClassLoaderExt::max_classpath_index;
bool ClassLoaderExt::_has_app_classes = false;
bool ClassLoaderExt::_has_ext_classes = false;
bool ClassLoaderExt::_has_cus_classes = false;

void ClassLoaderExt::setup_app_search_path() {
  assert(DumpSharedSpaces, "this function is only used with -Xshare:dump and -XX:+UseAppCDS");
  _app_paths_start_index = _num_entries;
  char* app_class_path = os::strdup(Arguments::get_appclasspath());

  if (strcmp(app_class_path, ".") == 0) {
    // This doesn't make any sense, even for AppCDS, so let's skip it. We
    // don't want to throw an error here because -cp "." is usually assigned
    // by the launcher when classpath is not specified.
    trace_class_path(tty, "app loader class path (skipped)=", app_class_path);
  } else {
    trace_class_path(tty, "app loader class path=", app_class_path);
    shared_paths_misc_info()->add_app_classpath(app_class_path);
    ClassLoader::setup_search_path(app_class_path);
  }
}

void ClassLoaderExt::add_class_path_entry(const char* path,
                                          bool check_for_duplicates,
                                          ClassPathEntry* new_entry) {
  add_to_list(new_entry);
  if (_app_paths_start_index == ClassLoaderExt::max_classpath_index) {
    // We are still processing the boot JAR files (which do NOT
    // support Class-Path: attribute, so no need to process
    // their manifest files)
  } else {
    if (DumpSharedSpaces) {
      if (new_entry->is_jar_file()) {
        ClassLoaderExt::process_jar_manifest(new_entry, check_for_duplicates);
      } else {
        if (!IgnoreAppCDSDirCheck) {
          if (strcmp(path, ".") != 0 && !os::dir_is_empty(path)) {
            tty->print_cr("Error: non-empty directory '%s'", path);
            exit_with_path_failure("Cannot have non-empty directory in boot/ext/app classpaths", NULL);
          }
        }
      }
    }
  }
}

char* ClassLoaderExt::read_manifest(ClassPathEntry* entry, jint *manifest_size, bool clean_text, TRAPS) {
  const char* name = "META-INF/MANIFEST.MF";
  char* manifest;
  jint size;

  assert(entry->is_jar_file(), "must be");

  if (!entry->is_lazy()) {
    manifest = (char*) ((ClassPathZipEntry*)entry )->open_entry(name, &size, true, CHECK_NULL);
  } else {
    manifest = (char*) ((LazyClassPathEntry*)entry)->open_entry(name, &size, true, CHECK_NULL);
  }

  if (manifest == NULL) { // No Manifest
    *manifest_size = 0;
    return NULL;
  }


  if (clean_text) {
    // See http://docs.oracle.com/javase/6/docs/technotes/guides/jar/jar.html#JAR%20Manifest
    // (1): replace all CR/LF and CR with LF
    StringUtils::replace_no_expand(manifest, "\r\n", "\n");

    // (2) remove all new-line continuation (remove all "\n " substrings)
    StringUtils::replace_no_expand(manifest, "\n ", "");
  }

  *manifest_size = (jint)strlen(manifest);
  return manifest;
}

char* ClassLoaderExt::get_class_path_attr(const char* jar_path, char* manifest, jint manifest_size) {
  const char* tag = "Class-Path: ";
  const int tag_len = (int)strlen(tag);
  char* found = NULL;
  char* line_start = manifest;
  char* end = manifest + manifest_size;

  assert(*end == 0, "must be nul-terminated");

  while (line_start < end) {
    char* line_end = strchr(line_start, '\n');
    if (line_end == NULL) {
      // JAR spec require the manifest file to be terminated by a new line.
      break;
    }
    if (strncmp(tag, line_start, tag_len) == 0) {
      if (found != NULL) {
        // Same behavior as jdk/src/share/classes/java/util/jar/Attributes.java
        // If duplicated entries are found, the last one is used.
        tty->print_cr("Warning: Duplicate name in Manifest: %s.\n"
                      "Ensure that the manifest does not have duplicate entries, and\n"
                      "that blank lines separate individual sections in both your\n"
                      "manifest and in the META-INF/MANIFEST.MF entry in the jar file:\n%s\n", tag, jar_path);
      }
      found = line_start + tag_len;
      assert(found <= line_end, "sanity");
      *line_end = '\0';
    }
    line_start = line_end + 1;
  }
  return found;
}

void ClassLoaderExt::process_jar_manifest(ClassPathEntry* entry,
                                          bool check_for_duplicates) {
  Thread* THREAD = Thread::current();
  ResourceMark rm(THREAD);
  jint manifest_size;
  char* manifest = read_manifest(entry, &manifest_size, CHECK);

  if (manifest == NULL) {
    return;
  }

  if (strstr(manifest, "Extension-List:") != NULL) {
    tty->print_cr("-Xshare:dump does not support Extension-List in JAR manifest: %s", entry->name());
    vm_exit(1);
  }

  char* cp_attr = get_class_path_attr(entry->name(), manifest, manifest_size);

  if (cp_attr != NULL && strlen(cp_attr) > 0) {
    ClassLoader::trace_class_path(tty, "found Class-Path: ", cp_attr);

    char sep = os::file_separator()[0];
    const char* dir_name = entry->name();
    const char* dir_tail = strrchr(dir_name, sep);
    int dir_len;
    if (dir_tail == NULL) {
      dir_len = 0;
    } else {
      dir_len = dir_tail - dir_name + 1;
    }

    // Split the cp_attr by spaces, and add each file
    char* file_start = cp_attr;
    char* end = file_start + strlen(file_start);

    while (file_start < end) {
      char* file_end = strchr(file_start, ' ');
      if (file_end != NULL) {
        *file_end = 0;
        file_end += 1;
      } else {
        file_end = end;
      }

      int name_len = (int)strlen(file_start);
      if (name_len > 0) {
        ResourceMark rm(THREAD);
        char* libname = NEW_RESOURCE_ARRAY(char, dir_len + name_len + 1);
        *libname = 0;
        strncat(libname, dir_name, dir_len);
        strncat(libname, file_start, name_len);
        ClassLoader::trace_class_path(tty, "library = ", libname);
        ClassLoader::update_class_path_entry_list(libname, true, false);
      }

      file_start = file_end;
    }
  }
}

void ClassLoaderExt::setup_search_paths() {
  if (UseAppCDS) {
    shared_paths_misc_info()->record_app_offset();
    ClassLoaderExt::setup_app_search_path();
  }
}

bool ClassLoaderExt::check(ClassLoaderExt::Context *context,
                           const ClassFileStream* stream,
                           const int classpath_index) {
  if (stream != NULL) {
    // Ignore any App classes from signed JAR file during CDS archiving
    // dumping
    if (DumpSharedSpaces &&
        SharedClassUtil::is_classpath_entry_signed(classpath_index) &&
        classpath_index >= _app_paths_start_index) {
      tty->print_cr("Preload Warning: Skipping %s from signed JAR",
                    context->class_name());
      return false;
    }
    if (classpath_index >= _app_paths_start_index) {
      _has_app_classes = true;
      _has_ext_classes = true;
    }
  }

  return true;
}

InstanceKlass* ClassLoaderExt::record_result(ClassLoaderExt::Context *context,
                                             Symbol* class_name,
                                             ClassPathEntry* e,
                                             const s2 classpath_index,
                                             InstanceKlass* result,
                                             TRAPS) {

  oop loader = result->class_loader();
  ClassLoader::ClassLoaderType classloader_type = ClassLoader::BOOT_LOADER;
  if (DumpSharedSpaces) {
    // We need to remember where the class comes from during dumping.
    // App class via ClassLoader.loadClass may via bootStrap loader.
    if (SystemDictionary::is_system_class_loader(loader) ||
        classpath_index >= ClassLoaderExt::_app_paths_start_index) {
      classloader_type = ClassLoader::APP_LOADER;
      ClassLoaderExt::set_has_app_classes();
    } else if (SystemDictionary::is_ext_class_loader(loader)) {
      classloader_type = ClassLoader::EXT_LOADER;
      ClassLoaderExt::set_has_ext_classes();
    } else if (SystemDictionary::is_cus_class_loader(loader)) {
      classloader_type = ClassLoader::CUS_LOADER;
      ClassLoaderExt::set_has_cus_classes();
    }

    if (classloader_type != ClassLoader::CUS_LOADER) {
      result->set_shared_classpath_index(classpath_index);
    }
    result->set_class_loader_type(classloader_type);


    if (classloader_type == ClassLoader::APP_LOADER ||
        classloader_type == ClassLoader::EXT_LOADER) {
      if (SharedClassListFile == NULL) {
        assert(e != NULL, "No class path entry");
        assert(strcmp(e->name(),
                      FileMapInfo::shared_classpath_name(classpath_index)) == 0,
               "Incorrect shared class path entry");
        // Class loaded by the App or Ext or Cus loader is not in a system package.
        // Don't add to to package table, or else we would crash at run-time inside
        // ClassLoader::get_system_package
      }
      return result;
    }
    if (classloader_type == ClassLoader::CUS_LOADER) {
      return result;
    }
    if (classpath_index >= _app_paths_start_index) {
      return result;
    }
  }
  // Delete the following assert for CDS
  //    [1] When dumping, it will ignore those files which were added by `append_boot_classpath`  (e.g. by agent)
  //    [2] When replaying, the number of boot classpaths may increase if it calls `append_boot_classpath`. However,
  //        the _app_paths_start_index was initialized with the dumping statistics, and thus, it will fail in this assert.
  //        CDS will find these classes which are not in shared dictionary (not in .jsa) through noraml path,
  //        and it should ignore this check in light of the increase of boot classpaths.
  // assert(classpath_index < _app_paths_start_index && classloader_type == BOOT_LOADER, "Sanity check");

  // add to package table
  if (add_package(context->file_name(), classpath_index, THREAD)) {
    return result;
  } else {
    return NULL; // - failed
  }
}

void ClassLoaderExt::finalize_shared_paths_misc_info() {
  if (UseAppCDS) {
    if (!_has_app_classes) {
      shared_paths_misc_info()->pop_app();
    }
  }
}

static ClassFileStream* check_class_file_load_hook(ClassFileStream* stream,
                                                   Symbol* name,
                                                   ClassLoaderData* loader_data,
                                                   Handle protection_domain,
                                                   JvmtiCachedClassFileData** cached_class_file,
                                                   TRAPS) {

  assert(stream != NULL, "invariant");

  if (JvmtiExport::should_post_class_file_load_hook()) {
    assert(THREAD->is_Java_thread(), "must be a JavaThread");
    const JavaThread* jt = (JavaThread*)THREAD;

    Handle class_loader(THREAD, loader_data->class_loader());

    // Get the cached class file bytes (if any) from the class that
    // is being redefined or retransformed. We use jvmti_thread_state()
    // instead of JvmtiThreadState::state_for(jt) so we don't allocate
    // a JvmtiThreadState any earlier than necessary. This will help
    // avoid the bug described by 7126851.

    JvmtiThreadState* state = jt->jvmti_thread_state();

    if (state != NULL) {
      KlassHandle* kh = state->get_class_being_redefined();

      if (kh->not_null()) {
        InstanceKlass* class_being_redefined = InstanceKlass::cast((*kh)());
        *cached_class_file = class_being_redefined->get_cached_class_file();
      }
    }

    unsigned char* ptr = const_cast<unsigned char*>(stream->buffer());
    unsigned char* end_ptr = ptr + stream->length();

    JvmtiExport::post_class_file_load_hook(name,
                                           class_loader,
                                           protection_domain,
                                           &ptr,
                                           &end_ptr,
                                           cached_class_file);

    if (ptr != stream->buffer()) {
      // JVMTI agent has modified class file data.
      // Set new class file stream using JVMTI agent modified class file data.
      stream = new ClassFileStream(ptr,
                                   end_ptr - ptr,
                                   stream->source());
    }
  }

  return stream;
}

InstanceKlass* ClassLoaderExt::create_from_stream(ClassFileStream* stream,
                                                Symbol* name,
                                                ClassLoaderData* loader_data,
                                                Handle protection_domain,
                                                const InstanceKlass* host_klass,
                                                GrowableArray<Handle>* cp_patches,
                                                TRAPS) {
  assert(DumpSharedSpaces, "this function is only used with -Xshare:dump");
  assert(stream != NULL, "invariant");
  assert(loader_data != NULL, "invariant");
  assert(THREAD->is_Java_thread(), "must be a JavaThread");

  ResourceMark rm;
  HandleMark hm;

  JvmtiCachedClassFileData* cached_class_file = NULL;

  ClassFileStream* old_stream = stream;

  stringStream st;
  // st.print() uses too much stack space while handling a StackOverflowError
  // st.print("%s.class", h_name->as_utf8());
  st.print_raw(name->as_utf8());
  st.print_raw(".class");
  const char* file_name = st.as_string();
  ClassLoaderExt::Context context(name->as_C_string(), file_name, THREAD);
  ClassFileParser parser(stream);
  TempNewSymbol parsed_name = NULL;

  // Skip this processing for VM anonymous classes
  if (host_klass == NULL) {
    stream = check_class_file_load_hook(stream,
                                        name,
                                        loader_data,
                                        protection_domain,
                                        &cached_class_file,
                                        CHECK_NULL);
  }


  instanceKlassHandle kh =parser.parseClassFile(name,
                                                loader_data,
                                                protection_domain,
                                                host_klass,
                                                cp_patches,
                                                parsed_name,
                                                false, /*should_verify*/
                                                THREAD);

  if (kh.is_null()) {
    return NULL;
  }

  if (cached_class_file != NULL) {
    // JVMTI: we have an InstanceKlass now, tell it about the cached bytes
    kh()->set_cached_class_file(cached_class_file);
  }

#if INCLUDE_CDS
  if (DumpSharedSpaces) {
    ClassLoader::record_shared_class_loader_type(kh(), stream);
#if INCLUDE_JVMTI
    assert(cached_class_file == NULL, "Sanity");
    // Archive the class stream data into the optional data section
    JvmtiCachedClassFileData *p;
    int len;
    const unsigned char *bytes;
    // event based tracing might set cached_class_file
    if ((bytes = kh->get_cached_class_file_bytes()) != NULL) {
      len = kh->get_cached_class_file_len();
    } else {
      len = stream->length();
      bytes = stream->buffer();
    }
    p = (JvmtiCachedClassFileData*)os::malloc(offset_of(JvmtiCachedClassFileData, data) + len, mtInternal);
    p->length = len;
    memcpy(p->data, bytes, len);
    kh()->set_archived_class_data(p);
#endif // INCLUDE_JVMTI
  }
#endif // INCLUDE_CDS

  return kh();
}

// Load the class of the given name from the location given by path. The path is specified by
// the "source:" in the class list file (see classListParser.cpp), and can be a directory or
// a JAR file.
InstanceKlass* ClassLoaderExt::load_class(Symbol* name, const char* path, int defining_loader_hash,
                                          int initiating_loader_hash, uint64 fingerprint, TRAPS) {

  assert(name != NULL, "invariant");
  assert(DumpSharedSpaces, "this function is only used with -Xshare:dump");
  ResourceMark rm(THREAD);
  const char* class_name = name->as_C_string();

  const char* file_name = file_name_for_class_name(class_name,
                                                   name->utf8_length());
  assert(file_name != NULL, "invariant");

  ClassLoaderExt::Context context(class_name, file_name, THREAD);
  // Lookup stream for parsing .class file
  ClassFileStream* stream = NULL;
  ClassPathEntry* e = find_classpath_entry_from_cache(path, CHECK_NULL);
  if (e == NULL) {
    return NULL;
  }
  {
    PerfClassTraceTime vmtimer(perf_sys_class_lookup_time(),
                               ((JavaThread*) THREAD)->get_thread_stat()->perf_timers_addr(),
                               PerfClassTraceTime::CLASS_LOAD);
    stream = e->open_stream(file_name, CHECK_NULL);
  }

  if (NULL == stream) {
    tty->print_cr("Preload Warning: Cannot find %s", class_name);
    return NULL;
  }

  if (EagerAppCDS && fingerprint != 0 && fingerprint != stream->compute_fingerprint()) {
      tty->print_cr("Preload Warning: class %s with different fingerprint " PTR64_FORMAT " and " PTR64_FORMAT, class_name, fingerprint, stream->compute_fingerprint());
      return NULL;
  }

  assert(stream != NULL, "invariant");
  stream->set_verify(true);

  ClassLoaderData* loader_data = ClassLoaderData::the_null_class_loader_data();
  Handle protection_domain;

  InstanceKlass* result = create_from_stream(stream,
                                             name,
                                             loader_data,
                                             protection_domain,
                                             NULL, // host_klass
                                             NULL, // cp_patches
                                             THREAD);

  if (HAS_PENDING_EXCEPTION) {
    tty->print_cr("Preload Error: Failed to load %s", class_name);
    return NULL;
  }
  result->set_shared_classpath_index(UNREGISTERED_INDEX);
  SystemDictionaryShared::set_shared_class_misc_info(result, stream, defining_loader_hash, initiating_loader_hash);
  return result;
}

struct CachedClassPathEntry {
  const char* _path;
  ClassPathEntry* _entry;
};

static GrowableArray<CachedClassPathEntry>* cached_path_entries = NULL;

ClassPathEntry* ClassLoaderExt::find_classpath_entry_from_cache(const char* path, TRAPS) {
  // This is called from dump time so it's single threaded and there's no need for a lock.
  assert(DumpSharedSpaces, "this function is only used with -Xshare:dump");
  if (cached_path_entries == NULL) {
    cached_path_entries = new (ResourceObj::C_HEAP, mtClass) GrowableArray<CachedClassPathEntry>(20, /*c heap*/ true);
  }
  CachedClassPathEntry ccpe;
  for (int i = 0; i < cached_path_entries->length(); i++) {
    ccpe = cached_path_entries->at(i);
    if (strcmp(ccpe._path, path) == 0) {
      if (i != 0) {
        // Put recent entries at the beginning to speed up searches.
        cached_path_entries->remove_at(i);
        cached_path_entries->insert_before(0, ccpe);
      }
      return ccpe._entry;
    }
  }

  struct stat st;
  if (strncmp(path, "file:", 5) == 0) path += 5;
  if (os::stat(path, &st) != 0) {
    // File or directory not found
    return NULL;
  }
  ClassPathEntry* new_entry = NULL;

  new_entry = create_class_path_entry(path, &st, false, false, CHECK_NULL);
  if (new_entry == NULL) {
    return NULL;
  }
  ccpe._path = strdup(path);
  ccpe._entry = new_entry;
  cached_path_entries->insert_before(0, ccpe);
  return new_entry;
}

Klass* ClassLoaderExt::load_one_class(ClassListParser* parser, TRAPS) {
  return parser->load_current_class(THREAD);
}
