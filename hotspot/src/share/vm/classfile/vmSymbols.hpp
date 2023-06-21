/*
 * Copyright (c) 1997, 2022, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_CLASSFILE_VMSYMBOLS_HPP
#define SHARE_VM_CLASSFILE_VMSYMBOLS_HPP

#include "jfr/support/jfrIntrinsics.hpp"
#include "memory/iterator.hpp"
#include "oops/symbol.hpp"
#include "utilities/macros.hpp"

// The class vmSymbols is a name space for fast lookup of
// symbols commonly used in the VM.
//
// Sample usage:
//
//   Symbol* obj       = vmSymbols::java_lang_Object();


// Useful sub-macros exported by this header file:

#define VM_SYMBOL_ENUM_NAME(name)    name##_enum
#define VM_INTRINSIC_IGNORE(id, class, name, sig, flags) /*ignored*/
#define VM_SYMBOL_IGNORE(id, name)                       /*ignored*/
#define VM_ALIAS_IGNORE(id, id2)                         /*ignored*/


// Mapping function names to values. New entries should be added below.

#define VM_SYMBOLS_DO(template, do_alias)                                                         \
  /* commonly used class names */                                                                 \
  template(java_lang_System,                          "java/lang/System")                         \
  template(java_lang_Object,                          "java/lang/Object")                         \
  template(java_lang_Class,                           "java/lang/Class")                          \
  template(java_lang_String,                          "java/lang/String")                         \
  template(com_alibaba_tenant_TenantGlobals,          "com/alibaba/tenant/TenantGlobals")         \
  template(com_alibaba_tenant_TenantConfiguration,    "com/alibaba/tenant/TenantConfiguration")   \
  template(com_alibaba_tenant_TenantState,            "com/alibaba/tenant/TenantState")           \
  template(com_alibaba_tenant_TenantException,        "com/alibaba/tenant/TenantException")       \
  template(com_alibaba_tenant_TenantContainer,        "com/alibaba/tenant/TenantContainer")       \
  template(com_alibaba_tenant_JGroup,                 "com/alibaba/tenant/JGroup")                \
  template(java_lang_Thread,                          "java/lang/Thread")                         \
  template(java_lang_ThreadGroup,                     "java/lang/ThreadGroup")                    \
  template(java_lang_Cloneable,                       "java/lang/Cloneable")                      \
  template(java_lang_Throwable,                       "java/lang/Throwable")                      \
  template(java_lang_ClassLoader,                     "java/lang/ClassLoader")                    \
  template(java_lang_ClassLoader_NativeLibrary,       "java/lang/ClassLoader\x024NativeLibrary")  \
  template(java_lang_ThreadDeath,                     "java/lang/ThreadDeath")                    \
  template(java_lang_Boolean,                         "java/lang/Boolean")                        \
  template(java_lang_Character,                       "java/lang/Character")                      \
  template(java_lang_Character_CharacterCache,        "java/lang/Character$CharacterCache")       \
  template(java_lang_Float,                           "java/lang/Float")                          \
  template(java_lang_Double,                          "java/lang/Double")                         \
  template(java_lang_Byte,                            "java/lang/Byte")                           \
  template(java_lang_Byte_ByteCache,                  "java/lang/Byte$ByteCache")                 \
  template(java_lang_Short,                           "java/lang/Short")                          \
  template(java_lang_Short_ShortCache,                "java/lang/Short$ShortCache")               \
  template(java_lang_Integer,                         "java/lang/Integer")                        \
  template(java_lang_Integer_IntegerCache,            "java/lang/Integer$IntegerCache")           \
  template(java_lang_Long,                            "java/lang/Long")                           \
  template(java_lang_Long_LongCache,                  "java/lang/Long$LongCache")                 \
  template(java_lang_Shutdown,                        "java/lang/Shutdown")                       \
  template(java_lang_ref_Reference,                   "java/lang/ref/Reference")                  \
  template(java_lang_ref_SoftReference,               "java/lang/ref/SoftReference")              \
  template(java_lang_ref_WeakReference,               "java/lang/ref/WeakReference")              \
  template(java_lang_ref_FinalReference,              "java/lang/ref/FinalReference")             \
  template(java_lang_ref_PhantomReference,            "java/lang/ref/PhantomReference")           \
  template(java_lang_ref_Finalizer,                   "java/lang/ref/Finalizer")                  \
  template(java_lang_ref_ReferenceQueue,              "java/lang/ref/ReferenceQueue")             \
  template(java_lang_reflect_AccessibleObject,        "java/lang/reflect/AccessibleObject")       \
  template(java_lang_reflect_Method,                  "java/lang/reflect/Method")                 \
  template(java_lang_reflect_Constructor,             "java/lang/reflect/Constructor")            \
  template(java_lang_reflect_Field,                   "java/lang/reflect/Field")                  \
  template(java_lang_reflect_Parameter,               "java/lang/reflect/Parameter")              \
  template(java_lang_reflect_Array,                   "java/lang/reflect/Array")                  \
  template(java_lang_StringBuffer,                    "java/lang/StringBuffer")                   \
  template(java_lang_StringBuilder,                   "java/lang/StringBuilder")                  \
  template(java_lang_CharSequence,                    "java/lang/CharSequence")                   \
  template(java_lang_SecurityManager,                 "java/lang/SecurityManager")                \
  template(java_security_AccessControlContext,        "java/security/AccessControlContext")       \
  template(java_security_CodeSource,                  "java/security/CodeSource")                 \
  template(java_security_ProtectionDomain,            "java/security/ProtectionDomain")           \
  template(java_security_SecureClassLoader,           "java/security/SecureClassLoader")          \
  template(java_net_URLClassLoader,                   "java/net/URLClassLoader")                  \
  template(java_net_URL,                              "java/net/URL")                             \
  template(java_util_jar_Manifest,                    "java/util/jar/Manifest")                   \
  template(impliesCreateAccessControlContext_name,    "impliesCreateAccessControlContext")        \
  template(java_io_OutputStream,                      "java/io/OutputStream")                     \
  template(java_io_Reader,                            "java/io/Reader")                           \
  template(java_io_BufferedReader,                    "java/io/BufferedReader")                   \
  template(java_io_File,                              "java/io/File")                             \
  template(java_io_FileInputStream,                   "java/io/FileInputStream")                  \
  template(java_io_ByteArrayInputStream,              "java/io/ByteArrayInputStream")             \
  template(java_io_Serializable,                      "java/io/Serializable")                     \
  template(java_util_Arrays,                          "java/util/Arrays")                         \
  template(java_util_Properties,                      "java/util/Properties")                     \
  template(java_util_Vector,                          "java/util/Vector")                         \
  template(java_util_AbstractList,                    "java/util/AbstractList")                   \
  template(java_util_Hashtable,                       "java/util/Hashtable")                      \
  template(java_lang_Compiler,                        "java/lang/Compiler")                       \
  template(sun_misc_Signal,                           "sun/misc/Signal")                          \
  template(sun_misc_Launcher,                         "sun/misc/Launcher")                        \
  template(java_lang_AssertionStatusDirectives,       "java/lang/AssertionStatusDirectives")      \
  template(getBootClassPathEntryForClass_name,        "getBootClassPathEntryForClass")            \
  template(sun_misc_PostVMInitHook,                   "sun/misc/PostVMInitHook")                  \
  template(sun_misc_Launcher_AppClassLoader,          "sun/misc/Launcher$AppClassLoader")         \
  template(sun_misc_Launcher_ExtClassLoader,          "sun/misc/Launcher$ExtClassLoader")         \
                                                                                                  \
  /* Java runtime version access */                                                               \
  template(sun_misc_Version,                          "sun/misc/Version")                         \
  template(java_runtime_name_name,                    "java_runtime_name")                        \
  template(java_runtime_version_name,                 "java_runtime_version")                     \
  template(java_distro_name_name,                     "java_distro_name")                         \
  template(java_distro_version_name,                  "java_distro_version")                      \
                                                                                                  \
  /* class file format tags */                                                                    \
  template(tag_source_file,                           "SourceFile")                               \
  template(tag_inner_classes,                         "InnerClasses")                             \
  template(tag_constant_value,                        "ConstantValue")                            \
  template(tag_code,                                  "Code")                                     \
  template(tag_exceptions,                            "Exceptions")                               \
  template(tag_line_number_table,                     "LineNumberTable")                          \
  template(tag_local_variable_table,                  "LocalVariableTable")                       \
  template(tag_local_variable_type_table,             "LocalVariableTypeTable")                   \
  template(tag_method_parameters,                     "MethodParameters")                         \
  template(tag_stack_map_table,                       "StackMapTable")                            \
  template(tag_synthetic,                             "Synthetic")                                \
  template(tag_deprecated,                            "Deprecated")                               \
  template(tag_source_debug_extension,                "SourceDebugExtension")                     \
  template(tag_signature,                             "Signature")                                \
  template(tag_runtime_visible_annotations,           "RuntimeVisibleAnnotations")                \
  template(tag_runtime_invisible_annotations,         "RuntimeInvisibleAnnotations")              \
  template(tag_runtime_visible_parameter_annotations, "RuntimeVisibleParameterAnnotations")       \
  template(tag_runtime_invisible_parameter_annotations,"RuntimeInvisibleParameterAnnotations")    \
  template(tag_annotation_default,                    "AnnotationDefault")                        \
  template(tag_runtime_visible_type_annotations,      "RuntimeVisibleTypeAnnotations")            \
  template(tag_runtime_invisible_type_annotations,    "RuntimeInvisibleTypeAnnotations")          \
  template(tag_enclosing_method,                      "EnclosingMethod")                          \
  template(tag_bootstrap_methods,                     "BootstrapMethods")                         \
                                                                                                  \
  /* exception klasses: at least all exceptions thrown by the VM have entries here */             \
  template(java_lang_ArithmeticException,             "java/lang/ArithmeticException")            \
  template(java_lang_ArrayIndexOutOfBoundsException,  "java/lang/ArrayIndexOutOfBoundsException") \
  template(java_lang_ArrayStoreException,             "java/lang/ArrayStoreException")            \
  template(java_lang_ClassCastException,              "java/lang/ClassCastException")             \
  template(java_lang_ClassNotFoundException,          "java/lang/ClassNotFoundException")         \
  template(java_lang_CloneNotSupportedException,      "java/lang/CloneNotSupportedException")     \
  template(java_lang_IllegalAccessException,          "java/lang/IllegalAccessException")         \
  template(java_lang_IllegalArgumentException,        "java/lang/IllegalArgumentException")       \
  template(java_lang_IllegalStateException,           "java/lang/IllegalStateException")          \
  template(java_lang_IllegalMonitorStateException,    "java/lang/IllegalMonitorStateException")   \
  template(java_lang_IllegalThreadStateException,     "java/lang/IllegalThreadStateException")    \
  template(java_lang_IndexOutOfBoundsException,       "java/lang/IndexOutOfBoundsException")      \
  template(java_lang_InstantiationException,          "java/lang/InstantiationException")         \
  template(java_lang_InstantiationError,              "java/lang/InstantiationError")             \
  template(java_lang_InterruptedException,            "java/lang/InterruptedException")           \
  template(java_lang_BootstrapMethodError,            "java/lang/BootstrapMethodError")           \
  template(java_lang_LinkageError,                    "java/lang/LinkageError")                   \
  template(java_lang_NegativeArraySizeException,      "java/lang/NegativeArraySizeException")     \
  template(java_lang_NoSuchFieldException,            "java/lang/NoSuchFieldException")           \
  template(java_lang_NoSuchMethodException,           "java/lang/NoSuchMethodException")          \
  template(java_lang_NullPointerException,            "java/lang/NullPointerException")           \
  template(java_lang_StringIndexOutOfBoundsException, "java/lang/StringIndexOutOfBoundsException")\
  template(java_lang_UnsupportedOperationException,   "java/lang/UnsupportedOperationException")  \
  template(java_lang_InvalidClassException,           "java/lang/InvalidClassException")          \
  template(java_lang_reflect_InvocationTargetException, "java/lang/reflect/InvocationTargetException") \
  template(java_lang_Exception,                       "java/lang/Exception")                      \
  template(java_lang_RuntimeException,                "java/lang/RuntimeException")               \
  template(java_io_IOException,                       "java/io/IOException")                      \
  template(java_security_PrivilegedActionException,   "java/security/PrivilegedActionException")  \
                                                                                                  \
  /* error klasses: at least all errors thrown by the VM have entries here */                     \
  template(java_lang_AbstractMethodError,             "java/lang/AbstractMethodError")            \
  template(java_lang_ClassCircularityError,           "java/lang/ClassCircularityError")          \
  template(java_lang_ClassFormatError,                "java/lang/ClassFormatError")               \
  template(java_lang_UnsupportedClassVersionError,    "java/lang/UnsupportedClassVersionError")   \
  template(java_lang_Error,                           "java/lang/Error")                          \
  template(java_lang_ExceptionInInitializerError,     "java/lang/ExceptionInInitializerError")    \
  template(java_lang_IllegalAccessError,              "java/lang/IllegalAccessError")             \
  template(java_lang_IncompatibleClassChangeError,    "java/lang/IncompatibleClassChangeError")   \
  template(java_lang_InternalError,                   "java/lang/InternalError")                  \
  template(java_lang_NoClassDefFoundError,            "java/lang/NoClassDefFoundError")           \
  template(java_lang_NoSuchFieldError,                "java/lang/NoSuchFieldError")               \
  template(java_lang_NoSuchMethodError,               "java/lang/NoSuchMethodError")              \
  template(java_lang_OutOfMemoryError,                "java/lang/OutOfMemoryError")               \
  template(java_lang_UnsatisfiedLinkError,            "java/lang/UnsatisfiedLinkError")           \
  template(java_lang_VerifyError,                     "java/lang/VerifyError")                    \
  template(java_lang_SecurityException,               "java/lang/SecurityException")              \
  template(java_lang_VirtualMachineError,             "java/lang/VirtualMachineError")            \
  template(java_lang_StackOverflowError,              "java/lang/StackOverflowError")             \
  template(java_lang_StackTraceElement,               "java/lang/StackTraceElement")              \
                                                                                                  \
  /* Concurrency support */                                                                       \
  template(java_util_concurrent_locks_AbstractOwnableSynchronizer,           "java/util/concurrent/locks/AbstractOwnableSynchronizer") \
  template(java_util_concurrent_atomic_AtomicIntegerFieldUpdater_Impl,       "java/util/concurrent/atomic/AtomicIntegerFieldUpdater$AtomicIntegerFieldUpdaterImpl") \
  template(java_util_concurrent_atomic_AtomicLongFieldUpdater_CASUpdater,    "java/util/concurrent/atomic/AtomicLongFieldUpdater$CASUpdater") \
  template(java_util_concurrent_atomic_AtomicLongFieldUpdater_LockedUpdater, "java/util/concurrent/atomic/AtomicLongFieldUpdater$LockedUpdater") \
  template(java_util_concurrent_atomic_AtomicReferenceFieldUpdater_Impl,     "java/util/concurrent/atomic/AtomicReferenceFieldUpdater$AtomicReferenceFieldUpdaterImpl") \
  template(sun_misc_Contended_signature,              "Lsun/misc/Contended;")                     \
                                                                                                  \
  /* class symbols needed by intrinsics */                                                        \
  VM_INTRINSICS_DO(VM_INTRINSIC_IGNORE, template, VM_SYMBOL_IGNORE, VM_SYMBOL_IGNORE, VM_ALIAS_IGNORE) \
                                                                                                  \
  /* Support for reflection based on dynamic bytecode generation (JDK 1.4 and above) */           \
                                                                                                  \
  template(sun_reflect_FieldInfo,                     "sun/reflect/FieldInfo")                    \
  template(sun_reflect_MethodInfo,                    "sun/reflect/MethodInfo")                   \
  template(sun_reflect_MagicAccessorImpl,             "sun/reflect/MagicAccessorImpl")            \
  template(sun_reflect_MethodAccessorImpl,            "sun/reflect/MethodAccessorImpl")           \
  template(sun_reflect_ConstructorAccessorImpl,       "sun/reflect/ConstructorAccessorImpl")      \
  template(sun_reflect_SerializationConstructorAccessorImpl, "sun/reflect/SerializationConstructorAccessorImpl") \
  template(sun_reflect_DelegatingClassLoader,         "sun/reflect/DelegatingClassLoader")        \
  template(sun_reflect_Reflection,                    "sun/reflect/Reflection")                   \
  template(sun_reflect_CallerSensitive,               "sun/reflect/CallerSensitive")              \
  template(sun_reflect_CallerSensitive_signature,     "Lsun/reflect/CallerSensitive;")            \
  template(checkedExceptions_name,                    "checkedExceptions")                        \
  template(clazz_name,                                "clazz")                                    \
  template(exceptionTypes_name,                       "exceptionTypes")                           \
  template(modifiers_name,                            "modifiers")                                \
  template(newConstructor_name,                       "newConstructor")                           \
  template(newConstructor_signature,                  "(Lsun/reflect/MethodInfo;)Ljava/lang/reflect/Constructor;") \
  template(newField_name,                             "newField")                                 \
  template(newField_signature,                        "(Lsun/reflect/FieldInfo;)Ljava/lang/reflect/Field;") \
  template(newMethod_name,                            "newMethod")                                \
  template(newMethod_signature,                       "(Lsun/reflect/MethodInfo;)Ljava/lang/reflect/Method;") \
  template(invokeBasic_name,                          "invokeBasic")                              \
  template(linkToVirtual_name,                        "linkToVirtual")                            \
  template(linkToStatic_name,                         "linkToStatic")                             \
  template(linkToSpecial_name,                        "linkToSpecial")                            \
  template(linkToInterface_name,                      "linkToInterface")                          \
  template(compiledLambdaForm_name,                   "<compiledLambdaForm>")  /*fake name*/      \
  template(star_name,                                 "*") /*not really a name*/                  \
  template(invoke_name,                               "invoke")                                   \
  template(override_name,                             "override")                                 \
  template(parameterTypes_name,                       "parameterTypes")                           \
  template(returnType_name,                           "returnType")                               \
  template(slot_name,                                 "slot")                                     \
                                                                                                  \
  /* Support for annotations (JDK 1.5 and above) */                                               \
                                                                                                  \
  template(annotations_name,                          "annotations")                              \
  template(index_name,                                "index")                                    \
  template(executable_name,                           "executable")                               \
  template(parameter_annotations_name,                "parameterAnnotations")                     \
  template(annotation_default_name,                   "annotationDefault")                        \
  template(sun_reflect_ConstantPool,                  "sun/reflect/ConstantPool")                 \
  template(ConstantPool_name,                         "constantPoolOop")                          \
  template(sun_reflect_UnsafeStaticFieldAccessorImpl, "sun/reflect/UnsafeStaticFieldAccessorImpl")\
  template(base_name,                                 "base")                                     \
  /* Type Annotations (JDK 8 and above) */                                                        \
  template(type_annotations_name,                     "typeAnnotations")                          \
                                                                                                  \
                                                                                                  \
  /* Support for JSR 292 & invokedynamic (JDK 1.7 and above) */                                   \
  template(java_lang_invoke_CallSite,                 "java/lang/invoke/CallSite")                \
  template(java_lang_invoke_ConstantCallSite,         "java/lang/invoke/ConstantCallSite")        \
  template(java_lang_invoke_DirectMethodHandle,       "java/lang/invoke/DirectMethodHandle")      \
  template(java_lang_invoke_MutableCallSite,          "java/lang/invoke/MutableCallSite")         \
  template(java_lang_invoke_VolatileCallSite,         "java/lang/invoke/VolatileCallSite")        \
  template(java_lang_invoke_MethodHandle,             "java/lang/invoke/MethodHandle")            \
  template(java_lang_invoke_MethodType,               "java/lang/invoke/MethodType")              \
  template(java_lang_invoke_MethodType_signature,     "Ljava/lang/invoke/MethodType;")            \
  template(java_lang_invoke_MemberName_signature,     "Ljava/lang/invoke/MemberName;")            \
  template(java_lang_invoke_LambdaForm_signature,     "Ljava/lang/invoke/LambdaForm;")            \
  template(java_lang_invoke_MethodHandle_signature,   "Ljava/lang/invoke/MethodHandle;")          \
  /* internal classes known only to the JVM: */                                                   \
  template(java_lang_invoke_MemberName,               "java/lang/invoke/MemberName")              \
  template(java_lang_invoke_MethodHandleNatives,      "java/lang/invoke/MethodHandleNatives")     \
  template(java_lang_invoke_LambdaForm,               "java/lang/invoke/LambdaForm")              \
  template(java_lang_invoke_ForceInline_signature,    "Ljava/lang/invoke/ForceInline;")           \
  template(java_lang_invoke_DontInline_signature,     "Ljava/lang/invoke/DontInline;")            \
  template(java_lang_invoke_InjectedProfile_signature, "Ljava/lang/invoke/InjectedProfile;")      \
  template(java_lang_invoke_Stable_signature,         "Ljava/lang/invoke/Stable;")                \
  template(java_lang_invoke_LambdaForm_Compiled_signature, "Ljava/lang/invoke/LambdaForm$Compiled;") \
  template(java_lang_invoke_LambdaForm_Hidden_signature, "Ljava/lang/invoke/LambdaForm$Hidden;")  \
  /* internal up-calls made only by the JVM, via class sun.invoke.MethodHandleNatives: */         \
  template(findMethodHandleType_name,                 "findMethodHandleType")                     \
  template(findMethodHandleType_signature,       "(Ljava/lang/Class;[Ljava/lang/Class;)Ljava/lang/invoke/MethodType;") \
  template(linkMethodHandleConstant_name,             "linkMethodHandleConstant")                 \
  template(linkMethodHandleConstant_signature, "(Ljava/lang/Class;ILjava/lang/Class;Ljava/lang/String;Ljava/lang/Object;)Ljava/lang/invoke/MethodHandle;") \
  template(linkMethod_name,                           "linkMethod")                               \
  template(linkMethod_signature, "(Ljava/lang/Class;ILjava/lang/Class;Ljava/lang/String;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/invoke/MemberName;") \
  template(linkCallSite_name,                         "linkCallSite")                             \
  template(linkCallSite_signature, "(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/invoke/MemberName;") \
  template(setTargetNormal_name,                      "setTargetNormal")                          \
  template(setTargetVolatile_name,                    "setTargetVolatile")                        \
  template(setTarget_signature,                       "(Ljava/lang/invoke/MethodHandle;)V")       \
  NOT_LP64(  do_alias(intptr_signature,               int_signature)  )                           \
  LP64_ONLY( do_alias(intptr_signature,               long_signature) )                           \
                                                                                                  \
  /* common method and field names */                                                             \
  template(object_initializer_name,                   "<init>")                                   \
  template(class_initializer_name,                    "<clinit>")                                 \
  template(println_name,                              "println")                                  \
  template(printStackTrace_name,                      "printStackTrace")                          \
  template(main_name,                                 "main")                                     \
  template(name_name,                                 "name")                                     \
  template(priority_name,                             "priority")                                 \
  template(stillborn_name,                            "stillborn")                                \
  template(group_name,                                "group")                                    \
  template(daemon_name,                               "daemon")                                   \
  template(eetop_name,                                "eetop")                                    \
  template(inheritedTenantContainer_name,             "inheritedTenantContainer")                 \
  template(resourceContainer_name,                    "resourceContainer")                        \
  template(thread_status_name,                        "threadStatus")                             \
  template(run_method_name,                           "run")                                      \
  template(runThread_method_name,                     "runThread")                                \
  template(exit_method_name,                          "exit")                                     \
  template(add_method_name,                           "add")                                      \
  template(remove_method_name,                        "remove")                                   \
  template(parent_name,                               "parent")                                   \
  template(threads_name,                              "threads")                                  \
  template(groups_name,                               "groups")                                   \
  template(maxPriority_name,                          "maxPriority")                              \
  template(destroyed_name,                            "destroyed")                                \
  template(vmAllowSuspension_name,                    "vmAllowSuspension")                        \
  template(nthreads_name,                             "nthreads")                                 \
  template(ngroups_name,                              "ngroups")                                  \
  template(shutdown_method_name,                      "shutdown")                                 \
  template(finalize_method_name,                      "finalize")                                 \
  template(reference_lock_name,                       "lock")                                     \
  template(reference_discovered_name,                 "discovered")                               \
  template(run_finalization_name,                     "runFinalization")                          \
  template(uncaughtException_name,                    "uncaughtException")                        \
  template(dispatchUncaughtException_name,            "dispatchUncaughtException")                \
  template(initializeSystemClass_name,                "initializeSystemClass")                    \
  template(initializeWispClass_name,                  "initializeWispClass")                      \
  template(startWispDaemons_name,                     "startWispDaemons")                         \
  template(initialize_name,                           "initialize")                               \
  template(loadClass_name,                            "loadClass")                                \
  template(loadClassInternal_name,                    "loadClassInternal")                        \
  template(loadClassFromCDS_name,                     "loadClassFromCDS")                         \
  template(loadClassFromCDS_signature,                "(Ljava/lang/String;Ljava/lang/String;JI)Ljava/lang/Class;") \
  template(get_name,                                  "get")                                      \
  template(put_name,                                  "put")                                      \
  template(type_name,                                 "type")                                     \
  template(findNative_name,                           "findNative")                               \
  template(initializeTenantContainerClass_name,       "initializeTenantContainerClass")           \
  template(initializeJGroupClass_name,                "initializeJGroupClass")                    \
  template(deadChild_name,                            "deadChild")                                \
  template(addClass_name,                             "addClass")                                 \
  template(throwIllegalAccessError_name,              "throwIllegalAccessError")                  \
  template(getFromClass_name,                         "getFromClass")                             \
  template(dispatch_name,                             "dispatch")                                 \
  template(getSystemClassLoader_name,                 "getSystemClassLoader")                     \
  template(fillInStackTrace_name,                     "fillInStackTrace")                         \
  template(fillInStackTrace0_name,                    "fillInStackTrace0")                        \
  template(getCause_name,                             "getCause")                                 \
  template(initCause_name,                            "initCause")                                \
  template(setProperty_name,                          "setProperty")                              \
  template(getProperty_name,                          "getProperty")                              \
  template(context_name,                              "context")                                  \
  template(privilegedContext_name,                    "privilegedContext")                        \
  template(contextClassLoader_name,                   "contextClassLoader")                       \
  template(inheritedAccessControlContext_name,        "inheritedAccessControlContext")            \
  template(isPrivileged_name,                         "isPrivileged")                             \
  template(isAuthorized_name,                         "isAuthorized")                             \
  template(getClassContext_name,                      "getClassContext")                          \
  template(wait_name,                                 "wait")                                     \
  template(checkPackageAccess_name,                   "checkPackageAccess")                       \
  template(stackSize_name,                            "stackSize")                                \
  template(thread_id_name,                            "tid")                                      \
  template(newInstance0_name,                         "newInstance0")                             \
  template(limit_name,                                "limit")                                    \
  template(member_name,                               "member")                                   \
  template(forName_name,                              "forName")                                  \
  template(forName0_name,                             "forName0")                                 \
  template(isJavaIdentifierStart_name,                "isJavaIdentifierStart")                    \
  template(isJavaIdentifierPart_name,                 "isJavaIdentifierPart")                     \
  template(exclusive_owner_thread_name,               "exclusiveOwnerThread")                     \
  template(park_blocker_name,                         "parkBlocker")                              \
  template(park_event_name,                           "nativeParkEventPointer")                   \
  template(cache_field_name,                          "cache")                                    \
  template(value_name,                                "value")                                    \
  template(offset_name,                               "offset")                                   \
  template(count_name,                                "count")                                    \
  template(hash_name,                                 "hash")                                     \
  template(numberOfLeadingZeros_name,                 "numberOfLeadingZeros")                     \
  template(numberOfTrailingZeros_name,                "numberOfTrailingZeros")                    \
  template(bitCount_name,                             "bitCount")                                 \
  template(profile_name,                              "profile")                                  \
  template(equals_name,                               "equals")                                   \
  template(target_name,                               "target")                                   \
  template(toString_name,                             "toString")                                 \
  template(values_name,                               "values")                                   \
  template(receiver_name,                             "receiver")                                 \
  template(vmtarget_name,                             "vmtarget")                                 \
  template(next_target_name,                          "next_target")                              \
  template(vmloader_name,                             "vmloader")                                 \
  template(vmindex_name,                              "vmindex")                                  \
  template(vmcount_name,                              "vmcount")                                  \
  template(vmentry_name,                              "vmentry")                                  \
  template(flags_name,                                "flags")                                    \
  template(rtype_name,                                "rtype")                                    \
  template(ptypes_name,                               "ptypes")                                   \
  template(form_name,                                 "form")                                     \
  template(basicType_name,                            "basicType")                                \
  template(append_name,                               "append")                                   \
  template(klass_name,                                "klass")                                    \
  template(array_klass_name,                          "array_klass")                              \
  template(oop_size_name,                             "oop_size")                                 \
  template(static_oop_field_count_name,               "static_oop_field_count")                   \
  template(protection_domain_name,                    "protection_domain")                        \
  template(init_lock_name,                            "init_lock")                                \
  template(signers_name,                              "signers_name")                             \
  template(loader_data_name,                          "loader_data")                              \
  template(dependencies_name,                         "dependencies")                             \
  template(com_alibaba_jwarmup_JWarmUp,               "com/alibaba/jwarmup/JWarmUp")              \
  template(jwarmup_notify_application_startup_is_done_name, "notifyApplicationStartUpIsDone")     \
  template(jwarmup_check_if_compilation_is_complete_name, "checkIfCompilationIsComplete")         \
  template(jwarmup_notify_jvm_deopt_warmup_methods_name,  "notifyJVMDeoptWarmUpMethods")          \
  template(jwarmup_dummy_name,                        "dummy")                                    \
  template(input_stream_void_signature,               "(Ljava/io/InputStream;)V")                 \
  template(getFileURL_name,                           "getFileURL")                               \
  template(getFileURL_signature,                      "(Ljava/io/File;)Ljava/net/URL;")           \
  template(definePackageInternal_name,                "definePackageInternal")                    \
  template(definePackageInternal_signature,           "(Ljava/lang/String;Ljava/util/jar/Manifest;Ljava/net/URL;)V") \
  template(addPackageInfo_name,                       "addPackageInfo")                           \
  template(addPackageInfo_signature,                  "(Ljava/lang/ClassLoader;Ljava/lang/String;Ljava/lang/String;)Ljava/security/ProtectionDomain;") \
  template(registerAsCDSLoader_name,                  "registerAsCDSLoader")                      \
  template(classloader_string_void_signature,         "(Ljava/lang/ClassLoader;Ljava/lang/String;)V") \
  template(getProtectionDomain_name,                  "getProtectionDomain")                      \
  template(getProtectionDomain_signature,             "(Ljava/security/CodeSource;)Ljava/security/ProtectionDomain;") \
  template(url_code_signer_array_void_signature,      "(Ljava/net/URL;[Ljava/security/CodeSigner;)V") \
  template(resolved_references_name,                  "<resolved_references>")                    \
  template(referencequeue_null_name,                  "NULL")                                     \
  template(referencequeue_enqueued_name,              "ENQUEUED")                                 \
                                                                                                  \
  /* non-intrinsic name/signature pairs: */                                                       \
  template(register_method_name,                      "register")                                 \
  do_alias(register_method_signature,         object_void_signature)                              \
                                                                                                  \
  /* name symbols needed by intrinsics */                                                         \
  VM_INTRINSICS_DO(VM_INTRINSIC_IGNORE, VM_SYMBOL_IGNORE, template, VM_SYMBOL_IGNORE, VM_ALIAS_IGNORE) \
                                                                                                  \
  /* common signatures names */                                                                   \
  template(void_method_signature,                     "()V")                                      \
  template(void_boolean_signature,                    "()Z")                                      \
  template(void_byte_signature,                       "()B")                                      \
  template(void_char_signature,                       "()C")                                      \
  template(void_short_signature,                      "()S")                                      \
  template(void_int_signature,                        "()I")                                      \
  template(void_long_signature,                       "()J")                                      \
  template(void_float_signature,                      "()F")                                      \
  template(void_double_signature,                     "()D")                                      \
  template(int_void_signature,                        "(I)V")                                     \
  template(long_void_signature,                       "(J)V")                                     \
  template(int_int_signature,                         "(I)I")                                     \
  template(char_char_signature,                       "(C)C")                                     \
  template(short_short_signature,                     "(S)S")                                     \
  template(int_bool_signature,                        "(I)Z")                                     \
  template(float_int_signature,                       "(F)I")                                     \
  template(double_long_signature,                     "(D)J")                                     \
  template(double_double_signature,                   "(D)D")                                     \
  template(int_float_signature,                       "(I)F")                                     \
  template(long_int_signature,                        "(J)I")                                     \
  template(long_long_signature,                       "(J)J")                                     \
  template(long_double_signature,                     "(J)D")                                     \
  template(byte_signature,                            "B")                                        \
  template(char_signature,                            "C")                                        \
  template(double_signature,                          "D")                                        \
  template(float_signature,                           "F")                                        \
  template(int_signature,                             "I")                                        \
  template(long_signature,                            "J")                                        \
  template(short_signature,                           "S")                                        \
  template(bool_signature,                            "Z")                                        \
  template(void_signature,                            "V")                                        \
  template(byte_array_signature,                      "[B")                                       \
  template(char_array_signature,                      "[C")                                       \
  template(int_array_signature,                       "[I")                                       \
  template(object_void_signature,                     "(Ljava/lang/Object;)V")                    \
  template(object_int_signature,                      "(Ljava/lang/Object;)I")                    \
  template(object_boolean_signature,                  "(Ljava/lang/Object;)Z")                    \
  template(string_void_signature,                     "(Ljava/lang/String;)V")                    \
  template(string_int_signature,                      "(Ljava/lang/String;)I")                    \
  template(runnable_void_signature,                   "(Ljava/lang/Runnable;)V")                  \
  template(throwable_void_signature,                  "(Ljava/lang/Throwable;)V")                 \
  template(void_throwable_signature,                  "()Ljava/lang/Throwable;")                  \
  template(throwable_throwable_signature,             "(Ljava/lang/Throwable;)Ljava/lang/Throwable;")             \
  template(class_void_signature,                      "(Ljava/lang/Class;)V")                     \
  template(class_int_signature,                       "(Ljava/lang/Class;)I")                     \
  template(class_long_signature,                      "(Ljava/lang/Class;)J")                     \
  template(class_boolean_signature,                   "(Ljava/lang/Class;)Z")                     \
  template(throwable_string_void_signature,           "(Ljava/lang/Throwable;Ljava/lang/String;)V")               \
  template(string_array_void_signature,               "([Ljava/lang/String;)V")                                   \
  template(string_array_string_array_void_signature,  "([Ljava/lang/String;[Ljava/lang/String;)V")                \
  template(thread_throwable_void_signature,           "(Ljava/lang/Thread;Ljava/lang/Throwable;)V")               \
  template(thread_void_signature,                     "(Ljava/lang/Thread;)V")                                    \
  template(threadgroup_runnable_void_signature,       "(Ljava/lang/ThreadGroup;Ljava/lang/Runnable;)V")           \
  template(threadgroup_string_void_signature,         "(Ljava/lang/ThreadGroup;Ljava/lang/String;)V")             \
  template(string_class_signature,                    "(Ljava/lang/String;)Ljava/lang/Class;")                    \
  template(object_object_object_signature,            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;") \
  template(string_string_string_signature,            "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;") \
  template(string_string_signature,                   "(Ljava/lang/String;)Ljava/lang/String;")                   \
  template(classloader_string_long_signature,         "(Ljava/lang/ClassLoader;Ljava/lang/String;)J")             \
  template(byte_array_void_signature,                 "([B)V")                                                    \
  template(char_array_void_signature,                 "([C)V")                                                    \
  template(int_int_void_signature,                    "(II)V")                                                    \
  template(long_long_void_signature,                  "(JJ)V")                                                    \
  template(void_classloader_signature,                "()Ljava/lang/ClassLoader;")                                \
  template(void_object_signature,                     "()Ljava/lang/Object;")                                     \
  template(void_class_signature,                      "()Ljava/lang/Class;")                                      \
  template(void_class_array_signature,                "()[Ljava/lang/Class;")                                     \
  template(void_string_signature,                     "()Ljava/lang/String;")                                     \
  template(object_array_object_signature,             "([Ljava/lang/Object;)Ljava/lang/Object;")                  \
  template(object_object_array_object_signature,      "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;")\
  template(exception_void_signature,                  "(Ljava/lang/Exception;)V")                                 \
  template(protectiondomain_signature,                "[Ljava/security/ProtectionDomain;")                        \
  template(accesscontrolcontext_signature,            "Ljava/security/AccessControlContext;")                     \
  template(tenantcontainer_signature,                 "Lcom/alibaba/tenant/TenantContainer;")                     \
  template(resourcecontainer_signature,               "Lcom/alibaba/rcm/internal/AbstractResourceContainer;")     \
  template(class_protectiondomain_signature,          "(Ljava/lang/Class;Ljava/security/ProtectionDomain;)V")     \
  template(thread_signature,                          "Ljava/lang/Thread;")                                       \
  template(thread_array_signature,                    "[Ljava/lang/Thread;")                                      \
  template(threadgroup_signature,                     "Ljava/lang/ThreadGroup;")                                  \
  template(threadgroup_array_signature,               "[Ljava/lang/ThreadGroup;")                                 \
  template(class_array_signature,                     "[Ljava/lang/Class;")                                       \
  template(classloader_signature,                     "Ljava/lang/ClassLoader;")                                  \
  template(object_signature,                          "Ljava/lang/Object;")                                       \
  template(state_name,                                 "state")                                                   \
  template(com_alibaba_tenant_TenantState_signature,   "Lcom/alibaba/tenant/TenantState;")                        \
  template(class_signature,                           "Ljava/lang/Class;")                                        \
  template(string_signature,                          "Ljava/lang/String;")                                       \
  template(reference_signature,                       "Ljava/lang/ref/Reference;")                                \
  template(referencequeue_signature,                  "Ljava/lang/ref/ReferenceQueue;")                           \
  template(executable_signature,                      "Ljava/lang/reflect/Executable;")                           \
  template(concurrenthashmap_signature,               "Ljava/util/concurrent/ConcurrentHashMap;")                 \
  template(String_StringBuilder_signature,            "(Ljava/lang/String;)Ljava/lang/StringBuilder;")            \
  template(int_StringBuilder_signature,               "(I)Ljava/lang/StringBuilder;")                             \
  template(char_StringBuilder_signature,              "(C)Ljava/lang/StringBuilder;")                             \
  template(String_StringBuffer_signature,             "(Ljava/lang/String;)Ljava/lang/StringBuffer;")             \
  template(int_StringBuffer_signature,                "(I)Ljava/lang/StringBuffer;")                              \
  template(char_StringBuffer_signature,               "(C)Ljava/lang/StringBuffer;")                              \
  template(int_String_signature,                      "(I)Ljava/lang/String;")                                    \
  template(codesource_permissioncollection_signature, "(Ljava/security/CodeSource;Ljava/security/PermissionCollection;)V") \
  /* signature symbols needed by intrinsics */                                                                    \
  VM_INTRINSICS_DO(VM_INTRINSIC_IGNORE, VM_SYMBOL_IGNORE, VM_SYMBOL_IGNORE, template, VM_ALIAS_IGNORE)            \
                                                                                                                  \
  /* symbol aliases needed by intrinsics */                                                                       \
  VM_INTRINSICS_DO(VM_INTRINSIC_IGNORE, VM_SYMBOL_IGNORE, VM_SYMBOL_IGNORE, VM_SYMBOL_IGNORE, do_alias)           \
                                                                                                                  \
  /* returned by the C1 compiler in case there's not enough memory to allocate a new symbol*/                     \
  template(dummy_symbol,                              "illegal symbol")                                           \
                                                                                                                  \
  /* used by ClassFormatError when class name is not known yet */                                                 \
  template(unknown_class_name,                        "<Unknown>")                                                \
                                                                                                                  \
  /* used to identify class loaders handling parallel class loading */                                            \
  template(parallelCapable_name,                      "parallelLockMap")                                          \
                                                                                                                  \
  template(signature_name,                             "signature")                                               \
  /* JVM monitoring and management support */                                                                     \
  template(java_lang_StackTraceElement_array,          "[Ljava/lang/StackTraceElement;")                          \
  template(java_lang_management_ThreadState,           "java/lang/management/ThreadState")                        \
  template(java_lang_management_MemoryUsage,           "java/lang/management/MemoryUsage")                        \
  template(java_lang_management_ThreadInfo,            "java/lang/management/ThreadInfo")                         \
  template(sun_management_ManagementFactory,           "sun/management/ManagementFactory")                        \
  template(sun_management_Sensor,                      "sun/management/Sensor")                                   \
  template(sun_management_Agent,                       "sun/management/Agent")                                    \
  template(sun_management_DiagnosticCommandImpl,       "sun/management/DiagnosticCommandImpl")                    \
  template(sun_management_GarbageCollectorImpl,        "sun/management/GarbageCollectorImpl")                     \
  template(sun_management_ManagementFactoryHelper,     "sun/management/ManagementFactoryHelper")                  \
  template(getDiagnosticCommandMBean_name,             "getDiagnosticCommandMBean")                               \
  template(getDiagnosticCommandMBean_signature,        "()Lcom/sun/management/DiagnosticCommandMBean;")           \
  template(getGcInfoBuilder_name,                      "getGcInfoBuilder")                                        \
  template(getGcInfoBuilder_signature,                 "()Lsun/management/GcInfoBuilder;")                        \
  template(com_sun_management_GcInfo,                  "com/sun/management/GcInfo")                               \
  template(com_sun_management_GcInfo_constructor_signature, "(Lsun/management/GcInfoBuilder;JJJ[Ljava/lang/management/MemoryUsage;[Ljava/lang/management/MemoryUsage;[Ljava/lang/Object;)V") \
  template(createGCNotification_name,                  "createGCNotification")                                    \
  template(createGCNotification_signature,             "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Lcom/sun/management/GcInfo;)V") \
  template(createDiagnosticFrameworkNotification_name, "createDiagnosticFrameworkNotification")                   \
  template(createMemoryPoolMBean_name,                 "createMemoryPoolMBean")                                   \
  template(createMemoryManagerMBean_name,              "createMemoryManagerMBean")                                \
  template(createGarbageCollectorMBean_name,           "createGarbageCollectorMBean")                             \
  template(createMemoryPoolMBean_signature,            "(Ljava/lang/String;ZJJ)Ljava/lang/management/MemoryPoolMBean;") \
  template(createMemoryManagerMBean_signature,         "(Ljava/lang/String;)Ljava/lang/management/MemoryManagerMBean;") \
  template(createGarbageCollectorMBean_signature,      "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/management/GarbageCollectorMBean;") \
  template(trigger_name,                               "trigger")                                                 \
  template(clear_name,                                 "clear")                                                   \
  template(trigger_method_signature,                   "(ILjava/lang/management/MemoryUsage;)V")                                                 \
  template(startAgent_name,                            "startAgent")                                              \
  template(startRemoteAgent_name,                      "startRemoteManagementAgent")                              \
  template(startLocalAgent_name,                       "startLocalManagementAgent")                               \
  template(stopRemoteAgent_name,                       "stopRemoteManagementAgent")                               \
  template(java_lang_management_ThreadInfo_constructor_signature, "(Ljava/lang/Thread;ILjava/lang/Object;Ljava/lang/Thread;JJJJ[Ljava/lang/StackTraceElement;)V") \
  template(java_lang_management_ThreadInfo_with_locks_constructor_signature, "(Ljava/lang/Thread;ILjava/lang/Object;Ljava/lang/Thread;JJJJ[Ljava/lang/StackTraceElement;[Ljava/lang/Object;[I[Ljava/lang/Object;)V") \
  template(long_long_long_long_void_signature,         "(JJJJ)V")                                                 \
  template(finalizer_histogram_klass,                  "java/lang/ref/FinalizerHistogram")                        \
  template(void_finalizer_histogram_entry_array_signature,  "()[Ljava/lang/ref/FinalizerHistogram$Entry;")                        \
  template(get_finalizer_histogram_name,               "getFinalizerHistogram")                                   \
  template(finalizer_histogram_entry_name_field,       "className")                                               \
  template(finalizer_histogram_entry_count_field,      "instanceCount")                                           \
                                                                                                                  \
  template(java_lang_management_MemoryPoolMXBean,      "java/lang/management/MemoryPoolMXBean")                   \
  template(java_lang_management_MemoryManagerMXBean,   "java/lang/management/MemoryManagerMXBean")                \
  template(java_lang_management_GarbageCollectorMXBean,"java/lang/management/GarbageCollectorMXBean")             \
  template(gcInfoBuilder_name,                         "gcInfoBuilder")                                           \
  template(createMemoryPool_name,                      "createMemoryPool")                                        \
  template(createMemoryManager_name,                   "createMemoryManager")                                     \
  template(createGarbageCollector_name,                "createGarbageCollector")                                  \
  template(createMemoryPool_signature,                 "(Ljava/lang/String;ZJJ)Ljava/lang/management/MemoryPoolMXBean;") \
  template(createMemoryManager_signature,              "(Ljava/lang/String;)Ljava/lang/management/MemoryManagerMXBean;") \
  template(createGarbageCollector_signature,           "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/management/GarbageCollectorMXBean;") \
  template(addThreadDumpForMonitors_name,              "addThreadDumpForMonitors")                                \
  template(addThreadDumpForSynchronizers_name,         "addThreadDumpForSynchronizers")                           \
  template(addThreadDumpForMonitors_signature,         "(Ljava/lang/management/ThreadInfo;[Ljava/lang/Object;[I)V") \
  template(addThreadDumpForSynchronizers_signature,    "(Ljava/lang/management/ThreadInfo;[Ljava/lang/Object;)V")   \
                                                                                                                  \
  /* JVMTI/java.lang.instrument support and VM Attach mechanism */                                                \
  template(sun_misc_VMSupport,                         "sun/misc/VMSupport")                                      \
  template(appendToClassPathForInstrumentation_name,   "appendToClassPathForInstrumentation")                     \
  do_alias(appendToClassPathForInstrumentation_signature, string_void_signature)                                  \
  template(serializePropertiesToByteArray_name,        "serializePropertiesToByteArray")                          \
  template(serializePropertiesToByteArray_signature,   "()[B")                                                    \
  template(serializeAgentPropertiesToByteArray_name,   "serializeAgentPropertiesToByteArray")                     \
  template(classRedefinedCount_name,                   "classRedefinedCount")                                     \
  template(classLoader_name,                           "classLoader")                                             \
  template(is_dead_name,                               "isDead")                                                  \
                                                                                                                  \
  /* jfr signatures */                                                                                            \
  JFR_TEMPLATES(template)                                                                                         \
  /* CDS support */                                                                                               \
  template(isNotFound_name,                            "isNotFound")                                              \
  template(isNotFound_signature,                       "(Ljava/lang/String;I)Z")                                  \
  /* coroutine support */                                                                                         \
  template(java_dyn_CoroutineSupport,                  "java/dyn/CoroutineSupport")                               \
  template(java_dyn_CoroutineBase,                     "java/dyn/CoroutineBase")                                  \
  template(java_dyn_CoroutineExitException,            "java/dyn/CoroutineExitException")                         \
  template(com_alibaba_wisp_engine_WispTask,           "com/alibaba/wisp/engine/WispTask")                        \
  template(com_alibaba_wisp_engine_WispTask_CacheableCoroutine,                                                   \
                                                       "com/alibaba/wisp/engine/WispTask$CacheableCoroutine")     \
  template(com_alibaba_wisp_engine_WispControlGroup_signature,                                                    \
                                                       "Lcom/alibaba/wisp/engine/WispControlGroup;")              \
  template(com_alibaba_wisp_engine_WispControlGroup_CpuLimit_signature,                                           \
                                                       "Lcom/alibaba/wisp/engine/WispControlGroup$CpuLimit;")     \
  template(com_alibaba_rcm_internal_AbstractResourceContainer_signature,                                          \
                                                       "com/alibaba/rcm/internal/AbstractResourceContainer")      \
  template(com_alibaba_wisp_engine_WispEngine,         "com/alibaba/wisp/engine/WispEngine")                      \
  template(com_alibaba_wisp_engine_WispCarrier,        "com/alibaba/wisp/engine/WispCarrier")                     \
  template(com_alibaba_wisp_engine_WispEventPump,      "com/alibaba/wisp/engine/WispEventPump")                   \
  template(com_alibaba_util_QuickStart,                "com/alibaba/util/QuickStart")                             \
  template(com_alibaba_util_CDSDumpHook,               "com/alibaba/util/CDSDumpHook")                            \
  template(dumpCDSInfo_name,                           "dumpCDSInfo")                                             \
  template(notifyDump_name,                            "notifyDump")                                              \
  template(bool_string_void_signature,                 "(ZLjava/lang/String;)V")                                  \
  template(int_string_bool_stringarray_string_void_signature,            "(ILjava/lang/String;Z[Ljava/lang/String;Ljava/lang/String;)V")                                 \
  template(string_string_string_string_bool_void_signature,   "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)V") \
  template(isInCritical_name,                          "isInCritical")                                            \
  template(jdkParkStatus_name,                         "jdkParkStatus")                                           \
  template(jvmParkStatus_name,                         "jvmParkStatus")                                           \
  template(ttr_name,                                   "ttr")                                                     \
  template(controlGroup_name,                          "controlGroup")                                            \
  template(cpuLimit_name,                              "cpuLimit")                                                \
  template(cfsPeriod_name,                             "cfsPeriod")                                               \
  template(cfsQuota_name,                              "cfsQuota")                                                \
  template(id_name,                                    "id")                                                      \
  template(threadWrapper_name,                         "threadWrapper")                                           \
  template(activeCount_name,                           "activeCount")                                             \
  template(stealCount_name,                            "stealCount")                                              \
  template(stealFailureCount_name,                     "stealFailureCount")                                       \
  template(preemptCount_name,                          "preemptCount")                                            \
  template(unparkById_name,                            "unparkById")                                              \
  template(interruptById_name,                         "interruptById")                                           \
  template(interrupted_name,                           "interrupted")                                             \
  template(yield_name,                                 "yield")                                                   \
  template(runOutsideWisp_name,                        "runOutsideWisp")                                          \
  template(nativeCoroutine_name,                       "nativeCoroutine")                                         \
  template(stack_name,                                 "stack")                                                   \
  template(current_name,                               "current")                                                 \
  template(java_dyn_CoroutineBase_signature,           "Ljava/dyn/CoroutineBase;")                                \
  template(reflect_method_signature,                   "Ljava/lang/reflect/Method;")                              \
  template(startInternal_method_name,                  "startInternal")                                           \
  template(initializeCoroutineSupport_method_name,     "initializeCoroutineSupport")                              \
  template(destroyCoroutineSupport_method_name,        "destroyCoroutineSupport")                                 \
  template(method_name,                                "method")                                                  \
  template(bci_name,                                   "bci")                                                     \
  template(localCount_name,                            "localCount")                                              \
  template(expressionCount_name,                       "expressionCount")                                         \
  template(scalarValues_name,                          "scalarValues")                                            \
  template(objectValues_name,                          "objectValues")                                            \
  template(long_array_signature,                       "[J")                                                      \
  template(object_array_signature,                     "[Ljava/lang/Object;")                                     \
                                                                                                                  \
  /* coroutine work steal support */                                                                              \
  template(java_security_AccessController,             "java/security/AccessController")                          \
  template(doPrivileged_name,                          "doPrivileged")                                            \
  template(doPrivileged_signature_1,                   "(Ljava/security/PrivilegedAction;)Ljava/lang/Object;")    \
  template(doPrivileged_signature_2,                   "(Ljava/security/PrivilegedAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;")    \
  template(doPrivileged_signature_3,                   "(Ljava/security/PrivilegedExceptionAction;)Ljava/lang/Object;")    \
  template(doPrivileged_signature_4,                   "(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;")    \
  template(sun_reflect_NativeMethodAccessorImpl,       "sun/reflect/NativeMethodAccessorImpl")                    \
  template(invoke0_name,                               "invoke0")                                                 \
  template(invoke0_signature,                          "(Ljava/lang/reflect/Method;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;")  \
  template(sun_reflect_NativeConstructorAccessorImpl,  "sun/reflect/NativeConstructorAccessorImpl")               \
  template(newInstance0_signature,                     "(Ljava/lang/reflect/Constructor;[Ljava/lang/Object;)Ljava/lang/Object;")  \
                                                                                                                  \
  /*end*/

// Here are all the intrinsics known to the runtime and the CI.
// Each intrinsic consists of a public enum name (like _hashCode),
// followed by a specification of its klass, name, and signature:
//    template(<id>,  <klass>,  <name>, <sig>, <FCODE>)
//
// If you add an intrinsic here, you must also define its name
// and signature as members of the VM symbols.  The VM symbols for
// the intrinsic name and signature may be defined above.
//
// Because the VM_SYMBOLS_DO macro makes reference to VM_INTRINSICS_DO,
// you can also define an intrinsic's name and/or signature locally to the
// intrinsic, if this makes sense.  (It often does make sense.)
//
// For example:
//    do_intrinsic(_foo,  java_lang_Object,  foo_name, foo_signature, F_xx)
//     do_name(     foo_name, "foo")
//     do_signature(foo_signature, "()F")
// klass      = vmSymbols::java_lang_Object()
// name       = vmSymbols::foo_name()
// signature  = vmSymbols::foo_signature()
//
// The name and/or signature might be a "well known" symbol
// like "equal" or "()I", in which case there will be no local
// re-definition of the symbol.
//
// The do_class, do_name, and do_signature calls are all used for the
// same purpose:  Define yet another VM symbol.  They could all be merged
// into a common 'do_symbol' call, but it seems useful to record our
// intentions here about kinds of symbols (class vs. name vs. signature).
//
// The F_xx is one of the Flags enum; see below.
//
// for Emacs: (let ((c-backslash-column 120) (c-backslash-max-column 120)) (c-backslash-region (point) (point-max) nil t))
#define VM_INTRINSICS_DO(do_intrinsic, do_class, do_name, do_signature, do_alias)                                       \
  do_intrinsic(_hashCode,                 java_lang_Object,       hashCode_name, void_int_signature,             F_R)   \
   do_name(     hashCode_name,                                   "hashCode")                                            \
  do_intrinsic(_getClass,                 java_lang_Object,       getClass_name, void_class_signature,           F_R)   \
   do_name(     getClass_name,                                   "getClass")                                            \
  do_intrinsic(_clone,                    java_lang_Object,       clone_name, void_object_signature,             F_R)   \
   do_name(     clone_name,                                      "clone")                                               \
                                                                                                                        \
  /* Math & StrictMath intrinsics are defined in terms of just a few signatures: */                                     \
  do_class(java_lang_Math,                "java/lang/Math")                                                             \
  do_class(java_lang_StrictMath,          "java/lang/StrictMath")                                                       \
  do_signature(double2_double_signature,  "(DD)D")                                                                      \
  do_signature(int2_int_signature,        "(II)I")                                                                      \
  do_signature(long2_long_signature,      "(JJ)J")                                                                         \
                                                                                                                        \
  /* here are the math names, all together: */                                                                          \
  do_name(abs_name,"abs")       do_name(sin_name,"sin")         do_name(cos_name,"cos")                                 \
  do_name(tan_name,"tan")       do_name(atan2_name,"atan2")     do_name(sqrt_name,"sqrt")                               \
  do_name(log_name,"log")       do_name(log10_name,"log10")     do_name(pow_name,"pow")                                 \
  do_name(exp_name,"exp")       do_name(min_name,"min")         do_name(max_name,"max")                                 \
                                                                                                                        \
  do_name(addExact_name,"addExact")                                                                                     \
  do_name(decrementExact_name,"decrementExact")                                                                         \
  do_name(incrementExact_name,"incrementExact")                                                                         \
  do_name(multiplyExact_name,"multiplyExact")                                                                           \
  do_name(negateExact_name,"negateExact")                                                                               \
  do_name(subtractExact_name,"subtractExact")                                                                           \
                                                                                                                        \
  do_intrinsic(_dabs,                     java_lang_Math,         abs_name,   double_double_signature,           F_S)   \
  do_intrinsic(_dsin,                     java_lang_Math,         sin_name,   double_double_signature,           F_S)   \
  do_intrinsic(_dcos,                     java_lang_Math,         cos_name,   double_double_signature,           F_S)   \
  do_intrinsic(_dtan,                     java_lang_Math,         tan_name,   double_double_signature,           F_S)   \
  do_intrinsic(_datan2,                   java_lang_Math,         atan2_name, double2_double_signature,          F_S)   \
  do_intrinsic(_dsqrt,                    java_lang_Math,         sqrt_name,  double_double_signature,           F_S)   \
  do_intrinsic(_dlog,                     java_lang_Math,         log_name,   double_double_signature,           F_S)   \
  do_intrinsic(_dlog10,                   java_lang_Math,         log10_name, double_double_signature,           F_S)   \
  do_intrinsic(_dpow,                     java_lang_Math,         pow_name,   double2_double_signature,          F_S)   \
  do_intrinsic(_dexp,                     java_lang_Math,         exp_name,   double_double_signature,           F_S)   \
  do_intrinsic(_min,                      java_lang_Math,         min_name,   int2_int_signature,                F_S)   \
  do_intrinsic(_max,                      java_lang_Math,         max_name,   int2_int_signature,                F_S)   \
  do_intrinsic(_addExactI,                java_lang_Math,         addExact_name, int2_int_signature,             F_S)   \
  do_intrinsic(_addExactL,                java_lang_Math,         addExact_name, long2_long_signature,           F_S)   \
  do_intrinsic(_decrementExactI,          java_lang_Math,         decrementExact_name, int_int_signature,        F_S)   \
  do_intrinsic(_decrementExactL,          java_lang_Math,         decrementExact_name, long_long_signature,      F_S)   \
  do_intrinsic(_incrementExactI,          java_lang_Math,         incrementExact_name, int_int_signature,        F_S)   \
  do_intrinsic(_incrementExactL,          java_lang_Math,         incrementExact_name, long_long_signature,      F_S)   \
  do_intrinsic(_multiplyExactI,           java_lang_Math,         multiplyExact_name, int2_int_signature,        F_S)   \
  do_intrinsic(_multiplyExactL,           java_lang_Math,         multiplyExact_name, long2_long_signature,      F_S)   \
  do_intrinsic(_negateExactI,             java_lang_Math,         negateExact_name, int_int_signature,           F_S)   \
  do_intrinsic(_negateExactL,             java_lang_Math,         negateExact_name, long_long_signature,         F_S)   \
  do_intrinsic(_subtractExactI,           java_lang_Math,         subtractExact_name, int2_int_signature,        F_S)   \
  do_intrinsic(_subtractExactL,           java_lang_Math,         subtractExact_name, long2_long_signature,      F_S)   \
                                                                                                                        \
  do_intrinsic(_floatToRawIntBits,        java_lang_Float,        floatToRawIntBits_name,   float_int_signature, F_S)   \
   do_name(     floatToRawIntBits_name,                          "floatToRawIntBits")                                   \
  do_intrinsic(_floatToIntBits,           java_lang_Float,        floatToIntBits_name,      float_int_signature, F_S)   \
   do_name(     floatToIntBits_name,                             "floatToIntBits")                                      \
  do_intrinsic(_intBitsToFloat,           java_lang_Float,        intBitsToFloat_name,      int_float_signature, F_S)   \
   do_name(     intBitsToFloat_name,                             "intBitsToFloat")                                      \
  do_intrinsic(_doubleToRawLongBits,      java_lang_Double,       doubleToRawLongBits_name, double_long_signature, F_S) \
   do_name(     doubleToRawLongBits_name,                        "doubleToRawLongBits")                                 \
  do_intrinsic(_doubleToLongBits,         java_lang_Double,       doubleToLongBits_name,    double_long_signature, F_S) \
   do_name(     doubleToLongBits_name,                           "doubleToLongBits")                                    \
  do_intrinsic(_longBitsToDouble,         java_lang_Double,       longBitsToDouble_name,    long_double_signature, F_S) \
   do_name(     longBitsToDouble_name,                           "longBitsToDouble")                                    \
                                                                                                                        \
  do_intrinsic(_numberOfLeadingZeros_i,   java_lang_Integer,      numberOfLeadingZeros_name,int_int_signature,   F_S)   \
  do_intrinsic(_numberOfLeadingZeros_l,   java_lang_Long,         numberOfLeadingZeros_name,long_int_signature,  F_S)   \
                                                                                                                        \
  do_intrinsic(_numberOfTrailingZeros_i,  java_lang_Integer,      numberOfTrailingZeros_name,int_int_signature,  F_S)   \
  do_intrinsic(_numberOfTrailingZeros_l,  java_lang_Long,         numberOfTrailingZeros_name,long_int_signature, F_S)   \
                                                                                                                        \
  do_intrinsic(_bitCount_i,               java_lang_Integer,      bitCount_name,            int_int_signature,   F_S)   \
  do_intrinsic(_bitCount_l,               java_lang_Long,         bitCount_name,            long_int_signature,  F_S)   \
                                                                                                                        \
  do_intrinsic(_reverseBytes_i,           java_lang_Integer,      reverseBytes_name,        int_int_signature,   F_S)   \
   do_name(     reverseBytes_name,                               "reverseBytes")                                        \
  do_intrinsic(_reverseBytes_l,           java_lang_Long,         reverseBytes_name,        long_long_signature, F_S)   \
    /*  (symbol reverseBytes_name defined above) */                                                                     \
  do_intrinsic(_reverseBytes_c,           java_lang_Character,    reverseBytes_name,        char_char_signature, F_S)   \
    /*  (symbol reverseBytes_name defined above) */                                                                     \
  do_intrinsic(_reverseBytes_s,           java_lang_Short,        reverseBytes_name,        short_short_signature, F_S) \
    /*  (symbol reverseBytes_name defined above) */                                                                     \
                                                                                                                        \
  do_intrinsic(_identityHashCode,         java_lang_System,       identityHashCode_name, object_int_signature,   F_S)   \
   do_name(     identityHashCode_name,                           "identityHashCode")                                    \
  do_intrinsic(_currentTimeMillis,        java_lang_System,       currentTimeMillis_name, void_long_signature,   F_S)   \
                                                                                                                        \
   do_name(     currentTimeMillis_name,                          "currentTimeMillis")                                   \
  do_intrinsic(_nanoTime,                 java_lang_System,       nanoTime_name,          void_long_signature,   F_S)   \
   do_name(     nanoTime_name,                                   "nanoTime")                                            \
                                                                                                                        \
  JFR_INTRINSICS(do_intrinsic, do_class, do_name, do_signature, do_alias)                                               \
                                                                                                                        \
  do_intrinsic(_arraycopy,                java_lang_System,       arraycopy_name, arraycopy_signature,           F_S)   \
   do_name(     arraycopy_name,                                  "arraycopy")                                           \
   do_signature(arraycopy_signature,                             "(Ljava/lang/Object;ILjava/lang/Object;II)V")          \
  do_intrinsic(_isInterrupted,            java_lang_Thread,       isInterrupted_name, isInterrupted_signature,   F_R)   \
   do_name(     isInterrupted_name,                              "isInterrupted")                                       \
   do_signature(isInterrupted_signature,                         "(Z)Z")                                                \
  do_intrinsic(_currentThread,            java_lang_Thread,       currentThread_name, currentThread_signature,   F_S)   \
   do_name(     currentThread_name,                              "currentThread0")                                      \
   do_signature(currentThread_signature,                         "()Ljava/lang/Thread;")                                \
                                                                                                                        \
  /* reflective intrinsics, for java/lang/Class, etc. */                                                                \
  do_intrinsic(_isAssignableFrom,         java_lang_Class,        isAssignableFrom_name, class_boolean_signature, F_RN) \
   do_name(     isAssignableFrom_name,                           "isAssignableFrom")                                    \
  do_intrinsic(_isInstance,               java_lang_Class,        isInstance_name, object_boolean_signature,     F_RN)  \
   do_name(     isInstance_name,                                 "isInstance")                                          \
  do_intrinsic(_getModifiers,             java_lang_Class,        getModifiers_name, void_int_signature,         F_RN)  \
   do_name(     getModifiers_name,                               "getModifiers")                                        \
  do_intrinsic(_isInterface,              java_lang_Class,        isInterface_name, void_boolean_signature,      F_RN)  \
   do_name(     isInterface_name,                                "isInterface")                                         \
  do_intrinsic(_isArray,                  java_lang_Class,        isArray_name, void_boolean_signature,          F_RN)  \
   do_name(     isArray_name,                                    "isArray")                                             \
  do_intrinsic(_isPrimitive,              java_lang_Class,        isPrimitive_name, void_boolean_signature,      F_RN)  \
   do_name(     isPrimitive_name,                                "isPrimitive")                                         \
  do_intrinsic(_getSuperclass,            java_lang_Class,        getSuperclass_name, void_class_signature,      F_RN)  \
   do_name(     getSuperclass_name,                              "getSuperclass")                                       \
  do_intrinsic(_getComponentType,         java_lang_Class,        getComponentType_name, void_class_signature,   F_RN)  \
   do_name(     getComponentType_name,                           "getComponentType")                                    \
                                                                                                                        \
  do_intrinsic(_getClassAccessFlags,      sun_reflect_Reflection, getClassAccessFlags_name, class_int_signature, F_SN)  \
   do_name(     getClassAccessFlags_name,                        "getClassAccessFlags")                                 \
  do_intrinsic(_getLength,                java_lang_reflect_Array, getLength_name, object_int_signature,         F_SN)  \
   do_name(     getLength_name,                                   "getLength")                                          \
                                                                                                                        \
  do_intrinsic(_getCallerClass,           sun_reflect_Reflection, getCallerClass_name, void_class_signature,     F_SN)  \
   do_name(     getCallerClass_name,                             "getCallerClass")                                      \
                                                                                                                        \
  do_intrinsic(_newArray,                 java_lang_reflect_Array, newArray_name, newArray_signature,            F_SN)  \
   do_name(     newArray_name,                                    "newArray")                                           \
   do_signature(newArray_signature,                               "(Ljava/lang/Class;I)Ljava/lang/Object;")             \
                                                                                                                        \
  do_intrinsic(_copyOf,                   java_util_Arrays,       copyOf_name, copyOf_signature,                 F_S)   \
   do_name(     copyOf_name,                                     "copyOf")                                              \
   do_signature(copyOf_signature,             "([Ljava/lang/Object;ILjava/lang/Class;)[Ljava/lang/Object;")             \
                                                                                                                        \
  do_intrinsic(_copyOfRange,              java_util_Arrays,       copyOfRange_name, copyOfRange_signature,       F_S)   \
   do_name(     copyOfRange_name,                                "copyOfRange")                                         \
   do_signature(copyOfRange_signature,        "([Ljava/lang/Object;IILjava/lang/Class;)[Ljava/lang/Object;")            \
                                                                                                                        \
  do_intrinsic(_equalsC,                  java_util_Arrays,       equals_name,    equalsC_signature,             F_S)   \
   do_signature(equalsC_signature,                               "([C[C)Z")                                             \
                                                                                                                        \
  do_intrinsic(_compareTo,                java_lang_String,       compareTo_name, string_int_signature,          F_R)   \
   do_name(     compareTo_name,                                  "compareTo")                                           \
  do_intrinsic(_indexOf,                  java_lang_String,       indexOf_name, string_int_signature,            F_R)   \
   do_name(     indexOf_name,                                    "indexOf")                                             \
  do_intrinsic(_equals,                   java_lang_String,       equals_name, object_boolean_signature,         F_R)   \
                                                                                                                        \
  do_class(java_nio_Buffer,               "java/nio/Buffer")                                                            \
  do_intrinsic(_checkIndex,               java_nio_Buffer,        checkIndex_name, int_int_signature,            F_R)   \
   do_name(     checkIndex_name,                                 "checkIndex")                                          \
                                                                                                                        \
  do_class(sun_nio_cs_iso8859_1_Encoder,  "sun/nio/cs/ISO_8859_1$Encoder")                                              \
  do_intrinsic(_encodeISOArray,     sun_nio_cs_iso8859_1_Encoder, encodeISOArray_name, encodeISOArray_signature, F_S)   \
   do_name(     encodeISOArray_name,                             "encodeISOArray")                                      \
   do_signature(encodeISOArray_signature,                        "([CI[BII)I")                                          \
                                                                                                                        \
  do_class(java_math_BigInteger,                      "java/math/BigInteger")                                           \
  do_intrinsic(_multiplyToLen,      java_math_BigInteger, multiplyToLen_name, multiplyToLen_signature, F_S)             \
   do_name(     multiplyToLen_name,                             "multiplyToLen")                                        \
   do_signature(multiplyToLen_signature,                        "([II[II[I)[I")                                         \
                                                                                                                        \
  do_intrinsic(_squareToLen, java_math_BigInteger, squareToLen_name, squareToLen_signature, F_S)                        \
   do_name(     squareToLen_name,                             "implSquareToLen")                                        \
   do_signature(squareToLen_signature,                        "([II[II)[I")                                             \
                                                                                                                        \
  do_intrinsic(_mulAdd, java_math_BigInteger, mulAdd_name, mulAdd_signature, F_S)                                       \
   do_name(     mulAdd_name,                                  "implMulAdd")                                             \
   do_signature(mulAdd_signature,                             "([I[IIII)I")                                             \
                                                                                                                        \
  do_intrinsic(_montgomeryMultiply,      java_math_BigInteger, montgomeryMultiply_name, montgomeryMultiply_signature, F_S) \
   do_name(     montgomeryMultiply_name,                             "implMontgomeryMultiply")                          \
   do_signature(montgomeryMultiply_signature,                        "([I[I[IIJ[I)[I")                                  \
                                                                                                                        \
  do_intrinsic(_montgomerySquare,      java_math_BigInteger, montgomerySquare_name, montgomerySquare_signature, F_S)    \
   do_name(     montgomerySquare_name,                             "implMontgomerySquare")                              \
   do_signature(montgomerySquare_signature,                        "([I[IIJ[I)[I")                                      \
                                                                                                                        \
  /* java/lang/ref/Reference */                                                                                         \
  do_intrinsic(_Reference_get,            java_lang_ref_Reference, get_name,    void_object_signature, F_R)             \
                                                                                                                        \
  /* support for com.sun.crypto.provider.AESCrypt and some of its callers */                                            \
  do_class(com_sun_crypto_provider_aescrypt,      "com/sun/crypto/provider/AESCrypt")                                   \
  do_intrinsic(_aescrypt_encryptBlock, com_sun_crypto_provider_aescrypt, encryptBlock_name, byteArray_int_byteArray_int_signature, F_R)   \
  do_intrinsic(_aescrypt_decryptBlock, com_sun_crypto_provider_aescrypt, decryptBlock_name, byteArray_int_byteArray_int_signature, F_R)   \
   do_name(     encryptBlock_name,                                 "implEncryptBlock")                                  \
   do_name(     decryptBlock_name,                                 "implDecryptBlock")                                  \
   do_signature(byteArray_int_byteArray_int_signature,             "([BI[BI)V")                                         \
                                                                                                                        \
  do_class(com_sun_crypto_provider_cipherBlockChaining,            "com/sun/crypto/provider/CipherBlockChaining")       \
   do_intrinsic(_cipherBlockChaining_encryptAESCrypt, com_sun_crypto_provider_cipherBlockChaining, encrypt_name, byteArray_int_int_byteArray_int_signature, F_R)   \
   do_intrinsic(_cipherBlockChaining_decryptAESCrypt, com_sun_crypto_provider_cipherBlockChaining, decrypt_name, byteArray_int_int_byteArray_int_signature, F_R)   \
   do_name(     encrypt_name,                                      "implEncrypt")                                       \
   do_name(     decrypt_name,                                      "implDecrypt")                                       \
   do_signature(byteArray_int_int_byteArray_int_signature,         "([BII[BI)I")                                        \
                                                                                                                        \
  /* support for sun.security.provider.SHA */                                                                           \
  do_class(sun_security_provider_sha,                              "sun/security/provider/SHA")                         \
  do_intrinsic(_sha_implCompress, sun_security_provider_sha, implCompress_name, implCompress_signature, F_R)            \
   do_name(     implCompress_name,                                 "implCompress0")                                     \
   do_signature(implCompress_signature,                            "([BI)V")                                            \
                                                                                                                        \
  /* support for sun.security.provider.SHA2 */                                                                          \
  do_class(sun_security_provider_sha2,                             "sun/security/provider/SHA2")                        \
  do_intrinsic(_sha2_implCompress, sun_security_provider_sha2, implCompress_name, implCompress_signature, F_R)          \
                                                                                                                        \
  /* support for sun.security.provider.SHA5 */                                                                          \
  do_class(sun_security_provider_sha5,                             "sun/security/provider/SHA5")                        \
  do_intrinsic(_sha5_implCompress, sun_security_provider_sha5, implCompress_name, implCompress_signature, F_R)          \
                                                                                                                        \
  /* support for sun.security.provider.DigestBase */                                                                    \
  do_class(sun_security_provider_digestbase,                       "sun/security/provider/DigestBase")                  \
  do_intrinsic(_digestBase_implCompressMB, sun_security_provider_digestbase, implCompressMB_name, implCompressMB_signature, F_R)   \
   do_name(     implCompressMB_name,                               "implCompressMultiBlock0")                           \
   do_signature(implCompressMB_signature,                          "([BII)I")                                           \
                                                                                                                        \
  /* support for com.sun.crypto.provider.GHASH */                                                                       \
  do_class(com_sun_crypto_provider_ghash, "com/sun/crypto/provider/GHASH")                                              \
  do_intrinsic(_ghash_processBlocks, com_sun_crypto_provider_ghash, processBlocks_name, ghash_processBlocks_signature, F_S) \
   do_name(processBlocks_name, "processBlocks")                                                                         \
   do_signature(ghash_processBlocks_signature, "([BII[J[J)V")                                                           \
                                                                                                                        \
  /* support for java.util.zip */                                                                                       \
  do_class(java_util_zip_CRC32,           "java/util/zip/CRC32")                                                        \
  do_intrinsic(_updateCRC32,               java_util_zip_CRC32,   update_name, int2_int_signature,               F_SN)  \
   do_name(     update_name,                                      "update")                                             \
  do_intrinsic(_updateBytesCRC32,          java_util_zip_CRC32,   updateBytes_name, updateBytes_signature,       F_SN)  \
   do_name(     updateBytes_name,                                "updateBytes")                                         \
   do_signature(updateBytes_signature,                           "(I[BII)I")                                            \
  do_intrinsic(_updateByteBufferCRC32,     java_util_zip_CRC32,   updateByteBuffer_name, updateByteBuffer_signature, F_SN) \
   do_name(     updateByteBuffer_name,                           "updateByteBuffer")                                    \
   do_signature(updateByteBuffer_signature,                      "(IJII)I")                                             \
                                                                                                                        \
  /* support for com.alibaba.tenant.TenantContainer */                                                                  \
  do_name(      allocation_context_address,                      "allocationContext")                                   \
  do_name(      tenant_id_address,                               "tenantId")                                            \
                                                                                                                        \
  /* support for sun.misc.Unsafe */                                                                                     \
  do_class(sun_misc_Unsafe,               "sun/misc/Unsafe")                                                            \
                                                                                                                        \
  do_intrinsic(_allocateInstance,         sun_misc_Unsafe,        allocateInstance_name, allocateInstance_signature, F_RN) \
   do_name(     allocateInstance_name,                           "allocateInstance")                                    \
   do_signature(allocateInstance_signature,   "(Ljava/lang/Class;)Ljava/lang/Object;")                                  \
  do_intrinsic(_copyMemory,               sun_misc_Unsafe,        copyMemory_name, copyMemory_signature,         F_RN)  \
   do_name(     copyMemory_name,                                 "copyMemory")                                          \
   do_signature(copyMemory_signature,         "(Ljava/lang/Object;JLjava/lang/Object;JJ)V")                             \
  do_intrinsic(_park,                     sun_misc_Unsafe,        park_name, park_signature,                     F_RN)  \
   do_name(     park_name,                                       "park")                                                \
   do_signature(park_signature,                                  "(ZJ)V")                                               \
  do_intrinsic(_unpark,                   sun_misc_Unsafe,        unpark_name, unpark_signature,                 F_RN)  \
   do_name(     unpark_name,                                     "unpark")                                              \
   do_alias(    unpark_signature,                               /*(LObject;)V*/ object_void_signature)                  \
  do_intrinsic(_loadFence,                sun_misc_Unsafe,        loadFence_name, loadFence_signature,           F_RN)  \
   do_name(     loadFence_name,                                  "loadFence")                                           \
   do_alias(    loadFence_signature,                              void_method_signature)                                \
  do_intrinsic(_storeFence,               sun_misc_Unsafe,        storeFence_name, storeFence_signature,         F_RN)  \
   do_name(     storeFence_name,                                 "storeFence")                                          \
   do_alias(    storeFence_signature,                             void_method_signature)                                \
  do_intrinsic(_fullFence,                sun_misc_Unsafe,        fullFence_name, fullFence_signature,           F_RN)  \
   do_name(     fullFence_name,                                  "fullFence")                                           \
   do_alias(    fullFence_signature,                              void_method_signature)                                \
                                                                                                                        \
  /* Custom branch frequencies profiling support for JSR292 */                                                          \
  do_class(java_lang_invoke_MethodHandleImpl,               "java/lang/invoke/MethodHandleImpl")                        \
  do_intrinsic(_profileBoolean, java_lang_invoke_MethodHandleImpl, profileBoolean_name, profileBoolean_signature,    F_S)  \
   do_name(     profileBoolean_name,                               "profileBoolean")                                     \
   do_signature(profileBoolean_signature,                           "(Z[I)Z")                                            \
                                                                                                                        \
  /* unsafe memory references (there are a lot of them...) */                                                           \
  do_signature(getObject_signature,       "(Ljava/lang/Object;J)Ljava/lang/Object;")                                    \
  do_signature(putObject_signature,       "(Ljava/lang/Object;JLjava/lang/Object;)V")                                   \
  do_signature(getBoolean_signature,      "(Ljava/lang/Object;J)Z")                                                     \
  do_signature(putBoolean_signature,      "(Ljava/lang/Object;JZ)V")                                                    \
  do_signature(getByte_signature,         "(Ljava/lang/Object;J)B")                                                     \
  do_signature(putByte_signature,         "(Ljava/lang/Object;JB)V")                                                    \
  do_signature(getShort_signature,        "(Ljava/lang/Object;J)S")                                                     \
  do_signature(putShort_signature,        "(Ljava/lang/Object;JS)V")                                                    \
  do_signature(getChar_signature,         "(Ljava/lang/Object;J)C")                                                     \
  do_signature(putChar_signature,         "(Ljava/lang/Object;JC)V")                                                    \
  do_signature(getInt_signature,          "(Ljava/lang/Object;J)I")                                                     \
  do_signature(putInt_signature,          "(Ljava/lang/Object;JI)V")                                                    \
  do_signature(getLong_signature,         "(Ljava/lang/Object;J)J")                                                     \
  do_signature(putLong_signature,         "(Ljava/lang/Object;JJ)V")                                                    \
  do_signature(getFloat_signature,        "(Ljava/lang/Object;J)F")                                                     \
  do_signature(putFloat_signature,        "(Ljava/lang/Object;JF)V")                                                    \
  do_signature(getDouble_signature,       "(Ljava/lang/Object;J)D")                                                     \
  do_signature(putDouble_signature,       "(Ljava/lang/Object;JD)V")                                                    \
                                                                                                                        \
  do_name(getObject_name,"getObject")           do_name(putObject_name,"putObject")                                     \
  do_name(getBoolean_name,"getBoolean")         do_name(putBoolean_name,"putBoolean")                                   \
  do_name(getByte_name,"getByte")               do_name(putByte_name,"putByte")                                         \
  do_name(getShort_name,"getShort")             do_name(putShort_name,"putShort")                                       \
  do_name(getChar_name,"getChar")               do_name(putChar_name,"putChar")                                         \
  do_name(getInt_name,"getInt")                 do_name(putInt_name,"putInt")                                           \
  do_name(getLong_name,"getLong")               do_name(putLong_name,"putLong")                                         \
  do_name(getFloat_name,"getFloat")             do_name(putFloat_name,"putFloat")                                       \
  do_name(getDouble_name,"getDouble")           do_name(putDouble_name,"putDouble")                                     \
                                                                                                                        \
  do_intrinsic(_getObject,                sun_misc_Unsafe,        getObject_name, getObject_signature,           F_RN)  \
  do_intrinsic(_getBoolean,               sun_misc_Unsafe,        getBoolean_name, getBoolean_signature,         F_RN)  \
  do_intrinsic(_getByte,                  sun_misc_Unsafe,        getByte_name, getByte_signature,               F_RN)  \
  do_intrinsic(_getShort,                 sun_misc_Unsafe,        getShort_name, getShort_signature,             F_RN)  \
  do_intrinsic(_getChar,                  sun_misc_Unsafe,        getChar_name, getChar_signature,               F_RN)  \
  do_intrinsic(_getInt,                   sun_misc_Unsafe,        getInt_name, getInt_signature,                 F_RN)  \
  do_intrinsic(_getLong,                  sun_misc_Unsafe,        getLong_name, getLong_signature,               F_RN)  \
  do_intrinsic(_getFloat,                 sun_misc_Unsafe,        getFloat_name, getFloat_signature,             F_RN)  \
  do_intrinsic(_getDouble,                sun_misc_Unsafe,        getDouble_name, getDouble_signature,           F_RN)  \
  do_intrinsic(_putObject,                sun_misc_Unsafe,        putObject_name, putObject_signature,           F_RN)  \
  do_intrinsic(_putBoolean,               sun_misc_Unsafe,        putBoolean_name, putBoolean_signature,         F_RN)  \
  do_intrinsic(_putByte,                  sun_misc_Unsafe,        putByte_name, putByte_signature,               F_RN)  \
  do_intrinsic(_putShort,                 sun_misc_Unsafe,        putShort_name, putShort_signature,             F_RN)  \
  do_intrinsic(_putChar,                  sun_misc_Unsafe,        putChar_name, putChar_signature,               F_RN)  \
  do_intrinsic(_putInt,                   sun_misc_Unsafe,        putInt_name, putInt_signature,                 F_RN)  \
  do_intrinsic(_putLong,                  sun_misc_Unsafe,        putLong_name, putLong_signature,               F_RN)  \
  do_intrinsic(_putFloat,                 sun_misc_Unsafe,        putFloat_name, putFloat_signature,             F_RN)  \
  do_intrinsic(_putDouble,                sun_misc_Unsafe,        putDouble_name, putDouble_signature,           F_RN)  \
                                                                                                                        \
  do_name(getObjectVolatile_name,"getObjectVolatile")   do_name(putObjectVolatile_name,"putObjectVolatile")             \
  do_name(getBooleanVolatile_name,"getBooleanVolatile") do_name(putBooleanVolatile_name,"putBooleanVolatile")           \
  do_name(getByteVolatile_name,"getByteVolatile")       do_name(putByteVolatile_name,"putByteVolatile")                 \
  do_name(getShortVolatile_name,"getShortVolatile")     do_name(putShortVolatile_name,"putShortVolatile")               \
  do_name(getCharVolatile_name,"getCharVolatile")       do_name(putCharVolatile_name,"putCharVolatile")                 \
  do_name(getIntVolatile_name,"getIntVolatile")         do_name(putIntVolatile_name,"putIntVolatile")                   \
  do_name(getLongVolatile_name,"getLongVolatile")       do_name(putLongVolatile_name,"putLongVolatile")                 \
  do_name(getFloatVolatile_name,"getFloatVolatile")     do_name(putFloatVolatile_name,"putFloatVolatile")               \
  do_name(getDoubleVolatile_name,"getDoubleVolatile")   do_name(putDoubleVolatile_name,"putDoubleVolatile")             \
                                                                                                                        \
  do_intrinsic(_getObjectVolatile,        sun_misc_Unsafe,        getObjectVolatile_name, getObject_signature,   F_RN)  \
  do_intrinsic(_getBooleanVolatile,       sun_misc_Unsafe,        getBooleanVolatile_name, getBoolean_signature, F_RN)  \
  do_intrinsic(_getByteVolatile,          sun_misc_Unsafe,        getByteVolatile_name, getByte_signature,       F_RN)  \
  do_intrinsic(_getShortVolatile,         sun_misc_Unsafe,        getShortVolatile_name, getShort_signature,     F_RN)  \
  do_intrinsic(_getCharVolatile,          sun_misc_Unsafe,        getCharVolatile_name, getChar_signature,       F_RN)  \
  do_intrinsic(_getIntVolatile,           sun_misc_Unsafe,        getIntVolatile_name, getInt_signature,         F_RN)  \
  do_intrinsic(_getLongVolatile,          sun_misc_Unsafe,        getLongVolatile_name, getLong_signature,       F_RN)  \
  do_intrinsic(_getFloatVolatile,         sun_misc_Unsafe,        getFloatVolatile_name, getFloat_signature,     F_RN)  \
  do_intrinsic(_getDoubleVolatile,        sun_misc_Unsafe,        getDoubleVolatile_name, getDouble_signature,   F_RN)  \
  do_intrinsic(_putObjectVolatile,        sun_misc_Unsafe,        putObjectVolatile_name, putObject_signature,   F_RN)  \
  do_intrinsic(_putBooleanVolatile,       sun_misc_Unsafe,        putBooleanVolatile_name, putBoolean_signature, F_RN)  \
  do_intrinsic(_putByteVolatile,          sun_misc_Unsafe,        putByteVolatile_name, putByte_signature,       F_RN)  \
  do_intrinsic(_putShortVolatile,         sun_misc_Unsafe,        putShortVolatile_name, putShort_signature,     F_RN)  \
  do_intrinsic(_putCharVolatile,          sun_misc_Unsafe,        putCharVolatile_name, putChar_signature,       F_RN)  \
  do_intrinsic(_putIntVolatile,           sun_misc_Unsafe,        putIntVolatile_name, putInt_signature,         F_RN)  \
  do_intrinsic(_putLongVolatile,          sun_misc_Unsafe,        putLongVolatile_name, putLong_signature,       F_RN)  \
  do_intrinsic(_putFloatVolatile,         sun_misc_Unsafe,        putFloatVolatile_name, putFloat_signature,     F_RN)  \
  do_intrinsic(_putDoubleVolatile,        sun_misc_Unsafe,        putDoubleVolatile_name, putDouble_signature,   F_RN)  \
                                                                                                                        \
  /* %%% these are redundant except perhaps for getAddress, but Unsafe has native methods for them */                   \
  do_signature(getByte_raw_signature,     "(J)B")                                                                       \
  do_signature(putByte_raw_signature,     "(JB)V")                                                                      \
  do_signature(getShort_raw_signature,    "(J)S")                                                                       \
  do_signature(putShort_raw_signature,    "(JS)V")                                                                      \
  do_signature(getChar_raw_signature,     "(J)C")                                                                       \
  do_signature(putChar_raw_signature,     "(JC)V")                                                                      \
  do_signature(putInt_raw_signature,      "(JI)V")                                                                      \
      do_alias(getLong_raw_signature,    /*(J)J*/ long_long_signature)                                                  \
      do_alias(putLong_raw_signature,    /*(JJ)V*/ long_long_void_signature)                                            \
  do_signature(getFloat_raw_signature,    "(J)F")                                                                       \
  do_signature(putFloat_raw_signature,    "(JF)V")                                                                      \
      do_alias(getDouble_raw_signature,  /*(J)D*/ long_double_signature)                                                \
  do_signature(putDouble_raw_signature,   "(JD)V")                                                                      \
      do_alias(getAddress_raw_signature, /*(J)J*/ long_long_signature)                                                  \
      do_alias(putAddress_raw_signature, /*(JJ)V*/ long_long_void_signature)                                            \
                                                                                                                        \
   do_name(    getAddress_name,           "getAddress")                                                                 \
   do_name(    putAddress_name,           "putAddress")                                                                 \
                                                                                                                        \
  do_intrinsic(_getByte_raw,              sun_misc_Unsafe,        getByte_name, getByte_raw_signature,           F_RN)  \
  do_intrinsic(_getShort_raw,             sun_misc_Unsafe,        getShort_name, getShort_raw_signature,         F_RN)  \
  do_intrinsic(_getChar_raw,              sun_misc_Unsafe,        getChar_name, getChar_raw_signature,           F_RN)  \
  do_intrinsic(_getInt_raw,               sun_misc_Unsafe,        getInt_name, long_int_signature,               F_RN)  \
  do_intrinsic(_getLong_raw,              sun_misc_Unsafe,        getLong_name, getLong_raw_signature,           F_RN)  \
  do_intrinsic(_getFloat_raw,             sun_misc_Unsafe,        getFloat_name, getFloat_raw_signature,         F_RN)  \
  do_intrinsic(_getDouble_raw,            sun_misc_Unsafe,        getDouble_name, getDouble_raw_signature,       F_RN)  \
  do_intrinsic(_getAddress_raw,           sun_misc_Unsafe,        getAddress_name, getAddress_raw_signature,     F_RN)  \
  do_intrinsic(_putByte_raw,              sun_misc_Unsafe,        putByte_name, putByte_raw_signature,           F_RN)  \
  do_intrinsic(_putShort_raw,             sun_misc_Unsafe,        putShort_name, putShort_raw_signature,         F_RN)  \
  do_intrinsic(_putChar_raw,              sun_misc_Unsafe,        putChar_name, putChar_raw_signature,           F_RN)  \
  do_intrinsic(_putInt_raw,               sun_misc_Unsafe,        putInt_name, putInt_raw_signature,             F_RN)  \
  do_intrinsic(_putLong_raw,              sun_misc_Unsafe,        putLong_name, putLong_raw_signature,           F_RN)  \
  do_intrinsic(_putFloat_raw,             sun_misc_Unsafe,        putFloat_name, putFloat_raw_signature,         F_RN)  \
  do_intrinsic(_putDouble_raw,            sun_misc_Unsafe,        putDouble_name, putDouble_raw_signature,       F_RN)  \
  do_intrinsic(_putAddress_raw,           sun_misc_Unsafe,        putAddress_name, putAddress_raw_signature,     F_RN)  \
                                                                                                                        \
  do_intrinsic(_compareAndSwapObject,     sun_misc_Unsafe,        compareAndSwapObject_name, compareAndSwapObject_signature, F_RN) \
   do_name(     compareAndSwapObject_name,                       "compareAndSwapObject")                                \
   do_signature(compareAndSwapObject_signature,  "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z")          \
  do_intrinsic(_compareAndSwapLong,       sun_misc_Unsafe,        compareAndSwapLong_name, compareAndSwapLong_signature, F_RN) \
   do_name(     compareAndSwapLong_name,                         "compareAndSwapLong")                                  \
   do_signature(compareAndSwapLong_signature,                    "(Ljava/lang/Object;JJJ)Z")                            \
  do_intrinsic(_compareAndSwapInt,        sun_misc_Unsafe,        compareAndSwapInt_name, compareAndSwapInt_signature, F_RN) \
   do_name(     compareAndSwapInt_name,                          "compareAndSwapInt")                                   \
   do_signature(compareAndSwapInt_signature,                     "(Ljava/lang/Object;JII)Z")                            \
  do_intrinsic(_putOrderedObject,         sun_misc_Unsafe,        putOrderedObject_name, putOrderedObject_signature, F_RN) \
   do_name(     putOrderedObject_name,                           "putOrderedObject")                                    \
   do_alias(    putOrderedObject_signature,                     /*(LObject;JLObject;)V*/ putObject_signature)           \
  do_intrinsic(_putOrderedLong,           sun_misc_Unsafe,        putOrderedLong_name, putOrderedLong_signature, F_RN)  \
   do_name(     putOrderedLong_name,                             "putOrderedLong")                                      \
   do_alias(    putOrderedLong_signature,                       /*(Ljava/lang/Object;JJ)V*/ putLong_signature)          \
  do_intrinsic(_putOrderedInt,            sun_misc_Unsafe,        putOrderedInt_name, putOrderedInt_signature,   F_RN)  \
   do_name(     putOrderedInt_name,                              "putOrderedInt")                                       \
   do_alias(    putOrderedInt_signature,                        /*(Ljava/lang/Object;JI)V*/ putInt_signature)           \
                                                                                                                        \
  do_intrinsic(_getAndAddInt,             sun_misc_Unsafe,        getAndAddInt_name, getAndAddInt_signature, F_R)       \
   do_name(     getAndAddInt_name,                                "getAndAddInt")                                       \
   do_signature(getAndAddInt_signature,                           "(Ljava/lang/Object;JI)I" )                           \
  do_intrinsic(_getAndAddLong,            sun_misc_Unsafe,        getAndAddLong_name, getAndAddLong_signature, F_R)     \
   do_name(     getAndAddLong_name,                               "getAndAddLong")                                      \
   do_signature(getAndAddLong_signature,                          "(Ljava/lang/Object;JJ)J" )                           \
  do_intrinsic(_getAndSetInt,             sun_misc_Unsafe,        getAndSetInt_name, getAndSetInt_signature, F_R)       \
   do_name(     getAndSetInt_name,                                "getAndSetInt")                                       \
   do_alias(    getAndSetInt_signature,                         /*"(Ljava/lang/Object;JI)I"*/ getAndAddInt_signature)   \
  do_intrinsic(_getAndSetLong,            sun_misc_Unsafe,        getAndSetLong_name, getAndSetLong_signature, F_R)     \
   do_name(     getAndSetLong_name,                               "getAndSetLong")                                      \
   do_alias(    getAndSetLong_signature,                        /*"(Ljava/lang/Object;JJ)J"*/ getAndAddLong_signature)  \
  do_intrinsic(_getAndSetObject,          sun_misc_Unsafe,        getAndSetObject_name, getAndSetObject_signature,  F_R)\
   do_name(     getAndSetObject_name,                             "getAndSetObject")                                    \
   do_signature(getAndSetObject_signature,                        "(Ljava/lang/Object;JLjava/lang/Object;)Ljava/lang/Object;" ) \
                                                                                                                        \
  /* prefetch_signature is shared by all prefetch variants */                                                           \
  do_signature( prefetch_signature,        "(Ljava/lang/Object;J)V")                                                    \
                                                                                                                        \
  do_intrinsic(_prefetchRead,             sun_misc_Unsafe,        prefetchRead_name, prefetch_signature,         F_RN)  \
   do_name(     prefetchRead_name,                               "prefetchRead")                                        \
  do_intrinsic(_prefetchWrite,            sun_misc_Unsafe,        prefetchWrite_name, prefetch_signature,        F_RN)  \
   do_name(     prefetchWrite_name,                              "prefetchWrite")                                       \
  do_intrinsic(_prefetchReadStatic,       sun_misc_Unsafe,        prefetchReadStatic_name, prefetch_signature,   F_SN)  \
   do_name(     prefetchReadStatic_name,                         "prefetchReadStatic")                                  \
  do_intrinsic(_prefetchWriteStatic,      sun_misc_Unsafe,        prefetchWriteStatic_name, prefetch_signature,  F_SN)  \
   do_name(     prefetchWriteStatic_name,                        "prefetchWriteStatic")                                 \
                                                                                                                        \
    /*== LAST_COMPILER_INLINE*/                                                                                         \
    /*the compiler does have special inlining code for these; bytecode inline is just fine */                           \
                                                                                                                        \
  /* coroutine intrinsics */                                                                                            \
  do_intrinsic(_switchTo,                 java_dyn_CoroutineSupport, switchTo_name, switchTo_signature, F_SN)           \
   do_name(     switchTo_name,                                    "switchTo")                                           \
   do_signature(switchTo_signature,                               "(Ljava/dyn/CoroutineBase;Ljava/dyn/CoroutineBase;)V") \
  do_intrinsic(_switchToAndTerminate,     java_dyn_CoroutineSupport, switchToAndTerminate_name, switchTo_signature, F_SN) \
   do_name(     switchToAndTerminate_name,                        "switchToAndTerminate")                               \
  do_intrinsic(_switchToAndExit,          java_dyn_CoroutineSupport, switchToAndExit_name, switchTo_signature, F_SN)    \
   do_name(     switchToAndExit_name,                             "switchToAndExit")                                    \
                                                                                                                        \
  do_intrinsic(_fillInStackTrace,         java_lang_Throwable, fillInStackTrace_name, void_throwable_signature,  F_RNY) \
                                                                                                                          \
  do_intrinsic(_StringBuilder_void,   java_lang_StringBuilder, object_initializer_name, void_method_signature,     F_R)   \
  do_intrinsic(_StringBuilder_int,    java_lang_StringBuilder, object_initializer_name, int_void_signature,        F_R)   \
  do_intrinsic(_StringBuilder_String, java_lang_StringBuilder, object_initializer_name, string_void_signature,     F_R)   \
                                                                                                                          \
  do_intrinsic(_StringBuilder_append_char,   java_lang_StringBuilder, append_name, char_StringBuilder_signature,   F_R)   \
  do_intrinsic(_StringBuilder_append_int,    java_lang_StringBuilder, append_name, int_StringBuilder_signature,    F_R)   \
  do_intrinsic(_StringBuilder_append_String, java_lang_StringBuilder, append_name, String_StringBuilder_signature, F_R)   \
                                                                                                                          \
  do_intrinsic(_StringBuilder_toString, java_lang_StringBuilder, toString_name, void_string_signature,             F_R)   \
                                                                                                                          \
  do_intrinsic(_StringBuffer_void,   java_lang_StringBuffer, object_initializer_name, void_method_signature,       F_R)   \
  do_intrinsic(_StringBuffer_int,    java_lang_StringBuffer, object_initializer_name, int_void_signature,          F_R)   \
  do_intrinsic(_StringBuffer_String, java_lang_StringBuffer, object_initializer_name, string_void_signature,       F_R)   \
                                                                                                                          \
  do_intrinsic(_StringBuffer_append_char,   java_lang_StringBuffer, append_name, char_StringBuffer_signature,      F_Y)   \
  do_intrinsic(_StringBuffer_append_int,    java_lang_StringBuffer, append_name, int_StringBuffer_signature,       F_Y)   \
  do_intrinsic(_StringBuffer_append_String, java_lang_StringBuffer, append_name, String_StringBuffer_signature,    F_Y)   \
                                                                                                                          \
  do_intrinsic(_StringBuffer_toString,  java_lang_StringBuffer, toString_name, void_string_signature,              F_Y)   \
                                                                                                                          \
  do_intrinsic(_Integer_toString,      java_lang_Integer, toString_name, int_String_signature,                     F_S)   \
                                                                                                                          \
  do_intrinsic(_String_String, java_lang_String, object_initializer_name, string_void_signature,                   F_R)   \
                                                                                                                          \
  do_intrinsic(_Object_init,              java_lang_Object, object_initializer_name, void_method_signature,        F_R)   \
  /*    (symbol object_initializer_name defined above) */                                                                 \
                                                                                                                          \
  do_intrinsic(_invoke,                   java_lang_reflect_Method, invoke_name, object_object_array_object_signature, F_R) \
  /*   (symbols invoke_name and invoke_signature defined above) */                                                      \
  /* the polymorphic MH intrinsics must be in compact order, with _invokeGeneric first and _linkToInterface last */     \
  do_intrinsic(_invokeGeneric,            java_lang_invoke_MethodHandle, invoke_name,           star_name, F_RN)        \
  do_intrinsic(_invokeBasic,              java_lang_invoke_MethodHandle, invokeBasic_name,      star_name, F_RN)        \
  do_intrinsic(_linkToVirtual,            java_lang_invoke_MethodHandle, linkToVirtual_name,    star_name, F_SN)        \
  do_intrinsic(_linkToStatic,             java_lang_invoke_MethodHandle, linkToStatic_name,     star_name, F_SN)        \
  do_intrinsic(_linkToSpecial,            java_lang_invoke_MethodHandle, linkToSpecial_name,    star_name, F_SN)        \
  do_intrinsic(_linkToInterface,          java_lang_invoke_MethodHandle, linkToInterface_name,  star_name, F_SN)        \
  /* special marker for bytecode generated for the JVM from a LambdaForm: */                                            \
  do_intrinsic(_compiledLambdaForm,       java_lang_invoke_MethodHandle, compiledLambdaForm_name, star_name, F_RN)      \
                                                                                                                        \
  /* unboxing methods: */                                                                                               \
  do_intrinsic(_booleanValue,             java_lang_Boolean,      booleanValue_name, void_boolean_signature, F_R)       \
   do_name(     booleanValue_name,       "booleanValue")                                                                \
  do_intrinsic(_byteValue,                java_lang_Byte,         byteValue_name, void_byte_signature, F_R)             \
   do_name(     byteValue_name,          "byteValue")                                                                   \
  do_intrinsic(_charValue,                java_lang_Character,    charValue_name, void_char_signature, F_R)             \
   do_name(     charValue_name,          "charValue")                                                                   \
  do_intrinsic(_shortValue,               java_lang_Short,        shortValue_name, void_short_signature, F_R)           \
   do_name(     shortValue_name,         "shortValue")                                                                  \
  do_intrinsic(_intValue,                 java_lang_Integer,      intValue_name, void_int_signature, F_R)               \
   do_name(     intValue_name,           "intValue")                                                                    \
  do_intrinsic(_longValue,                java_lang_Long,         longValue_name, void_long_signature, F_R)             \
   do_name(     longValue_name,          "longValue")                                                                   \
  do_intrinsic(_floatValue,               java_lang_Float,        floatValue_name, void_float_signature, F_R)           \
   do_name(     floatValue_name,         "floatValue")                                                                  \
  do_intrinsic(_doubleValue,              java_lang_Double,       doubleValue_name, void_double_signature, F_R)         \
   do_name(     doubleValue_name,        "doubleValue")                                                                 \
                                                                                                                        \
  /* boxing methods: */                                                                                                 \
   do_name(    valueOf_name,              "valueOf")                                                                    \
  do_intrinsic(_Boolean_valueOf,          java_lang_Boolean,      valueOf_name, Boolean_valueOf_signature, F_S)         \
   do_name(     Boolean_valueOf_signature,                       "(Z)Ljava/lang/Boolean;")                              \
  do_intrinsic(_Byte_valueOf,             java_lang_Byte,         valueOf_name, Byte_valueOf_signature, F_S)            \
   do_name(     Byte_valueOf_signature,                          "(B)Ljava/lang/Byte;")                                 \
  do_intrinsic(_Character_valueOf,        java_lang_Character,    valueOf_name, Character_valueOf_signature, F_S)       \
   do_name(     Character_valueOf_signature,                     "(C)Ljava/lang/Character;")                            \
  do_intrinsic(_Short_valueOf,            java_lang_Short,        valueOf_name, Short_valueOf_signature, F_S)           \
   do_name(     Short_valueOf_signature,                         "(S)Ljava/lang/Short;")                                \
  do_intrinsic(_Integer_valueOf,          java_lang_Integer,      valueOf_name, Integer_valueOf_signature, F_S)         \
   do_name(     Integer_valueOf_signature,                       "(I)Ljava/lang/Integer;")                              \
  do_intrinsic(_Long_valueOf,             java_lang_Long,         valueOf_name, Long_valueOf_signature, F_S)            \
   do_name(     Long_valueOf_signature,                          "(J)Ljava/lang/Long;")                                 \
  do_intrinsic(_Float_valueOf,            java_lang_Float,        valueOf_name, Float_valueOf_signature, F_S)           \
   do_name(     Float_valueOf_signature,                         "(F)Ljava/lang/Float;")                                \
  do_intrinsic(_Double_valueOf,           java_lang_Double,       valueOf_name, Double_valueOf_signature, F_S)          \
   do_name(     Double_valueOf_signature,                        "(D)Ljava/lang/Double;")                               \
                                                                                                                        \
    /*end*/




// Class vmSymbols

class vmSymbols: AllStatic {
  friend class vmIntrinsics;
  friend class VMStructs;
 public:
  // enum for figuring positions and size of array holding Symbol*s
  enum SID {
    NO_SID = 0,

    #define VM_SYMBOL_ENUM(name, string) VM_SYMBOL_ENUM_NAME(name),
    VM_SYMBOLS_DO(VM_SYMBOL_ENUM, VM_ALIAS_IGNORE)
    #undef VM_SYMBOL_ENUM

    SID_LIMIT,

    #define VM_ALIAS_ENUM(name, def) VM_SYMBOL_ENUM_NAME(name) = VM_SYMBOL_ENUM_NAME(def),
    VM_SYMBOLS_DO(VM_SYMBOL_IGNORE, VM_ALIAS_ENUM)
    #undef VM_ALIAS_ENUM

    FIRST_SID = NO_SID + 1
  };
  enum {
    log2_SID_LIMIT = 10         // checked by an assert at start-up
  };

 private:
  // The symbol array
  static Symbol* _symbols[];

  // Field signatures indexed by BasicType.
  static Symbol* _type_signatures[T_VOID+1];

 public:
  // Initialization
  static void initialize(TRAPS);
  // Accessing
  #define VM_SYMBOL_DECLARE(name, ignore)                 \
    static Symbol* name() {                               \
      return _symbols[VM_SYMBOL_ENUM_NAME(name)];         \
    }
  VM_SYMBOLS_DO(VM_SYMBOL_DECLARE, VM_SYMBOL_DECLARE)
  #undef VM_SYMBOL_DECLARE

  // Sharing support
  static void symbols_do(SymbolClosure* f);
  static void serialize(SerializeClosure* soc);

  static Symbol* type_signature(BasicType t) {
    assert((uint)t < T_VOID+1, "range check");
    assert(_type_signatures[t] != NULL, "domain check");
    return _type_signatures[t];
  }
  // inverse of type_signature; returns T_OBJECT if s is not recognized
  static BasicType signature_type(Symbol* s);

  static Symbol* symbol_at(SID id) {
    assert(id >= FIRST_SID && id < SID_LIMIT, "oob");
    assert(_symbols[id] != NULL, "init");
    return _symbols[id];
  }

  // Returns symbol's SID if one is assigned, else NO_SID.
  static SID find_sid(Symbol* symbol);
  static SID find_sid(const char* symbol_name);

#ifndef PRODUCT
  // No need for this in the product:
  static const char* name_for(SID sid);
#endif //PRODUCT
};

// VM Intrinsic ID's uniquely identify some very special methods
class vmIntrinsics: AllStatic {
  friend class vmSymbols;
  friend class ciObjectFactory;

 public:
  // Accessing
  enum ID {
    _none = 0,                      // not an intrinsic (default answer)

    #define VM_INTRINSIC_ENUM(id, klass, name, sig, flags)  id,
    VM_INTRINSICS_DO(VM_INTRINSIC_ENUM,
                     VM_SYMBOL_IGNORE, VM_SYMBOL_IGNORE, VM_SYMBOL_IGNORE, VM_ALIAS_IGNORE)
    #undef VM_INTRINSIC_ENUM

    ID_LIMIT,
    LAST_COMPILER_INLINE = _prefetchWriteStatic,
    FIRST_MH_SIG_POLY    = _invokeGeneric,
    FIRST_MH_STATIC      = _linkToVirtual,
    LAST_MH_SIG_POLY     = _linkToInterface,

    FIRST_ID = _none + 1
  };

  enum Flags {
    // AccessFlags syndromes relevant to intrinsics.
    F_none = 0,
    F_R,                        // !static ?native !synchronized (R="regular")
    F_S,                        //  static ?native !synchronized
    F_Y,                        // !static ?native  synchronized
    F_RN,                       // !static  native !synchronized
    F_SN,                       //  static  native !synchronized
    F_RNY,                      // !static  native  synchronized

    FLAG_LIMIT
  };
  enum {
    log2_FLAG_LIMIT = 4         // checked by an assert at start-up
  };

public:
  static ID ID_from(int raw_id) {
    assert(raw_id >= (int)_none && raw_id < (int)ID_LIMIT,
           "must be a valid intrinsic ID");
    return (ID)raw_id;
  }

  static const char* name_at(ID id);

private:
  static ID find_id_impl(vmSymbols::SID holder,
                         vmSymbols::SID name,
                         vmSymbols::SID sig,
                         jshort flags);

public:
  // Given a method's class, name, signature, and access flags, report its ID.
  static ID find_id(vmSymbols::SID holder,
                    vmSymbols::SID name,
                    vmSymbols::SID sig,
                    jshort flags) {
    ID id = find_id_impl(holder, name, sig, flags);
#ifdef ASSERT
    // ID _none does not hold the following asserts.
    if (id == _none)  return id;
#endif
    assert(    class_for(id) == holder, "correct id");
    assert(     name_for(id) == name,   "correct id");
    assert(signature_for(id) == sig,    "correct id");
    return id;
  }

  static void verify_method(ID actual_id, Method* m) PRODUCT_RETURN;

  // Find out the symbols behind an intrinsic:
  static vmSymbols::SID     class_for(ID id);
  static vmSymbols::SID      name_for(ID id);
  static vmSymbols::SID signature_for(ID id);
  static Flags              flags_for(ID id);

  static const char* short_name_as_C_string(ID id, char* buf, int size);

  // Wrapper object methods:
  static ID for_boxing(BasicType type);
  static ID for_unboxing(BasicType type);

  // Raw conversion:
  static ID for_raw_conversion(BasicType src, BasicType dest);

  static bool should_be_pinned(vmIntrinsics::ID id);
};

#endif // SHARE_VM_CLASSFILE_VMSYMBOLS_HPP
