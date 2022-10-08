/*
 * Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/dictionary.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/loaderConstraints.hpp"
#include "classfile/placeholders.hpp"
#include "classfile/systemDictionary.hpp"
#include "ci/ciField.hpp"
#include "ci/ciInstance.hpp"
#include "ci/ciObjArrayKlass.hpp"
#include "ci/ciMethodData.hpp"
#include "ci/ciSymbol.hpp"
#include "code/codeBlob.hpp"
#include "code/codeCache.hpp"
#include "code/compressedStream.hpp"
#include "code/location.hpp"
#include "code/nmethod.hpp"
#include "code/pcDesc.hpp"
#include "code/stubs.hpp"
#include "code/vmreg.hpp"
#include "compiler/oopMap.hpp"
#include "compiler/compileBroker.hpp"
#include "gc_implementation/shared/immutableSpace.hpp"
#include "gc_implementation/shared/markSweep.hpp"
#include "gc_implementation/shared/mutableSpace.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "interpreter/bytecodeInterpreter.hpp"
#include "interpreter/bytecodes.hpp"
#include "interpreter/interpreter.hpp"
#include "memory/allocation.hpp"
#include "memory/cardTableRS.hpp"
#include "memory/defNewGeneration.hpp"
#include "memory/freeBlockDictionary.hpp"
#include "memory/genCollectedHeap.hpp"
#include "memory/generation.hpp"
#include "memory/generationSpec.hpp"
#include "memory/heap.hpp"
#include "memory/metachunk.hpp"
#include "memory/referenceType.hpp"
#include "memory/space.hpp"
#include "memory/tenuredGeneration.hpp"
#include "memory/universe.hpp"
#include "memory/watermark.hpp"
#include "oops/arrayKlass.hpp"
#include "oops/arrayOop.hpp"
#include "oops/compiledICHolder.hpp"
#include "oops/constMethod.hpp"
#include "oops/constantPool.hpp"
#include "oops/cpCache.hpp"
#include "oops/instanceClassLoaderKlass.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "oops/instanceOop.hpp"
#include "oops/klass.hpp"
#include "oops/markOop.hpp"
#include "oops/methodData.hpp"
#include "oops/methodCounters.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/objArrayOop.hpp"
#include "oops/oop.inline.hpp"
#include "oops/symbol.hpp"
#include "oops/typeArrayKlass.hpp"
#include "oops/typeArrayOop.hpp"
#include "prims/jvmtiAgentThread.hpp"
#include "runtime/arguments.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/vframeArray.hpp"
#include "runtime/globals.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/perfMemory.hpp"
#include "runtime/serviceThread.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/virtualspace.hpp"
#include "runtime/vmStructs.hpp"
#include "utilities/array.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/hashtable.hpp"
#include "utilities/macros.hpp"

#ifdef TARGET_ARCH_x86
# include "vmStructs_x86.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "vmStructs_aarch64.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "vmStructs_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "vmStructs_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "vmStructs_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "vmStructs_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_x86
# include "vmStructs_linux_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_aarch64
# include "vmStructs_linux_aarch64.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_sparc
# include "vmStructs_linux_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_zero
# include "vmStructs_linux_zero.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_x86
# include "vmStructs_solaris_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_sparc
# include "vmStructs_solaris_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_windows_x86
# include "vmStructs_windows_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_arm
# include "vmStructs_linux_arm.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_ppc
# include "vmStructs_linux_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_aix_ppc
# include "vmStructs_aix_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_x86
# include "vmStructs_bsd_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_zero
# include "vmStructs_bsd_zero.hpp"
#endif
#if INCLUDE_ALL_GCS
#include "gc_implementation/concurrentMarkSweep/compactibleFreeListSpace.hpp"
#include "gc_implementation/concurrentMarkSweep/concurrentMarkSweepGeneration.hpp"
#include "gc_implementation/concurrentMarkSweep/concurrentMarkSweepThread.hpp"
#include "gc_implementation/concurrentMarkSweep/vmStructs_cms.hpp"
#include "gc_implementation/parNew/parNewGeneration.hpp"
#include "gc_implementation/parNew/vmStructs_parNew.hpp"
#include "gc_implementation/parallelScavenge/asPSOldGen.hpp"
#include "gc_implementation/parallelScavenge/asPSYoungGen.hpp"
#include "gc_implementation/parallelScavenge/parallelScavengeHeap.hpp"
#include "gc_implementation/parallelScavenge/psOldGen.hpp"
#include "gc_implementation/parallelScavenge/psVirtualspace.hpp"
#include "gc_implementation/parallelScavenge/psYoungGen.hpp"
#include "gc_implementation/parallelScavenge/vmStructs_parallelgc.hpp"
#include "gc_implementation/g1/vmStructs_g1.hpp"
#endif // INCLUDE_ALL_GCS

#ifdef COMPILER2
#include "opto/addnode.hpp"
#include "opto/block.hpp"
#include "opto/callnode.hpp"
#include "opto/cfgnode.hpp"
#include "opto/chaitin.hpp"
#include "opto/divnode.hpp"
#include "opto/locknode.hpp"
#include "opto/loopnode.hpp"
#include "opto/machnode.hpp"
#include "opto/matcher.hpp"
#include "opto/mathexactnode.hpp"
#include "opto/mulnode.hpp"
#include "opto/phaseX.hpp"
#include "opto/parse.hpp"
#include "opto/regalloc.hpp"
#include "opto/rootnode.hpp"
#include "opto/subnode.hpp"
#include "opto/vectornode.hpp"
#if defined ADGLOBALS_MD_HPP
# include ADGLOBALS_MD_HPP
#elif defined TARGET_ARCH_MODEL_x86_32
# include "adfiles/adGlobals_x86_32.hpp"
#elif defined TARGET_ARCH_MODEL_x86_64
# include "adfiles/adGlobals_x86_64.hpp"
#elif defined TARGET_ARCH_MODEL_aarch64
# include "adfiles/adGlobals_aarch64.hpp"
#elif defined TARGET_ARCH_MODEL_sparc
# include "adfiles/adGlobals_sparc.hpp"
#elif defined TARGET_ARCH_MODEL_zero
# include "adfiles/adGlobals_zero.hpp"
#elif defined TARGET_ARCH_MODEL_ppc_64
# include "adfiles/adGlobals_ppc_64.hpp"
#endif
#endif // COMPILER2

// Note: the cross-product of (c1, c2, product, nonproduct, ...),
// (nonstatic, static), and (unchecked, checked) has not been taken.
// Only the macros currently needed have been defined.

// A field whose type is not checked is given a null string as the
// type name, indicating an "opaque" type to the serviceability agent.

// NOTE: there is an interdependency between this file and
// HotSpotTypeDataBase.java, which parses the type strings.

#ifndef REG_COUNT
  #define REG_COUNT 0
#endif
// whole purpose of this function is to work around bug c++/27724 in gcc 4.1.1
// with optimization turned on it doesn't affect produced code
static inline uint64_t cast_uint64_t(size_t x)
{
  return x;
}

#if INCLUDE_JVMTI
  #define JVMTI_STRUCTS(static_field) \
    static_field(JvmtiExport,                     _can_access_local_variables,                  bool)                                  \
    static_field(JvmtiExport,                     _can_hotswap_or_post_breakpoint,              bool)                                  \
    static_field(JvmtiExport,                     _can_post_on_exceptions,                      bool)                                  \
    static_field(JvmtiExport,                     _can_walk_any_space,                          bool)
#else
  #define JVMTI_STRUCTS(static_field)
#endif // INCLUDE_JVMTI

typedef HashtableEntry<intptr_t, mtInternal>  IntptrHashtableEntry;
typedef Hashtable<intptr_t, mtInternal>       IntptrHashtable;
typedef Hashtable<Symbol*, mtSymbol>          SymbolHashtable;
typedef HashtableEntry<Symbol*, mtClass>      SymbolHashtableEntry;
typedef Hashtable<oop, mtSymbol>              StringHashtable;
typedef TwoOopHashtable<Klass*, mtClass>      KlassTwoOopHashtable;
typedef Hashtable<Klass*, mtClass>            KlassHashtable;
typedef HashtableEntry<Klass*, mtClass>       KlassHashtableEntry;
typedef TwoOopHashtable<Symbol*, mtClass>     SymbolTwoOopHashtable;

//--------------------------------------------------------------------------------
// VM_STRUCTS
//
// This list enumerates all of the fields the serviceability agent
// needs to know about. Be sure to see also the type table below this one.
// NOTE that there are platform-specific additions to this table in
// vmStructs_<os>_<cpu>.hpp.

#define VM_STRUCTS(nonstatic_field, \
                   static_field, \
                   static_ptr_volatile_field, \
                   unchecked_nonstatic_field, \
                   volatile_nonstatic_field, \
                   nonproduct_nonstatic_field, \
                   c1_nonstatic_field, \
                   c2_nonstatic_field, \
                   unchecked_c1_static_field, \
                   unchecked_c2_static_field) \
                                                                                                                                     \
  /******************************************************************/                                                               \
  /* OopDesc and Klass hierarchies (NOTE: MethodData* incomplete) */                                                                 \
  /******************************************************************/                                                               \
                                                                                                                                     \
  volatile_nonstatic_field(oopDesc,            _mark,                                         markOop)                               \
  volatile_nonstatic_field(oopDesc,            _metadata._klass,                              Klass*)                                \
  volatile_nonstatic_field(oopDesc,            _metadata._compressed_klass,                   narrowOop)                             \
     static_field(oopDesc,                     _bs,                                           BarrierSet*)                           \
  nonstatic_field(ArrayKlass,                  _dimension,                                    int)                                   \
  volatile_nonstatic_field(ArrayKlass,         _higher_dimension,                             Klass*)                                \
  volatile_nonstatic_field(ArrayKlass,         _lower_dimension,                              Klass*)                                \
  nonstatic_field(ArrayKlass,                  _vtable_len,                                   int)                                   \
  nonstatic_field(ArrayKlass,                  _component_mirror,                             oop)                                   \
  nonstatic_field(CompiledICHolder,     _holder_metadata,                              Metadata*)                             \
  nonstatic_field(CompiledICHolder,     _holder_klass,                                 Klass*)                                \
  nonstatic_field(ConstantPool,         _tags,                                         Array<u1>*)                            \
  nonstatic_field(ConstantPool,         _cache,                                        ConstantPoolCache*)                    \
  nonstatic_field(ConstantPool,         _pool_holder,                                  InstanceKlass*)                        \
  nonstatic_field(ConstantPool,         _operands,                                     Array<u2>*)                            \
  nonstatic_field(ConstantPool,         _length,                                       int)                                   \
  nonstatic_field(ConstantPool,         _resolved_references,                          jobject)                               \
  nonstatic_field(ConstantPool,         _reference_map,                                Array<u2>*)                            \
  nonstatic_field(ConstantPoolCache,    _length,                                       int)                                   \
  nonstatic_field(ConstantPoolCache,    _constant_pool,                                ConstantPool*)                         \
  nonstatic_field(InstanceKlass,               _array_klasses,                                Klass*)                                \
  nonstatic_field(InstanceKlass,               _methods,                                      Array<Method*>*)                       \
  nonstatic_field(InstanceKlass,               _default_methods,                              Array<Method*>*)                       \
  nonstatic_field(InstanceKlass,               _local_interfaces,                             Array<Klass*>*)                        \
  nonstatic_field(InstanceKlass,               _transitive_interfaces,                        Array<Klass*>*)                        \
  nonstatic_field(InstanceKlass,               _fields,                                       Array<u2>*)                            \
  nonstatic_field(InstanceKlass,               _java_fields_count,                            u2)                                    \
  nonstatic_field(InstanceKlass,               _constants,                                    ConstantPool*)                         \
  nonstatic_field(InstanceKlass,               _class_loader_data,                            ClassLoaderData*)                      \
  nonstatic_field(InstanceKlass,               _source_file_name_index,                            u2)                               \
  nonstatic_field(InstanceKlass,               _source_debug_extension,                       char*)                                 \
  nonstatic_field(InstanceKlass,               _inner_classes,                               Array<jushort>*)                       \
  nonstatic_field(InstanceKlass,               _nonstatic_field_size,                         int)                                   \
  nonstatic_field(InstanceKlass,               _static_field_size,                            int)                                   \
  nonstatic_field(InstanceKlass,               _static_oop_field_count,                       u2)                                   \
  nonstatic_field(InstanceKlass,               _nonstatic_oop_map_size,                       int)                                   \
  nonstatic_field(InstanceKlass,               _is_marked_dependent,                          bool)                                  \
  nonstatic_field(InstanceKlass,               _minor_version,                                u2)                                    \
  nonstatic_field(InstanceKlass,               _major_version,                                u2)                                    \
  nonstatic_field(InstanceKlass,               _init_state,                                   u1)                                    \
  nonstatic_field(InstanceKlass,               _init_thread,                                  Thread*)                               \
  nonstatic_field(InstanceKlass,               _vtable_len,                                   int)                                   \
  nonstatic_field(InstanceKlass,               _itable_len,                                   int)                                   \
  nonstatic_field(InstanceKlass,               _reference_type,                               u1)                                    \
  volatile_nonstatic_field(InstanceKlass,      _oop_map_cache,                                OopMapCache*)                          \
  nonstatic_field(InstanceKlass,               _jni_ids,                                      JNIid*)                                \
  nonstatic_field(InstanceKlass,               _osr_nmethods_head,                            nmethod*)                              \
  nonstatic_field(InstanceKlass,               _breakpoints,                                  BreakpointInfo*)                       \
  nonstatic_field(InstanceKlass,               _generic_signature_index,                      u2)                                    \
  nonstatic_field(InstanceKlass,               _methods_jmethod_ids,                          jmethodID*)                            \
  volatile_nonstatic_field(InstanceKlass,      _idnum_allocated_count,                        u2)                                    \
  nonstatic_field(InstanceKlass,               _annotations,                                  Annotations*)                          \
  nonstatic_field(InstanceKlass,               _dependencies,                                 nmethodBucket*)                        \
  nonstatic_field(nmethodBucket,               _nmethod,                                      nmethod*)                              \
  nonstatic_field(nmethodBucket,               _count,                                        int)                                   \
  nonstatic_field(nmethodBucket,               _next,                                         nmethodBucket*)                        \
  nonstatic_field(InstanceKlass,               _method_ordering,                              Array<int>*)                           \
  nonstatic_field(InstanceKlass,               _default_vtable_indices,                       Array<int>*)                           \
  nonstatic_field(Klass,                       _super_check_offset,                           juint)                                 \
  nonstatic_field(Klass,                       _secondary_super_cache,                        Klass*)                                \
  nonstatic_field(Klass,                       _secondary_supers,                             Array<Klass*>*)                        \
  nonstatic_field(Klass,                       _primary_supers[0],                            Klass*)                                \
  nonstatic_field(Klass,                       _java_mirror,                                  oop)                                   \
  nonstatic_field(Klass,                       _modifier_flags,                               jint)                                  \
  nonstatic_field(Klass,                       _super,                                        Klass*)                                \
  nonstatic_field(Klass,                       _subklass,                                     Klass*)                                \
  nonstatic_field(Klass,                       _layout_helper,                                jint)                                  \
  nonstatic_field(Klass,                       _name,                                         Symbol*)                               \
  nonstatic_field(Klass,                       _access_flags,                                 AccessFlags)                           \
  nonstatic_field(Klass,                       _prototype_header,                             markOop)                               \
  nonstatic_field(Klass,                       _next_sibling,                                 Klass*)                                \
  nonstatic_field(vtableEntry,                 _method,                                       Method*)                               \
  nonstatic_field(MethodData,           _size,                                         int)                                   \
  nonstatic_field(MethodData,           _method,                                       Method*)                               \
  nonstatic_field(MethodData,           _data_size,                                    int)                                   \
  nonstatic_field(MethodData,           _data[0],                                      intptr_t)                              \
  nonstatic_field(MethodData,           _nof_decompiles,                               uint)                                  \
  nonstatic_field(MethodData,           _nof_overflow_recompiles,                      uint)                                  \
  nonstatic_field(MethodData,           _nof_overflow_traps,                           uint)                                  \
  nonstatic_field(MethodData,           _trap_hist._array[0],                          u1)                                    \
  nonstatic_field(MethodData,           _eflags,                                       intx)                                  \
  nonstatic_field(MethodData,           _arg_local,                                    intx)                                  \
  nonstatic_field(MethodData,           _arg_stack,                                    intx)                                  \
  nonstatic_field(MethodData,           _arg_returned,                                 intx)                                  \
  nonstatic_field(DataLayout,           _header._struct._tag,                          u1)                                    \
  nonstatic_field(DataLayout,           _header._struct._flags,                        u1)                                    \
  nonstatic_field(DataLayout,           _header._struct._bci,                          u2)                                    \
  nonstatic_field(DataLayout,           _cells[0],                                     intptr_t)                              \
  nonstatic_field(MethodCounters,       _interpreter_invocation_count,                 int)                                   \
  nonstatic_field(MethodCounters,       _interpreter_throwout_count,                   u2)                                    \
  nonstatic_field(MethodCounters,       _number_of_breakpoints,                        u2)                                    \
  nonstatic_field(MethodCounters,       _invocation_counter,                           InvocationCounter)                     \
  nonstatic_field(MethodCounters,       _backedge_counter,                             InvocationCounter)                     \
  nonstatic_field(Method,               _constMethod,                                  ConstMethod*)                          \
  nonstatic_field(Method,               _method_data,                                  MethodData*)                           \
  nonstatic_field(Method,               _method_counters,                              MethodCounters*)                       \
  nonstatic_field(Method,               _access_flags,                                 AccessFlags)                           \
  nonstatic_field(Method,               _vtable_index,                                 int)                                   \
  nonstatic_field(Method,               _method_size,                                  u2)                                    \
  nonstatic_field(Method,               _intrinsic_id,                                 u1)                                    \
  nonproduct_nonstatic_field(Method,    _compiled_invocation_count,                    int)                                   \
  volatile_nonstatic_field(Method,      _code,                                         nmethod*)                              \
  nonstatic_field(Method,               _i2i_entry,                                    address)                               \
  nonstatic_field(Method,               _adapter,                                      AdapterHandlerEntry*)                  \
  volatile_nonstatic_field(Method,      _from_compiled_entry,                          address)                               \
  volatile_nonstatic_field(Method,      _from_interpreted_entry,                       address)                               \
  volatile_nonstatic_field(ConstMethod, _fingerprint,                                  uint64_t)                              \
  nonstatic_field(ConstMethod,          _constants,                                    ConstantPool*)                         \
  nonstatic_field(ConstMethod,          _stackmap_data,                                Array<u1>*)                            \
  nonstatic_field(ConstMethod,          _constMethod_size,                             int)                                   \
  nonstatic_field(ConstMethod,          _flags,                                        u2)                                    \
  nonstatic_field(ConstMethod,          _code_size,                                    u2)                                    \
  nonstatic_field(ConstMethod,          _name_index,                                   u2)                                    \
  nonstatic_field(ConstMethod,          _signature_index,                              u2)                                    \
  nonstatic_field(ConstMethod,          _method_idnum,                                 u2)                                    \
  nonstatic_field(ConstMethod,          _max_stack,                                    u2)                                    \
  nonstatic_field(ConstMethod,          _max_locals,                                   u2)                                    \
  nonstatic_field(ConstMethod,          _size_of_parameters,                           u2)                                    \
  nonstatic_field(ObjArrayKlass,               _element_klass,                                Klass*)                                \
  nonstatic_field(ObjArrayKlass,               _bottom_klass,                                 Klass*)                                \
  volatile_nonstatic_field(Symbol,             _refcount,                                     short)                                 \
  nonstatic_field(Symbol,                      _identity_hash,                                int)                                   \
  nonstatic_field(Symbol,                      _length,                                       unsigned short)                        \
  unchecked_nonstatic_field(Symbol,            _body,                                         sizeof(jbyte)) /* NOTE: no type */     \
  nonstatic_field(TypeArrayKlass,              _max_length,                                   int)                                   \
                                                                                                                                     \
  /***********************/                                                                                                          \
  /* Constant Pool Cache */                                                                                                          \
  /***********************/                                                                                                          \
                                                                                                                                     \
  volatile_nonstatic_field(ConstantPoolCacheEntry,      _indices,                                      intx)                         \
  nonstatic_field(ConstantPoolCacheEntry,               _f1,                                           volatile Metadata*)           \
  volatile_nonstatic_field(ConstantPoolCacheEntry,      _f2,                                           intx)                         \
  volatile_nonstatic_field(ConstantPoolCacheEntry,      _flags,                                        intx)                         \
                                                                                                                                     \
  /********************************/                                                                                                 \
  /* MethodOop-related structures */                                                                                                 \
  /********************************/                                                                                                 \
                                                                                                                                     \
  nonstatic_field(CheckedExceptionElement,     class_cp_index,                                u2)                                    \
  nonstatic_field(LocalVariableTableElement,   start_bci,                                     u2)                                    \
  nonstatic_field(LocalVariableTableElement,   length,                                        u2)                                    \
  nonstatic_field(LocalVariableTableElement,   name_cp_index,                                 u2)                                    \
  nonstatic_field(LocalVariableTableElement,   descriptor_cp_index,                           u2)                                    \
  nonstatic_field(LocalVariableTableElement,   signature_cp_index,                            u2)                                    \
  nonstatic_field(LocalVariableTableElement,   slot,                                          u2)                                    \
  nonstatic_field(ExceptionTableElement,       start_pc,                                      u2)                                    \
  nonstatic_field(ExceptionTableElement,       end_pc,                                        u2)                                    \
  nonstatic_field(ExceptionTableElement,       handler_pc,                                    u2)                                    \
  nonstatic_field(ExceptionTableElement,       catch_type_index,                              u2)                                    \
  nonstatic_field(BreakpointInfo,              _orig_bytecode,                                Bytecodes::Code)                       \
  nonstatic_field(BreakpointInfo,              _bci,                                          int)                                   \
  nonstatic_field(BreakpointInfo,              _name_index,                                   u2)                                    \
  nonstatic_field(BreakpointInfo,              _signature_index,                              u2)                                    \
  nonstatic_field(BreakpointInfo,              _next,                                         BreakpointInfo*)                       \
  /***********/                                                                                                                      \
  /* JNI IDs */                                                                                                                      \
  /***********/                                                                                                                      \
                                                                                                                                     \
  nonstatic_field(JNIid,                       _holder,                                       Klass*)                                \
  nonstatic_field(JNIid,                       _next,                                         JNIid*)                                \
  nonstatic_field(JNIid,                       _offset,                                       int)                                   \
  /************/                                                                                                                     \
  /* Universe */                                                                                                                     \
  /************/                                                                                                                     \
                                                                                                                                     \
     static_field(Universe,                    _boolArrayKlassObj,                            Klass*)                                \
     static_field(Universe,                    _byteArrayKlassObj,                            Klass*)                                \
     static_field(Universe,                    _charArrayKlassObj,                            Klass*)                                \
     static_field(Universe,                    _intArrayKlassObj,                             Klass*)                                \
     static_field(Universe,                    _shortArrayKlassObj,                           Klass*)                                \
     static_field(Universe,                    _longArrayKlassObj,                            Klass*)                                \
     static_field(Universe,                    _singleArrayKlassObj,                          Klass*)                                \
     static_field(Universe,                    _doubleArrayKlassObj,                          Klass*)                                \
     static_field(Universe,                    _mirrors[0],                                   oop)                                   \
     static_field(Universe,                    _main_thread_group,                            oop)                                   \
     static_field(Universe,                    _system_thread_group,                          oop)                                   \
     static_field(Universe,                    _the_empty_class_klass_array,                  objArrayOop)                           \
     static_field(Universe,                    _null_ptr_exception_instance,                  oop)                                   \
     static_field(Universe,                    _arithmetic_exception_instance,                oop)                                   \
     static_field(Universe,                    _vm_exception,                                 oop)                                   \
     static_field(Universe,                    _collectedHeap,                                CollectedHeap*)                        \
     static_field(Universe,                    _base_vtable_size,                             int)                                   \
     static_field(Universe,                    _bootstrapping,                                bool)                                  \
     static_field(Universe,                    _fully_initialized,                            bool)                                  \
     static_field(Universe,                    _verify_count,                                 int)                                   \
     static_field(Universe,                    _non_oop_bits,                                 intptr_t)                              \
     static_field(Universe,                    _narrow_oop._base,                             address)                               \
     static_field(Universe,                    _narrow_oop._shift,                            int)                                   \
     static_field(Universe,                    _narrow_oop._use_implicit_null_checks,         bool)                                  \
     static_field(Universe,                    _narrow_klass._base,                           address)                               \
     static_field(Universe,                    _narrow_klass._shift,                          int)                                   \
                                                                                                                                     \
  /******/                                                                                                                           \
  /* os */                                                                                                                           \
  /******/                                                                                                                           \
                                                                                                                                     \
     static_field(os,                          _polling_page,                                 address)                               \
                                                                                                                                     \
  /**********************************************************************************/                                               \
  /* Generation and Space hierarchies                                               */                                               \
  /**********************************************************************************/                                               \
                                                                                                                                     \
  unchecked_nonstatic_field(ageTable,          sizes,                                         sizeof(ageTable::sizes))               \
                                                                                                                                     \
  nonstatic_field(BarrierSet,                  _max_covered_regions,                          int)                                   \
  nonstatic_field(BarrierSet,                  _kind,                                         BarrierSet::Name)                      \
  nonstatic_field(BlockOffsetTable,            _bottom,                                       HeapWord*)                             \
  nonstatic_field(BlockOffsetTable,            _end,                                          HeapWord*)                             \
                                                                                                                                     \
  nonstatic_field(BlockOffsetSharedArray,      _reserved,                                     MemRegion)                             \
  nonstatic_field(BlockOffsetSharedArray,      _end,                                          HeapWord*)                             \
  nonstatic_field(BlockOffsetSharedArray,      _vs,                                           VirtualSpace)                          \
  nonstatic_field(BlockOffsetSharedArray,      _offset_array,                                 u_char*)                               \
                                                                                                                                     \
  nonstatic_field(BlockOffsetArray,            _array,                                        BlockOffsetSharedArray*)               \
  nonstatic_field(BlockOffsetArray,            _sp,                                           Space*)                                \
  nonstatic_field(BlockOffsetArrayContigSpace, _next_offset_threshold,                        HeapWord*)                             \
  nonstatic_field(BlockOffsetArrayContigSpace, _next_offset_index,                            size_t)                                \
                                                                                                                                     \
  nonstatic_field(BlockOffsetArrayNonContigSpace, _unallocated_block,                         HeapWord*)                             \
                                                                                                                                     \
  nonstatic_field(CardGeneration,              _rs,                                           GenRemSet*)                            \
  nonstatic_field(CardGeneration,              _bts,                                          BlockOffsetSharedArray*)               \
  nonstatic_field(CardGeneration,              _shrink_factor,                                size_t)                                \
  nonstatic_field(CardGeneration,              _capacity_at_prologue,                         size_t)                                \
  nonstatic_field(CardGeneration,              _used_at_prologue,                             size_t)                                \
                                                                                                                                     \
  nonstatic_field(CardTableModRefBS,           _whole_heap,                                   const MemRegion)                       \
  nonstatic_field(CardTableModRefBS,           _guard_index,                                  const size_t)                          \
  nonstatic_field(CardTableModRefBS,           _last_valid_index,                             const size_t)                          \
  nonstatic_field(CardTableModRefBS,           _page_size,                                    const size_t)                          \
  nonstatic_field(CardTableModRefBS,           _byte_map_size,                                const size_t)                          \
  nonstatic_field(CardTableModRefBS,           _byte_map,                                     jbyte*)                                \
  nonstatic_field(CardTableModRefBS,           _cur_covered_regions,                          int)                                   \
  nonstatic_field(CardTableModRefBS,           _covered,                                      MemRegion*)                            \
  nonstatic_field(CardTableModRefBS,           _committed,                                    MemRegion*)                            \
  nonstatic_field(CardTableModRefBS,           _guard_region,                                 MemRegion)                             \
  nonstatic_field(CardTableModRefBS,           byte_map_base,                                 jbyte*)                                \
                                                                                                                                     \
  nonstatic_field(CardTableRS,                 _ct_bs,                                        CardTableModRefBSForCTRS*)             \
                                                                                                                                     \
  nonstatic_field(CollectedHeap,               _reserved,                                     MemRegion)                             \
  nonstatic_field(CollectedHeap,               _barrier_set,                                  BarrierSet*)                           \
  nonstatic_field(CollectedHeap,               _defer_initial_card_mark,                      bool)                                  \
  nonstatic_field(CollectedHeap,               _is_gc_active,                                 bool)                                  \
  nonstatic_field(CollectedHeap,               _total_collections,                            unsigned int)                          \
  nonstatic_field(CompactibleSpace,            _compaction_top,                               HeapWord*)                             \
  nonstatic_field(CompactibleSpace,            _first_dead,                                   HeapWord*)                             \
  nonstatic_field(CompactibleSpace,            _end_of_live,                                  HeapWord*)                             \
                                                                                                                                     \
                                                                                                                                     \
  nonstatic_field(ContiguousSpace,             _top,                                          HeapWord*)                             \
  nonstatic_field(ContiguousSpace,             _concurrent_iteration_safe_limit,              HeapWord*)                             \
  nonstatic_field(ContiguousSpace,             _saved_mark_word,                              HeapWord*)                             \
                                                                                                                                     \
  nonstatic_field(DefNewGeneration,            _next_gen,                                     Generation*)                           \
  nonstatic_field(DefNewGeneration,            _tenuring_threshold,                           uint)                                  \
  nonstatic_field(DefNewGeneration,            _age_table,                                    ageTable)                              \
  nonstatic_field(DefNewGeneration,            _eden_space,                                   EdenSpace*)                            \
  nonstatic_field(DefNewGeneration,            _from_space,                                   ContiguousSpace*)                      \
  nonstatic_field(DefNewGeneration,            _to_space,                                     ContiguousSpace*)                      \
                                                                                                                                     \
  nonstatic_field(EdenSpace,                   _gen,                                          DefNewGeneration*)                     \
                                                                                                                                     \
  nonstatic_field(Generation,                  _reserved,                                     MemRegion)                             \
  nonstatic_field(Generation,                  _virtual_space,                                VirtualSpace)                          \
  nonstatic_field(Generation,                  _level,                                        int)                                   \
  nonstatic_field(Generation,                  _stat_record,                                  Generation::StatRecord)                \
                                                                                                                                     \
  nonstatic_field(Generation::StatRecord,      invocations,                                   int)                                   \
  nonstatic_field(Generation::StatRecord,      accumulated_time,                              elapsedTimer)                          \
                                                                                                                                     \
  nonstatic_field(GenerationSpec,              _name,                                         Generation::Name)                      \
  nonstatic_field(GenerationSpec,              _init_size,                                    size_t)                                \
  nonstatic_field(GenerationSpec,              _max_size,                                     size_t)                                \
                                                                                                                                     \
    static_field(GenCollectedHeap,             _gch,                                          GenCollectedHeap*)                     \
 nonstatic_field(GenCollectedHeap,             _n_gens,                                       int)                                   \
 unchecked_nonstatic_field(GenCollectedHeap,   _gens,                                         sizeof(GenCollectedHeap::_gens)) /* NOTE: no type */ \
  nonstatic_field(GenCollectedHeap,            _gen_specs,                                    GenerationSpec**)                      \
                                                                                                                                     \
  nonstatic_field(HeapWord,                    i,                                             char*)                                 \
                                                                                                                                     \
  nonstatic_field(MemRegion,                   _start,                                        HeapWord*)                             \
  nonstatic_field(MemRegion,                   _word_size,                                    size_t)                                \
                                                                                                                                     \
  nonstatic_field(OffsetTableContigSpace,      _offsets,                                      BlockOffsetArray)                      \
                                                                                                                                     \
  nonstatic_field(OneContigSpaceCardGeneration, _min_heap_delta_bytes,                        size_t)                                \
  nonstatic_field(OneContigSpaceCardGeneration, _the_space,                                   ContiguousSpace*)                      \
  nonstatic_field(OneContigSpaceCardGeneration, _last_gc,                                     WaterMark)                             \
                                                                                                                                     \
                                                                                                                                     \
                                                                                                                                     \
  nonstatic_field(Space,                       _bottom,                                       HeapWord*)                             \
  nonstatic_field(Space,                       _end,                                          HeapWord*)                             \
                                                                                                                                     \
  nonstatic_field(ThreadLocalAllocBuffer,      _start,                                        HeapWord*)                             \
  nonstatic_field(ThreadLocalAllocBuffer,      _top,                                          HeapWord*)                             \
  nonstatic_field(ThreadLocalAllocBuffer,      _end,                                          HeapWord*)                             \
  nonstatic_field(ThreadLocalAllocBuffer,      _desired_size,                                 size_t)                                \
  nonstatic_field(ThreadLocalAllocBuffer,      _refill_waste_limit,                           size_t)                                \
     static_field(ThreadLocalAllocBuffer,      _target_refills,                               unsigned)                              \
  nonstatic_field(ThreadLocalAllocBuffer,      _number_of_refills,                            unsigned)                              \
  nonstatic_field(ThreadLocalAllocBuffer,      _fast_refill_waste,                            unsigned)                              \
  nonstatic_field(ThreadLocalAllocBuffer,      _slow_refill_waste,                            unsigned)                              \
  nonstatic_field(ThreadLocalAllocBuffer,      _gc_waste,                                     unsigned)                              \
  nonstatic_field(ThreadLocalAllocBuffer,      _slow_allocations,                             unsigned)                              \
  nonstatic_field(VirtualSpace,                _low_boundary,                                 char*)                                 \
  nonstatic_field(VirtualSpace,                _high_boundary,                                char*)                                 \
  nonstatic_field(VirtualSpace,                _low,                                          char*)                                 \
  nonstatic_field(VirtualSpace,                _high,                                         char*)                                 \
  nonstatic_field(VirtualSpace,                _lower_high,                                   char*)                                 \
  nonstatic_field(VirtualSpace,                _middle_high,                                  char*)                                 \
  nonstatic_field(VirtualSpace,                _upper_high,                                   char*)                                 \
  nonstatic_field(WaterMark,                   _point,                                        HeapWord*)                             \
  nonstatic_field(WaterMark,                   _space,                                        Space*)                                \
                                                                                                                                     \
  /************************/                                                                                                         \
  /* PerfMemory - jvmstat */                                                                                                         \
  /************************/                                                                                                         \
                                                                                                                                     \
  nonstatic_field(PerfDataPrologue,            magic,                                         jint)                                  \
  nonstatic_field(PerfDataPrologue,            byte_order,                                    jbyte)                                 \
  nonstatic_field(PerfDataPrologue,            major_version,                                 jbyte)                                 \
  nonstatic_field(PerfDataPrologue,            minor_version,                                 jbyte)                                 \
  nonstatic_field(PerfDataPrologue,            accessible,                                    jbyte)                                 \
  nonstatic_field(PerfDataPrologue,            used,                                          jint)                                  \
  nonstatic_field(PerfDataPrologue,            overflow,                                      jint)                                  \
  nonstatic_field(PerfDataPrologue,            mod_time_stamp,                                jlong)                                 \
  nonstatic_field(PerfDataPrologue,            entry_offset,                                  jint)                                  \
  nonstatic_field(PerfDataPrologue,            num_entries,                                   jint)                                  \
                                                                                                                                     \
  nonstatic_field(PerfDataEntry,               entry_length,                                  jint)                                  \
  nonstatic_field(PerfDataEntry,               name_offset,                                   jint)                                  \
  nonstatic_field(PerfDataEntry,               vector_length,                                 jint)                                  \
  nonstatic_field(PerfDataEntry,               data_type,                                     jbyte)                                 \
  nonstatic_field(PerfDataEntry,               flags,                                         jbyte)                                 \
  nonstatic_field(PerfDataEntry,               data_units,                                    jbyte)                                 \
  nonstatic_field(PerfDataEntry,               data_variability,                              jbyte)                                 \
  nonstatic_field(PerfDataEntry,               data_offset,                                   jint)                                  \
                                                                                                                                     \
     static_field(PerfMemory,                  _start,                                        char*)                                 \
     static_field(PerfMemory,                  _end,                                          char*)                                 \
     static_field(PerfMemory,                  _top,                                          char*)                                 \
     static_field(PerfMemory,                  _capacity,                                     size_t)                                \
     static_field(PerfMemory,                  _prologue,                                     PerfDataPrologue*)                     \
     static_field(PerfMemory,                  _initialized,                                  jint)                                  \
                                                                                                                                     \
  /***************/                                                                                                                  \
  /* SymbolTable */                                                                                                                  \
  /***************/                                                                                                                  \
                                                                                                                                     \
     static_field(SymbolTable,                  _the_table,                                   SymbolTable*)                          \
                                                                                                                                     \
  /***************/                                                                                                                  \
  /* StringTable */                                                                                                                  \
  /***************/                                                                                                                  \
                                                                                                                                     \
     static_field(StringTable,                  _the_table,                                   StringTable*)                          \
                                                                                                                                     \
  /********************/                                                                                                             \
  /* SystemDictionary */                                                                                                             \
  /********************/                                                                                                             \
                                                                                                                                     \
      static_field(SystemDictionary,            _dictionary,                                   Dictionary*)                          \
      static_field(SystemDictionary,            _placeholders,                                 PlaceholderTable*)                    \
      static_field(SystemDictionary,            _shared_dictionary,                            Dictionary*)                          \
      static_field(SystemDictionary,            _system_loader_lock_obj,                       oop)                                  \
      static_field(SystemDictionary,            _loader_constraints,                           LoaderConstraintTable*)               \
      static_field(SystemDictionary,            WK_KLASS(Object_klass),                        Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(String_klass),                        Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Class_klass),                         Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Cloneable_klass),                     Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(ClassLoader_klass),                   Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Serializable_klass),                  Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(System_klass),                        Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Throwable_klass),                     Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(ThreadDeath_klass),                   Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Error_klass),                         Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Exception_klass),                     Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(RuntimeException_klass),              Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(ClassNotFoundException_klass),        Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(NoClassDefFoundError_klass),          Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(LinkageError_klass),                  Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(ClassCastException_klass),            Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(ArrayStoreException_klass),           Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(VirtualMachineError_klass),           Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(OutOfMemoryError_klass),              Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(StackOverflowError_klass),            Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(ProtectionDomain_klass),              Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(AccessControlContext_klass),          Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(SecureClassLoader_klass),             Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Reference_klass),                     Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(SoftReference_klass),                 Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(WeakReference_klass),                 Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(FinalReference_klass),                Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(PhantomReference_klass),              Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Finalizer_klass),                     Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Thread_klass),                        Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(ThreadGroup_klass),                   Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(Properties_klass),                    Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(StringBuffer_klass),                  Klass*)                               \
      static_field(SystemDictionary,            WK_KLASS(MethodHandle_klass),                  Klass*)                               \
      static_field(SystemDictionary,            _box_klasses[0],                               Klass*)                               \
      static_field(SystemDictionary,            _java_system_loader,                           oop)                                  \
                                                                                                                                     \
  /*************/                                                                                                                    \
  /* vmSymbols */                                                                                                                    \
  /*************/                                                                                                                    \
                                                                                                                                     \
      static_field(vmSymbols,                   _symbols[0],                                  Symbol*)                               \
                                                                                                                                     \
  /*******************/                                                                                                              \
  /* HashtableBucket */                                                                                                              \
  /*******************/                                                                                                              \
                                                                                                                                     \
  nonstatic_field(HashtableBucket<mtInternal>,  _entry,                                        BasicHashtableEntry<mtInternal>*)     \
                                                                                                                                     \
  /******************/                                                                                                               \
  /* HashtableEntry */                                                                                                               \
  /******************/                                                                                                               \
                                                                                                                                     \
  nonstatic_field(BasicHashtableEntry<mtInternal>, _next,                                     BasicHashtableEntry<mtInternal>*)      \
  nonstatic_field(BasicHashtableEntry<mtInternal>, _hash,                                     unsigned int)                          \
  nonstatic_field(IntptrHashtableEntry,            _literal,                                  intptr_t)                              \
                                                                                                                                     \
  /*************/                                                                                                                    \
  /* Hashtable */                                                                                                                    \
  /*************/                                                                                                                    \
                                                                                                                                     \
  nonstatic_field(BasicHashtable<mtInternal>, _table_size,                                   int)                                   \
  nonstatic_field(BasicHashtable<mtInternal>, _buckets,                                      HashtableBucket<mtInternal>*)          \
  volatile_nonstatic_field(BasicHashtable<mtInternal>,  _free_list,                          BasicHashtableEntry<mtInternal>*)      \
  nonstatic_field(BasicHashtable<mtInternal>, _first_free_entry,                             char*)                                 \
  nonstatic_field(BasicHashtable<mtInternal>, _end_block,                                    char*)                                 \
  nonstatic_field(BasicHashtable<mtInternal>, _entry_size,                                   int)                                   \
                                                                                                                                     \
  /*******************/                                                                                                              \
  /* DictionaryEntry */                                                                                                              \
  /*******************/                                                                                                              \
                                                                                                                                     \
  nonstatic_field(DictionaryEntry,             _loader_data,                                  ClassLoaderData*)                      \
  nonstatic_field(DictionaryEntry,             _pd_set,                                       ProtectionDomainEntry*)                \
                                                                                                                                     \
  /********************/                                                                                                             \
                                                                                                                                     \
  nonstatic_field(PlaceholderEntry,            _loader_data,                                  ClassLoaderData*)                      \
                                                                                                                                     \
  /**************************/                                                                                                       \
  /* ProtectionDomainEntry  */                                                                                                       \
  /**************************/                                                                                                       \
                                                                                                                                     \
  nonstatic_field(ProtectionDomainEntry,       _next,                                         ProtectionDomainEntry*)                \
  nonstatic_field(ProtectionDomainEntry,       _pd_cache,                                     ProtectionDomainCacheEntry*)           \
                                                                                                                                     \
  /*******************************/                                                                                                  \
  /* ProtectionDomainCacheEntry  */                                                                                                  \
  /*******************************/                                                                                                  \
                                                                                                                                     \
  nonstatic_field(ProtectionDomainCacheEntry,  _literal,                                      oop)                                   \
                                                                                                                                     \
  /*************************/                                                                                                        \
  /* LoaderConstraintEntry */                                                                                                        \
  /*************************/                                                                                                        \
                                                                                                                                     \
  nonstatic_field(LoaderConstraintEntry,       _name,                                         Symbol*)                               \
  nonstatic_field(LoaderConstraintEntry,       _num_loaders,                                  int)                                   \
  nonstatic_field(LoaderConstraintEntry,       _max_loaders,                                  int)                                   \
  nonstatic_field(LoaderConstraintEntry,       _loaders,                                      ClassLoaderData**)                     \
                                                                                                                                     \
  nonstatic_field(ClassLoaderData,             _class_loader,                                 oop)                                   \
  nonstatic_field(ClassLoaderData,             _next,                                         ClassLoaderData*)                      \
                                                                                                                                     \
  static_field(ClassLoaderDataGraph,           _head,                                         ClassLoaderData*)                      \
                                                                                                                                     \
  /**********/                                                                                                                       \
  /* Arrays */                                                                                                                       \
  /**********/                                                                                                                       \
                                                                                                                                     \
  nonstatic_field(Array<Klass*>,               _length,                                       int)                                   \
  nonstatic_field(Array<Klass*>,               _data[0],                                      Klass*)                                \
                                                                                                                                     \
  /*******************/                                                                                                              \
  /* GrowableArrays  */                                                                                                              \
  /*******************/                                                                                                              \
                                                                                                                                     \
  nonstatic_field(GenericGrowableArray,        _len,                                          int)                                   \
  nonstatic_field(GenericGrowableArray,        _max,                                          int)                                   \
  nonstatic_field(GenericGrowableArray,        _arena,                                        Arena*)                                \
  nonstatic_field(GrowableArray<int>,          _data,                                         int*)                                  \
                                                                                                                                     \
  /********************************/                                                                                                 \
  /* CodeCache (NOTE: incomplete) */                                                                                                 \
  /********************************/                                                                                                 \
                                                                                                                                     \
     static_field(CodeCache,                   _heap,                                         CodeHeap*)                             \
     static_field(CodeCache,                   _scavenge_root_nmethods,                       nmethod*)                              \
                                                                                                                                     \
  /*******************************/                                                                                                  \
  /* CodeHeap (NOTE: incomplete) */                                                                                                  \
  /*******************************/                                                                                                  \
                                                                                                                                     \
  nonstatic_field(CodeHeap,                    _memory,                                       VirtualSpace)                          \
  nonstatic_field(CodeHeap,                    _segmap,                                       VirtualSpace)                          \
  nonstatic_field(CodeHeap,                    _log2_segment_size,                            int)                                   \
  nonstatic_field(HeapBlock,                   _header,                                       HeapBlock::Header)                     \
  nonstatic_field(HeapBlock::Header,           _length,                                       size_t)                                \
  nonstatic_field(HeapBlock::Header,           _used,                                         bool)                                  \
                                                                                                                                     \
  /**********************************/                                                                                               \
  /* Interpreter (NOTE: incomplete) */                                                                                               \
  /**********************************/                                                                                               \
                                                                                                                                     \
     static_field(AbstractInterpreter,         _code,                                         StubQueue*)                            \
                                                                                                                                     \
  /****************************/                                                                                                     \
  /* Stubs (NOTE: incomplete) */                                                                                                     \
  /****************************/                                                                                                     \
                                                                                                                                     \
  nonstatic_field(StubQueue,                   _stub_buffer,                                  address)                               \
  nonstatic_field(StubQueue,                   _buffer_limit,                                 int)                                   \
  nonstatic_field(StubQueue,                   _queue_begin,                                  int)                                   \
  nonstatic_field(StubQueue,                   _queue_end,                                    int)                                   \
  nonstatic_field(StubQueue,                   _number_of_stubs,                              int)                                   \
  nonstatic_field(InterpreterCodelet,          _size,                                         int)                                   \
  nonstatic_field(InterpreterCodelet,          _description,                                  const char*)                           \
  nonstatic_field(InterpreterCodelet,          _bytecode,                                     Bytecodes::Code)                       \
                                                                                                                                     \
  /***********************************/                                                                                              \
  /* StubRoutines (NOTE: incomplete) */                                                                                              \
  /***********************************/                                                                                              \
                                                                                                                                     \
     static_field(StubRoutines,                _verify_oop_count,                             jint)                                  \
     static_field(StubRoutines,                _call_stub_return_address,                     address)                               \
     static_field(StubRoutines,                _aescrypt_encryptBlock,                        address)                               \
     static_field(StubRoutines,                _aescrypt_decryptBlock,                        address)                               \
     static_field(StubRoutines,                _cipherBlockChaining_encryptAESCrypt,          address)                               \
     static_field(StubRoutines,                _cipherBlockChaining_decryptAESCrypt,          address)                               \
     static_field(StubRoutines,                _ghash_processBlocks,                          address)                               \
     static_field(StubRoutines,                _updateBytesCRC32,                             address)                               \
     static_field(StubRoutines,                _crc_table_adr,                                address)                               \
     static_field(StubRoutines,                _multiplyToLen,                                address)                               \
     static_field(StubRoutines,                _squareToLen,                                  address)                               \
     static_field(StubRoutines,                _mulAdd,                                       address)                               \
                                                                                                                                     \
  /*****************/                                                                                                                \
  /* SharedRuntime */                                                                                                                \
  /*****************/                                                                                                                \
                                                                                                                                     \
     static_field(SharedRuntime,               _ic_miss_blob,                                 RuntimeStub*)                          \
                                                                                                                                     \
  /***************************************/                                                                                          \
  /* PcDesc and other compiled code info */                                                                                          \
  /***************************************/                                                                                          \
                                                                                                                                     \
  nonstatic_field(PcDesc,                      _pc_offset,                                    int)                                   \
  nonstatic_field(PcDesc,                      _scope_decode_offset,                          int)                                   \
  nonstatic_field(PcDesc,                      _obj_decode_offset,                            int)                                   \
  nonstatic_field(PcDesc,                      _flags,                                        int)                                   \
                                                                                                                                     \
  /***************************************************/                                                                              \
  /* CodeBlobs (NOTE: incomplete, but only a little) */                                                                              \
  /***************************************************/                                                                              \
                                                                                                                                     \
  nonstatic_field(CodeBlob,                    _name,                                         const char*)                           \
  nonstatic_field(CodeBlob,                    _size,                                         int)                                   \
  nonstatic_field(CodeBlob,                    _header_size,                                  int)                                   \
  nonstatic_field(CodeBlob,                    _relocation_size,                              int)                                   \
  nonstatic_field(CodeBlob,                    _content_offset,                               int)                                   \
  nonstatic_field(CodeBlob,                    _code_offset,                                  int)                                   \
  nonstatic_field(CodeBlob,                    _frame_complete_offset,                        int)                                   \
  nonstatic_field(CodeBlob,                    _data_offset,                                  int)                                   \
  nonstatic_field(CodeBlob,                    _frame_size,                                   int)                                   \
  nonstatic_field(CodeBlob,                    _oop_maps,                                     OopMapSet*)                            \
                                                                                                                                     \
  nonstatic_field(RuntimeStub,                 _caller_must_gc_arguments,                     bool)                                  \
                                                                                                                                     \
  /**************************************************/                                                                               \
  /* NMethods (NOTE: incomplete, but only a little) */                                                                               \
  /**************************************************/                                                                               \
                                                                                                                                     \
  nonstatic_field(nmethod,             _method,                                       Method*)                        \
  nonstatic_field(nmethod,             _entry_bci,                                    int)                                   \
  nonstatic_field(nmethod,             _osr_link,                                     nmethod*)                              \
  nonstatic_field(nmethod,             _scavenge_root_link,                           nmethod*)                              \
  nonstatic_field(nmethod,             _scavenge_root_state,                          jbyte)                                 \
  nonstatic_field(nmethod,             _state,                                        volatile unsigned char)                \
  nonstatic_field(nmethod,             _exception_offset,                             int)                                   \
  nonstatic_field(nmethod,             _deoptimize_offset,                            int)                                   \
  nonstatic_field(nmethod,             _deoptimize_mh_offset,                         int)                                   \
  nonstatic_field(nmethod,             _orig_pc_offset,                               int)                                   \
  nonstatic_field(nmethod,             _stub_offset,                                  int)                                   \
  nonstatic_field(nmethod,             _consts_offset,                                int)                                   \
  nonstatic_field(nmethod,             _oops_offset,                                  int)                                   \
  nonstatic_field(nmethod,             _metadata_offset,                              int)                                   \
  nonstatic_field(nmethod,             _scopes_data_offset,                           int)                                   \
  nonstatic_field(nmethod,             _scopes_pcs_offset,                            int)                                   \
  nonstatic_field(nmethod,             _dependencies_offset,                          int)                                   \
  nonstatic_field(nmethod,             _handler_table_offset,                         int)                                   \
  nonstatic_field(nmethod,             _nul_chk_table_offset,                         int)                                   \
  nonstatic_field(nmethod,             _nmethod_end_offset,                           int)                                   \
  nonstatic_field(nmethod,             _entry_point,                                  address)                               \
  nonstatic_field(nmethod,             _verified_entry_point,                         address)                               \
  nonstatic_field(nmethod,             _osr_entry_point,                              address)                               \
  nonstatic_field(nmethod,             _lock_count,                                   jint)                                  \
  nonstatic_field(nmethod,             _stack_traversal_mark,                         long)                                  \
  nonstatic_field(nmethod,             _compile_id,                                   int)                                   \
  nonstatic_field(nmethod,             _comp_level,                                   int)                                   \
  volatile_nonstatic_field(nmethod,    _exception_cache,                              ExceptionCache*)                       \
  nonstatic_field(nmethod,             _marked_for_deoptimization,                    bool)                                  \
                                                                                                                             \
  unchecked_c2_static_field(Deoptimization,         _trap_reason_name,                   void*)                              \
                                                                                                                                     \
  /********************************/                                                                                                 \
  /* JavaCalls (NOTE: incomplete) */                                                                                                 \
  /********************************/                                                                                                 \
                                                                                                                                     \
  nonstatic_field(JavaCallWrapper,             _anchor,                                       JavaFrameAnchor)                       \
  /********************************/                                                                                                 \
  /* JavaFrameAnchor (NOTE: incomplete) */                                                                                           \
  /********************************/                                                                                                 \
  volatile_nonstatic_field(JavaFrameAnchor,    _last_Java_sp,                                 intptr_t*)                             \
  volatile_nonstatic_field(JavaFrameAnchor,    _last_Java_pc,                                 address)                               \
                                                                                                                                     \
  /******************************/                                                                                                   \
  /* Threads (NOTE: incomplete) */                                                                                                   \
  /******************************/                                                                                                   \
                                                                                                                                     \
     static_field(Threads,                     _thread_list,                                  JavaThread*)                           \
     static_field(Threads,                     _number_of_threads,                            int)                                   \
     static_field(Threads,                     _number_of_non_daemon_threads,                 int)                                   \
     static_field(Threads,                     _return_code,                                  int)                                   \
                                                                                                                                     \
  nonstatic_field(ThreadShadow,                _pending_exception,                            oop)                                   \
  nonstatic_field(ThreadShadow,                _exception_file,                               const char*)                           \
  nonstatic_field(ThreadShadow,                _exception_line,                               int)                                   \
   volatile_nonstatic_field(Thread,            _suspend_flags,                                uint32_t)                              \
  nonstatic_field(Thread,                      _active_handles,                               JNIHandleBlock*)                       \
  nonstatic_field(Thread,                      _tlab,                                         ThreadLocalAllocBuffer)                \
  nonstatic_field(Thread,                      _allocated_bytes,                              jlong)                                 \
  nonstatic_field(Thread,                      _current_pending_monitor,                      ObjectMonitor*)                        \
  nonstatic_field(Thread,                      _current_pending_monitor_is_from_java,         bool)                                  \
  nonstatic_field(Thread,                      _current_waiting_monitor,                      ObjectMonitor*)                        \
  nonstatic_field(NamedThread,                 _name,                                         char*)                                 \
  nonstatic_field(NamedThread,                 _processed_thread,                             JavaThread*)                           \
  nonstatic_field(JavaThread,                  _next,                                         JavaThread*)                           \
  nonstatic_field(JavaThread,                  _threadObj,                                    oop)                                   \
  nonstatic_field(JavaThread,                  _anchor,                                       JavaFrameAnchor)                       \
  nonstatic_field(JavaThread,                  _vm_result,                                    oop)                                   \
  nonstatic_field(JavaThread,                  _vm_result_2,                                  Metadata*)                             \
  nonstatic_field(JavaThread,                  _pending_async_exception,                      oop)                                   \
  volatile_nonstatic_field(JavaThread,         _exception_oop,                                oop)                                   \
  volatile_nonstatic_field(JavaThread,         _exception_pc,                                 address)                               \
  volatile_nonstatic_field(JavaThread,         _is_method_handle_return,                      int)                                   \
  nonstatic_field(JavaThread,                  _special_runtime_exit_condition,               JavaThread::AsyncRequests)             \
  nonstatic_field(JavaThread,                  _saved_exception_pc,                           address)                               \
   volatile_nonstatic_field(JavaThread,        _thread_state,                                 JavaThreadState)                       \
  nonstatic_field(JavaThread,                  _osthread,                                     OSThread*)                             \
  nonstatic_field(JavaThread,                  _stack_base,                                   address)                               \
  nonstatic_field(JavaThread,                  _stack_size,                                   size_t)                                \
  nonstatic_field(JavaThread,                  _vframe_array_head,                            vframeArray*)                          \
  nonstatic_field(JavaThread,                  _vframe_array_last,                            vframeArray*)                          \
  nonstatic_field(JavaThread,                  _satb_mark_queue,                              ObjPtrQueue)                           \
  nonstatic_field(JavaThread,                  _dirty_card_queue,                             DirtyCardQueue)                        \
  nonstatic_field(Thread,                      _resource_area,                                ResourceArea*)                         \
  nonstatic_field(CompilerThread,              _env,                                          ciEnv*)                                \
                                                                                                                                     \
  /************/                                                                                                                     \
  /* OSThread */                                                                                                                     \
  /************/                                                                                                                     \
                                                                                                                                     \
  volatile_nonstatic_field(OSThread,           _interrupted,                                  jint)                                  \
                                                                                                                                     \
  /************************/                                                                                                         \
  /* OopMap and OopMapSet */                                                                                                         \
  /************************/                                                                                                         \
                                                                                                                                     \
  nonstatic_field(OopMap,                      _pc_offset,                                    int)                                   \
  nonstatic_field(OopMap,                      _omv_count,                                    int)                                   \
  nonstatic_field(OopMap,                      _omv_data_size,                                int)                                   \
  nonstatic_field(OopMap,                      _omv_data,                                     unsigned char*)                        \
  nonstatic_field(OopMap,                      _write_stream,                                 CompressedWriteStream*)                \
  nonstatic_field(OopMapSet,                   _om_count,                                     int)                                   \
  nonstatic_field(OopMapSet,                   _om_size,                                      int)                                   \
  nonstatic_field(OopMapSet,                   _om_data,                                      OopMap**)                              \
                                                                                                                                     \
  /*********************************/                                                                                                \
  /* JNIHandles and JNIHandleBlock */                                                                                                \
  /*********************************/                                                                                                \
     static_field(JNIHandles,                  _global_handles,                               JNIHandleBlock*)                       \
     static_field(JNIHandles,                  _weak_global_handles,                          JNIHandleBlock*)                       \
     static_field(JNIHandles,                  _deleted_handle,                               oop)                                   \
                                                                                                                                     \
  unchecked_nonstatic_field(JNIHandleBlock,    _handles,                                      JNIHandleBlock::block_size_in_oops * sizeof(Oop)) /* Note: no type */ \
  nonstatic_field(JNIHandleBlock,              _top,                                          int)                                   \
  nonstatic_field(JNIHandleBlock,              _next,                                         JNIHandleBlock*)                       \
                                                                                                                                     \
  /********************/                                                                                                             \
  /* CompressedStream */                                                                                                             \
  /********************/                                                                                                             \
                                                                                                                                     \
  nonstatic_field(CompressedStream,            _buffer,                                       u_char*)                               \
  nonstatic_field(CompressedStream,            _position,                                     int)                                   \
                                                                                                                                     \
  /*********************************/                                                                                                \
  /* VMRegImpl (NOTE: incomplete) */                                                                                                 \
  /*********************************/                                                                                                \
                                                                                                                                     \
     static_field(VMRegImpl,                   regName[0],                                    const char*)                           \
     static_field(VMRegImpl,                   stack0,                                        VMReg)                                 \
                                                                                                                                     \
  /*******************************/                                                                                                  \
  /* Runtime1 (NOTE: incomplete) */                                                                                                  \
  /*******************************/                                                                                                  \
                                                                                                                                     \
  unchecked_c1_static_field(Runtime1,          _blobs,                                 sizeof(Runtime1::_blobs)) /* NOTE: no type */ \
                                                                                                                                     \
  /**************/                                                                                                                   \
  /* allocation */                                                                                                                   \
  /**************/                                                                                                                   \
                                                                                                                                     \
  nonstatic_field(Chunk, _next, Chunk*)                                                                                              \
  nonstatic_field(Chunk, _len, const size_t)                                                                                         \
                                                                                                                                     \
  nonstatic_field(Arena, _first, Chunk*)                                                                                             \
  nonstatic_field(Arena, _chunk, Chunk*)                                                                                             \
  nonstatic_field(Arena, _hwm, char*)                                                                                                \
  nonstatic_field(Arena, _max, char*)                                                                                                \
                                                                                                                                     \
  /************/                                                                                                                     \
  /* CI */                                                                                                                           \
  /************/                                                                                                                     \
                                                                                                                                     \
 nonstatic_field(ciEnv,               _system_dictionary_modification_counter, int)                                                  \
 nonstatic_field(ciEnv,               _compiler_data, void*)                                                                         \
 nonstatic_field(ciEnv,               _failure_reason, const char*)                                                                  \
 nonstatic_field(ciEnv,               _factory, ciObjectFactory*)                                                                    \
 nonstatic_field(ciEnv,               _dependencies, Dependencies*)                                                                  \
 nonstatic_field(ciEnv,               _task, CompileTask*)                                                                           \
 nonstatic_field(ciEnv,               _arena, Arena*)                                                                                \
                                                                                                                                     \
 nonstatic_field(ciBaseObject,    _ident, uint)                                                                                      \
                                                                                                                                     \
 nonstatic_field(ciObject,    _handle, jobject)                                                                                      \
 nonstatic_field(ciObject,    _klass, ciKlass*)                                                                                      \
                                                                                                                                     \
 nonstatic_field(ciMetadata,  _metadata, Metadata*)                                                                           \
                                                                                                                                     \
 nonstatic_field(ciSymbol,    _symbol, Symbol*)                                                                                      \
                                                                                                                                     \
 nonstatic_field(ciType,    _basic_type, BasicType)                                                                                  \
                                                                                                                                     \
 nonstatic_field(ciKlass,   _name, ciSymbol*)                                                                                        \
                                                                                                                                     \
 nonstatic_field(ciArrayKlass,   _dimension, jint)                                                                                   \
                                                                                                                                     \
 nonstatic_field(ciObjArrayKlass, _element_klass, ciKlass*)                                                                          \
 nonstatic_field(ciObjArrayKlass, _base_element_klass, ciKlass*)                                                                     \
                                                                                                                                     \
 nonstatic_field(ciInstanceKlass,   _init_state, InstanceKlass::ClassState)                                                          \
 nonstatic_field(ciInstanceKlass,   _is_shared,  bool)                                                                               \
                                                                                                                                     \
 nonstatic_field(ciMethod,     _interpreter_invocation_count, int)                                                                   \
 nonstatic_field(ciMethod,     _interpreter_throwout_count, int)                                                                     \
 nonstatic_field(ciMethod,     _instructions_size, int)                                                                              \
                                                                                                                                     \
 nonstatic_field(ciMethodData, _data_size, int)                                                                                      \
 nonstatic_field(ciMethodData, _state, u_char)                                                                                       \
 nonstatic_field(ciMethodData, _extra_data_size, int)                                                                                \
 nonstatic_field(ciMethodData, _data, intptr_t*)                                                                                     \
 nonstatic_field(ciMethodData, _hint_di, int)                                                                                        \
 nonstatic_field(ciMethodData, _eflags, intx)                                                                                        \
 nonstatic_field(ciMethodData, _arg_local, intx)                                                                                     \
 nonstatic_field(ciMethodData, _arg_stack, intx)                                                                                     \
 nonstatic_field(ciMethodData, _arg_returned, intx)                                                                                  \
 nonstatic_field(ciMethodData, _current_mileage, int)                                                                                \
 nonstatic_field(ciMethodData, _orig, MethodData)                                                                             \
                                                                                                                                     \
 nonstatic_field(ciField,     _holder, ciInstanceKlass*)                                                                             \
 nonstatic_field(ciField,     _name, ciSymbol*)                                                                                      \
 nonstatic_field(ciField,     _signature, ciSymbol*)                                                                                 \
 nonstatic_field(ciField,     _offset, int)                                                                                          \
 nonstatic_field(ciField,     _is_constant, bool)                                                                                    \
 nonstatic_field(ciField,     _constant_value, ciConstant)                                                                           \
                                                                                                                                     \
 nonstatic_field(ciObjectFactory,     _ci_metadata, GrowableArray<ciMetadata*>*)                                                     \
 nonstatic_field(ciObjectFactory,     _symbols, GrowableArray<ciSymbol*>*)                                                           \
 nonstatic_field(ciObjectFactory,     _unloaded_methods, GrowableArray<ciMethod*>*)                                                  \
                                                                                                                                     \
 nonstatic_field(ciConstant,     _type, BasicType)                                                                                   \
 nonstatic_field(ciConstant,     _value._int, jint)                                                                                  \
 nonstatic_field(ciConstant,     _value._long, jlong)                                                                                \
 nonstatic_field(ciConstant,     _value._float, jfloat)                                                                              \
 nonstatic_field(ciConstant,     _value._double, jdouble)                                                                            \
 nonstatic_field(ciConstant,     _value._object, ciObject*)                                                                          \
                                                                                                                                     \
  /************/                                                                                                                     \
  /* Monitors */                                                                                                                     \
  /************/                                                                                                                     \
                                                                                                                                     \
  volatile_nonstatic_field(ObjectMonitor,      _header,                                       markOop)                               \
  unchecked_nonstatic_field(ObjectMonitor,     _object,                                       sizeof(void *)) /* NOTE: no type */    \
  unchecked_nonstatic_field(ObjectMonitor,     _owner,                                        sizeof(void *)) /* NOTE: no type */    \
  volatile_nonstatic_field(ObjectMonitor,      _count,                                        intptr_t)                              \
  volatile_nonstatic_field(ObjectMonitor,      _waiters,                                      intptr_t)                              \
  volatile_nonstatic_field(ObjectMonitor,      _recursions,                                   intptr_t)                              \
  nonstatic_field(ObjectMonitor,               FreeNext,                                      ObjectMonitor*)                        \
  volatile_nonstatic_field(BasicLock,          _displaced_header,                             markOop)                               \
  nonstatic_field(BasicObjectLock,             _lock,                                         BasicLock)                             \
  nonstatic_field(BasicObjectLock,             _obj,                                          oop)                                   \
  static_ptr_volatile_field(ObjectSynchronizer,gBlockList,                                    ObjectMonitor*)                        \
                                                                                                                                     \
  /*********************/                                                                                                            \
  /* Matcher (C2 only) */                                                                                                            \
  /*********************/                                                                                                            \
                                                                                                                                     \
  unchecked_c2_static_field(Matcher,           _regEncode,                          sizeof(Matcher::_regEncode)) /* NOTE: no type */ \
                                                                                                                                     \
  c2_nonstatic_field(Node,               _in,                      Node**)                                                           \
  c2_nonstatic_field(Node,               _out,                     Node**)                                                           \
  c2_nonstatic_field(Node,               _cnt,                     node_idx_t)                                                       \
  c2_nonstatic_field(Node,               _max,                     node_idx_t)                                                       \
  c2_nonstatic_field(Node,               _outcnt,                  node_idx_t)                                                       \
  c2_nonstatic_field(Node,               _outmax,                  node_idx_t)                                                       \
  c2_nonstatic_field(Node,               _idx,                     const node_idx_t)                                                 \
  c2_nonstatic_field(Node,               _class_id,                jushort)                                                          \
  c2_nonstatic_field(Node,               _flags,                   jushort)                                                          \
                                                                                                                                     \
  c2_nonstatic_field(Compile,            _root,                    RootNode*)                                                        \
  c2_nonstatic_field(Compile,            _unique,                  uint)                                                             \
  c2_nonstatic_field(Compile,            _entry_bci,               int)                                                              \
  c2_nonstatic_field(Compile,            _top,                     Node*)                                                            \
  c2_nonstatic_field(Compile,            _cfg,                     PhaseCFG*)                                                        \
  c2_nonstatic_field(Compile,            _regalloc,                PhaseRegAlloc*)                                                   \
  c2_nonstatic_field(Compile,            _method,                  ciMethod*)                                                        \
  c2_nonstatic_field(Compile,            _compile_id,              const int)                                                        \
  c2_nonstatic_field(Compile,            _save_argument_registers, const bool)                                                       \
  c2_nonstatic_field(Compile,            _subsume_loads,           const bool)                                                       \
  c2_nonstatic_field(Compile,            _do_escape_analysis,      const bool)                                                       \
  c2_nonstatic_field(Compile,            _eliminate_boxing,        const bool)                                                       \
  c2_nonstatic_field(Compile,            _ilt,                     InlineTree*)                                                      \
                                                                                                                                     \
  c2_nonstatic_field(InlineTree,         _caller_jvms,             JVMState*)                                                        \
  c2_nonstatic_field(InlineTree,         _method,                  ciMethod*)                                                        \
  c2_nonstatic_field(InlineTree,         _caller_tree,             InlineTree*)                                                      \
  c2_nonstatic_field(InlineTree,         _subtrees,                GrowableArray<InlineTree*>)                                       \
                                                                                                                                     \
  c2_nonstatic_field(OptoRegPair,        _first,                   short)                                                            \
  c2_nonstatic_field(OptoRegPair,        _second,                  short)                                                            \
                                                                                                                                     \
  c2_nonstatic_field(JVMState,           _caller,                  JVMState*)                                                        \
  c2_nonstatic_field(JVMState,           _depth,                   uint)                                                             \
  c2_nonstatic_field(JVMState,           _locoff,                  uint)                                                             \
  c2_nonstatic_field(JVMState,           _stkoff,                  uint)                                                             \
  c2_nonstatic_field(JVMState,           _monoff,                  uint)                                                             \
  c2_nonstatic_field(JVMState,           _scloff,                  uint)                                                             \
  c2_nonstatic_field(JVMState,           _endoff,                  uint)                                                             \
  c2_nonstatic_field(JVMState,           _sp,                      uint)                                                             \
  c2_nonstatic_field(JVMState,           _bci,                     int)                                                              \
  c2_nonstatic_field(JVMState,           _method,                  ciMethod*)                                                        \
  c2_nonstatic_field(JVMState,           _map,                     SafePointNode*)                                                   \
                                                                                                                                     \
  c2_nonstatic_field(SafePointNode,      _jvms,                    JVMState* const)                                                  \
                                                                                                                                     \
  c2_nonstatic_field(MachSafePointNode,  _jvms,                    JVMState*)                                                        \
  c2_nonstatic_field(MachSafePointNode,  _jvmadj,                  uint)                                                             \
                                                                                                                                     \
  c2_nonstatic_field(MachIfNode,         _prob,                    jfloat)                                                           \
  c2_nonstatic_field(MachIfNode,         _fcnt,                    jfloat)                                                           \
                                                                                                                                     \
  c2_nonstatic_field(CallNode,           _entry_point,             address)                                                          \
                                                                                                                                     \
  c2_nonstatic_field(CallJavaNode,       _method,                  ciMethod*)                                                        \
                                                                                                                                     \
  c2_nonstatic_field(CallRuntimeNode,    _name,                    const char*)                                                      \
                                                                                                                                     \
  c2_nonstatic_field(CallStaticJavaNode, _name,                    const char*)                                                      \
                                                                                                                                     \
  c2_nonstatic_field(MachCallJavaNode,   _method,                  ciMethod*)                                                        \
  c2_nonstatic_field(MachCallJavaNode,   _bci,                     int)                                                              \
                                                                                                                                     \
  c2_nonstatic_field(MachCallStaticJavaNode, _name,                const char*)                                                      \
                                                                                                                                     \
  c2_nonstatic_field(MachCallRuntimeNode,  _name,                  const char*)                                                      \
                                                                                                                                     \
  c2_nonstatic_field(PhaseCFG,           _number_of_blocks,        uint)                                                             \
  c2_nonstatic_field(PhaseCFG,           _blocks,                  Block_List)                                                       \
  c2_nonstatic_field(PhaseCFG,           _node_to_block_mapping,   Block_Array)                                                      \
  c2_nonstatic_field(PhaseCFG,           _root_block,              Block*)                                                           \
                                                                                                                                     \
  c2_nonstatic_field(PhaseRegAlloc,      _node_regs,               OptoRegPair*)                                                     \
  c2_nonstatic_field(PhaseRegAlloc,      _node_regs_max_index,     uint)                                                             \
  c2_nonstatic_field(PhaseRegAlloc,      _framesize,               uint)                                                             \
  c2_nonstatic_field(PhaseRegAlloc,      _max_reg,                 OptoReg::Name)                                                    \
                                                                                                                                     \
  c2_nonstatic_field(PhaseChaitin,       _trip_cnt,                int)                                                              \
  c2_nonstatic_field(PhaseChaitin,       _alternate,               int)                                                              \
  c2_nonstatic_field(PhaseChaitin,       _lo_degree,               uint)                                                             \
  c2_nonstatic_field(PhaseChaitin,       _lo_stk_degree,           uint)                                                             \
  c2_nonstatic_field(PhaseChaitin,       _hi_degree,               uint)                                                             \
  c2_nonstatic_field(PhaseChaitin,       _simplified,              uint)                                                             \
                                                                                                                                     \
  c2_nonstatic_field(Block,              _nodes,                   Node_List)                                                        \
  c2_nonstatic_field(Block,              _succs,                   Block_Array)                                                      \
  c2_nonstatic_field(Block,              _num_succs,               uint)                                                             \
  c2_nonstatic_field(Block,              _pre_order,               uint)                                                             \
  c2_nonstatic_field(Block,              _dom_depth,               uint)                                                             \
  c2_nonstatic_field(Block,              _idom,                    Block*)                                                           \
  c2_nonstatic_field(Block,              _freq,                    jfloat)                                                           \
                                                                                                                                     \
  c2_nonstatic_field(CFGElement,         _freq,                    jfloat)                                                           \
                                                                                                                                     \
  c2_nonstatic_field(Block_List,         _cnt,                     uint)                                                             \
                                                                                                                                     \
  c2_nonstatic_field(Block_Array,        _size,                    uint)                                                             \
  c2_nonstatic_field(Block_Array,        _blocks,                  Block**)                                                          \
  c2_nonstatic_field(Block_Array,        _arena,                   Arena*)                                                           \
                                                                                                                                     \
  c2_nonstatic_field(Node_List,          _cnt,                     uint)                                                             \
                                                                                                                                     \
  c2_nonstatic_field(Node_Array,         _max,                     uint)                                                             \
  c2_nonstatic_field(Node_Array,         _nodes,                   Node**)                                                           \
  c2_nonstatic_field(Node_Array,         _a,                       Arena*)                                                           \
                                                                                                                                     \
                                                                                                                                     \
  /*********************/                                                                                                            \
  /* -XX flags         */                                                                                                            \
  /*********************/                                                                                                            \
                                                                                                                                     \
  nonstatic_field(Flag,                        _type,                                         const char*)                           \
  nonstatic_field(Flag,                        _name,                                         const char*)                           \
  unchecked_nonstatic_field(Flag,              _addr,                                         sizeof(void*)) /* NOTE: no type */     \
  nonstatic_field(Flag,                        _flags,                                        Flag::Flags)                           \
  static_field(Flag,                           flags,                                         Flag*)                                 \
  static_field(Flag,                           numFlags,                                      size_t)                                \
                                                                                                                                     \
  /*************************/                                                                                                        \
  /* JDK / VM version info */                                                                                                        \
  /*************************/                                                                                                        \
                                                                                                                                     \
  static_field(Abstract_VM_Version,            _s_vm_release,                                 const char*)                           \
  static_field(Abstract_VM_Version,            _s_internal_vm_info_string,                    const char*)                           \
  static_field(Abstract_VM_Version,            _vm_major_version,                             int)                                   \
  static_field(Abstract_VM_Version,            _vm_minor_version,                             int)                                   \
  static_field(Abstract_VM_Version,            _vm_build_number,                              int)                                   \
  static_field(Abstract_VM_Version,            _reserve_for_allocation_prefetch,              int)                                   \
                                                                                                                                     \
  static_field(JDK_Version,                    _current,                                      JDK_Version)                           \
  nonstatic_field(JDK_Version,                 _partially_initialized,                        bool)                                  \
  nonstatic_field(JDK_Version,                 _major,                                        unsigned char)                         \
                                                                                                                                     \
  /*************************/                                                                                                        \
  /* JVMTI */                                                                                                                        \
  /*************************/                                                                                                        \
                                                                                                                                     \
  JVMTI_STRUCTS(static_field)                                                                                                        \
                                                                                                                                     \
  /*************/                                                                                                                    \
  /* Arguments */                                                                                                                    \
  /*************/                                                                                                                    \
                                                                                                                                     \
  static_field(Arguments,                      _jvm_flags_array,                              char**)                                \
  static_field(Arguments,                      _num_jvm_flags,                                int)                                   \
  static_field(Arguments,                      _jvm_args_array,                               char**)                                \
  static_field(Arguments,                      _num_jvm_args,                                 int)                                   \
  static_field(Arguments,                      _java_command,                                 char*)                                 \
                                                                                                                                     \
  /************/                                                                                                                     \
  /* Array<T> */                                                                                                                     \
  /************/                                                                                                                     \
                                                                                                                                     \
  nonstatic_field(Array<int>,                      _length,                                   int)                                   \
  unchecked_nonstatic_field(Array<int>,            _data,                                     sizeof(int))                           \
  unchecked_nonstatic_field(Array<u1>,             _data,                                     sizeof(u1))                            \
  unchecked_nonstatic_field(Array<u2>,             _data,                                     sizeof(u2))                            \
  unchecked_nonstatic_field(Array<Method*>,        _data,                                     sizeof(Method*))                       \
  unchecked_nonstatic_field(Array<Klass*>,         _data,                                     sizeof(Klass*))                        \
                                                                                                                                     \
  /*********************************/                                                                                                \
  /* java_lang_Class fields        */                                                                                                \
  /*********************************/                                                                                                \
                                                                                                                                     \
  static_field(java_lang_Class,                _klass_offset,                                 int)                                   \
  static_field(java_lang_Class,                _array_klass_offset,                           int)                                   \
  static_field(java_lang_Class,                _oop_size_offset,                              int)                                   \
  static_field(java_lang_Class,                _static_oop_field_count_offset,                int)                                   \
                                                                                                                                     \
  /************************/                                                                                                         \
  /* Miscellaneous fields */                                                                                                         \
  /************************/                                                                                                         \
                                                                                                                                     \
  nonstatic_field(CompileTask,                 _method,                                      Method*)                                \
  nonstatic_field(CompileTask,                 _osr_bci,                                     int)                                    \
  nonstatic_field(CompileTask,                 _comp_level,                                  int)                                    \
  nonstatic_field(CompileTask,                 _compile_id,                                  uint)                                   \
  nonstatic_field(CompileTask,                 _next,                                        CompileTask*)                           \
  nonstatic_field(CompileTask,                 _prev,                                        CompileTask*)                           \
                                                                                                                                     \
  nonstatic_field(vframeArray,                 _next,                                        vframeArray*)                           \
  nonstatic_field(vframeArray,                 _original,                                    frame)                                  \
  nonstatic_field(vframeArray,                 _caller,                                      frame)                                  \
  nonstatic_field(vframeArray,                 _frames,                                      int)                                    \
                                                                                                                                     \
  nonstatic_field(vframeArrayElement,          _frame,                                       frame)                                  \
  nonstatic_field(vframeArrayElement,          _bci,                                         int)                                    \
  nonstatic_field(vframeArrayElement,          _method,                                      Method*)                                \
                                                                                                                                     \
  nonstatic_field(PtrQueue,                    _active,                                      bool)                                   \
  nonstatic_field(PtrQueue,                    _buf,                                         void**)                                 \
  nonstatic_field(PtrQueue,                    _index,                                       size_t)                                 \
                                                                                                                                     \
  nonstatic_field(AccessFlags,                 _flags,                                       jint)                                   \
  nonstatic_field(elapsedTimer,                _counter,                                     jlong)                                  \
  nonstatic_field(elapsedTimer,                _active,                                      bool)                                   \
  nonstatic_field(InvocationCounter,           _counter,                                     unsigned int)                           \
  volatile_nonstatic_field(FreeChunk,          _size,                                        size_t)                                 \
  nonstatic_field(FreeChunk,                   _next,                                        FreeChunk*)                             \
  nonstatic_field(FreeChunk,                   _prev,                                        FreeChunk*)                             \
  nonstatic_field(AdaptiveFreeList<FreeChunk>, _size,                                        size_t)                                 \
  nonstatic_field(AdaptiveFreeList<FreeChunk>, _count,                                       ssize_t)


