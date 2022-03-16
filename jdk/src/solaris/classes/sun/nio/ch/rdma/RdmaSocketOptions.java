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

package sun.nio.ch.rdma;

import jdk.net.NetworkPermission;
import sun.misc.JavaIOFileDescriptorAccess;
import sun.misc.SharedSecrets;

import java.io.FileDescriptor;
import java.lang.annotation.Native;
import java.net.SocketException;
import java.net.SocketOption;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Collections;
import java.util.Set;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * Defines socket options specific to RDMA-based TCP sockets and channels.
 *
 * @since 12
 */
public final class RdmaSocketOptions {

    private static class RdmaSocketOption<T> implements SocketOption<T> {
        private final String name;
        private final Class<T> type;
        RdmaSocketOption(String name, Class<T> type) {
            this.name = name;
            this.type = type;
        }
        @Override public String name() { return name; }
        @Override public Class<T> type() { return type; }
        @Override public String toString() { return name; }
    }

    private RdmaSocketOptions() { }

    /**
     * The integer size of the underlying RDMA send queue used by the
     * platform for network I/O.
     *
     * The initial/default size of the RDMA send queue and the range of
     * allowable values is system and device dependent although a negative
     * size is not allowed.
     *
     * <p> An implementation allows this socket option to be set before the
     * socket is bound or connected. Changing the value of this socket option
     * after the socket is bound or connected has no effect.
     *
     * Valid for RDMA-based TCP sockets.
     */
    public static final SocketOption<Integer> RDMA_SQSIZE = new
        RdmaSocketOption<Integer>("RDMA_SQSIZE", Integer.class);

    /**
     * The integer size of the underlying RDMA receive queue used by the
     * platform for network I/O.
     *
     * The initial/default size of the RDMA receive queue and the range of
     * allowable values is system and device dependent although a negative
     * size is not allowed.
     *
     * <p> An implementation allows this socket option to be set before the
     * socket is bound or connected. Changing the value of this socket option
     * after the socket is bound or connected has no effect.
     *
     * Valid for RDMA-based TCP sockets.
     */
    public static final SocketOption<Integer> RDMA_RQSIZE = new
        RdmaSocketOption<Integer>("RDMA_RQSIZE", Integer.class);

    /**
     * The integer size of the underlying RDMA inline data used by the
     * platform for network I/O.
     *
     * The initial/default size of the RDMA inline data and the range of
     * allowable values is system and device dependent although a negative
     * size is not allowed.
     *
     * <p> An implementation allows this socket option to be set before the
     * socket is bound or connected. Changing the value of this socket option
     * after the socket is bound or connected has no effect.
     *
     * Valid for RDMA-based TCP sockets.
     */
    public static final SocketOption<Integer> RDMA_INLINE = new
        RdmaSocketOption<Integer>("RDMA_INLINE", Integer.class);

    @Native private static final int SQSIZE = 0x3001;

    @Native private static final int RQSIZE = 0x3002;

    @Native private static final int INLINE = 0x3003;

    private static final PlatformRdmaSocketOptions platformRdmaSocketOptions =
            PlatformRdmaSocketOptions.get();

    private static final boolean rdmaSocketSupported =
            platformRdmaSocketOptions.rdmaSocketSupported();

    private static final Set<SocketOption<?>> rdmaOptions = options();

    static Set<SocketOption<?>> options() {
        if (rdmaSocketSupported)
            return Stream.of(RDMA_SQSIZE, RDMA_RQSIZE, RDMA_INLINE).collect(Collectors.toSet());
        else
            return Collections.<SocketOption<?>>emptySet();
    }

