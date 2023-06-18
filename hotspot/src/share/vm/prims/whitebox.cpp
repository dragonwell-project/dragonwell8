/*
 * Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "memory/metadataFactory.hpp"
#include "memory/metaspaceShared.hpp"
#include "memory/iterator.hpp"
#include "memory/universe.hpp"
#include "memory/gcLocker.inline.hpp"
#include "oops/oop.inline.hpp"

#include "classfile/symbolTable.hpp"
#include "classfile/classLoaderData.hpp"

#include "prims/whitebox.hpp"
#include "prims/wbtestmethods/parserTests.hpp"

#include "runtime/arguments.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/os.hpp"
#include "utilities/array.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"
#include "utilities/macros.hpp"
#include "utilities/exceptions.hpp"

#if INCLUDE_CDS
#include "prims/cdsoffsets.hpp"
#endif // INCLUDE_CDS

#if INCLUDE_ALL_GCS
#include "gc_implementation/parallelScavenge/parallelScavengeHeap.inline.hpp"
#include "gc_implementation/g1/concurrentMark.hpp"
#include "gc_implementation/g1/concurrentMarkThread.hpp"
#include "gc_implementation/g1/g1CollectedHeap.inline.hpp"
#include "gc_implementation/g1/heapRegionRemSet.hpp"
#endif // INCLUDE_ALL_GCS

#if INCLUDE_NMT
#include "services/mallocSiteTable.hpp"
#include "services/memTracker.hpp"
#include "utilities/nativeCallStack.hpp"
#endif // INCLUDE_NMT

#include "compiler/compileBroker.hpp"
#include "jvmtifiles/jvmtiEnv.hpp"
#include "runtime/compilationPolicy.hpp"

#ifdef LINUX
#include "osContainer_linux.hpp"
#include "cgroupSubsystem_linux.hpp"
#endif

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

#define SIZE_T_MAX_VALUE ((size_t) -1)

bool WhiteBox::_used = false;

WB_ENTRY(jlong, WB_GetObjectAddress(JNIEnv* env, jobject o, jobject obj))
  return (jlong)(void*)JNIHandles::resolve(obj);
WB_END

WB_ENTRY(jint, WB_GetHeapOopSize(JNIEnv* env, jobject o))
  return heapOopSize;
WB_END

WB_ENTRY(jint, WB_GetVMPageSize(JNIEnv* env, jobject o))
  return os::vm_page_size();
WB_END

WB_ENTRY(jlong, WB_GetVMLargePageSize(JNIEnv* env, jobject o))
  return os::large_page_size();
WB_END

class WBIsKlassAliveClosure : public KlassClosure {
    Symbol* _name;
    bool _found;
public:
    WBIsKlassAliveClosure(Symbol* name) : _name(name), _found(false) {}

    void do_klass(Klass* k) {
      if (_found) return;
      Symbol* ksym = k->name();
      if (ksym->fast_compare(_name) == 0) {
        _found = true;
      }
    }

    bool found() const {
        return _found;
    }
};

WB_ENTRY(jboolean, WB_IsClassAlive(JNIEnv* env, jobject target, jstring name))
  Handle h_name = JNIHandles::resolve(name);
  if (h_name.is_null()) return false;
  Symbol* sym = java_lang_String::as_symbol(h_name, CHECK_false);
  TempNewSymbol tsym(sym); // Make sure to decrement reference count on sym on return

  WBIsKlassAliveClosure closure(sym);
  ClassLoaderDataGraph::classes_do(&closure);

  return closure.found();
WB_END

WB_ENTRY(jboolean, WB_ClassKnownToNotExist(JNIEnv* env, jobject o, jobject loader, jstring name))
  ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
  const char* class_name = env->GetStringUTFChars(name, NULL);
  jboolean result = JVM_KnownToNotExist(env, loader, class_name);
  env->ReleaseStringUTFChars(name, class_name);
  return result;
WB_END

WB_ENTRY(jobjectArray, WB_GetLookupCacheURLs(JNIEnv* env, jobject o, jobject loader))
  ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
  return JVM_GetResourceLookupCacheURLs(env, loader);
WB_END

WB_ENTRY(jintArray, WB_GetLookupCacheMatches(JNIEnv* env, jobject o, jobject loader, jstring name))
  ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
  const char* resource_name = env->GetStringUTFChars(name, NULL);
  jintArray result = JVM_GetResourceLookupCache(env, loader, resource_name);

  env->ReleaseStringUTFChars(name, resource_name);
  return result;
WB_END

WB_ENTRY(void, WB_AddToBootstrapClassLoaderSearch(JNIEnv* env, jobject o, jstring segment)) {
#if INCLUDE_JVMTI
  ResourceMark rm;
  const char* seg = java_lang_String::as_utf8_string(JNIHandles::resolve_non_null(segment));
  JvmtiEnv* jvmti_env = JvmtiEnv::create_a_jvmti(JVMTI_VERSION);
  jvmtiError err = jvmti_env->AddToBootstrapClassLoaderSearch(seg);
  assert(err == JVMTI_ERROR_NONE, "must not fail");
#endif
}
WB_END

WB_ENTRY(void, WB_AddToSystemClassLoaderSearch(JNIEnv* env, jobject o, jstring segment)) {
#if INCLUDE_JVMTI
  ResourceMark rm;
  const char* seg = java_lang_String::as_utf8_string(JNIHandles::resolve_non_null(segment));
  JvmtiEnv* jvmti_env = JvmtiEnv::create_a_jvmti(JVMTI_VERSION);
  jvmtiError err = jvmti_env->AddToSystemClassLoaderSearch(seg);
  assert(err == JVMTI_ERROR_NONE, "must not fail");
#endif
}
WB_END

#ifdef LINUX
#include "utilities/elfFile.hpp"
#endif

WB_ENTRY(jlong, WB_GetCompressedOopsMaxHeapSize(JNIEnv* env, jobject o)) {
  return (jlong)Arguments::max_heap_for_compressed_oops();
}
WB_END

WB_ENTRY(void, WB_PrintHeapSizes(JNIEnv* env, jobject o)) {
  CollectorPolicy * p = Universe::heap()->collector_policy();
  gclog_or_tty->print_cr("Minimum heap " SIZE_FORMAT " Initial heap "
    SIZE_FORMAT " Maximum heap " SIZE_FORMAT " Space alignment " SIZE_FORMAT " Heap alignment " SIZE_FORMAT,
    p->min_heap_byte_size(), p->initial_heap_byte_size(), p->max_heap_byte_size(),
    p->space_alignment(), p->heap_alignment());
}
WB_END

#ifndef PRODUCT
// Forward declaration
void TestReservedSpace_test();
void TestReserveMemorySpecial_test();
void TestVirtualSpace_test();
void TestMetaspaceAux_test();
#endif

WB_ENTRY(void, WB_RunMemoryUnitTests(JNIEnv* env, jobject o))
#ifndef PRODUCT
  TestReservedSpace_test();
  TestReserveMemorySpecial_test();
  TestVirtualSpace_test();
  TestMetaspaceAux_test();
#endif
WB_END

WB_ENTRY(void, WB_ReadFromNoaccessArea(JNIEnv* env, jobject o))
  size_t granularity = os::vm_allocation_granularity();
  ReservedHeapSpace rhs(100 * granularity, granularity, false, NULL);
  VirtualSpace vs;
  vs.initialize(rhs, 50 * granularity);

  //Check if constraints are complied
  if (!( UseCompressedOops && rhs.base() != NULL &&
         Universe::narrow_oop_base() != NULL &&
         Universe::narrow_oop_use_implicit_null_checks() )) {
    tty->print_cr("WB_ReadFromNoaccessArea method is useless:\n "
                  "\tUseCompressedOops is %d\n"
                  "\trhs.base() is " PTR_FORMAT "\n"
                  "\tUniverse::narrow_oop_base() is " PTR_FORMAT "\n"
                  "\tUniverse::narrow_oop_use_implicit_null_checks() is %d",
                  UseCompressedOops,
                  rhs.base(),
                  Universe::narrow_oop_base(),
                  Universe::narrow_oop_use_implicit_null_checks());
    return;
  }
  tty->print_cr("Reading from no access area... ");
  tty->print_cr("*(vs.low_boundary() - rhs.noaccess_prefix() / 2 ) = %c",
                *(vs.low_boundary() - rhs.noaccess_prefix() / 2 ));
WB_END

static jint wb_stress_virtual_space_resize(size_t reserved_space_size,
                                           size_t magnitude, size_t iterations) {
  size_t granularity = os::vm_allocation_granularity();
  ReservedHeapSpace rhs(reserved_space_size * granularity, granularity, false, NULL);
  VirtualSpace vs;
  if (!vs.initialize(rhs, 0)) {
    tty->print_cr("Failed to initialize VirtualSpace. Can't proceed.");
    return 3;
  }

  long seed = os::random();
  tty->print_cr("Random seed is %ld", seed);
  os::init_random(seed);

  for (size_t i = 0; i < iterations; i++) {

    // Whether we will shrink or grow
    bool shrink = os::random() % 2L == 0;

    // Get random delta to resize virtual space
    size_t delta = (size_t)os::random() % magnitude;

    // If we are about to shrink virtual space below zero, then expand instead
    if (shrink && vs.committed_size() < delta) {
      shrink = false;
    }

    // Resizing by delta
    if (shrink) {
      vs.shrink_by(delta);
    } else {
      // If expanding fails expand_by will silently return false
      vs.expand_by(delta, true);
    }
  }
  return 0;
}

WB_ENTRY(jint, WB_StressVirtualSpaceResize(JNIEnv* env, jobject o,
        jlong reserved_space_size, jlong magnitude, jlong iterations))
  tty->print_cr("reservedSpaceSize=" JLONG_FORMAT ", magnitude=" JLONG_FORMAT ", "
                "iterations=" JLONG_FORMAT "\n", reserved_space_size, magnitude,
                iterations);
  if (reserved_space_size < 0 || magnitude < 0 || iterations < 0) {
    tty->print_cr("One of variables printed above is negative. Can't proceed.\n");
    return 1;
  }

  // sizeof(size_t) depends on whether OS is 32bit or 64bit. sizeof(jlong) is
  // always 8 byte. That's why we should avoid overflow in case of 32bit platform.
  if (sizeof(size_t) < sizeof(jlong)) {
    jlong size_t_max_value = (jlong) SIZE_T_MAX_VALUE;
    if (reserved_space_size > size_t_max_value || magnitude > size_t_max_value
        || iterations > size_t_max_value) {
      tty->print_cr("One of variables printed above overflows size_t. Can't proceed.\n");
      return 2;
    }
  }

  return wb_stress_virtual_space_resize((size_t) reserved_space_size,
                                        (size_t) magnitude, (size_t) iterations);
WB_END

WB_ENTRY(jboolean, WB_isObjectInOldGen(JNIEnv* env, jobject o, jobject obj))
  oop p = JNIHandles::resolve(obj);
#if INCLUDE_ALL_GCS
  if (UseG1GC) {
    G1CollectedHeap* g1 = G1CollectedHeap::heap();
    const HeapRegion* hr = g1->heap_region_containing(p);
    if (hr == NULL) {
      return false;
    }
    return !(hr->is_young());
  } else if (UseParallelGC) {
    ParallelScavengeHeap* psh = ParallelScavengeHeap::heap();
    return !psh->is_in_young(p);
  }
#endif // INCLUDE_ALL_GCS
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  return !gch->is_in_young(p);
WB_END

WB_ENTRY(jlong, WB_GetObjectSize(JNIEnv* env, jobject o, jobject obj))
  oop p = JNIHandles::resolve(obj);
  return p->size() * HeapWordSize;
WB_END

#if INCLUDE_ALL_GCS
WB_ENTRY(jboolean, WB_G1IsHumongous(JNIEnv* env, jobject o, jobject obj))
  G1CollectedHeap* g1 = G1CollectedHeap::heap();
  oop result = JNIHandles::resolve(obj);
  const HeapRegion* hr = g1->heap_region_containing(result);
  return hr->isHumongous();
WB_END

WB_ENTRY(jlong, WB_G1NumMaxRegions(JNIEnv* env, jobject o))
  G1CollectedHeap* g1 = G1CollectedHeap::heap();
  size_t nr = g1->max_regions();
  return (jlong)nr;
WB_END

WB_ENTRY(jlong, WB_G1NumFreeRegions(JNIEnv* env, jobject o))
  G1CollectedHeap* g1 = G1CollectedHeap::heap();
  size_t nr = g1->num_free_regions();
  return (jlong)nr;
WB_END

WB_ENTRY(jboolean, WB_G1InConcurrentMark(JNIEnv* env, jobject o))
  G1CollectedHeap* g1 = G1CollectedHeap::heap();
  return g1->concurrent_mark()->cmThread()->during_cycle();
WB_END

WB_ENTRY(jboolean, WB_G1StartMarkCycle(JNIEnv* env, jobject o))
  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  if (!g1h->concurrent_mark()->cmThread()->during_cycle()) {
    g1h->collect(GCCause::_wb_conc_mark);
    return true;
  }
  return false;
WB_END

WB_ENTRY(jint, WB_G1RegionSize(JNIEnv* env, jobject o))
  return (jint)HeapRegion::GrainBytes;
WB_END

WB_ENTRY(jobject, WB_G1AuxiliaryMemoryUsage(JNIEnv* env))
  ResourceMark rm(THREAD);
  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  MemoryUsage usage = g1h->get_auxiliary_data_memory_usage();
  Handle h = MemoryService::create_MemoryUsage_obj(usage, CHECK_NULL);
  return JNIHandles::make_local(env, h());
WB_END
#endif // INCLUDE_ALL_GCS

#if INCLUDE_NMT
// Alloc memory using the test memory type so that we can use that to see if
// NMT picks it up correctly
WB_ENTRY(jlong, WB_NMTMalloc(JNIEnv* env, jobject o, jlong size))
  jlong addr = 0;
  addr = (jlong)(uintptr_t)os::malloc(size, mtTest);
  return addr;
WB_END

// Alloc memory with pseudo call stack. The test can create psudo malloc
// allocation site to stress the malloc tracking.
WB_ENTRY(jlong, WB_NMTMallocWithPseudoStack(JNIEnv* env, jobject o, jlong size, jint pseudo_stack))
  address pc = (address)(size_t)pseudo_stack;
  NativeCallStack stack(&pc, 1);
  return (jlong)(uintptr_t)os::malloc(size, mtTest, stack);
WB_END

// Alloc memory with pseudo call stack and specific memory type.
WB_ENTRY(jlong, WB_NMTMallocWithPseudoStackAndType(JNIEnv* env, jobject o, jlong size, jint pseudo_stack, jint type))
  address pc = (address)(size_t)pseudo_stack;
  NativeCallStack stack(&pc, 1);
  return (jlong)(uintptr_t)os::malloc(size, (MEMFLAGS)type, stack);
WB_END

// Free the memory allocated by NMTAllocTest
WB_ENTRY(void, WB_NMTFree(JNIEnv* env, jobject o, jlong mem))
  os::free((void*)(uintptr_t)mem, mtTest);
WB_END

WB_ENTRY(jlong, WB_NMTReserveMemory(JNIEnv* env, jobject o, jlong size))
  jlong addr = 0;

    addr = (jlong)(uintptr_t)os::reserve_memory(size);
    MemTracker::record_virtual_memory_type((address)addr, mtTest);

  return addr;
WB_END


WB_ENTRY(void, WB_NMTCommitMemory(JNIEnv* env, jobject o, jlong addr, jlong size))
  os::commit_memory((char *)(uintptr_t)addr, size, !ExecMem);
  MemTracker::record_virtual_memory_type((address)(uintptr_t)addr, mtTest);
WB_END

WB_ENTRY(void, WB_NMTUncommitMemory(JNIEnv* env, jobject o, jlong addr, jlong size))
  os::uncommit_memory((char *)(uintptr_t)addr, size);
WB_END

WB_ENTRY(void, WB_NMTReleaseMemory(JNIEnv* env, jobject o, jlong addr, jlong size))
  os::release_memory((char *)(uintptr_t)addr, size);
WB_END

WB_ENTRY(jboolean, WB_NMTIsDetailSupported(JNIEnv* env))
  return MemTracker::tracking_level() == NMT_detail;
WB_END

WB_ENTRY(jboolean, WB_NMTChangeTrackingLevel(JNIEnv* env))
  // Test that we can downgrade NMT levels but not upgrade them.
  if (MemTracker::tracking_level() == NMT_off) {
    MemTracker::transition_to(NMT_off);
    return MemTracker::tracking_level() == NMT_off;
  } else {
    assert(MemTracker::tracking_level() == NMT_detail, "Should start out as detail tracking");
    MemTracker::transition_to(NMT_summary);
    assert(MemTracker::tracking_level() == NMT_summary, "Should be summary now");

    // Can't go to detail once NMT is set to summary.
    MemTracker::transition_to(NMT_detail);
    assert(MemTracker::tracking_level() == NMT_summary, "Should still be summary now");

    // Shutdown sets tracking level to minimal.
    MemTracker::shutdown();
    assert(MemTracker::tracking_level() == NMT_minimal, "Should be minimal now");

    // Once the tracking level is minimal, we cannot increase to summary.
    // The code ignores this request instead of asserting because if the malloc site
    // table overflows in another thread, it tries to change the code to summary.
    MemTracker::transition_to(NMT_summary);
    assert(MemTracker::tracking_level() == NMT_minimal, "Should still be minimal now");

    // Really can never go up to detail, verify that the code would never do this.
    MemTracker::transition_to(NMT_detail);
    assert(MemTracker::tracking_level() == NMT_minimal, "Should still be minimal now");
    return MemTracker::tracking_level() == NMT_minimal;
  }
WB_END

WB_ENTRY(jint, WB_NMTGetHashSize(JNIEnv* env, jobject o))
  int hash_size = MallocSiteTable::hash_buckets();
  assert(hash_size > 0, "NMT hash_size should be > 0");
  return (jint)hash_size;
WB_END

WB_ENTRY(jlong, WB_NMTNewArena(JNIEnv* env, jobject o, jlong init_size))
  Arena* arena =  new (mtTest) Arena(mtTest, size_t(init_size));
  return (jlong)arena;
WB_END

WB_ENTRY(void, WB_NMTFreeArena(JNIEnv* env, jobject o, jlong arena))
  Arena* a = (Arena*)arena;
  delete a;
WB_END

WB_ENTRY(void, WB_NMTArenaMalloc(JNIEnv* env, jobject o, jlong arena, jlong size))
  Arena* a = (Arena*)arena;
  a->Amalloc(size_t(size));
WB_END
#endif // INCLUDE_NMT

static jmethodID reflected_method_to_jmid(JavaThread* thread, JNIEnv* env, jobject method) {
  assert(method != NULL, "method should not be null");
  ThreadToNativeFromVM ttn(thread);
  return env->FromReflectedMethod(method);
}

WB_ENTRY(void, WB_DeoptimizeAll(JNIEnv* env, jobject o))
  MutexLockerEx mu(Compile_lock);
  CodeCache::mark_all_nmethods_for_deoptimization();
  VM_Deoptimize op;
  VMThread::execute(&op);
WB_END

WB_ENTRY(jint, WB_DeoptimizeMethod(JNIEnv* env, jobject o, jobject method, jboolean is_osr))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  int result = 0;
  CHECK_JNI_EXCEPTION_(env, result);
  MutexLockerEx mu(Compile_lock);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  if (is_osr) {
    result += mh->mark_osr_nmethods();
  } else if (mh->code() != NULL) {
    mh->code()->mark_for_deoptimization();
    ++result;
  }
  result += CodeCache::mark_for_deoptimization(mh());
  if (result > 0) {
    VM_Deoptimize op;
    VMThread::execute(&op);
  }
  return result;
WB_END

WB_ENTRY(jboolean, WB_IsMethodCompiled(JNIEnv* env, jobject o, jobject method, jboolean is_osr))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION_(env, JNI_FALSE);
  MutexLockerEx mu(Compile_lock);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  nmethod* code = is_osr ? mh->lookup_osr_nmethod_for(InvocationEntryBci, CompLevel_none, false) : mh->code();
  if (code == NULL) {
    return JNI_FALSE;
  }
  return (code->is_alive() && !code->is_marked_for_deoptimization());
WB_END

WB_ENTRY(jboolean, WB_IsMethodCompilable(JNIEnv* env, jobject o, jobject method, jint comp_level, jboolean is_osr))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION_(env, JNI_FALSE);
  MutexLockerEx mu(Compile_lock);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  if (is_osr) {
    return CompilationPolicy::can_be_osr_compiled(mh, comp_level);
  } else {
    return CompilationPolicy::can_be_compiled(mh, comp_level);
  }
WB_END

WB_ENTRY(jboolean, WB_IsMethodQueuedForCompilation(JNIEnv* env, jobject o, jobject method))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION_(env, JNI_FALSE);
  MutexLockerEx mu(Compile_lock);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  return mh->queued_for_compilation();
WB_END

WB_ENTRY(jint, WB_GetMethodCompilationLevel(JNIEnv* env, jobject o, jobject method, jboolean is_osr))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION_(env, CompLevel_none);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  nmethod* code = is_osr ? mh->lookup_osr_nmethod_for(InvocationEntryBci, CompLevel_none, false) : mh->code();
  return (code != NULL ? code->comp_level() : CompLevel_none);
WB_END

WB_ENTRY(void, WB_MakeMethodNotCompilable(JNIEnv* env, jobject o, jobject method, jint comp_level, jboolean is_osr))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION(env);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  if (is_osr) {
    mh->set_not_osr_compilable(comp_level, true /* report */, "WhiteBox");
  } else {
    mh->set_not_compilable(comp_level, true /* report */, "WhiteBox");
  }
