/*
 * Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.
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


#ifndef SHARE_VM_CLASSFILE_SYSTEMDICTIONARYSHARED_HPP
#define SHARE_VM_CLASSFILE_SYSTEMDICTIONARYSHARED_HPP

#include "oops/klass.hpp"
#include "classfile/dictionary.hpp"
#include "classfile/systemDictionary.hpp"
#include "memory/filemap.hpp"

/*===============================================================================

    Handling of the classes in the AppCDS archive

    To ensure safety and to simplify the implementation, archived classes are
    "segregated" into several types. The following rules describe how they
    are stored and looked up.

[1] Category of archived classes

    There are 3 disjoint groups of classes stored in the AppCDS archive. They are
    categorized as by their SharedDictionaryEntry::loader_type()

    BUILTIN:              These classes may be defined ONLY by the BOOT/PLATFORM/APP
                          loaders.

    UNREGISTERED:         These classes may be defined ONLY by a ClassLoader
                          instance that's not listed above (using fingerprint matching)

[2] How classes from different categories are specified in the classlist:

    Starting from JDK9, each class in the classlist may be specified with
    these keywords: "id", "super", "interfaces", "loader" and "source".


    BUILTIN               Only the "id" keyword may be (optionally) specified. All other
                          keywords are forbidden.

                          The named class is looked up from the jimage and from
                          Xbootclasspath/a and CLASSPATH.

    UNREGISTERED:         The "id", "super", and "source" keywords must all be
                          specified.

                          The "interfaces" keyword must be specified if the class implements
                          one or more local interfaces. The "interfaces" keyword must not be
                          specified if the class does not implement local interfaces.

                          The named class is looked up from the location specified in the
                          "source" keyword.

    Example classlist:

    # BUILTIN
    java/lang/Object id: 0
    java/lang/Cloneable id: 1
    java/lang/String

    # UNREGISTERED
    Bar id: 3 super: 0 interfaces: 1 source: /foo.jar


[3] Identifying the loader_type of archived classes in the shared dictionary

    Each archived Klass* C is associated with a SharedDictionaryEntry* E

    BUILTIN:              (C->shared_classpath_index() >= 0)
    UNREGISTERED:         (C->shared_classpath_index() <  0)

[4] Lookup of archived classes at run time:

    (a) BUILTIN loaders:

        Search the shared directory for a BUILTIN class with a matching name.

    (b) UNREGISTERED loaders:

        The search originates with SystemDictionaryShared::lookup_from_stream().

        Search the shared directory for a UNREGISTERED class with a matching
        (name, clsfile_len, clsfile_crc32) tuple.

===============================================================================*/
#define UNREGISTERED_INDEX -9999
#define NOT_FOUND_CLASS "not.found.class"

class ClassFileStream;
// Archived classes need extra information not needed by traditionally loaded classes.
// To keep footprint small, we add these in the dictionary entry instead of the InstanceKlass.
class SharedDictionaryEntry : public DictionaryEntry {

public:
  enum LoaderType {
    LT_BUILTIN,
    LT_UNREGISTERED
  };

  enum {
    FROM_FIELD_IS_PROTECTED = 1 << 0,
    FROM_IS_ARRAY           = 1 << 1,
    FROM_IS_OBJECT          = 1 << 2
  };

  int             _id;
  int             _clsfile_size;
  int             _clsfile_crc32;
  int             _initiating_loader_hash;
  int             _defining_loader_hash;
  void*           _verifier_constraints; // FIXME - use a union here to avoid type casting??
  void*           _verifier_constraint_flags;

  // These are used to check that the runtime hierarchy matches the recorded hierarchy
  // at dumping time. For UNREGISTERED classes only.
  Klass* _recorded_super;
  Array<Klass*>*  _recorded_local_interfaces;

  // See "Identifying the loader_type of archived classes" comments above.
  LoaderType loader_type() const {
    Klass* k = (Klass*)literal();

    if ((k->shared_classpath_index() != UNREGISTERED_INDEX)) {
      return LT_BUILTIN;
    } else {
      return LT_UNREGISTERED;
    }
  }

  SharedDictionaryEntry* next() {
    return (SharedDictionaryEntry*)(DictionaryEntry::next());
  }

  bool is_builtin() const {
    return loader_type() == LT_BUILTIN;
  }
  bool is_unregistered() const {
    return loader_type() == LT_UNREGISTERED;
  }
  void add_verification_constraint(Symbol* name,
         Symbol* from_name, bool from_field_is_protected, bool from_is_array, bool from_is_object);
  int finalize_verification_constraints();
  void record_class_hierarchy();
  void check_verification_constraints(InstanceKlass* klass, TRAPS);
  void print();
};

