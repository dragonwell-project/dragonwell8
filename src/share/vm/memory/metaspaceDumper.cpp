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

#include "precompiled.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "memory/allocation.hpp"
#include "memory/binaryTreeDictionary.hpp"
#include "memory/freeList.hpp"
#include "memory/collectorPolicy.hpp"
#include "memory/filemap.hpp"
#include "memory/freeList.hpp"
#include "memory/gcLocker.hpp"
#include "memory/metachunk.hpp"
#include "memory/metaspace.hpp"
#include "memory/metaspaceGCThresholdUpdater.hpp"
#include "memory/metaspaceShared.hpp"
#include "memory/metaspaceTracer.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.hpp"
#include "runtime/atomic.inline.hpp"
#include "runtime/globals.hpp"
#include "runtime/init.hpp"
#include "runtime/java.hpp"
#include "runtime/mutex.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "services/memTracker.hpp"
#include "services/memoryService.hpp"
#include "utilities/copy.hpp"
#include "utilities/debug.hpp"
#include "utilities/dumpUtil.hpp"
#include "classfile/classLoader.hpp"
#include "memory/metaspaceDumper.hpp"

#define INDENT_SPACE_COUNT 2
// outputStream has the length limit of output string,
// so use STR_FORMAT to limit the max length of string, such as method name.
#define STR_FORMAT "%.512s"
#define PRINT_INDENT _dump_file->sp(_dump_file->indentation() * INDENT_SPACE_COUNT)
#define PRINT_INDENT_IN_CLOSURE _dumper.dump_file()->sp(_dumper.dump_file()->indentation() * INDENT_SPACE_COUNT)
#define PRINT_CR(format, ...) _dump_file->print_cr(format, ##__VA_ARGS__)
#define PRINT_CR_IN_CLOSURE(format, ...) _dumper.dump_file()->print_cr(format, ##__VA_ARGS__)
#define PRINT(format, ...) _dump_file->print(format, ##__VA_ARGS__)
#define PRINT_IN_CLOSURE(format, ...) _dumper.dump_file()->print(format, ##__VA_ARGS__)
#define PRINT_RAW(str) _dump_file->print_raw(str)
#define PRINT_RAW_IN_CLOSURE(str) _dumper.dump_file()->print_raw(str)

#define DUMP_REASON_NAME_INITIALIZE(name) #name,
const char* MetaspaceDumper::_reason_names[MetaspaceDumper::Reason_Terminating] = \
  { DUMP_REASON_DO(DUMP_REASON_NAME_INITIALIZE) };

MetaspaceDumper::MetaspaceDumper(const char* log_path, DumpReason reason,
                                 OOMEMetaspaceArea oome_area,
                                 ClassLoaderData* oome_loader_data)
  : _reason(reason),
    _oome_area(oome_area),
    _oome_thread(NULL),
    _oome_loader_data(oome_loader_data),
    _class_loader_label_name(NULL),
    _class_loader_label_name_sig(NULL)
{

  _dump_file = new(ResourceObj::C_HEAP, mtInternal) fileStream(log_path);

  if (_reason== MetaspaceDumper::OnOutOfMemoryError) {
    _oome_thread = JavaThread::current();
  }
}

MetaspaceDumper::~MetaspaceDumper() {
  if (_dump_file->is_open()) {
    _dump_file->flush();
  }
  delete _dump_file;
}

class VM_MetaspaceDumper : public VM_Operation {

  private:
    MetaspaceDumper& _dumper;

  public:
    VM_MetaspaceDumper(MetaspaceDumper& dumper) : _dumper(dumper) {}

    void doit();

    VMOp_Type type() const { return VMOp_MetaspaceDump; }
};

void VM_MetaspaceDumper::doit() {
  _dumper.do_dump();
}

