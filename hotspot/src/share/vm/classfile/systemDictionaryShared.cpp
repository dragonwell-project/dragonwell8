/*
 * Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/classListParser.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/classLoaderExt.hpp"
#include "classfile/dictionary.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/sharedClassUtil.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/systemDictionaryShared.hpp"
#include "classfile/verificationType.hpp"
#include "classfile/vmSymbols.hpp"
#include "memory/allocation.hpp"
#include "memory/filemap.hpp"
#include "memory/metadataFactory.hpp"
#include "memory/oopFactory.hpp"
#include "memory/resourceArea.hpp"
#include "oops/klass.inline.hpp"
#include "oops/instanceKlass.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "utilities/hashtable.inline.hpp"
#include "utilities/stringUtils.hpp"
#include "services/threadService.hpp"

oop         SystemDictionaryShared::_java_ext_loader            =  NULL;
objArrayOop SystemDictionaryShared::_shared_protection_domains  =  NULL;
objArrayOop SystemDictionaryShared::_shared_jar_urls            =  NULL;
objArrayOop SystemDictionaryShared::_shared_jar_manifests       =  NULL;

static Mutex* SharedDictionary_lock = NULL;

void SystemDictionaryShared::initialize(TRAPS) {
  if (_java_system_loader != NULL) {
    SharedDictionary_lock = new Mutex(Mutex::leaf, "SharedDictionary_lock", true);
    fieldDescriptor fd;
    InstanceKlass *ik = InstanceKlass::cast(_java_system_loader->klass());
    if (ik->find_field(vmSymbols::parent_name(),
                      vmSymbols::classloader_signature(), &fd)) {
      _java_ext_loader = _java_system_loader->obj_field(fd.offset());
    } else {
      // User may have specified -Djava.system.class.loader=<loader> to
      // use a different system loader.
    }
    // These classes need to be initialized before calling get_shared_jar_manifest(), etc.
    SystemDictionary::ByteArrayInputStream_klass()->initialize(CHECK);
    SystemDictionary::File_klass()->initialize(CHECK);
    SystemDictionary::Jar_Manifest_klass()->initialize(CHECK);
    SystemDictionary::CodeSource_klass()->initialize(CHECK);
  }
}

void SystemDictionaryShared::atomic_set_array_index(objArrayOop array, int index, oop o) {
  // Benign race condition:  array.obj_at(index) may already be filled in.
  // The important thing here is that all threads pick up the same result.
  // It doesn't matter which racing thread wins, as long as only one
  // result is used by all threads, and all future queries.
  SystemDictLocker mu(SystemDictionary_lock, Thread::current());
  if (array->obj_at(index) == NULL) {
    OrderAccess::storestore(); // ensure correctness of the lock-free path
    array->obj_at_put(index, o);
  } else {
    // Another thread succeeded before me. Use his value instead.
    // <o> is not saved and will be GC'ed.
  }
}

Handle SystemDictionaryShared::get_shared_jar_manifest(int shared_path_index, TRAPS) {
  Handle empty;
  Handle manifest ;
  if (shared_jar_manifest(shared_path_index) == NULL) {
    SharedClassPathEntryExt* ent = (SharedClassPathEntryExt*)FileMapInfo::shared_classpath(shared_path_index);
    long size = ent->manifest_size();
    if (size <= 0) {
      return empty; // No manifest - return NULL handle
    }

    // ByteArrayInputStream bais = new ByteArrayInputStream(buf);
    Klass* k = SystemDictionary::ByteArrayInputStream_klass();
    InstanceKlass* bais_klass = InstanceKlass::cast(k);
    Handle bais = bais_klass->allocate_instance_handle(CHECK_(empty));
    {
      const char* src = (const char*)ent->manifest();
      assert(src != NULL, "No Manifest data");
      typeArrayOop buf = oopFactory::new_byteArray(size, CHECK_(empty));
      typeArrayHandle bufhandle(THREAD, buf);
      char* dst = (char*)(buf->byte_at_addr(0));
      memcpy(dst, src, (size_t)size);

      JavaValue result(T_VOID);
      JavaCalls::call_special(&result, bais, bais_klass,
                              vmSymbols::object_initializer_name(),
                              vmSymbols::byte_array_void_signature(),
                              bufhandle, CHECK_(empty));
    }

    // manifest = new Manifest(bais)
    Klass* m_k = SystemDictionary::Jar_Manifest_klass();
    InstanceKlass* manifest_klass = InstanceKlass::cast(m_k);
    manifest = manifest_klass->allocate_instance_handle(CHECK_(empty));
    {
      JavaValue result(T_VOID);
      JavaCalls::call_special(&result, manifest, manifest_klass,
                              vmSymbols::object_initializer_name(),
                              vmSymbols::input_stream_void_signature(),
                              bais, CHECK_(empty));
    }
    atomic_set_shared_jar_manifest(shared_path_index, manifest());
  }

  manifest = Handle(THREAD, shared_jar_manifest(shared_path_index));
  assert(manifest.not_null(), "sanity");
  return manifest;
}

Handle SystemDictionaryShared::get_shared_jar_url(int shared_path_index, TRAPS) {
  Handle url_h;
  if (shared_jar_url(shared_path_index) == NULL) {
    // File file = new File(path);
    InstanceKlass* file_klass = InstanceKlass::cast(SystemDictionary::File_klass());
    Handle file = file_klass->allocate_instance_handle(CHECK_(url_h));
    {
      const char* path = FileMapInfo::shared_classpath_name(shared_path_index);
      Handle path_string = java_lang_String::create_from_str(path, CHECK_(url_h));
      JavaValue result(T_VOID);
      JavaCalls::call_special(&result, file, KlassHandle(THREAD, file_klass),
                              vmSymbols::object_initializer_name(),
                              vmSymbols::string_void_signature(),
                              path_string, CHECK_(url_h));
    }

    // result = Launcher.getURL(file)
    KlassHandle launcher_klass(THREAD,
                               SystemDictionary::sun_misc_Launcher_klass());
    JavaValue result(T_OBJECT);
    JavaCalls::call_static(&result, launcher_klass,
                           vmSymbols::getFileURL_name(),
                           vmSymbols::getFileURL_signature(),
                           file, CHECK_(url_h));
    atomic_set_shared_jar_url(shared_path_index, (oop)result.get_jobject());
  }

  url_h = Handle(THREAD, shared_jar_url(shared_path_index));
  assert(url_h.not_null(), "sanity");
  return url_h;
}

// Define Package for shared app classes from JAR file and also checks for
// package sealing (all done in Java code)
// See http://docs.oracle.com/javase/tutorial/deployment/jar/sealman.html
void SystemDictionaryShared::define_shared_package(Symbol*  class_name,
                                                   Handle class_loader,
                                                   Handle manifest,
                                                   Handle url,
                                                   TRAPS) {
  ResourceMark rm(THREAD);
  stringStream st;
  st.print_raw(class_name->as_utf8());
  char* name = st.as_string();
  const char *p = strrchr(name, '/');
  if (p != NULL) { // Package prefix found
    // No locking is needed here. It's handled by the java code,
    // see ClassLoader.definePackage().
    int n = p - name;
    char* pkgname = NEW_RESOURCE_ARRAY_IN_THREAD(THREAD, char, n + 1);
    memcpy(pkgname, name, n);
    for (int i = 0; i < n-1; i++) {
      if (pkgname[i] == '/') {
        pkgname[i] = '.';
      }
    }
    pkgname[n] = '\0';
    Handle pkgname_string = java_lang_String::create_from_str(pkgname,
                                                              CHECK);
    KlassHandle url_classLoader_klass(
        THREAD, SystemDictionary::URLClassLoader_klass());
    JavaValue result(T_VOID);
    JavaCallArguments args(3);
    args.set_receiver(class_loader);
    args.push_oop(pkgname_string);
    args.push_oop(manifest);
    args.push_oop(url);
    JavaCalls::call_virtual(&result, url_classLoader_klass,
                            vmSymbols::definePackageInternal_name(),
                            vmSymbols::definePackageInternal_signature(),
                            &args,
                            CHECK);
  }
}


// Get the ProtectionDomain associated with the CodeSource from the classloader.
Handle SystemDictionaryShared::get_protection_domain_from_classloader(Handle class_loader,
                                                                      Handle url, TRAPS) {
  // CodeSource cs = new CodeSource(url, null);
  InstanceKlass* cs_klass = InstanceKlass::cast(SystemDictionary::CodeSource_klass());
  Handle cs = cs_klass->allocate_instance_handle(CHECK_NH);
  JavaValue void_result(T_VOID);
  JavaCalls::call_special(&void_result, cs, cs_klass,
                          vmSymbols::object_initializer_name(),
                          vmSymbols::url_code_signer_array_void_signature(),
                          url, Handle(), CHECK_NH);

  // protection_domain = SecureClassLoader.getProtectionDomain(cs);
  Klass* secureClassLoader_klass = SystemDictionary::SecureClassLoader_klass();
  JavaValue obj_result(T_OBJECT);
  JavaCalls::call_virtual(&obj_result, class_loader, secureClassLoader_klass,
                          vmSymbols::getProtectionDomain_name(),
                          vmSymbols::getProtectionDomain_signature(),
                          cs, CHECK_NH);
  return Handle(THREAD, (oop)obj_result.get_jobject());
}

// Returns the ProtectionDomain associated with the JAR file identified by the url.
Handle SystemDictionaryShared::get_shared_protection_domain(Handle class_loader,
                                                            int shared_path_index,
                                                            Handle url,
                                                            TRAPS) {

  Handle protection_domain;
  if (shared_protection_domain(shared_path_index) == NULL) {
    Handle pd = get_protection_domain_from_classloader(class_loader, url, THREAD);
    atomic_set_shared_protection_domain(shared_path_index, pd());
  }

  // Acquire from the cache because if another thread beats the current one to
  // set the shared protection_domain and the atomic_set fails, the current thread
  // needs to get the updated protection_domain from the cache.
  protection_domain = Handle(THREAD, shared_protection_domain(shared_path_index));
  assert(protection_domain.not_null(), "sanity");
  return protection_domain;
}

// Initializes the java.lang.Package and java.security.ProtectionDomain objects associated with
// the given InstanceKlass.
// Returns the ProtectionDomain for the InstanceKlass.
Handle SystemDictionaryShared::init_security_info(Handle class_loader, instanceKlassHandle ik, TRAPS) {
  Handle pd;

  if (ik.not_null()) {
    int index = ik->shared_classpath_index();
    assert(index >= 0, "Sanity");
    SharedClassPathEntryExt* ent =
            (SharedClassPathEntryExt*)FileMapInfo::shared_classpath(index);
    Symbol* class_name = ik->name();

    // For shared app/ext classes originated from JAR files on the class path:
    //   Each of the 3 SystemDictionaryShared::_shared_xxx arrays has the same length
    //   as the shared classpath table in the shared archive (see
    //   FileMap::_classpath_entry_table in filemap.hpp for details).
    //
    //   If a shared InstanceKlass k is loaded from the class path, let
    //
    //     index = k->shared_classpath_index():
    //
    //   FileMap::_classpath_entry_table[index] identifies the JAR file that contains k.
    //
    //   k's protection domain is:
    //
    //     ProtectionDomain pd = _shared_protection_domains[index];
    //
    //   and k's Package is initialized using
    //
    //     manifest = _shared_jar_manifests[index];
    //     url = _shared_jar_urls[index];
    //     define_shared_package(class_name, class_loader, manifest, url, CHECK_(pd));
    //
    //   Note that if an element of these 3 _shared_xxx arrays is NULL, it will be initialized by
    //   the corresponding SystemDictionaryShared::get_shared_xxx() function.
    Handle manifest = get_shared_jar_manifest(index, CHECK_(pd));
    Handle url = get_shared_jar_url(index, CHECK_(pd));
    define_shared_package(class_name, class_loader, manifest, url, CHECK_(pd));
    pd = get_shared_protection_domain(class_loader, index, url, CHECK_(pd));
  }
  return pd;
}

bool SystemDictionaryShared::is_builtin_loader(ClassLoaderData* loader_data) {
  oop class_loader = loader_data->class_loader();
  return (class_loader == NULL ||
          class_loader == _java_system_loader ||
          class_loader == _java_ext_loader);
}

/*
// DO NOT delete this comment, it is from upstream and works with Modules.
// Currently AppCDS only archives classes from the run-time image, the
// -Xbootclasspath/a path, and the class path. The following rules need to be
// revised when AppCDS is changed to archive classes from other code sources
// in the future, for example the module path (specified by -p).
//
// Check if a shared class can be loaded by the specific classloader. Following
// are the "visible" archived classes for different classloaders.
//
// NULL classloader:
//   - see SystemDictionary::is_shared_class_visible()
// Platform classloader:
//   - Module class from "modules" jimage. ModuleEntry must be defined in the
//     classloader.
// App Classloader:
//   - Module class from "modules" jimage. ModuleEntry must be defined in the
//     classloader.
//   - Class from -cp. The class must have no PackageEntry defined in any of the
//     boot/platform/app classloader, or must be in the unnamed module defined in the
//     AppClassLoader.
bool SystemDictionaryShared::is_shared_class_visible_for_classloader(
                                                     InstanceKlass* ik,
                                                     Handle class_loader,
                                                     const char* pkg_string,
                                                     Symbol* pkg_name,
                                                     PackageEntry* pkg_entry,
                                                     ModuleEntry* mod_entry,
                                                     TRAPS) {
  assert(class_loader.not_null(), "Class loader should not be NULL");
  assert(Universe::is_module_initialized(), "Module system is not initialized");

  int path_index = ik->shared_classpath_index();
  SharedClassPathEntry* ent =
            (SharedClassPathEntry*)FileMapInfo::shared_classpath(path_index);

  if (SystemDictionary::is_platform_class_loader(class_loader())) {
    assert(ent != NULL, "shared class for PlatformClassLoader should have valid SharedClassPathEntry");
    // The PlatformClassLoader can only load archived class originated from the
    // run-time image. The class' PackageEntry/ModuleEntry must be
    // defined by the PlatformClassLoader.
    if (mod_entry != NULL) {
      // PackageEntry/ModuleEntry is found in the classloader. Check if the
      // ModuleEntry's location agrees with the archived class' origination.
      if (ent->is_modules_image() && mod_entry->location()->starts_with("jrt:")) {
        return true; // Module class from the "modules" jimage
      }
    }
  } else if (SystemDictionary::is_system_class_loader(class_loader())) {
    assert(ent != NULL, "shared class for system loader should have valid SharedClassPathEntry");
    if (pkg_string == NULL) {
      // The archived class is in the unnamed package. Currently, the boot image
      // does not contain any class in the unnamed package.
      assert(!ent->is_modules_image(), "Class in the unnamed package must be from the classpath");
      if (path_index >= ClassLoaderExt::app_paths_start_index()) {
        return true;
      }
    } else {
      // Check if this is from a PackageEntry/ModuleEntry defined in the AppClassloader.
      if (pkg_entry == NULL) {
        // It's not guaranteed that the class is from the classpath if the
        // PackageEntry cannot be found from the AppClassloader. Need to check
        // the boot and platform classloader as well.
        if (get_package_entry(pkg_name, ClassLoaderData::class_loader_data_or_null(SystemDictionary::java_platform_loader())) == NULL &&
            get_package_entry(pkg_name, ClassLoaderData::the_null_class_loader_data()) == NULL) {
          // The PackageEntry is not defined in any of the boot/platform/app classloaders.
          // The archived class must from -cp path and not from the run-time image.
          if (!ent->is_modules_image() && path_index >= ClassLoaderExt::app_paths_start_index()) {
            return true;
          }
        }
      } else if (mod_entry != NULL) {
        // The package/module is defined in the AppClassLoader. Currently we only
        // support archiving application module class from the run-time image.
        // Packages from the -cp path are in the unnamed_module.
        if ((ent->is_modules_image() && mod_entry->location()->starts_with("jrt:")) ||
            (pkg_entry->in_unnamed_module() && path_index >= ClassLoaderExt::app_paths_start_index())) {
          DEBUG_ONLY( \
            ClassLoaderData* loader_data = class_loader_data(class_loader); \
            if (pkg_entry->in_unnamed_module()) \
              assert(mod_entry == loader_data->unnamed_module(), "the unnamed module is not defined in the classloader");)

          return true;
        }
      }
    }
  } else {
    // TEMP: if a shared class can be found by a custom loader, consider it visible now.
    // FIXME: is this actually correct?
    return true;
  }
  return false;
}*/