WB_END

WB_ENTRY(jint, WB_GetMethodEntryBci(JNIEnv* env, jobject o, jobject method))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION_(env, InvocationEntryBci);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  nmethod* code = mh->lookup_osr_nmethod_for(InvocationEntryBci, CompLevel_none, false);
  return (code != NULL && code->is_osr_method() ? code->osr_entry_bci() : InvocationEntryBci);
WB_END

WB_ENTRY(jboolean, WB_TestSetDontInlineMethod(JNIEnv* env, jobject o, jobject method, jboolean value))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION_(env, JNI_FALSE);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  bool result = mh->dont_inline();
  mh->set_dont_inline(value == JNI_TRUE);
  return result;
WB_END

WB_ENTRY(jint, WB_GetCompileQueueSize(JNIEnv* env, jobject o, jint comp_level))
  if (comp_level == CompLevel_any) {
    return CompileBroker::queue_size(CompLevel_full_optimization) /* C2 */ +
        CompileBroker::queue_size(CompLevel_full_profile) /* C1 */;
  } else {
    return CompileBroker::queue_size(comp_level);
  }
WB_END

WB_ENTRY(jboolean, WB_TestSetForceInlineMethod(JNIEnv* env, jobject o, jobject method, jboolean value))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION_(env, JNI_FALSE);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  bool result = mh->force_inline();
  mh->set_force_inline(value == JNI_TRUE);
  return result;