void MetaspaceDumper::dump(const char* log_path, DumpReason reason, OOMEMetaspaceArea oome_area,
                           ClassLoaderData* oome_loader_data) {
  MetaspaceDumper dumper(log_path, reason, oome_area, oome_loader_data);
  // check file
  if (!dumper._dump_file->is_open()) {
    warning("MetaspaceDumper: Cannot open log file: %s", log_path);
    return;
  }

  VM_MetaspaceDumper dump_op(dumper);
  if (Thread::current()->is_VM_thread()) {
    dump_op.doit();
  } else {
    VMThread::execute(&dump_op);
  }
}

static char base_path[JVM_MAXPATHLEN] = {'\0'};
static uint dump_file_seq = 0;
static const char* dump_file_name = "java_pid";
static const char* dump_file_ext = ".mprof";

void MetaspaceDumper::dump(DumpReason reason, OOMEMetaspaceArea oome_area,
                           ClassLoaderData* oome_loader_data) {
  DumpAux aux(base_path, &dump_file_seq, dump_file_name, dump_file_ext, MetaspaceDumpPath);
  DumpPathBuilder path_builder(aux);
  const char* path = path_builder.build();
  if (path != NULL) {
    dump(path, reason, oome_area, oome_loader_data);
  }
}

void MetaspaceDumper::do_dump() {

  assert(SafepointSynchronize::is_at_safepoint() && Thread::current()->is_VM_thread(),
         "metaspace dump must be at safepoint and in vm thread");

  if (ClassLoaderModuleFieldName != NULL && ClassLoaderModuleFieldName[0] != '\0') {
    const char* label_field_name = ClassLoaderModuleFieldName;
    unsigned int hash = 0;
    _class_loader_label_name = SymbolTable::lookup_only(label_field_name, (int)strlen(label_field_name), hash);
    _class_loader_label_name_sig = vmSymbols::string_signature();
    assert(_class_loader_label_name_sig != NULL, "must not be null");
  }

  timer()->start();

  /**
   * Format:
   * [Basic Information]
   * Version : ...
   * ...
   */
  dump_basic_information();

  /**
   * Format:
   * [Class Space]
   * * Space : [0x00000007c0000000, 0x00000007c286d400, 0x00000007c2880000, 0x0000000800000000)
   *
   * [NonClass Spaces]
   * Space : ...
   * ...
   * * Space : ...
   */
  dump_virtual_spaces();


  /**
   * Format:
   * [Class Free Chunks]
   * Chunks : size = 4096 B, count = 1
   *   Chunk : 0x00002aab0420a000
   * ...
   *
   * [NoClass Free Chunks]
   * ...
   */
  dump_free_chunks();

  /**
   * Format:
   * [Global Shared Data]
   * Array (_the_array_interfaces_array) : 0x00007efe1c3e7028, size = 24 B, length = 2
   * ...
   */
  dump_global_shared_stuff();

  /**
   * Format:
   * [Class Loader Data]
   * ClassLoaderData : loader = 0x0000000000000000, loader_klass = 0x0000000000000000, loader_klass_name = <bootloader>, label = No Label
   *   Class Used Chunks :
   *     * Chunk : [0x00000007c23a8400, 0x00000007c23a8678, 0x00000007c23a8800)
   *   NonClass Used Chunks :
   *     * Chunk : [0x00007efcc92fa800, 0x00007efcc92fa978, 0x00007efcc92fb800)
   *   Klasses :
   *     Klass : 0x00000007c23a8428, name = sun/security/x509/X509CertImpl$$Lambda$28, size = 592 B
   *       ConstantPool : 0x00007efcc92fc850, size = 352 B
   *         ConstantPoolCache : 0x00007efcc92fa870, size = 144 B
   *       Array (local_interfaces) : 0x00007efcc92fc9b0, size = 16 B, length = 1
   *       ...
   *       Array (methods) : 0x00007efcc92fc9e0, size = 32 B, length = 3
   *         Method : 0x00007efcc92fcae8, name = get$Lambda(Lsun/security/x509/X509CertImpl;)Ljava/util/function/Function;, size = 104 B
   *           ConstMethod : 0x00007efcc92fcaa8, size = 64 B
   *           ...
   *         ...
   *       Array (default_methods) : 0x00007efcc92fa858, size = 24 B, length = 2
   *         ...
   *
   * [Unloading Class Loader Data]
   */
  dump_all_class_loader_data();

  timer()->stop();

  /**
   * [End Information]
   * Elapsed Time: xxx milliseconds
   *
   */
  dump_end_information();
}