// The following stack shows how this code is reached:
//
//   [0] SystemDictionaryShared::find_or_load_shared_class()
//   [1] JVM_FindLoadedClass
//   [2] java.lang.ClassLoader.findLoadedClass0()
//   [3] java.lang.ClassLoader.findLoadedClass()
//   [4] java.lang.ClassLoader.loadClass()
//   [5] jdk.internal.loader.ClassLoaders$AppClassLoader_klass.loadClass()
//
// Because AppCDS supports only the PlatformClassLoader and AppClassLoader, we make the following
// assumptions (based on the JDK 8.0 source code):
//
// [a] these two loaders use the default implementation of
//     ClassLoader.loadClass(String name, boolean resolve), which
// [b] calls findLoadedClass(name), immediately followed by parent.loadClass(),
//     immediately followed by findClass(name).
// [c] If the requested class is a shared class of the current class loader, parent.loadClass()
//     always returns null, and
// [d] if AppCDS is not enabled, the class would be loaded by findClass() by decoding it from a
//     JAR file and then parsed.
//
// Given these assumptions, we intercept the findLoadedClass() call to invoke
// SystemDictionaryShared::find_or_load_shared_class() to load the shared class from
// the archive. The reasons are:
//
// + Because AppCDS is a commercial feature, we want to hide the implementation. There
//   is currently no easy way to hide Java code, so we did it with native code.
// + Start-up is improved because we avoid decoding the JAR file, and avoid delegating
//   to the parent (since we know the parent will not find this class).
//
// NOTE: there's a lot of assumption about the Java code. If any of that change, this
// needs to be redesigned.
//
// An alternative is to modify the Java code of AppClassLoader.loadClass().
//
instanceKlassHandle SystemDictionaryShared::find_or_load_shared_class(
                 Symbol* name, Handle class_loader, TRAPS) {
  if (DumpSharedSpaces) {
    return NULL;
  }

  if (shared_dictionary() != NULL &&
      UseAppCDS && (SystemDictionary::is_system_class_loader(class_loader()) ||
                    SystemDictionary::is_ext_class_loader(class_loader()))) {

    // Fix for 4474172; see evaluation for more details
    class_loader = Handle(
      THREAD, java_lang_ClassLoader::non_reflection_class_loader(class_loader()));
    ClassLoaderData *loader_data = register_loader(class_loader, CHECK_NULL);
    Dictionary* dictionary = SystemDictionary::dictionary();

    bool DoObjectLock = true;
    if (is_parallelCapable(class_loader)) {
      DoObjectLock = false;
    }

    // Make sure we are synchronized on the class loader before we proceed
    //
    // Note: currently, find_or_load_shared_class is called only from
    // JVM_FindLoadedClass and used for AppClassLoader,
    // which are parallel-capable loaders, so this lock is NOT taken.
    Handle lockObject = compute_loader_lock_object(class_loader, THREAD);
    check_loader_lock_contention(lockObject, THREAD);
    ObjectLocker ol(lockObject, THREAD, DoObjectLock);

    {
      SystemDictLocker mu(SystemDictionary_lock, THREAD);
      unsigned int hash = dictionary->compute_hash(name, loader_data);
      int index = dictionary->hash_to_index(hash);
      Klass* check = dictionary->find_class(index, hash, name, loader_data);
      if (check != NULL) {
        InstanceKlass* k = InstanceKlass::cast(check);
        return instanceKlassHandle(THREAD, k);
      }
    }
    // the name is somehow relics of the change. It includes custom loader too.
    instanceKlassHandle kh = load_shared_class_for_builtin_loader(name, class_loader, THREAD);
    if (kh.not_null()) {
      define_instance_class(kh, THREAD);
      if (HAS_PENDING_EXCEPTION) {
        if (TraceAppCDSLoading && UseSharedSpaces) {
          ResourceMark rm;
          tty->print_cr("[Fail 3] define_instance_class ex: %s %s %s",
                        name->as_utf8(), !class_loader.is_null() ? class_loader()->klass()->name()->as_C_string() : "NULL", PENDING_EXCEPTION->klass_or_null()->name()->as_utf8());
        }
        return NULL;
      }
    }
    return kh;
  } else if (EagerAppCDS && shared_dictionary() != NULL && java_lang_ClassLoader::signature(class_loader()) != 0 &&
             (strncmp(name->as_C_string(), "java/", 5) != 0)) {
    instanceKlassHandle kh = load_shared_class_for_registered_loader(name, class_loader, CHECK_NULL);
    return kh;
  }

  return instanceKlassHandle(THREAD, NULL);
}

