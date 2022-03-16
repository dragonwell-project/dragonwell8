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

import java.io.IOException;
import java.net.ProtocolFamily;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.StandardProtocolFamily;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.channels.spi.SelectorProvider;
import java.util.Objects;

/**
 * Factory methods for creating RDMA-based TCP sockets and channels.
 *
 * <p>The {@link #openSocket(ProtocolFamily family) openSocket} and {@link
 * #openServerSocket(ProtocolFamily family) openServerSocket} methods
 * create RDMA-based TCP sockets.
 *
 * <p>The {@link #openSelector() openSelector}, {@link
 * #openSocketChannel(ProtocolFamily family) openSocketChannel}, and {@link
 * #openServerSocketChannel(ProtocolFamily family) openServerSocketChannel}
 * methods create selectors and selectable channels for use with RDMA sockets.
 * These objects are created by a {@link SelectorProvider
 * SelectorProvider} that is not the default {@code SelectorProvider}.
 * Consequently, selectable channels to RDMA sockets may not be multiplexed
 * with selectable channels created by the default selector provider. Its
 * selector provider does not support datagram channels and pipes.
 * The {@link SelectorProvider#openDatagramChannel
 * openDatagramChannel} and
 * {@link SelectorProvider#openPipe openPipe} methods
 * throw {@link UnsupportedOperationException
 * UnsupportedOperationException}.
 *
 * @since 12
 */
public class RdmaSockets {

    private RdmaSockets() {}

    /**
     * Creates an unbound and unconnected RDMA socket.
     *
     * <p> An RDMA socket supports the same socket options that {@link
     * Socket java.net.Socket} defines. In addition, it supports the
     * socket options specified by {@link RdmaSocketOptions}.
     *
     * <p> When binding the socket to a local address, or invoking {@code
     * connect} to connect the socket, the socket address specified to those
     * methods must correspond to the protocol family specified here.
     *
     * @param   family
     *          The protocol family
     *
     * @throws IOException
     *         If an I/O error occurs
     * @throws NullPointerException
     *         If name is {@code null}
     * @throws UnsupportedOperationException
     *         If RDMA sockets are not supported on this platform or if the
     *         specified protocol family is not supported. For example, if
     *         the parameter is {@link StandardProtocolFamily#INET6
     *         StandardProtocolFamily.INET6} but IPv6 is not enabled on the
     *         platform.
     */
    public static Socket openSocket(ProtocolFamily family) throws IOException {
        Objects.requireNonNull("protocol family is null");
        return RdmaPollSelectorProvider.provider().openSocketChannel().socket();
    }

    /**
     * Creates an unbound RDMA server socket.
     *
     * <p> An RDMA socket supports the same socket options that {@link
     * ServerSocket java.net.ServerSocket} defines.
     *
     * <p> When binding the socket to an address, the socket address specified
     * to the {@code bind} method must correspond to the protocol family
     * specified here.
     *
     * @param   family
     *          The protocol family
     *
     * @throws IOException
     *         If an I/O error occurs
     * @throws NullPointerException
     *         If name is {@code null}
     * @throws UnsupportedOperationException
     *         If RDMA sockets are not supported on this platform or if the
     *         specified protocol family is not supported. For example, if
     *         the parameter is {@link StandardProtocolFamily#INET6
     *         StandardProtocolFamily.INET6} but IPv6 is not enabled on the
     *         platform.
     */
    public static ServerSocket openServerSocket(ProtocolFamily family)
            throws IOException {
        Objects.requireNonNull("protocol family is null");
        return RdmaPollSelectorProvider.provider().openServerSocketChannel().socket();
    }

    /**
     * Opens a socket channel to an RDMA socket. A newly created socket channel
     * is {@link SocketChannel#isOpen() open}, not yet bound to a {@link
     * SocketChannel#getLocalAddress() local address}, and not yet
     * {@link SocketChannel#isConnected() connected}.
     *
     * <p> A socket channel to an RDMA socket supports all of the socket options
     * specified by {@code SocketChannel}. In addition, it supports the
     * socket options specified by {@link RdmaSocketOptions}.
     *
     * <p> When binding the channel's socket to a local address, or invoking
     * {@code connect} to connect channel's socket, the socket address specified
     * to those methods must correspond to the protocol family specified here.
     *
     * @param   family
     *          The protocol family
     *
     * @throws IOException
     *         If an I/O error occurs
     * @throws NullPointerException
     *         If name is {@code null}
     * @throws UnsupportedOperationException
     *         If RDMA sockets are not supported on this platform or if the
     *         specified protocol family is not supported. For example, if
     *         the parameter is {@link StandardProtocolFamily#INET6
     *         StandardProtocolFamily.INET6} but IPv6 is not enabled on the
     *         platform.
     */
    public static SocketChannel openSocketChannel(ProtocolFamily family)
            throws IOException {
        Objects.requireNonNull("protocol family is null");
        SelectorProvider provider = RdmaPollSelectorProvider.provider();
        return ((RdmaPollSelectorProvider)provider).openSocketChannel(family);
    }

    /**
     * Opens a server socket channel to an RDMA socket. A newly created socket
     * channel is {@link SocketChannel#isOpen() open} but not yet bound to a
     * {@link SocketChannel#getLocalAddress() local address}.
     *
     * <p> When binding the channel's socket to an address, the socket address
     * specified to the {@code bind} method must correspond to the protocol
     * family specified here.
     *
     * @param   family
     *          The protocol family
     *
     * @throws IOException
     *         If an I/O error occurs
     * @throws NullPointerException
     *         If name is {@code null}
     * @throws UnsupportedOperationException
     *         If RDMA sockets are not supported on this platform or if the
     *         specified protocol family is not supported. For example, if
     *         the parameter is {@link StandardProtocolFamily#INET6
     *         StandardProtocolFamily.INET6} but IPv6 is not enabled on the
     *         platform.
     */
    public static ServerSocketChannel openServerSocketChannel(
            ProtocolFamily family) throws IOException {
        Objects.requireNonNull("protocol family is null");
        SelectorProvider provider = RdmaPollSelectorProvider.provider();
        return ((RdmaPollSelectorProvider)provider)
                .openServerSocketChannel(family);
    }

    /**
     * Opens a selector to multiplex selectable channels to RDMA sockets.
     *
     * @throws IOException
     *         If an I/O error occurs
     * @throws UnsupportedOperationException
     *         If RDMA sockets are not supported on this platform
     */
    public static Selector openSelector() throws IOException {
        SelectorProvider provider = RdmaPollSelectorProvider.provider();
        return provider.openSelector();
    }
}