//--------------------------------------------------------------------------------
// VM_TYPES
//
// This list must enumerate at least all of the types in the above
// list. For the types in the above list, the entry below must have
// exactly the same spacing since string comparisons are done in the
// code which verifies the consistency of these tables (in the debug
// build).
//
// In addition to the above types, this list is required to enumerate
// the JNI's java types, which are used to indicate the size of Java
// fields in this VM to the SA. Further, oop types are currently
// distinguished by name (i.e., ends with "oop") over in the SA.
//
// The declare_toplevel_type macro should be used to declare types
// which do not have a superclass.
//
// The declare_integer_type and declare_unsigned_integer_type macros
// are required in order to properly identify C integer types over in
// the SA. They should be used for any type which is otherwise opaque
// and which it is necessary to coerce into an integer value. This
// includes, for example, the type uintptr_t. Note that while they
// will properly identify the type's size regardless of the platform,
// since it is does not seem possible to deduce or check signedness at
// compile time using the pointer comparison tricks, it is currently
// required that the given types have the same signedness across all
// platforms.
//
// NOTE that there are platform-specific additions to this table in
// vmStructs_<os>_<cpu>.hpp.

#define VM_TYPES(declare_type,                                            \
                 declare_toplevel_type,                                   \
                 declare_oop_type,                                        \
                 declare_integer_type,                                    \
                 declare_unsigned_integer_type,                           \
                 declare_c1_toplevel_type,                                \
                 declare_c2_type,                                         \
                 declare_c2_toplevel_type)                                \
                                                                          \
  /*************************************************************/         \
  /* Java primitive types -- required by the SA implementation */         \
  /* in order to determine the size of Java fields in this VM  */         \
  /* (the implementation looks up these names specifically)    */         \
  /* NOTE: since we fetch these sizes from the remote VM, we   */         \
  /* have a bootstrapping sequence during which it is not      */         \
  /* valid to fetch Java values from the remote process, only  */         \
  /* C integer values (of known size). NOTE also that we do    */         \
  /* NOT include "Java unsigned" types like juint here; since  */         \
  /* Java does not have unsigned primitive types, those can    */         \
  /* not be mapped directly and are considered to be C integer */         \
  /* types in this system (see the "other types" section,      */         \
  /* below.)                                                   */         \
  /*************************************************************/         \
                                                                          \
  declare_toplevel_type(jboolean)                                         \
  declare_toplevel_type(jbyte)                                            \
  declare_toplevel_type(jchar)                                            \
  declare_toplevel_type(jdouble)                                          \
  declare_toplevel_type(jfloat)                                           \
  declare_toplevel_type(jint)                                             \
  declare_toplevel_type(jlong)                                            \
  declare_toplevel_type(jshort)                                           \
                                                                          \
  /*********************************************************************/ \
  /* C integer types. User-defined typedefs (like "size_t" or          */ \
  /* "intptr_t") are guaranteed to be present with the same names over */ \
  /* in the SA's type database. Names like "unsigned short" are not    */ \
  /* guaranteed to be visible through the SA's type database lookup    */ \
  /* mechanism, though they will have a Type object created for them   */ \
  /* and are valid types for Fields.                                   */ \
  /*********************************************************************/ \
  declare_integer_type(bool)                                              \
  declare_integer_type(short)                                             \
  declare_integer_type(int)                                               \
  declare_integer_type(long)                                              \
  declare_integer_type(char)                                              \
  declare_unsigned_integer_type(unsigned char)                            \
  declare_unsigned_integer_type(volatile unsigned char)                   \
  declare_unsigned_integer_type(u_char)                                   \
  declare_unsigned_integer_type(unsigned int)                             \
  declare_unsigned_integer_type(uint)                                     \
  declare_unsigned_integer_type(unsigned short)                           \
  declare_unsigned_integer_type(jushort)                                  \
  declare_unsigned_integer_type(unsigned long)                            \
  /* The compiler thinks this is a different type than */                 \
  /* unsigned short on Win32 */                                           \
  declare_unsigned_integer_type(u1)                                       \
  declare_unsigned_integer_type(u2)                                       \
  declare_unsigned_integer_type(u4)                                       \
  declare_unsigned_integer_type(u8)                                       \
  declare_unsigned_integer_type(unsigned)                                 \
                                                                          \
  /*****************************/                                         \
  /* C primitive pointer types */                                         \
  /*****************************/                                         \
                                                                          \
  declare_toplevel_type(void*)                                            \
  declare_toplevel_type(int*)                                             \
  declare_toplevel_type(char*)                                            \
  declare_toplevel_type(char**)                                           \
  declare_toplevel_type(u_char*)                                          \
  declare_toplevel_type(unsigned char*)                                   \
  declare_toplevel_type(volatile unsigned char*)                          \
                                                                          \
  /*******************************************************************/   \
  /* Types which it will be handy to have available over in the SA   */   \
  /* in order to do platform-independent address -> integer coercion */   \
  /* (note: these will be looked up by name)                         */   \
  /*******************************************************************/   \
                                                                          \
  declare_unsigned_integer_type(size_t)                                   \
  declare_integer_type(ssize_t)                                           \
  declare_integer_type(intx)                                              \
  declare_integer_type(intptr_t)                                          \
  declare_unsigned_integer_type(uintx)                                    \
  declare_unsigned_integer_type(uintptr_t)                                \
  declare_unsigned_integer_type(uint32_t)                                 \
  declare_unsigned_integer_type(uint64_t)                                 \
                                                                          \
  /******************************************/                            \
  /* OopDesc hierarchy (NOTE: some missing) */                            \
  /******************************************/                            \
                                                                          \
  declare_toplevel_type(oopDesc)                                          \
    declare_type(arrayOopDesc, oopDesc)                                   \
      declare_type(objArrayOopDesc, arrayOopDesc)                         \
    declare_type(instanceOopDesc, oopDesc)                                \
    declare_type(markOopDesc, oopDesc)                                    \
                                                                          \
  /**************************************************/                    \
  /* MetadataOopDesc hierarchy (NOTE: some missing) */                    \
  /**************************************************/                    \
                                                                          \
  declare_toplevel_type(CompiledICHolder)                                 \
  declare_toplevel_type(MetaspaceObj)                                     \
    declare_type(Metadata, MetaspaceObj)                                  \
    declare_type(Klass, Metadata)                                         \
           declare_type(ArrayKlass, Klass)                                \
           declare_type(ObjArrayKlass, ArrayKlass)                        \
           declare_type(TypeArrayKlass, ArrayKlass)                       \
      declare_type(InstanceKlass, Klass)                                  \
        declare_type(InstanceClassLoaderKlass, InstanceKlass)             \
        declare_type(InstanceMirrorKlass, InstanceKlass)                  \
        declare_type(InstanceRefKlass, InstanceKlass)                     \
    declare_type(ConstantPool, Metadata)                                  \
    declare_type(ConstantPoolCache, MetaspaceObj)                         \
    declare_type(MethodData, Metadata)                                    \
    declare_type(Method, Metadata)                                        \
    declare_type(MethodCounters, MetaspaceObj)                            \
    declare_type(ConstMethod, MetaspaceObj)                               \
                                                                          \
  declare_toplevel_type(vtableEntry)                                      \
                                                                          \
           declare_toplevel_type(Symbol)                                  \
           declare_toplevel_type(Symbol*)                                 \
  declare_toplevel_type(volatile Metadata*)                               \
                                                                          \
  declare_toplevel_type(DataLayout)                                       \
  declare_toplevel_type(nmethodBucket)                                    \
                                                                          \
  /********/                                                              \
  /* Oops */                                                              \
  /********/                                                              \
                                                                          \
  declare_oop_type(markOop)                                               \
  declare_oop_type(objArrayOop)                                           \
  declare_oop_type(oop)                                                   \
  declare_oop_type(narrowOop)                                             \
  declare_oop_type(typeArrayOop)                                          \
                                                                          \
  /*************************************/                                 \
  /* MethodOop-related data structures */                                 \
  /*************************************/                                 \
                                                                          \
  declare_toplevel_type(CheckedExceptionElement)                          \
  declare_toplevel_type(LocalVariableTableElement)                        \
  declare_toplevel_type(ExceptionTableElement)                            \
  declare_toplevel_type(MethodParametersElement)                          \
                                                                          \
  declare_toplevel_type(ClassLoaderData)                                  \
  declare_toplevel_type(ClassLoaderDataGraph)                             \
                                                                          \
  /******************************************/                            \
  /* Generation and space hierarchies       */                            \
  /* (needed for run-time type information) */                            \
  /******************************************/                            \
                                                                          \
  declare_toplevel_type(CollectedHeap)                                    \
           declare_type(SharedHeap,                   CollectedHeap)      \
           declare_type(GenCollectedHeap,             SharedHeap)         \
  declare_toplevel_type(Generation)                                       \
           declare_type(DefNewGeneration,             Generation)         \
           declare_type(CardGeneration,               Generation)         \
           declare_type(OneContigSpaceCardGeneration, CardGeneration)     \
           declare_type(TenuredGeneration,            OneContigSpaceCardGeneration) \
  declare_toplevel_type(Space)                                            \
  declare_toplevel_type(BitMap)                                           \
           declare_type(CompactibleSpace,             Space)              \
           declare_type(ContiguousSpace,              CompactibleSpace)   \
           declare_type(EdenSpace,                    ContiguousSpace)    \
           declare_type(OffsetTableContigSpace,       ContiguousSpace)    \
           declare_type(TenuredSpace,                 OffsetTableContigSpace) \
  declare_toplevel_type(BarrierSet)                                       \
           declare_type(ModRefBarrierSet,             BarrierSet)         \
           declare_type(CardTableModRefBS,            ModRefBarrierSet)   \
           declare_type(CardTableModRefBSForCTRS,     CardTableModRefBS)  \
  declare_toplevel_type(BarrierSet::Name)                                 \
  declare_toplevel_type(GenRemSet)                                        \
           declare_type(CardTableRS,                  GenRemSet)          \
  declare_toplevel_type(BlockOffsetSharedArray)                           \
  declare_toplevel_type(BlockOffsetTable)                                 \
           declare_type(BlockOffsetArray,             BlockOffsetTable)   \
           declare_type(BlockOffsetArrayContigSpace,  BlockOffsetArray)   \
           declare_type(BlockOffsetArrayNonContigSpace, BlockOffsetArray) \
                                                                          \
  /* Miscellaneous other GC types */                                      \
                                                                          \
  declare_toplevel_type(ageTable)                                         \
  declare_toplevel_type(Generation::StatRecord)                           \
  declare_toplevel_type(GenerationSpec)                                   \
  declare_toplevel_type(HeapWord)                                         \
  declare_toplevel_type(MemRegion)                                        \
  declare_toplevel_type(ThreadLocalAllocBuffer)                           \
  declare_toplevel_type(VirtualSpace)                                     \
  declare_toplevel_type(WaterMark)                                        \
  declare_toplevel_type(ObjPtrQueue)                                      \
  declare_toplevel_type(DirtyCardQueue)                                   \
                                                                          \
  /* Pointers to Garbage Collection types */                              \
                                                                          \
  declare_toplevel_type(BarrierSet*)                                      \
  declare_toplevel_type(BlockOffsetSharedArray*)                          \
  declare_toplevel_type(GenRemSet*)                                       \
  declare_toplevel_type(CardTableRS*)                                     \
  declare_toplevel_type(CardTableModRefBS*)                               \
  declare_toplevel_type(CardTableModRefBS**)                              \
  declare_toplevel_type(CardTableModRefBSForCTRS*)                        \
  declare_toplevel_type(CardTableModRefBSForCTRS**)                       \
  declare_toplevel_type(CollectedHeap*)                                   \
  declare_toplevel_type(ContiguousSpace*)                                 \
  declare_toplevel_type(DefNewGeneration*)                                \
  declare_toplevel_type(EdenSpace*)                                       \
  declare_toplevel_type(GenCollectedHeap*)                                \
  declare_toplevel_type(Generation*)                                      \
  declare_toplevel_type(GenerationSpec**)                                 \
  declare_toplevel_type(HeapWord*)                                        \
  declare_toplevel_type(MemRegion*)                                       \
  declare_toplevel_type(OffsetTableContigSpace*)                          \
  declare_toplevel_type(OneContigSpaceCardGeneration*)                    \
  declare_toplevel_type(Space*)                                           \
  declare_toplevel_type(ThreadLocalAllocBuffer*)                          \
                                                                          \
  /************************/                                              \
  /* PerfMemory - jvmstat */                                              \
  /************************/                                              \
                                                                          \
  declare_toplevel_type(PerfDataPrologue)                                 \
  declare_toplevel_type(PerfDataPrologue*)                                \
  declare_toplevel_type(PerfDataEntry)                                    \
  declare_toplevel_type(PerfMemory)                                       \
                                                                          \
  /*********************************/                                     \
  /* SymbolTable, SystemDictionary */                                     \
  /*********************************/                                     \
                                                                          \
  declare_toplevel_type(BasicHashtable<mtInternal>)                       \
    declare_type(IntptrHashtable, BasicHashtable<mtInternal>)             \
  declare_type(SymbolTable, SymbolHashtable)                              \
  declare_type(StringTable, StringHashtable)                              \
    declare_type(LoaderConstraintTable, KlassHashtable)                   \
    declare_type(KlassTwoOopHashtable, KlassHashtable)                    \
    declare_type(Dictionary, KlassTwoOopHashtable)                        \
    declare_type(PlaceholderTable, SymbolTwoOopHashtable)                 \
  declare_toplevel_type(BasicHashtableEntry<mtInternal>)                  \
  declare_type(IntptrHashtableEntry, BasicHashtableEntry<mtInternal>)     \
    declare_type(DictionaryEntry, KlassHashtableEntry)                    \
    declare_type(PlaceholderEntry, SymbolHashtableEntry)                  \
    declare_type(LoaderConstraintEntry, KlassHashtableEntry)              \
  declare_toplevel_type(HashtableBucket<mtInternal>)                      \
  declare_toplevel_type(SystemDictionary)                                 \
  declare_toplevel_type(vmSymbols)                                        \
  declare_toplevel_type(ProtectionDomainEntry)                            \
  declare_toplevel_type(ProtectionDomainCacheEntry)                       \
                                                                          \
  declare_toplevel_type(GenericGrowableArray)                             \
  declare_toplevel_type(GrowableArray<int>)                               \
  declare_toplevel_type(Arena)                                            \
    declare_type(ResourceArea, Arena)                                     \
  declare_toplevel_type(Chunk)                                            \
                                                                          \
  /***********************************************************/           \
  /* Thread hierarchy (needed for run-time type information) */           \
  /***********************************************************/           \
                                                                          \
  declare_toplevel_type(Threads)                                          \
  declare_toplevel_type(ThreadShadow)                                     \
           declare_type(Thread, ThreadShadow)                             \
           declare_type(NamedThread, Thread)                              \
           declare_type(WatcherThread, Thread)                            \
           declare_type(JavaThread, Thread)                               \
           declare_type(JvmtiAgentThread, JavaThread)                     \
           declare_type(ServiceThread, JavaThread)                        \
  declare_type(CompilerThread, JavaThread)                                \
  declare_toplevel_type(OSThread)                                         \
  declare_toplevel_type(JavaFrameAnchor)                                  \
                                                                          \
  /***************/                                                       \
  /* Interpreter */                                                       \
  /***************/                                                       \
                                                                          \
  declare_toplevel_type(AbstractInterpreter)                              \
                                                                          \
  /*********/                                                             \
  /* Stubs */                                                             \
  /*********/                                                             \
                                                                          \
  declare_toplevel_type(StubQueue)                                        \
  declare_toplevel_type(StubRoutines)                                     \
  declare_toplevel_type(Stub)                                             \
           declare_type(InterpreterCodelet, Stub)                         \
                                                                          \
  /*************/                                                         \
  /* JavaCalls */                                                         \
  /*************/                                                         \
                                                                          \
  declare_toplevel_type(JavaCallWrapper)                                  \
                                                                          \
  /*************/                                                         \
  /* CodeCache */                                                         \
  /*************/                                                         \
                                                                          \
  declare_toplevel_type(CodeCache)                                        \
                                                                          \
  /************/                                                          \
  /* CodeHeap */                                                          \
  /************/                                                          \
                                                                          \
  declare_toplevel_type(CodeHeap)                                         \
  declare_toplevel_type(CodeHeap*)                                        \
  declare_toplevel_type(HeapBlock)                                        \
  declare_toplevel_type(HeapBlock::Header)                                \
           declare_type(FreeBlock, HeapBlock)                             \
                                                                          \
  /*************************************************************/         \
  /* CodeBlob hierarchy (needed for run-time type information) */         \
  /*************************************************************/         \
                                                                          \
  declare_toplevel_type(SharedRuntime)                                    \
                                                                          \
  declare_toplevel_type(CodeBlob)                                         \
  declare_type(BufferBlob,               CodeBlob)                        \
  declare_type(AdapterBlob,              BufferBlob)                      \
  declare_type(MethodHandlesAdapterBlob, BufferBlob)                      \
  declare_type(nmethod,                  CodeBlob)                        \
  declare_type(RuntimeStub,              CodeBlob)                        \
  declare_type(SingletonBlob,            CodeBlob)                        \
  declare_type(SafepointBlob,            SingletonBlob)                   \
  declare_type(DeoptimizationBlob,       SingletonBlob)                   \
  declare_c2_type(ExceptionBlob,         SingletonBlob)                   \
  declare_c2_type(UncommonTrapBlob,      CodeBlob)                        \
                                                                          \
  /***************************************/                               \
  /* PcDesc and other compiled code info */                               \
  /***************************************/                               \
                                                                          \
  declare_toplevel_type(PcDesc)                                           \
  declare_toplevel_type(ExceptionCache)                                   \
  declare_toplevel_type(PcDescCache)                                      \
  declare_toplevel_type(Dependencies)                                     \
  declare_toplevel_type(CompileTask)                                      \
  declare_toplevel_type(Deoptimization)                                   \
                                                                          \
  /************************/                                              \
  /* OopMap and OopMapSet */                                              \
  /************************/                                              \
                                                                          \
  declare_toplevel_type(OopMap)                                           \
  declare_toplevel_type(OopMapSet)                                        \
                                                                          \
  /********************/                                                  \
  /* CompressedStream */                                                  \
  /********************/                                                  \
                                                                          \
  declare_toplevel_type(CompressedStream)                                 \
                                                                          \
  /**************/                                                        \
  /* VMRegImpl  */                                                        \
  /**************/                                                        \
                                                                          \
  declare_toplevel_type(VMRegImpl)                                        \
                                                                          \
  /*********************************/                                     \
  /* JNIHandles and JNIHandleBlock */                                     \
  /*********************************/                                     \
                                                                          \
  declare_toplevel_type(JNIHandles)                                       \
  declare_toplevel_type(JNIHandleBlock)                                   \
  declare_toplevel_type(jobject)                                          \
                                                                          \
  /**********************/                                                \
  /* Runtime1 (C1 only) */                                                \
  /**********************/                                                \
                                                                          \
  declare_c1_toplevel_type(Runtime1)                                      \
                                                                          \
  /************/                                                          \
  /* Monitors */                                                          \
  /************/                                                          \
                                                                          \
  declare_toplevel_type(ObjectMonitor)                                    \
  declare_toplevel_type(ObjectSynchronizer)                               \
  declare_toplevel_type(BasicLock)                                        \
  declare_toplevel_type(BasicObjectLock)                                  \
                                                                          \
  /*********************/                                                 \
  /* Matcher (C2 only) */                                                 \
  /*********************/                                                 \
                                                                          \
  declare_c2_toplevel_type(Matcher)                                       \
  declare_c2_toplevel_type(Compile)                                       \
  declare_c2_toplevel_type(InlineTree)                                    \
  declare_c2_toplevel_type(OptoRegPair)                                   \
  declare_c2_toplevel_type(JVMState)                                      \
  declare_c2_toplevel_type(Phase)                                         \
    declare_c2_type(PhaseCFG, Phase)                                      \
    declare_c2_type(PhaseRegAlloc, Phase)                                 \
    declare_c2_type(PhaseChaitin, PhaseRegAlloc)                          \
  declare_c2_toplevel_type(CFGElement)                                    \
    declare_c2_type(Block, CFGElement)                                    \
  declare_c2_toplevel_type(Block_Array)                                   \
    declare_c2_type(Block_List, Block_Array)                              \
  declare_c2_toplevel_type(Node_Array)                                    \
  declare_c2_type(Node_List, Node_Array)                                  \
  declare_c2_type(Unique_Node_List, Node_List)                            \
  declare_c2_toplevel_type(Node)                                          \
  declare_c2_type(AddNode, Node)                                          \
  declare_c2_type(AddINode, AddNode)                                      \
  declare_c2_type(AddLNode, AddNode)                                      \
  declare_c2_type(AddFNode, AddNode)                                      \
  declare_c2_type(AddDNode, AddNode)                                      \
  declare_c2_type(AddPNode, Node)                                         \
  declare_c2_type(OrINode, AddNode)                                       \
  declare_c2_type(OrLNode, AddNode)                                       \
  declare_c2_type(XorINode, AddNode)                                      \
  declare_c2_type(XorLNode, AddNode)                                      \
  declare_c2_type(MaxNode, AddNode)                                       \
  declare_c2_type(MaxINode, MaxNode)                                      \
  declare_c2_type(MinINode, MaxNode)                                      \
  declare_c2_type(StartNode, MultiNode)                                   \
  declare_c2_type(StartOSRNode, StartNode)                                \
  declare_c2_type(ParmNode, ProjNode)                                     \
  declare_c2_type(ReturnNode, Node)                                       \
  declare_c2_type(RethrowNode, Node)                                      \
  declare_c2_type(TailCallNode, ReturnNode)                               \
  declare_c2_type(TailJumpNode, ReturnNode)                               \
  declare_c2_type(SafePointNode, MultiNode)                               \
  declare_c2_type(CallNode, SafePointNode)                                \
  declare_c2_type(CallJavaNode, CallNode)                                 \
  declare_c2_type(CallStaticJavaNode, CallJavaNode)                       \
  declare_c2_type(CallDynamicJavaNode, CallJavaNode)                      \
  declare_c2_type(CallRuntimeNode, CallNode)                              \
  declare_c2_type(CallLeafNode, CallRuntimeNode)                          \
  declare_c2_type(CallLeafNoFPNode, CallLeafNode)                         \
  declare_c2_type(AllocateNode, CallNode)                                 \
  declare_c2_type(AllocateArrayNode, AllocateNode)                        \
  declare_c2_type(LockNode, AbstractLockNode)                             \
  declare_c2_type(UnlockNode, AbstractLockNode)                           \
  declare_c2_type(FastLockNode, CmpNode)                                  \
  declare_c2_type(FastUnlockNode, CmpNode)                                \
  declare_c2_type(RegionNode, Node)                                       \
  declare_c2_type(JProjNode, ProjNode)                                    \
  declare_c2_type(PhiNode, TypeNode)                                      \
  declare_c2_type(GotoNode, Node)                                         \
  declare_c2_type(CProjNode, ProjNode)                                    \
  declare_c2_type(MultiBranchNode, MultiNode)                             \
  declare_c2_type(IfNode, MultiBranchNode)                                \
  declare_c2_type(IfTrueNode, CProjNode)                                  \
  declare_c2_type(IfFalseNode, CProjNode)                                 \
  declare_c2_type(PCTableNode, MultiBranchNode)                           \
  declare_c2_type(JumpNode, PCTableNode)                                  \
  declare_c2_type(JumpProjNode, JProjNode)                                \
  declare_c2_type(CatchNode, PCTableNode)                                 \
  declare_c2_type(CatchProjNode, CProjNode)                               \
  declare_c2_type(CreateExNode, TypeNode)                                 \
  declare_c2_type(ClearArrayNode, Node)                                   \
  declare_c2_type(NeverBranchNode, MultiBranchNode)                       \
  declare_c2_type(ConNode, TypeNode)                                      \
  declare_c2_type(ConINode, ConNode)                                      \
  declare_c2_type(ConPNode, ConNode)                                      \
  declare_c2_type(ConNNode, ConNode)                                      \
  declare_c2_type(ConLNode, ConNode)                                      \
  declare_c2_type(ConFNode, ConNode)                                      \
  declare_c2_type(ConDNode, ConNode)                                      \
  declare_c2_type(BinaryNode, Node)                                       \
  declare_c2_type(CMoveNode, TypeNode)                                    \
  declare_c2_type(CMoveDNode, CMoveNode)                                  \
  declare_c2_type(CMoveFNode, CMoveNode)                                  \
  declare_c2_type(CMoveINode, CMoveNode)                                  \
  declare_c2_type(CMoveLNode, CMoveNode)                                  \
  declare_c2_type(CMovePNode, CMoveNode)                                  \
  declare_c2_type(CMoveNNode, CMoveNode)                                  \
  declare_c2_type(EncodePNode, TypeNode)                                  \
  declare_c2_type(DecodeNNode, TypeNode)                                  \
  declare_c2_type(EncodePKlassNode, TypeNode)                             \
  declare_c2_type(DecodeNKlassNode, TypeNode)                             \
  declare_c2_type(ConstraintCastNode, TypeNode)                           \
  declare_c2_type(CastIINode, ConstraintCastNode)                         \
  declare_c2_type(CastPPNode, ConstraintCastNode)                         \
  declare_c2_type(CheckCastPPNode, TypeNode)                              \
  declare_c2_type(Conv2BNode, Node)                                       \
  declare_c2_type(ConvD2FNode, Node)                                      \
  declare_c2_type(ConvD2INode, Node)                                      \
  declare_c2_type(ConvD2LNode, Node)                                      \
  declare_c2_type(ConvF2DNode, Node)                                      \
  declare_c2_type(ConvF2INode, Node)                                      \
  declare_c2_type(ConvF2LNode, Node)                                      \
  declare_c2_type(ConvI2DNode, Node)                                      \
  declare_c2_type(ConvI2FNode, Node)                                      \
  declare_c2_type(ConvI2LNode, TypeNode)                                  \
  declare_c2_type(ConvL2DNode, Node)                                      \
  declare_c2_type(ConvL2FNode, Node)                                      \
  declare_c2_type(ConvL2INode, Node)                                      \
  declare_c2_type(CastX2PNode, Node)                                      \
  declare_c2_type(CastP2XNode, Node)                                      \
  declare_c2_type(MemBarNode, MultiNode)                                  \
  declare_c2_type(MemBarAcquireNode, MemBarNode)                          \
  declare_c2_type(MemBarReleaseNode, MemBarNode)                          \
  declare_c2_type(LoadFenceNode, MemBarNode)                              \
  declare_c2_type(StoreFenceNode, MemBarNode)                             \
  declare_c2_type(MemBarVolatileNode, MemBarNode)                         \
  declare_c2_type(MemBarCPUOrderNode, MemBarNode)                         \
  declare_c2_type(InitializeNode, MemBarNode)                             \
  declare_c2_type(ThreadLocalNode, Node)                                  \
  declare_c2_type(Opaque1Node, Node)                                      \
  declare_c2_type(Opaque2Node, Node)                                      \
  declare_c2_type(PartialSubtypeCheckNode, Node)                          \
  declare_c2_type(MoveI2FNode, Node)                                      \
  declare_c2_type(MoveL2DNode, Node)                                      \
  declare_c2_type(MoveF2INode, Node)                                      \
  declare_c2_type(MoveD2LNode, Node)                                      \
  declare_c2_type(DivINode, Node)                                         \
  declare_c2_type(DivLNode, Node)                                         \
  declare_c2_type(DivFNode, Node)                                         \
  declare_c2_type(DivDNode, Node)                                         \
  declare_c2_type(ModINode, Node)                                         \
  declare_c2_type(ModLNode, Node)                                         \
  declare_c2_type(ModFNode, Node)                                         \
  declare_c2_type(ModDNode, Node)                                         \
  declare_c2_type(DivModNode, MultiNode)                                  \
  declare_c2_type(DivModINode, DivModNode)                                \
  declare_c2_type(DivModLNode, DivModNode)                                \
  declare_c2_type(BoxLockNode, Node)                                      \
  declare_c2_type(LoopNode, RegionNode)                                   \
  declare_c2_type(CountedLoopNode, LoopNode)                              \
  declare_c2_type(CountedLoopEndNode, IfNode)                             \
  declare_c2_type(MachNode, Node)                                         \
  declare_c2_type(MachIdealNode, MachNode)                                \
  declare_c2_type(MachTypeNode, MachNode)                                 \
  declare_c2_type(MachBreakpointNode, MachIdealNode)                      \
  declare_c2_type(MachUEPNode, MachIdealNode)                             \
  declare_c2_type(MachPrologNode, MachIdealNode)                          \
  declare_c2_type(MachEpilogNode, MachIdealNode)                          \
  declare_c2_type(MachNopNode, MachIdealNode)                             \
  declare_c2_type(MachSpillCopyNode, MachIdealNode)                       \
  declare_c2_type(MachNullCheckNode, MachIdealNode)                       \
  declare_c2_type(MachProjNode, ProjNode)                                 \
  declare_c2_type(MachIfNode, MachNode)                                   \
  declare_c2_type(MachFastLockNode, MachNode)                             \
  declare_c2_type(MachReturnNode, MachNode)                               \
  declare_c2_type(MachSafePointNode, MachReturnNode)                      \
  declare_c2_type(MachCallNode, MachSafePointNode)                        \
  declare_c2_type(MachCallJavaNode, MachCallNode)                         \
  declare_c2_type(MachCallStaticJavaNode, MachCallJavaNode)               \
  declare_c2_type(MachCallDynamicJavaNode, MachCallJavaNode)              \
  declare_c2_type(MachCallRuntimeNode, MachCallNode)                      \
  declare_c2_type(MachHaltNode, MachReturnNode)                           \
  declare_c2_type(MachTempNode, MachNode)                                 \
  declare_c2_type(MemNode, Node)                                          \
  declare_c2_type(MergeMemNode, Node)                                     \
  declare_c2_type(LoadNode, MemNode)                                      \
  declare_c2_type(LoadBNode, LoadNode)                                    \
  declare_c2_type(LoadUSNode, LoadNode)                                   \
  declare_c2_type(LoadINode, LoadNode)                                    \
  declare_c2_type(LoadRangeNode, LoadINode)                               \
  declare_c2_type(LoadLNode, LoadNode)                                    \
  declare_c2_type(LoadL_unalignedNode, LoadLNode)                         \
  declare_c2_type(LoadFNode, LoadNode)                                    \
  declare_c2_type(LoadDNode, LoadNode)                                    \
  declare_c2_type(LoadD_unalignedNode, LoadDNode)                         \
  declare_c2_type(LoadPNode, LoadNode)                                    \
  declare_c2_type(LoadNNode, LoadNode)                                    \
  declare_c2_type(LoadKlassNode, LoadPNode)                               \
  declare_c2_type(LoadNKlassNode, LoadNNode)                              \
  declare_c2_type(LoadSNode, LoadNode)                                    \
  declare_c2_type(StoreNode, MemNode)                                     \
  declare_c2_type(StoreBNode, StoreNode)                                  \
  declare_c2_type(StoreCNode, StoreNode)                                  \
  declare_c2_type(StoreINode, StoreNode)                                  \
  declare_c2_type(StoreLNode, StoreNode)                                  \
  declare_c2_type(StoreFNode, StoreNode)                                  \
  declare_c2_type(StoreDNode, StoreNode)                                  \
  declare_c2_type(StorePNode, StoreNode)                                  \
  declare_c2_type(StoreNNode, StoreNode)                                  \
  declare_c2_type(StoreNKlassNode, StoreNode)                             \
  declare_c2_type(StoreCMNode, StoreNode)                                 \
  declare_c2_type(LoadPLockedNode, LoadPNode)                             \
  declare_c2_type(SCMemProjNode, ProjNode)                                \
  declare_c2_type(LoadStoreNode, Node)                                    \
  declare_c2_type(StorePConditionalNode, LoadStoreNode)                   \
  declare_c2_type(StoreLConditionalNode, LoadStoreNode)                   \
  declare_c2_type(CompareAndSwapLNode, LoadStoreNode)                     \
  declare_c2_type(CompareAndSwapINode, LoadStoreNode)                     \
  declare_c2_type(CompareAndSwapPNode, LoadStoreNode)                     \
  declare_c2_type(CompareAndSwapNNode, LoadStoreNode)                     \
  declare_c2_type(PrefetchReadNode, Node)                                 \
  declare_c2_type(PrefetchWriteNode, Node)                                \
  declare_c2_type(MulNode, Node)                                          \
  declare_c2_type(MulINode, MulNode)                                      \
  declare_c2_type(MulLNode, MulNode)                                      \
  declare_c2_type(MulFNode, MulNode)                                      \
  declare_c2_type(MulDNode, MulNode)                                      \
  declare_c2_type(MulHiLNode, Node)                                       \
  declare_c2_type(AndINode, MulINode)                                     \
  declare_c2_type(AndLNode, MulLNode)                                     \
  declare_c2_type(LShiftINode, Node)                                      \
  declare_c2_type(LShiftLNode, Node)                                      \
  declare_c2_type(RShiftINode, Node)                                      \
  declare_c2_type(RShiftLNode, Node)                                      \
  declare_c2_type(URShiftINode, Node)                                     \
  declare_c2_type(URShiftLNode, Node)                                     \
  declare_c2_type(MultiNode, Node)                                        \
  declare_c2_type(ProjNode, Node)                                         \
  declare_c2_type(TypeNode, Node)                                         \
  declare_c2_type(NodeHash, StackObj)                                     \
  declare_c2_type(RootNode, LoopNode)                                     \
  declare_c2_type(HaltNode, Node)                                         \
  declare_c2_type(SubNode, Node)                                          \
  declare_c2_type(SubINode, SubNode)                                      \
  declare_c2_type(SubLNode, SubNode)                                      \
  declare_c2_type(SubFPNode, SubNode)                                     \
  declare_c2_type(SubFNode, SubFPNode)                                    \
  declare_c2_type(SubDNode, SubFPNode)                                    \
  declare_c2_type(CmpNode, SubNode)                                       \
  declare_c2_type(CmpINode, CmpNode)                                      \
  declare_c2_type(CmpUNode, CmpNode)                                      \
  declare_c2_type(CmpPNode, CmpNode)                                      \
  declare_c2_type(CmpNNode, CmpNode)                                      \
  declare_c2_type(CmpLNode, CmpNode)                                      \
  declare_c2_type(CmpULNode, CmpNode)                                     \
  declare_c2_type(CmpL3Node, CmpLNode)                                    \
  declare_c2_type(CmpFNode, CmpNode)                                      \
  declare_c2_type(CmpF3Node, CmpFNode)                                    \
  declare_c2_type(CmpDNode, CmpNode)                                      \
  declare_c2_type(CmpD3Node, CmpDNode)                                    \
  declare_c2_type(BoolNode, Node)                                         \
  declare_c2_type(AbsNode, Node)                                          \
  declare_c2_type(AbsINode, AbsNode)                                      \
  declare_c2_type(AbsFNode, AbsNode)                                      \
  declare_c2_type(AbsDNode, AbsNode)                                      \
  declare_c2_type(CmpLTMaskNode, Node)                                    \
  declare_c2_type(NegNode, Node)                                          \
  declare_c2_type(NegFNode, NegNode)                                      \
  declare_c2_type(NegDNode, NegNode)                                      \
  declare_c2_type(CosDNode, Node)                                         \
  declare_c2_type(SinDNode, Node)                                         \
  declare_c2_type(TanDNode, Node)                                         \
  declare_c2_type(AtanDNode, Node)                                        \
  declare_c2_type(SqrtDNode, Node)                                        \
  declare_c2_type(ExpDNode, Node)                                         \
  declare_c2_type(LogDNode, Node)                                         \
  declare_c2_type(Log10DNode, Node)                                       \
  declare_c2_type(PowDNode, Node)                                         \
  declare_c2_type(ReverseBytesINode, Node)                                \
  declare_c2_type(ReverseBytesLNode, Node)                                \
  declare_c2_type(VectorNode, Node)                                       \
  declare_c2_type(AddVBNode, VectorNode)                                  \
  declare_c2_type(AddVSNode, VectorNode)                                  \
  declare_c2_type(AddVINode, VectorNode)                                  \
  declare_c2_type(AddVLNode, VectorNode)                                  \
  declare_c2_type(AddVFNode, VectorNode)                                  \
  declare_c2_type(AddVDNode, VectorNode)                                  \
  declare_c2_type(SubVBNode, VectorNode)                                  \
  declare_c2_type(SubVSNode, VectorNode)                                  \
  declare_c2_type(SubVINode, VectorNode)                                  \
  declare_c2_type(SubVLNode, VectorNode)                                  \
  declare_c2_type(SubVFNode, VectorNode)                                  \
  declare_c2_type(SubVDNode, VectorNode)                                  \
  declare_c2_type(MulVSNode, VectorNode)                                  \
  declare_c2_type(MulVINode, VectorNode)                                  \
  declare_c2_type(MulVFNode, VectorNode)                                  \
  declare_c2_type(MulVDNode, VectorNode)                                  \
  declare_c2_type(DivVFNode, VectorNode)                                  \
  declare_c2_type(DivVDNode, VectorNode)                                  \
  declare_c2_type(LShiftVBNode, VectorNode)                               \
  declare_c2_type(LShiftVSNode, VectorNode)                               \
  declare_c2_type(LShiftVINode, VectorNode)                               \
  declare_c2_type(LShiftVLNode, VectorNode)                               \
  declare_c2_type(RShiftVBNode, VectorNode)                               \
  declare_c2_type(RShiftVSNode, VectorNode)                               \
  declare_c2_type(RShiftVINode, VectorNode)                               \
  declare_c2_type(RShiftVLNode, VectorNode)                               \
  declare_c2_type(URShiftVBNode, VectorNode)                              \
  declare_c2_type(URShiftVSNode, VectorNode)                              \
  declare_c2_type(URShiftVINode, VectorNode)                              \
  declare_c2_type(URShiftVLNode, VectorNode)                              \
  declare_c2_type(AndVNode, VectorNode)                                   \
  declare_c2_type(OrVNode, VectorNode)                                    \
  declare_c2_type(XorVNode, VectorNode)                                   \
  declare_c2_type(LoadVectorNode, LoadNode)                               \
  declare_c2_type(StoreVectorNode, StoreNode)                             \
  declare_c2_type(ReplicateBNode, VectorNode)                             \
  declare_c2_type(ReplicateSNode, VectorNode)                             \
  declare_c2_type(ReplicateINode, VectorNode)                             \
  declare_c2_type(ReplicateLNode, VectorNode)                             \
  declare_c2_type(ReplicateFNode, VectorNode)                             \
  declare_c2_type(ReplicateDNode, VectorNode)                             \
  declare_c2_type(PackNode, VectorNode)                                   \
  declare_c2_type(PackBNode, PackNode)                                    \
  declare_c2_type(PackSNode, PackNode)                                    \
  declare_c2_type(PackINode, PackNode)                                    \
  declare_c2_type(PackLNode, PackNode)                                    \
  declare_c2_type(PackFNode, PackNode)                                    \
  declare_c2_type(PackDNode, PackNode)                                    \
  declare_c2_type(Pack2LNode, PackNode)                                   \
  declare_c2_type(Pack2DNode, PackNode)                                   \
  declare_c2_type(ExtractNode, Node)                                      \
  declare_c2_type(ExtractBNode, ExtractNode)                              \
  declare_c2_type(ExtractUBNode, ExtractNode)                             \
  declare_c2_type(ExtractCNode, ExtractNode)                              \
  declare_c2_type(ExtractSNode, ExtractNode)                              \
  declare_c2_type(ExtractINode, ExtractNode)                              \
  declare_c2_type(ExtractLNode, ExtractNode)                              \
  declare_c2_type(ExtractFNode, ExtractNode)                              \
  declare_c2_type(ExtractDNode, ExtractNode)                              \
  declare_c2_type(OverflowNode, CmpNode)                                  \
  declare_c2_type(OverflowINode, OverflowNode)                            \
  declare_c2_type(OverflowAddINode, OverflowINode)                        \
  declare_c2_type(OverflowSubINode, OverflowINode)                        \
  declare_c2_type(OverflowMulINode, OverflowINode)                        \
  declare_c2_type(OverflowLNode, OverflowNode)                            \
  declare_c2_type(OverflowAddLNode, OverflowLNode)                        \
  declare_c2_type(OverflowSubLNode, OverflowLNode)                        \
  declare_c2_type(OverflowMulLNode, OverflowLNode)                        \
                                                                          \
  /*********************/                                                 \
  /* Adapter Blob Entries */                                              \
  /*********************/                                                 \
  declare_toplevel_type(AdapterHandlerEntry)                              \
  declare_toplevel_type(AdapterHandlerEntry*)                             \
                                                                          \
  /*********************/                                                 \
  /* CI */                                                                \
  /*********************/                                                 \
  declare_toplevel_type(ciEnv)                                            \
  declare_toplevel_type(ciObjectFactory)                                  \
  declare_toplevel_type(ciConstant)                                       \
  declare_toplevel_type(ciField)                                          \
  declare_toplevel_type(ciSymbol)                                         \
  declare_toplevel_type(ciBaseObject)                                     \
  declare_type(ciObject, ciBaseObject)                                    \
  declare_type(ciInstance, ciObject)                                      \
  declare_type(ciMetadata, ciBaseObject)                                  \
  declare_type(ciMethod, ciMetadata)                                      \
  declare_type(ciMethodData, ciMetadata)                                  \
  declare_type(ciType, ciMetadata)                                        \
  declare_type(ciKlass, ciType)                                           \
  declare_type(ciInstanceKlass, ciKlass)                                  \
  declare_type(ciArrayKlass, ciKlass)                                     \
  declare_type(ciTypeArrayKlass, ciArrayKlass)                            \
  declare_type(ciObjArrayKlass, ciArrayKlass)                             \
                                                                          \
  /********************/                                                  \
  /* -XX flags        */                                                  \
  /********************/                                                  \
                                                                          \
  declare_toplevel_type(Flag)                                             \
  declare_toplevel_type(Flag*)                                            \
                                                                          \
  /********************/                                                  \
  /* JVMTI            */                                                  \
  /********************/                                                  \
                                                                          \
  declare_toplevel_type(JvmtiExport)                                      \
                                                                          \
  /********************/                                                  \
  /* JDK/VM version   */                                                  \
  /********************/                                                  \
                                                                          \
  declare_toplevel_type(Abstract_VM_Version)                              \
  declare_toplevel_type(JDK_Version)                                      \
                                                                          \
  /*************/                                                         \
  /* Arguments */                                                         \
  /*************/                                                         \
                                                                          \
  declare_toplevel_type(Arguments)                                        \
                                                                          \
  /***************/                                                       \
  /* Other types */                                                       \
  /***************/                                                       \
                                                                          \
  /* all enum types */                                                    \
                                                                          \
   declare_integer_type(Bytecodes::Code)                                  \
   declare_integer_type(Generation::Name)                                 \
   declare_integer_type(InstanceKlass::ClassState)                        \
   declare_integer_type(JavaThreadState)                                  \
   declare_integer_type(Location::Type)                                   \
   declare_integer_type(Location::Where)                                  \
   declare_integer_type(Flag::Flags)                                      \
   COMPILER2_PRESENT(declare_integer_type(OptoReg::Name))                 \
                                                                          \
   declare_toplevel_type(CHeapObj<mtInternal>)                            \
            declare_type(Array<int>, MetaspaceObj)                        \
            declare_type(Array<u1>, MetaspaceObj)                         \
            declare_type(Array<u2>, MetaspaceObj)                         \
            declare_type(Array<Klass*>, MetaspaceObj)                     \
            declare_type(Array<Method*>, MetaspaceObj)                    \
                                                                          \
   declare_integer_type(AccessFlags)  /* FIXME: wrong type (not integer) */\
  declare_toplevel_type(address)      /* FIXME: should this be an integer type? */\
   declare_integer_type(BasicType)   /* FIXME: wrong type (not integer) */\
  declare_toplevel_type(BreakpointInfo)                                   \
  declare_toplevel_type(BreakpointInfo*)                                  \
  declare_toplevel_type(CodeBlob*)                                        \
  declare_toplevel_type(CompressedWriteStream*)                           \
  declare_toplevel_type(ConstantPoolCacheEntry)                           \
  declare_toplevel_type(elapsedTimer)                                     \
  declare_toplevel_type(frame)                                            \
  declare_toplevel_type(intptr_t*)                                        \
   declare_unsigned_integer_type(InvocationCounter) /* FIXME: wrong type (not integer) */ \
  declare_toplevel_type(JavaThread*)                                      \
  declare_toplevel_type(java_lang_Class)                                  \
  declare_integer_type(JavaThread::AsyncRequests)                         \
  declare_toplevel_type(jbyte*)                                           \
  declare_toplevel_type(jbyte**)                                          \
  declare_toplevel_type(jint*)                                            \
  declare_toplevel_type(jniIdMapBase*)                                    \
  declare_unsigned_integer_type(juint)                                    \
  declare_unsigned_integer_type(julong)                                   \
  declare_toplevel_type(JNIHandleBlock*)                                  \
  declare_toplevel_type(JNIid)                                            \
  declare_toplevel_type(JNIid*)                                           \
  declare_toplevel_type(jmethodID*)                                       \
  declare_toplevel_type(Mutex*)                                           \
  declare_toplevel_type(nmethod*)                                         \
  COMPILER2_PRESENT(declare_unsigned_integer_type(node_idx_t))            \
  declare_toplevel_type(ObjectMonitor*)                                   \
  declare_toplevel_type(oop*)                                             \
  declare_toplevel_type(OopMap**)                                         \
  declare_toplevel_type(OopMapCache*)                                     \
  declare_toplevel_type(OopMapSet*)                                       \
  declare_toplevel_type(VMReg)                                            \
  declare_toplevel_type(OSThread*)                                        \
   declare_integer_type(ReferenceType)                                    \
  declare_toplevel_type(StubQueue*)                                       \
  declare_toplevel_type(Thread*)                                          \
  declare_toplevel_type(Universe)                                         \
  declare_toplevel_type(os)                                               \
  declare_toplevel_type(vframeArray)                                      \
  declare_toplevel_type(vframeArrayElement)                               \
  declare_toplevel_type(Annotations*)                                     \
                                                                          \
  /***************/                                                       \
  /* Miscellaneous types */                                               \
  /***************/                                                       \
                                                                          \
  declare_toplevel_type(PtrQueue)                                         \
                                                                          \
  /* freelist */                                                          \
  declare_toplevel_type(FreeChunk*)                                       \
  declare_toplevel_type(AdaptiveFreeList<FreeChunk>*)                     \
  declare_toplevel_type(AdaptiveFreeList<FreeChunk>)


