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
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.net.SocketException;
import java.nio.channels.SocketChannel;


public class WispSocketImpl
{

    public WispSocketImpl(Socket so) {
        throw new UnsupportedOperationException();
    }

    public WispSocketImpl(SocketChannel sc, Socket so) {
        throw new UnsupportedOperationException();
    }

    public SocketChannel getChannel() {
        throw new UnsupportedOperationException();
    }

    public void connect(SocketAddress remote) throws IOException {
        throw new UnsupportedOperationException();
    }

    public void connect(SocketAddress remote, int timeout) throws IOException {
        throw new UnsupportedOperationException();
    }

    public void bind(SocketAddress local) throws IOException {
        throw new UnsupportedOperationException();
    }

    public InetAddress getInetAddress() {
        throw new UnsupportedOperationException();
    }

    public InetAddress getLocalAddress() {
        throw new UnsupportedOperationException();
    }

    public int getPort() {
        throw new UnsupportedOperationException();
    }

    public int getLocalPort() {
        throw new UnsupportedOperationException();
    }

    public InputStream getInputStream() throws IOException {
        throw new UnsupportedOperationException();
    }

    public OutputStream getOutputStream() throws IOException {
        throw new UnsupportedOperationException();
    }

    public void setTcpNoDelay(boolean on) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public boolean getTcpNoDelay() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void setSoLinger(boolean on, int linger) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public int getSoLinger() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void sendUrgentData(int data) throws IOException {
        throw new UnsupportedOperationException();
    }

    public void setOOBInline(boolean on) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public boolean getOOBInline() throws SocketException {
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

    public void setKeepAlive(boolean on) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public boolean getKeepAlive() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void setTrafficClass(int tc) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public int getTrafficClass() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void setReuseAddress(boolean on) throws SocketException {
        throw new UnsupportedOperationException();
    }

    public boolean getReuseAddress() throws SocketException {
        throw new UnsupportedOperationException();
    }

    public void close() throws IOException {
        throw new UnsupportedOperationException();
    }

    public void shutdownInput() throws IOException {
        throw new UnsupportedOperationException();
    }

    public void shutdownOutput() throws IOException {
        throw new UnsupportedOperationException();
    }

    public String toString() {
        throw new UnsupportedOperationException();
    }

    public boolean isConnected() {
        throw new UnsupportedOperationException();
    }

    public boolean isBound() {
        throw new UnsupportedOperationException();
    }

    public boolean isClosed() {
        throw new UnsupportedOperationException();
    }

    public boolean isInputShutdown() {
        throw new UnsupportedOperationException();
    }

    public boolean isOutputShutdown() {
        throw new UnsupportedOperationException();
    }

}
