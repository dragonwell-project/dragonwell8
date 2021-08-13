/*
 * Copyright (c) 2020 SAP SE. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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

import java.io.IOException;
import java.lang.management.ManagementFactory;

import javax.net.SocketFactory;
import javax.net.ssl.SSLSocketFactory;

import com.sun.management.UnixOperatingSystemMXBean;

/*
 * @test
 * @bug 8256818 8257670 8257884 8257997
 * @summary Test that creating and closing SSL Sockets without bind/connect
 *          will not leave leaking socket file descriptors
 * @run main/native/manual/othervm SSLSocketLeak
 * @comment native library is required only on Windows, use the following commands:
 *          "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
 *          cl ^
 *              /LD ^
 *              <path/to/jdksrc>\jdk\test\sun\security\ssl\SSLSocketImpl\libFileUtils.c ^
 *              /I<path/to/jdkbin>\include ^
 *              /I<path/to/jdkbin>\include\win32 ^
 *              /FeFileUtils.dll
 *          jtreg <...> -nativepath:. <path/to/jdk8u>\jdk\test\sun\security\ssl\SSLSocketImpl\SSLSocketLeak.java
 */
public class SSLSocketLeak {

    // number of sockets to open/close
    private static final int NUM_TEST_SOCK = 500;
    private static final boolean IS_WINDOWS = System.getProperty("os.name").toLowerCase().startsWith("windows");
    private static volatile boolean nativeLibLoaded;

    // percentage of accepted growth of open handles
    private static final int OPEN_HANDLE_GROWTH_THRESHOLD_PERCENTAGE = IS_WINDOWS ? 25 : 10;

    public static void main(String[] args) throws IOException {
        long fds_start = getProcessHandleCount();
        System.out.println("FDs at the beginning: " + fds_start);

        SocketFactory f = SSLSocketFactory.getDefault();
        for (int i = 0; i < NUM_TEST_SOCK; i++) {
            f.createSocket().close();
        }

        long fds_end = getProcessHandleCount();
        System.out.println("FDs in the end: " + fds_end);

        if ((fds_end - fds_start) > ((NUM_TEST_SOCK * OPEN_HANDLE_GROWTH_THRESHOLD_PERCENTAGE)) / 100) {
            throw new RuntimeException("Too many open file descriptors. Looks leaky.");
        }
    }

    // Return the current process handle count
    private static long getProcessHandleCount() {
        if (IS_WINDOWS) {
            if (!nativeLibLoaded) {
                System.loadLibrary("FileUtils");
                nativeLibLoaded = true;
            }
            return getWinProcessHandleCount();
        } else {
            return ((UnixOperatingSystemMXBean)ManagementFactory.getOperatingSystemMXBean()).getOpenFileDescriptorCount();
        }
    }

    private static native long getWinProcessHandleCount();
}