class IndentHelper : public StackObj {

  private:
    MetaspaceDumper& _dumper;

  public:
    IndentHelper(MetaspaceDumper& dumper) : _dumper(dumper) {
      _dumper.dump_file()->inc();
    }

    ~IndentHelper() { _dumper.dump_file()->dec(); }
};

class SectionHelper : public StackObj {

  private:
    MetaspaceDumper& _dumper;

  public:
    SectionHelper(MetaspaceDumper& dumper, const char* section_title) : _dumper(dumper) {
      PRINT_CR_IN_CLOSURE("%s", section_title);
    }

    ~SectionHelper() { _dumper.dump_file()->cr(); }
};

void MetaspaceDumper::dump_basic_information() {
  SectionHelper section_helper(*this, "[Basic Information]");
  PRINT_CR("Version : (%s %s) (%s) (%s)", JDK_Version::distro_name(), JDK_Version::distro_version(),
           JDK_Version::runtime_version(), VM_Version::vm_release());
  PRINT_CR("Dump Reason : %s", _reason_names[_reason]);
  PRINT_CR("MaxMetaspaceSize : " SIZE_FORMAT " B", (size_t)MaxMetaspaceSize);
  if (Metaspace::using_class_space()) {
    PRINT_CR("CompressedClassSpaceSize : " SIZE_FORMAT " B", (size_t)CompressedClassSpaceSize);
  }
  if (reason() == MetaspaceDumper::OnOutOfMemoryError) {
    const char* area = oome_area() == MetaspaceDumper::Class ?
                                     "Class Metaspace" : "NonClass Metaspace";
    PRINT_CR("Out Of Memory Area : %s", area);
    PRINT_CR("Out Of Memory Thread Information : [");
    oome_thread()->print_on(_dump_file);
    oome_thread()->print_stack_on(_dump_file);
    PRINT_CR("]");
  }
  if (Metaspace::using_class_space()) {
    dump_space_basic_info("Class Space", Metaspace::ClassType);
  }
  dump_space_basic_info("NonClass Spaces", Metaspace::NonClassType);
}

void MetaspaceDumper::dump_space_basic_info(const char* space_name, Metaspace::MetadataType dataType) {
  PRINT_CR("%s Used : " SIZE_FORMAT " B", space_name, (size_t)(MetaspaceAux::used_bytes(dataType)));
  PRINT_CR("%s Capacity : " SIZE_FORMAT " B", space_name, (size_t)(MetaspaceAux::capacity_bytes(dataType)));
  PRINT_CR("%s Committed : " SIZE_FORMAT " B", space_name, (size_t)(MetaspaceAux::committed_bytes(dataType)));
  PRINT_CR("%s Reserved : " SIZE_FORMAT " B", space_name, (size_t)(MetaspaceAux::reserved_bytes(dataType)));
}

void MetaspaceDumper::dump_virtual_spaces() {
  // class type virtual spaces
  if (Metaspace::using_class_space()) {
    SectionHelper section_helper(*this, "[Class Space]");
    dump_virtual_spaces(Metaspace::ClassType);
  }

  // nonClass type virtual spaces
  SectionHelper section_helper(*this, "[NonClass Spaces]");
  dump_virtual_spaces(Metaspace::NonClassType);
}

class VirtualSpaceDumpClosure : public Metaspace::VirtualSpaceClosure {

  private:
   MetaspaceDumper& _dumper;

