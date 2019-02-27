/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
package jdk.testlibrary.jfr;

import jdk.jfr.EventType;

/**
 * Contains id for events that are shipped with the JDK.
 *
 */
public class EventNames {

    public final static String PREFIX = "com.oracle.jdk.";
    private static final String GC_CATEGORY = "GC";

    // JVM Configuration
    public final static String JVMInformation = PREFIX + "JVMInformation"; // "vm.info";
    public final static String InitialSystemProperty = PREFIX + "InitialSystemProperty";// "vm.initial_system_property";
    public final static String IntFlag = PREFIX + "IntFlag"; // "vm.flag.int";
    public final static String UnsignedIntFlag = PREFIX + "UnsignedIntFlag"; // "vm.flag.uint";
    public final static String LongFlag = PREFIX + "LongFlag"; // "vm.flag.long";
    public final static String UnsignedLongFlag = PREFIX + "UnsignedLongFlag"; // "vm.flag.ulong";
    public final static String DoubleFlag = PREFIX + "DoubleFlag"; // "vm.flag.double";
    public final static String BooleanFlag = PREFIX + "BooleanFlag"; // vm.flag.boolean";
    public final static String StringFlag = PREFIX + "StringFlag"; // vm.flag.string";
    public final static String IntFlagChanged = PREFIX + "IntFlagChanged"; // vm.flag.int_changed";
    public final static String UnsignedIntFlagChanged = PREFIX + "UnsignedIntFlagChanged"; // "vm.flag.uint_changed";
    public final static String LongFlagChanged = PREFIX + "LongFlagChanged"; // "vm.flag.long_changed";
    public final static String UnsignedLongFlagChanged = PREFIX + "UnsignedLongFlagChanged"; // "vm.flag.ulong_changed";
    public final static String DoubleFlagChanged = PREFIX + "DoubleFlagChanged"; // vm.flag.double_changed";
    public final static String BooleanFlagChanged = PREFIX + "BooleanFlagChanged"; // "vm.flag.boolean_changed";
    public final static String StringFlagChanged = PREFIX + "StringFlagChanged"; // "vm.flag.string_changed";

    // Runtime
    public final static String VMException = PREFIX + "JavaErrorThrow"; // java.vm.exception_throw";
    public final static String ThreadStart = PREFIX + "ThreadStart";// "java.thread_start";
    public final static String ThreadEnd = PREFIX + "ThreadEnd";// "java.thread_end";
    public final static String ThreadSleep = PREFIX + "ThreadSleep"; // "java.thread_sleep";
    public final static String ThreadPark = PREFIX + "ThreadPark"; // "java.thread_park";
    public final static String JavaMonitorEnter = PREFIX + "JavaMonitorEnter"; // "java.monitor_enter";
    public final static String JavaMonitorWait = PREFIX + "JavaMonitorWait"; // "java.monitor_wait";
    public final static String JavaMonitorInflate = PREFIX + "JavaMonitorInflate"; // "java.monitor_inflate";
    public final static String ClassLoad = PREFIX + "ClassLoad"; // "vm.class.load";
    public final static String ClassDefine = PREFIX + "ClassDefine";// "vm.class.define";
    public final static String ClassUnload = PREFIX + "ClassUnload";// "vm.class.unload";
    public final static String SafepointBegin = PREFIX + "SafepointBegin";// "vm.runtime.safepoint.begin";
    public final static String SafepointStateSyncronization = PREFIX + "SafepointStateSynchronization";// "vm.runtime.safepoint.statesync";
    public final static String SafepointWaitBlocked = PREFIX + "SafepointWaitBlocked";// "vm.runtime.safepoint.waitblocked";
    public final static String SafepointCleanup = PREFIX + "SafepointCleanup";// "vm.runtime.safepoint.cleanup";
    public final static String SafepointCleanupTask = PREFIX + "SafepointCleanupTask";// "vm.runtime.safepoint.cleanuptask";
    public final static String SafepointEnd = PREFIX + "SafepointEnd";// "vm.runtime.safepoint.end";
    public final static String ExecuteVMOperation = PREFIX + "ExecuteVMOperation"; // "vm.runtime.execute_vm_operation";
    public final static String Shutdown = PREFIX + "Shutdown"; // "vm.runtime.shutdown";
    public final static String VMError = PREFIX + "VMError"; // "vm.runtime.vm_error";
    public final static String JavaThreadStatistics = PREFIX + "JavaThreadStatistics"; // "java.statistics.threads";
    public final static String ClassLoadingStatistics = PREFIX + "ClassLoadingStatistics"; // "java.statistics.class_loading";
    public final static String ClassLoaderStatistics = PREFIX + "ClassLoaderStatistics"; // "java.statistics.class_loaders";
    public final static String ThreadAllocationStatistics = PREFIX + "ThreadAllocationStatistics"; // "java.statistics.thread_allocation";
    public final static String ExecutionSample = PREFIX + "ExecutionSample"; // "vm.prof.execution_sample";
    public final static String ExecutionSampling = PREFIX + "ExecutionSampling"; // "vm.prof.execution_sampling_info";
    public final static String ThreadDump = PREFIX + "ThreadDump"; // "vm.runtime.thread_dump";
    public final static String OldObjectSample = PREFIX + "OldObjectSample";
    public final static String BiasedLockRevocation = PREFIX + "BiasedLockRevocation"; // "java.biased_lock_revocation";
    public final static String BiasedLockSelfRevocation = PREFIX + "BiasedLockSelfRevocation"; // "java.biased_lock_self_revocation";
    public final static String BiasedLockClassRevocation = PREFIX + "BiasedLockClassRevocation"; // "java.biased_lock_class_revocation";