//--------------------------------------------------------------------------------
// VM_INT_CONSTANTS
//
// This table contains integer constants required over in the
// serviceability agent. The "declare_constant" macro is used for all
// enums, etc., while "declare_preprocessor_constant" must be used for
// all #defined constants.

#define VM_INT_CONSTANTS(declare_constant,                                \
                         declare_preprocessor_constant,                   \
                         declare_c1_constant,                             \
                         declare_c2_constant,                             \
                         declare_c2_preprocessor_constant)                \
                                                                          \
  /******************/                                                    \
  /* Useful globals */                                                    \
  /******************/                                                    \
                                                                          \
  declare_preprocessor_constant("ASSERT", DEBUG_ONLY(1) NOT_DEBUG(0))     \
                                                                          \
  /**************/                                                        \
  /* Stack bias */                                                        \
  /**************/                                                        \
                                                                          \
  declare_preprocessor_constant("STACK_BIAS", STACK_BIAS)                 \
                                                                          \
  /****************/                                                      \
  /* Object sizes */                                                      \
  /****************/                                                      \
                                                                          \
  declare_constant(oopSize)                                               \
  declare_constant(LogBytesPerWord)                                       \
  declare_constant(BytesPerWord)                                          \
  declare_constant(BytesPerLong)                                          \
                                                                          \
  declare_constant(LogKlassAlignmentInBytes)                              \
                                                                          \
  /********************************************/                          \
  /* Generation and Space Hierarchy Constants */                          \
  /********************************************/                          \
                                                                          \
  declare_constant(ageTable::table_size)                                  \
                                                                          \
  declare_constant(BarrierSet::ModRef)                                    \
  declare_constant(BarrierSet::CardTableModRef)                           \
  declare_constant(BarrierSet::CardTableExtension)                        \
  declare_constant(BarrierSet::G1SATBCT)                                  \
  declare_constant(BarrierSet::G1SATBCTLogging)                           \
  declare_constant(BarrierSet::Other)                                     \
                                                                          \
  declare_constant(BlockOffsetSharedArray::LogN)                          \
  declare_constant(BlockOffsetSharedArray::LogN_words)                    \
  declare_constant(BlockOffsetSharedArray::N_bytes)                       \
  declare_constant(BlockOffsetSharedArray::N_words)                       \
                                                                          \
  declare_constant(BlockOffsetArray::N_words)                             \
                                                                          \
  declare_constant(CardTableModRefBS::clean_card)                         \
  declare_constant(CardTableModRefBS::last_card)                          \
  declare_constant(CardTableModRefBS::dirty_card)                         \
  declare_constant(CardTableModRefBS::Precise)                            \
  declare_constant(CardTableModRefBS::ObjHeadPreciseArray)                \
  declare_constant(CardTableModRefBS::card_shift)                         \
  declare_constant(CardTableModRefBS::card_size)                          \
  declare_constant(CardTableModRefBS::card_size_in_words)                 \
                                                                          \
  declare_constant(CardTableRS::youngergen_card)                          \
                                                                          \
  declare_constant(CollectedHeap::Abstract)                               \
  declare_constant(CollectedHeap::SharedHeap)                             \
  declare_constant(CollectedHeap::GenCollectedHeap)                       \
                                                                          \
  declare_constant(GenCollectedHeap::max_gens)                            \
                                                                          \
  /* constants from Generation::Name enum */                              \
                                                                          \
  declare_constant(Generation::DefNew)                                    \
  declare_constant(Generation::MarkSweepCompact)                          \
  declare_constant(Generation::Other)                                     \
                                                                          \
  declare_constant(Generation::LogOfGenGrain)                             \
  declare_constant(Generation::GenGrain)                                  \
                                                                          \
  declare_constant(HeapWordSize)                                          \
  declare_constant(LogHeapWordSize)                                       \
                                                                          \
                                                                          \
  /************************/                                              \
  /* PerfMemory - jvmstat */                                              \
  /************************/                                              \
                                                                          \
  declare_preprocessor_constant("PERFDATA_MAJOR_VERSION", PERFDATA_MAJOR_VERSION) \
  declare_preprocessor_constant("PERFDATA_MINOR_VERSION", PERFDATA_MINOR_VERSION) \
  declare_preprocessor_constant("PERFDATA_BIG_ENDIAN", PERFDATA_BIG_ENDIAN)       \
  declare_preprocessor_constant("PERFDATA_LITTLE_ENDIAN", PERFDATA_LITTLE_ENDIAN) \
                                                                          \
  /***********************************/                                   \
  /* LoaderConstraintTable constants */                                   \
  /***********************************/                                   \
                                                                          \
  declare_constant(LoaderConstraintTable::_loader_constraint_size)        \
  declare_constant(LoaderConstraintTable::_nof_buckets)                   \
                                                                          \
  /************************************************************/          \
  /* HotSpot specific JVM_ACC constants from global anon enum */          \
  /************************************************************/          \
                                                                          \
  declare_constant(JVM_ACC_WRITTEN_FLAGS)                                 \
  declare_constant(JVM_ACC_MONITOR_MATCH)                                 \
  declare_constant(JVM_ACC_HAS_MONITOR_BYTECODES)                         \
  declare_constant(JVM_ACC_HAS_LOOPS)                                     \
  declare_constant(JVM_ACC_LOOPS_FLAG_INIT)                               \
  declare_constant(JVM_ACC_QUEUED)                                        \
  declare_constant(JVM_ACC_NOT_C2_OSR_COMPILABLE)                         \
  declare_constant(JVM_ACC_HAS_LINE_NUMBER_TABLE)                         \
  declare_constant(JVM_ACC_HAS_CHECKED_EXCEPTIONS)                        \
  declare_constant(JVM_ACC_HAS_JSRS)                                      \
  declare_constant(JVM_ACC_IS_OLD)                                        \
  declare_constant(JVM_ACC_IS_OBSOLETE)                                   \
  declare_constant(JVM_ACC_IS_PREFIXED_NATIVE)                            \
  declare_constant(JVM_ACC_HAS_MIRANDA_METHODS)                           \
  declare_constant(JVM_ACC_HAS_VANILLA_CONSTRUCTOR)                       \
  declare_constant(JVM_ACC_HAS_FINALIZER)                                 \
  declare_constant(JVM_ACC_IS_CLONEABLE)                                  \
  declare_constant(JVM_ACC_HAS_LOCAL_VARIABLE_TABLE)                      \
  declare_constant(JVM_ACC_PROMOTED_FLAGS)                                \
  declare_constant(JVM_ACC_FIELD_ACCESS_WATCHED)                          \
  declare_constant(JVM_ACC_FIELD_MODIFICATION_WATCHED)                    \
                                                                          \
  /*****************************/                                         \
  /* Thread::SuspendFlags enum */                                         \
  /*****************************/                                         \
                                                                          \
  declare_constant(Thread::_external_suspend)                             \
  declare_constant(Thread::_ext_suspended)                                \
  declare_constant(Thread::_has_async_exception)                          \
                                                                          \
  /*******************/                                                   \
  /* JavaThreadState */                                                   \
  /*******************/                                                   \
                                                                          \
  declare_constant(_thread_uninitialized)                                 \
  declare_constant(_thread_new)                                           \
  declare_constant(_thread_new_trans)                                     \
  declare_constant(_thread_in_native)                                     \
  declare_constant(_thread_in_native_trans)                               \
  declare_constant(_thread_in_vm)                                         \
  declare_constant(_thread_in_vm_trans)                                   \
  declare_constant(_thread_in_Java)                                       \
  declare_constant(_thread_in_Java_trans)                                 \
  declare_constant(_thread_blocked)                                       \
  declare_constant(_thread_blocked_trans)                                 \
                                                                          \
  /******************************/                                        \
  /* Klass misc. enum constants */                                        \
  /******************************/                                        \
                                                                          \
  declare_constant(Klass::_primary_super_limit)                           \
  declare_constant(Klass::_lh_instance_slow_path_bit)                     \
  declare_constant(Klass::_lh_log2_element_size_shift)                    \
  declare_constant(Klass::_lh_log2_element_size_mask)                     \
  declare_constant(Klass::_lh_element_type_shift)                         \
  declare_constant(Klass::_lh_element_type_mask)                          \
  declare_constant(Klass::_lh_header_size_shift)                          \
  declare_constant(Klass::_lh_header_size_mask)                           \
  declare_constant(Klass::_lh_array_tag_shift)                            \
  declare_constant(Klass::_lh_array_tag_type_value)                       \
  declare_constant(Klass::_lh_array_tag_obj_value)                        \
                                                                          \
  /********************************/                                      \
  /* ConstMethod anon-enum */                                             \
  /********************************/                                      \
                                                                          \
  declare_constant(ConstMethod::_has_linenumber_table)                    \
  declare_constant(ConstMethod::_has_checked_exceptions)                  \
  declare_constant(ConstMethod::_has_localvariable_table)                 \
  declare_constant(ConstMethod::_has_exception_table)                     \
  declare_constant(ConstMethod::_has_generic_signature)                   \
  declare_constant(ConstMethod::_has_method_parameters)                   \
  declare_constant(ConstMethod::_has_method_annotations)                  \
  declare_constant(ConstMethod::_has_parameter_annotations)               \
  declare_constant(ConstMethod::_has_default_annotations)                 \
  declare_constant(ConstMethod::_has_type_annotations)                    \
                                                                          \
  /**************/                                                        \
  /* DataLayout */                                                        \
  /**************/                                                        \
                                                                          \
  declare_constant(DataLayout::cell_size)                                 \
                                                                          \
  /*************************************/                                 \
  /* InstanceKlass enum                */                                 \
  /*************************************/                                 \
                                                                          \
                                                                          \
  /*************************************/                                 \
  /* FieldInfo FieldOffset enum        */                                 \
  /*************************************/                                 \
                                                                          \
  declare_constant(FieldInfo::access_flags_offset)                        \
  declare_constant(FieldInfo::name_index_offset)                          \
  declare_constant(FieldInfo::signature_index_offset)                     \
  declare_constant(FieldInfo::initval_index_offset)                       \
  declare_constant(FieldInfo::low_packed_offset)                          \
  declare_constant(FieldInfo::high_packed_offset)                         \
  declare_constant(FieldInfo::field_slots)                                \
                                                                          \
  /*************************************/                                 \
  /* FieldInfo tag constants           */                                 \
  /*************************************/                                 \
                                                                          \
  declare_preprocessor_constant("FIELDINFO_TAG_SIZE", FIELDINFO_TAG_SIZE) \
  declare_preprocessor_constant("FIELDINFO_TAG_MASK", FIELDINFO_TAG_MASK) \
  declare_preprocessor_constant("FIELDINFO_TAG_OFFSET", FIELDINFO_TAG_OFFSET) \
                                                                          \
  /************************************************/                      \
  /* InstanceKlass InnerClassAttributeOffset enum */                      \
  /************************************************/                      \
                                                                          \
  declare_constant(InstanceKlass::inner_class_inner_class_info_offset)    \
  declare_constant(InstanceKlass::inner_class_outer_class_info_offset)    \
  declare_constant(InstanceKlass::inner_class_inner_name_offset)          \
  declare_constant(InstanceKlass::inner_class_access_flags_offset)        \
  declare_constant(InstanceKlass::inner_class_next_offset)                \
                                                                          \
  /*********************************/                                     \
  /* InstanceKlass ClassState enum */                                     \
  /*********************************/                                     \
                                                                          \
  declare_constant(InstanceKlass::allocated)                              \
  declare_constant(InstanceKlass::loaded)                                 \
  declare_constant(InstanceKlass::linked)                                 \
  declare_constant(InstanceKlass::being_initialized)                      \
  declare_constant(InstanceKlass::fully_initialized)                      \
  declare_constant(InstanceKlass::initialization_error)                   \
                                                                          \
  /*********************************/                                     \
  /* Symbol* - symbol max length */                                       \
  /*********************************/                                     \
                                                                          \
  declare_constant(Symbol::max_symbol_length)                             \
                                                                          \
  /*************************************************/                     \
  /* ConstantPool* layout enum for InvokeDynamic */                     \
  /*************************************************/                     \
                                                                          \
  declare_constant(ConstantPool::_indy_bsm_offset)                 \
  declare_constant(ConstantPool::_indy_argc_offset)                \
  declare_constant(ConstantPool::_indy_argv_offset)                \
                                                                          \
  /********************************/                                      \
  /* ConstantPoolCacheEntry enums */                                      \
  /********************************/                                      \
                                                                          \
  declare_constant(ConstantPoolCacheEntry::is_volatile_shift)             \
  declare_constant(ConstantPoolCacheEntry::is_final_shift)                \
  declare_constant(ConstantPoolCacheEntry::is_forced_virtual_shift)       \
  declare_constant(ConstantPoolCacheEntry::is_vfinal_shift)               \
  declare_constant(ConstantPoolCacheEntry::is_field_entry_shift)          \
  declare_constant(ConstantPoolCacheEntry::tos_state_shift)               \
                                                                          \
  /***************************************/                               \
  /* java_lang_Thread::ThreadStatus enum */                               \
  /***************************************/                               \
                                                                          \
  declare_constant(java_lang_Thread::NEW)                                 \
  declare_constant(java_lang_Thread::RUNNABLE)                            \
  declare_constant(java_lang_Thread::SLEEPING)                            \
  declare_constant(java_lang_Thread::IN_OBJECT_WAIT)                      \
  declare_constant(java_lang_Thread::IN_OBJECT_WAIT_TIMED)                \
  declare_constant(java_lang_Thread::PARKED)                              \
  declare_constant(java_lang_Thread::PARKED_TIMED)                        \
  declare_constant(java_lang_Thread::BLOCKED_ON_MONITOR_ENTER)            \
  declare_constant(java_lang_Thread::TERMINATED)                          \
                                                                          \
  /******************************/                                        \
  /* Debug info                 */                                        \
  /******************************/                                        \
                                                                          \
  declare_constant(Location::OFFSET_MASK)                                 \
  declare_constant(Location::OFFSET_SHIFT)                                \
  declare_constant(Location::TYPE_MASK)                                   \
  declare_constant(Location::TYPE_SHIFT)                                  \
  declare_constant(Location::WHERE_MASK)                                  \
  declare_constant(Location::WHERE_SHIFT)                                 \
                                                                          \
  /* constants from Location::Type enum  */                               \
                                                                          \
  declare_constant(Location::normal)                                      \
  declare_constant(Location::oop)                                         \
  declare_constant(Location::narrowoop)                                   \
  declare_constant(Location::int_in_long)                                 \
  declare_constant(Location::lng)                                         \
  declare_constant(Location::float_in_dbl)                                \
  declare_constant(Location::dbl)                                         \
  declare_constant(Location::addr)                                        \
  declare_constant(Location::invalid)                                     \
                                                                          \
  /* constants from Location::Where enum */                               \
                                                                          \
  declare_constant(Location::on_stack)                                    \
  declare_constant(Location::in_register)                                 \
                                                                          \
  declare_constant(Deoptimization::Reason_many)                           \
  declare_constant(Deoptimization::Reason_none)                           \
  declare_constant(Deoptimization::Reason_null_check)                     \
  declare_constant(Deoptimization::Reason_null_assert)                    \
  declare_constant(Deoptimization::Reason_range_check)                    \
  declare_constant(Deoptimization::Reason_class_check)                    \
  declare_constant(Deoptimization::Reason_array_check)                    \
  declare_constant(Deoptimization::Reason_intrinsic)                      \
  declare_constant(Deoptimization::Reason_bimorphic)                      \
  declare_constant(Deoptimization::Reason_unloaded)                       \
  declare_constant(Deoptimization::Reason_uninitialized)                  \
  declare_constant(Deoptimization::Reason_unreached)                      \
  declare_constant(Deoptimization::Reason_unhandled)                      \
  declare_constant(Deoptimization::Reason_constraint)                     \
  declare_constant(Deoptimization::Reason_div0_check)                     \
  declare_constant(Deoptimization::Reason_age)                            \
  declare_constant(Deoptimization::Reason_predicate)                      \
  declare_constant(Deoptimization::Reason_loop_limit_check)               \
  declare_constant(Deoptimization::Reason_unstable_if)                    \
  declare_constant(Deoptimization::Reason_LIMIT)                          \
  declare_constant(Deoptimization::Reason_RECORDED_LIMIT)                 \
                                                                          \
  declare_constant(Deoptimization::Action_none)                           \
  declare_constant(Deoptimization::Action_maybe_recompile)                \
  declare_constant(Deoptimization::Action_reinterpret)                    \
  declare_constant(Deoptimization::Action_make_not_entrant)               \
  declare_constant(Deoptimization::Action_make_not_compilable)            \
  declare_constant(Deoptimization::Action_LIMIT)                          \
                                                                          \
  /*********************/                                                 \
  /* Matcher (C2 only) */                                                 \
  /*********************/                                                 \
                                                                          \
  declare_c2_preprocessor_constant("Matcher::interpreter_frame_pointer_reg", Matcher::interpreter_frame_pointer_reg()) \
                                                                          \
  /*********************************************/                         \
  /* MethodCompilation (globalDefinitions.hpp) */                         \
  /*********************************************/                         \
                                                                          \
  declare_constant(InvocationEntryBci)                                    \
  declare_constant(InvalidOSREntryBci)                                    \
                                                                          \
  /***************/                                                       \
  /* OopMapValue */                                                       \
  /***************/                                                       \
                                                                          \
  declare_constant(OopMapValue::type_bits)                                \
  declare_constant(OopMapValue::register_bits)                            \
  declare_constant(OopMapValue::type_shift)                               \
  declare_constant(OopMapValue::register_shift)                           \
  declare_constant(OopMapValue::type_mask)                                \
  declare_constant(OopMapValue::type_mask_in_place)                       \
  declare_constant(OopMapValue::register_mask)                            \
  declare_constant(OopMapValue::register_mask_in_place)                   \
  declare_constant(OopMapValue::unused_value)                             \
  declare_constant(OopMapValue::oop_value)                                \
  declare_constant(OopMapValue::value_value)                              \
  declare_constant(OopMapValue::narrowoop_value)                          \
  declare_constant(OopMapValue::callee_saved_value)                       \
  declare_constant(OopMapValue::derived_oop_value)                        \
                                                                          \
  /******************/                                                    \
  /* JNIHandleBlock */                                                    \
  /******************/                                                    \
                                                                          \
  declare_constant(JNIHandleBlock::block_size_in_oops)                    \
                                                                          \
  /**********************/                                                \
  /* ObjectSynchronizer */                                                \
  /**********************/                                                \
                                                                          \
  declare_constant(ObjectSynchronizer::_BLOCKSIZE)                        \
                                                                          \
  /**********************/                                                \
  /* PcDesc             */                                                \
  /**********************/                                                \
                                                                          \
  declare_constant(PcDesc::PCDESC_reexecute)                              \
  declare_constant(PcDesc::PCDESC_is_method_handle_invoke)                \
  declare_constant(PcDesc::PCDESC_return_oop)                             \
                                                                          \
  /**********************/                                                \
  /* frame              */                                                \
  /**********************/                                                \
                                                                          \
  NOT_ZERO(X86_ONLY(declare_constant(frame::entry_frame_call_wrapper_offset)))      \
  declare_constant(frame::pc_return_offset)                               \
                                                                          \
  /*************/                                                         \
  /* vmSymbols */                                                         \
  /*************/                                                         \
                                                                          \
  declare_constant(vmSymbols::FIRST_SID)                                  \
  declare_constant(vmSymbols::SID_LIMIT)                                  \
                                                                          \
  /****************/                                                      \
  /* vmIntrinsics */                                                      \
  /****************/                                                      \
                                                                          \
  declare_constant(vmIntrinsics::_invokeBasic)                            \
  declare_constant(vmIntrinsics::_linkToVirtual)                          \
  declare_constant(vmIntrinsics::_linkToStatic)                           \
  declare_constant(vmIntrinsics::_linkToSpecial)                          \
  declare_constant(vmIntrinsics::_linkToInterface)                        \
                                                                          \
  /********************************/                                      \
  /* Calling convention constants */                                      \
  /********************************/                                      \
                                                                          \
  declare_constant(RegisterImpl::number_of_registers)                     \
  declare_constant(ConcreteRegisterImpl::number_of_registers)             \
  declare_preprocessor_constant("REG_COUNT", REG_COUNT)                \
  declare_c2_preprocessor_constant("SAVED_ON_ENTRY_REG_COUNT", SAVED_ON_ENTRY_REG_COUNT) \
  declare_c2_preprocessor_constant("C_SAVED_ON_ENTRY_REG_COUNT", C_SAVED_ON_ENTRY_REG_COUNT)


