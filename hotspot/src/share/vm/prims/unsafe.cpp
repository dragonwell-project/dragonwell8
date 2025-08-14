/*
 * Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/vmSymbols.hpp"
#include "utilities/macros.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#endif // INCLUDE_ALL_GCS
#include "jfr/jfrEvents.hpp"
#include "memory/allocation.inline.hpp"
#include "prims/jni.h"
#include "prims/jvm.h"
#include "runtime/globals.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/prefetch.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/reflection.hpp"
#include "runtime/synchronizer.hpp"
#include "services/threadService.hpp"
#include "utilities/copy.hpp"
#include "utilities/dtrace.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

/*
 *      Implementation of class sun.misc.Unsafe
 */

#ifndef USDT2
HS_DTRACE_PROBE_DECL3(hotspot, thread__park__begin, uintptr_t, int, long long);
HS_DTRACE_PROBE_DECL1(hotspot, thread__park__end, uintptr_t);
HS_DTRACE_PROBE_DECL1(hotspot, thread__unpark, uintptr_t);
#endif /* !USDT2 */

#define MAX_OBJECT_SIZE \
  ( arrayOopDesc::header_size(T_DOUBLE) * HeapWordSize \
    + ((julong)max_jint * sizeof(double)) )


#define UNSAFE_ENTRY(result_type, header) \
  JVM_ENTRY(result_type, header)

// Can't use UNSAFE_LEAF because it has the signature of a straight
// call into the runtime (just like JVM_LEAF, funny that) but it's
// called like a Java Native and thus the wrapper built for it passes
// arguments like a JNI call.  It expects those arguments to be popped
// from the stack on Intel like all good JNI args are, and adjusts the
// stack according.  Since the JVM_LEAF call expects no extra
// arguments the stack isn't popped in the C code, is pushed by the
// wrapper and we get sick.
//#define UNSAFE_LEAF(result_type, header) \
//  JVM_LEAF(result_type, header)

#define UNSAFE_END JVM_END

#define UnsafeWrapper(arg) /*nothing, for the present*/


inline void* addr_from_java(jlong addr) {
  // This assert fails in a variety of ways on 32-bit systems.
  // It is impossible to predict whether native code that converts
  // pointers to longs will sign-extend or zero-extend the addresses.
  //assert(addr == (uintptr_t)addr, "must not be odd high bits");
  return (void*)(uintptr_t)addr;
}

inline jlong addr_to_java(void* p) {
  assert(p == (void*)(uintptr_t)p, "must not be odd high bits");
  return (uintptr_t)p;
}


// Note: The VM's obj_field and related accessors use byte-scaled
// ("unscaled") offsets, just as the unsafe methods do.

// However, the method Unsafe.fieldOffset explicitly declines to
// guarantee this.  The field offset values manipulated by the Java user
// through the Unsafe API are opaque cookies that just happen to be byte
// offsets.  We represent this state of affairs by passing the cookies
// through conversion functions when going between the VM and the Unsafe API.
// The conversion functions just happen to be no-ops at present.

inline jlong field_offset_to_byte_offset(jlong field_offset) {
  return field_offset;
}

inline jlong field_offset_from_byte_offset(jlong byte_offset) {
  return byte_offset;
}

inline jint invocation_key_from_method_slot(jint slot) {
  return slot;
}

inline jint invocation_key_to_method_slot(jint key) {
  return key;
}

inline void* index_oop_from_field_offset_long(oop p, jlong field_offset) {
  jlong byte_offset = field_offset_to_byte_offset(field_offset);
#ifdef ASSERT
  if (p != NULL) {
    assert(byte_offset >= 0 && byte_offset <= (jlong)MAX_OBJECT_SIZE, "sane offset");
    if (byte_offset == (jint)byte_offset) {
      void* ptr_plus_disp = (address)p + byte_offset;
      assert((void*)p->obj_field_addr<oop>((jint)byte_offset) == ptr_plus_disp,
             "raw [ptr+disp] must be consistent with oop::field_base");
    }
    jlong p_size = HeapWordSize * (jlong)(p->size());
    assert(byte_offset < p_size, err_msg("Unsafe access: offset " INT64_FORMAT " > object's size " INT64_FORMAT, byte_offset, p_size));
  }
#endif
  if (sizeof(char*) == sizeof(jint))    // (this constant folds!)
    return (address)p + (jint) byte_offset;
  else
    return (address)p +        byte_offset;
}

// Externally callable versions:
// (Use these in compiler intrinsics which emulate unsafe primitives.)
jlong Unsafe_field_offset_to_byte_offset(jlong field_offset) {
  return field_offset;
}
jlong Unsafe_field_offset_from_byte_offset(jlong byte_offset) {
  return byte_offset;
}
jint Unsafe_invocation_key_from_method_slot(jint slot) {
  return invocation_key_from_method_slot(slot);
}
jint Unsafe_invocation_key_to_method_slot(jint key) {
  return invocation_key_to_method_slot(key);
}


///// Data in the Java heap.

#define truncate_jboolean(x) ((x) & 1)
#define truncate_jbyte(x) (x)
#define truncate_jshort(x) (x)
#define truncate_jchar(x) (x)
#define truncate_jint(x) (x)
#define truncate_jlong(x) (x)
#define truncate_jfloat(x) (x)
#define truncate_jdouble(x) (x)

// Note that GET_FIELD and SET_FIELD cast the object field's C type to
// volatile at the point of access. That is needed in order to ensure
// that the C compiler does not reorder the object field access wrt to
// preceding and succeeding volatile accesses to the thread flag field
// (made by UnsafeWrapper) which safeguard the field access. See
// JDK-8186787 for details.

#define GET_FIELD(obj, offset, type_name, v) \
  oop p = JNIHandles::resolve(obj); \
  type_name v = *(volatile type_name*)index_oop_from_field_offset_long(p, offset)

#define SET_FIELD(obj, offset, type_name, x) \
  oop p = JNIHandles::resolve(obj); \
  *(volatile type_name*)index_oop_from_field_offset_long(p, offset) = truncate_##type_name(x)

#define GET_FIELD_VOLATILE(obj, offset, type_name, v) \
  oop p = JNIHandles::resolve(obj); \
  if (support_IRIW_for_not_multiple_copy_atomic_cpu) { \
    OrderAccess::fence(); \
  } \
  volatile type_name v = OrderAccess::load_acquire((volatile type_name*)index_oop_from_field_offset_long(p, offset));

#define SET_FIELD_VOLATILE(obj, offset, type_name, x) \
  oop p = JNIHandles::resolve(obj); \
  OrderAccess::release_store_fence((volatile type_name*)index_oop_from_field_offset_long(p, offset), truncate_##type_name(x));

// Macros for oops that check UseCompressedOops

#define GET_OOP_FIELD(obj, offset, v) \
  oop p = JNIHandles::resolve(obj);   \
  oop v;                              \
  if (UseCompressedOops) {            \
    narrowOop n = *(narrowOop*)index_oop_from_field_offset_long(p, offset); \
    v = oopDesc::decode_heap_oop(n);                                \
  } else {                            \
    v = *(oop*)index_oop_from_field_offset_long(p, offset);                 \
  }


// Get/SetObject must be special-cased, since it works with handles.

// We could be accessing the referent field in a reference
// object. If G1 is enabled then we need to register non-null
// referent with the SATB barrier.

#if INCLUDE_ALL_GCS
static bool is_java_lang_ref_Reference_access(oop o, jlong offset) {
  if (offset == java_lang_ref_Reference::referent_offset && o != NULL) {
    Klass* k = o->klass();
    if (InstanceKlass::cast(k)->reference_type() != REF_NONE) {
      assert(InstanceKlass::cast(k)->is_subclass_of(SystemDictionary::Reference_klass()), "sanity");
      return true;
    }
  }
 return false;
}
#endif

static void ensure_satb_referent_alive(oop o, jlong offset, oop v) {
#if INCLUDE_ALL_GCS
  if (UseG1GC && v != NULL && is_java_lang_ref_Reference_access(o, offset)) {
    G1SATBCardTableModRefBS::enqueue(v);
  }
#endif
}

// The xxx140 variants for backward compatibility do not allow a full-width offset.
UNSAFE_ENTRY(jobject, Unsafe_GetObject140(JNIEnv *env, jobject unsafe, jobject obj, jint offset))
  UnsafeWrapper("Unsafe_GetObject");
  if (obj == NULL)  THROW_0(vmSymbols::java_lang_NullPointerException());
  GET_OOP_FIELD(obj, offset, v)

  ensure_satb_referent_alive(p, offset, v);

  return JNIHandles::make_local(env, v);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetObject140(JNIEnv *env, jobject unsafe, jobject obj, jint offset, jobject x_h))
  UnsafeWrapper("Unsafe_SetObject");
  if (obj == NULL)  THROW(vmSymbols::java_lang_NullPointerException());
  oop x = JNIHandles::resolve(x_h);
  //SET_FIELD(obj, offset, oop, x);
  oop p = JNIHandles::resolve(obj);
  if (UseCompressedOops) {
    if (x != NULL) {
      // If there is a heap base pointer, we are obliged to emit a store barrier.
      oop_store((narrowOop*)index_oop_from_field_offset_long(p, offset), x);
    } else {
      narrowOop n = oopDesc::encode_heap_oop_not_null(x);
      *(narrowOop*)index_oop_from_field_offset_long(p, offset) = n;
    }
  } else {
    if (x != NULL) {
      // If there is a heap base pointer, we are obliged to emit a store barrier.
      oop_store((oop*)index_oop_from_field_offset_long(p, offset), x);
    } else {
      *(oop*)index_oop_from_field_offset_long(p, offset) = x;
    }
  }
UNSAFE_END

// The normal variants allow a null base pointer with an arbitrary address.
// But if the base pointer is non-null, the offset should make some sense.
// That is, it should be in the range [0, MAX_OBJECT_SIZE].
UNSAFE_ENTRY(jobject, Unsafe_GetObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset))
  UnsafeWrapper("Unsafe_GetObject");
  GET_OOP_FIELD(obj, offset, v)

  ensure_satb_referent_alive(p, offset, v);

  return JNIHandles::make_local(env, v);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h))
  UnsafeWrapper("Unsafe_SetObject");
  oop x = JNIHandles::resolve(x_h);
  oop p = JNIHandles::resolve(obj);
  if (UseCompressedOops) {
    oop_store((narrowOop*)index_oop_from_field_offset_long(p, offset), x);
  } else {
    oop_store((oop*)index_oop_from_field_offset_long(p, offset), x);
  }
UNSAFE_END

