/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

package sun.net.ext;

import java.io.FileDescriptor;
import java.net.SocketException;
import java.net.SocketOption;
import java.util.Collections;
import java.util.Set;

/**
 * Defines the infrastructure to support RDMA socket options.
 *
 * RDMA socket options are accessed through the jdk.net API, which is in
 * the jdk.net module.
 */
public abstract class RdmaSocketOptions {

    private final Set<SocketOption<?>> options;

    /** Tells whether or not the option is supported. */
    public final boolean isOptionSupported(SocketOption<?> option) {
        return options().contains(option);
    }

    /** Return the, possibly empty, set of RDMA socket options available. */
    public final Set<SocketOption<?>> options() { return options; }

    /** Sets the value of a socket option, for the given socket. */
    public abstract void setOption(FileDescriptor fd, SocketOption<?> option,
            Object value) throws SocketException;

    /** Returns the value of a socket option, for the given socket. */
    public abstract Object getOption(FileDescriptor fd, SocketOption<?> option)
            throws SocketException;

    protected RdmaSocketOptions(Set<SocketOption<?>> options) {
        this.options = options;
    }

    private static volatile RdmaSocketOptions instance;

    public static final RdmaSocketOptions getInstance() { return instance; }

    /** Registers support for RDMA socket options. Invoked by the jdk.net module. */
    public static final void register(RdmaSocketOptions rdmaOptions) {
        if (instance != null)
            throw new InternalError("Attempting to reregister RDMA options");

        instance = rdmaOptions;
    }

    static {
        try {
            // If the class is present, it will be initialized which
            // triggers registration of the RDMA socket options.
            Class<?> c = Class.forName("jdk.net.RdmaSocketOptions");
        } catch (ClassNotFoundException e) {
            // the jdk.net module is not present => no RDMA socket options
            instance = new NoRdmaSocketOptions();
        }
    }

    static final class NoRdmaSocketOptions extends RdmaSocketOptions {

        NoRdmaSocketOptions() {
            super(Collections.<SocketOption<?>>emptySet());
        }

        @Override
        public void setOption(FileDescriptor fd, SocketOption<?> option,
                Object value) throws SocketException
        {
            throw new UnsupportedOperationException(
                    "no RDMA options: " + option.name());
        }

        @Override
        public Object getOption(FileDescriptor fd, SocketOption<?> option)
            throws SocketException
        {
            throw new UnsupportedOperationException(
                    "no RDMA options: " + option.name());
        }
    }
}
