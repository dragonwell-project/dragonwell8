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

import sun.misc.SharedSecrets;
import sun.net.ext.RdmaSocketOptions;
import sun.nio.ch.IOUtil;
import sun.nio.ch.Net;
import sun.nio.ch.NetAccess;

import java.io.FileDescriptor;
import java.io.IOException;
import java.net.*;
import java.nio.channels.UnsupportedAddressTypeException;
import java.security.AccessController;
import java.security.PrivilegedAction;

public class RdmaNet {
    private static final NetAccess NET_ACCESS = SharedSecrets.getNetAccess();

    private RdmaNet() { }

    static final ProtocolFamily UNSPEC = new ProtocolFamily() {
        public String name() {
            return "UNSPEC";
        }
    };

    static boolean isReusePortAvailable() {
        return false;
    }

    private static volatile boolean checkedRdma;
    private static volatile boolean isRdmaAvailable;

    public static boolean isRdmaAvailable() {
        if (!checkedRdma) {
            isRdmaAvailable = isRdmaAvailable0();
            checkedRdma = true;
        }
        return isRdmaAvailable;
    }

    private static native boolean isRdmaAvailable0();

    static InetSocketAddress checkAddress(SocketAddress sa, ProtocolFamily family) {
        InetSocketAddress isa = Net.checkAddress(sa);
        if (family == StandardProtocolFamily.INET) {
            InetAddress addr = isa.getAddress();
            if (!(addr instanceof Inet4Address))
                throw new UnsupportedAddressTypeException();
        }
        if (family == StandardProtocolFamily.INET6) {
            InetAddress addr = isa.getAddress();
            if (!(addr instanceof Inet6Address))
                throw new UnsupportedAddressTypeException();
        }
        return isa;
    }

    // -- Socket options

    static final RdmaSocketOptions rdmaOptions =
            RdmaSocketOptions.getInstance();

    static void setSocketOption(FileDescriptor fd, ProtocolFamily family,
            SocketOption<?> name, Object value) throws IOException
    {
        if (value == null)
            throw new IllegalArgumentException("Invalid option value");

        Class<?> type = name.type();

        if (rdmaOptions.isOptionSupported(name)) {
            rdmaOptions.setOption(fd, name, value);
            return;
        }

        if (type != Integer.class && type != Boolean.class)
            throw new AssertionError("Should not reach here");

        if (name == StandardSocketOptions.SO_RCVBUF ||
            name == StandardSocketOptions.SO_SNDBUF)
        {
            int i = ((Integer)value).intValue();
            if (i < 0)
                throw new IllegalArgumentException(
                    "Invalid send/receive buffer size");
        }

        RdmaOptionKey key = RdmaSocketOptionRegistry.findOption(name, family);
        if (key == null)
            throw new AssertionError("Option not found");

        int arg;
        int maxValue = 1024 * 1024 * 1024 - 1;
        if (type == Integer.class) {
            arg = ((Integer)value).intValue();
            if (arg > maxValue)
                arg = maxValue;
        } else {
            boolean b = ((Boolean)value).booleanValue();
            arg = (b) ? 1 : 0;
        }

        boolean mayNeedConversion = (family == UNSPEC);
        setIntOption0(fd, mayNeedConversion, key.level(),
                      key.name(), arg);
    }

    static Object getSocketOption(FileDescriptor fd, ProtocolFamily family,
            SocketOption<?> name) throws IOException
    {
        Class<?> type = name.type();

        if (rdmaOptions.isOptionSupported(name)) {
            return rdmaOptions.getOption(fd, name);
        }

        if (type != Integer.class && type != Boolean.class)
            throw new AssertionError("Should not reach here");

        RdmaOptionKey key = RdmaSocketOptionRegistry.findOption(name, family);
        if (key == null)
            throw new AssertionError("Option not found");

        boolean mayNeedConversion = (family == UNSPEC);
        int value = getIntOption0(fd, mayNeedConversion, key.level(),
                                  key.name());

        if (type == Integer.class) {
            return Integer.valueOf(value);
        } else {
            return (value == 0) ? Boolean.FALSE : Boolean.TRUE;
        }
    }

    // -- Socket operations --
    static FileDescriptor socket(ProtocolFamily family, boolean stream)
            throws IOException {
        boolean preferIPv6 = NET_ACCESS.isIPv6Available() &&
                (family != StandardProtocolFamily.INET);
        return IOUtil.newFD(socket0(preferIPv6, stream, false));
    }

    static FileDescriptor serverSocket(ProtocolFamily family, boolean stream)
            throws IOException {
        boolean preferIPv6 = NET_ACCESS.isIPv6Available() &&
                (family != StandardProtocolFamily.INET);
        return IOUtil.newFD(socket0(preferIPv6, stream, true));
    }

    private static native int socket0(boolean preferIPv6, boolean stream,
            boolean reuse);
    static void bind(ProtocolFamily family, FileDescriptor fd,
            InetAddress addr, int port) throws IOException
    {
        boolean preferIPv6 = NET_ACCESS.isIPv6Available() &&
                (family != StandardProtocolFamily.INET);
        bind0(fd, preferIPv6, addr, port);
    }

    private static native void bind0(FileDescriptor fd, boolean preferIPv6,
            InetAddress addr, int port) throws IOException;

    static native void listen(FileDescriptor fd, int backlog)
            throws IOException;

    static int connect(FileDescriptor fd, InetAddress remote, int remotePort)
            throws IOException
    {
        return connect(UNSPEC, fd, remote, remotePort);
    }

    static int connect(ProtocolFamily family, FileDescriptor fd,
            InetAddress remote, int remotePort) throws IOException
    {
        boolean preferIPv6 = NET_ACCESS.isIPv6Available() &&
                (family != StandardProtocolFamily.INET);
        return connect0(preferIPv6, fd, remote, remotePort);
    }

    public static InetSocketAddress localAddress(FileDescriptor fd)
            throws IOException
    {
        return new InetSocketAddress(localInetAddress(fd), localPort(fd));
    }

    private static native int connect0(boolean preferIPv6, FileDescriptor fd,
            InetAddress remote, int remotePort) throws IOException;

    static native void shutdown(FileDescriptor fd, int how) throws IOException;

    private static native int localPort(FileDescriptor fd)
            throws IOException;

    private static native InetAddress localInetAddress(FileDescriptor fd)
            throws IOException;

    private static native int getIntOption0(FileDescriptor fd,
            boolean mayNeedConversion, int level, int opt) throws IOException;

    private static native void setIntOption0(FileDescriptor fd,
            boolean mayNeedConversion, int level, int opt, int arg)
            throws IOException;

    static native int poll(FileDescriptor fd, int events, long timeout)
            throws IOException;

    public static native void configureBlocking(FileDescriptor fd,
            boolean blocking);

    private static native void initIDs();

    static {
        AccessController.doPrivileged(
            new PrivilegedAction<Object>() {
                public Void run() {
                    System.loadLibrary("extnet");
                    return null;
                }
            });
        IOUtil.load();
        initIDs();
    }
}
