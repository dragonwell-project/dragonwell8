/*
 * Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.
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

/**
 * IOUtils: A collection of IO-related public static methods.
 */

package sun.misc;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;

import java.util.Arrays;
import java.util.Objects;

public class IOUtils {

    private static final int DEFAULT_BUFFER_SIZE = 8192;

    /**
     * The maximum size of array to allocate.
     * Some VMs reserve some header words in an array.
     * Attempts to allocate larger arrays may result in
     * OutOfMemoryError: Requested array size exceeds VM limit
     */
    private static final int MAX_BUFFER_SIZE = Integer.MAX_VALUE - 8;

    /**
     * Read up to {@code length} of bytes from {@code in}
     * until EOF is detected.
     * @param is input stream, must not be null
     * @param length number of bytes to read, -1 or Integer.MAX_VALUE means
     *        read as much as possible
     * @param readAll if true, an EOFException will be thrown if not enough
     *        bytes are read. Ignored when length is -1 or Integer.MAX_VALUE
     * @return bytes read
     * @throws IOException Any IO error or a premature EOF is detected
     */
    public static byte[] readFully(InputStream is, int length, boolean readAll)
            throws IOException {
        byte[] output = {};
        if (length == -1) length = Integer.MAX_VALUE;
        int pos = 0;
        while (pos < length) {
            int bytesToRead;
            if (pos >= output.length) { // Only expand when there's no room
                bytesToRead = Math.min(length - pos, output.length + 1024);
                if (output.length < pos + bytesToRead) {
                    output = Arrays.copyOf(output, pos + bytesToRead);
                }
            } else {
                bytesToRead = output.length - pos;
            }
            int cc = is.read(output, pos, bytesToRead);
            if (cc < 0) {
                if (readAll && length != Integer.MAX_VALUE) {
                    throw new EOFException("Detect premature EOF");
                } else {
                    if (output.length != pos) {
                        output = Arrays.copyOf(output, pos);
                    }
                    break;
                }
            }
            pos += cc;
        }
        return output;
    }

    /**
     * Read {@code length} of bytes from {@code in}. An exception is
     * thrown if there are not enough bytes in the stream.
     *
     * @param is input stream, must not be null
     * @param length number of bytes to read, must not be negative
     * @return bytes read
     * @throws IOException if any IO error or a premature EOF is detected, or
     *      if {@code length} is negative since this length is usually also
     *      read from {@code is}.
     */
    public static byte[] readNBytes(InputStream is, int length) throws IOException {
        if (length < 0) {
            throw new IOException("length cannot be negative: " + length);
        }
        return readFully(is, length, true);
    }

    /**
     * Reads all remaining bytes from the input stream. This method blocks until
     * all remaining bytes have been read and end of stream is detected, or an
     * exception is thrown. This method does not close the input stream.
     *
     * <p> When this stream reaches end of stream, further invocations of this
     * method will return an empty byte array.
     *
     * <p> Note that this method is intended for simple cases where it is
     * convenient to read all bytes into a byte array. It is not intended for
     * reading input streams with large amounts of data.
     *
     * <p> The behavior for the case where the input stream is <i>asynchronously
     * closed</i>, or the thread interrupted during the read, is highly input
     * stream specific, and therefore not specified.
     *
     * <p> If an I/O error occurs reading from the input stream, then it may do
     * so after some, but not all, bytes have been read. Consequently the input
     * stream may not be at end of stream and may be in an inconsistent state.
     * It is strongly recommended that the stream be promptly closed if an I/O
     * error occurs.
     *
     * @param is input stream, must not be null
     * @return a byte array containing the bytes read from this input stream
     * @throws IOException if an I/O error occurs
     * @throws OutOfMemoryError if an array of the required size cannot be
     *         allocated. For example, if an array larger than {@code 2GB} would
     *         be required to store the bytes.
     *
     * @since 1.9
     */
    public static byte[] readAllBytes(InputStream is) throws IOException {
        byte[] buf = new byte[DEFAULT_BUFFER_SIZE];
        int capacity = buf.length;
        int nread = 0;
        int n;
        for (;;) {
            // read to EOF which may read more or less than initial buffer size
            while ((n = is.read(buf, nread, capacity - nread)) > 0)
                nread += n;

            // if the last call to read returned -1, then we're done
            if (n < 0)
                break;

            // need to allocate a larger buffer
            if (capacity <= MAX_BUFFER_SIZE - capacity) {
                capacity = capacity << 1;
            } else {
                if (capacity == MAX_BUFFER_SIZE)
                    throw new OutOfMemoryError("Required array size too large");
                capacity = MAX_BUFFER_SIZE;
            }
            buf = Arrays.copyOf(buf, capacity);
        }
        return (capacity == nread) ? buf : Arrays.copyOf(buf, nread);
    }

    /**
     * Reads the requested number of bytes from the input stream into the given
     * byte array. This method blocks until {@code len} bytes of input data have
     * been read, end of stream is detected, or an exception is thrown. The
     * number of bytes actually read, possibly zero, is returned. This method
     * does not close the input stream.
     *
     * <p> In the case where end of stream is reached before {@code len} bytes
     * have been read, then the actual number of bytes read will be returned.
     * When this stream reaches end of stream, further invocations of this
     * method will return zero.
     *
     * <p> If {@code len} is zero, then no bytes are read and {@code 0} is
     * returned; otherwise, there is an attempt to read up to {@code len} bytes.
     *
     * <p> The first byte read is stored into element {@code b[off]}, the next
     * one in to {@code b[off+1]}, and so on. The number of bytes read is, at
     * most, equal to {@code len}. Let <i>k</i> be the number of bytes actually
     * read; these bytes will be stored in elements {@code b[off]} through
     * {@code b[off+}<i>k</i>{@code -1]}, leaving elements {@code b[off+}<i>k</i>
     * {@code ]} through {@code b[off+len-1]} unaffected.
     *
     * <p> The behavior for the case where the input stream is <i>asynchronously
     * closed</i>, or the thread interrupted during the read, is highly input
     * stream specific, and therefore not specified.
     *
     * <p> If an I/O error occurs reading from the input stream, then it may do
     * so after some, but not all, bytes of {@code b} have been updated with
     * data from the input stream. Consequently the input stream and {@code b}
     * may be in an inconsistent state. It is strongly recommended that the
     * stream be promptly closed if an I/O error occurs.
     *
     * @param  is input stream, must not be null
     * @param  b the byte array into which the data is read
     * @param  off the start offset in {@code b} at which the data is written
     * @param  len the maximum number of bytes to read
     * @return the actual number of bytes read into the buffer
     * @throws IOException if an I/O error occurs
     * @throws NullPointerException if {@code b} is {@code null}
     * @throws IndexOutOfBoundsException If {@code off} is negative, {@code len}
     *         is negative, or {@code len} is greater than {@code b.length - off}
     *
     * @since 1.9
     */
    public static int readNBytes(InputStream is, byte[] b, int off, int len) throws IOException {
        Objects.requireNonNull(b);
        if (off < 0 || len < 0 || len > b.length - off)
            throw new IndexOutOfBoundsException();
        int n = 0;
        while (n < len) {
            int count = is.read(b, off + n, len - n);
            if (count < 0)
                break;
            n += count;
        }
        return n;
    }

}