    // GC
    public final static String GCHeapSummary = PREFIX + "GCHeapSummary"; // "vm.gc.heap.summary";
    public final static String MetaspaceSummary = PREFIX + "MetaspaceSummary"; // "vm.gc.heap.metaspace_summary";
    public final static String MetaspaceGCThreshold = PREFIX + "MetaspaceGCThreshold"; // "vm.gc.metaspace.gc_threshold";
    public final static String MetaspaceAllocationFailure = PREFIX + "MetaspaceAllocationFailure"; // "vm.gc.metaspace.allocation_failure";
    public final static String MetaspaceOOM = PREFIX + "MetaspaceOOM"; // "vm.gc.metaspace.out_of_memory";
    public final static String MetaspaceChunkFreeListSummary = PREFIX + "MetaspaceChunkFreeListSummary"; // "vm.gc.metaspace.chunk_free_list_summary";
    public final static String PSHeapSummary = PREFIX + "PSHeapSummary"; // "vm.gc.heap.ps_summary";
    public final static String G1HeapSummary = PREFIX + "G1HeapSummary"; // "vm.gc.heap.g1_summary";
    public final static String G1HeapRegionInformation = PREFIX + "G1HeapRegionInformation"; // "vm.gc.detailed.g1_heap_region_information";
    public final static String G1HeapRegionTypeChange = PREFIX + "G1HeapRegionTypeChange"; // "vm.gc.detailed.g1_heap_region_type_change";
    public final static String TenuringDistribution = PREFIX + "TenuringDistribution"; // "vm.gc.detailed.tenuring_distribution";
    public final static String GarbageCollection = PREFIX + "GarbageCollection"; // "vm.gc.collector.garbage_collection";
    public final static String ParallelOldCollection = PREFIX + "ParallelOldGarbageCollection"; // "vm.gc.collector.parold_garbage_collection";
    public final static String YoungGarbageCollection = PREFIX + "YoungGarbageCollection";// "vm.gc.collector.young_garbage_collection";
    public final static String OldGarbageCollection = PREFIX + "OldGarbageCollection"; // "vm.gc.collector.old_garbage_collection";
    public final static String G1GarbageCollection = PREFIX + "G1GarbageCollection"; // "vm.gc.collector.g1_garbage_collection";
    public final static String G1MMU = PREFIX + "G1MMU"; // "vm.gc.detailed.g1_mmu_info";
    public final static String EvacuationInfo = PREFIX + "EvacuationInfo";// "vm.gc.detailed.evacuation_info";
    public final static String GCReferenceStatistics = PREFIX + "GCReferenceStatistics";// "vm.gc.reference.statistics";
    public final static String ObjectCountAfterGC = PREFIX + "ObjectCountAfterGC"; // "vm.gc.detailed.object_count_after_gc";
    public final static String PromoteObjectInNewPLAB = PREFIX + "PromoteObjectInNewPLAB";// "vm.gc.detailed.object_promotion_in_new_PLAB";
    public final static String PromoteObjectOutsidePLAB = PREFIX + "PromoteObjectOutsidePLAB"; // "vm.gc.detailed.object_promotion_outside_PLAB";
    public final static String PromotionFailed = PREFIX + "PromotionFailed"; // "vm.gc.detailed.promotion_failed";
    public final static String EvacuationFailed = PREFIX + "EvacuationFailed"; // "vm.gc.detailed.evacuation_failed";
    public final static String ConcurrentModeFailure = PREFIX + "ConcurrentModeFailure"; // "vm.gc.detailed.concurrent_mode_failure";
    public final static String GCPhasePause = PREFIX + "GCPhasePause"; // "vm.gc.phases.pause";
    public final static String GCPhasePauseLevel1 = PREFIX + "GCPhasePauseLevel1"; // "vm.gc.phases.pause_level_1";
    public final static String GCPhasePauseLevel2 = PREFIX + "GCPhasePauseLevel2"; // vm.gc.phases.pause_level_2";
    public final static String GCPhasePauseLevel3 = PREFIX + "GCPhasePauseLevel3"; // "vm.gc.phases.pause_level_3";
    public final static String ObjectCount = PREFIX + "ObjectCount"; // "vm.gc.detailed.object_count";
    public final static String GCConfiguration = PREFIX + "GCConfiguration"; // vm.gc.configuration.gc";
    public final static String GCSurvivorConfiguration = PREFIX + "GCSurvivorConfiguration"; // "vm.gc.configuration.survivor";
    public final static String GCTLABConfiguration = PREFIX + "GCTLABConfiguration";// "vm.gc.configuration.tlab";
    public final static String GCHeapConfiguration = PREFIX + "GCHeapConfiguration";// "vm.gc.configuration.heap";
    public final static String YoungGenerationConfiguration = PREFIX + "YoungGenerationConfiguration"; // "vm.gc.configuration.young_generation";
    public final static String G1AdaptiveIHOP = PREFIX + "G1AdaptiveIHOP"; // "vm/gc/detailed/g1_adaptive_ihop_status"
    public final static String G1EvacuationYoungStatistics = PREFIX + "G1EvacuationYoungStatistics"; // "vm/gc/detailed/g1_evac_young_stats"
    public final static String G1EvacuationOldStatistics = PREFIX + "G1EvacuationOldStatistics"; // "vm/gc/detailed/g1_evac_old_stats"
    public final static String G1BasicIHOP = PREFIX + "G1BasicIHOP"; // "vm/gc/detailed/g1_basic_ihop_status"
    public final static String AllocationRequiringGC = PREFIX + "AllocationRequiringGC"; // "vm/gc/detailed/allocation_requiring_gc"