UNSAFE_ENTRY(jobject, Unsafe_GetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset))
  UnsafeWrapper("Unsafe_GetObjectVolatile");
  oop p = JNIHandles::resolve(obj);
  void* addr = index_oop_from_field_offset_long(p, offset);
  volatile oop v;
  if (UseCompressedOops) {
    volatile narrowOop n = *(volatile narrowOop*) addr;
    (void)const_cast<oop&>(v = oopDesc::decode_heap_oop(n));
  } else {
    (void)const_cast<oop&>(v = *(volatile oop*) addr);
  }

  ensure_satb_referent_alive(p, offset, v);

  OrderAccess::acquire();
  return JNIHandles::make_local(env, v);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h))
  UnsafeWrapper("Unsafe_SetObjectVolatile");
  oop x = JNIHandles::resolve(x_h);
  oop p = JNIHandles::resolve(obj);
  void* addr = index_oop_from_field_offset_long(p, offset);
  OrderAccess::release();
  if (UseCompressedOops) {
    oop_store((narrowOop*)addr, x);
  } else {
    oop_store((oop*)addr, x);
  }
  OrderAccess::fence();
UNSAFE_END

#ifndef SUPPORTS_NATIVE_CX8

// VM_Version::supports_cx8() is a surrogate for 'supports atomic long memory ops'.
//
// On platforms which do not support atomic compare-and-swap of jlong (8 byte)
// values we have to use a lock-based scheme to enforce atomicity. This has to be
// applied to all Unsafe operations that set the value of a jlong field. Even so
// the compareAndSwapLong operation will not be atomic with respect to direct stores
// to the field from Java code. It is important therefore that any Java code that
// utilizes these Unsafe jlong operations does not perform direct stores. To permit
// direct loads of the field from Java code we must also use Atomic::store within the
// locked regions. And for good measure, in case there are direct stores, we also
// employ Atomic::load within those regions. Note that the field in question must be
// volatile and so must have atomic load/store accesses applied at the Java level.
//
// The locking scheme could utilize a range of strategies for controlling the locking
// granularity: from a lock per-field through to a single global lock. The latter is
// the simplest and is used for the current implementation. Note that the Java object
// that contains the field, can not, in general, be used for locking. To do so can lead
// to deadlocks as we may introduce locking into what appears to the Java code to be a
// lock-free path.
//
// As all the locked-regions are very short and themselves non-blocking we can treat
// them as leaf routines and elide safepoint checks (ie we don't perform any thread
// state transitions even when blocking for the lock). Note that if we do choose to
// add safepoint checks and thread state transitions, we must ensure that we calculate
// the address of the field _after_ we have acquired the lock, else the object may have
// been moved by the GC

UNSAFE_ENTRY(jlong, Unsafe_GetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset))
  UnsafeWrapper("Unsafe_GetLongVolatile");
  {
    if (VM_Version::supports_cx8()) {
      GET_FIELD_VOLATILE(obj, offset, jlong, v);
      return v;
    }
    else {
      Handle p (THREAD, JNIHandles::resolve(obj));
      jlong* addr = (jlong*)(index_oop_from_field_offset_long(p(), offset));
      MutexLockerEx mu(UnsafeJlong_lock, Mutex::_no_safepoint_check_flag);
      jlong value = Atomic::load(addr);
      return value;
    }
  }
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetLongVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong x))
  UnsafeWrapper("Unsafe_SetLongVolatile");
  {
    if (VM_Version::supports_cx8()) {
      SET_FIELD_VOLATILE(obj, offset, jlong, x);
    }
    else {
      Handle p (THREAD, JNIHandles::resolve(obj));
      jlong* addr = (jlong*)(index_oop_from_field_offset_long(p(), offset));
      MutexLockerEx mu(UnsafeJlong_lock, Mutex::_no_safepoint_check_flag);
      Atomic::store(x, addr);
    }
  }
UNSAFE_END

#endif // not SUPPORTS_NATIVE_CX8

#define DEFINE_GETSETOOP(jboolean, Boolean) \
 \