WB_END

#ifdef LINUX
bool WhiteBox::validate_cgroup(const char* proc_cgroups,
                               const char* proc_self_cgroup,
                               const char* proc_self_mountinfo,
                               u1* cg_flags) {
  CgroupInfo cg_infos[4];
  return CgroupSubsystemFactory::determine_type(cg_infos, proc_cgroups,
                                                    proc_self_cgroup,
                                                    proc_self_mountinfo, cg_flags);
}
#endif

bool WhiteBox::compile_method(Method* method, int comp_level, int bci, Thread* THREAD) {
  // Screen for unavailable/bad comp level or null method
  AbstractCompiler* comp = CompileBroker::compiler(comp_level);
  if (method == NULL) {
    tty->print_cr("WB error: request to compile NULL method");
    return false;
  }
  if (comp_level > MIN2((CompLevel) TieredStopAtLevel, CompLevel_highest_tier)) {
    tty->print_cr("WB error: invalid compilation level %d", comp_level);
    return false;
  }
  if (comp == NULL) {
    tty->print_cr("WB error: no compiler for requested compilation level %d", comp_level);
    return false;
  }

  methodHandle mh(THREAD, method);

  // Compile method and check result
  nmethod* nm = CompileBroker::compile_method(mh, bci, comp_level, mh, mh->invocation_count(), "Whitebox", THREAD);
  MutexLocker mu(Compile_lock);
  bool is_queued = mh->queued_for_compilation();
  if (is_queued || nm != NULL) {
    return true;
  }
  tty->print("WB error: failed to compile at level %d method ", comp_level);
  mh->print_short_name(tty);
  tty->cr();
  if (is_queued) {
    tty->print_cr("WB error: blocking compilation is still in queue!");
  }
  return false;
}

WB_ENTRY(jboolean, WB_EnqueueMethodForCompilation(JNIEnv* env, jobject o, jobject method, jint comp_level, jint bci))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION_(env, JNI_FALSE);
  return WhiteBox::compile_method(Method::checked_resolve_jmethod_id(jmid), comp_level, bci, THREAD);
WB_END

WB_ENTRY(jboolean, WB_EnqueueInitializerForCompilation(JNIEnv* env, jobject o, jclass klass, jint comp_level))
  InstanceKlass* ik = InstanceKlass::cast(java_lang_Class::as_Klass(JNIHandles::resolve(klass)));
  Method* clinit = ik->class_initializer();
  if (clinit == NULL) {
    return false;
  }
  return WhiteBox::compile_method(clinit, comp_level, InvocationEntryBci, THREAD);
WB_END

class VM_WhiteBoxOperation : public VM_Operation {
 public:
  VM_WhiteBoxOperation()                         { }
  VMOp_Type type()                  const        { return VMOp_WhiteBoxOperation; }
  bool allow_nested_vm_operations() const        { return true; }
};

static AlwaysFalseClosure always_false;

class VM_WhiteBoxCleanMethodData : public VM_WhiteBoxOperation {
 public:
  VM_WhiteBoxCleanMethodData(MethodData* mdo) : _mdo(mdo) { }
  void doit() {
    _mdo->clean_method_data(&always_false);
  }
 private:
  MethodData* _mdo;
};

WB_ENTRY(void, WB_ClearMethodState(JNIEnv* env, jobject o, jobject method))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION(env);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  MutexLockerEx mu(Compile_lock);
  MethodData* mdo = mh->method_data();
  MethodCounters* mcs = mh->method_counters();

  if (mdo != NULL) {
    mdo->init();
    ResourceMark rm;
    int arg_count = mdo->method()->size_of_parameters();
    for (int i = 0; i < arg_count; i++) {
      mdo->set_arg_modified(i, 0);
    }
    VM_WhiteBoxCleanMethodData op(mdo);
    VMThread::execute(&op);
  }

  mh->clear_not_c1_compilable();
  mh->clear_not_c2_compilable();
  mh->clear_not_c2_osr_compilable();
  NOT_PRODUCT(mh->set_compiled_invocation_count(0));
  if (mcs != NULL) {
    mcs->backedge_counter()->init();
    mcs->invocation_counter()->init();
    mcs->set_interpreter_invocation_count(0);
    mcs->set_interpreter_throwout_count(0);

#ifdef TIERED
    mcs->set_rate(0.0F);
    mh->set_prev_event_count(0);
    mh->set_prev_time(0);
#endif
  }
WB_END

WB_ENTRY(void, WB_MarkMethodProfiled(JNIEnv* env, jobject o, jobject method))
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION(env);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));

  MethodData* mdo = mh->method_data();
  if (mdo == NULL) {
    Method::build_interpreter_method_data(mh, CHECK_AND_CLEAR);
    mdo = mh->method_data();
  }
  mdo->init();
  InvocationCounter* icnt = mdo->invocation_counter();
  InvocationCounter* bcnt = mdo->backedge_counter();
  // set i-counter according to AdvancedThresholdPolicy::is_method_profiled
  // because SimpleThresholdPolicy::call_predicate_helper uses > in jdk8u, that's why we need to plus one.
  icnt->set(InvocationCounter::wait_for_compile, Tier4MinInvocationThreshold + 1);
  bcnt->set(InvocationCounter::wait_for_compile, Tier4CompileThreshold + 1);
WB_END