    // Compiler
    public final static String Compilation = PREFIX + "Compilation";// "vm.compiler.compilation";
    public final static String CompilerPhase = PREFIX + "CompilerPhase";// "vm.compiler.phase";
    public final static String CompilationFailure = PREFIX + "CompilationFailure";// "vm.compiler.failure";
    public final static String CompilerInlining = PREFIX + "CompilerInlining";// "vm.compiler.optimization.inlining";
    public final static String CompilerStatistics = PREFIX + "CompilerStatistics";// "vm.compiler.stats";
    public final static String CompilerConfig = PREFIX + "CompilerConfiguration";// "vm.compiler.config";
    public final static String CodeCacheStatistics = PREFIX + "CodeCacheStatistics";// "vm.code_cache.stats";
    public final static String CodeCacheConfiguration = PREFIX + "CodeCacheConfiguration";// "vm.code_cache.config";
    public final static String CodeSweeperStatistics = PREFIX + "CodeSweeperStatistics";// "vm.code_sweeper.stats";
    public final static String CodeSweeperConfiguration = PREFIX + "CodeSweeperConfiguration";// "vm.code_sweeper.config";
    public final static String SweepCodeCache = PREFIX + "SweepCodeCache";// "vm.code_sweeper.sweep";
    public final static String CodeCacheFull = PREFIX + "CodeCacheFull";// "vm.code_cache.full";
    public final static String ObjectAllocationInNewTLAB = PREFIX + "ObjectAllocationInNewTLAB";// "java.object_alloc_in_new_TLAB";
    public final static String ObjectAllocationOutsideTLAB = PREFIX + "ObjectAllocationOutsideTLAB";// "java.object_alloc_outside_TLAB";