instanceKlassHandle SystemDictionaryShared::load_shared_class_for_builtin_loader(
                 Symbol* class_name, Handle class_loader, TRAPS) {
  assert(UseAppCDS && shared_dictionary() != NULL, "already checked");
  Klass* k = shared_dictionary()->find_class_for_builtin_loader(class_name);

  if (k != NULL) {
    InstanceKlass* ik = InstanceKlass::cast(k);
    if (ik->shared_classpath_index() <= 0) {
      ResourceMark rm;
      tty->print_cr("FIXME shared_classpath_index is wrong: %s %d",
                    ik->external_name(), ik->shared_classpath_index());
      return NULL;
    }

    if ((ik->is_shared_app_class() &&
         SystemDictionary::is_system_class_loader(class_loader()))  ||
        (ik->is_shared_ext_class() &&
         SystemDictionary::is_ext_class_loader(class_loader()))) {
      Handle protection_domain =
        SystemDictionaryShared::init_security_info(class_loader, ik, CHECK_NULL);
      instanceKlassHandle ikh = load_shared_class(ik, class_loader, protection_domain, THREAD);
      if (TraceAppCDSLoading && ikh.is_null() && UseSharedSpaces) {
        ResourceMark rm;
        tty->print_cr("[Fail 2] load_shared_class() check fail: %s %s",
                      class_name->as_utf8(), !class_loader.is_null() ? class_loader()->klass()->name()->as_C_string() : "NULL");
      }
      return ikh;
    }
  }

  return NULL;
}

void SystemDictionaryShared::oops_do(OopClosure* f) {
  f->do_oop((oop*)&_shared_protection_domains);
  f->do_oop((oop*)&_shared_jar_urls);
  f->do_oop((oop*)&_shared_jar_manifests);
  f->do_oop(&_java_ext_loader);
}

void SystemDictionaryShared::allocate_shared_protection_domain_array(int size, TRAPS) {
  if (_shared_protection_domains == NULL) {
    _shared_protection_domains = oopFactory::new_objArray(
        SystemDictionary::ProtectionDomain_klass(), size, CHECK);
  }
}

void SystemDictionaryShared::allocate_shared_jar_url_array(int size, TRAPS) {
  if (_shared_jar_urls == NULL) {
    _shared_jar_urls = oopFactory::new_objArray(
        SystemDictionary::URL_klass(), size, CHECK);
  }
}

void SystemDictionaryShared::allocate_shared_jar_manifest_array(int size, TRAPS) {
  if (_shared_jar_manifests == NULL) {
    _shared_jar_manifests = oopFactory::new_objArray(
        SystemDictionary::Jar_Manifest_klass(), size, CHECK);
  }
}

void SystemDictionaryShared::allocate_shared_data_arrays(int size, TRAPS) {
  allocate_shared_protection_domain_array(size, CHECK);
  allocate_shared_jar_url_array(size, CHECK);
  allocate_shared_jar_manifest_array(size, CHECK);
}

bool SystemDictionaryShared::is_supported(InstanceKlass* k) {
  int min_version = 50; // FIXME - STACKMAP_ATTRIBUTE_MAJOR_VERSION;

  if (k->major_version() < min_version) {
    return false;
  } else {
    return true;
  }
}

bool SystemDictionaryShared::is_hierarchy_supported(InstanceKlass* k) {
  // all interfaces must be supported
  Array<Klass*>* interfaces = k->transitive_interfaces();
  int num_interfaces = interfaces->length();
  for (int i=0; i<num_interfaces; i++) {
    if (!is_supported(InstanceKlass::cast(interfaces->at(i)))) {
      return false;
    }
  }

  // all supers must be supported
  while (k != NULL) {
    if (!is_supported(InstanceKlass::cast(k))) {
      return false;
    }
    k = k->java_super();
  }
  return true;
}

// FIXME -- need to remove from this table when class is unloaded!

class Klass2IDTable : public Hashtable<InstanceKlass*, mtClass> {
public:
  Klass2IDTable() : Hashtable<InstanceKlass*, mtClass>(1987, sizeof(HashtableEntry<InstanceKlass*, mtClass>)) { }

  unsigned int get_hash(InstanceKlass* klass) {
    return (unsigned int)(p2i(klass) >> LogBytesPerWord);
  }

  void add(int id, InstanceKlass* klass) {
    unsigned int hash = get_hash(klass);
    HashtableEntry<InstanceKlass*, mtClass>* entry = new_entry((unsigned int)id, klass);
    add_entry(hash_to_index(hash), entry);
  }

  int lookup(InstanceKlass* klass) {
    unsigned int hash = get_hash(klass);
    int index = hash_to_index(hash);
    for (HashtableEntry<InstanceKlass*, mtClass>* e = bucket(index); e != NULL; e = e->next()) {
      if (e->literal() == klass) {
        return (int)(e->hash());
      }
    }
    return -1;
  }
};