template <typename T>
static bool GetVMFlag(JavaThread* thread, JNIEnv* env, jstring name, T* value, bool (*TAt)(const char*, T*, bool, bool)) {
  if (name == NULL) {
    return false;
  }
  ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
  const char* flag_name = env->GetStringUTFChars(name, NULL);
  bool result = (*TAt)(flag_name, value, true, true);
  env->ReleaseStringUTFChars(name, flag_name);
  return result;
}

template <typename T>
static bool SetVMFlag(JavaThread* thread, JNIEnv* env, jstring name, T* value, bool (*TAtPut)(const char*, T*, Flag::Flags)) {
  if (name == NULL) {
    return false;
  }
  ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
  const char* flag_name = env->GetStringUTFChars(name, NULL);
  bool result = (*TAtPut)(flag_name, value, Flag::INTERNAL);
  env->ReleaseStringUTFChars(name, flag_name);
  return result;
}

template <typename T>
static jobject box(JavaThread* thread, JNIEnv* env, Symbol* name, Symbol* sig, T value) {
  ResourceMark rm(thread);
  jclass clazz = env->FindClass(name->as_C_string());
  CHECK_JNI_EXCEPTION_(env, NULL);
  jmethodID methodID = env->GetStaticMethodID(clazz,
        vmSymbols::valueOf_name()->as_C_string(),
        sig->as_C_string());
  CHECK_JNI_EXCEPTION_(env, NULL);
  jobject result = env->CallStaticObjectMethod(clazz, methodID, value);
  CHECK_JNI_EXCEPTION_(env, NULL);
  return result;
}

static jobject booleanBox(JavaThread* thread, JNIEnv* env, jboolean value) {
  return box(thread, env, vmSymbols::java_lang_Boolean(), vmSymbols::Boolean_valueOf_signature(), value);
}
static jobject integerBox(JavaThread* thread, JNIEnv* env, jint value) {
  return box(thread, env, vmSymbols::java_lang_Integer(), vmSymbols::Integer_valueOf_signature(), value);
}
static jobject longBox(JavaThread* thread, JNIEnv* env, jlong value) {
  return box(thread, env, vmSymbols::java_lang_Long(), vmSymbols::Long_valueOf_signature(), value);
}
/* static jobject floatBox(JavaThread* thread, JNIEnv* env, jfloat value) {
  return box(thread, env, vmSymbols::java_lang_Float(), vmSymbols::Float_valueOf_signature(), value);
}*/
static jobject doubleBox(JavaThread* thread, JNIEnv* env, jdouble value) {
  return box(thread, env, vmSymbols::java_lang_Double(), vmSymbols::Double_valueOf_signature(), value);
}

WB_ENTRY(jobject, WB_GetBooleanVMFlag(JNIEnv* env, jobject o, jstring name))
  bool result;
  if (GetVMFlag <bool> (thread, env, name, &result, &CommandLineFlags::boolAt)) {
    ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
    return booleanBox(thread, env, result);
  }
  return NULL;
WB_END

WB_ENTRY(jobject, WB_GetIntxVMFlag(JNIEnv* env, jobject o, jstring name))
  intx result;
  if (GetVMFlag <intx> (thread, env, name, &result, &CommandLineFlags::intxAt)) {
    ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
    return longBox(thread, env, result);
  }
  return NULL;
WB_END

WB_ENTRY(jobject, WB_GetUintxVMFlag(JNIEnv* env, jobject o, jstring name))
  uintx result;
  if (GetVMFlag <uintx> (thread, env, name, &result, &CommandLineFlags::uintxAt)) {
    ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
    return longBox(thread, env, result);
  }
  return NULL;
WB_END

WB_ENTRY(jobject, WB_GetUint64VMFlag(JNIEnv* env, jobject o, jstring name))
  uint64_t result;
  if (GetVMFlag <uint64_t> (thread, env, name, &result, &CommandLineFlags::uint64_tAt)) {
    ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
    return longBox(thread, env, result);
  }
  return NULL;
WB_END

WB_ENTRY(jobject, WB_GetDoubleVMFlag(JNIEnv* env, jobject o, jstring name))
  double result;
  if (GetVMFlag <double> (thread, env, name, &result, &CommandLineFlags::doubleAt)) {
    ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
    return doubleBox(thread, env, result);
  }
  return NULL;
WB_END

WB_ENTRY(jstring, WB_GetStringVMFlag(JNIEnv* env, jobject o, jstring name))
  ccstr ccstrResult;
  if (GetVMFlag <ccstr> (thread, env, name, &ccstrResult, &CommandLineFlags::ccstrAt)) {
    ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
    jstring result = env->NewStringUTF(ccstrResult);
    CHECK_JNI_EXCEPTION_(env, NULL);
    return result;
  }
  return NULL;
WB_END

WB_ENTRY(void, WB_SetBooleanVMFlag(JNIEnv* env, jobject o, jstring name, jboolean value))
  bool result = value == JNI_TRUE ? true : false;
  SetVMFlag <bool> (thread, env, name, &result, &CommandLineFlags::boolAtPut);
WB_END

WB_ENTRY(void, WB_SetIntxVMFlag(JNIEnv* env, jobject o, jstring name, jlong value))
  intx result = value;
  SetVMFlag <intx> (thread, env, name, &result, &CommandLineFlags::intxAtPut);
WB_END

WB_ENTRY(void, WB_SetUintxVMFlag(JNIEnv* env, jobject o, jstring name, jlong value))
  uintx result = value;
  SetVMFlag <uintx> (thread, env, name, &result, &CommandLineFlags::uintxAtPut);
WB_END

WB_ENTRY(void, WB_SetUint64VMFlag(JNIEnv* env, jobject o, jstring name, jlong value))
  uint64_t result = value;
  SetVMFlag <uint64_t> (thread, env, name, &result, &CommandLineFlags::uint64_tAtPut);
WB_END

WB_ENTRY(void, WB_SetDoubleVMFlag(JNIEnv* env, jobject o, jstring name, jdouble value))
  double result = value;
  SetVMFlag <double> (thread, env, name, &result, &CommandLineFlags::doubleAtPut);
WB_END

WB_ENTRY(void, WB_SetStringVMFlag(JNIEnv* env, jobject o, jstring name, jstring value))
  ThreadToNativeFromVM ttnfv(thread);   // can't be in VM when we call JNI
  const char* ccstrValue = (value == NULL) ? NULL : env->GetStringUTFChars(value, NULL);
  ccstr ccstrResult = ccstrValue;
  bool needFree;
  {
    ThreadInVMfromNative ttvfn(thread); // back to VM
    needFree = SetVMFlag <ccstr> (thread, env, name, &ccstrResult, &CommandLineFlags::ccstrAtPut);
  }
  if (value != NULL) {
    env->ReleaseStringUTFChars(value, ccstrValue);
  }
  if (needFree) {
    FREE_C_HEAP_ARRAY(char, ccstrResult, mtInternal);
  }
WB_END

WB_ENTRY(jboolean, WB_IsInStringTable(JNIEnv* env, jobject o, jstring javaString))
  ResourceMark rm(THREAD);
  int len;
  jchar* name = java_lang_String::as_unicode_string(JNIHandles::resolve(javaString), len, CHECK_false);
  return (StringTable::lookup(name, len) != NULL);
WB_END

WB_ENTRY(void, WB_FullGC(JNIEnv* env, jobject o))
  Universe::heap()->collector_policy()->set_should_clear_all_soft_refs(true);
  Universe::heap()->collect(GCCause::_last_ditch_collection);
#if INCLUDE_ALL_GCS
  if (UseG1GC) {
    // Needs to be cleared explicitly for G1
    Universe::heap()->collector_policy()->set_should_clear_all_soft_refs(false);
  }
#endif // INCLUDE_ALL_GCS
WB_END

WB_ENTRY(void, WB_YoungGC(JNIEnv* env, jobject o))
  Universe::heap()->collect(GCCause::_wb_young_gc);
WB_END

WB_ENTRY(void, WB_ReadReservedMemory(JNIEnv* env, jobject o))
  // static+volatile in order to force the read to happen
  // (not be eliminated by the compiler)
  static char c;
  static volatile char* p;

  p = os::reserve_memory(os::vm_allocation_granularity(), NULL, 0);
  if (p == NULL) {
    THROW_MSG(vmSymbols::java_lang_OutOfMemoryError(), "Failed to reserve memory");
  }

  c = *p;
WB_END

WB_ENTRY(jstring, WB_GetCPUFeatures(JNIEnv* env, jobject o))
  const char* cpu_features = VM_Version::cpu_features();
  ThreadToNativeFromVM ttn(thread);
  jstring features_string = env->NewStringUTF(cpu_features);

  CHECK_JNI_EXCEPTION_(env, NULL);

  return features_string;
WB_END

WB_ENTRY(void, WB_GCLockCritical(JNIEnv* env, jobject o))
  GC_locker::lock_critical(thread);
WB_END

WB_ENTRY(void, WB_GCUnlockCritical(JNIEnv* env, jobject o))
  GC_locker::unlock_critical(thread);
WB_END

int WhiteBox::get_blob_type(const CodeBlob* code) {
  guarantee(WhiteBoxAPI, "internal testing API :: WhiteBox has to be enabled");
  return CodeBlobType::All;;
}

struct CodeBlobStub {
  CodeBlobStub(const CodeBlob* blob) :
      name(os::strdup(blob->name())),
      size(blob->size()),
      blob_type(WhiteBox::get_blob_type(blob)),
      address((jlong) blob) { }
  ~CodeBlobStub() { os::free((void*) name); }
  const char* const name;
  const jint        size;
  const jint        blob_type;
  const jlong       address;
};

