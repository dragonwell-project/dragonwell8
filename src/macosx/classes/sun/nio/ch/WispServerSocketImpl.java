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
import java.net.InetAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.net.SocketException;
import java.nio.channels.ServerSocketChannel;


public class WispServerSocketImpl
{

    public WispServerSocketImpl() {
    }

    public void bind(SocketAddress local) throws IOException {
        throw new UnsupportedOperationException();
    }

    public void bind(SocketAddress local, int backlog) throws IOException {
        throw new UnsupportedOperationException();
    }

    public InetAddress getInetAddress() {
        throw new UnsupportedOperationException();
    }

    public int getLocalPort() {
        throw new UnsupportedOperationException();
    }

    public Socket accept() throws IOException {
        throw new UnsupportedOperationException();
    }

    public void close() throws IOException {
        throw new UnsupportedOperationException();
    }

    public ServerSocketChannel getChannel() {
        throw new UnsupportedOperationException();
    }

    public boolean isBound() {
        throw new UnsupportedOperationException();
    }

    public boolean isClosed() {
        throw new UnsupportedOperationException();
    }

    public void setSoTimeout(int timeout) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public int getSoTimeout() throws IOException {
        throw new UnsupportedOperationException();
    }

    public void setReuseAddress(boolean on) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public boolean getReuseAddress() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public String toString() {
        throw new UnsupportedOperationException();
    }

    public void setReceiveBufferSize(int size) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public int getReceiveBufferSize() throws SocketException {
        throw new UnsupportedOperationException();
    }

}