class SharedDictionary : public Dictionary {
  SharedDictionaryEntry* get_entry_for_builtin_loader(const Symbol* name) const;

  // Convenience functions
  SharedDictionaryEntry* bucket(int index) const {
    return (SharedDictionaryEntry*)(Dictionary::bucket(index));
  }

public:
  SharedDictionaryEntry* get_entry_for_unregistered_loader(const Symbol* name) const;
  SharedDictionaryEntry* find_entry_for(Klass* klass);
  void finalize_verification_constraints();
  void record_class_hierarchy();

  bool add_non_builtin_klass(const Symbol* class_name,
                             ClassLoaderData* loader_data,
                             InstanceKlass* obj);

  void update_entry(Klass* klass, int id);

  Klass* find_class_for_builtin_loader(const Symbol* name) const;
  bool class_exists_for_unregistered_loader(const Symbol* name) {
    return (get_entry_for_unregistered_loader(name) != NULL);
  }
  void print();
};

class SystemDictionaryShared: public SystemDictionary {
 private:
   // These _shared_xxxs arrays are used to initialize the java.lang.Package and
   // java.security.ProtectionDomain objects associated with each shared class.
   //
   // See SystemDictionaryShared::init_security_info for more info.
   static objArrayOop _shared_protection_domains;
   static objArrayOop _shared_jar_urls;
   static objArrayOop _shared_jar_manifests;

   static oop _java_ext_loader;

   static instanceKlassHandle load_shared_class_for_builtin_loader(
                                                Symbol* class_name,
                                                Handle class_loader,
                                                TRAPS);
   static void define_shared_package(Symbol*  class_name,
                                     Handle class_loader,
                                     Handle manifest,
                                     Handle url,
                                     TRAPS);
   static Handle get_shared_jar_manifest(int shared_path_index, TRAPS);
 public:
   static Handle get_shared_jar_url(int shared_path_index, TRAPS);
 private:
   static Handle get_shared_protection_domain(Handle class_loader,
                                              int shared_path_index,
                                              Handle url,
                                              TRAPS);
   static Handle get_protection_domain_from_classloader(Handle class_loader,
                                                       Handle url, TRAPS);
 public:
   static Handle init_security_info(Handle class_loader, instanceKlassHandle ik, TRAPS);
   static Handle init_protection_domain(const Symbol *class_name, Handle class_loader, instanceKlassHandle ik, TRAPS);
   static bool check_not_found_class(Symbol *class_name, int hash_value);
   static void record_not_found_class(Symbol* class_name, int hash_value);
 private:
   static void atomic_set_array_index(objArrayOop array, int index, oop o);
   static InstanceKlass *load_class_from_cds(const Symbol* class_name, Handle class_loader, InstanceKlass *ik, int hash, TRAPS);

   static oop shared_protection_domain(int index) {
     return _shared_protection_domains->obj_at(index);
   }
   static void atomic_set_shared_protection_domain(int index, oop pd) {
     atomic_set_array_index(_shared_protection_domains, index, pd);
   }
   static void allocate_shared_protection_domain_array(int size, TRAPS);
   static oop shared_jar_url(int index) {
     return _shared_jar_urls->obj_at(index);
   }
   static void atomic_set_shared_jar_url(int index, oop url) {
     atomic_set_array_index(_shared_jar_urls, index, url);
   }
   static void allocate_shared_jar_url_array(int size, TRAPS);
   static oop shared_jar_manifest(int index) {
     return _shared_jar_manifests->obj_at(index);
   }
   static void atomic_set_shared_jar_manifest(int index, oop man) {
     atomic_set_array_index(_shared_jar_manifests, index, man);
   }
   static void allocate_shared_jar_manifest_array(int size, TRAPS);
  static bool has_same_hierarchy_for_unregistered_class(Klass* expected, Klass* actual,
                                                        GrowableArray<InstanceKlass*>* replaced_super_types);
  static bool check_hierarchy(InstanceKlass *ik, Handle class_loader,
                              Handle protection_domain,  GrowableArray<InstanceKlass*>* replaced_super_types,
                              TRAPS);
  template <class T>
  static Array<T>* copy(ClassLoaderData* loader_data, Array<T>* src, TRAPS);

public:
   static InstanceKlass* acquire_class_for_current_thread(
                                 InstanceKlass *ik,
                                 Handle class_loader,
                                 Handle protection_domain,
                                 TRAPS);