UNSAFE_ENTRY(jboolean, Unsafe_Get##Boolean##140(JNIEnv *env, jobject unsafe, jobject obj, jint offset)) \
  UnsafeWrapper("Unsafe_Get"#Boolean); \
  if (obj == NULL)  THROW_0(vmSymbols::java_lang_NullPointerException()); \
  GET_FIELD(obj, offset, jboolean, v); \
  return v; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_Set##Boolean##140(JNIEnv *env, jobject unsafe, jobject obj, jint offset, jboolean x)) \
  UnsafeWrapper("Unsafe_Set"#Boolean); \
  if (obj == NULL)  THROW(vmSymbols::java_lang_NullPointerException()); \
  SET_FIELD(obj, offset, jboolean, x); \
UNSAFE_END \
 \
UNSAFE_ENTRY(jboolean, Unsafe_Get##Boolean(JNIEnv *env, jobject unsafe, jobject obj, jlong offset)) \
  UnsafeWrapper("Unsafe_Get"#Boolean); \
  GET_FIELD(obj, offset, jboolean, v); \
  return v; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_Set##Boolean(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jboolean x)) \
  UnsafeWrapper("Unsafe_Set"#Boolean); \
  SET_FIELD(obj, offset, jboolean, x); \
UNSAFE_END \
 \
// END DEFINE_GETSETOOP.

DEFINE_GETSETOOP(jboolean, Boolean)
DEFINE_GETSETOOP(jbyte, Byte)
DEFINE_GETSETOOP(jshort, Short);
DEFINE_GETSETOOP(jchar, Char);
DEFINE_GETSETOOP(jint, Int);
DEFINE_GETSETOOP(jlong, Long);
DEFINE_GETSETOOP(jfloat, Float);
DEFINE_GETSETOOP(jdouble, Double);

#undef DEFINE_GETSETOOP

#define DEFINE_GETSETOOP_VOLATILE(jboolean, Boolean) \
 \
UNSAFE_ENTRY(jboolean, Unsafe_Get##Boolean##Volatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset)) \
  UnsafeWrapper("Unsafe_Get"#Boolean); \
  GET_FIELD_VOLATILE(obj, offset, jboolean, v); \
  return v; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_Set##Boolean##Volatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jboolean x)) \
  UnsafeWrapper("Unsafe_Set"#Boolean); \
  SET_FIELD_VOLATILE(obj, offset, jboolean, x); \
UNSAFE_END \
 \
// END DEFINE_GETSETOOP_VOLATILE.

DEFINE_GETSETOOP_VOLATILE(jboolean, Boolean)
DEFINE_GETSETOOP_VOLATILE(jbyte, Byte)
DEFINE_GETSETOOP_VOLATILE(jshort, Short);
DEFINE_GETSETOOP_VOLATILE(jchar, Char);
DEFINE_GETSETOOP_VOLATILE(jint, Int);
DEFINE_GETSETOOP_VOLATILE(jfloat, Float);
DEFINE_GETSETOOP_VOLATILE(jdouble, Double);

#ifdef SUPPORTS_NATIVE_CX8
DEFINE_GETSETOOP_VOLATILE(jlong, Long);
#endif

#undef DEFINE_GETSETOOP_VOLATILE

// The non-intrinsified versions of setOrdered just use setVolatile

UNSAFE_ENTRY(void, Unsafe_SetOrderedInt(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jint x))
  UnsafeWrapper("Unsafe_SetOrderedInt");
  SET_FIELD_VOLATILE(obj, offset, jint, x);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetOrderedObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h))
  UnsafeWrapper("Unsafe_SetOrderedObject");
  oop x = JNIHandles::resolve(x_h);
  oop p = JNIHandles::resolve(obj);
  void* addr = index_oop_from_field_offset_long(p, offset);
  OrderAccess::release();
  if (UseCompressedOops) {
    oop_store((narrowOop*)addr, x);
  } else {
    oop_store((oop*)addr, x);
  }
  OrderAccess::fence();
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetOrderedLong(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong x))
  UnsafeWrapper("Unsafe_SetOrderedLong");
#ifdef SUPPORTS_NATIVE_CX8
  SET_FIELD_VOLATILE(obj, offset, jlong, x);
#else
  // Keep old code for platforms which may not have atomic long (8 bytes) instructions
  {
    if (VM_Version::supports_cx8()) {
      SET_FIELD_VOLATILE(obj, offset, jlong, x);
    }
    else {
      Handle p (THREAD, JNIHandles::resolve(obj));
      jlong* addr = (jlong*)(index_oop_from_field_offset_long(p(), offset));
      MutexLockerEx mu(UnsafeJlong_lock, Mutex::_no_safepoint_check_flag);
      Atomic::store(x, addr);
    }
  }
#endif
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_LoadFence(JNIEnv *env, jobject unsafe))
  UnsafeWrapper("Unsafe_LoadFence");
  OrderAccess::acquire();
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_StoreFence(JNIEnv *env, jobject unsafe))
  UnsafeWrapper("Unsafe_StoreFence");
  OrderAccess::release();
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_FullFence(JNIEnv *env, jobject unsafe))
  UnsafeWrapper("Unsafe_FullFence");
  OrderAccess::fence();
UNSAFE_END

////// Data in the C heap.

// Note:  These do not throw NullPointerException for bad pointers.
// They just crash.  Only a oop base pointer can generate a NullPointerException.
//
#define DEFINE_GETSETNATIVE(java_type, Type, native_type) \
 \
UNSAFE_ENTRY(java_type, Unsafe_GetNative##Type(JNIEnv *env, jobject unsafe, jlong addr)) \
  UnsafeWrapper("Unsafe_GetNative"#Type); \
  void* p = addr_from_java(addr); \
  JavaThread* t = JavaThread::current(); \
  t->set_doing_unsafe_access(true); \
  java_type x = *(volatile native_type*)p; \
  t->set_doing_unsafe_access(false); \
  return x; \
UNSAFE_END \
 \
UNSAFE_ENTRY(void, Unsafe_SetNative##Type(JNIEnv *env, jobject unsafe, jlong addr, java_type x)) \
  UnsafeWrapper("Unsafe_SetNative"#Type); \
  JavaThread* t = JavaThread::current(); \
  t->set_doing_unsafe_access(true); \
  void* p = addr_from_java(addr); \
  *(volatile native_type*)p = x; \
  t->set_doing_unsafe_access(false); \
UNSAFE_END \
 \
// END DEFINE_GETSETNATIVE.

DEFINE_GETSETNATIVE(jbyte, Byte, signed char)
DEFINE_GETSETNATIVE(jshort, Short, signed short);
DEFINE_GETSETNATIVE(jchar, Char, unsigned short);
DEFINE_GETSETNATIVE(jint, Int, jint);
// no long -- handled specially
DEFINE_GETSETNATIVE(jfloat, Float, float);
DEFINE_GETSETNATIVE(jdouble, Double, double);

#undef DEFINE_GETSETNATIVE

UNSAFE_ENTRY(jlong, Unsafe_GetNativeLong(JNIEnv *env, jobject unsafe, jlong addr))
  UnsafeWrapper("Unsafe_GetNativeLong");
  JavaThread* t = JavaThread::current();
  // We do it this way to avoid problems with access to heap using 64
  // bit loads, as jlong in heap could be not 64-bit aligned, and on
  // some CPUs (SPARC) it leads to SIGBUS.
  t->set_doing_unsafe_access(true);
  void* p = addr_from_java(addr);
  jlong x;
  if (((intptr_t)p & 7) == 0) {
    // jlong is aligned, do a volatile access
    x = *(volatile jlong*)p;
  } else {
    jlong_accessor acc;
    acc.words[0] = ((volatile jint*)p)[0];
    acc.words[1] = ((volatile jint*)p)[1];
    x = acc.long_value;
  }
  t->set_doing_unsafe_access(false);
  return x;
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetNativeLong(JNIEnv *env, jobject unsafe, jlong addr, jlong x))
  UnsafeWrapper("Unsafe_SetNativeLong");
  JavaThread* t = JavaThread::current();
  // see comment for Unsafe_GetNativeLong
  t->set_doing_unsafe_access(true);
  void* p = addr_from_java(addr);
  if (((intptr_t)p & 7) == 0) {
    // jlong is aligned, do a volatile access
    *(volatile jlong*)p = x;
  } else {
    jlong_accessor acc;
    acc.long_value = x;
    ((volatile jint*)p)[0] = acc.words[0];
    ((volatile jint*)p)[1] = acc.words[1];
  }
  t->set_doing_unsafe_access(false);
UNSAFE_END


UNSAFE_ENTRY(jlong, Unsafe_GetNativeAddress(JNIEnv *env, jobject unsafe, jlong addr))
  UnsafeWrapper("Unsafe_GetNativeAddress");
  void* p = addr_from_java(addr);
  return addr_to_java(*(void**)p);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetNativeAddress(JNIEnv *env, jobject unsafe, jlong addr, jlong x))
  UnsafeWrapper("Unsafe_SetNativeAddress");
  void* p = addr_from_java(addr);
  *(void**)p = addr_from_java(x);
UNSAFE_END


////// Allocation requests

UNSAFE_ENTRY(jobject, Unsafe_AllocateInstance(JNIEnv *env, jobject unsafe, jclass cls))
  UnsafeWrapper("Unsafe_AllocateInstance");
  {
    ThreadToNativeFromVM ttnfv(thread);
    return env->AllocObject(cls);
  }
UNSAFE_END

UNSAFE_ENTRY(jlong, Unsafe_AllocateMemory(JNIEnv *env, jobject unsafe, jlong size))
  UnsafeWrapper("Unsafe_AllocateMemory");
  size_t sz = (size_t)size;
  if (sz != (julong)size || size < 0) {
    THROW_0(vmSymbols::java_lang_IllegalArgumentException());
  }
  if (sz == 0) {
    return 0;
  }
  sz = round_to(sz, HeapWordSize);
  void* x = os::malloc(sz, mtInternal);
  if (x == NULL) {
    THROW_0(vmSymbols::java_lang_OutOfMemoryError());
  }
  //Copy::fill_to_words((HeapWord*)x, sz / HeapWordSize);
  return addr_to_java(x);
UNSAFE_END

UNSAFE_ENTRY(jlong, Unsafe_ReallocateMemory(JNIEnv *env, jobject unsafe, jlong addr, jlong size))
  UnsafeWrapper("Unsafe_ReallocateMemory");
  void* p = addr_from_java(addr);
  size_t sz = (size_t)size;
  if (sz != (julong)size || size < 0) {
    THROW_0(vmSymbols::java_lang_IllegalArgumentException());
  }
  if (sz == 0) {
    os::free(p);
    return 0;
  }
  sz = round_to(sz, HeapWordSize);
  void* x = (p == NULL) ? os::malloc(sz, mtInternal) : os::realloc(p, sz, mtInternal);
  if (x == NULL) {
    THROW_0(vmSymbols::java_lang_OutOfMemoryError());
  }
  return addr_to_java(x);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_FreeMemory(JNIEnv *env, jobject unsafe, jlong addr))
  UnsafeWrapper("Unsafe_FreeMemory");
  void* p = addr_from_java(addr);
  if (p == NULL) {
    return;
  }
  os::free(p);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetMemory(JNIEnv *env, jobject unsafe, jlong addr, jlong size, jbyte value))
  UnsafeWrapper("Unsafe_SetMemory");
  size_t sz = (size_t)size;
  if (sz != (julong)size || size < 0) {
    THROW(vmSymbols::java_lang_IllegalArgumentException());
  }
  char* p = (char*) addr_from_java(addr);
  Copy::fill_to_memory_atomic(p, sz, value);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_SetMemory2(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong size, jbyte value))
  UnsafeWrapper("Unsafe_SetMemory");
  size_t sz = (size_t)size;
  if (sz != (julong)size || size < 0) {
    THROW(vmSymbols::java_lang_IllegalArgumentException());
  }
  oop base = JNIHandles::resolve(obj);
  void* p = index_oop_from_field_offset_long(base, offset);
  Copy::fill_to_memory_atomic(p, sz, value);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_CopyMemory(JNIEnv *env, jobject unsafe, jlong srcAddr, jlong dstAddr, jlong size))
  UnsafeWrapper("Unsafe_CopyMemory");
  if (size == 0) {
    return;
  }
  size_t sz = (size_t)size;
  if (sz != (julong)size || size < 0) {
    THROW(vmSymbols::java_lang_IllegalArgumentException());
  }
  void* src = addr_from_java(srcAddr);
  void* dst = addr_from_java(dstAddr);
  Copy::conjoint_memory_atomic(src, dst, sz);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_CopyMemory2(JNIEnv *env, jobject unsafe, jobject srcObj, jlong srcOffset, jobject dstObj, jlong dstOffset, jlong size))
  UnsafeWrapper("Unsafe_CopyMemory");
  if (size == 0) {
    return;
  }
  size_t sz = (size_t)size;
  if (sz != (julong)size || size < 0) {
    THROW(vmSymbols::java_lang_IllegalArgumentException());
  }
  oop srcp = JNIHandles::resolve(srcObj);
  oop dstp = JNIHandles::resolve(dstObj);
  if (dstp != NULL && !dstp->is_typeArray()) {
    // NYI:  This works only for non-oop arrays at present.
    // Generalizing it would be reasonable, but requires card marking.
    // Also, autoboxing a Long from 0L in copyMemory(x,y, 0L,z, n) would be bad.
    THROW(vmSymbols::java_lang_IllegalArgumentException());
  }
  void* src = index_oop_from_field_offset_long(srcp, srcOffset);
  void* dst = index_oop_from_field_offset_long(dstp, dstOffset);
  Copy::conjoint_memory_atomic(src, dst, sz);
UNSAFE_END


////// Random queries

// See comment at file start about UNSAFE_LEAF
//UNSAFE_LEAF(jint, Unsafe_AddressSize())
UNSAFE_ENTRY(jint, Unsafe_AddressSize(JNIEnv *env, jobject unsafe))
  UnsafeWrapper("Unsafe_AddressSize");
  return sizeof(void*);
UNSAFE_END

// See comment at file start about UNSAFE_LEAF
//UNSAFE_LEAF(jint, Unsafe_PageSize())
UNSAFE_ENTRY(jint, Unsafe_PageSize(JNIEnv *env, jobject unsafe))
  UnsafeWrapper("Unsafe_PageSize");
  return os::vm_page_size();
UNSAFE_END

jint find_field_offset(jobject field, int must_be_static, TRAPS) {
  if (field == NULL) {
    THROW_0(vmSymbols::java_lang_NullPointerException());
  }

  oop reflected   = JNIHandles::resolve_non_null(field);
  oop mirror      = java_lang_reflect_Field::clazz(reflected);
  Klass* k      = java_lang_Class::as_Klass(mirror);
  int slot        = java_lang_reflect_Field::slot(reflected);
  int modifiers   = java_lang_reflect_Field::modifiers(reflected);

  if (must_be_static >= 0) {
    int really_is_static = ((modifiers & JVM_ACC_STATIC) != 0);
    if (must_be_static != really_is_static) {
      THROW_0(vmSymbols::java_lang_IllegalArgumentException());
    }
  }

  int offset = InstanceKlass::cast(k)->field_offset(slot);
  return field_offset_from_byte_offset(offset);
}

UNSAFE_ENTRY(jlong, Unsafe_ObjectFieldOffset(JNIEnv *env, jobject unsafe, jobject field))
  UnsafeWrapper("Unsafe_ObjectFieldOffset");
  return find_field_offset(field, 0, THREAD);
UNSAFE_END

UNSAFE_ENTRY(jlong, Unsafe_StaticFieldOffset(JNIEnv *env, jobject unsafe, jobject field))
  UnsafeWrapper("Unsafe_StaticFieldOffset");
  return find_field_offset(field, 1, THREAD);
UNSAFE_END

UNSAFE_ENTRY(jobject, Unsafe_StaticFieldBaseFromField(JNIEnv *env, jobject unsafe, jobject field))
  UnsafeWrapper("Unsafe_StaticFieldBase");
  // Note:  In this VM implementation, a field address is always a short
  // offset from the base of a a klass metaobject.  Thus, the full dynamic
  // range of the return type is never used.  However, some implementations
  // might put the static field inside an array shared by many classes,
  // or even at a fixed address, in which case the address could be quite
  // large.  In that last case, this function would return NULL, since
  // the address would operate alone, without any base pointer.

  if (field == NULL)  THROW_0(vmSymbols::java_lang_NullPointerException());

  oop reflected   = JNIHandles::resolve_non_null(field);
  oop mirror      = java_lang_reflect_Field::clazz(reflected);
  int modifiers   = java_lang_reflect_Field::modifiers(reflected);

  if ((modifiers & JVM_ACC_STATIC) == 0) {
    THROW_0(vmSymbols::java_lang_IllegalArgumentException());
  }

  return JNIHandles::make_local(env, mirror);
UNSAFE_END