  public:
   VirtualSpaceDumpClosure(MetaspaceDumper& dumper) : _dumper(dumper) {}
   ~VirtualSpaceDumpClosure() {}

   void do_space(VirtualSpace *space, bool current, MetaWord *top) {
     if (current) {
       PRINT_RAW_IN_CLOSURE("* ");
     }
     PRINT_CR_IN_CLOSURE("Space : [" PTR_FORMAT ", " PTR_FORMAT ", "
                         PTR_FORMAT ", " PTR_FORMAT ")",
                         p2i(space->low()), p2i(top), p2i(space->high()),
                         p2i(space->high_boundary()));
   }
};

void MetaspaceDumper::dump_virtual_spaces(Metaspace::MetadataType metadataType) {
  VirtualSpaceDumpClosure closure(*this);
  Metaspace::spaces_do(metadataType, &closure);
}

void MetaspaceDumper::dump_free_chunks() {
  if (Metaspace::using_class_space()) {
    SectionHelper section_helper(*this, "[Class Free Chunks]");
    dump_free_chunks(Metaspace::ClassType);
  }

  SectionHelper section_helper(*this, "[NonClass Free Chunks]");
  dump_free_chunks(Metaspace::NonClassType);
}

// COB : Chunk or Block
template <class COB>
class FreeListDumpClosure : public FreeListClosure<FreeList<COB> > {

 private:
  MetaspaceDumper& _dumper;

 public:
  FreeListDumpClosure(MetaspaceDumper& dumper) : _dumper(dumper) {}
  ~FreeListDumpClosure() {}

  void do_list(FreeList<COB> *free_list) {
    PRINT_INDENT_IN_CLOSURE;
    PRINT_CR_IN_CLOSURE("%ss : size = " SIZE_FORMAT " B, count = " SIZE_FORMAT,
                        tag(), (size_t)(free_list->size() * sizeof(MetaWord)), (size_t)(free_list->count()));
    IndentHelper indent(_dumper);
    for (COB *chunk = free_list->head(); chunk != NULL; chunk = chunk->next()) {
      PRINT_INDENT_IN_CLOSURE;
      PRINT_CR_IN_CLOSURE("%s : " PTR_FORMAT, tag(), p2i(chunk));
    }
  }

 private:
  static const char* tag();
};

template<>
const char* FreeListDumpClosure<Metachunk>::tag() { return "Chunk"; }

template<>
const char* FreeListDumpClosure<Metablock>::tag() { return "Block"; }

void MetaspaceDumper::dump_free_chunks(Metaspace::MetadataType metadataType) {
  FreeListDumpClosure<Metachunk> closure(*this);
  Metaspace::free_chunks_do(metadataType, &closure);
}

void MetaspaceDumper::dump_global_shared_stuff() {
  SectionHelper section_helper(*this, "[Global Shared Data]");
  dump_array("_the_array_interfaces_array", Universe::the_array_interfaces_array());
  dump_array("_the_empty_int_array", Universe::the_empty_int_array());
  dump_array("_the_empty_short_array", Universe::the_empty_short_array());
  dump_array("_the_empty_method_array", Universe::the_empty_method_array());
  dump_array("_the_empty_klass_array", Universe::the_empty_klass_array());
}

void MetaspaceDumper::dump_all_class_loader_data() {
  {
    SectionHelper section_helper(*this, "[Class Loader Data]");
    for (ClassLoaderData* data = ClassLoaderDataGraph::_head; data != NULL; data = data->next()) {
      dump_class_loader_data(data);
    }
  }

  {
    SectionHelper section_helper(*this, "[Unloading Class Loader Data]");
    for (ClassLoaderData* data = ClassLoaderDataGraph::_unloading; data != NULL; data = data->next()) {
      dump_class_loader_data(data);
    }
  }
}

