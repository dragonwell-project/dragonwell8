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

import java.io.IOException;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;


// Make a udp socket channel look like a datagram socket.

public class WispUdpSocketImpl {

    public WispUdpSocketImpl(DatagramSocket so) {
        throw new UnsupportedOperationException();
    }

    public void bind(SocketAddress local) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void connect(InetAddress address, int port) {
        throw new UnsupportedOperationException();
    }

    public void connect(SocketAddress remote) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void disconnect() {
        throw new UnsupportedOperationException();
    }

    public boolean isBound() {
        throw new UnsupportedOperationException();
    }

    public boolean isConnected() {
        throw new UnsupportedOperationException();
    }

    public InetAddress getInetAddress() {
        throw new UnsupportedOperationException();
    }

    public int getPort() {
        throw new UnsupportedOperationException();
    }

    public void send(DatagramPacket p) throws IOException {
        throw new UnsupportedOperationException();
    }

    private SocketAddress receive(ByteBuffer bb) throws IOException {
        throw new UnsupportedOperationException();
    }

    public int receive(DatagramPacket p, int bufLen) throws IOException {
        throw new UnsupportedOperationException();
    }

    public InetAddress getLocalAddress() {
        throw new UnsupportedOperationException();
    }

    public int getLocalPort() {
        throw new UnsupportedOperationException();
    }

    public void setSoTimeout(int timeout) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public int getSoTimeout() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void setSendBufferSize(int size) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public int getSendBufferSize() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void setReceiveBufferSize(int size) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public int getReceiveBufferSize() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void setReuseAddress(boolean on) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public boolean getReuseAddress() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void setBroadcast(boolean on) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public boolean getBroadcast() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void setTrafficClass(int tc) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public int getTrafficClass() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void close() {
        throw new UnsupportedOperationException();
    }

    public boolean isClosed() {
        throw new UnsupportedOperationException();
    }

    public DatagramChannel getChannel() {
        throw new UnsupportedOperationException();
    }

}