  static void initialize(TRAPS);
  // Called by PLATFORM/APP loader only
  static instanceKlassHandle find_or_load_shared_class(Symbol* class_name,
                                                       Handle class_loader,
                                                       TRAPS);
  static void allocate_shared_data_arrays(int size, TRAPS);
  static void oops_do(OopClosure* f);
  static void roots_oops_do(OopClosure* f) {
    oops_do(f);
  }

  static bool add_non_builtin_klass(Symbol* class_name, ClassLoaderData* loader_data,
                                    InstanceKlass* k, TRAPS);

  static Klass* dump_time_resolve_super_or_fail(Symbol* child_name,
                                                Symbol* class_name,
                                                Handle class_loader,
                                                Handle protection_domain,
                                                bool is_superclass,
                                                TRAPS);
  static size_t dictionary_entry_size() {
    return (DumpSharedSpaces) ? sizeof(SharedDictionaryEntry) : sizeof(DictionaryEntry);
  }
  static void init_shared_dictionary_entry(Klass* k, DictionaryEntry* entry) NOT_CDS_RETURN;
  static bool is_builtin(DictionaryEntry* ent) {
    // Can't use virtual function is_builtin because DictionaryEntry doesn't initialize
    // vtable because it's not constructed properly.
    SharedDictionaryEntry* entry = (SharedDictionaryEntry*)ent;
    return entry->is_builtin();
  }

  // For convenient access to the SharedDictionaryEntry's of the archived classes.
  static SharedDictionary* shared_dictionary() {
    assert(!DumpSharedSpaces, "not for dumping");
    return (SharedDictionary*)SystemDictionary::shared_dictionary();
  }

  static SharedDictionary* dumptime_shared_dictionary() {
    assert(DumpSharedSpaces, "for dumping only");
    return (SharedDictionary*)SystemDictionary::dictionary();
  }

  static void update_shared_entry(Klass* klass, int id) {
    assert(DumpSharedSpaces, "sanity");
    assert((SharedDictionary*)(SystemDictionary::dictionary()) != NULL, "sanity");
    ((SharedDictionary*)(SystemDictionary::dictionary()))->update_entry(klass, id);
  }

  static void set_shared_class_misc_info(Klass* k, ClassFileStream* cfs, int defining_loader_hash, int initiating_loader_hash);

  static bool is_builtin_loader(ClassLoaderData* loader_data);
  static bool is_supported(InstanceKlass* k);
  static bool is_hierarchy_supported(InstanceKlass* k);
  static void log_loaded_klass(InstanceKlass* k, const ClassFileStream* st,
                               ClassLoaderData* loader_data,
                               TRAPS) NOT_CDS_RETURN;
  static void log_not_founded_klass(const char* class_name,
                               Handle class_loader,
                               TRAPS) NOT_CDS_RETURN;
  static InstanceKlass* lookup_from_stream(const Symbol* class_name,
                                            Handle class_loader,
                                            Handle protection_domain,
                                            const ClassFileStream* st,
                                            TRAPS);

  static InstanceKlass* load_shared_class_for_registered_loader(Symbol* class_name,
                                                                Handle class_loader,
                                                                TRAPS);

  static InstanceKlass* lookup_shared(Symbol* class_name,
                                      Handle class_loader,
                                      bool &not_found,
                                      bool check_not_found,
                                      TRAPS);

  static InstanceKlass* define_class_from_cds(InstanceKlass *ik,
                                              Handle class_loader,
                                              Handle protection_domain,
                                              TRAPS);

  // The (non-application) CDS implementation supports only classes in the bootPS);
  // "verification_constraints" are a set of checks performed by
  // VerificationType::is_reference_assignable_from when verifying a shared class during
  // dump time.
  //
  // With AppCDS, it is possible to override archived classes by calling
  // ClassLoader.defineClass() directly. SystemDictionary::load_shared_class() already
  // ensures that you cannot load a shared class if its super type(s) are changed. However,
  // we need an additional check to ensure that the verification_constraints did not change
  // between dump time and runtime.
  static bool add_verification_constraint(Klass* k, Symbol* name,
                  Symbol* from_name, bool from_field_is_protected,
                  bool from_is_array, bool from_is_object) NOT_CDS_RETURN_(false);
  static void finalize_verification_constraints() NOT_CDS_RETURN;
  static void record_class_hierarchy() NOT_CDS_RETURN;
  static void check_verification_constraints(InstanceKlass* klass,
                                             TRAPS) NOT_CDS_RETURN;
  static bool has_same_hierarchy_for_unregistered_class(Klass* actual,
                                                        Klass* expected,
                                                        Handle class_loader);
  static void print();
};

#endif // SHARE_VM_CLASSFILE_SYSTEMDICTIONARYSHARED_HPP