void SystemDictionaryShared::log_not_founded_klass(const char* class_name, Handle class_loader, TRAPS) {
  assert(NotFoundClassOpt, "sanity check");
  if (DumpLoadedClassList == NULL || !classlist_file->is_open() || !NotFoundClassOpt) {
    return;
  }
  if (class_loader.is_null()) {
    return;
  }
  int signature = java_lang_ClassLoader::signature(class_loader());
  if (signature == 0) {
    return;
  }

  ResourceMark rm(THREAD);
  const char *name = class_name;
  if (invalid_class_name_for_EagerAppCDS(name)) {
    return;
  }

  if (invalid_class_name(name)) {
    return;
  }
  MutexLocker mu(DumpLoadedClassList_lock, THREAD);
  classlist_file->print("%s source: %s", class_name, NOT_FOUND_CLASS);
  classlist_file->print(" initiating_loader_hash: %x", signature);
  classlist_file->cr();
  classlist_file->flush();
}

void SystemDictionaryShared::log_loaded_klass(InstanceKlass* ik,
                                              const ClassFileStream* cfs,
                                              ClassLoaderData* loader_data, TRAPS) {
  if (ik == NULL) {
    return;
  }

  if (DumpLoadedClassList == NULL || !classlist_file->is_open() || cfs->source() == NULL) {
    return;
  }

  if (SystemDictionary::should_not_dump_class(ik)) {
    return;
  }

  if(!UseAppCDS && loader_data->is_system_class_loader_data()) {
    return;
  }

  ResourceMark rm(THREAD);
  const char *name = ik->name()->as_C_string();
  bool is_builtin = loader_data->is_builtin_class_loader_data();

  MutexLocker mu(DumpLoadedClassList_lock, THREAD);
  if (is_builtin) {
    if (!DumpAppCDSWithKlassId) {
      classlist_file->print("%s", name);
    } else {
      classlist_file->print("%s klass: " INTPTR_FORMAT, name, p2i(ik));
    }
    if (EagerAppCDS) {
      classlist_file->print(" super: " INTPTR_FORMAT, p2i(ik->superklass()));
      classlist_file->print(" origin: %s", cfs->source());
      classlist_file->print(" fingerprint: " PTR64_FORMAT, cfs->compute_fingerprint());
      Array<Klass*>* intf = ik->local_interfaces();
      if (intf->length() > 0) {
        classlist_file->print(" interfaces:");
        int length = intf->length();
        for (int i=0; i < length; i++) {
          classlist_file->print(" " INTPTR_FORMAT,
                                p2i(InstanceKlass::cast(intf->at(i))));
        }
      }
    }
  } else {
    // TestSimple source: file:/tmp/classes/com/alibaba/cds/TestDumpAndLoadClass.d/ klass: 0x0000000800066840
    // super: 0x0000000800001000 defining_loader_hash: fa474cbf fingerprint: 0x00000199e3c89ea7
    // If the signature is 0, still dump the class loading information for AppCDS usage
    classlist_file->print("%s origin: %s source: %s klass: " INTPTR_FORMAT, name, cfs->source(), cfs->source(), p2i(ik));
    classlist_file->print(" super: " INTPTR_FORMAT, p2i(ik->superklass()));

    Array<Klass*>* intf = ik->local_interfaces();
    if (intf->length() > 0) {
      classlist_file->print(" interfaces:");
      int length = intf->length();
      for (int i=0; i < length; i++) {
        classlist_file->print(" " INTPTR_FORMAT,
                              p2i(InstanceKlass::cast(intf->at(i))));
      }
    }

    if (EagerAppCDS) {
      int hash = java_lang_ClassLoader::signature(loader_data->class_loader());
      if (hash != 0) {
        classlist_file->print(" defining_loader_hash: %x", hash);
      }
      classlist_file->print(" origin: %s", cfs->source());
      classlist_file->print(" fingerprint: " PTR64_FORMAT, cfs->compute_fingerprint());
    }
  }
  classlist_file->cr();
  classlist_file->flush();
}

InstanceKlass* SystemDictionaryShared::lookup_from_stream(const Symbol* class_name,
                                                          Handle class_loader,
                                                          Handle protection_domain,
                                                          const ClassFileStream* cfs,
                                                          TRAPS) {
  if (!UseAppCDS || shared_dictionary() == NULL) {
    return NULL;
  }
  if (class_name == NULL) {  // don't do this for anonymous classes
    return NULL;
  }
  if (class_loader.is_null() ||
      SystemDictionary::is_system_class_loader(class_loader()) ||
      SystemDictionary::is_ext_class_loader(class_loader())) {
    // This function is called for loading only UNREGISTERED classes.
    // Do nothing for the BUILTIN loaders.
    return NULL;
  }

  ClassLoaderData* loader_data = ClassLoaderData::class_loader_data(class_loader());
  Klass* k;

  if (!shared_dictionary()->class_exists_for_unregistered_loader(class_name)) {
    // No classes of this name for unregistered loaders.
    return NULL;
  }

  int clsfile_size  = cfs->length();
  int clsfile_crc32 = ClassLoader::crc32(0, (const char*)cfs->buffer(), cfs->length());

  for (SharedDictionaryEntry* entry = shared_dictionary()->get_entry_for_unregistered_loader(class_name);
       entry != NULL; entry = entry->next()) {
    if (entry->is_unregistered() &&
        entry->_clsfile_size == clsfile_size &&
        entry->_clsfile_crc32 == clsfile_crc32) {
      k = entry->literal();
      InstanceKlass* loaded = acquire_class_for_current_thread(InstanceKlass::cast(k), class_loader,
                                                               protection_domain, CHECK_NULL);
      if (loaded != NULL) {
        return loaded;
      }
    }
  }
  return NULL;
}

bool SystemDictionaryShared::check_not_found_class(Symbol *class_name, int hash_value) {
  NotFoundClassTable* table = not_found_class_table();
  if (table == NULL) {
    return false;
  }
  unsigned int hash = table->compute_hash(class_name);
  int index = table->index_for(class_name);
  if (table->find_entry(index, hash, class_name, hash_value) != NULL) {
    return true;
  }
  return false;
}

void SystemDictionaryShared::record_not_found_class(Symbol* class_name, int hash_value) {
  assert(DumpSharedSpaces, "must be in dump phase");
  assert(NotFoundClassOpt, "must be with the opt for not found class");

  if (not_found_class_table() == NULL) {
    create_not_found_class_table();
  }

  NotFoundClassTable* table = not_found_class_table();
  unsigned int hash = table->compute_hash(class_name);
  int index = table->index_for(class_name);
  if (table->find_entry(index, hash, class_name, hash_value) == NULL) {
    table->add_entry(index, hash, class_name, hash_value);
  }
}

InstanceKlass* SystemDictionaryShared::load_class_from_cds(const Symbol* class_name,
                                                           Handle class_loader, InstanceKlass *ik, int defining_loader_hash, TRAPS) {

  PerfClassTraceTime vmtimer(ClassLoader::perf_cds_cust_classload_call_java_time(),
                             ClassLoader::perf_cds_cust_classload_call_java_selftime(),
                             ClassLoader::perf_cds_cust_classload_call_java_count(),
                             ((JavaThread*)THREAD)->get_thread_stat()->perf_recursion_counts_addr(),
                             ((JavaThread*)THREAD)->get_thread_stat()->perf_timers_addr(),
                             PerfClassTraceTime::CDS_CUST_LOAD_CALL_JAVA);

  JavaValue result(T_OBJECT);

  JavaCallArguments args(4);
  args.set_receiver(class_loader);
  // arg 1 className
  Handle s = java_lang_String::create_from_symbol(const_cast<Symbol*>(class_name), CHECK_NULL);
  // Translate to external class name format, i.e., convert '/' chars to '.'
  Handle string = java_lang_String::externalize_classname(s, CHECK_NULL);
  args.push_oop(string);
  // arg 2 sourcePath
  Symbol *path = ik->source_file_path();
  Handle url = java_lang_String::create_from_symbol(path, CHECK_NULL);
  args.push_oop(url);
  // arg 3 ik
  jlong ik_ptr = (jlong)ik;
  args.push_long(ik_ptr);
  // arg 4 definingLoaderHash
  args.push_int(defining_loader_hash);

  InstanceKlass* spec_klass = InstanceKlass::cast(SystemDictionary::ClassLoader_klass());

  JavaCalls::call_virtual(&result,
                          spec_klass,
                          vmSymbols::loadClassFromCDS_name(),
                          vmSymbols::loadClassFromCDS_signature(),
                          &args,
                          THREAD);

  if (HAS_PENDING_EXCEPTION) {
    // print exception
    if (PrintEagerAppCDSExceptions) {
      Handle ex(THREAD, PENDING_EXCEPTION);
      CLEAR_PENDING_EXCEPTION;
      ResourceMark rm;
      tty->print_cr("%s", ex()->klass()->name()->as_C_string());
      java_lang_Throwable::print_stack_trace(ex(), tty);
    } else {
      CLEAR_PENDING_EXCEPTION;
    }
    return NULL;
  }

  oop klass = (oop)result.get_jobject();
  if (klass != NULL) {
    return InstanceKlass::cast(java_lang_Class::as_Klass(klass));
  }
  return NULL;
}