const char* MetaspaceDumper::find_module_field_name(ClassLoaderData* data) {
  // Note: there is no ResourceMark here, the caller should put ResourceMark at an appropriate place.
  const char* default_label = "N/A";
  if (_class_loader_label_name == NULL || data->is_unloading()) {
    return default_label;
  }
  if (_class_loader_label_name != NULL) {
    oop class_loader = data->class_loader();
    if (class_loader != NULL) {
      InstanceKlass* k = (InstanceKlass*)(class_loader->klass());
      fieldDescriptor fd;
      if (k->find_field(_class_loader_label_name, _class_loader_label_name_sig, &fd) != NULL) {
        oop str = class_loader->obj_field(fd.offset());
        if ( str != NULL ) {
          return java_lang_String::as_utf8_string(str);
        }
      }
    }
  }
  return default_label;
}

class KlassDumpClosure : public KlassClosure {

 private:
  MetaspaceDumper& _dumper;

 public:
  KlassDumpClosure(MetaspaceDumper& dumper) : _dumper(dumper) {
    PRINT_INDENT_IN_CLOSURE;
    PRINT_CR_IN_CLOSURE("Klasses :");
  }

  ~KlassDumpClosure() {}

  void do_klass(Klass* klass) {
    _dumper.dump_klass(klass);
  }
};


void MetaspaceDumper::dump_class_loader_data(ClassLoaderData* data) {
  ResourceMark rm;
  if (reason() == MetaspaceDumper::OnOutOfMemoryError && data == oome_loader_data()) {
    PRINT_RAW("* ");
  }
  bool is_unloading = data->is_unloading();
  oop class_loader = data->class_loader();
  PRINT_CR("ClassLoaderData : loader = " PTR_FORMAT ", loader_klass = "
            PTR_FORMAT ", loader_klass_name = " STR_FORMAT ", label = " STR_FORMAT, p2i(class_loader),
            class_loader == NULL || is_unloading ? NULL : p2i(class_loader->klass()),
            is_unloading ? "N/A" : data->loader_name(),
            find_module_field_name(data));

  Metaspace* metaspace = data->metaspace_or_null();
  IndentHelper indent(*this);

  if (metaspace != NULL) {
    dump_used_chunks(metaspace);
    GrowableArray<Metadata*>* deallocate_list = data->_deallocate_list;
    if (deallocate_list != NULL) {
      dump_deallocate_list(deallocate_list);
    }
    KlassDumpClosure klass_dump_closure(*this);
    IndentHelper _indent(*this);
    data->classes_do(&klass_dump_closure);
  }
}

class InUsedChunkDumpClosure : public Metaspace::InUsedChunkClosure {

 private:
  MetaspaceDumper& _dumper;

 public:
  InUsedChunkDumpClosure(MetaspaceDumper &dumper) : _dumper(dumper) {}
  ~InUsedChunkDumpClosure() {}

  void do_chunk(Metachunk *chunk, bool current) {
    PRINT_INDENT_IN_CLOSURE;
    if (current) {
      PRINT_RAW_IN_CLOSURE("* ");
    }
    PRINT_CR_IN_CLOSURE("Chunk : [" PTR_FORMAT ", " PTR_FORMAT ", " PTR_FORMAT ")",
                        p2i(chunk->bottom()), p2i(chunk->top()), p2i(chunk->end()));
  }
};

void MetaspaceDumper::dump_used_chunks(Metaspace *metaspace) {
  if (Metaspace::using_class_space()) {
    dump_used_chunks(metaspace, Metaspace::ClassType);
  }
  dump_used_chunks(metaspace, Metaspace::NonClassType);
}

void MetaspaceDumper::dump_used_chunks(Metaspace *metaspace, Metaspace::MetadataType metadataType) {
  assert(metaspace != NULL, "must not be null");
  InUsedChunkDumpClosure in_used_chunk_dump_closure(*this);
  FreeListDumpClosure<Metablock> free_block_dump_closure(*this);
  PRINT_INDENT;
  PRINT_CR(metadataType == Metaspace::ClassType ? "Class Used Chunks :" : "NonClass Used Chunks :");
  IndentHelper indent(*this);
  metaspace->in_used_chunks_do(metadataType, &in_used_chunk_dump_closure);
  metaspace->free_blocks_do(metadataType, &free_block_dump_closure);
}

