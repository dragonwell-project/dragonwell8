/*
 * Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
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

package com.sun.management;

import java.lang.management.ThreadInfo;
import java.util.Map;

/**
 * Platform-specific management interface for the thread system
 * of the Java virtual machine.
 * <p>
 * This platform extension is only available to a thread
 * implementation that supports this extension.
 *
 * @author  Paul Hohensee
 * @since   6u25
 */

@jdk.Exported
public interface ThreadMXBean extends java.lang.management.ThreadMXBean {
    /**
     * Returns the total CPU time for each thread whose ID is
     * in the input array {@code ids} in nanoseconds.
     * The returned values are of nanoseconds precision but
     * not necessarily nanoseconds accuracy.
     * <p>
     * This method is equivalent to calling the
     * {@link ThreadMXBean#getThreadCpuTime(long)}
     * method for each thread ID in the input array {@code ids} and setting the
     * returned value in the corresponding element of the returned array.
     *
     * @param ids an array of thread IDs.
     * @return an array of long values, each of which is the amount of CPU
     * time the thread whose ID is in the corresponding element of the input
     * array of IDs has used,
     * if the thread of a specified ID exists, the thread is alive,
     * and CPU time measurement is enabled;
     * {@code -1} otherwise.
     *
     * @throws NullPointerException if {@code ids} is {@code null}
     * @throws IllegalArgumentException if any element in the input array
     *         {@code ids} is {@code <=} {@code 0}.
     * @throws java.lang.UnsupportedOperationException if the Java
     *         virtual machine implementation does not support CPU time
     *         measurement.
     *
     * @see ThreadMXBean#getThreadCpuTime(long)
     * @see #getThreadUserTime
     * @see ThreadMXBean#isThreadCpuTimeSupported
     * @see ThreadMXBean#isThreadCpuTimeEnabled
     * @see ThreadMXBean#setThreadCpuTimeEnabled
     */
    public long[] getThreadCpuTime(long[] ids);

    /**
     * Returns the CPU time that each thread whose ID is in the input array
     * {@code ids} has executed in user mode in nanoseconds.
     * The returned values are of nanoseconds precision but
     * not necessarily nanoseconds accuracy.
     * <p>
     * This method is equivalent to calling the
     * {@link ThreadMXBean#getThreadUserTime(long)}
     * method for each thread ID in the input array {@code ids} and setting the
     * returned value in the corresponding element of the returned array.
     *
     * @param ids an array of thread IDs.
     * @return an array of long values, each of which is the amount of user
     * mode CPU time the thread whose ID is in the corresponding element of
     * the input array of IDs has used,
     * if the thread of a specified ID exists, the thread is alive,
     * and CPU time measurement is enabled;
     * {@code -1} otherwise.
     *
     * @throws NullPointerException if {@code ids} is {@code null}
     * @throws IllegalArgumentException if any element in the input array
     *         {@code ids} is {@code <=} {@code 0}.
     * @throws java.lang.UnsupportedOperationException if the Java
     *         virtual machine implementation does not support CPU time
     *         measurement.
     *
     * @see ThreadMXBean#getThreadUserTime(long)
     * @see #getThreadCpuTime
     * @see ThreadMXBean#isThreadCpuTimeSupported
     * @see ThreadMXBean#isThreadCpuTimeEnabled
     * @see ThreadMXBean#setThreadCpuTimeEnabled
     */
    public long[] getThreadUserTime(long[] ids);

    /**
     * Returns an approximation of the total amount of memory, in bytes,
     * allocated in heap memory for the thread of the specified ID.
     * The returned value is an approximation because some Java virtual machine
     * implementations may use object allocation mechanisms that result in a
     * delay between the time an object is allocated and the time its size is
     * recorded.
     * <p>
     * If the thread of the specified ID is not alive or does not exist,
     * this method returns {@code -1}. If thread memory allocation measurement
     * is disabled, this method returns {@code -1}.
     * A thread is alive if it has been started and has not yet died.
     * <p>
     * If thread memory allocation measurement is enabled after the thread has
     * started, the Java virtual machine implementation may choose any time up
     * to and including the time that the capability is enabled as the point
     * where thread memory allocation measurement starts.
     *
     * @param id the thread ID of a thread
     * @return an approximation of the total memory allocated, in bytes, in
     * heap memory for a thread of the specified ID
     * if the thread of the specified ID exists, the thread is alive,
     * and thread memory allocation measurement is enabled;
     * {@code -1} otherwise.
     *
     * @throws IllegalArgumentException if {@code id} {@code <=} {@code 0}.
     * @throws java.lang.UnsupportedOperationException if the Java virtual
     *         machine implementation does not support thread memory allocation
     *         measurement.
     *
     * @see #isThreadAllocatedMemorySupported
     * @see #isThreadAllocatedMemoryEnabled
     * @see #setThreadAllocatedMemoryEnabled
     */
    public long getThreadAllocatedBytes(long id);