InstanceKlass* SystemDictionaryShared::load_shared_class_for_registered_loader(
        Symbol* class_name, Handle class_loader, TRAPS) {
  ResourceMark rm(THREAD);
  char* name = class_name->as_C_string();
  // Only boot and platform class loaders can define classes in "java/" packages
  // in EagerAppCDS, ignore the classes in "java/" packages
  if ((strncmp(name, "java/", 5) == 0) || invalid_class_name_for_EagerAppCDS(name)) {
    return NULL;
  }

  bool not_found = false;
  // In findLoadedClass flow, disable the optimization for not found class
  return SystemDictionaryShared::lookup_shared(class_name, class_loader, not_found, false, THREAD);
}

/*
                                      +-------------------------------------+
                                      |SystemDictionaryShared::lookup_shared|
                                      +-------------------------------------+
                                        Find Entry in SharedDictionary
       +----------------------------+                |                        +---------------------+
       |   load_class_from_cds (vm) +----------------+------------------------+  loadClass (Java)   |
       |                            |     YES                 NO              |                     |
       +------------+---------------+                                         +---------------------+
                    |
       +------------v-------------+
       |  loadClassFromCDS (Java) |
       +------------+-------------+
                    |
       +------------v-------------+      NO     +-------------------------------------------+
       |Find defining class loader+-------------+ throw exception (fall back to normal path)|
       +--------------------------+             +-------------------------------------------+
                 YES|
       +------------v------------------------+
       | definingClassLoader.findClassFromCDS|
       +-------------------------------------+
                    |
       +------------v--------------------------+
       | definieClassLoader.defineClassFromCDS |
       +---------------------------------------+
                    |
       +------------v----------------+
       | JVM_DefineClassFromCDS (vm) |
       +------------+----------------+
                    |
       +------------v---------------------+
       | acquire_class_for_current_thread |
       +------------+---------------------+
                    |
       +------------v-----------+
       | define_instance_class  |
       +------------+-----------+
                    |
       +------------v--------------------------------+
       | load successfully, and return InstanceKlass |
       +---------------------------------------------+
*/
InstanceKlass* SystemDictionaryShared::lookup_shared(Symbol* class_name,
                                            Handle class_loader, bool &not_found,
                                            bool check_not_found,
                                            TRAPS) {
  assert(EagerAppCDS, "must be EagerAppCDS");

  bool loop = false;

  int loader_hash = java_lang_ClassLoader::signature(class_loader());
  for (SharedDictionaryEntry* entry = shared_dictionary()->get_entry_for_unregistered_loader(class_name);
    entry; entry = entry->next()) {
    Klass* k = entry->literal();

    loop = true;

    if (InstanceKlass::cast(k)->name() == class_name &&
        entry->_initiating_loader_hash == loader_hash) {
      InstanceKlass* ik = InstanceKlass::cast(k);

      InstanceKlass *loaded = load_class_from_cds(class_name, class_loader, ik, entry->_defining_loader_hash, THREAD);
      if (loaded != NULL) {
        if (TraceCDSLoading) {
          tty->print_cr("[CDS load class] Successful loading of class %s with class loader %s (%x)", class_name->as_C_string(),
                  class_loader()->klass()->name()->as_C_string(), loader_hash);
        }
        return loaded;
      }
    }
  }

  if (NotFoundClassOpt && check_not_found && !loop && check_not_found_class(class_name, loader_hash)) {
    not_found = true;
    if (TraceCDSLoading) {
      tty->print_cr("[CDS load class] Not found class %s with class loader %s (%x)", class_name->as_C_string(),
              class_loader()->klass()->name()->as_C_string(), loader_hash);
    }
    return NULL;
  }

  if (TraceCDSLoading) {
    ResourceMark rm;
    tty->print_cr("[CDS load class] Failed to load class %s with class loader %s (%x) since %s", class_name->as_C_string(),
            class_loader()->klass()->name()->as_C_string(), loader_hash,
            loop ? "failed to load from jsa" : " the class isn't in shared dictionary");
  }

  return NULL;
}



InstanceKlass* SystemDictionaryShared::define_class_from_cds(
                   InstanceKlass *ik,
                   Handle class_loader,
                   Handle protection_domain,
                   TRAPS) {

  ClassLoaderData* loader_data = register_loader(class_loader, THREAD);
  InstanceKlass* k = acquire_class_for_current_thread(ik, class_loader, protection_domain, THREAD);
  if (!k) {
    return NULL;
  }
  Symbol* h_name = k->name();
  // Add class just loaded
  // If a class loader supports parallel classloading handle parallel define requests
  // find_or_define_instance_class may return a different InstanceKlass
  if (is_parallelCapable(class_loader)) {
    InstanceKlass* defined_k = find_or_define_instance_class(h_name, class_loader, k, THREAD)();
    if (!HAS_PENDING_EXCEPTION && defined_k != k) {
      // If a parallel capable class loader already defined this class, register 'k' for cleanup.
      assert(defined_k != NULL, "Should have a klass if there's no exception");
      loader_data->add_to_deallocate_list(k);
      k = defined_k;
    }
  } else {
    define_instance_class(k, THREAD);
  }

  // If defining the class throws an exception register 'k' for cleanup.
  if (HAS_PENDING_EXCEPTION) {
    assert(k != NULL, "Must have an instance klass here!");
    loader_data->add_to_deallocate_list(k);
    return NULL;
  }
  return k;
}

InstanceKlass* SystemDictionaryShared::acquire_class_for_current_thread(
                   InstanceKlass *ik,
                   Handle class_loader,
                   Handle protection_domain,
                   TRAPS) {
  ClassLoaderData* loader_data = SystemDictionary::register_loader(class_loader, THREAD);

  {
    MutexLocker mu(SharedDictionary_lock, THREAD);
    if (ik->class_loader_data() != NULL) {
      //    ik is already loaded (by this loader or by a different loader)
      // or ik is being loaded by a different thread (by this loader or by a different loader)
      return NULL;
    }

    // No other thread has acquired this yet, so give it to *this thread*
    ik->set_class_loader_data(loader_data);
  }

  ResourceMark rm(THREAD);
#if 0
  GrowableArray<InstanceKlass*>* replaced_super_types = new GrowableArray<InstanceKlass*>(16);
  bool status = check_hierarchy(ik, class_loader, protection_domain, replaced_super_types, CHECK_NULL);
  if (!status) {
    // The current class hierarchy in this class_loader doesn't match what ik expects. Don't
    // use it. ik has not been modified, so it may be used by other loaders.
    MutexLocker mu(SharedDictionary_lock, THREAD);
    assert(ik->class_loader_data() == loader_data, "I should still own it");
    ik->set_class_loader_data(NULL);
    ik->set_shared_load_status(-1);
    return NULL;
  }
#endif

  // No longer holding SharedDictionary_lock
  // No need to lock, as <ik> can be held only by a single thread.
  loader_data->add_class(ik);

  // Load and check super/interfaces, restore unsharable info
  instanceKlassHandle shared_klass = load_shared_class(ik, class_loader, protection_domain, CHECK_NULL);
  assert(shared_klass() == NULL || shared_klass() == ik, "must be");

  if (shared_klass() == NULL) {
#if 0
    loader_data->remove_class(ik);

    // We found a class with matching name/size/crc32, but it doesn't match the expected class hierarchy.
    MutexLocker mu(SharedDictionary_lock, THREAD);
    assert(ik->class_loader_data() == loader_data, "I should still own it");
    if (replaced_super_types->length() > 0) {
      // ik has not been modified, so don't let anyone else use it.
      ik->set_class_loader_data((ClassLoaderData*)0xdeadbeef);
    } else {
      ik->set_class_loader_data(NULL);
#endif
      ik->set_shared_load_status(-1);
      return NULL;
#if 0
    }
#endif
  }

 return ik;
}

class SuperTypeReplacer : StackObj {
  InstanceKlass *_klass;
  GrowableArray<InstanceKlass*>* _replacements;

  InstanceKlass* find_replacement(InstanceKlass* k) {
    int len = _replacements->length();
    for (int i=0; i<len; i+=2) {
      if (_replacements->at(i) == k) {
        return _replacements->at(i+1);
      }
    }
    return NULL;
  }

  Method* find_replacement(Method* m, InstanceKlass* holder) {
    InstanceKlass* rep_holder = find_replacement(holder);
    if (rep_holder != NULL) {
      Symbol* name = m->name();
      Symbol* sig = m->signature();
      Array<Method*>* methods = rep_holder->methods();
      int len = methods->length();

      for (int i=0; i<len; i++) {
        Method* r = methods->at(i);
        if (r->name() == name && r->signature() == sig) {
          return r;
        }
      }
      assert(0, "replacement must be found!");
    }
    return NULL;
  }

