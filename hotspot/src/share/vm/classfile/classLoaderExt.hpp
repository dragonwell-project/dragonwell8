/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_CLASSFILE_CLASSLOADEREXT_HPP
#define SHARE_VM_CLASSFILE_CLASSLOADEREXT_HPP

#include "classfile/classLoader.hpp"
#include "utilities/macros.hpp"

CDS_ONLY(class SharedPathsMiscInfoExt;)
CDS_ONLY(class ClassListParser;)
class ClassLoaderExt: public ClassLoader { // AllStatic
public:
  enum SomeConstants {
    max_classpath_index = 0x7fff
  };
  // ClassLoaderExt::Context --
  //
  // This is used by DumpSharedSpaces only - it enforces the same classloader
  // delegation model as would be in run-time. I.e.,
  // + classes defined by the NULL class loader cannot load classes in the EXT or APP paths.
  // + classes defined by the EXT  class loader cannot load classes in the APP paths.
  class Context {
    static Thread* _dump_thread;
    const char* _class_name;
    const char* _file_name;
  public:

    const char* class_name() {
      return _class_name;
    }
    const char* file_name() {
      return _file_name;
    }

    Context(const char* class_name, const char* file_name, TRAPS) {
      _class_name = class_name;
      _file_name = file_name;
#if INCLUDE_CDS
      if (!DumpSharedSpaces && !UseSharedSpaces) {
        // Must not modify _app_paths_start_index if we're not using CDS.
        assert(_app_paths_start_index == ClassLoaderExt::max_classpath_index, "must be");
      }
#endif
    }

    bool check(ClassFileStream* stream, const int classpath_index) {
      CDS_ONLY(return ClassLoaderExt::check(this, stream, classpath_index);)
      NOT_CDS(return true;)
    }

    bool should_verify(int classpath_index) {
      CDS_ONLY(return (classpath_index >= _app_paths_start_index);)
      NOT_CDS(return false;)
    }

    InstanceKlass* record_result(Symbol* class_name,
                                 ClassPathEntry* e,
                                 const s2 classpath_index,
                                 InstanceKlass* result,
                                 TRAPS) {
#if INCLUDE_CDS
      return ClassLoaderExt::record_result(this, class_name, e, classpath_index,
                                           result, THREAD);
#else
      if (ClassLoader::add_package(_file_name, classpath_index, THREAD)) {
        return result;
      } else {
        return NULL;
      }
#endif  // INCLUDE_CDS
    }

    ~Context() {
#if INCLUDE_CDS
      if (!DumpSharedSpaces && !UseSharedSpaces) {
        // Must not modify app_paths_start_index if we're not using CDS.
        assert(_app_paths_start_index == ClassLoaderExt::max_classpath_index, "must be");
      }
#endif
    }
  }; // end ClassLoaderExt::Context

 private:
#if INCLUDE_CDS
  static char* get_class_path_attr(const char* jar_path, char* manifest, jint manifest_size);
  static void setup_app_search_path(); // Only when -Xshare:dump
  static SharedPathsMiscInfoExt* shared_paths_misc_info() {
    return (SharedPathsMiscInfoExt*)_shared_paths_misc_info;
  }

  static jshort _app_paths_start_index; // index of first app JAR in shared classpath entry table

  static bool _has_app_classes;
  static bool _has_ext_classes;
  static bool _has_cus_classes;
#endif

public:
  static void add_class_path_entry(const char* path, bool check_for_duplicates,
                                   ClassPathEntry* new_entry) CDS_ONLY(;)
  NOT_CDS( { ClassLoader::add_to_list(new_entry); })

  CDS_ONLY(static void process_jar_manifest(ClassPathEntry* entry, bool check_for_duplicates);)

  static bool should_verify(int classpath_index) {
    CDS_ONLY(return (classpath_index >= _app_paths_start_index);)
    NOT_CDS(return false;)
  }

  // Called by JVMTI code to add boot classpath
  static void append_boot_classpath(ClassPathEntry* new_entry) {
#if INCLUDE_CDS
    if (UseAppCDS) {
      if (CDSIgnoreBootClasspathDiff) {
        warning("AppCDS is still enabled, but bootstrap classpath has been appended");
      } else {
        warning("UseAppCDS is disabled because bootstrap classpath has been appended");
        UseAppCDS = false;
      }
    }
#endif
    ClassLoader::add_to_list(new_entry);
  }

  static void setup_search_paths() NOT_CDS_RETURN;

#if INCLUDE_CDS
 private:
  static char* read_manifest(ClassPathEntry* entry, jint *manifest_size, bool clean_text,  TRAPS);
  static ClassPathEntry* find_classpath_entry_from_cache(const char* path, TRAPS);
  static InstanceKlass* create_from_stream(ClassFileStream* stream,
                                           Symbol* name,
                                           ClassLoaderData* loader_data,
                                           Handle protection_domain,
                                           const InstanceKlass* host_klass,
                                           GrowableArray<Handle>* cp_patches,
                                           TRAPS);
public:
  static char* read_manifest(ClassPathEntry* entry, jint *manifest_size, TRAPS) {
    // Remove all the new-line continuations (which wrap long lines at 72 characters, see
    // http://docs.oracle.com/javase/6/docs/technotes/guides/jar/jar.html#JAR%20Manifest), so
    // that the manifest is easier to parse.
    return read_manifest(entry, manifest_size, true, THREAD);
  }
  static char* read_raw_manifest(ClassPathEntry* entry, jint *manifest_size, TRAPS) {
    // Do not remove new-line continuations, so we can easily pass it as an argument to
    // java.util.jar.Manifest.getManifest() at run-time.
    return read_manifest(entry, manifest_size, false, THREAD);
  }

  static void finalize_shared_paths_misc_info();

  static jshort app_paths_start_index() { return _app_paths_start_index; }

  static void init_paths_start_index(jshort app_start) {
    _app_paths_start_index = app_start;
  }

  static bool has_ext_or_app_or_cus_classes() {
    return _has_app_classes || _has_ext_classes || _has_cus_classes;
  }

  static bool check(class ClassLoaderExt::Context *context,
                    const ClassFileStream* stream,
                    const int classpath_index);

  static InstanceKlass* record_result(ClassLoaderExt::Context *context,
                                      Symbol* class_name,
                                      ClassPathEntry* e,
                                      const s2 classpath_index,
                                      InstanceKlass* result, TRAPS);
  static InstanceKlass* load_class(Symbol* h_name, const char* path,
                                   int defining_loader_hash, int initiating_loader_hash,
                                   uint64 fingerprint, TRAPS);
  static Klass* load_one_class(ClassListParser* parser, TRAPS);

  static void set_has_app_classes() {
    _has_app_classes = true;
  }
  static void set_has_ext_classes() {
    _has_ext_classes = true;
  }
  static void set_has_cus_classes() {
    _has_cus_classes = true;
  }
#endif

  static bool known_to_not_exist(JNIEnv *env, jobject loader, const char *classname, TRAPS) {return false;}
  static jobjectArray get_lookup_cache_urls(JNIEnv *env, jobject loader, TRAPS) {return NULL;}
  static jintArray get_lookup_cache(JNIEnv *env, jobject loader, const char *pkgname, TRAPS) {return NULL;}
};

#endif // SHARE_VM_CLASSFILE_CLASSLOADEREXT_HPP