static jobjectArray codeBlob2objectArray(JavaThread* thread, JNIEnv* env, CodeBlobStub* cb) {
  jclass clazz = env->FindClass(vmSymbols::java_lang_Object()->as_C_string());
  CHECK_JNI_EXCEPTION_(env, NULL);
  jobjectArray result = env->NewObjectArray(4, clazz, NULL);

  jstring name = env->NewStringUTF(cb->name);
  CHECK_JNI_EXCEPTION_(env, NULL);
  env->SetObjectArrayElement(result, 0, name);

  jobject obj = integerBox(thread, env, cb->size);
  CHECK_JNI_EXCEPTION_(env, NULL);
  env->SetObjectArrayElement(result, 1, obj);

  obj = integerBox(thread, env, cb->blob_type);
  CHECK_JNI_EXCEPTION_(env, NULL);
  env->SetObjectArrayElement(result, 2, obj);

  obj = longBox(thread, env, cb->address);
  CHECK_JNI_EXCEPTION_(env, NULL);
  env->SetObjectArrayElement(result, 3, obj);

  return result;
}

WB_ENTRY(jobjectArray, WB_GetNMethod(JNIEnv* env, jobject o, jobject method, jboolean is_osr))
  ResourceMark rm(THREAD);
  jmethodID jmid = reflected_method_to_jmid(thread, env, method);
  CHECK_JNI_EXCEPTION_(env, NULL);
  methodHandle mh(THREAD, Method::checked_resolve_jmethod_id(jmid));
  nmethod* code = is_osr ? mh->lookup_osr_nmethod_for(InvocationEntryBci, CompLevel_none, false) : mh->code();
  jobjectArray result = NULL;
  if (code == NULL) {
    return result;
  }
  int insts_size = code->insts_size();

  ThreadToNativeFromVM ttn(thread);
  jclass clazz = env->FindClass(vmSymbols::java_lang_Object()->as_C_string());
  CHECK_JNI_EXCEPTION_(env, NULL);
  result = env->NewObjectArray(3, clazz, NULL);
  if (result == NULL) {
    return result;
  }

  jobject level = integerBox(thread, env, code->comp_level());
  CHECK_JNI_EXCEPTION_(env, NULL);
  env->SetObjectArrayElement(result, 0, level);

  jobject id = integerBox(thread, env, code->compile_id());
  CHECK_JNI_EXCEPTION_(env, NULL);
  env->SetObjectArrayElement(result, 1, id);

  jbyteArray insts = env->NewByteArray(insts_size);
  CHECK_JNI_EXCEPTION_(env, NULL);
  env->SetByteArrayRegion(insts, 0, insts_size, (jbyte*) code->insts_begin());
  env->SetObjectArrayElement(result, 2, insts);

  return result;
WB_END

CodeBlob* WhiteBox::allocate_code_blob(int size, int blob_type) {
  guarantee(WhiteBoxAPI, "internal testing API :: WhiteBox has to be enabled");
  BufferBlob* blob;
  int full_size = CodeBlob::align_code_offset(sizeof(BufferBlob));
  if (full_size < size) {
    full_size += align_up(size - full_size, oopSize);
  }
  {
    MutexLockerEx mu(CodeCache_lock, Mutex::_no_safepoint_check_flag);
    blob = (BufferBlob*) CodeCache::allocate(full_size);
    ::new (blob) BufferBlob("WB::DummyBlob", full_size);
  }
  // Track memory usage statistic after releasing CodeCache_lock
  MemoryService::track_code_cache_memory_usage();
  return blob;
}

WB_ENTRY(jlong, WB_AllocateCodeBlob(JNIEnv* env, jobject o, jint size, jint blob_type))
  if (size < 0) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(),
      err_msg("WB_AllocateCodeBlob: size is negative: " INT32_FORMAT, size));
  }
  return (jlong) WhiteBox::allocate_code_blob(size, blob_type);
WB_END

WB_ENTRY(void, WB_FreeCodeBlob(JNIEnv* env, jobject o, jlong addr))
  if (addr == 0) {
    return;
  }
  BufferBlob::free((BufferBlob*) addr);
WB_END

WB_ENTRY(jobjectArray, WB_GetCodeBlob(JNIEnv* env, jobject o, jlong addr))
  if (addr == 0) {
    THROW_MSG_NULL(vmSymbols::java_lang_NullPointerException(),
      "WB_GetCodeBlob: addr is null");
  }
  ThreadToNativeFromVM ttn(thread);
  CodeBlobStub stub((CodeBlob*) addr);
  return codeBlob2objectArray(thread, env, &stub);
WB_END

int WhiteBox::array_bytes_to_length(size_t bytes) {
  return Array<u1>::bytes_to_length(bytes);
}

WB_ENTRY(jlong, WB_AllocateMetaspace(JNIEnv* env, jobject wb, jobject class_loader, jlong size))
  if (size < 0) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(),
        err_msg("WB_AllocateMetaspace: size is negative: " JLONG_FORMAT, size));
  }

  oop class_loader_oop = JNIHandles::resolve(class_loader);
  ClassLoaderData* cld = class_loader_oop != NULL
      ? java_lang_ClassLoader::loader_data(class_loader_oop)
      : ClassLoaderData::the_null_class_loader_data();

  void* metadata = MetadataFactory::new_writeable_array<u1>(cld, WhiteBox::array_bytes_to_length((size_t)size), thread);

  return (jlong)(uintptr_t)metadata;
WB_END

WB_ENTRY(void, WB_FreeMetaspace(JNIEnv* env, jobject wb, jobject class_loader, jlong addr, jlong size))
  oop class_loader_oop = JNIHandles::resolve(class_loader);
  ClassLoaderData* cld = class_loader_oop != NULL
      ? java_lang_ClassLoader::loader_data(class_loader_oop)
      : ClassLoaderData::the_null_class_loader_data();

  MetadataFactory::free_array(cld, (Array<u1>*)(uintptr_t)addr);
WB_END

WB_ENTRY(jlong, WB_IncMetaspaceCapacityUntilGC(JNIEnv* env, jobject wb, jlong inc))
  if (inc < 0) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(),
        err_msg("WB_IncMetaspaceCapacityUntilGC: inc is negative: " JLONG_FORMAT, inc));
  }

  jlong max_size_t = (jlong) ((size_t) -1);
  if (inc > max_size_t) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(),
        err_msg("WB_IncMetaspaceCapacityUntilGC: inc does not fit in size_t: " JLONG_FORMAT, inc));
  }

  size_t new_cap_until_GC = 0;
  size_t aligned_inc = align_size_down((size_t) inc, Metaspace::commit_alignment());
  bool success = MetaspaceGC::inc_capacity_until_GC(aligned_inc, &new_cap_until_GC);
  if (!success) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalStateException(),
                "WB_IncMetaspaceCapacityUntilGC: could not increase capacity until GC "
                "due to contention with another thread");
  }
  return (jlong) new_cap_until_GC;
WB_END

WB_ENTRY(jlong, WB_MetaspaceCapacityUntilGC(JNIEnv* env, jobject wb))
  return (jlong) MetaspaceGC::capacity_until_GC();
WB_END

WB_ENTRY(jboolean, WB_IsSharedClass(JNIEnv* env, jobject wb, jclass clazz))
  return (jboolean)MetaspaceShared::is_in_shared_space(java_lang_Class::as_Klass(JNIHandles::resolve_non_null(clazz)));
WB_END

WB_ENTRY(jboolean, WB_IsMonitorInflated(JNIEnv* env, jobject wb, jobject obj))
  oop obj_oop = JNIHandles::resolve(obj);
  return (jboolean) obj_oop->mark()->has_monitor();
WB_END

WB_ENTRY(void, WB_ForceSafepoint(JNIEnv* env, jobject wb))
  VM_ForceSafepoint force_safepoint_op;
  VMThread::execute(&force_safepoint_op);
WB_END

WB_ENTRY(jlong, WB_GetHeapAlignment(JNIEnv* env, jobject o))
    size_t alignment = Universe::heap()->collector_policy()->heap_alignment();
    return (jlong)alignment;
WB_END

//Some convenience methods to deal with objects from java
int WhiteBox::offset_for_field(const char* field_name, oop object,
    Symbol* signature_symbol) {
  assert(field_name != NULL && strlen(field_name) > 0, "Field name not valid");
  Thread* THREAD = Thread::current();

  //Get the class of our object
  Klass* arg_klass = object->klass();
  //Turn it into an instance-klass
  InstanceKlass* ik = InstanceKlass::cast(arg_klass);

  //Create symbols to look for in the class
  TempNewSymbol name_symbol = SymbolTable::lookup(field_name, (int) strlen(field_name),
      THREAD);

  //To be filled in with an offset of the field we're looking for
  fieldDescriptor fd;

  Klass* res = ik->find_field(name_symbol, signature_symbol, &fd);
  if (res == NULL) {
    tty->print_cr("Invalid layout of %s at %s", ik->external_name(),
        name_symbol->as_C_string());
    vm_exit_during_initialization("Invalid layout of preloaded class: use -XX:+TraceClassLoading to see the origin of the problem class");
  }

  //fetch the field at the offset we've found
  int dest_offset = fd.offset();

  return dest_offset;
}


const char* WhiteBox::lookup_jstring(const char* field_name, oop object) {
  int offset = offset_for_field(field_name, object,
      vmSymbols::string_signature());
  oop string = object->obj_field(offset);
  if (string == NULL) {
    return NULL;
  }
  const char* ret = java_lang_String::as_utf8_string(string);
  return ret;
}