 public:
  SuperTypeReplacer(InstanceKlass *klass, GrowableArray<InstanceKlass*>* replacements) :
    _klass(klass), _replacements(replacements)
  {}

  inline void push(InstanceKlass** klass_addr) {
    push((Klass**)klass_addr);
  }

  void push(Klass** klass_addr) {
    Klass* k = *klass_addr;
    if (k != NULL && k->oop_is_instance()) {
      InstanceKlass* ik = InstanceKlass::cast(k);
      InstanceKlass* rep = find_replacement(ik);
      if (rep) {
        tty->print_cr("             -> %p %s", rep, rep->internal_name());
        *klass_addr = rep;
      }
    }
  }

  void push(Method** method_addr) {
    Method* m = *method_addr;
    if (m != NULL) {
      InstanceKlass* holder = m->method_holder();
      if (holder != _klass) {
        Method* rep = find_replacement(m, holder);
        if (rep != NULL) {
          tty->print_cr("             -> %p", rep);
          *method_addr = rep;
        }
      }
    }
  }
};

template <class T>
  Array<T>* SystemDictionaryShared::copy(ClassLoaderData* loader_data, Array<T>* src, TRAPS) {
  if (src == NULL) {
    return src;
  }
  int len = src->length();
  if (len == 0) {
    return src;
  }
  Array<T>* dst = MetadataFactory::new_array<T>(loader_data, len, 0, CHECK_NULL);
  for (int i=0; i<len; i++) {
    dst->at_put(i, src->at(i));
  }
  return dst;
}

bool SystemDictionaryShared::check_hierarchy(
                  InstanceKlass *ik,
                   Handle class_loader,
                   Handle protection_domain,
                   GrowableArray<InstanceKlass*>* replaced_super_types,
                   TRAPS) {
  InstanceKlass* expected_super = ik->java_super();
  if (expected_super != NULL) {
    Klass* k = resolve_super_or_fail(ik->name(), expected_super->name(),
                                     class_loader, protection_domain, true, CHECK_0);
    InstanceKlass* actual_super = InstanceKlass::cast(k);
    if (class_loader() != actual_super->class_loader() && actual_super != ik->super() &&
        !has_same_hierarchy_for_unregistered_class(expected_super, actual_super, replaced_super_types)) {
      return false;
    }
  }

  Array<Klass*>* interfaces = ik->local_interfaces();
  int len = interfaces->length();
  for (int i = 0; i < len; i++) {
    Klass* expected_intf = interfaces->at(i);
    Klass* actual_intf = resolve_super_or_fail(ik->name(), expected_intf->name(), class_loader, protection_domain, false, CHECK_NULL);
    if (class_loader() != actual_intf->class_loader() && actual_intf != expected_intf &&
        !has_same_hierarchy_for_unregistered_class(expected_intf, actual_intf, replaced_super_types)) {
      return false;
    }
  }

  if (replaced_super_types->length() > 0) {
    ClassLoaderData* loader_data = ClassLoaderData::class_loader_data(class_loader());
    Array<Klass*>* local_interfaces = copy(loader_data, ik->local_interfaces(), CHECK_0);
    Array<Klass*>* transitive_interfaces = copy(loader_data, ik->transitive_interfaces(), CHECK_0);
    Array<Klass*>* secondary_supers = copy(loader_data, ik->secondary_supers(), CHECK_0);
    Array<Method*>* methods = copy(loader_data, ik->methods(), CHECK_0);
    Array<Method*>* default_methods = copy(loader_data, ik->default_methods(), CHECK_0);


    ik->set_local_interfaces(NULL);
    ik->set_local_interfaces(local_interfaces);
    ik->set_transitive_interfaces(NULL);
    ik->set_transitive_interfaces(transitive_interfaces);
    ik->set_secondary_supers(NULL);
    ik->set_secondary_supers(secondary_supers);
    ik->set_methods(methods);
    ik->set_default_methods(default_methods);

    SuperTypeReplacer replacer(ik, replaced_super_types);
    ik->klass_and_method_pointers_do(&replacer);
  }

  return true;
}

bool SystemDictionaryShared::add_non_builtin_klass(Symbol* name, ClassLoaderData* loader_data,
                                                   InstanceKlass* k,
                                                   TRAPS) {
  assert(DumpSharedSpaces, "only when dumping");
  SharedDictionary* dict = dumptime_shared_dictionary();
  assert(UseAppCDS && dict != NULL, "must be");

  if (dict->add_non_builtin_klass(name, loader_data, k)) {
    MutexLocker mu_r(Compile_lock, THREAD); // not really necessary, but add_to_hierarchy asserts this.
    add_to_hierarchy(k, CHECK_0);
    return true;
  }
  return false;
}

// This function is called to resolve the super/interfaces of shared classes for
// non-built-in loaders. E.g., ChildClass in the below example
// where "super:" (and optionally "interface:") have been specified.
//
// java/lang/Object id: 0
// Interface   id: 2 super: 0 source: cust.jar
// ChildClass  id: 4 super: 0 interfaces: 2 source: cust.jar
Klass* SystemDictionaryShared::dump_time_resolve_super_or_fail(
    Symbol* child_name, Symbol* class_name, Handle class_loader,
    Handle protection_domain, bool is_superclass, TRAPS) {

  assert(DumpSharedSpaces, "only when dumping");

  ClassListParser* parser = ClassListParser::instance();
  if (parser == NULL) {
    // We're still loading the well-known classes, before the ClassListParser is created.
    return NULL;
  }
  if (child_name->equals(parser->current_class_name())) {
    // When this function is called, all the numbered super and interface types
    // must have already been loaded. Hence this function is never recursively called.
    if (is_superclass) {
      return parser->lookup_super_for_current_class(class_name);
    } else {
      return parser->lookup_interface_for_current_class(class_name);
    }
  } else {
    // The VM is not trying to resolve a super type of parser->current_class_name().
    // Instead, it's resolving an error class (because parser->current_class_name() has
    // failed parsing or verification). Don't do anything here.
    return NULL;
  }
}

struct SharedMiscInfo {
  Klass* _klass;
  int _clsfile_size;
  int _clsfile_crc32;
  int _defining_loader_hash;
  int _initiating_loader_hash;
};

static GrowableArray<SharedMiscInfo>* misc_info_array = NULL;

void SystemDictionaryShared::set_shared_class_misc_info(Klass* k, ClassFileStream* cfs, int defining_loader_hash, int initiating_loader_hash) {
  assert(DumpSharedSpaces, "only when dumping");
  int clsfile_size  = cfs->length();
  int clsfile_crc32 = ClassLoader::crc32(0, (const char*)cfs->buffer(), cfs->length());

  if (misc_info_array == NULL) {
    misc_info_array = new (ResourceObj::C_HEAP, mtClass) GrowableArray<SharedMiscInfo>(20, /*c heap*/ true);
  }

  SharedMiscInfo misc_info;
  DEBUG_ONLY({
      for (int i=0; i<misc_info_array->length(); i++) {
        misc_info = misc_info_array->at(i);
        assert(misc_info._klass != k, "cannot call set_shared_class_misc_info twice for the same class");
      }
    });

  misc_info._klass = k;
  misc_info._clsfile_size = clsfile_size;
  misc_info._clsfile_crc32 = clsfile_crc32;
  misc_info._defining_loader_hash = defining_loader_hash;
  misc_info._initiating_loader_hash = initiating_loader_hash;

  misc_info_array->append(misc_info);
}

void SystemDictionaryShared::init_shared_dictionary_entry(Klass* k, DictionaryEntry* ent) {
  SharedDictionaryEntry* entry = (SharedDictionaryEntry*)ent;
  entry->_id = -1;
  entry->_clsfile_size = -1;
  entry->_clsfile_crc32 = -1;
  entry->_verifier_constraints = NULL;
  entry->_verifier_constraint_flags = NULL;
  entry->_defining_loader_hash = 0;
  entry->_initiating_loader_hash = 0;

  if (misc_info_array != NULL) {
    for (int i=0; i<misc_info_array->length(); i++) {
      SharedMiscInfo misc_info = misc_info_array->at(i);
      if (misc_info._klass == k) {
        entry->_clsfile_size = misc_info._clsfile_size;
        entry->_clsfile_crc32 = misc_info._clsfile_crc32;
        entry->_defining_loader_hash = misc_info._defining_loader_hash;
        entry->_initiating_loader_hash = misc_info._initiating_loader_hash;
        misc_info_array->remove_at(i);
        return;
      }
    }
  }
}

