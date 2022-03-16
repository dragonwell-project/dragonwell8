/*
 * @test
 * @summary Test deoptimization of _monitorexit bytecode for synchronized block
 * @library /testlibrary /testlibrary/whitebox
 * @build sun.hotspot.WhiteBox
 * @run driver ClassFileInstaller sun.hotspot.WhiteBox sun.hotspot.WhiteBox$WhiteBoxPermission
 * @run main/othervm -XX:TieredStopAtLevel=1 -XX:CompileCommand=compileonly,TestSynchronizedMethodWhenUnwindingDeopt::bar -XX:CompileCommand=compileonly,TestSynchronizedMethodWhenUnwindingDeopt::foo -XX:+PrintDeoptimizationDetails -XX:+IgnoreUnrecognizedVMOptions -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestSynchronizedMethodWhenUnwindingDeopt
 * @run main/othervm -XX:-TieredCompilation -XX:CompileCommand=compileonly,TestSynchronizedMethodWhenUnwindingDeopt::bar -XX:CompileCommand=compileonly,TestSynchronizedMethodWhenUnwindingDeopt::foo -XX:+PrintDeoptimizationDetails -XX:+IgnoreUnrecognizedVMOptions -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestSynchronizedMethodWhenUnwindingDeopt
 */

import sun.hotspot.WhiteBox;

// crash at: fatal error: Possible safepoint reached by thread that does not allow it
/**
 * V  [libjvm.so+0x1ea4de7]  VMError::report_and_die(int, char const*, char const*, __va_list_tag*, Thread*, unsigned char*, void*, void*, char const*, int, unsigned long)+0x187
 * V  [libjvm.so+0x1ea5e77]  VMError::report_and_die(Thread*, void*, char const*, int, char const*, char const*, __va_list_tag*)+0x47
 * V  [libjvm.so+0xfde22d]  report_fatal(char const*, int, char const*, ...)+0x10d
 * V  [libjvm.so+0x1dc7cb7]  Thread::check_for_valid_safepoint_state(bool)+0xc7
 * V  [libjvm.so+0x19f774f]  Monitor::check_prelock_state(Thread*, bool)+0xef
 * V  [libjvm.so+0x19f79b8]  Monitor::lock(Thread*)+0x58
 * V  [libjvm.so+0x1997186]  Method::build_interpreter_method_data(methodHandle const&, Thread*)+0x56
 * V  [libjvm.so+0x13f5124]  InterpreterRuntime::profile_method(JavaThread*)+0x174
 * j  java.lang.Class.cast(Ljava/lang/Object;)Ljava/lang/Object;+0
 * j  java.lang.invoke.VarHandleObjects$FieldInstanceReadWrite.set(Ljava/lang/invoke/VarHandleObjects$FieldInstanceReadWrite;Ljava/lang/Object;Ljava/lang/Object;)V+23
 * j  java.lang.invoke.VarHandleGuards.guard_LL_V(Ljava/lang/invoke/VarHandle;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/invoke/VarHandle$AccessDescriptor;)V+33
 * j  java.util.concurrent.ConcurrentLinkedQueue$Node.<init>(Ljava/lang/Object;)V+9
 * j  java.util.concurrent.ConcurrentLinkedQueue.offer(Ljava/lang/Object;)Z+8
 * j  com.alibaba.wisp.engine.WispScheduler$Worker.pushAndSignal(Lcom/alibaba/wisp/engine/StealAwareRunnable;)Z+5
 * j  com.alibaba.wisp.engine.WispScheduler$SchedulingPolicy$2.enqueue(Lcom/alibaba/wisp/engine/WispScheduler$Worker;ZLcom/alibaba/wisp/engine/StealAwareRunnable;)V+50
 * j  com.alibaba.wisp.engine.WispScheduler.executeWithWorkerThread(Lcom/alibaba/wisp/engine/StealAwareRunnable;Ljava/lang/Thread;)V+46
 * j  com.alibaba.wisp.engine.WispCarrier.wakeupTask(Lcom/alibaba/wisp/engine/WispTask;)V+83
 * j  com.alibaba.wisp.engine.WispTask.unparkInternal(Z)V+55
 * j  com.alibaba.wisp.engine.WispTask.unpark()V+2
 * j  com.alibaba.wisp.engine.WispTask.unparkById(I)V+10
 * v  ~StubRoutines::call_stub
 * V  [libjvm.so+0x140d6f6]  JavaCalls::call_helper(JavaValue*, methodHandle const&, JavaCallArguments*, Thread*)+0x916
 * V  [libjvm.so+0x1409a4e]  JavaCalls::call(JavaValue*, methodHandle const&, JavaCallArguments*, Thread*)+0x4e
 * V  [libjvm.so+0xfcc379]  WispThread::unpark(int, bool, bool, ParkEvent*, WispThread*, Thread*)+0x389
 * V  [libjvm.so+0x1a4e043]  ObjectMonitor::ExitEpilog(Thread*, ObjectWaiter*)+0xe3
 * V  [libjvm.so+0x1d61813]  ObjectSynchronizer::fast_exit(Handle, BasicLock*, Thread*)+0x133
 * V  [libjvm.so+0x1c5555f]  SharedRuntime::complete_monitor_unlocking_C(oopDesc*, BasicLock*, JavaThread*)+0x26f
 * J 94 c2 TestSynchronizedMethodWhenUnwindingDeopt.foo()V (44 bytes) @ 0x00007f9093c9dc25 [0x00007f9093c9d960+0x00000000000002c5]
 * J 95 c2 TestSynchronizedMethodWhenUnwindingDeopt.bar()V (14 bytes) @ 0x00007f9093c9a854 [0x00007f9093c9a840+0x0000000000000014]
 */

public class TestSynchronizedMethodWhenUnwindingDeopt {
    private static WhiteBox WB = WhiteBox.getWhiteBox();

    public static void main(String[] args) throws Exception {
        new Thread(() -> {
            for (int i = 0; i < 50; i++) {
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                WB.deoptimizeAll();
            }
        }).start();
        MultiThreadWispRunner.run(
                "TestSynchronizedMethodWhenUnwindingDeopt",
                4,
                4,
                200_000,
                100000,
                () -> bar(3));
    }

    private volatile static byte WRITE_VAL = 1;

    // A will be inlined
    private synchronized static void foo() {
        for (int i = 0; i < bs.length; i++) {
            bs[i] = WRITE_VAL;
        }
        if (bs[3] == WRITE_VAL)  throw new RuntimeException();  // It must unwind
    }
    private static void bar(int p) {
        try {
            foo();
        } catch (RuntimeException e) {

        } finally {

        }
    }

    static byte[] bs = new byte[12];
}