bool WhiteBox::lookup_bool(const char* field_name, oop object) {
  int offset =
      offset_for_field(field_name, object, vmSymbols::bool_signature());
  bool ret = (object->bool_field(offset) == JNI_TRUE);
  return ret;
}

void WhiteBox::register_methods(JNIEnv* env, jclass wbclass, JavaThread* thread, JNINativeMethod* method_array, int method_count) {
  ResourceMark rm;
  ThreadToNativeFromVM ttnfv(thread); // can't be in VM when we call JNI

  //  one by one registration natives for exception catching
  jclass no_such_method_error_klass = env->FindClass(vmSymbols::java_lang_NoSuchMethodError()->as_C_string());
  CHECK_JNI_EXCEPTION(env);
  for (int i = 0, n = method_count; i < n; ++i) {
    // Skip dummy entries
    if (method_array[i].fnPtr == NULL) continue;
    if (env->RegisterNatives(wbclass, &method_array[i], 1) != 0) {
      jthrowable throwable_obj = env->ExceptionOccurred();
      if (throwable_obj != NULL) {
        env->ExceptionClear();
        if (env->IsInstanceOf(throwable_obj, no_such_method_error_klass)) {
          // NoSuchMethodError is thrown when a method can't be found or a method is not native.
          // Ignoring the exception since it is not preventing use of other WhiteBox methods.
          tty->print_cr("Warning: 'NoSuchMethodError' on register of sun.hotspot.WhiteBox::%s%s",
              method_array[i].name, method_array[i].signature);
        }
      } else {
        // Registration failed unexpectedly.
        tty->print_cr("Warning: unexpected error on register of sun.hotspot.WhiteBox::%s%s. All methods will be unregistered",
            method_array[i].name, method_array[i].signature);
        env->UnregisterNatives(wbclass);
        break;
      }
    }
  }
}

// Checks that the library libfile has the noexecstack bit set.
WB_ENTRY(jboolean, WB_CheckLibSpecifiesNoexecstack(JNIEnv* env, jobject o, jstring libfile))
  jboolean ret = false;
#ifdef LINUX
  // Can't be in VM when we call JNI.
  ThreadToNativeFromVM ttnfv(thread);
  const char* lf = env->GetStringUTFChars(libfile, NULL);
  CHECK_JNI_EXCEPTION_(env, 0);
  ElfFile ef(lf);
  ret = (jboolean) ef.specifies_noexecstack();
  env->ReleaseStringUTFChars(libfile, lf);
#endif
  return ret;
WB_END

WB_ENTRY(jboolean, WB_IsContainerized(JNIEnv* env, jobject o))
  LINUX_ONLY(return OSContainer::is_containerized();)
  return false;
WB_END

WB_ENTRY(jint, WB_ValidateCgroup(JNIEnv* env,
                                    jobject o,
                                    jstring proc_cgroups,
                                    jstring proc_self_cgroup,
                                    jstring proc_self_mountinfo))
  jint ret = 0;
#ifdef LINUX
  ThreadToNativeFromVM ttnfv(thread);
  const char* p_cgroups = env->GetStringUTFChars(proc_cgroups, NULL);
  CHECK_JNI_EXCEPTION_(env, 0);
  const char* p_s_cgroup = env->GetStringUTFChars(proc_self_cgroup, NULL);
  CHECK_JNI_EXCEPTION_(env, 0);
  const char* p_s_mountinfo = env->GetStringUTFChars(proc_self_mountinfo, NULL);
  CHECK_JNI_EXCEPTION_(env, 0);
  u1 cg_type_flags = 0;
  // This sets cg_type_flags
  WhiteBox::validate_cgroup(p_cgroups, p_s_cgroup, p_s_mountinfo, &cg_type_flags);
  ret = (jint)cg_type_flags;
  env->ReleaseStringUTFChars(proc_cgroups, p_cgroups);
  env->ReleaseStringUTFChars(proc_self_cgroup, p_s_cgroup);
  env->ReleaseStringUTFChars(proc_self_mountinfo, p_s_mountinfo);
#endif
  return ret;
WB_END

WB_ENTRY(void, WB_PrintOsInfo(JNIEnv* env, jobject o))
  os::print_os_info(tty);
WB_END

WB_ENTRY(jobjectArray, WB_GetClassInitOrderList(JNIEnv* env, jobject o))
  ResourceMark rm(THREAD);
  ThreadToNativeFromVM ttn(thread);
  jclass clazz = env->FindClass(vmSymbols::java_lang_String()->as_C_string());
  CHECK_JNI_EXCEPTION_(env, NULL);
  if (!CompilationWarmUpRecording) {
    return NULL;
  }
  LinkedListImpl<ClassSymbolEntry>* lst =
    JitWarmUp::instance()->recorder()->class_init_list();
  if (lst == NULL) {
    return NULL;
  }

  jsize size = (jsize)lst->size();
  jobjectArray result = NULL;
  result = env->NewObjectArray(size, clazz, NULL);
  if (result == NULL) {
    return result;
  }

  int idx = 0;
  LinkedListNode<ClassSymbolEntry>* node = lst->head();
  while(node != NULL) {
    ResourceMark rm_inner(THREAD);
    Symbol* class_name = node->peek()->class_name();
    jobject obj = env->NewStringUTF(class_name->as_C_string());
    CHECK_JNI_EXCEPTION_(env, NULL);
    assert(idx < size, "index out of bound");
    env->SetObjectArrayElement(result, idx, obj);
    idx++;
    node = node->next();
  }
  return result;
WB_END

WB_ENTRY(jobjectArray, WB_GetClassChainSymbolList(JNIEnv* env, jobject o))
  ResourceMark rm(THREAD);
  ThreadToNativeFromVM ttn(thread);
  jclass clazz = env->FindClass(vmSymbols::java_lang_String()->as_C_string());
  CHECK_JNI_EXCEPTION_(env, NULL);
  if (!CompilationWarmUp) {
    return NULL;
  }

  PreloadClassChain* chain = JitWarmUp::instance()->preloader()->chain();
  if (chain == NULL) {
    return NULL;
  }

  jsize size = (jsize)chain->length();
  jobjectArray result = NULL;
  result = env->NewObjectArray(size, clazz, NULL);
  if (result == NULL) {
    return result;
  }

  for (int i = 0; i < size; i++) {
    ResourceMark rm_inner(THREAD);
    Symbol* name = chain->at(i)->class_name();
    if (name == NULL) {
      continue;
    }
    jobject obj = env->NewStringUTF(name->as_C_string());
    CHECK_JNI_EXCEPTION_(env, NULL);
    env->SetObjectArrayElement(result, i, obj);
  }

  return result;
WB_END

WB_ENTRY(jintArray, WB_GetClassChainStateList(JNIEnv* env, jobject o))
  ResourceMark rm(THREAD);
  ThreadToNativeFromVM ttn(thread);
  CHECK_JNI_EXCEPTION_(env, NULL);
  if (!CompilationWarmUp) {
    return NULL;
  }
  PreloadClassChain* chain = JitWarmUp::instance()->preloader()->chain();
  if (chain == NULL) {
    return NULL;
  }

  jsize size = (jsize)chain->length();
  jintArray result = NULL;
  result = env->NewIntArray(size);
  if (result == NULL) {
    return result;
  }

  jint* arr = env->GetIntArrayElements(result, NULL);
  for (int i = 0; i < size; i++) {
    arr[i] = chain->at(i)->state();
  }

  env->ReleaseIntArrayElements(result, arr, 0);
  return result;
WB_END

WB_ENTRY(jobjectArray, WB_GetCompiledMethodList(JNIEnv* env, jobject o))
  ResourceMark rm(THREAD);
  ThreadToNativeFromVM ttn(thread);
  jclass clazz = env->FindClass(vmSymbols::java_lang_String()->as_C_string());
  CHECK_JNI_EXCEPTION_(env, NULL);

  if (!CompilationWarmUpRecording) {
    return NULL;
  }
  ProfileRecordDictionary* dict = JitWarmUp::instance()->recorder()->dict();
  if (dict == NULL) {
    return NULL;
  }

  jsize size = (jsize)dict->count();
  jobjectArray result = NULL;
  result = env->NewObjectArray(size, clazz, NULL);
  if (result == NULL) {
    return result;
  }

  int count = 0;
  for (int index = 0; index < dict->table_size(); index++) {
    for (ProfileRecorderEntry* entry = dict->bucket(index);
                               entry != NULL;
                               entry = entry->next()) {
      ResourceMark rm_inner(THREAD);
      Symbol* method_name = entry->literal()->name();
      jobject obj = env->NewStringUTF(method_name->as_C_string());
      CHECK_JNI_EXCEPTION_(env, NULL);
      if (count >= size) {
        break;
      }
      env->SetObjectArrayElement(result, count, obj);
      count++;
    }
  }

  return result;
WB_END

