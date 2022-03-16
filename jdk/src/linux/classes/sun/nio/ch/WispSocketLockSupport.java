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

package sun.nio.ch;

import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;

import java.util.concurrent.locks.ReentrantLock;

/**
 * This class supports fd use across {@link WispTask} by adding read/write reentrantLocks for
 * {@link WispSocketImpl} {@link WispServerSocketImpl} and {@link WispUdpSocketImpl},
 * {@link WispTask}s will park once they encountered a contended socket.
 */
class WispSocketLockSupport {
    private static WispEngineAccess WEA = SharedSecrets.getWispEngineAccess();

    /** Lock held when reading and accepting
     */
    private final ReentrantLock readLock = WEA.enableSocketLock()? new ReentrantLock() : null;

    /** Lock held when writing and connecting
     */
    private final ReentrantLock writeLock = WEA.enableSocketLock()? new ReentrantLock() : null;

    /** Lock held when tracking current blocked WispTask and closing
     */
    private final ReentrantLock stateLock = WEA.enableSocketLock()? new ReentrantLock() : null;

    WispTask blockedReadWispTask  = null;
    WispTask blockedWriteWispTask = null;

    /**
     * This is not a  ReadWriteLock, they are separate locks here, readLock protect
     * reading to fd while writeLock protect writing to fd.
     */
    private void lockRead() {
        readLock.lock();
    }

    private void lockWrite() {
        writeLock.lock();
    }

    private void unLockRead() {
        readLock.unlock();
    }

    private void unLockWrite() {
        writeLock.unlock();
    }

    void beginRead() {
        if (!WEA.enableSocketLock()) {
            return;
        }
        lockRead();
        stateLock.lock();
        try {
            blockedReadWispTask = WEA.getCurrentTask();
        } finally {
            stateLock.unlock();
        }
    }

    void endRead() {
        if (!WEA.enableSocketLock()) {
            return;
        }

        stateLock.lock();
        try {
            blockedReadWispTask = null;
        } finally {
            stateLock.unlock();
        }
        unLockRead();
    }

    void beginWrite() {
        if (!WEA.enableSocketLock()) {
            return;
        }
        lockWrite();
        stateLock.lock();
        try {
            blockedWriteWispTask = WEA.getCurrentTask();
        } finally {
            stateLock.unlock();
        }
    }

    void endWrite() {
        if (!WEA.enableSocketLock()) {
            return;
        }
        stateLock.lock();
        try {
            blockedWriteWispTask = null;
        } finally {
            stateLock.unlock();
        }
        unLockWrite();
    }

    void unparkBlockedWispTask() {
        stateLock.lock();
        try {
            if (blockedReadWispTask != null) {
                WEA.unpark(blockedReadWispTask);
                blockedReadWispTask = null;
            }
            if (blockedWriteWispTask != null) {
                WEA.unpark(blockedWriteWispTask);
                blockedWriteWispTask = null;
            }
        } finally {
            stateLock.unlock();
        }
    }
}