void MetaspaceDumper::dump_deallocate_list(GrowableArray<Metadata*>* deallocate_list) {
  PRINT_INDENT;
  PRINT_CR("Deallocate List :");
  IndentHelper indent(*this);
  for (int i = deallocate_list->length() - 1; i >= 0; i--) {
    Metadata* m = deallocate_list->at(i);
    bool on_stack = m->on_stack();
    if (m->is_method()) {
      Method* method = (Method*) m;
      dump_method(method, on_stack);
    } else if (m->is_constantPool()) {
       ConstantPool* pool = (ConstantPool*) m;
       dump_constant_pool(pool, on_stack);
    } else if (m->is_klass()) {
      InstanceKlass* klass = (InstanceKlass*) m;
      dump_klass(klass, on_stack);
    } else {
      ShouldNotReachHere();
    }
  }
}

void MetaspaceDumper::dump_klass(Klass* klass, bool append_asterisk) {
  ResourceMark rm;
  PRINT_INDENT;
  if (append_asterisk) {
    PRINT_RAW("* ");
  }
  PRINT_CR("Klass : " PTR_FORMAT ", name = " STR_FORMAT ", size = " SIZE_FORMAT " B",
           p2i(klass), klass->name()->as_C_string(), (size_t)klass->size() * HeapWordSize);

  IndentHelper indent(*this);
  if (klass->oop_is_instance()) {
    dump_instance_klass((InstanceKlass*) klass);
  } else if (klass->oop_is_array()) {
    dump_array_klass((ArrayKlass*) klass);
  } else {
    ShouldNotReachHere();
  }
}

void MetaspaceDumper::dump_instance_klass(InstanceKlass* klass) {

  dump_constant_pool(klass->constants());

  Annotations* annotations = klass->annotations();
  if (annotations != NULL) {
    dump_annotations(annotations);
  }

  dump_array("local_interfaces", klass->local_interfaces());
  dump_array("transitive_interfaces", klass->transitive_interfaces());
  dump_array("secondary_supers", klass->secondary_supers());
  dump_array("inner_classes", klass->inner_classes());
  dump_array("method_ordering", klass->method_ordering());
  dump_array("default_vtable_indices", klass->default_vtable_indices());
  dump_array("fields", klass->fields());

  Array<Method*>* methods = klass->methods();
  if (methods != NULL) {
    dump_methods(methods, false);
  }

  Array<Method*>* default_methods = klass->default_methods();
  if (default_methods != NULL) {
    dump_methods(default_methods, true);
  }
}

void MetaspaceDumper::dump_array_klass(ArrayKlass* klass) {
  dump_array("secondary_supers", klass->secondary_supers());
}

void MetaspaceDumper::dump_constant_pool(ConstantPool* pool, bool append_asterisk) {
  PRINT_INDENT;
  if (append_asterisk) {
    PRINT_RAW("* ");
  }
  PRINT_CR("ConstantPool : " PTR_FORMAT ", size = " SIZE_FORMAT " B",
           p2i(pool), (size_t)pool->size() * HeapWordSize);
  IndentHelper indent(*this);
  dump_array("tags", pool->tags());
  dump_array("jwp_tags", pool->jwp_tags());
  dump_array("operands", pool->operands());
  dump_array("reference_map", pool->reference_map());

  ConstantPoolCache* cache = pool->cache();
  if (cache != NULL) {
    PRINT_INDENT;
    PRINT_CR("ConstantPoolCache : " PTR_FORMAT ", size = " SIZE_FORMAT " B",
             p2i(cache), (size_t)cache->size() * HeapWordSize);
  }
}

