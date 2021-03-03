/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8172297 8196491
 * @summary Test that carriage-return and new-line characters
 * are preserved in webservice parameters
 * @compile ws/HelloWorld.java ws/HelloWorldImpl.java Main.java
 * @run testng/othervm Main
 */

import java.io.IOException;
import java.net.ServerSocket;
import java.net.URL;
import java.util.Collections;
import java.util.Set;
import java.util.concurrent.CountDownLatch;

import javax.xml.namespace.QName;
import javax.xml.soap.SOAPMessage;
import javax.xml.ws.Endpoint;
import javax.xml.ws.Service;
import javax.xml.ws.handler.Handler;
import javax.xml.ws.handler.MessageContext;
import javax.xml.ws.handler.soap.SOAPHandler;
import javax.xml.ws.handler.soap.SOAPMessageContext;

import org.testng.Assert;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;

import ws.HelloWorld;
import ws.HelloWorldImpl;

public class Main {

    @Test(dataProvider="callHandlerDataProvider")
    public void runTest(boolean callGetMessageInHandler) throws Exception {
        CountDownLatch serverInitSignal = new CountDownLatch(1);
        CountDownLatch testDoneSignal = new CountDownLatch(1);

        WebserviceRunner serverThread = new WebserviceRunner(serverInitSignal, testDoneSignal);
        (new Thread(serverThread)).start();

        serverInitSignal.await();

        boolean paramModified = runClientCode(serverThread.getPort(), callGetMessageInHandler);

        testDoneSignal.countDown();

        Assert.assertEquals(callGetMessageInHandler, paramModified,
            "WS parameter has not been processed as expected");
    }

    @DataProvider
    public Object[][] callHandlerDataProvider() {
        return new Object[][]{{true}, {false}};
    }

    /*
     * Connects to launched web service endpoint, sends message with CR/NL symbols and
     * checks if it was modified during the round trip client/server communication.
     */
    private boolean runClientCode(int port, boolean callGetMessage) throws Exception {
        System.out.println("Launching WS client connection on " + port + " port");
        URL url = new URL("http://localhost:" + port + "/ws/hello?wsdl");
        QName qname = new QName("http://ws/", "HelloWorldImplService");
        Service service = Service.create(url, qname);

        registerHandler(service, callGetMessage);

        HelloWorld hello = (HelloWorld) service.getPort(HelloWorld.class);

        logStringContent("Client input parameter", WS_PARAM_VALUE);

        String response = hello.getHelloWorldAsString(WS_PARAM_VALUE);
        logStringContent("Client response parameter", response);

        return !WS_PARAM_VALUE.equals(response);
    }

    /*
     * Register message handler and call SOAPMessageContext.getMessage
     * to emulate issue reported in JDK-8196491
     */
    private void registerHandler(Service service, final boolean callGetMessage) {
        System.out.printf( "Client %s call getMessage inside message handler%n",
            callGetMessage ? "will" : "will not" );
        // Set custom SOAP message handler resolver
        service.setHandlerResolver(portInfo -> {
            Handler h = new SOAPHandler<SOAPMessageContext>() {

                @Override
                public boolean handleMessage(SOAPMessageContext context) {
                    if (callGetMessage) {
                        // Trigger exception from JDK-8196491
                        SOAPMessage msg = context.getMessage();
                    }
                    return true;
                }

                @Override
                public boolean handleFault(SOAPMessageContext context) {
                    return true;
                }

                @Override
                public void close(MessageContext context) {
                }

                @Override
                public Set<QName> getHeaders() {
                    return null;
                }

            };
            return Collections.singletonList(h);
        });
    }

    /*
     * Outputs the parameter value with newline and carriage-return symbols
     * replaced with #CR and #NL text abbreviations.
     */
    private static void logStringContent(String description, String parameter) {
        String readableContent = parameter.replaceAll("\r", "#CR")
                                          .replaceAll("\n", "#NL");
        System.out.println(description + ": '" + readableContent + "'");
    }

    /* Web service parameter value with newline and carriage-return symbols */
    private final static String WS_PARAM_VALUE = "\r\r\n\r\r CarriageReturn and "
                                                +"NewLine \r\n\n Test \r\r\r\r";

    /*
     * Web service server thread that publishes WS on vacant port and waits
     * for client to finalize testing
     */
    class WebserviceRunner implements Runnable {
        // Latch used to signalize when WS endpoint is initialized
        private final CountDownLatch initSignal;
        // Latch used to signalize when client completed testing
        private final CountDownLatch doneSignal;
        // Port where WS endpoint is published
        private volatile int port = 0;

        // Constructor
        WebserviceRunner(CountDownLatch initSignal, CountDownLatch doneSignal) {
            this.initSignal = initSignal;
            this.doneSignal = doneSignal;
        }

        // Returns port of the published endpoint
        public int getPort() {
            return port;
        }

        /*
         * Publish web service on vacant port and waits for the client to
         * complete testing.
         */
        public void run() {
            try {
                // Find vacant port number
                ServerSocket ss = new ServerSocket(0);
                port = ss.getLocalPort();
                ss.close();

                // Publish WebService
                System.out.println("Publishing WebService on " + port + " port");
                Endpoint ep = Endpoint.publish("http://localhost:" + port + "/ws/hello", new HelloWorldImpl());

                // Notify main thread that WS endpoint is published
                initSignal.countDown();

                // Wait for main thread to complete testing
                System.out.println("Waiting for done signal from test client.");
                doneSignal.await();

                // Terminate WS endpoint
                System.out.println("Got done signal from the client. Stopping WS endpoint.");
                ep.stop();
            } catch (IOException ioe) {
                System.out.println("Failed to get vacant port number:" + ioe);
            } catch (InterruptedException ie) {
                System.out.println("Failed to wait for test completion:" + ie);
            }
        }
    }
}