//--------------------------------------------------------------------------------
// VM_LONG_CONSTANTS
//
// This table contains long constants required over in the
// serviceability agent. The "declare_constant" macro is used for all
// enums, etc., while "declare_preprocessor_constant" must be used for
// all #defined constants.

#define VM_LONG_CONSTANTS(declare_constant, declare_preprocessor_constant, declare_c1_constant, declare_c2_constant, declare_c2_preprocessor_constant) \
                                                                          \
  /*********************/                                                 \
  /* MarkOop constants */                                                 \
  /*********************/                                                 \
                                                                          \
  /* Note: some of these are declared as long constants just for */       \
  /* consistency. The mask constants are the only ones requiring */       \
  /* 64 bits (on 64-bit platforms). */                                    \
                                                                          \
  declare_constant(markOopDesc::age_bits)                                 \
  declare_constant(markOopDesc::lock_bits)                                \
  declare_constant(markOopDesc::biased_lock_bits)                         \
  declare_constant(markOopDesc::max_hash_bits)                            \
  declare_constant(markOopDesc::hash_bits)                                \
                                                                          \
  declare_constant(markOopDesc::lock_shift)                               \
  declare_constant(markOopDesc::biased_lock_shift)                        \
  declare_constant(markOopDesc::age_shift)                                \
  declare_constant(markOopDesc::hash_shift)                               \
                                                                          \
  declare_constant(markOopDesc::lock_mask)                                \
  declare_constant(markOopDesc::lock_mask_in_place)                       \
  declare_constant(markOopDesc::biased_lock_mask)                         \
  declare_constant(markOopDesc::biased_lock_mask_in_place)                \
  declare_constant(markOopDesc::biased_lock_bit_in_place)                 \
  declare_constant(markOopDesc::age_mask)                                 \
  declare_constant(markOopDesc::age_mask_in_place)                        \
  declare_constant(markOopDesc::epoch_mask)                               \
  declare_constant(markOopDesc::epoch_mask_in_place)                      \
  declare_constant(markOopDesc::hash_mask)                                \
  declare_constant(markOopDesc::hash_mask_in_place)                       \
  declare_constant(markOopDesc::biased_lock_alignment)                    \
                                                                          \
  declare_constant(markOopDesc::locked_value)                             \
  declare_constant(markOopDesc::unlocked_value)                           \
  declare_constant(markOopDesc::monitor_value)                            \
  declare_constant(markOopDesc::marked_value)                             \
  declare_constant(markOopDesc::biased_lock_pattern)                      \
                                                                          \
  declare_constant(markOopDesc::no_hash)                                  \
  declare_constant(markOopDesc::no_hash_in_place)                         \
  declare_constant(markOopDesc::no_lock_in_place)                         \
  declare_constant(markOopDesc::max_age)                                  \
                                                                          \
  /* Constants in markOop used by CMS. */                                 \
  declare_constant(markOopDesc::cms_shift)                                \
  declare_constant(markOopDesc::cms_mask)                                 \
  declare_constant(markOopDesc::size_shift)


