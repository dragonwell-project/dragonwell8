/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @bug 8373677
 * @summary Tests for verifying that a non-SSL server can detect
 *          when a client attempts to use SSL.
 * @library /lib/testlibrary
 * @run junit/othervm ClearTextServerSSL
 */

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
import jdk.testlibrary.net.URIBuilder;
import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.Collections;
import java.util.List;
import java.util.logging.ConsoleHandler;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;

import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLException;

import sun.misc.IOUtils;

public class ClearTextServerSSL {

    static final InetAddress LOOPBACK_ADDR = InetAddress.getLoopbackAddress();
    static final boolean ENABLE_LOGGING = true;
    static final Logger logger = Logger.getLogger("com.sun.net.httpserver");
    static final long RSPBODY_EMPTY = -1l;

    static final String CTXT_PATH = "/ClearTextServerSSL";

    @BeforeAll
    public static void setup() {
        if (ENABLE_LOGGING) {
            ConsoleHandler ch = new ConsoleHandler();
            logger.setLevel(Level.ALL);
            ch.setLevel(Level.ALL);
            logger.addHandler(ch);
        }
    }

    @Test
    public void test() throws Exception {
        TestHandler handler = new TestHandler();
        HttpServer server = HttpServer.create(new InetSocketAddress(LOOPBACK_ADDR, 0), 0);
        server.createContext(path(""), handler);
        server.start();
        try {
            URL url = uri("http", server, path("/clear")).toURL();
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            int response = conn.getResponseCode();
            Assertions.assertEquals(200, response);
            URL httpsurl = uri("https", server, path("/ssl")).toURL();
            HttpsURLConnection httpsconn = (HttpsURLConnection) httpsurl.openConnection();
            Assertions.assertThrows(SSLException.class, () -> {
                try {
                    httpsconn.getResponseCode();
                } catch (IOException ioe) {
                    // unwraps unnecessary wrapping fixed in JDK-8216498
                    if (ioe.getCause() instanceof SSLException) {
                        throw new SSLException(ioe);
                    } else {
                        throw ioe;
                    }
                }
            });
            try (Socket socket = new Socket()) {
                socket.connect(server.getAddress());
                byte[] badRequest = {
                        22, 'B', 'A', 'D', ' ',
                        '/', ' ' ,
                        'H', 'T', 'T', 'P', '/', '1', '.', '1' };
                socket.getOutputStream().write(badRequest);
                socket.getOutputStream().flush();
                BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                List<String> line = reader.lines().collect(Collectors.collectingAndThen(
                        Collectors.toList(),
                        Collections::unmodifiableList));
                Assertions.assertEquals("HTTP/1.1 400 Bad Request", line.get(0));
                System.out.println("Got expected response:");
                line.stream().map(l -> "\t" + l).forEach(System.out::println);
            }

        } finally {
            server.stop(0);
        }
    }

    // --- infra ---

    static String path(String path) {
        assert CTXT_PATH.startsWith("/");
        assert !CTXT_PATH.endsWith("/");
        if (path.startsWith("/")) {
            return CTXT_PATH + path;
        } else {
            return CTXT_PATH + "/" + path;
        }
    }

    static URI uri(String scheme, HttpServer server, String path) throws URISyntaxException {
        return URIBuilder.newBuilder()
                .scheme(scheme)
                .loopback()
                .port(server.getAddress().getPort())
                .path(path)
                .build();
    }

    /**
     * A test handler that reads any request bytes and sends
     * an empty 200 response
     */
    static class TestHandler implements HttpHandler {
        @java.lang.Override
        public void handle(HttpExchange exchange) throws IOException {
            try (InputStream reqBody = exchange.getRequestBody()) {
                IOUtils.readAllBytes(reqBody);
                exchange.sendResponseHeaders(200, RSPBODY_EMPTY);
            } catch (Throwable t) {
                t.printStackTrace();
                exchange.sendResponseHeaders(500, RSPBODY_EMPTY);
            }
        }
    }
}