WB_ENTRY(jobjectArray, WB_GetClassListFromLogfile(JNIEnv* env, jobject o))
  ResourceMark rm(THREAD);
  ThreadToNativeFromVM ttn(thread);
  jclass clazz = env->FindClass(vmSymbols::java_lang_String()->as_C_string());
  CHECK_JNI_EXCEPTION_(env, NULL);
  JitWarmUp* jwp = JitWarmUp::instance();
  if (jwp == NULL) {
    return NULL;
  }
  PreloadJitInfo* preloader = jwp->preloader();
  if (preloader == NULL) {
    return NULL;
  }
  PreloadClassDictionary* dict = preloader->dict();
  if (dict == NULL) {
    return NULL;
  }

  int entry_count = 0;
  for (int index = 0; index < dict->table_size(); index++) {
    for (PreloadClassEntry* entry = dict->bucket(index);
                            entry != NULL;
                            entry = entry->next()) {
      entry_count++;
    }
  }

  jsize size = (jsize)entry_count;
  jobjectArray result = NULL;
  result = env->NewObjectArray(size, clazz, NULL);
  if (result == NULL) {
    return result;
  }

  int count = 0;
  for (int index = 0; index < dict->table_size(); index++) {
    for (PreloadClassEntry* entry = dict->bucket(index);
                            entry != NULL;
                            entry = entry->next()) {
      ResourceMark rm_inner(THREAD);
      Symbol* class_name = entry->literal();
      jobject obj = env->NewStringUTF(class_name->as_C_string());
      CHECK_JNI_EXCEPTION_(env, NULL);
      //assert(index < size, "index out of bound");
      if (count >= size) {
        break;
      }
      env->SetObjectArrayElement(result, count, obj);
      count++;
    }
  }
  return result;
WB_END

WB_ENTRY(jobjectArray, WB_GetMethodListFromLogfile(JNIEnv* env, jobject o))
  ResourceMark rm(THREAD);
  ThreadToNativeFromVM ttn(thread);
  jclass clazz = env->FindClass(vmSymbols::java_lang_String()->as_C_string());
  CHECK_JNI_EXCEPTION_(env, NULL);

  JitWarmUp* jwp = JitWarmUp::instance();
  if (jwp == NULL) {
    return NULL;
  }
  PreloadJitInfo* preloader = jwp->preloader();
  if (preloader == NULL) {
    return NULL;
  }
  PreloadClassDictionary* dict = preloader->dict();
  if (dict == NULL) {
    return NULL;
  }

  jsize size = (jsize)preloader->loaded_count();
  jobjectArray result = NULL;
  result = env->NewObjectArray(size, clazz, NULL);
  if (result == NULL) {
    return result;
  }

  int count = 0;
  for (int index = 0; index < dict->table_size(); index++) {
    for (PreloadClassEntry* entry = dict->bucket(index);
                            entry != NULL;
                            entry = entry->next()) {
      for (PreloadClassHolder* holder = entry->head_holder();
                               holder != NULL;
                               holder = holder->next()) {
        GrowableArray<PreloadMethodHolder*>* arr = holder->method_list();
        if (arr == NULL) {
          continue;
        }
        int arr_len = arr->length();
        for (int i = 0; i < arr_len; i++ ) {
          PreloadMethodHolder* mh = arr->at(i);
          ResourceMark rm_inner(THREAD);
          Symbol* method_name = mh->name();
          jobject obj = env->NewStringUTF(method_name->as_C_string());
          CHECK_JNI_EXCEPTION_(env, NULL);
          //assert(index < size, "index out of bound");
          if (count >= size) {
            break;
          }
          env->SetObjectArrayElement(result, count, obj);
          count++;
        } // end of method list loop
      } // end of class holder loop
    } // end of entry bucket loop
  } // end of table size loop
  return result;
WB_END

WB_ENTRY(jboolean, WB_TestFixDanglingPointerInDeopt(JNIEnv* env, jobject o, jstring name))
  Handle h_name(THREAD, JNIHandles::resolve(name));
  if (h_name.is_null()) return false;
  Symbol* sym = java_lang_String::as_symbol(h_name(), CHECK_false);

  JitWarmUp* jwp = JitWarmUp::instance();
  PreloadClassChain* chain = jwp->preloader()->chain();
  assert(chain != NULL, "sanity check");
  int index = chain->length() - 1;
  PreloadMethodHolder* begin_holder = NULL;
  while (index > 0 && begin_holder == NULL) {
    PreloadClassChain::PreloadClassChainEntry* entry = chain->at(index);
    begin_holder = entry->method_holder();
    index--;
  }
  if (begin_holder == NULL) {
    return false;
  }
  MethodHolderIterator iter(chain, begin_holder, index);
  while (*iter != NULL) {
    PreloadMethodHolder* pmh = *iter;
    if (pmh->name()->fast_compare(sym) == 0) {
      if (pmh->resolved_method() != NULL) {
        return false;
      }
    }
    iter.next();
  }
  return true;
WB_END

WB_ENTRY(jboolean, WB_IsInCurrentTLAB(JNIEnv* env, jobject wb, jobject o))
  ThreadToNativeFromVM ttn(thread);
  if (o != NULL) {
    HeapWord* addr = (HeapWord*)JNIHandles::resolve_non_null(o);
    ThreadLocalAllocBuffer& tlab = Thread::current()->tlab();
    return (addr >= tlab.start() && addr < tlab.end()) ? JNI_TRUE : JNI_FALSE;
  }
  return JNI_FALSE;
WB_END

#if INCLUDE_CDS
WB_ENTRY(jint, WB_GetOffsetForName(JNIEnv* env, jobject o, jstring name))
  ResourceMark rm;
  char* c_name = java_lang_String::as_utf8_string(JNIHandles::resolve_non_null(name));
  int result = CDSOffsets::find_offset(c_name);
  return (jint)result;
WB_END
#endif // INCLUDE_CDS

#define CC (char*)