//@deprecated
UNSAFE_ENTRY(jint, Unsafe_FieldOffset(JNIEnv *env, jobject unsafe, jobject field))
  UnsafeWrapper("Unsafe_FieldOffset");
  // tries (but fails) to be polymorphic between static and non-static:
  jlong offset = find_field_offset(field, -1, THREAD);
  guarantee(offset == (jint)offset, "offset fits in 32 bits");
  return (jint)offset;
UNSAFE_END

//@deprecated
UNSAFE_ENTRY(jobject, Unsafe_StaticFieldBaseFromClass(JNIEnv *env, jobject unsafe, jobject clazz))
  UnsafeWrapper("Unsafe_StaticFieldBase");
  if (clazz == NULL) {
    THROW_0(vmSymbols::java_lang_NullPointerException());
  }
  return JNIHandles::make_local(env, JNIHandles::resolve_non_null(clazz));
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_EnsureClassInitialized(JNIEnv *env, jobject unsafe, jobject clazz)) {
  UnsafeWrapper("Unsafe_EnsureClassInitialized");
  if (clazz == NULL) {
    THROW(vmSymbols::java_lang_NullPointerException());
  }
  oop mirror = JNIHandles::resolve_non_null(clazz);

  Klass* klass = java_lang_Class::as_Klass(mirror);
  if (klass != NULL && klass->should_be_initialized()) {
    InstanceKlass* k = InstanceKlass::cast(klass);
    k->initialize(CHECK);
  }
}
UNSAFE_END

UNSAFE_ENTRY(jboolean, Unsafe_ShouldBeInitialized(JNIEnv *env, jobject unsafe, jobject clazz)) {
  UnsafeWrapper("Unsafe_ShouldBeInitialized");
  if (clazz == NULL) {
    THROW_(vmSymbols::java_lang_NullPointerException(), false);
  }
  oop mirror = JNIHandles::resolve_non_null(clazz);
  Klass* klass = java_lang_Class::as_Klass(mirror);
  if (klass != NULL && klass->should_be_initialized()) {
    return true;
  }
  return false;
}
UNSAFE_END

static void getBaseAndScale(int& base, int& scale, jclass acls, TRAPS) {
  if (acls == NULL) {
    THROW(vmSymbols::java_lang_NullPointerException());
  }
  oop      mirror = JNIHandles::resolve_non_null(acls);
  Klass* k      = java_lang_Class::as_Klass(mirror);
  if (k == NULL || !k->oop_is_array()) {
    THROW(vmSymbols::java_lang_InvalidClassException());
  } else if (k->oop_is_objArray()) {
    base  = arrayOopDesc::base_offset_in_bytes(T_OBJECT);
    scale = heapOopSize;
  } else if (k->oop_is_typeArray()) {
    TypeArrayKlass* tak = TypeArrayKlass::cast(k);
    base  = tak->array_header_in_bytes();
    assert(base == arrayOopDesc::base_offset_in_bytes(tak->element_type()), "array_header_size semantics ok");
    scale = (1 << tak->log2_element_size());
  } else {
    ShouldNotReachHere();
  }
}

UNSAFE_ENTRY(jint, Unsafe_ArrayBaseOffset(JNIEnv *env, jobject unsafe, jclass acls))
  UnsafeWrapper("Unsafe_ArrayBaseOffset");
  int base = 0, scale = 0;
  getBaseAndScale(base, scale, acls, CHECK_0);
  return field_offset_from_byte_offset(base);
UNSAFE_END


UNSAFE_ENTRY(jint, Unsafe_ArrayIndexScale(JNIEnv *env, jobject unsafe, jclass acls))
  UnsafeWrapper("Unsafe_ArrayIndexScale");
  int base = 0, scale = 0;
  getBaseAndScale(base, scale, acls, CHECK_0);
  // This VM packs both fields and array elements down to the byte.
  // But watch out:  If this changes, so that array references for
  // a given primitive type (say, T_BOOLEAN) use different memory units
  // than fields, this method MUST return zero for such arrays.
  // For example, the VM used to store sub-word sized fields in full
  // words in the object layout, so that accessors like getByte(Object,int)
  // did not really do what one might expect for arrays.  Therefore,
  // this function used to report a zero scale factor, so that the user
  // would know not to attempt to access sub-word array elements.
  // // Code for unpacked fields:
  // if (scale < wordSize)  return 0;

  // The following allows for a pretty general fieldOffset cookie scheme,
  // but requires it to be linear in byte offset.
  return field_offset_from_byte_offset(scale) - field_offset_from_byte_offset(0);
UNSAFE_END


static inline void throw_new(JNIEnv *env, const char *ename) {
  char buf[100];
  strcpy(buf, "java/lang/");
  strcat(buf, ename);
  jclass cls = env->FindClass(buf);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    tty->print_cr("Unsafe: cannot throw %s because FindClass has failed", buf);
    return;
  }
  char* msg = NULL;
  env->ThrowNew(cls, msg);
}

static jclass Unsafe_DefineClass_impl(JNIEnv *env, jstring name, jbyteArray data, int offset, int length, jobject loader, jobject pd) {
  {
    // Code lifted from JDK 1.3 ClassLoader.c

    jbyte *body;
    char *utfName;
    jclass result = 0;
    char buf[128];

    if (UsePerfData) {
      ClassLoader::unsafe_defineClassCallCounter()->inc();
    }

    if (data == NULL) {
        throw_new(env, "NullPointerException");
        return 0;
    }

    /* Work around 4153825. malloc crashes on Solaris when passed a
     * negative size.
     */
    if (length < 0) {
        throw_new(env, "ArrayIndexOutOfBoundsException");
        return 0;
    }

    body = NEW_C_HEAP_ARRAY(jbyte, length, mtInternal);

    if (body == 0) {
        throw_new(env, "OutOfMemoryError");
        return 0;
    }

    env->GetByteArrayRegion(data, offset, length, body);

    if (env->ExceptionOccurred())
        goto free_body;

    if (name != NULL) {
        uint len = env->GetStringUTFLength(name);
        int unicode_len = env->GetStringLength(name);
        if (len >= sizeof(buf)) {
            utfName = NEW_C_HEAP_ARRAY(char, len + 1, mtInternal);
            if (utfName == NULL) {
                throw_new(env, "OutOfMemoryError");
                goto free_body;
            }
        } else {
            utfName = buf;
        }
        env->GetStringUTFRegion(name, 0, unicode_len, utfName);
        //VerifyFixClassname(utfName);
        for (uint i = 0; i < len; i++) {
          if (utfName[i] == '.')   utfName[i] = '/';
        }
    } else {
        utfName = NULL;
    }

    result = JVM_DefineClass(env, utfName, loader, body, length, pd);

    if (utfName && utfName != buf)
        FREE_C_HEAP_ARRAY(char, utfName, mtInternal);

 free_body:
    FREE_C_HEAP_ARRAY(jbyte, body, mtInternal);
    return result;
  }
}


UNSAFE_ENTRY(jclass, Unsafe_DefineClass(JNIEnv *env, jobject unsafe, jstring name, jbyteArray data, int offset, int length, jobject loader, jobject pd))
  UnsafeWrapper("Unsafe_DefineClass");
  {
    ThreadToNativeFromVM ttnfv(thread);
    return Unsafe_DefineClass_impl(env, name, data, offset, length, loader, pd);
  }
UNSAFE_END


UNSAFE_ENTRY(jclass, Unsafe_DefineClass0(JNIEnv *env, jobject unsafe, jstring name, jbyteArray data, int offset, int length))
  UnsafeWrapper("Unsafe_DefineClass");
  {
    ThreadToNativeFromVM ttnfv(thread);

    int depthFromDefineClass0 = 1;
    jclass  caller = JVM_GetCallerClass(env, depthFromDefineClass0);
    jobject loader = (caller == NULL) ? NULL : JVM_GetClassLoader(env, caller);
    jobject pd     = (caller == NULL) ? NULL : JVM_GetProtectionDomain(env, caller);

    return Unsafe_DefineClass_impl(env, name, data, offset, length, loader, pd);
  }
UNSAFE_END


#define DAC_Args CLS"[B["OBJ
// define a class but do not make it known to the class loader or system dictionary
// - host_class:  supplies context for linkage, access control, protection domain, and class loader
// - data:  bytes of a class file, a raw memory address (length gives the number of bytes)
// - cp_patches:  where non-null entries exist, they replace corresponding CP entries in data

// When you load an anonymous class U, it works as if you changed its name just before loading,
// to a name that you will never use again.  Since the name is lost, no other class can directly
// link to any member of U.  Just after U is loaded, the only way to use it is reflectively,
// through java.lang.Class methods like Class.newInstance.

// Access checks for linkage sites within U continue to follow the same rules as for named classes.
// The package of an anonymous class is given by the package qualifier on the name under which it was loaded.
// An anonymous class also has special privileges to access any member of its host class.
// This is the main reason why this loading operation is unsafe.  The purpose of this is to
// allow language implementations to simulate "open classes"; a host class in effect gets
// new code when an anonymous class is loaded alongside it.  A less convenient but more
// standard way to do this is with reflection, which can also be set to ignore access
// restrictions.

// Access into an anonymous class is possible only through reflection.  Therefore, there
// are no special access rules for calling into an anonymous class.  The relaxed access
// rule for the host class is applied in the opposite direction:  A host class reflectively
// access one of its anonymous classes.

// If you load the same bytecodes twice, you get two different classes.  You can reload
// the same bytecodes with or without varying CP patches.

// By using the CP patching array, you can have a new anonymous class U2 refer to an older one U1.
// The bytecodes for U2 should refer to U1 by a symbolic name (doesn't matter what the name is).
// The CONSTANT_Class entry for that name can be patched to refer directly to U1.

// This allows, for example, U2 to use U1 as a superclass or super-interface, or as
// an outer class (so that U2 is an anonymous inner class of anonymous U1).
// It is not possible for a named class, or an older anonymous class, to refer by
// name (via its CP) to a newer anonymous class.

// CP patching may also be used to modify (i.e., hack) the names of methods, classes,
// or type descriptors used in the loaded anonymous class.

// Finally, CP patching may be used to introduce "live" objects into the constant pool,
// instead of "dead" strings.  A compiled statement like println((Object)"hello") can
// be changed to println(greeting), where greeting is an arbitrary object created before
// the anonymous class is loaded.  This is useful in dynamic languages, in which
// various kinds of metaobjects must be introduced as constants into bytecode.
// Note the cast (Object), which tells the verifier to expect an arbitrary object,
// not just a literal string.  For such ldc instructions, the verifier uses the
// type Object instead of String, if the loaded constant is not in fact a String.

