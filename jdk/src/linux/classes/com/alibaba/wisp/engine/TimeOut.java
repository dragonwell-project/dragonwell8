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

import java.util.Arrays;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.TimeUnit;

/**
 * Represents {@link WispTask} related time out
 */
public class TimeOut {
    enum Action {
        JVM_UNPARK,
        JDK_UNPARK,
        RESUME
    }
    private final Action action;

    final WispTask task;
    long deadlineNano;
    boolean canceled = false;
    /**
     * its position in the heapArray, -1 indicates that TimeOut has been deleted
     */
    private int queueIdx;
    private TimerManager manager;

    /**
     * @param task         related {@link WispTask}
     * @param deadlineNano wake up related {@link WispTask} at {@code deadline} if not canceled
     */
    public TimeOut(WispTask task, long deadlineNano, Action action) {
        this.task = task;
        this.deadlineNano = deadlineNano;
        this.action = action;
    }

    /**
     * @return {@code true} if and only if associated timer is expired.
     */
    public boolean expired() {
        return !canceled && System.nanoTime() >= deadlineNano;
    }

    /**
     * unpark the blocked task or resume the pending timeout task.
     */
    void doAction() {
        switch (action) {
            case RESUME:
                task.carrier.wakeupTask(task);
                break;
            case JDK_UNPARK:
                task.jdkUnpark();
                break;
            case JVM_UNPARK:
                task.unpark();
                break;
        }
    }

    /**
     * We use minimum heap algorithm to get the minimum deadline of all timers,
     * also keep every TimeOut's queueIdx(its position in heap) so that we can easily
     * remove it.
     */
    static class TimerManager {
        Queue queue = new Queue();
        ConcurrentLinkedQueue<TimeOut> rmQ = new ConcurrentLinkedQueue<>();

        void copyTimer(Queue copiedQueue) {
            copiedQueue.size = 0;
            for (TimeOut timeOut : copiedQueue.queue) {
                if (timeOut != null) {
                    addTimer(timeOut);
                }
            }
        }

        void addTimer(TimeOut timeOut) {
            timeOut.deadlineNano = overflowFree(timeOut.deadlineNano, queue.peek());
            timeOut.manager = this;
            queue.offer(timeOut);
        }

        void cancelTimer(TimeOut timeOut) {
            if (timeOut.queueIdx != -1) {
                if (timeOut.manager == this) {
                    queue.remove(timeOut);
                } else {
                    timeOut.manager.rmQ.add(timeOut);
                }
            }
        }

        /**
         * Dispatch timeout events and return the timeout deadline for next
         * first timeout task
         *
         * @return -1: there's no timeout task, > 0 deadline nanos
         */
        long processTimeoutEventsAndGetWaitDeadline(final long now) {
            TimeOut timeOut;
            while ((timeOut = rmQ.poll()) != null) {
                assert timeOut.manager == this;
                queue.remove(timeOut);
            }

            long deadline = -1;
            if (queue.size != 0) {
                while ((timeOut = queue.peek()) != null) {
                    if (timeOut.canceled) {
                        queue.poll();
                    } else if (timeOut.deadlineNano <= now) {
                        queue.poll();
                        timeOut.doAction();
                    } else {
                        deadline = timeOut.deadlineNano;
                        break;
                    }
                }
            }
            return deadline;
        }

        static class Queue {
            private static final int INITIAL_CAPACITY = 16;
            private TimeOut[] queue = new TimeOut[INITIAL_CAPACITY];
            private int size = 0;

            /**
             * Inserts TimeOut x at position k, maintaining heap invariant by
             * promoting x up the tree until it is greater than or equal to
             * its parent, or is the root.
             *
             * @param k       the position to fill
             * @param timeOut the TimeOut to insert
             */
            private void siftUp(int k, TimeOut timeOut) {
                while (k > 0) {
                    int parent = (k - 1) >>> 1;
                    TimeOut e = queue[parent];
                    if (timeOut.deadlineNano >= e.deadlineNano) {
                        break;
                    }
                    queue[k] = e;
                    e.queueIdx = k;
                    k = parent;
                }
                queue[k] = timeOut;
                timeOut.queueIdx = k;
            }

            /**
             * Inserts item x at position k, maintaining heap invariant by
             * demoting x down the tree repeatedly until it is less than or
             * equal to its children or is a leaf.
             *
             * @param k       the position to fill
             * @param timeOut the item to insert
             */
            private void siftDown(int k, TimeOut timeOut) {
                int half = size >>> 1;
                while (k < half) {
                    int child = (k << 1) + 1;
                    TimeOut c = queue[child];
                    int right = child + 1;
                    if (right < size && c.deadlineNano > queue[right].deadlineNano) {
                        c = queue[child = right];
                    }
                    if (timeOut.deadlineNano <= c.deadlineNano) {
                        break;
                    }
                    queue[k] = c;
                    c.queueIdx = k;
                    k = child;
                }
                queue[k] = timeOut;
                timeOut.queueIdx = k;
            }


            public boolean remove(TimeOut timeOut) {
                int i = timeOut.queueIdx;
                if (i == -1) {
                    //this timeOut has been deleted.
                    return false;
                }

                queue[i].queueIdx = -1;
                int s = --size;
                TimeOut replacement = queue[s];
                queue[s] = null;
                if (s != i) {
                    siftDown(i, replacement);
                    if (queue[i] == replacement) {
                        siftUp(i, replacement);
                    }
                }
                return true;
            }

            public int size() {
                return size;
            }

            public boolean offer(TimeOut timeOut) {
                int i = size++;
                if (i >= queue.length) {
                    queue = Arrays.copyOf(queue, queue.length * 2);
                }
                if (i == 0) {
                    queue[0] = timeOut;
                    timeOut.queueIdx = 0;
                } else {
                    siftUp(i, timeOut);
                }
                return true;
            }

            public TimeOut poll() {
                if (size == 0) {
                    return null;
                }
                TimeOut f = queue[0];
                int s = --size;
                TimeOut x = queue[s];
                queue[s] = null;
                if (s != 0) {
                    siftDown(0, x);
                }
                f.queueIdx = -1;
                return f;
            }

            public TimeOut peek() {
                return queue[0];
            }
        }
    }

    private static long overflowFree(long deadlineNano, TimeOut head) {
        if (deadlineNano < Long.MIN_VALUE / 2) { // deadlineNano regarded as negative overflow
            deadlineNano = Long.MAX_VALUE;
        }
        if (head != null && head.deadlineNano < 0 && deadlineNano > 0 && deadlineNano - head.deadlineNano < 0) {
            deadlineNano = Long.MAX_VALUE + head.deadlineNano;
            // then deadlineNano - head.deadlineNano = Long.MAX_VALUE > 0
            // i.e. deadlineNano > head.deadlineNano
        }
        return deadlineNano;
    }

    static long nanos2Millis(long nanos) {
        long ms = TimeUnit.NANOSECONDS.toMillis(nanos + TimeUnit.MILLISECONDS.toNanos(1) / 2);
        if (ms < 0) {
            ms = 0;
        }
        if (WispConfiguration.PARK_ONE_MS_AT_LEAST && ms == 0) {
            ms = 1;
        }
        return ms;
    }
}