    /**
     * Returns an approximation of the total amount of memory, in bytes,
     * allocated in heap memory for each thread whose ID is in the input
     * array {@code ids}.
     * The returned values are approximations because some Java virtual machine
     * implementations may use object allocation mechanisms that result in a
     * delay between the time an object is allocated and the time its size is
     * recorded.
     * <p>
     * This method is equivalent to calling the
     * {@link #getThreadAllocatedBytes(long)}
     * method for each thread ID in the input array {@code ids} and setting the
     * returned value in the corresponding element of the returned array.
     *
     * @param ids an array of thread IDs.
     * @return an array of long values, each of which is an approximation of
     * the total memory allocated, in bytes, in heap memory for the thread
     * whose ID is in the corresponding element of the input array of IDs.
     *
     * @throws NullPointerException if {@code ids} is {@code null}
     * @throws IllegalArgumentException if any element in the input array
     *         {@code ids} is {@code <=} {@code 0}.
     * @throws java.lang.UnsupportedOperationException if the Java virtual
     *         machine implementation does not support thread memory allocation
     *         measurement.
     *
     * @see #getThreadAllocatedBytes(long)
     * @see #isThreadAllocatedMemorySupported
     * @see #isThreadAllocatedMemoryEnabled
     * @see #setThreadAllocatedMemoryEnabled
     */
    public long[] getThreadAllocatedBytes(long[] ids);

    /**
     * Tests if the Java virtual machine implementation supports thread memory
     * allocation measurement.
     *
     * @return
     *   {@code true}
     *     if the Java virtual machine implementation supports thread memory
     *     allocation measurement;
     *   {@code false} otherwise.
     */
    public boolean isThreadAllocatedMemorySupported();

    /**
     * Tests if thread memory allocation measurement is enabled.
     *
     * @return {@code true} if thread memory allocation measurement is enabled;
     *         {@code false} otherwise.
     *
     * @throws java.lang.UnsupportedOperationException if the Java virtual
     *         machine does not support thread memory allocation measurement.
     *
     * @see #isThreadAllocatedMemorySupported
     */
    public boolean isThreadAllocatedMemoryEnabled();

    /**
     * Enables or disables thread memory allocation measurement.  The default
     * is platform dependent.
     *
     * @param enable {@code true} to enable;
     *               {@code false} to disable.
     *
     * @throws java.lang.UnsupportedOperationException if the Java virtual
     *         machine does not support thread memory allocation measurement.
     *
     * @throws java.lang.SecurityException if a security manager
     *         exists and the caller does not have
     *         ManagementPermission("control").
     *
     * @see #isThreadAllocatedMemorySupported
     */
    public void setThreadAllocatedMemoryEnabled(boolean enable);