static JNINativeMethod methods[] = {
  {CC"getObjectAddress",   CC"(Ljava/lang/Object;)J", (void*)&WB_GetObjectAddress  },
  {CC"getObjectSize",      CC"(Ljava/lang/Object;)J", (void*)&WB_GetObjectSize     },
  {CC"isObjectInOldGen",   CC"(Ljava/lang/Object;)Z", (void*)&WB_isObjectInOldGen  },
  {CC"getHeapOopSize",     CC"()I",                   (void*)&WB_GetHeapOopSize    },
  {CC"getVMPageSize",      CC"()I",                   (void*)&WB_GetVMPageSize     },
  {CC"getVMLargePageSize", CC"()J",                   (void*)&WB_GetVMLargePageSize},
  {CC"getHeapAlignment",   CC"()J",                   (void*)&WB_GetHeapAlignment  },
  {CC"isClassAlive0",      CC"(Ljava/lang/String;)Z", (void*)&WB_IsClassAlive      },
  {CC"classKnownToNotExist",
                           CC"(Ljava/lang/ClassLoader;Ljava/lang/String;)Z",(void*)&WB_ClassKnownToNotExist},
  {CC"getLookupCacheURLs", CC"(Ljava/lang/ClassLoader;)[Ljava/net/URL;",    (void*)&WB_GetLookupCacheURLs},
  {CC"getLookupCacheMatches", CC"(Ljava/lang/ClassLoader;Ljava/lang/String;)[I",
                                                      (void*)&WB_GetLookupCacheMatches},
  {CC"parseCommandLine",
      CC"(Ljava/lang/String;[Lsun/hotspot/parser/DiagnosticCommand;)[Ljava/lang/Object;",
      (void*) &WB_ParseCommandLine
  },
  {CC"addToBootstrapClassLoaderSearch", CC"(Ljava/lang/String;)V",
                                                      (void*)&WB_AddToBootstrapClassLoaderSearch},
  {CC"addToSystemClassLoaderSearch",    CC"(Ljava/lang/String;)V",
                                                      (void*)&WB_AddToSystemClassLoaderSearch},
  {CC"getCompressedOopsMaxHeapSize", CC"()J",
      (void*)&WB_GetCompressedOopsMaxHeapSize},
  {CC"printHeapSizes",     CC"()V",                   (void*)&WB_PrintHeapSizes    },
  {CC"runMemoryUnitTests", CC"()V",                   (void*)&WB_RunMemoryUnitTests},
  {CC"readFromNoaccessArea",CC"()V",                  (void*)&WB_ReadFromNoaccessArea},
  {CC"stressVirtualSpaceResize",CC"(JJJ)I",           (void*)&WB_StressVirtualSpaceResize},
  {CC"isSharedClass", CC"(Ljava/lang/Class;)Z",       (void*)&WB_IsSharedClass },
#if INCLUDE_CDS
  {CC"getOffsetForName0", CC"(Ljava/lang/String;)I",  (void*)&WB_GetOffsetForName},
#endif
#if INCLUDE_ALL_GCS
  {CC"g1InConcurrentMark", CC"()Z",                   (void*)&WB_G1InConcurrentMark},
  {CC"g1IsHumongous",      CC"(Ljava/lang/Object;)Z", (void*)&WB_G1IsHumongous     },
  {CC"g1NumMaxRegions",    CC"()J",                   (void*)&WB_G1NumMaxRegions  },
  {CC"g1NumFreeRegions",   CC"()J",                   (void*)&WB_G1NumFreeRegions  },
  {CC"g1RegionSize",       CC"()I",                   (void*)&WB_G1RegionSize      },
  {CC"g1StartConcMarkCycle",       CC"()Z",           (void*)&WB_G1StartMarkCycle  },
  {CC"g1AuxiliaryMemoryUsage", CC"()Ljava/lang/management/MemoryUsage;",
                                                      (void*)&WB_G1AuxiliaryMemoryUsage  },
#endif // INCLUDE_ALL_GCS
#if INCLUDE_NMT
  {CC"NMTMalloc",           CC"(J)J",                 (void*)&WB_NMTMalloc          },
  {CC"NMTMallocWithPseudoStack", CC"(JI)J",           (void*)&WB_NMTMallocWithPseudoStack},
  {CC"NMTMallocWithPseudoStackAndType", CC"(JII)J",   (void*)&WB_NMTMallocWithPseudoStackAndType},
  {CC"NMTFree",             CC"(J)V",                 (void*)&WB_NMTFree            },
  {CC"NMTReserveMemory",    CC"(J)J",                 (void*)&WB_NMTReserveMemory   },
  {CC"NMTCommitMemory",     CC"(JJ)V",                (void*)&WB_NMTCommitMemory    },
  {CC"NMTUncommitMemory",   CC"(JJ)V",                (void*)&WB_NMTUncommitMemory  },
  {CC"NMTReleaseMemory",    CC"(JJ)V",                (void*)&WB_NMTReleaseMemory   },
  {CC"NMTIsDetailSupported",CC"()Z",                  (void*)&WB_NMTIsDetailSupported},
  {CC"NMTChangeTrackingLevel", CC"()Z",               (void*)&WB_NMTChangeTrackingLevel},
  {CC"NMTGetHashSize",      CC"()I",                  (void*)&WB_NMTGetHashSize     },
  {CC"NMTNewArena",         CC"(J)J",                 (void*)&WB_NMTNewArena        },
  {CC"NMTFreeArena",        CC"(J)V",                 (void*)&WB_NMTFreeArena       },
  {CC"NMTArenaMalloc",      CC"(JJ)V",                (void*)&WB_NMTArenaMalloc     },
#endif // INCLUDE_NMT
  {CC"deoptimizeAll",      CC"()V",                   (void*)&WB_DeoptimizeAll     },
  {CC"deoptimizeMethod",   CC"(Ljava/lang/reflect/Executable;Z)I",
                                                      (void*)&WB_DeoptimizeMethod  },
  {CC"isMethodCompiled",   CC"(Ljava/lang/reflect/Executable;Z)Z",
                                                      (void*)&WB_IsMethodCompiled  },
  {CC"isMethodCompilable", CC"(Ljava/lang/reflect/Executable;IZ)Z",
                                                      (void*)&WB_IsMethodCompilable},
  {CC"isMethodQueuedForCompilation",
      CC"(Ljava/lang/reflect/Executable;)Z",          (void*)&WB_IsMethodQueuedForCompilation},
  {CC"makeMethodNotCompilable",
      CC"(Ljava/lang/reflect/Executable;IZ)V",        (void*)&WB_MakeMethodNotCompilable},
  {CC"testSetDontInlineMethod",
      CC"(Ljava/lang/reflect/Executable;Z)Z",         (void*)&WB_TestSetDontInlineMethod},
  {CC"getMethodCompilationLevel",
      CC"(Ljava/lang/reflect/Executable;Z)I",         (void*)&WB_GetMethodCompilationLevel},
  {CC"getMethodEntryBci",
      CC"(Ljava/lang/reflect/Executable;)I",          (void*)&WB_GetMethodEntryBci},
  {CC"getCompileQueueSize",
      CC"(I)I",                                       (void*)&WB_GetCompileQueueSize},
  {CC"testSetForceInlineMethod",
      CC"(Ljava/lang/reflect/Executable;Z)Z",         (void*)&WB_TestSetForceInlineMethod},
  {CC"enqueueMethodForCompilation0",
      CC"(Ljava/lang/reflect/Executable;II)Z",        (void*)&WB_EnqueueMethodForCompilation},
  {CC"enqueueInitializerForCompilation0",
      CC"(Ljava/lang/Class;I)Z",                      (void*)&WB_EnqueueInitializerForCompilation},
  {CC"clearMethodState",
      CC"(Ljava/lang/reflect/Executable;)V",          (void*)&WB_ClearMethodState},
  {CC"markMethodProfiled",
      CC"(Ljava/lang/reflect/Executable;)V",          (void*)&WB_MarkMethodProfiled},
  {CC"setBooleanVMFlag",   CC"(Ljava/lang/String;Z)V",(void*)&WB_SetBooleanVMFlag},
  {CC"setIntxVMFlag",      CC"(Ljava/lang/String;J)V",(void*)&WB_SetIntxVMFlag},
  {CC"setUintxVMFlag",     CC"(Ljava/lang/String;J)V",(void*)&WB_SetUintxVMFlag},
  {CC"setUint64VMFlag",    CC"(Ljava/lang/String;J)V",(void*)&WB_SetUint64VMFlag},
  {CC"setDoubleVMFlag",    CC"(Ljava/lang/String;D)V",(void*)&WB_SetDoubleVMFlag},
  {CC"setStringVMFlag",    CC"(Ljava/lang/String;Ljava/lang/String;)V",
                                                      (void*)&WB_SetStringVMFlag},
  {CC"getBooleanVMFlag",   CC"(Ljava/lang/String;)Ljava/lang/Boolean;",
                                                      (void*)&WB_GetBooleanVMFlag},
  {CC"getIntxVMFlag",      CC"(Ljava/lang/String;)Ljava/lang/Long;",
                                                      (void*)&WB_GetIntxVMFlag},
  {CC"getUintxVMFlag",     CC"(Ljava/lang/String;)Ljava/lang/Long;",
                                                      (void*)&WB_GetUintxVMFlag},
  {CC"getUint64VMFlag",    CC"(Ljava/lang/String;)Ljava/lang/Long;",
                                                      (void*)&WB_GetUint64VMFlag},
  {CC"getDoubleVMFlag",    CC"(Ljava/lang/String;)Ljava/lang/Double;",
                                                      (void*)&WB_GetDoubleVMFlag},
  {CC"getStringVMFlag",    CC"(Ljava/lang/String;)Ljava/lang/String;",
                                                      (void*)&WB_GetStringVMFlag},
  {CC"isInStringTable",    CC"(Ljava/lang/String;)Z", (void*)&WB_IsInStringTable  },
  {CC"fullGC",   CC"()V",                             (void*)&WB_FullGC },
  {CC"youngGC",  CC"()V",                             (void*)&WB_YoungGC },
  {CC"readReservedMemory", CC"()V",                   (void*)&WB_ReadReservedMemory },
  {CC"allocateCodeBlob",   CC"(II)J",                 (void*)&WB_AllocateCodeBlob   },
  {CC"freeCodeBlob",       CC"(J)V",                  (void*)&WB_FreeCodeBlob       },
  {CC"getCodeBlob",        CC"(J)[Ljava/lang/Object;",(void*)&WB_GetCodeBlob        },
  {CC"allocateMetaspace",
     CC"(Ljava/lang/ClassLoader;J)J",                 (void*)&WB_AllocateMetaspace },
  {CC"freeMetaspace",
     CC"(Ljava/lang/ClassLoader;JJ)V",                (void*)&WB_FreeMetaspace },
  {CC"incMetaspaceCapacityUntilGC", CC"(J)J",         (void*)&WB_IncMetaspaceCapacityUntilGC },
  {CC"metaspaceCapacityUntilGC", CC"()J",             (void*)&WB_MetaspaceCapacityUntilGC },
  {CC"getCPUFeatures",     CC"()Ljava/lang/String;",  (void*)&WB_GetCPUFeatures     },
  {CC"getNMethod",         CC"(Ljava/lang/reflect/Executable;Z)[Ljava/lang/Object;",
                                                      (void*)&WB_GetNMethod         },
  {CC"isMonitorInflated",  CC"(Ljava/lang/Object;)Z", (void*)&WB_IsMonitorInflated  },
  {CC"forceSafepoint",     CC"()V",                   (void*)&WB_ForceSafepoint     },
  {CC"checkLibSpecifiesNoexecstack", CC"(Ljava/lang/String;)Z",
                                                      (void*)&WB_CheckLibSpecifiesNoexecstack},
  {CC"isContainerized",           CC"()Z",            (void*)&WB_IsContainerized },
  {CC"validateCgroup",
      CC"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
                                                      (void*)&WB_ValidateCgroup },
  {CC"printOsInfo",               CC"()V",            (void*)&WB_PrintOsInfo },
  {CC"getClassListFromLogfile", CC"()[Ljava/lang/String;",
                                                      (void*)&WB_GetClassListFromLogfile },
  {CC"getMethodListFromLogfile", CC"()[Ljava/lang/String;",
                                                      (void*)&WB_GetMethodListFromLogfile },
  {CC"getCompiledMethodList", CC"()[Ljava/lang/String;",
                                                      (void*)&WB_GetCompiledMethodList },
  {CC"getClassChainSymbolList", CC"()[Ljava/lang/String;",
                                                      (void*)&WB_GetClassChainSymbolList },
  {CC"getClassChainStateList", CC"()[I",
                                                      (void*)&WB_GetClassChainStateList },
  {CC"testFixDanglingPointerInDeopt",
      CC"(Ljava/lang/String;)Z",                      (void*)&WB_TestFixDanglingPointerInDeopt},
  {CC"getClassInitOrderList", CC"()[Ljava/lang/String;",
                                                      (void*)&WB_GetClassInitOrderList },
  {CC"isInCurrentTLAB",    CC"(Ljava/lang/Object;)Z", (void*)&WB_IsInCurrentTLAB },
  {CC"gcLockCritical",            CC"()V",            (void*)&WB_GCLockCritical},
  {CC"gcUnlockCritical",          CC"()V",            (void*)&WB_GCUnlockCritical},
};

#undef CC

JVM_ENTRY(void, JVM_RegisterWhiteBoxMethods(JNIEnv* env, jclass wbclass))
  {
    if (WhiteBoxAPI) {
      // Make sure that wbclass is loaded by the null classloader
      instanceKlassHandle ikh = instanceKlassHandle(JNIHandles::resolve(wbclass)->klass());
      Handle loader(ikh->class_loader());
      if (loader.is_null()) {
        WhiteBox::register_methods(env, wbclass, thread, methods, sizeof(methods) / sizeof(methods[0]));
        WhiteBox::register_extended(env, wbclass, thread);
        WhiteBox::set_used();
      }
    }
  }
JVM_END