static instanceKlassHandle
Unsafe_DefineAnonymousClass_impl(JNIEnv *env,
                                 jclass host_class, jbyteArray data, jobjectArray cp_patches_jh,
                                 HeapWord* *temp_alloc,
                                 TRAPS) {

  if (UsePerfData) {
    ClassLoader::unsafe_defineClassCallCounter()->inc();
  }

  if (data == NULL) {
    THROW_0(vmSymbols::java_lang_NullPointerException());
  }

  jint length = typeArrayOop(JNIHandles::resolve_non_null(data))->length();
  jint word_length = (length + sizeof(HeapWord)-1) / sizeof(HeapWord);
  HeapWord* body = NEW_C_HEAP_ARRAY(HeapWord, word_length, mtInternal);
  if (body == NULL) {
    THROW_0(vmSymbols::java_lang_OutOfMemoryError());
  }

  // caller responsible to free it:
  (*temp_alloc) = body;

  {
    jbyte* array_base = typeArrayOop(JNIHandles::resolve_non_null(data))->byte_at_addr(0);
    Copy::conjoint_words((HeapWord*) array_base, body, word_length);
  }

  u1* class_bytes = (u1*) body;
  int class_bytes_length = (int) length;
  if (class_bytes_length < 0)  class_bytes_length = 0;
  if (class_bytes == NULL
      || host_class == NULL
      || length != class_bytes_length)
    THROW_0(vmSymbols::java_lang_IllegalArgumentException());

  objArrayHandle cp_patches_h;
  if (cp_patches_jh != NULL) {
    oop p = JNIHandles::resolve_non_null(cp_patches_jh);
    if (!p->is_objArray())
      THROW_0(vmSymbols::java_lang_IllegalArgumentException());
    cp_patches_h = objArrayHandle(THREAD, (objArrayOop)p);
  }

  KlassHandle host_klass(THREAD, java_lang_Class::as_Klass(JNIHandles::resolve_non_null(host_class)));
  const char* host_source = host_klass->external_name();
  Handle      host_loader(THREAD, host_klass->class_loader());
  Handle      host_domain(THREAD, host_klass->protection_domain());

  GrowableArray<Handle>* cp_patches = NULL;
  if (cp_patches_h.not_null()) {
    int alen = cp_patches_h->length();
    for (int i = alen-1; i >= 0; i--) {
      oop p = cp_patches_h->obj_at(i);
      if (p != NULL) {
        Handle patch(THREAD, p);
        if (cp_patches == NULL)
          cp_patches = new GrowableArray<Handle>(i+1, i+1, Handle());
        cp_patches->at_put(i, patch);
      }
    }
  }

  ClassFileStream st(class_bytes, class_bytes_length, (char*) host_source);

  instanceKlassHandle anon_klass;
  {
    Symbol* no_class_name = NULL;
    Klass* anonk = SystemDictionary::parse_stream(no_class_name,
                                                    host_loader, host_domain,
                                                    &st, host_klass, cp_patches,
                                                    CHECK_NULL);
    if (anonk == NULL)  return NULL;
    anon_klass = instanceKlassHandle(THREAD, anonk);
  }

  return anon_klass;
}

UNSAFE_ENTRY(jclass, Unsafe_DefineAnonymousClass(JNIEnv *env, jobject unsafe, jclass host_class, jbyteArray data, jobjectArray cp_patches_jh))
{
  instanceKlassHandle anon_klass;
  jobject res_jh = NULL;

  UnsafeWrapper("Unsafe_DefineAnonymousClass");
  ResourceMark rm(THREAD);

  HeapWord* temp_alloc = NULL;

  anon_klass = Unsafe_DefineAnonymousClass_impl(env, host_class, data,
                                                cp_patches_jh,
                                                   &temp_alloc, THREAD);
  if (anon_klass() != NULL)
    res_jh = JNIHandles::make_local(env, anon_klass->java_mirror());

  // try/finally clause:
  if (temp_alloc != NULL) {
    FREE_C_HEAP_ARRAY(HeapWord, temp_alloc, mtInternal);
  }

  // The anonymous class loader data has been artificially been kept alive to
  // this point.   The mirror and any instances of this class have to keep
  // it alive afterwards.
  if (anon_klass() != NULL) {
    anon_klass->class_loader_data()->set_keep_alive(false);
  }

  // let caller initialize it as needed...

  return (jclass) res_jh;
}
UNSAFE_END



UNSAFE_ENTRY(void, Unsafe_MonitorEnter(JNIEnv *env, jobject unsafe, jobject jobj))
  UnsafeWrapper("Unsafe_MonitorEnter");
  {
    if (jobj == NULL) {
      THROW(vmSymbols::java_lang_NullPointerException());
    }
    Handle obj(thread, JNIHandles::resolve_non_null(jobj));
    ObjectSynchronizer::jni_enter(obj, CHECK);
  }
UNSAFE_END


UNSAFE_ENTRY(jboolean, Unsafe_TryMonitorEnter(JNIEnv *env, jobject unsafe, jobject jobj))
  UnsafeWrapper("Unsafe_TryMonitorEnter");
  {
    if (jobj == NULL) {
      THROW_(vmSymbols::java_lang_NullPointerException(), JNI_FALSE);
    }
    Handle obj(thread, JNIHandles::resolve_non_null(jobj));
    bool res = ObjectSynchronizer::jni_try_enter(obj, CHECK_0);
    return (res ? JNI_TRUE : JNI_FALSE);
  }
UNSAFE_END


UNSAFE_ENTRY(void, Unsafe_MonitorExit(JNIEnv *env, jobject unsafe, jobject jobj))
  UnsafeWrapper("Unsafe_MonitorExit");
  {
    if (jobj == NULL) {
      THROW(vmSymbols::java_lang_NullPointerException());
    }
    Handle obj(THREAD, JNIHandles::resolve_non_null(jobj));
    ObjectSynchronizer::jni_exit(obj(), CHECK);
  }
UNSAFE_END


UNSAFE_ENTRY(void, Unsafe_ThrowException(JNIEnv *env, jobject unsafe, jthrowable thr))
  UnsafeWrapper("Unsafe_ThrowException");
  {
    ThreadToNativeFromVM ttnfv(thread);
    env->Throw(thr);
  }
UNSAFE_END

// JSR166 ------------------------------------------------------------------

UNSAFE_ENTRY(jboolean, Unsafe_CompareAndSwapObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject e_h, jobject x_h))
  UnsafeWrapper("Unsafe_CompareAndSwapObject");
  oop x = JNIHandles::resolve(x_h);
  oop e = JNIHandles::resolve(e_h);
  oop p = JNIHandles::resolve(obj);
  HeapWord* addr = (HeapWord *)index_oop_from_field_offset_long(p, offset);
  oop res = oopDesc::atomic_compare_exchange_oop(x, addr, e, true);
  jboolean success  = (res == e);
  if (success)
    update_barrier_set((void*)addr, x);
  return success;
UNSAFE_END

UNSAFE_ENTRY(jboolean, Unsafe_CompareAndSwapInt(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jint e, jint x))
  UnsafeWrapper("Unsafe_CompareAndSwapInt");
  oop p = JNIHandles::resolve(obj);
  jint* addr = (jint *) index_oop_from_field_offset_long(p, offset);
  return (jint)(Atomic::cmpxchg(x, addr, e)) == e;
UNSAFE_END

UNSAFE_ENTRY(jboolean, Unsafe_CompareAndSwapLong(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jlong e, jlong x))
  UnsafeWrapper("Unsafe_CompareAndSwapLong");
  Handle p (THREAD, JNIHandles::resolve(obj));
  jlong* addr = (jlong*)(index_oop_from_field_offset_long(p(), offset));
#ifdef SUPPORTS_NATIVE_CX8
  return (jlong)(Atomic::cmpxchg(x, addr, e)) == e;
#else
  if (VM_Version::supports_cx8())
    return (jlong)(Atomic::cmpxchg(x, addr, e)) == e;
  else {
    jboolean success = false;
    MutexLockerEx mu(UnsafeJlong_lock, Mutex::_no_safepoint_check_flag);
    jlong val = Atomic::load(addr);
    if (val == e) { Atomic::store(x, addr); success = true; }
    return success;
  }
#endif
UNSAFE_END

static void post_thread_park_event(EventThreadPark* event, const oop obj, jlong timeout_nanos, jlong until_epoch_millis) {
  assert(event != NULL, "invariant");
  assert(event->should_commit(), "invariant");
  event->set_parkedClass((obj != NULL) ? obj->klass() : NULL);
  event->set_timeout(timeout_nanos);
  event->set_until(until_epoch_millis);
  event->set_address((obj != NULL) ? (u8)cast_from_oop<uintptr_t>(obj) : 0);
  event->commit();
}

UNSAFE_ENTRY(void, Unsafe_Park(JNIEnv *env, jobject unsafe, jboolean isAbsolute, jlong time))
  UnsafeWrapper("Unsafe_Park");
  EventThreadPark event;
#ifndef USDT2
  HS_DTRACE_PROBE3(hotspot, thread__park__begin, thread->parker(), (int) isAbsolute, time);
#else /* USDT2 */
   HOTSPOT_THREAD_PARK_BEGIN(
                             (uintptr_t) thread->parker(), (int) isAbsolute, time);
#endif /* USDT2 */
  JavaThreadParkedState jtps(thread, time != 0);
  thread->parker()->park(isAbsolute != 0, time);
#ifndef USDT2
  HS_DTRACE_PROBE1(hotspot, thread__park__end, thread->parker());
#else /* USDT2 */
  HOTSPOT_THREAD_PARK_END(
                          (uintptr_t) thread->parker());
#endif /* USDT2 */
  if (event.should_commit()) {
    const oop obj = thread->current_park_blocker();
    if (time == 0) {
      post_thread_park_event(&event, obj, min_jlong, min_jlong);
    } else {
      if (isAbsolute != 0) {
        post_thread_park_event(&event, obj, min_jlong, time);
      } else {
        post_thread_park_event(&event, obj, time, min_jlong);
      }
    }
  }
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_Unpark(JNIEnv *env, jobject unsafe, jobject jthread))
  UnsafeWrapper("Unsafe_Unpark");
  Parker* p = NULL;
  if (jthread != NULL) {
    oop java_thread = JNIHandles::resolve_non_null(jthread);
    if (java_thread != NULL) {
      jlong lp = java_lang_Thread::park_event(java_thread);
      if (lp != 0) {
        // This cast is OK even though the jlong might have been read
        // non-atomically on 32bit systems, since there, one word will
        // always be zero anyway and the value set is always the same
        p = (Parker*)addr_from_java(lp);
      } else {
        // Grab lock if apparently null or using older version of library
        MutexLocker mu(Threads_lock);
        java_thread = JNIHandles::resolve_non_null(jthread);
        if (java_thread != NULL) {
          JavaThread* thr = java_lang_Thread::thread(java_thread);
          if (thr != NULL) {
            p = thr->parker();
            if (p != NULL) { // Bind to Java thread for next time.
              java_lang_Thread::set_park_event(java_thread, addr_to_java(p));
            }
          }
        }
      }
    }
  }
  if (p != NULL) {
#ifndef USDT2
    HS_DTRACE_PROBE1(hotspot, thread__unpark, p);
#else /* USDT2 */
    HOTSPOT_THREAD_UNPARK(
                          (uintptr_t) p);
#endif /* USDT2 */
    p->unpark();
  }