    /**
     * Returns the thread info for each thread whose ID
     * is in the input array <tt>ids</tt>,
     * with stack trace of the specified maximum number of elements
     * and synchronization information.
     * If <tt>maxDepth == 0</tt>, no stack trace of the thread
     * will be dumped.
     *
     * <p>
     * This method obtains a snapshot of the thread information
     * for each thread including:
     * <ul>
     *    <li>stack trace of the specified maximum number of elements,</li>
     *    <li>the object monitors currently locked by the thread
     *        if <tt>lockedMonitors</tt> is <tt>true</tt>, and</li>
     *    <li>the <a href="{@docRoot}/../api/java/lang/management/LockInfo.html#OwnableSynchronizer">
     *        ownable synchronizers</a> currently locked by the thread
     *        if <tt>lockedSynchronizers</tt> is <tt>true</tt>.</li>
     * </ul>
     * <p>
     * This method returns an array of the <tt>ThreadInfo</tt> objects,
     * each is the thread information about the thread with the same index
     * as in the <tt>ids</tt> array.
     * If a thread of the given ID is not alive or does not exist,
     * <tt>null</tt> will be set in the corresponding element
     * in the returned array.  A thread is alive if
     * it has been started and has not yet died.
     * <p>
     * If a thread does not lock any object monitor or <tt>lockedMonitors</tt>
     * is <tt>false</tt>, the returned <tt>ThreadInfo</tt> object will have an
     * empty <tt>MonitorInfo</tt> array.  Similarly, if a thread does not
     * lock any synchronizer or <tt>lockedSynchronizers</tt> is <tt>false</tt>,
     * the returned <tt>ThreadInfo</tt> object
     * will have an empty <tt>LockInfo</tt> array.
     *
     * <p>
     * When both <tt>lockedMonitors</tt> and <tt>lockedSynchronizers</tt>
     * parameters are <tt>false</tt>, it is equivalent to calling:
     * <blockquote><pre>
     *     {@link #getThreadInfo(long[], int)  getThreadInfo(ids, maxDepth)}
     * </pre></blockquote>
     *
     * <p>
     * This method is designed for troubleshooting use, but not for
     * synchronization control.  It might be an expensive operation.
     *
     * <p>
     * <b>MBeanServer access</b>:<br>
     * The mapped type of <tt>ThreadInfo</tt> is
     * <tt>CompositeData</tt> with attributes as specified in the
     * {@link ThreadInfo#from ThreadInfo.from} method.
     *
     * @implSpec The default implementation throws
     * <tt>UnsupportedOperationException</tt>.
     *
     * @param  ids an array of thread IDs.
     * @param  lockedMonitors if <tt>true</tt>, retrieves all locked monitors.
     * @param  lockedSynchronizers if <tt>true</tt>, retrieves all locked
     *             ownable synchronizers.
     * @param  maxDepth indicates the maximum number of
     * {@link StackTraceElement} to be retrieved from the stack trace.
     *
     * @return an array of the {@link ThreadInfo} objects, each containing
     * information about a thread whose ID is in the corresponding
     * element of the input array of IDs.
     *
     * @throws IllegalArgumentException if <tt>maxDepth</tt> is negative.
     * @throws java.lang.SecurityException if a security manager
     *         exists and the caller does not have
     *         ManagementPermission("monitor").
     * @throws java.lang.UnsupportedOperationException
     *         <ul>
     *           <li>if <tt>lockedMonitors</tt> is <tt>true</tt> but
     *               the Java virtual machine does not support monitoring
     *               of {@linkplain #isObjectMonitorUsageSupported
     *               object monitor usage}; or</li>
     *           <li>if <tt>lockedSynchronizers</tt> is <tt>true</tt> but
     *               the Java virtual machine does not support monitoring
     *               of {@linkplain #isSynchronizerUsageSupported
     *               ownable synchronizer usage}.</li>
     *         </ul>
     *
     * @see #isObjectMonitorUsageSupported
     * @see #isSynchronizerUsageSupported
     *
     * @since 8u282
     */

    public default ThreadInfo[] getThreadInfo(long[] ids, boolean lockedMonitors,
                                              boolean lockedSynchronizers, int maxDepth) {
        throw new UnsupportedOperationException();
    }

    /**
     * Returns the thread info for all live threads
     * with stack trace of the specified maximum number of elements
     * and synchronization information.
     * if <tt>maxDepth == 0</tt>, no stack trace of the thread
     * will be dumped.
     * Some threads included in the returned array
     * may have been terminated when this method returns.
     *
     * <p>
     * This method returns an array of {@link ThreadInfo} objects
     * as specified in the {@link #getThreadInfo(long[], boolean, boolean, int)}
     * method.
     *
     * @implSpec The default implementation throws
     * <tt>UnsupportedOperationException</tt>.
     *
     * @param  lockedMonitors if <tt>true</tt>, dump all locked monitors.
     * @param  lockedSynchronizers if <tt>true</tt>, dump all locked
     *             ownable synchronizers.
     * @param  maxDepth indicates the maximum number of
     * {@link StackTraceElement} to be retrieved from the stack trace.
     *
     * @return an array of {@link ThreadInfo} for all live threads.
     *
     * @throws IllegalArgumentException if <tt>maxDepth</tt> is negative.
     * @throws java.lang.SecurityException if a security manager
     *         exists and the caller does not have
     *         ManagementPermission("monitor").
     * @throws java.lang.UnsupportedOperationException
     *         <ul>
     *           <li>if <tt>lockedMonitors</tt> is <tt>true</tt> but
     *               the Java virtual machine does not support monitoring
     *               of {@linkplain #isObjectMonitorUsageSupported
     *               object monitor usage}; or</li>
     *           <li>if <tt>lockedSynchronizers</tt> is <tt>true</tt> but
     *               the Java virtual machine does not support monitoring
     *               of {@linkplain #isSynchronizerUsageSupported
     *               ownable synchronizer usage}.</li>
     *         </ul>
     *
     * @see #isObjectMonitorUsageSupported
     * @see #isSynchronizerUsageSupported
     *
     * @since 8u282
     */
    public default ThreadInfo[] dumpAllThreads(boolean lockedMonitors,
                                               boolean lockedSynchronizers, int maxDepth) {
        throw new UnsupportedOperationException();
    }
}