//--------------------------------------------------------------------------------
// Macros operating on the above lists
//--------------------------------------------------------------------------------

// This utility macro quotes the passed string
#define QUOTE(x) #x

//--------------------------------------------------------------------------------
// VMStructEntry macros
//

// This macro generates a VMStructEntry line for a nonstatic field
#define GENERATE_NONSTATIC_VM_STRUCT_ENTRY(typeName, fieldName, type)              \
 { QUOTE(typeName), QUOTE(fieldName), QUOTE(type), 0, cast_uint64_t(offset_of(typeName, fieldName)), NULL },

// This macro generates a VMStructEntry line for a static field
#define GENERATE_STATIC_VM_STRUCT_ENTRY(typeName, fieldName, type)                 \
 { QUOTE(typeName), QUOTE(fieldName), QUOTE(type), 1, 0, &typeName::fieldName },

// This macro generates a VMStructEntry line for a static pointer volatile field,
// e.g.: "static ObjectMonitor * volatile gBlockList;"
#define GENERATE_STATIC_PTR_VOLATILE_VM_STRUCT_ENTRY(typeName, fieldName, type)    \
 { QUOTE(typeName), QUOTE(fieldName), QUOTE(type), 1, 0, (void*)&typeName::fieldName },

// This macro generates a VMStructEntry line for an unchecked
// nonstatic field, in which the size of the type is also specified.
// The type string is given as NULL, indicating an "opaque" type.
#define GENERATE_UNCHECKED_NONSTATIC_VM_STRUCT_ENTRY(typeName, fieldName, size)    \
  { QUOTE(typeName), QUOTE(fieldName), NULL, 0, cast_uint64_t(offset_of(typeName, fieldName)), NULL },