UNSAFE_END

UNSAFE_ENTRY(jint, Unsafe_Loadavg(JNIEnv *env, jobject unsafe, jdoubleArray loadavg, jint nelem))
  UnsafeWrapper("Unsafe_Loadavg");
  const int max_nelem = 3;
  double la[max_nelem];
  jint ret;

  typeArrayOop a = typeArrayOop(JNIHandles::resolve_non_null(loadavg));
  assert(a->is_typeArray(), "must be type array");

  if (nelem < 0 || nelem > max_nelem || a->length() < nelem) {
    ThreadToNativeFromVM ttnfv(thread);
    throw_new(env, "ArrayIndexOutOfBoundsException");
    return -1;
  }

  ret = os::loadavg(la, nelem);
  if (ret == -1) return -1;

  // if successful, ret is the number of samples actually retrieved.
  assert(ret >= 0 && ret <= max_nelem, "Unexpected loadavg return value");
  switch(ret) {
    case 3: a->double_at_put(2, (jdouble)la[2]); // fall through
    case 2: a->double_at_put(1, (jdouble)la[1]); // fall through
    case 1: a->double_at_put(0, (jdouble)la[0]); break;
  }
  return ret;
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_PrefetchRead(JNIEnv* env, jclass ignored, jobject obj, jlong offset))
  UnsafeWrapper("Unsafe_PrefetchRead");
  oop p = JNIHandles::resolve(obj);
  void* addr = index_oop_from_field_offset_long(p, 0);
  Prefetch::read(addr, (intx)offset);
UNSAFE_END

UNSAFE_ENTRY(void, Unsafe_PrefetchWrite(JNIEnv* env, jclass ignored, jobject obj, jlong offset))
  UnsafeWrapper("Unsafe_PrefetchWrite");
  oop p = JNIHandles::resolve(obj);
  void* addr = index_oop_from_field_offset_long(p, 0);
  Prefetch::write(addr, (intx)offset);
UNSAFE_END


/// JVM_RegisterUnsafeMethods

#define ADR "J"

#define LANG "Ljava/lang/"

#define OBJ LANG "Object;"
#define CLS LANG "Class;"
#define CTR LANG "reflect/Constructor;"
#define FLD LANG "reflect/Field;"
#define MTH LANG "reflect/Method;"
#define THR LANG "Throwable;"

#define DC0_Args LANG "String;[BII"
#define DC_Args  DC0_Args LANG "ClassLoader;" "Ljava/security/ProtectionDomain;"

#define CC (char*)  /*cast a literal from (const char*)*/
#define FN_PTR(f) CAST_FROM_FN_PTR(void*, &f)

// define deprecated accessors for compabitility with 1.4.0
#define DECLARE_GETSETOOP_140(Boolean, Z) \
    {CC "get" #Boolean,      CC "(" OBJ "I)" #Z,      FN_PTR(Unsafe_Get##Boolean##140)}, \
    {CC "put" #Boolean,      CC "(" OBJ "I" #Z ")V",   FN_PTR(Unsafe_Set##Boolean##140)}

// Note:  In 1.4.1, getObject and kin take both int and long offsets.
#define DECLARE_GETSETOOP_141(Boolean, Z) \
    {CC "get" #Boolean,      CC "(" OBJ "J)" #Z,      FN_PTR(Unsafe_Get##Boolean)}, \
    {CC "put" #Boolean,      CC "(" OBJ "J" #Z ")V",   FN_PTR(Unsafe_Set##Boolean)}

// Note:  In 1.5.0, there are volatile versions too
#define DECLARE_GETSETOOP(Boolean, Z) \
    {CC "get" #Boolean,      CC "(" OBJ "J)" #Z,      FN_PTR(Unsafe_Get##Boolean)}, \
    {CC "put" #Boolean,      CC "(" OBJ "J" #Z ")V",   FN_PTR(Unsafe_Set##Boolean)}, \
    {CC "get" #Boolean "Volatile",      CC "(" OBJ "J)" #Z,      FN_PTR(Unsafe_Get##Boolean##Volatile)}, \
    {CC "put" #Boolean "Volatile",      CC "(" OBJ "J" #Z ")V",   FN_PTR(Unsafe_Set##Boolean##Volatile)}


#define DECLARE_GETSETNATIVE(Byte, B) \
    {CC "get" #Byte,         CC "(" ADR ")" #B,       FN_PTR(Unsafe_GetNative##Byte)}, \
    {CC "put" #Byte,         CC "(" ADR#B ")V",      FN_PTR(Unsafe_SetNative##Byte)}



// These are the methods for 1.4.0
static JNINativeMethod methods_140[] = {
    {CC "getObject",        CC "(" OBJ "I)" OBJ "",   FN_PTR(Unsafe_GetObject140)},
    {CC "putObject",        CC "(" OBJ "I" OBJ ")V",  FN_PTR(Unsafe_SetObject140)},

    DECLARE_GETSETOOP_140(Boolean, Z),
    DECLARE_GETSETOOP_140(Byte, B),
    DECLARE_GETSETOOP_140(Short, S),
    DECLARE_GETSETOOP_140(Char, C),
    DECLARE_GETSETOOP_140(Int, I),
    DECLARE_GETSETOOP_140(Long, J),
    DECLARE_GETSETOOP_140(Float, F),
    DECLARE_GETSETOOP_140(Double, D),

    DECLARE_GETSETNATIVE(Byte, B),
    DECLARE_GETSETNATIVE(Short, S),
    DECLARE_GETSETNATIVE(Char, C),
    DECLARE_GETSETNATIVE(Int, I),
    DECLARE_GETSETNATIVE(Long, J),
    DECLARE_GETSETNATIVE(Float, F),
    DECLARE_GETSETNATIVE(Double, D),

    {CC "getAddress",         CC "(" ADR ")" ADR,             FN_PTR(Unsafe_GetNativeAddress)},
    {CC "putAddress",         CC "(" ADR "" ADR ")V",          FN_PTR(Unsafe_SetNativeAddress)},

    {CC "allocateMemory",     CC "(J)" ADR,                 FN_PTR(Unsafe_AllocateMemory)},
    {CC "reallocateMemory",   CC "(" ADR "J)" ADR,            FN_PTR(Unsafe_ReallocateMemory)},
    {CC "freeMemory",         CC "(" ADR ")V",               FN_PTR(Unsafe_FreeMemory)},

    {CC "fieldOffset",        CC "(" FLD ")I",               FN_PTR(Unsafe_FieldOffset)},
    {CC "staticFieldBase",    CC "(" CLS ")" OBJ,             FN_PTR(Unsafe_StaticFieldBaseFromClass)},
    {CC "ensureClassInitialized",CC "(" CLS ")V",            FN_PTR(Unsafe_EnsureClassInitialized)},
    {CC "arrayBaseOffset",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayBaseOffset)},
    {CC "arrayIndexScale",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayIndexScale)},
    {CC "addressSize",        CC "()I",                    FN_PTR(Unsafe_AddressSize)},
    {CC "pageSize",           CC "()I",                    FN_PTR(Unsafe_PageSize)},

    {CC "defineClass",        CC "(" DC0_Args ")" CLS,        FN_PTR(Unsafe_DefineClass0)},
    {CC "defineClass",        CC "(" DC_Args ")" CLS,         FN_PTR(Unsafe_DefineClass)},
    {CC "allocateInstance",   CC "(" CLS ")" OBJ,             FN_PTR(Unsafe_AllocateInstance)},
    {CC "monitorEnter",       CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorEnter)},
    {CC "monitorExit",        CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorExit)},
    {CC "throwException",     CC "(" THR ")V",               FN_PTR(Unsafe_ThrowException)}
};

// These are the methods prior to the JSR 166 changes in 1.5.0
static JNINativeMethod methods_141[] = {
    {CC "getObject",        CC "(" OBJ "J)" OBJ "",   FN_PTR(Unsafe_GetObject)},
    {CC "putObject",        CC "(" OBJ "J" OBJ ")V",  FN_PTR(Unsafe_SetObject)},

    DECLARE_GETSETOOP_141(Boolean, Z),
    DECLARE_GETSETOOP_141(Byte, B),
    DECLARE_GETSETOOP_141(Short, S),
    DECLARE_GETSETOOP_141(Char, C),
    DECLARE_GETSETOOP_141(Int, I),
    DECLARE_GETSETOOP_141(Long, J),
    DECLARE_GETSETOOP_141(Float, F),
    DECLARE_GETSETOOP_141(Double, D),

    DECLARE_GETSETNATIVE(Byte, B),
    DECLARE_GETSETNATIVE(Short, S),
    DECLARE_GETSETNATIVE(Char, C),
    DECLARE_GETSETNATIVE(Int, I),
    DECLARE_GETSETNATIVE(Long, J),
    DECLARE_GETSETNATIVE(Float, F),
    DECLARE_GETSETNATIVE(Double, D),

    {CC "getAddress",         CC "(" ADR ")" ADR,             FN_PTR(Unsafe_GetNativeAddress)},
    {CC "putAddress",         CC "(" ADR "" ADR ")V",          FN_PTR(Unsafe_SetNativeAddress)},

    {CC "allocateMemory",     CC "(J)" ADR,                 FN_PTR(Unsafe_AllocateMemory)},
    {CC "reallocateMemory",   CC "(" ADR "J)" ADR,            FN_PTR(Unsafe_ReallocateMemory)},
    {CC "freeMemory",         CC "(" ADR ")V",               FN_PTR(Unsafe_FreeMemory)},

    {CC "objectFieldOffset",  CC "(" FLD ")J",               FN_PTR(Unsafe_ObjectFieldOffset)},
    {CC "staticFieldOffset",  CC "(" FLD ")J",               FN_PTR(Unsafe_StaticFieldOffset)},
    {CC "staticFieldBase",    CC "(" FLD ")" OBJ,             FN_PTR(Unsafe_StaticFieldBaseFromField)},
    {CC "ensureClassInitialized",CC "(" CLS ")V",            FN_PTR(Unsafe_EnsureClassInitialized)},
    {CC "arrayBaseOffset",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayBaseOffset)},
    {CC "arrayIndexScale",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayIndexScale)},
    {CC "addressSize",        CC "()I",                    FN_PTR(Unsafe_AddressSize)},
    {CC "pageSize",           CC "()I",                    FN_PTR(Unsafe_PageSize)},

    {CC "defineClass",        CC "(" DC0_Args ")" CLS,        FN_PTR(Unsafe_DefineClass0)},
    {CC "defineClass",        CC "(" DC_Args ")" CLS,         FN_PTR(Unsafe_DefineClass)},
    {CC "allocateInstance",   CC "(" CLS ")" OBJ,             FN_PTR(Unsafe_AllocateInstance)},
    {CC "monitorEnter",       CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorEnter)},
    {CC "monitorExit",        CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorExit)},
    {CC "throwException",     CC "(" THR ")V",               FN_PTR(Unsafe_ThrowException)}

};

// These are the methods prior to the JSR 166 changes in 1.6.0
static JNINativeMethod methods_15[] = {
    {CC "getObject",        CC "(" OBJ "J)" OBJ "",   FN_PTR(Unsafe_GetObject)},
    {CC "putObject",        CC "(" OBJ "J" OBJ ")V",  FN_PTR(Unsafe_SetObject)},
    {CC "getObjectVolatile",CC "(" OBJ "J)" OBJ "",   FN_PTR(Unsafe_GetObjectVolatile)},
    {CC "putObjectVolatile",CC "(" OBJ "J" OBJ ")V",  FN_PTR(Unsafe_SetObjectVolatile)},


    DECLARE_GETSETOOP(Boolean, Z),
    DECLARE_GETSETOOP(Byte, B),
    DECLARE_GETSETOOP(Short, S),
    DECLARE_GETSETOOP(Char, C),
    DECLARE_GETSETOOP(Int, I),
    DECLARE_GETSETOOP(Long, J),
    DECLARE_GETSETOOP(Float, F),
    DECLARE_GETSETOOP(Double, D),

    DECLARE_GETSETNATIVE(Byte, B),
    DECLARE_GETSETNATIVE(Short, S),
    DECLARE_GETSETNATIVE(Char, C),
    DECLARE_GETSETNATIVE(Int, I),
    DECLARE_GETSETNATIVE(Long, J),
    DECLARE_GETSETNATIVE(Float, F),
    DECLARE_GETSETNATIVE(Double, D),

    {CC "getAddress",         CC "(" ADR ")" ADR,             FN_PTR(Unsafe_GetNativeAddress)},
    {CC "putAddress",         CC "(" ADR "" ADR ")V",          FN_PTR(Unsafe_SetNativeAddress)},

    {CC "allocateMemory",     CC "(J)" ADR,                 FN_PTR(Unsafe_AllocateMemory)},
    {CC "reallocateMemory",   CC "(" ADR "J)" ADR,            FN_PTR(Unsafe_ReallocateMemory)},
    {CC "freeMemory",         CC "(" ADR ")V",               FN_PTR(Unsafe_FreeMemory)},

    {CC "objectFieldOffset",  CC "(" FLD ")J",               FN_PTR(Unsafe_ObjectFieldOffset)},
    {CC "staticFieldOffset",  CC "(" FLD ")J",               FN_PTR(Unsafe_StaticFieldOffset)},
    {CC "staticFieldBase",    CC "(" FLD ")" OBJ,             FN_PTR(Unsafe_StaticFieldBaseFromField)},
    {CC "ensureClassInitialized",CC "(" CLS ")V",            FN_PTR(Unsafe_EnsureClassInitialized)},
    {CC "arrayBaseOffset",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayBaseOffset)},
    {CC "arrayIndexScale",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayIndexScale)},
    {CC "addressSize",        CC "()I",                    FN_PTR(Unsafe_AddressSize)},
    {CC "pageSize",           CC "()I",                    FN_PTR(Unsafe_PageSize)},

    {CC "defineClass",        CC "(" DC0_Args ")" CLS,        FN_PTR(Unsafe_DefineClass0)},
    {CC "defineClass",        CC "(" DC_Args ")" CLS,         FN_PTR(Unsafe_DefineClass)},
    {CC "allocateInstance",   CC "(" CLS ")" OBJ,             FN_PTR(Unsafe_AllocateInstance)},
    {CC "monitorEnter",       CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorEnter)},
    {CC "monitorExit",        CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorExit)},
    {CC "throwException",     CC "(" THR ")V",               FN_PTR(Unsafe_ThrowException)},
    {CC "compareAndSwapObject", CC "(" OBJ "J" OBJ "" OBJ ")Z",  FN_PTR(Unsafe_CompareAndSwapObject)},
    {CC "compareAndSwapInt",  CC "(" OBJ "J""I""I"")Z",      FN_PTR(Unsafe_CompareAndSwapInt)},
    {CC "compareAndSwapLong", CC "(" OBJ "J""J""J"")Z",      FN_PTR(Unsafe_CompareAndSwapLong)},
    {CC "park",               CC "(ZJ)V",                  FN_PTR(Unsafe_Park)},
    {CC "unpark",             CC "(" OBJ ")V",               FN_PTR(Unsafe_Unpark)}

};

