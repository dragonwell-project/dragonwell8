/*
 * Copyright (c) 2021, Alibaba Group Holding Limited. All rights reserved.
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

#ifndef SHARE_VM_MEMORY_METASPACEDUMPER_HPP
#define SHARE_VM_MEMORY_METASPACEDUMPER_HPP

#include "memory/metaspace.hpp"
#include "classfile/classLoaderData.hpp"

#define DUMP_REASON_ENUM(source) source,

#define DUMP_REASON_DO(template)     \
  template(JCMD)                     \
  template(BeforeFullGC)             \
  template(AfterFullGC)              \
  template(OnOutOfMemoryError)       \

template <typename T> class Array;
template <typename T> class GrowableArray;
class Annotations;
class Metadata;
class MetaspaceDumper : public StackObj {
  friend class DumperHolder;
  friend class KlassDumpClosure;
  friend class VM_MetaspaceDumper;

  public:
    enum DumpReason {
      DUMP_REASON_DO(DUMP_REASON_ENUM)
      Reason_Terminating
    };

    // area which caused the OOME, None means the dump was NOT caused by OOME.
    enum OOMEMetaspaceArea {
      Class,
      NonClass,
      None
    };

  private:
    fileStream* _dump_file;

    DumpReason _reason;
    OOMEMetaspaceArea _oome_area;
    JavaThread* _oome_thread;
    ClassLoaderData* _oome_loader_data;

    elapsedTimer _timer;

    // support for dump the label of class loader
    Symbol* _class_loader_label_name;
    Symbol* _class_loader_label_name_sig;

    static const char* _reason_names[];

  public:
    MetaspaceDumper(const char *log_path, DumpReason source,
                    OOMEMetaspaceArea oome_area = None,
                    ClassLoaderData* oome_loader_data = NULL);
    ~MetaspaceDumper();

    static void dump(const char* log_path, DumpReason reason,
                     OOMEMetaspaceArea oome_area = None,
                     ClassLoaderData* oome_loader_data = NULL);
    static void dump(DumpReason reason, OOMEMetaspaceArea oome_area = None,
                     ClassLoaderData* oome_loader_data = NULL);

    fileStream* dump_file() const             { return _dump_file; }

  private:
    DumpReason reason() const                 { return _reason; }
    OOMEMetaspaceArea oome_area() const       { return _oome_area; }
    JavaThread* oome_thread() const           { return _oome_thread; }
    ClassLoaderData* oome_loader_data() const { return _oome_loader_data; }
    elapsedTimer* timer()                     { return &_timer; }

    void do_dump();

    void dump_basic_information();
    void dump_space_basic_info(const char* space_name, Metaspace::MetadataType dataType);
    void dump_end_information();

    void dump_virtual_spaces();
    void dump_virtual_spaces(Metaspace::MetadataType);

    void dump_free_chunks();
    void dump_free_chunks(Metaspace::MetadataType);

    void dump_global_shared_stuff();

    void dump_all_class_loader_data();
    void dump_class_loader_data(ClassLoaderData*);
    const char* find_module_field_name(ClassLoaderData*);

    void dump_used_chunks(Metaspace *metaspace);
    void dump_used_chunks(Metaspace *metaspace, Metaspace::MetadataType);

    void dump_deallocate_list(GrowableArray<Metadata*>*);

    void dump_klass(Klass* klass, bool append_asterisk = false);
    void dump_instance_klass(InstanceKlass*);
    void dump_array_klass(ArrayKlass*);

    void dump_constant_pool(ConstantPool*, bool append_asterisk = false);

    void dump_annotations(Annotations*);

    void dump_methods(Array<Method*>* methods, bool is_default = false);
    void dump_method(Method*, bool append_asterisk = false);
    void dump_const_method(ConstMethod*);

    template <typename T>
    void dump_array(const char* desc, Array<T>*);
};
#endif // SHARE_VM_MEMORY_METASPACEDUMPER_HPP