void MetaspaceDumper::dump_annotations(Annotations* annotations) {
  PRINT_INDENT;
  PRINT_CR("Annotations : " PTR_FORMAT ", size = " SIZE_FORMAT " B",
           p2i(annotations), (size_t)annotations->size() * HeapWordSize);

  IndentHelper indent(*this);
  dump_array("class_annotations", annotations->class_annotations());
  Array<AnnotationArray*>* fields_annotations = annotations->fields_annotations();
  if (fields_annotations != NULL) {
    dump_array("fields_annotations", fields_annotations);
    IndentHelper _indent(*this);
    int len = fields_annotations->length();
    for (int i = 0; i < len; i++) {
      dump_array("fields_annotations_array", fields_annotations->at(i));
    }
  }

  dump_array("class_type_annotations", annotations->class_type_annotations());
  Array<AnnotationArray*>* fields_type_annotations = annotations->fields_type_annotations();
  if (fields_type_annotations!= NULL) {
    dump_array("fields_type_annotations", fields_type_annotations);
    IndentHelper indent(*this);
    int len = fields_type_annotations->length();
    for (int i = 0; i < len; i++) {
      dump_array("fields_type_annotations_array", fields_type_annotations->at(i));
    }
  }
}

void MetaspaceDumper::dump_methods(Array<Method*>* methods, bool is_default) {
  dump_array(is_default ? "default_methods" : "methods", methods);
  IndentHelper indent(*this);
  int len = methods->length();
  for (int i = 0; i < len; i++) {
     dump_method(methods->at(i));
  }
}

void MetaspaceDumper::dump_method(Method* method, bool append_asterisk) {
  ResourceMark rm;
  PRINT_INDENT;
  if (append_asterisk) {
    PRINT_RAW("* ");
  }
  PRINT_CR("Method : " PTR_FORMAT ", name = " STR_FORMAT  STR_FORMAT ", size = " SIZE_FORMAT " B",
           p2i(method), method->name()->as_C_string(), method->signature()->as_C_string(),
           (size_t)method->method_size() * BytesPerWord);

  IndentHelper indent(*this);
  dump_const_method(method->constMethod());

  MethodData* method_data = method->method_data();
  if (method_data != NULL) {
    PRINT_INDENT;
    PRINT_CR("MethodData : " PTR_FORMAT ", size = " SIZE_FORMAT " B",
             p2i(method_data), (size_t)method_data->size_in_bytes());
  }

  MethodCounters* method_counters = method->method_counters();
  if (method_counters != NULL) {
    PRINT_INDENT;
    PRINT_CR("MethodCounters : " PTR_FORMAT ", size = " SIZE_FORMAT " B",
             p2i(method_counters), (size_t)MethodCounters::size() * wordSize);
  }
}

void MetaspaceDumper::dump_const_method(ConstMethod* constMethod) {
  PRINT_INDENT;
  PRINT_CR("ConstMethod : " PTR_FORMAT ", size = " SIZE_FORMAT " B",
           p2i(constMethod), (size_t)constMethod->size() * BytesPerWord);
  IndentHelper indent(*this);
  dump_array("stackmap_data", constMethod->stackmap_data());
  dump_array("method_annotations", constMethod->method_annotations());
  dump_array("parameter_annotations", constMethod->parameter_annotations());
  dump_array("type_annotations", constMethod->type_annotations());
  dump_array("default_annotations", constMethod->default_annotations());
}

template <typename T>
void MetaspaceDumper::dump_array(const char* desc, Array<T>* array) {
  if (array != NULL) {
    PRINT_INDENT;
    PRINT_CR("Array (%s) : " PTR_FORMAT ", size = " SIZE_FORMAT " B, length = " SIZE_FORMAT,
             desc, p2i(array), (size_t)array->size() * BytesPerWord, (size_t)array->length());
  }
}

void MetaspaceDumper::dump_end_information() {
  SectionHelper section_helper(*this, "[End Information]");
  PRINT_CR("Elapsed Time: %lu milliseconds", timer()->milliseconds());
}