// These are the methods for 1.6.0 and 1.7.0
static JNINativeMethod methods_16[] = {
    {CC "getObject",        CC "(" OBJ "J)" OBJ "",   FN_PTR(Unsafe_GetObject)},
    {CC "putObject",        CC "(" OBJ "J" OBJ ")V",  FN_PTR(Unsafe_SetObject)},
    {CC "getObjectVolatile",CC "(" OBJ "J)" OBJ "",   FN_PTR(Unsafe_GetObjectVolatile)},
    {CC "putObjectVolatile",CC "(" OBJ "J" OBJ ")V",  FN_PTR(Unsafe_SetObjectVolatile)},

    DECLARE_GETSETOOP(Boolean, Z),
    DECLARE_GETSETOOP(Byte, B),
    DECLARE_GETSETOOP(Short, S),
    DECLARE_GETSETOOP(Char, C),
    DECLARE_GETSETOOP(Int, I),
    DECLARE_GETSETOOP(Long, J),
    DECLARE_GETSETOOP(Float, F),
    DECLARE_GETSETOOP(Double, D),

    DECLARE_GETSETNATIVE(Byte, B),
    DECLARE_GETSETNATIVE(Short, S),
    DECLARE_GETSETNATIVE(Char, C),
    DECLARE_GETSETNATIVE(Int, I),
    DECLARE_GETSETNATIVE(Long, J),
    DECLARE_GETSETNATIVE(Float, F),
    DECLARE_GETSETNATIVE(Double, D),

    {CC "getAddress",         CC "(" ADR ")" ADR,             FN_PTR(Unsafe_GetNativeAddress)},
    {CC "putAddress",         CC "(" ADR "" ADR ")V",          FN_PTR(Unsafe_SetNativeAddress)},

    {CC "allocateMemory",     CC "(J)" ADR,                 FN_PTR(Unsafe_AllocateMemory)},
    {CC "reallocateMemory",   CC "(" ADR "J)" ADR,            FN_PTR(Unsafe_ReallocateMemory)},
    {CC "freeMemory",         CC "(" ADR ")V",               FN_PTR(Unsafe_FreeMemory)},

    {CC "objectFieldOffset",  CC "(" FLD ")J",               FN_PTR(Unsafe_ObjectFieldOffset)},
    {CC "staticFieldOffset",  CC "(" FLD ")J",               FN_PTR(Unsafe_StaticFieldOffset)},
    {CC "staticFieldBase",    CC "(" FLD ")" OBJ,             FN_PTR(Unsafe_StaticFieldBaseFromField)},
    {CC "ensureClassInitialized",CC "(" CLS ")V",            FN_PTR(Unsafe_EnsureClassInitialized)},
    {CC "arrayBaseOffset",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayBaseOffset)},
    {CC "arrayIndexScale",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayIndexScale)},
    {CC "addressSize",        CC "()I",                    FN_PTR(Unsafe_AddressSize)},
    {CC "pageSize",           CC "()I",                    FN_PTR(Unsafe_PageSize)},

    {CC "defineClass",        CC "(" DC0_Args ")" CLS,        FN_PTR(Unsafe_DefineClass0)},
    {CC "defineClass",        CC "(" DC_Args ")" CLS,         FN_PTR(Unsafe_DefineClass)},
    {CC "allocateInstance",   CC "(" CLS ")" OBJ,             FN_PTR(Unsafe_AllocateInstance)},
    {CC "monitorEnter",       CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorEnter)},
    {CC "monitorExit",        CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorExit)},
    {CC "tryMonitorEnter",    CC "(" OBJ ")Z",               FN_PTR(Unsafe_TryMonitorEnter)},
    {CC "throwException",     CC "(" THR ")V",               FN_PTR(Unsafe_ThrowException)},
    {CC "compareAndSwapObject", CC "(" OBJ "J" OBJ "" OBJ ")Z",  FN_PTR(Unsafe_CompareAndSwapObject)},
    {CC "compareAndSwapInt",  CC "(" OBJ "J""I""I"")Z",      FN_PTR(Unsafe_CompareAndSwapInt)},
    {CC "compareAndSwapLong", CC "(" OBJ "J""J""J"")Z",      FN_PTR(Unsafe_CompareAndSwapLong)},
    {CC "putOrderedObject",   CC "(" OBJ "J" OBJ ")V",         FN_PTR(Unsafe_SetOrderedObject)},
    {CC "putOrderedInt",      CC "(" OBJ "JI)V",             FN_PTR(Unsafe_SetOrderedInt)},
    {CC "putOrderedLong",     CC "(" OBJ "JJ)V",             FN_PTR(Unsafe_SetOrderedLong)},
    {CC "park",               CC "(ZJ)V",                  FN_PTR(Unsafe_Park)},
    {CC "unpark",             CC "(" OBJ ")V",               FN_PTR(Unsafe_Unpark)}
};