// This macro generates a VMStructEntry line for an unchecked
// static field, in which the size of the type is also specified.
// The type string is given as NULL, indicating an "opaque" type.
#define GENERATE_UNCHECKED_STATIC_VM_STRUCT_ENTRY(typeName, fieldName, size)       \
 { QUOTE(typeName), QUOTE(fieldName), NULL, 1, 0, (void*) &typeName::fieldName },

// This macro generates the sentinel value indicating the end of the list
#define GENERATE_VM_STRUCT_LAST_ENTRY() \
 { NULL, NULL, NULL, 0, 0, NULL }

// This macro checks the type of a VMStructEntry by comparing pointer types
#define CHECK_NONSTATIC_VM_STRUCT_ENTRY(typeName, fieldName, type)                 \
 {typeName *dummyObj = NULL; type* dummy = &dummyObj->fieldName;                   \
  assert(offset_of(typeName, fieldName) < sizeof(typeName), "Illegal nonstatic struct entry, field offset too large"); }

// This macro checks the type of a volatile VMStructEntry by comparing pointer types
#define CHECK_VOLATILE_NONSTATIC_VM_STRUCT_ENTRY(typeName, fieldName, type)        \
 {typedef type dummyvtype; typeName *dummyObj = NULL; volatile dummyvtype* dummy = &dummyObj->fieldName; }