    // OS
    public final static String OSInformation = PREFIX + "OSInformation";// "os.information";
    public final static String CPUInformation = PREFIX + "CPUInformation";// "os.processor.cpu_information";
    public final static String CPULoad = PREFIX + "CPULoad";// "os.processor.cpu_load";
    public final static String ThreadCPULoad = PREFIX + "ThreadCPULoad"; // "os.processor.thread_cpu_load";
    public final static String SystemProcess = PREFIX + "SystemProcess";// "os.system_process";
    public final static String ThreadContextSwitchRate = PREFIX + "ThreadContextSwitchRate";// "os.processor.context_switch_rate";
    public final static String InitialEnvironmentVariable = PREFIX + "InitialEnvironmentVariable";// "os.initial_environment_variable";
    public final static String NativeLibrary = PREFIX + "NativeLibrary";// "vm.runtime.native_libraries";
    public final static String PhysicalMemory = PREFIX + "PhysicalMemory";// "os.memory.physical_memory";

    // JDK
    public static final String FileForce  = PREFIX + "FileForce";// "java.file_force";
    public static final String FileRead = PREFIX + "FileRead";// "java.file_read";
    public static final String FileWrite = PREFIX + "FileWrite"; // "java.file_write";
    public static final String SocketRead = PREFIX + "SocketRead";// "java.socket_read";
    public static final String SocketWrite = PREFIX + "SocketWrite";// "java.socket_write";
    public final static String ExceptionStatistics = PREFIX + "ExceptionStatistics"; // "java.statistics.throwables";
    public final static String JavaExceptionThrow = PREFIX + "JavaExceptionThrow"; // java.exception_throw";
    public final static String JavaErrorThrow = PREFIX + "JavaErrorThrow"; // "java.error_throw";;
    public final static String ModuleRequire = PREFIX + "ModuleRequire"; // "com.oracle.jdk.ModuleRequire"
    public final static String ModuleExport = PREFIX + "ModuleExport"; // "com.oracle.jdk.ModuleExport"

    // Flight Recorder
    public final static String DumpReason = PREFIX + "DumpReason";// "flight_recorder.dump_reason";
    public final static String DataLoss = PREFIX + "DataLoss"; // "flight_recorder.data_loss";
    public final static String CPUTimeStampCounter = PREFIX + "CPUTimeStampCounter";// "os.processor.cpu_tsc";
    public final static String ActiveRecording = PREFIX + "ActiveRecording";//"com.oracle.jdk.ActiveRecording"
    public final static String ActiveSetting = PREFIX + "ActiveSetting";//"com.oracle.jdk.ActiveSetting"

    public static boolean isGcEvent(EventType et) {
        return et.getCategoryNames().contains(GC_CATEGORY);
    }

}
