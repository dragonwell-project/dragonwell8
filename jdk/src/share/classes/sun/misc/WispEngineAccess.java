/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package sun.misc;


import com.alibaba.wisp.engine.WispTask;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.Constraint;

import java.io.IOException;
import java.nio.channels.SelectableChannel;
import java.util.concurrent.atomic.AtomicReference;

public interface WispEngineAccess {

    WispTask getCurrentTask();

    void registerEvent(SelectableChannel ch, int events) throws IOException;

    void unregisterEvent();

    int epollWait(int epfd, long pollArray, int arraySize, long timeout,
                  AtomicReference<Object> status, final Object INTERRUPTED) throws IOException;

    void interruptEpoll(AtomicReference<Object> status, Object INTERRUPTED, int interruptFD);

    void addTimer(long deadlineNano);

    void cancelTimer();

    void sleep(long ms);

    void yield();

    boolean isThreadTask(WispTask task);

    boolean isTimeout();

    void park(long timeoutNanos);

    void unpark(WispTask task);

    void destroy();

    boolean isAlive(WispTask task);

    void interrupt(WispTask task);

    boolean testInterruptedAndClear(WispTask task, boolean clear);

    boolean hasMoreTasks();

    boolean runningAsCoroutine(Thread t);

    boolean usingWispEpoll();

    boolean isAllThreadAsWisp();

    boolean tryStartThreadAsWisp(Thread thread, Runnable target, long stackSize);

    boolean useDirectSelectorWakeup();

    boolean enableSocketLock();

    StackTraceElement[] getStackTrace(WispTask task);

    void getCpuTime(long[] ids, long[] times);

    /**
     * @param channel Blocking SocketChannel waiting for interestOps
     * @param interestOps Net.* enum constant interests
     * @param timeout timeout in milliseconds
     * @return active event count
     * @throws IOException
     */
    int poll(SelectableChannel channel, int interestOps, long timeout) throws IOException;
}