// This macro checks the type of a static VMStructEntry by comparing pointer types
#define CHECK_STATIC_VM_STRUCT_ENTRY(typeName, fieldName, type)                    \
 {type* dummy = &typeName::fieldName; }

// This macro checks the type of a static pointer volatile VMStructEntry by comparing pointer types,
// e.g.: "static ObjectMonitor * volatile gBlockList;"
#define CHECK_STATIC_PTR_VOLATILE_VM_STRUCT_ENTRY(typeName, fieldName, type)       \
 {type volatile * dummy = &typeName::fieldName; }

// This macro ensures the type of a field and its containing type are
// present in the type table. The assertion string is shorter than
// preferable because (incredibly) of a bug in Solstice NFS client
// which seems to prevent very long lines from compiling. This assertion
// means that an entry in VMStructs::localHotSpotVMStructs[] was not
// found in VMStructs::localHotSpotVMTypes[].
#define ENSURE_FIELD_TYPE_PRESENT(typeName, fieldName, type)                       \
 { assert(findType(QUOTE(typeName)) != 0, "type \"" QUOTE(typeName) "\" not found in type table"); \
   assert(findType(QUOTE(type)) != 0, "type \"" QUOTE(type) "\" not found in type table"); }

// This is a no-op macro for unchecked fields
#define CHECK_NO_OP(a, b, c)

//
// Build-specific macros:
//

// Generate and check a nonstatic field in non-product builds
#ifndef PRODUCT
# define GENERATE_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY(a, b, c) GENERATE_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define CHECK_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)    CHECK_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define ENSURE_NONPRODUCT_FIELD_TYPE_PRESENT(a, b, c)          ENSURE_FIELD_TYPE_PRESENT(a, b, c)
# define GENERATE_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY(a, b, c) GENERATE_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define CHECK_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)    CHECK_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define ENSURE_NONPRODUCT_FIELD_TYPE_PRESENT(a, b, c)          ENSURE_FIELD_TYPE_PRESENT(a, b, c)
#else
# define GENERATE_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define CHECK_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define ENSURE_NONPRODUCT_FIELD_TYPE_PRESENT(a, b, c)
# define GENERATE_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define CHECK_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define ENSURE_NONPRODUCT_FIELD_TYPE_PRESENT(a, b, c)
#endif /* PRODUCT */

// Generate and check a nonstatic field in C1 builds
#ifdef COMPILER1
# define GENERATE_C1_NONSTATIC_VM_STRUCT_ENTRY(a, b, c) GENERATE_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define CHECK_C1_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)    CHECK_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define ENSURE_C1_FIELD_TYPE_PRESENT(a, b, c)          ENSURE_FIELD_TYPE_PRESENT(a, b, c)
#else
# define GENERATE_C1_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define CHECK_C1_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define ENSURE_C1_FIELD_TYPE_PRESENT(a, b, c)
#endif /* COMPILER1 */
// Generate and check a nonstatic field in C2 builds
#ifdef COMPILER2
# define GENERATE_C2_NONSTATIC_VM_STRUCT_ENTRY(a, b, c) GENERATE_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define CHECK_C2_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)    CHECK_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define ENSURE_C2_FIELD_TYPE_PRESENT(a, b, c)          ENSURE_FIELD_TYPE_PRESENT(a, b, c)
#else
# define GENERATE_C2_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define CHECK_C2_NONSTATIC_VM_STRUCT_ENTRY(a, b, c)
# define ENSURE_C2_FIELD_TYPE_PRESENT(a, b, c)
#endif /* COMPILER2 */

// Generate but do not check a static field in C1 builds
#ifdef COMPILER1
# define GENERATE_C1_UNCHECKED_STATIC_VM_STRUCT_ENTRY(a, b, c) GENERATE_UNCHECKED_STATIC_VM_STRUCT_ENTRY(a, b, c)
#else
# define GENERATE_C1_UNCHECKED_STATIC_VM_STRUCT_ENTRY(a, b, c)
#endif /* COMPILER1 */

// Generate but do not check a static field in C2 builds
#ifdef COMPILER2
# define GENERATE_C2_UNCHECKED_STATIC_VM_STRUCT_ENTRY(a, b, c) GENERATE_UNCHECKED_STATIC_VM_STRUCT_ENTRY(a, b, c)
#else
# define GENERATE_C2_UNCHECKED_STATIC_VM_STRUCT_ENTRY(a, b, c)
#endif /* COMPILER2 */

//--------------------------------------------------------------------------------
// VMTypeEntry macros
//

#define GENERATE_VM_TYPE_ENTRY(type, superclass) \
 { QUOTE(type), QUOTE(superclass), 0, 0, 0, sizeof(type) },

#define GENERATE_TOPLEVEL_VM_TYPE_ENTRY(type) \
 { QUOTE(type), NULL,              0, 0, 0, sizeof(type) },

#define GENERATE_OOP_VM_TYPE_ENTRY(type) \
 { QUOTE(type), NULL,              1, 0, 0, sizeof(type) },

#define GENERATE_INTEGER_VM_TYPE_ENTRY(type) \
 { QUOTE(type), NULL,              0, 1, 0, sizeof(type) },

#define GENERATE_UNSIGNED_INTEGER_VM_TYPE_ENTRY(type) \
 { QUOTE(type), NULL,              0, 1, 1, sizeof(type) },

#define GENERATE_VM_TYPE_LAST_ENTRY() \
 { NULL, NULL, 0, 0, 0, 0 }

#define CHECK_VM_TYPE_ENTRY(type, superclass) \
 { type* dummyObj = NULL; superclass* dummySuperObj = dummyObj; }

#define CHECK_VM_TYPE_NO_OP(a)
#define CHECK_SINGLE_ARG_VM_TYPE_NO_OP(a)

//
// Build-specific macros:
//

#ifdef COMPILER1
# define GENERATE_C1_TOPLEVEL_VM_TYPE_ENTRY(a)               GENERATE_TOPLEVEL_VM_TYPE_ENTRY(a)
# define CHECK_C1_TOPLEVEL_VM_TYPE_ENTRY(a)
#else
# define GENERATE_C1_TOPLEVEL_VM_TYPE_ENTRY(a)
# define CHECK_C1_TOPLEVEL_VM_TYPE_ENTRY(a)
#endif /* COMPILER1 */

#ifdef COMPILER2
# define GENERATE_C2_VM_TYPE_ENTRY(a, b)                     GENERATE_VM_TYPE_ENTRY(a, b)
# define CHECK_C2_VM_TYPE_ENTRY(a, b)                        CHECK_VM_TYPE_ENTRY(a, b)
# define GENERATE_C2_TOPLEVEL_VM_TYPE_ENTRY(a)               GENERATE_TOPLEVEL_VM_TYPE_ENTRY(a)
# define CHECK_C2_TOPLEVEL_VM_TYPE_ENTRY(a)
#else
# define GENERATE_C2_VM_TYPE_ENTRY(a, b)
# define CHECK_C2_VM_TYPE_ENTRY(a, b)
# define GENERATE_C2_TOPLEVEL_VM_TYPE_ENTRY(a)
# define CHECK_C2_TOPLEVEL_VM_TYPE_ENTRY(a)
#endif /* COMPILER2 */


//--------------------------------------------------------------------------------
// VMIntConstantEntry macros
//

#define GENERATE_VM_INT_CONSTANT_ENTRY(name) \
 { QUOTE(name), (int32_t) name },

#define GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY(name, value) \
 { name, (int32_t) value },

// This macro generates the sentinel value indicating the end of the list
#define GENERATE_VM_INT_CONSTANT_LAST_ENTRY() \
 { NULL, 0 }


// Generate an int constant for a C1 build
#ifdef COMPILER1
# define GENERATE_C1_VM_INT_CONSTANT_ENTRY(name)  GENERATE_VM_INT_CONSTANT_ENTRY(name)
#else
# define GENERATE_C1_VM_INT_CONSTANT_ENTRY(name)
#endif /* COMPILER1 */

// Generate an int constant for a C2 build
#ifdef COMPILER2
# define GENERATE_C2_VM_INT_CONSTANT_ENTRY(name)                      GENERATE_VM_INT_CONSTANT_ENTRY(name)
# define GENERATE_C2_PREPROCESSOR_VM_INT_CONSTANT_ENTRY(name, value)  GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY(name, value)
#else
# define GENERATE_C2_VM_INT_CONSTANT_ENTRY(name)
# define GENERATE_C2_PREPROCESSOR_VM_INT_CONSTANT_ENTRY(name, value)
#endif /* COMPILER1 */

//--------------------------------------------------------------------------------
// VMLongConstantEntry macros
//

#define GENERATE_VM_LONG_CONSTANT_ENTRY(name) \
  { QUOTE(name), cast_uint64_t(name) },

#define GENERATE_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY(name, value) \
  { name, cast_uint64_t(value) },

// This macro generates the sentinel value indicating the end of the list
#define GENERATE_VM_LONG_CONSTANT_LAST_ENTRY() \
 { NULL, 0 }

// Generate a long constant for a C1 build
#ifdef COMPILER1
# define GENERATE_C1_VM_LONG_CONSTANT_ENTRY(name)  GENERATE_VM_LONG_CONSTANT_ENTRY(name)
#else
# define GENERATE_C1_VM_LONG_CONSTANT_ENTRY(name)
#endif /* COMPILER1 */

// Generate a long constant for a C2 build
#ifdef COMPILER2
# define GENERATE_C2_VM_LONG_CONSTANT_ENTRY(name)                     GENERATE_VM_LONG_CONSTANT_ENTRY(name)
# define GENERATE_C2_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY(name, value) GENERATE_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY(name, value)
#else
# define GENERATE_C2_VM_LONG_CONSTANT_ENTRY(name)
# define GENERATE_C2_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY(name, value)
#endif /* COMPILER1 */

//
// Instantiation of VMStructEntries, VMTypeEntries and VMIntConstantEntries
//