bool SystemDictionaryShared::add_verification_constraint(Klass* k, Symbol* name,
         Symbol* from_name, bool from_field_is_protected, bool from_is_array, bool from_is_object) {
  assert(DumpSharedSpaces, "called at dump time only");

  // Skip anonymous classes, which are not archived as they are not in
  // dictionary (see assert_no_anonymoys_classes_in_dictionaries() in
  // VM_PopulateDumpSharedSpace::doit()).
  if (k->class_loader_data()->is_anonymous()) {
    return true; // anonymous classes are not archived, skip
  }

  ResourceMark rm;
  // Lambda classes are not archived and will be regenerated at runtime.
  SharedDictionary* dict = dumptime_shared_dictionary();
  SharedDictionaryEntry* entry = (SharedDictionaryEntry*)dict->find_entry_for(k);
  if (entry == NULL && strstr(k->name()->as_C_string(), "Lambda$") != NULL) {
    return true;
  }
  assert(entry != NULL, "class should be in dictionary before being verified");
  entry->add_verification_constraint(name, from_name, from_field_is_protected,
                                     from_is_array, from_is_object);
  if (entry->is_builtin()) {
    // For builtin class loaders, we can try to complete the verification check at dump time,
    // because we can resolve all the constraint classes.
    return false;
  } else {
    // For non-builtin class loaders, we cannot complete the verification check at dump time,
    // because at dump time we don't know how to resolve classes for such loaders.
    return true;
  }
}

void SystemDictionaryShared::finalize_verification_constraints() {
  dumptime_shared_dictionary()->finalize_verification_constraints();
}

void SystemDictionaryShared::check_verification_constraints(InstanceKlass* klass,
                                                             TRAPS) {
  assert(!DumpSharedSpaces && UseSharedSpaces, "called at run time with CDS enabled only");
  SharedDictionary* dict = (SharedDictionary*)shared_dictionary();
  SharedDictionaryEntry* entry = (SharedDictionaryEntry*)(dict->find_entry_for(klass));
  assert(entry != NULL, "call this only for shared classes");
  entry->check_verification_constraints(klass, THREAD);
}

void SystemDictionaryShared::record_class_hierarchy() {
  dumptime_shared_dictionary()->record_class_hierarchy();
}

bool SystemDictionaryShared::has_same_hierarchy_for_unregistered_class(Klass* expected,
                                                                       Klass* actual,
                                                                       GrowableArray<InstanceKlass*>* replaced_super_types) {
  assert(actual != expected, "don't call this function unless necessary");

  if (!actual->is_shared()) {
    // Do not consider any class hierarchy that contains non-shared classes.
    return false;
  }

  // (1) builtin vs unregistered
  SharedDictionaryEntry* expected_entry = shared_dictionary()->find_entry_for(expected);
  if (!expected_entry->is_unregistered()) {
    // If the expected class is a boot/ext/app class, it cannot be substituted.
    return false;
  }

  SharedDictionaryEntry* actual_entry = shared_dictionary()->find_entry_for(actual);
  if (!actual_entry->is_unregistered()) {
    // The expected super class is an unregistered class, it cannot be substituted by
    // a boot/ext/app class
    return false;
  }

  // (2) fingerprint must match
  if (actual_entry->_clsfile_size  != expected_entry->_clsfile_size ||
      actual_entry->_clsfile_crc32 != expected_entry->_clsfile_crc32) {
   return false;
  }

  // (3) super must match
  Klass* actual_super = actual->super();
  Klass* expected_super = expected_entry->_recorded_super;

  if (actual_super == NULL) {
   if (expected_super != NULL) {
      return false;
    }
  } else {
    if (expected_super == NULL) {
     return false;
    }

    if (actual_super != expected_super &&
        !has_same_hierarchy_for_unregistered_class(expected_super, actual_super, replaced_super_types)) {
      // the hierarchy of the super class doesn't match
      return false;
    }
  }

  // (4) local interfaces must match
  Array<Klass*>* actual_local_interfaces = InstanceKlass::cast(actual)->local_interfaces();
  Array<Klass*>* expected_local_interfaces = expected_entry->_recorded_local_interfaces;
  int len = actual_local_interfaces->length();

  if (len == 0) {
    if (expected_local_interfaces != NULL) {
      return false;
    }
  } else {
    if (len != expected_local_interfaces->length()) {
      return false;
    }

    for (int i=0; i<len; i++) {
      Klass* actual_intf = actual_local_interfaces->at(i);
      Klass* expected_intf = expected_local_interfaces->at(i);
      if (actual_intf != expected_intf &&
          !has_same_hierarchy_for_unregistered_class(actual_intf, expected_intf, replaced_super_types)) {
        // the hierarchy of the interface doesn't match
        return false;
      }
    }
  }

  replaced_super_types->append(InstanceKlass::cast(expected));
  replaced_super_types->append(InstanceKlass::cast(actual));
  return true;
}

void SystemDictionaryShared::print() {
  if (DumpSharedSpaces) {
    dumptime_shared_dictionary()->print();
  }
}

SharedDictionaryEntry* SharedDictionary::find_entry_for(Klass* klass) {
  Symbol* class_name = klass->name();
  unsigned int hash = compute_hash(class_name, ClassLoaderData::the_null_class_loader_data());
  int index = hash_to_index(hash);

  for (SharedDictionaryEntry* entry = bucket(index);
                              entry != NULL;
                              entry = entry->next()) {
    if (entry->hash() == hash && entry->literal() == klass) {
      return entry;
    }
  }

  return NULL;
}

void SharedDictionary::finalize_verification_constraints() {
  int bytes = 0, count = 0;
  for (int index = 0; index < table_size(); index++) {
    for (SharedDictionaryEntry *probe = bucket(index);
                                probe != NULL;
                               probe = probe->next()) {
      int n = probe->finalize_verification_constraints();
      if (n > 0) {
        bytes += n;
        count ++;
      }
    }
  }
  if (TraceVerificationConstraints) {
    double avg = 0;
    if (count > 0) {
      avg = double(bytes) / double(count);
    }
    tty->print_cr("Recorded verification constraints for %d classes = %d bytes (avg = %.2f bytes) ", count, bytes, avg);
  }
}

void SharedDictionaryEntry::add_verification_constraint(Symbol* name,
         Symbol* from_name, bool from_field_is_protected, bool from_is_array, bool from_is_object) {
  if (_verifier_constraints == NULL) {
    _verifier_constraints = new(ResourceObj::C_HEAP, mtClass) GrowableArray<Symbol*>(8, true, mtClass);
  }
  if (_verifier_constraint_flags == NULL) {
    _verifier_constraint_flags = new(ResourceObj::C_HEAP, mtClass) GrowableArray<char>(4, true, mtClass);
  }
  GrowableArray<Symbol*>* vc_array = (GrowableArray<Symbol*>*)_verifier_constraints;
  for (int i=0; i<vc_array->length(); i+= 2) {
    if (name      == vc_array->at(i) &&
        from_name == vc_array->at(i+1)) {
      return;
    }
  }
  vc_array->append(name);
  vc_array->append(from_name);

  GrowableArray<char>* vcflags_array = (GrowableArray<char>*)_verifier_constraint_flags;
  char c = 0;
  c |= from_field_is_protected ? FROM_FIELD_IS_PROTECTED : 0;
  c |= from_is_array           ? FROM_IS_ARRAY           : 0;
  c |= from_is_object          ? FROM_IS_OBJECT          : 0;
  vcflags_array->append(c);

  if (TraceVerificationConstraints) {
    ResourceMark rm;
    tty->print_cr("add_verification_constraint: %s must be subclass of %s",
                                 from_name->as_klass_external_name(),
                                 name->as_klass_external_name());
  }
}

int SharedDictionaryEntry::finalize_verification_constraints() {
  assert(DumpSharedSpaces, "called at dump time only");
  Thread* THREAD = Thread::current();
  ClassLoaderData* loader_data = ClassLoaderData::the_null_class_loader_data();
  GrowableArray<Symbol*>* vc_array = (GrowableArray<Symbol*>*)_verifier_constraints;
  GrowableArray<char>* vcflags_array = (GrowableArray<char>*)_verifier_constraint_flags;

  if (vc_array != NULL) {
    if (TraceVerificationConstraints) {
      ResourceMark rm;
      tty->print_cr("finalize_verification_constraint: %s",
                                   literal()->external_name());
    }

    // Copy the constraints from C_HEAP-alloced GrowableArrays to Metaspace-alloced
    // Arrays
    int size = 0;
    {
      // FIXME: change this to be done after relocation, so we can use symbol offset??
      int length = vc_array->length();
      Array<Symbol*>* out = MetadataFactory::new_array<Symbol*>(loader_data, length, 0, THREAD);
      assert(out != NULL, "Dump time allocation failure would have aborted VM");
      for (int i=0; i<length; i++) {
        out->at_put(i, vc_array->at(i));
      }
      _verifier_constraints = out;
      size += out->size() * BytesPerWord;
      delete vc_array;
    }
    {
      int length = vcflags_array->length();
      Array<char>* out = MetadataFactory::new_array<char>(loader_data, length, 0, THREAD);
      assert(out != NULL, "Dump time allocation failure would have aborted VM");
      for (int i=0; i<length; i++) {
        out->at_put(i, vcflags_array->at(i));
      }
      _verifier_constraint_flags = out;
      size += out->size() * BytesPerWord;
      delete vcflags_array;
    }

    return size;
  }
  return 0;
}