    static {
        sun.net.ext.RdmaSocketOptions.register(
                new sun.net.ext.RdmaSocketOptions(rdmaOptions) {

            @Override
            public void setOption(FileDescriptor fd,
                    SocketOption<?> option, Object value)
                    throws SocketException {
                SecurityManager sm = System.getSecurityManager();
                if (sm != null)
                    sm.checkPermission(new NetworkPermission("setOption."
                            + option.name()));

                if (fd == null || !fd.valid())
                    throw new SocketException("socket closed");

                if ((option == RDMA_SQSIZE) || (option == RDMA_RQSIZE)
                    || (option == RDMA_INLINE)) {
                    assert rdmaSocketSupported;

                    int val;
                    int opt;
                    if (option == RDMA_SQSIZE) {
                        if (value == null || (!(value instanceof Integer)))
                            throw new SocketException(
                                    "Bad parameter for RDMA_SQSIZE");
                        val = ((Integer) value).intValue();
                        opt = SQSIZE;
                        if (val < 0)
                            throw new IllegalArgumentException(
                                    "send queue size < 0");
                    } else if (option == RDMA_RQSIZE) {
                        if (value == null || (!(value instanceof Integer)))
                            throw new SocketException(
                                    "Bad parameter for RDMA_RQSIZE");
                        val = ((Integer) value).intValue();
                        opt = RQSIZE;
                        if (val < 0)
                            throw new IllegalArgumentException(
                                    "receive queue size < 0");
                    } else if (option == RDMA_INLINE) {
                        if (value == null || (!(value instanceof Integer)))
                            throw new SocketException(
                                    "Bad parameter for RDMA_INLINE");
                        val = ((Integer) value).intValue();
                        opt = INLINE;
                        if (val < 0)
                            throw new IllegalArgumentException(
                                    "inline size < 0");
                    } else {
                        throw new SocketException(
                                "unrecognized RDMA socket option: " + option);
                    }

                    setSockOpt(fd, opt, val);

                } else {
                    throw new InternalError("Unexpected option " + option);
                }
            }

            @Override
            public Object getOption(FileDescriptor fd,
                    SocketOption<?> option) throws SocketException {
                SecurityManager sm = System.getSecurityManager();
                if (sm != null)
                    sm.checkPermission(new NetworkPermission("getOption."
                            + option.name()));

                if (fd == null || !fd.valid())
                    throw new SocketException("socket closed");

                if ((option == RDMA_SQSIZE) || (option == RDMA_RQSIZE)
                    || (option == RDMA_INLINE)) {
                    assert rdmaSocketSupported;
                    int opt;
                    if (option == RDMA_SQSIZE) {
                        opt = SQSIZE;
                    } else if (option == RDMA_RQSIZE) {
                        opt = RQSIZE;
                    } else {
                        opt = INLINE;
                    }
                    return getSockOpt(fd, opt);
                } else {
                    throw new InternalError("Unexpected option " + option);
                }
            }
        });
    }

    private static final JavaIOFileDescriptorAccess fdAccess =
            SharedSecrets.getJavaIOFileDescriptorAccess();

    private static void setSockOpt(FileDescriptor fd, int opt, int val)
            throws SocketException {
        platformRdmaSocketOptions.setSockOpt(fdAccess.get(fd), opt, val);
    }

    private static int getSockOpt(FileDescriptor fd, int opt)
            throws SocketException {
        return platformRdmaSocketOptions.getSockOpt(fdAccess.get(fd), opt);
    }

    static class PlatformRdmaSocketOptions {

        protected PlatformRdmaSocketOptions() {}

        @SuppressWarnings("unchecked")
        private static PlatformRdmaSocketOptions newInstance(String cn) {
            Class<PlatformRdmaSocketOptions> c;
            try {
                c = (Class<PlatformRdmaSocketOptions>)Class.forName(cn);
                return c.getConstructor(new Class<?>[] { }).newInstance();
            } catch (ReflectiveOperationException x) {
                if (x.getCause() instanceof UnsupportedOperationException)
                    throw (UnsupportedOperationException)x.getCause();
                throw new AssertionError(x);
            }
        }

        private static PlatformRdmaSocketOptions create() {
            String osname = AccessController.doPrivileged(
                    new PrivilegedAction<String>() {
                        public String run() {
                            return System.getProperty("os.name");
                        }
                    });
            if ("Linux".equals(osname)) {
                try {
                    return newInstance("jdk.net.LinuxRdmaSocketOptions");
                } catch (UnsupportedOperationException uoe) {
                    return new PlatformRdmaSocketOptions();
                }
            }
            return new PlatformRdmaSocketOptions();
        }

        private static final PlatformRdmaSocketOptions instance = create();

        static PlatformRdmaSocketOptions get() {
            return instance;
        }

        void setSockOpt(int fd, int opt, int value) throws SocketException {
            throw new UnsupportedOperationException(
                    "unsupported socket option");
        }

        int getSockOpt(int fd, int opt) throws SocketException {
            throw new UnsupportedOperationException(
                    "unsupported socket option");
        }

        boolean rdmaSocketSupported() {
            return false;
        }
    }
}
