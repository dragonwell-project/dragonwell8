/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

/* @test
 * @bug 8158963
 *
 * @summary Disable RMI over HTTP by default
 *
 * @library ../../../../../java/rmi/testlibrary
 * @build TestIface TestImpl
 * @run main/othervm/timeout=60 DisableRMIOverHTTPTest
 * @run main/othervm/timeout=60 -Dsun.rmi.server.disableIncomingHttp=false DisableRMIOverHTTPTest
 */

/*
 * This test is an adaptation of ../blockAccept/BlockAcceptTest.java
 *
 * This test:
 * 1. Creates an object and exports it.
 * 2. Makes a regular call, using HTTP tunnelling.
 * 3. Either throws an exception if RMI over HTTP is disabled or completes
 *    execution if not.
 */

import java.rmi.*;
import java.rmi.server.RMISocketFactory;
import java.io.*;
import java.net.*;

import sun.rmi.transport.proxy.RMIMasterSocketFactory;
import sun.rmi.transport.proxy.RMIHttpToPortSocketFactory;

public class DisableRMIOverHTTPTest
{
    public static void main(String[] args)
        throws Exception
    {
        // HTTP direct to the server port
        System.setProperty("http.proxyHost", "127.0.0.1");
        boolean incomingHttpDisabled =
                Boolean.valueOf(
                    System.getProperty(
                            "sun.rmi.server.disableIncomingHttp", "true")
                        .equalsIgnoreCase("true"));

        // Set the socket factory.
        System.err.println("(installing HTTP-out socket factory)");
        HttpOutFactory fac = new HttpOutFactory();
        RMISocketFactory.setSocketFactory(fac);

        // Create remote object
        TestImpl impl = new TestImpl();

        // Export and get which port.
        System.err.println("(exporting remote object)");
        TestIface stub = impl.export();
        try {
            int port = fac.whichPort();

            // Sanity
            if (port == 0)
                throw new Error("TEST FAILED: export didn't reserve a port(?)");

            // The test itself: make a remote call and see if it's blocked or
            // if it works
            //Thread.sleep(2000);
            System.err.println("(making RMI-through-HTTP call)");
            String result = stub.testCall("dummy load");
            System.err.println(" => " + result);

            if ("OK".equals(result)) {
                if (incomingHttpDisabled) {
                    throw new Error(
                        "TEST FAILED: should not receive result if incoming http is disabled");
                }
            } else {
                if (!incomingHttpDisabled) {
                    throw new Error("TEST FAILED: result not OK");
                }
            }
            System.err.println("Test passed.");
        } catch (UnmarshalException e) {
            if (!incomingHttpDisabled) {
                throw e;
            } else {
                System.err.println("Test passed.");
            }
        } finally {
            try {
                impl.unexport();
            } catch (Throwable unmatter) {
            }
        }

        // Should exit here
    }

    private static class HttpOutFactory
        extends RMISocketFactory
    {
        private int servport = 0;

        public Socket createSocket(String h, int p)
            throws IOException
        {
            return ((new RMIHttpToPortSocketFactory()).createSocket(h, p));
        }

        /** Create a server socket and remember which port it's on.
         * Aborts if createServerSocket(0) is called twice, because then
         * it doesn't know whether to remember the first or second port.
         */
        public ServerSocket createServerSocket(int p)
            throws IOException
        {
            ServerSocket ss;
            ss = (new RMIMasterSocketFactory()).createServerSocket(p);
            if (p == 0) {
                if (servport != 0) {
                    System.err.println("TEST FAILED: " +
                                       "Duplicate createServerSocket(0)");
                    throw new Error("Test aborted (createServerSocket)");
                }
                servport = ss.getLocalPort();
            }
            return (ss);
        }

        /** Return which port was reserved by createServerSocket(0).
         * If the return value was 0, createServerSocket(0) wasn't called.
         */
        public int whichPort() {
            return (servport);
        }
    } // end class HttpOutFactory
}