void SharedDictionaryEntry::check_verification_constraints(InstanceKlass* klass, TRAPS) {
  Array<Symbol*>* vc_array = (Array<Symbol*>*)_verifier_constraints;
  Array<char>* vcflags_array = (Array<char>*)_verifier_constraint_flags;

  if (vc_array != NULL) {
    int length = vc_array->length();
    for (int i=0; i<length; i+=2) {
      Symbol* name      = vc_array->at(i);
      Symbol* from_name = vc_array->at(i+1);
      char c = vcflags_array->at(i/2);

      bool from_field_is_protected = (c & FROM_FIELD_IS_PROTECTED) ? true : false;
      bool from_is_array           = (c & FROM_IS_ARRAY)           ? true : false;
      bool from_is_object          = (c & FROM_IS_OBJECT)          ? true : false;

      bool ok = VerificationType::resolve_and_check_assignability(klass, name,
         from_name, from_field_is_protected, from_is_array, from_is_object, CHECK);
      if (!ok) {
        ResourceMark rm(THREAD);
        stringStream ss;

        ss.print_cr("Bad type on operand stack");
        ss.print_cr("Exception Details:");
        ss.print_cr("  Location:\n    %s", klass->name()->as_C_string());
        ss.print_cr("  Reason:\n    Type '%s' is not assignable to '%s'",
                    from_name->as_quoted_ascii(), name->as_quoted_ascii());
        THROW_MSG(vmSymbols::java_lang_VerifyError(), ss.as_string());
      }
    }
  }
}

void SharedDictionaryEntry::record_class_hierarchy() {
  Thread* THREAD = Thread::current();
  ClassLoaderData* loader_data = ClassLoaderData::the_null_class_loader_data();
  InstanceKlass* k = InstanceKlass::cast(literal());

  if (k->java_super() != NULL) {
    _recorded_super = k->java_super();
  }

  int len = k->local_interfaces()->length();
  if (len > 0) {
    Array<Klass*>* list = MetadataFactory::new_array<Klass*>(loader_data, len, 0, THREAD);
    for (int i=0; i<len; i++) {
      Klass* intf = k->local_interfaces()->at(i);
      list->at_put(i, intf);
    }
    _recorded_local_interfaces = list;
 }
}

void SharedDictionaryEntry::print() {
  ResourceMark rm;

  InstanceKlass* klass = (InstanceKlass*)literal();
  const char* loader;
  if (is_builtin()) {
    if (klass->is_shared_boot_class()) {
        loader = "boot";
    } else if (klass->is_shared_ext_class()) {
        loader = "ext";
    } else {
      assert(klass->is_shared_app_class(), "must be");
      loader = "app";
    }
  } else {
    assert(klass->is_shared_cus_class(), "must be");
    loader = "cust";
  }

  if (_id > 0) {
    tty->print("[%5d] ", _id);
  } else {
    tty->print("        ");
  }

  tty->print_cr("0x%08x " INTPTR_FORMAT " %-4s %s", hash(), p2i(klass), loader, klass->name()->as_klass_external_name());
  if (is_unregistered()) {
    Klass* super = _recorded_super;
    SharedDictionaryEntry* super_entry = SystemDictionaryShared::dumptime_shared_dictionary()->find_entry_for(super);
    tty->print_cr("          size: %d, crc: 0x%08x, super: " INTPTR_FORMAT " [%5d] %s",
                  _clsfile_size, _clsfile_crc32, p2i(super), super_entry->_id, super->name()->as_klass_external_name());
    if ( _recorded_local_interfaces != NULL) {
      int len = _recorded_local_interfaces->length();
      for (int i=0; i<len; i++) {
        Klass* intf = _recorded_local_interfaces->at(i);
        SharedDictionaryEntry* intf_entry = SystemDictionaryShared::dumptime_shared_dictionary()->find_entry_for(intf);
        tty->print_cr("          intf[%d]: " INTPTR_FORMAT " [%5d] %s",
                      i, p2i(intf), intf_entry->_id, intf->name()->as_klass_external_name());
      }
    }
  }
}

bool SharedDictionary::add_non_builtin_klass(const Symbol* class_name,
                                             ClassLoaderData* loader_data,
                                             InstanceKlass* klass) {

  assert(DumpSharedSpaces, "supported only when dumping");
  assert(klass != NULL, "adding NULL klass");
  assert(klass->name() == class_name, "sanity check on name");
  assert(klass->shared_classpath_index() < 0,
         "the shared classpath index should not be set for shared class loaded by the custom loaders");

  // Add an entry for a non-builtin class.
  // For a shared class for custom class loaders, SystemDictionary::resolve_or_null will
  // not find this class, because is_builtin() is false.
  unsigned int hash = compute_hash((Symbol*)class_name, loader_data);
  int index = hash_to_index(hash);

  assert(Dictionary::entry_size() >= sizeof(SharedDictionaryEntry), "must be big enough");
  SharedDictionaryEntry* entry = (SharedDictionaryEntry*)new_entry(hash, klass, loader_data);
  add_entry(index, entry);

  assert(entry->is_unregistered(), "sanity");
  assert(!entry->is_builtin(), "sanity");
  return true;
}

//-----------------
// SharedDictionary
//-----------------


Klass* SharedDictionary::find_class_for_builtin_loader(const Symbol* name) const {
  SharedDictionaryEntry* entry = get_entry_for_builtin_loader(name);
  return entry != NULL ? entry->literal() : (Klass*)NULL;
}

void SharedDictionary::update_entry(Klass* klass, int id) {
  assert(DumpSharedSpaces, "supported only when dumping");
  Symbol* class_name = klass->name();
  unsigned int hash = compute_hash(class_name, klass->class_loader_data());
  int index = hash_to_index(hash);

  for (SharedDictionaryEntry* entry = bucket(index);
                              entry != NULL;
                              entry = entry->next()) {
    if (entry->hash() == hash && entry->literal() == klass) {
      entry->_id = id;
      return;
    }
  }

  ShouldNotReachHere();
}

// the_null_class_loader_data is used for storing klass in archive
SharedDictionaryEntry* SharedDictionary::get_entry_for_builtin_loader(const Symbol* class_name) const {
  assert(!DumpSharedSpaces, "supported only when at runtime");
  // ClassLoaderData* class_loader_data = ClassLoaderData::the_null_class_loader_data();
  // Shared class use boot loader, the class_loader_data field is 0.
  ClassLoaderData* class_loader_data = NULL;

  unsigned int hash = compute_hash(class_name, class_loader_data);
  const int index = hash_to_index(hash);

  for (SharedDictionaryEntry* entry = bucket(index);
                              entry != NULL;
                              entry = entry->next()) {
    if (entry->hash() == hash && entry->equals((Symbol*)class_name, class_loader_data)) {
      if (entry->is_builtin()) {
        return entry;
      }
    }
  }
  return NULL;
}

// the_null_class_loader_data is used for storing klass in archive
SharedDictionaryEntry* SharedDictionary::get_entry_for_unregistered_loader(const Symbol* class_name) const {
  assert(!DumpSharedSpaces, "supported only when at runtime");
  // For shared dictoinary, CLD is null.
  ClassLoaderData* class_loader_data = NULL;
  unsigned int hash = compute_hash((Symbol*)class_name, class_loader_data);
  int index = hash_to_index(hash);

  for (SharedDictionaryEntry* entry = bucket(index);
                              entry != NULL;
                              entry = entry->next()) {
    if (entry->hash() == hash && entry->equals((Symbol*)class_name, class_loader_data)) {
      if (entry->is_unregistered()) {
          return entry;
      }
    }
  }
  return NULL;
}

void SharedDictionary::print() {
  for (int index = 0; index < table_size(); index++) {
    for (SharedDictionaryEntry *probe = bucket(index);
                                probe != NULL;
                               probe = probe->next()) {
      probe->print();
    }
  }
}

void SharedDictionary::record_class_hierarchy() {
  for (int index = 0; index < table_size(); index++) {
    for (SharedDictionaryEntry *probe = bucket(index);
                                probe != NULL;
                               probe = probe->next()) {
      if (probe->is_unregistered()) {
        probe->record_class_hierarchy();
      }
    }
  }
}