// These are the methods for 1.8.0
static JNINativeMethod methods_18[] = {
    {CC "getObject",        CC "(" OBJ "J)" OBJ "",   FN_PTR(Unsafe_GetObject)},
    {CC "putObject",        CC "(" OBJ "J" OBJ ")V",  FN_PTR(Unsafe_SetObject)},
    {CC "getObjectVolatile",CC "(" OBJ "J)" OBJ "",   FN_PTR(Unsafe_GetObjectVolatile)},
    {CC "putObjectVolatile",CC "(" OBJ "J" OBJ ")V",  FN_PTR(Unsafe_SetObjectVolatile)},

    DECLARE_GETSETOOP(Boolean, Z),
    DECLARE_GETSETOOP(Byte, B),
    DECLARE_GETSETOOP(Short, S),
    DECLARE_GETSETOOP(Char, C),
    DECLARE_GETSETOOP(Int, I),
    DECLARE_GETSETOOP(Long, J),
    DECLARE_GETSETOOP(Float, F),
    DECLARE_GETSETOOP(Double, D),

    DECLARE_GETSETNATIVE(Byte, B),
    DECLARE_GETSETNATIVE(Short, S),
    DECLARE_GETSETNATIVE(Char, C),
    DECLARE_GETSETNATIVE(Int, I),
    DECLARE_GETSETNATIVE(Long, J),
    DECLARE_GETSETNATIVE(Float, F),
    DECLARE_GETSETNATIVE(Double, D),

    {CC "getAddress",         CC "(" ADR ")" ADR,             FN_PTR(Unsafe_GetNativeAddress)},
    {CC "putAddress",         CC "(" ADR "" ADR ")V",          FN_PTR(Unsafe_SetNativeAddress)},

    {CC "allocateMemory",     CC "(J)" ADR,                 FN_PTR(Unsafe_AllocateMemory)},
    {CC "reallocateMemory",   CC "(" ADR "J)" ADR,            FN_PTR(Unsafe_ReallocateMemory)},
    {CC "freeMemory",         CC "(" ADR ")V",               FN_PTR(Unsafe_FreeMemory)},

    {CC "objectFieldOffset",  CC "(" FLD ")J",               FN_PTR(Unsafe_ObjectFieldOffset)},
    {CC "staticFieldOffset",  CC "(" FLD ")J",               FN_PTR(Unsafe_StaticFieldOffset)},
    {CC "staticFieldBase",    CC "(" FLD ")" OBJ,             FN_PTR(Unsafe_StaticFieldBaseFromField)},
    {CC "ensureClassInitialized",CC "(" CLS ")V",            FN_PTR(Unsafe_EnsureClassInitialized)},
    {CC "arrayBaseOffset",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayBaseOffset)},
    {CC "arrayIndexScale",    CC "(" CLS ")I",               FN_PTR(Unsafe_ArrayIndexScale)},
    {CC "addressSize",        CC "()I",                    FN_PTR(Unsafe_AddressSize)},
    {CC "pageSize",           CC "()I",                    FN_PTR(Unsafe_PageSize)},

    {CC "defineClass",        CC "(" DC_Args ")" CLS,         FN_PTR(Unsafe_DefineClass)},
    {CC "allocateInstance",   CC "(" CLS ")" OBJ,             FN_PTR(Unsafe_AllocateInstance)},
    {CC "monitorEnter",       CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorEnter)},
    {CC "monitorExit",        CC "(" OBJ ")V",               FN_PTR(Unsafe_MonitorExit)},
    {CC "tryMonitorEnter",    CC "(" OBJ ")Z",               FN_PTR(Unsafe_TryMonitorEnter)},
    {CC "throwException",     CC "(" THR ")V",               FN_PTR(Unsafe_ThrowException)},
    {CC "compareAndSwapObject", CC "(" OBJ "J" OBJ "" OBJ ")Z",  FN_PTR(Unsafe_CompareAndSwapObject)},
    {CC "compareAndSwapInt",  CC "(" OBJ "J""I""I"")Z",      FN_PTR(Unsafe_CompareAndSwapInt)},
    {CC "compareAndSwapLong", CC "(" OBJ "J""J""J"")Z",      FN_PTR(Unsafe_CompareAndSwapLong)},
    {CC "putOrderedObject",   CC "(" OBJ "J" OBJ ")V",         FN_PTR(Unsafe_SetOrderedObject)},
    {CC "putOrderedInt",      CC "(" OBJ "JI)V",             FN_PTR(Unsafe_SetOrderedInt)},
    {CC "putOrderedLong",     CC "(" OBJ "JJ)V",             FN_PTR(Unsafe_SetOrderedLong)},
    {CC "park",               CC "(ZJ)V",                  FN_PTR(Unsafe_Park)},
    {CC "unpark",             CC "(" OBJ ")V",               FN_PTR(Unsafe_Unpark)}
};

JNINativeMethod loadavg_method[] = {
    {CC "getLoadAverage",     CC "([DI)I",                 FN_PTR(Unsafe_Loadavg)}
};

JNINativeMethod prefetch_methods[] = {
    {CC "prefetchRead",       CC "(" OBJ "J)V",              FN_PTR(Unsafe_PrefetchRead)},
    {CC "prefetchWrite",      CC "(" OBJ "J)V",              FN_PTR(Unsafe_PrefetchWrite)},
    {CC "prefetchReadStatic", CC "(" OBJ "J)V",              FN_PTR(Unsafe_PrefetchRead)},
    {CC "prefetchWriteStatic",CC "(" OBJ "J)V",              FN_PTR(Unsafe_PrefetchWrite)}
};

JNINativeMethod memcopy_methods_17[] = {
    {CC "copyMemory",         CC "(" OBJ "J" OBJ "JJ)V",       FN_PTR(Unsafe_CopyMemory2)},
    {CC "setMemory",          CC "(" OBJ "JJB)V",            FN_PTR(Unsafe_SetMemory2)}
};

JNINativeMethod memcopy_methods_15[] = {
    {CC "setMemory",          CC "(" ADR "JB)V",             FN_PTR(Unsafe_SetMemory)},
    {CC "copyMemory",         CC "(" ADR ADR "J)V",          FN_PTR(Unsafe_CopyMemory)}
};

JNINativeMethod anonk_methods[] = {
    {CC "defineAnonymousClass", CC "(" DAC_Args ")" CLS,      FN_PTR(Unsafe_DefineAnonymousClass)},
};

JNINativeMethod lform_methods[] = {
    {CC "shouldBeInitialized",CC "(" CLS ")Z",               FN_PTR(Unsafe_ShouldBeInitialized)},
};

JNINativeMethod fence_methods[] = {
    {CC "loadFence",          CC "()V",                    FN_PTR(Unsafe_LoadFence)},
    {CC "storeFence",         CC "()V",                    FN_PTR(Unsafe_StoreFence)},
    {CC "fullFence",          CC "()V",                    FN_PTR(Unsafe_FullFence)},
};

#undef CC
#undef FN_PTR

#undef ADR
#undef LANG
#undef OBJ
#undef CLS
#undef CTR
#undef FLD
#undef MTH
#undef THR
#undef DC0_Args
#undef DC_Args

#undef DECLARE_GETSETOOP
#undef DECLARE_GETSETNATIVE


/**
 * Helper method to register native methods.
 */
static bool register_natives(const char* message, JNIEnv* env, jclass clazz, const JNINativeMethod* methods, jint nMethods) {
  int status = env->RegisterNatives(clazz, methods, nMethods);
  if (status < 0 || env->ExceptionOccurred()) {
    if (PrintMiscellaneous && (Verbose || WizardMode)) {
      tty->print_cr("Unsafe:  failed registering %s", message);
    }
    env->ExceptionClear();
    return false;
  } else {
    if (PrintMiscellaneous && (Verbose || WizardMode)) {
      tty->print_cr("Unsafe:  successfully registered %s", message);
    }
    return true;
  }
}


// This one function is exported, used by NativeLookup.
// The Unsafe_xxx functions above are called only from the interpreter.
// The optimizer looks at names and signatures to recognize
// individual functions.

JVM_ENTRY(void, JVM_RegisterUnsafeMethods(JNIEnv *env, jclass unsafecls))
  UnsafeWrapper("JVM_RegisterUnsafeMethods");
  {
    ThreadToNativeFromVM ttnfv(thread);

    // Unsafe methods
    {
      bool success = false;
      // We need to register the 1.6 methods first because the 1.8 methods would register fine on 1.7 and 1.6
      if (!success) {
        success = register_natives("1.6 methods",   env, unsafecls, methods_16,  sizeof(methods_16)/sizeof(JNINativeMethod));
      }
      if (!success) {
        success = register_natives("1.8 methods",   env, unsafecls, methods_18,  sizeof(methods_18)/sizeof(JNINativeMethod));
      }
      if (!success) {
        success = register_natives("1.5 methods",   env, unsafecls, methods_15,  sizeof(methods_15)/sizeof(JNINativeMethod));
      }
      if (!success) {
        success = register_natives("1.4.1 methods", env, unsafecls, methods_141, sizeof(methods_141)/sizeof(JNINativeMethod));
      }
      if (!success) {
        success = register_natives("1.4.0 methods", env, unsafecls, methods_140, sizeof(methods_140)/sizeof(JNINativeMethod));
      }
      guarantee(success, "register unsafe natives");
    }

    // Unsafe.getLoadAverage
    register_natives("1.6 loadavg method", env, unsafecls, loadavg_method, sizeof(loadavg_method)/sizeof(JNINativeMethod));

    // Prefetch methods
    register_natives("1.6 prefetch methods", env, unsafecls, prefetch_methods, sizeof(prefetch_methods)/sizeof(JNINativeMethod));

    // Memory copy methods
    {
      bool success = false;
      if (!success) {
        success = register_natives("1.7 memory copy methods", env, unsafecls, memcopy_methods_17, sizeof(memcopy_methods_17)/sizeof(JNINativeMethod));
      }
      if (!success) {
        success = register_natives("1.5 memory copy methods", env, unsafecls, memcopy_methods_15, sizeof(memcopy_methods_15)/sizeof(JNINativeMethod));
      }
    }

    // Unsafe.defineAnonymousClass
    if (EnableInvokeDynamic) {
      register_natives("1.7 define anonymous class method", env, unsafecls, anonk_methods, sizeof(anonk_methods)/sizeof(JNINativeMethod));
    }

    // Unsafe.shouldBeInitialized
    if (EnableInvokeDynamic) {
      register_natives("1.7 LambdaForm support", env, unsafecls, lform_methods, sizeof(lform_methods)/sizeof(JNINativeMethod));
    }

    // Fence methods
    register_natives("1.8 fence methods", env, unsafecls, fence_methods, sizeof(fence_methods)/sizeof(JNINativeMethod));
  }
JVM_END