// These initializers are allowed to access private fields in classes
// as long as class VMStructs is a friend
VMStructEntry VMStructs::localHotSpotVMStructs[] = {

  VM_STRUCTS(GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
             GENERATE_STATIC_VM_STRUCT_ENTRY,
             GENERATE_STATIC_PTR_VOLATILE_VM_STRUCT_ENTRY,
             GENERATE_UNCHECKED_NONSTATIC_VM_STRUCT_ENTRY,
             GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
             GENERATE_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY,
             GENERATE_C1_NONSTATIC_VM_STRUCT_ENTRY,
             GENERATE_C2_NONSTATIC_VM_STRUCT_ENTRY,
             GENERATE_C1_UNCHECKED_STATIC_VM_STRUCT_ENTRY,
             GENERATE_C2_UNCHECKED_STATIC_VM_STRUCT_ENTRY)

#if INCLUDE_ALL_GCS
  VM_STRUCTS_PARALLELGC(GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                        GENERATE_STATIC_VM_STRUCT_ENTRY)

  VM_STRUCTS_CMS(GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_STATIC_VM_STRUCT_ENTRY)

  VM_STRUCTS_G1(GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                GENERATE_STATIC_VM_STRUCT_ENTRY)
#endif // INCLUDE_ALL_GCS

  VM_STRUCTS_CPU(GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_STATIC_VM_STRUCT_ENTRY,
                 GENERATE_UNCHECKED_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_C2_NONSTATIC_VM_STRUCT_ENTRY,
                 GENERATE_C1_UNCHECKED_STATIC_VM_STRUCT_ENTRY,
                 GENERATE_C2_UNCHECKED_STATIC_VM_STRUCT_ENTRY)

  VM_STRUCTS_OS_CPU(GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                    GENERATE_STATIC_VM_STRUCT_ENTRY,
                    GENERATE_UNCHECKED_NONSTATIC_VM_STRUCT_ENTRY,
                    GENERATE_NONSTATIC_VM_STRUCT_ENTRY,
                    GENERATE_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY,
                    GENERATE_C2_NONSTATIC_VM_STRUCT_ENTRY,
                    GENERATE_C1_UNCHECKED_STATIC_VM_STRUCT_ENTRY,
                    GENERATE_C2_UNCHECKED_STATIC_VM_STRUCT_ENTRY)

  GENERATE_VM_STRUCT_LAST_ENTRY()
};

VMTypeEntry VMStructs::localHotSpotVMTypes[] = {

  VM_TYPES(GENERATE_VM_TYPE_ENTRY,
           GENERATE_TOPLEVEL_VM_TYPE_ENTRY,
           GENERATE_OOP_VM_TYPE_ENTRY,
           GENERATE_INTEGER_VM_TYPE_ENTRY,
           GENERATE_UNSIGNED_INTEGER_VM_TYPE_ENTRY,
           GENERATE_C1_TOPLEVEL_VM_TYPE_ENTRY,
           GENERATE_C2_VM_TYPE_ENTRY,
           GENERATE_C2_TOPLEVEL_VM_TYPE_ENTRY)

#if INCLUDE_ALL_GCS
  VM_TYPES_PARALLELGC(GENERATE_VM_TYPE_ENTRY,
                      GENERATE_TOPLEVEL_VM_TYPE_ENTRY)

  VM_TYPES_CMS(GENERATE_VM_TYPE_ENTRY,
               GENERATE_TOPLEVEL_VM_TYPE_ENTRY)

  VM_TYPES_PARNEW(GENERATE_VM_TYPE_ENTRY)

  VM_TYPES_G1(GENERATE_VM_TYPE_ENTRY,
              GENERATE_TOPLEVEL_VM_TYPE_ENTRY)
#endif // INCLUDE_ALL_GCS

  VM_TYPES_CPU(GENERATE_VM_TYPE_ENTRY,
               GENERATE_TOPLEVEL_VM_TYPE_ENTRY,
               GENERATE_OOP_VM_TYPE_ENTRY,
               GENERATE_INTEGER_VM_TYPE_ENTRY,
               GENERATE_UNSIGNED_INTEGER_VM_TYPE_ENTRY,
               GENERATE_C1_TOPLEVEL_VM_TYPE_ENTRY,
               GENERATE_C2_VM_TYPE_ENTRY,
               GENERATE_C2_TOPLEVEL_VM_TYPE_ENTRY)

  VM_TYPES_OS_CPU(GENERATE_VM_TYPE_ENTRY,
                  GENERATE_TOPLEVEL_VM_TYPE_ENTRY,
                  GENERATE_OOP_VM_TYPE_ENTRY,
                  GENERATE_INTEGER_VM_TYPE_ENTRY,
                  GENERATE_UNSIGNED_INTEGER_VM_TYPE_ENTRY,
                  GENERATE_C1_TOPLEVEL_VM_TYPE_ENTRY,
                  GENERATE_C2_VM_TYPE_ENTRY,
                  GENERATE_C2_TOPLEVEL_VM_TYPE_ENTRY)

  GENERATE_VM_TYPE_LAST_ENTRY()
};

VMIntConstantEntry VMStructs::localHotSpotVMIntConstants[] = {

  VM_INT_CONSTANTS(GENERATE_VM_INT_CONSTANT_ENTRY,
                   GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY,
                   GENERATE_C1_VM_INT_CONSTANT_ENTRY,
                   GENERATE_C2_VM_INT_CONSTANT_ENTRY,
                   GENERATE_C2_PREPROCESSOR_VM_INT_CONSTANT_ENTRY)

#if INCLUDE_ALL_GCS
  VM_INT_CONSTANTS_CMS(GENERATE_VM_INT_CONSTANT_ENTRY)

  VM_INT_CONSTANTS_PARNEW(GENERATE_VM_INT_CONSTANT_ENTRY)
#endif // INCLUDE_ALL_GCS

  VM_INT_CONSTANTS_CPU(GENERATE_VM_INT_CONSTANT_ENTRY,
                       GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY,
                       GENERATE_C1_VM_INT_CONSTANT_ENTRY,
                       GENERATE_C2_VM_INT_CONSTANT_ENTRY,
                       GENERATE_C2_PREPROCESSOR_VM_INT_CONSTANT_ENTRY)

  VM_INT_CONSTANTS_OS_CPU(GENERATE_VM_INT_CONSTANT_ENTRY,
                          GENERATE_PREPROCESSOR_VM_INT_CONSTANT_ENTRY,
                          GENERATE_C1_VM_INT_CONSTANT_ENTRY,
                          GENERATE_C2_VM_INT_CONSTANT_ENTRY,
                          GENERATE_C2_PREPROCESSOR_VM_INT_CONSTANT_ENTRY)

  GENERATE_VM_INT_CONSTANT_LAST_ENTRY()
};

VMLongConstantEntry VMStructs::localHotSpotVMLongConstants[] = {

  VM_LONG_CONSTANTS(GENERATE_VM_LONG_CONSTANT_ENTRY,
                    GENERATE_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY,
                    GENERATE_C1_VM_LONG_CONSTANT_ENTRY,
                    GENERATE_C2_VM_LONG_CONSTANT_ENTRY,
                    GENERATE_C2_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY)

  VM_LONG_CONSTANTS_CPU(GENERATE_VM_LONG_CONSTANT_ENTRY,
                        GENERATE_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY,
                        GENERATE_C1_VM_LONG_CONSTANT_ENTRY,
                        GENERATE_C2_VM_LONG_CONSTANT_ENTRY,
                        GENERATE_C2_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY)

  VM_LONG_CONSTANTS_OS_CPU(GENERATE_VM_LONG_CONSTANT_ENTRY,
                           GENERATE_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY,
                           GENERATE_C1_VM_LONG_CONSTANT_ENTRY,
                           GENERATE_C2_VM_LONG_CONSTANT_ENTRY,
                           GENERATE_C2_PREPROCESSOR_VM_LONG_CONSTANT_ENTRY)

  GENERATE_VM_LONG_CONSTANT_LAST_ENTRY()
};

// This is used both to check the types of referenced fields and, in
// debug builds, to ensure that all of the field types are present.
void
VMStructs::init() {
  VM_STRUCTS(CHECK_NONSTATIC_VM_STRUCT_ENTRY,
             CHECK_STATIC_VM_STRUCT_ENTRY,
             CHECK_STATIC_PTR_VOLATILE_VM_STRUCT_ENTRY,
             CHECK_NO_OP,
             CHECK_VOLATILE_NONSTATIC_VM_STRUCT_ENTRY,
             CHECK_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY,
             CHECK_C1_NONSTATIC_VM_STRUCT_ENTRY,
             CHECK_C2_NONSTATIC_VM_STRUCT_ENTRY,
             CHECK_NO_OP,
             CHECK_NO_OP);

#if INCLUDE_ALL_GCS
  VM_STRUCTS_PARALLELGC(CHECK_NONSTATIC_VM_STRUCT_ENTRY,
             CHECK_STATIC_VM_STRUCT_ENTRY);

  VM_STRUCTS_CMS(CHECK_NONSTATIC_VM_STRUCT_ENTRY,
             CHECK_VOLATILE_NONSTATIC_VM_STRUCT_ENTRY,
             CHECK_STATIC_VM_STRUCT_ENTRY);

  VM_STRUCTS_G1(CHECK_NONSTATIC_VM_STRUCT_ENTRY,
                CHECK_STATIC_VM_STRUCT_ENTRY);

#endif // INCLUDE_ALL_GCS

  VM_STRUCTS_CPU(CHECK_NONSTATIC_VM_STRUCT_ENTRY,
                 CHECK_STATIC_VM_STRUCT_ENTRY,
                 CHECK_NO_OP,
                 CHECK_VOLATILE_NONSTATIC_VM_STRUCT_ENTRY,
                 CHECK_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY,
                 CHECK_C2_NONSTATIC_VM_STRUCT_ENTRY,
                 CHECK_NO_OP,
                 CHECK_NO_OP);

  VM_STRUCTS_OS_CPU(CHECK_NONSTATIC_VM_STRUCT_ENTRY,
                    CHECK_STATIC_VM_STRUCT_ENTRY,
                    CHECK_NO_OP,
                    CHECK_VOLATILE_NONSTATIC_VM_STRUCT_ENTRY,
                    CHECK_NONPRODUCT_NONSTATIC_VM_STRUCT_ENTRY,
                    CHECK_C2_NONSTATIC_VM_STRUCT_ENTRY,
                    CHECK_NO_OP,
                    CHECK_NO_OP);

  VM_TYPES(CHECK_VM_TYPE_ENTRY,
           CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
           CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
           CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
           CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
           CHECK_C1_TOPLEVEL_VM_TYPE_ENTRY,
           CHECK_C2_VM_TYPE_ENTRY,
           CHECK_C2_TOPLEVEL_VM_TYPE_ENTRY);

#if INCLUDE_ALL_GCS
  VM_TYPES_PARALLELGC(CHECK_VM_TYPE_ENTRY,
                      CHECK_SINGLE_ARG_VM_TYPE_NO_OP);

  VM_TYPES_CMS(CHECK_VM_TYPE_ENTRY,
               CHECK_SINGLE_ARG_VM_TYPE_NO_OP);

  VM_TYPES_PARNEW(CHECK_VM_TYPE_ENTRY)

  VM_TYPES_G1(CHECK_VM_TYPE_ENTRY,
              CHECK_SINGLE_ARG_VM_TYPE_NO_OP);

#endif // INCLUDE_ALL_GCS

  VM_TYPES_CPU(CHECK_VM_TYPE_ENTRY,
               CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
               CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
               CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
               CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
               CHECK_C1_TOPLEVEL_VM_TYPE_ENTRY,
               CHECK_C2_VM_TYPE_ENTRY,
               CHECK_C2_TOPLEVEL_VM_TYPE_ENTRY);

  VM_TYPES_OS_CPU(CHECK_VM_TYPE_ENTRY,
                  CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
                  CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
                  CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
                  CHECK_SINGLE_ARG_VM_TYPE_NO_OP,
                  CHECK_C1_TOPLEVEL_VM_TYPE_ENTRY,
                  CHECK_C2_VM_TYPE_ENTRY,
                  CHECK_C2_TOPLEVEL_VM_TYPE_ENTRY);

  //
  // Split VM_STRUCTS() invocation into two parts to allow MS VC++ 6.0
  // to build with the source mounted over SNC3.2. Symptom was that
  // debug build failed with an internal compiler error. Has been seen
  // mounting sources from Solaris 2.6 and 2.7 hosts, but so far not
  // 2.8 hosts. Appears to occur because line is too long.
  //
  // If an assertion failure is triggered here it means that an entry
  // in VMStructs::localHotSpotVMStructs[] was not found in
  // VMStructs::localHotSpotVMTypes[]. (The assertion itself had to be
  // made less descriptive because of this above bug -- see the
  // definition of ENSURE_FIELD_TYPE_PRESENT.)
  //
  // NOTE: taken out because this was just not working on everyone's
  // Solstice NFS setup. If everyone switches to local workspaces on
  // Win32, we can put this back in.
#ifndef _WINDOWS
  debug_only(VM_STRUCTS(ENSURE_FIELD_TYPE_PRESENT,
                        CHECK_NO_OP,
                        CHECK_NO_OP,
                        CHECK_NO_OP,
                        CHECK_NO_OP,
                        CHECK_NO_OP,
                        CHECK_NO_OP,
                        CHECK_NO_OP,
                        CHECK_NO_OP,
                        CHECK_NO_OP));
  debug_only(VM_STRUCTS(CHECK_NO_OP,
                        ENSURE_FIELD_TYPE_PRESENT,
                        ENSURE_FIELD_TYPE_PRESENT,
                        CHECK_NO_OP,
                        ENSURE_FIELD_TYPE_PRESENT,
                        ENSURE_NONPRODUCT_FIELD_TYPE_PRESENT,
                        ENSURE_C1_FIELD_TYPE_PRESENT,
                        ENSURE_C2_FIELD_TYPE_PRESENT,
                        CHECK_NO_OP,
                        CHECK_NO_OP));
#if INCLUDE_ALL_GCS
  debug_only(VM_STRUCTS_PARALLELGC(ENSURE_FIELD_TYPE_PRESENT,
                                   ENSURE_FIELD_TYPE_PRESENT));
  debug_only(VM_STRUCTS_CMS(ENSURE_FIELD_TYPE_PRESENT,
                            ENSURE_FIELD_TYPE_PRESENT,
                            ENSURE_FIELD_TYPE_PRESENT));
  debug_only(VM_STRUCTS_G1(ENSURE_FIELD_TYPE_PRESENT,
                           ENSURE_FIELD_TYPE_PRESENT));
#endif // INCLUDE_ALL_GCS

  debug_only(VM_STRUCTS_CPU(ENSURE_FIELD_TYPE_PRESENT,
                            ENSURE_FIELD_TYPE_PRESENT,
                            CHECK_NO_OP,
                            ENSURE_FIELD_TYPE_PRESENT,
                            ENSURE_NONPRODUCT_FIELD_TYPE_PRESENT,
                            ENSURE_C2_FIELD_TYPE_PRESENT,
                            CHECK_NO_OP,
                            CHECK_NO_OP));
  debug_only(VM_STRUCTS_OS_CPU(ENSURE_FIELD_TYPE_PRESENT,
                               ENSURE_FIELD_TYPE_PRESENT,
                               CHECK_NO_OP,
                               ENSURE_FIELD_TYPE_PRESENT,
                               ENSURE_NONPRODUCT_FIELD_TYPE_PRESENT,
                               ENSURE_C2_FIELD_TYPE_PRESENT,
                               CHECK_NO_OP,
                               CHECK_NO_OP));
#endif
}

extern "C" {

// see comments on cast_uint64_t at the top of this file
#define ASSIGN_CONST_TO_64BIT_VAR(var, expr) \
    JNIEXPORT uint64_t var = cast_uint64_t(expr);
#define ASSIGN_OFFSET_TO_64BIT_VAR(var, type, field)   \
  ASSIGN_CONST_TO_64BIT_VAR(var, offset_of(type, field))
#define ASSIGN_STRIDE_TO_64BIT_VAR(var, array) \
  ASSIGN_CONST_TO_64BIT_VAR(var, (char*)&array[1] - (char*)&array[0])

JNIEXPORT VMStructEntry* gHotSpotVMStructs                 = VMStructs::localHotSpotVMStructs;
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMStructEntryTypeNameOffset,  VMStructEntry, typeName);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMStructEntryFieldNameOffset, VMStructEntry, fieldName);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMStructEntryTypeStringOffset, VMStructEntry, typeString);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMStructEntryIsStaticOffset, VMStructEntry, isStatic);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMStructEntryOffsetOffset, VMStructEntry, offset);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMStructEntryAddressOffset, VMStructEntry, address);
ASSIGN_STRIDE_TO_64BIT_VAR(gHotSpotVMStructEntryArrayStride, gHotSpotVMStructs);
JNIEXPORT VMTypeEntry*   gHotSpotVMTypes                   = VMStructs::localHotSpotVMTypes;
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMTypeEntryTypeNameOffset, VMTypeEntry, typeName);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMTypeEntrySuperclassNameOffset, VMTypeEntry, superclassName);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMTypeEntryIsOopTypeOffset, VMTypeEntry, isOopType);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMTypeEntryIsIntegerTypeOffset, VMTypeEntry, isIntegerType);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMTypeEntryIsUnsignedOffset, VMTypeEntry, isUnsigned);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMTypeEntrySizeOffset, VMTypeEntry, size);
ASSIGN_STRIDE_TO_64BIT_VAR(gHotSpotVMTypeEntryArrayStride,gHotSpotVMTypes);
JNIEXPORT VMIntConstantEntry* gHotSpotVMIntConstants       = VMStructs::localHotSpotVMIntConstants;
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMIntConstantEntryNameOffset, VMIntConstantEntry, name);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMIntConstantEntryValueOffset, VMIntConstantEntry, value);
ASSIGN_STRIDE_TO_64BIT_VAR(gHotSpotVMIntConstantEntryArrayStride, gHotSpotVMIntConstants);
JNIEXPORT VMLongConstantEntry* gHotSpotVMLongConstants     = VMStructs::localHotSpotVMLongConstants;
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMLongConstantEntryNameOffset, VMLongConstantEntry, name);
ASSIGN_OFFSET_TO_64BIT_VAR(gHotSpotVMLongConstantEntryValueOffset, VMLongConstantEntry, value);
ASSIGN_STRIDE_TO_64BIT_VAR(gHotSpotVMLongConstantEntryArrayStride, gHotSpotVMLongConstants);
}

#ifdef ASSERT
static int recursiveFindType(VMTypeEntry* origtypes, const char* typeName, bool isRecurse) {
  {
    VMTypeEntry* types = origtypes;
    while (types->typeName != NULL) {
      if (strcmp(typeName, types->typeName) == 0) {
        // Found it
        return 1;
      }
      ++types;
    }
  }
  // Search for the base type by peeling off const and *
  size_t len = strlen(typeName);
  if (typeName[len-1] == '*') {
    char * s = NEW_C_HEAP_ARRAY(char, len, mtInternal);
    strncpy(s, typeName, len - 1);
    s[len-1] = '\0';
    // tty->print_cr("checking \"%s\" for \"%s\"", s, typeName);
    if (recursiveFindType(origtypes, s, true) == 1) {
      FREE_C_HEAP_ARRAY(char, s, mtInternal);
      return 1;
    }
    FREE_C_HEAP_ARRAY(char, s, mtInternal);
  }
  const char* start = NULL;
  if (strstr(typeName, "GrowableArray<") == typeName) {
    start = typeName + strlen("GrowableArray<");
  } else if (strstr(typeName, "Array<") == typeName) {
    start = typeName + strlen("Array<");
  }
  if (start != NULL) {
    const char * end = strrchr(typeName, '>');
    int len = end - start + 1;
    char * s = NEW_C_HEAP_ARRAY(char, len, mtInternal);
    strncpy(s, start, len - 1);
    s[len-1] = '\0';
    // tty->print_cr("checking \"%s\" for \"%s\"", s, typeName);
    if (recursiveFindType(origtypes, s, true) == 1) {
      FREE_C_HEAP_ARRAY(char, s, mtInternal);
      return 1;
    }
    FREE_C_HEAP_ARRAY(char, s, mtInternal);
  }
  if (strstr(typeName, "const ") == typeName) {
    const char * s = typeName + strlen("const ");
    // tty->print_cr("checking \"%s\" for \"%s\"", s, typeName);
    if (recursiveFindType(origtypes, s, true) == 1) {
      return 1;
    }
  }
  if (strstr(typeName, " const") == typeName + len - 6) {
    char * s = strdup(typeName);
    s[len - 6] = '\0';
    // tty->print_cr("checking \"%s\" for \"%s\"", s, typeName);
    if (recursiveFindType(origtypes, s, true) == 1) {
      free(s);
      return 1;
    }
    free(s);
  }
  if (!isRecurse) {
    tty->print_cr("type \"%s\" not found", typeName);
  }
  return 0;
}


int
VMStructs::findType(const char* typeName) {
  VMTypeEntry* types = gHotSpotVMTypes;

  return recursiveFindType(types, typeName, false);
}
#endif

void vmStructs_init() {
  debug_only(VMStructs::init());
}

#ifndef PRODUCT
void VMStructs::test() {
  // Make sure last entry in the each array is indeed the correct end marker.
  // The reason why these are static is to make sure they are zero initialized.
  // Putting them on the stack will leave some garbage in the padding of some fields.
  static VMStructEntry struct_last_entry = GENERATE_VM_STRUCT_LAST_ENTRY();
  assert(memcmp(&localHotSpotVMStructs[(sizeof(localHotSpotVMStructs) / sizeof(VMStructEntry)) - 1],
                &struct_last_entry,
                sizeof(VMStructEntry)) == 0, "Incorrect last entry in localHotSpotVMStructs");

  static VMTypeEntry type_last_entry = GENERATE_VM_TYPE_LAST_ENTRY();
  assert(memcmp(&localHotSpotVMTypes[sizeof(localHotSpotVMTypes) / sizeof(VMTypeEntry) - 1],
                &type_last_entry,
                sizeof(VMTypeEntry)) == 0, "Incorrect last entry in localHotSpotVMTypes");

  static VMIntConstantEntry int_last_entry = GENERATE_VM_INT_CONSTANT_LAST_ENTRY();
  assert(memcmp(&localHotSpotVMIntConstants[sizeof(localHotSpotVMIntConstants) / sizeof(VMIntConstantEntry) - 1],
                &int_last_entry,
                sizeof(VMIntConstantEntry)) == 0, "Incorrect last entry in localHotSpotVMIntConstants");

  static VMLongConstantEntry long_last_entry = GENERATE_VM_LONG_CONSTANT_LAST_ENTRY();
  assert(memcmp(&localHotSpotVMLongConstants[sizeof(localHotSpotVMLongConstants) / sizeof(VMLongConstantEntry) - 1],
                &long_last_entry,
                sizeof(VMLongConstantEntry)) == 0, "Incorrect last entry in localHotSpotVMLongConstants");


  // Check for duplicate entries in type array
  for (int i = 0; localHotSpotVMTypes[i].typeName != NULL; i++) {
    for (int j = i + 1; localHotSpotVMTypes[j].typeName != NULL; j++) {
      if (strcmp(localHotSpotVMTypes[i].typeName, localHotSpotVMTypes[j].typeName) == 0) {
        tty->print_cr("Duplicate entries for '%s'", localHotSpotVMTypes[i].typeName);
        assert(false, "Duplicate types in localHotSpotVMTypes array");
      }
    }
  }
}
#endif
