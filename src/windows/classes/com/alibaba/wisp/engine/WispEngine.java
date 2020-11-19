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

package com.alibaba.wisp.engine;

import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;

import java.io.IOException;
import java.nio.channels.SelectableChannel;
import java.util.concurrent.atomic.AtomicReference;

public class WispEngine {

    public static boolean transparentWispSwitch() {
        return false;
    }

    public static boolean enableThreadAsWisp() {
        return false;
    }

    private static void setWispEngineAccess() {
        SharedSecrets.setWispEngineAccess(new WispEngineAccess() {

            @Override
            public WispTask getCurrentTask() {
                throw new UnsupportedOperationException();
            }

            @Override
            public void registerEvent(SelectableChannel ch, int events) throws IOException {
                throw new UnsupportedOperationException();
            }

            @Override
            public void unregisterEvent() {
                throw new UnsupportedOperationException();
            }

            @Override
            public int epollWait(int epfd, long pollArray, int arraySize, long timeout,
                                 AtomicReference<Object> status, Object INTERRUPTED) throws IOException {
                throw new UnsupportedOperationException();
            }

            @Override
            public void interruptEpoll(AtomicReference<Object> status, Object INTERRUPTED, int interruptFd) {
                throw new UnsupportedOperationException();
            }

            @Override
            public void addTimer(long deadlineNano) {
                throw new UnsupportedOperationException();
            }

            @Override
            public void cancelTimer() {
                throw new UnsupportedOperationException();
            }

            @Override
            public void sleep(long ms) {
                throw new UnsupportedOperationException();
            }

            @Override
            public void yield() {
                throw new UnsupportedOperationException();
            }

            @Override
            public boolean isThreadTask(WispTask task) {
                return false;
            }

            @Override
            public boolean isTimeout() {
                return false;
            }

            @Override
            public void park(long timeoutNano) {
                throw new UnsupportedOperationException();
            }

            @Override
            public void unpark(WispTask task) {
                throw new UnsupportedOperationException();
            }

            @Override
            public void destroy() {
                throw new UnsupportedOperationException();
            }

            @Override
            public boolean hasMoreTasks() {
                return false;
            }

            @Override
            public boolean runningAsCoroutine(Thread t) {
                return false;
            }

            @Override
            public boolean usingWispEpoll() {
                return false;
            }

            public boolean isAlive(WispTask task) {
                return false;
            }

            @Override
            public void interrupt(WispTask task) {
                throw new UnsupportedOperationException();
            }

            @Override
            public boolean testInterruptedAndClear(WispTask task, boolean clear) {
                return false;
            }

            @Override
            public boolean tryStartThreadAsWisp(Thread thread, Runnable target) {
                return false;
            }

            @Override
            public boolean isAllThreadAsWisp() {
                return false;
            }

            @Override
            public boolean useDirectSelectorWakeup() {
                return false;
            }

            @Override
            public boolean enableSocketLock() {
                return false;
            }

            @Override
            public StackTraceElement[] getStackTrace(WispTask task) {
                throw new UnsupportedOperationException();
            }

            @Override
            public void getCpuTime(long[] ids, long[] times) {
                throw new UnsupportedOperationException();
            }

            @Override
            public int poll(SelectableChannel channel, int interestOps, long timeout) throws IOException {
                throw new UnsupportedOperationException();
            }
        });
    }

}
