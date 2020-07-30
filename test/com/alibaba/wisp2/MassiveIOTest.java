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

/*
 * @test
 * @library /lib/testlibrary
 * @summary test massive IO
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 MassiveIOTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.pollerShardingSize=0 MassiveIOTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.pollerShardingSize=1000 MassiveIOTest
 */
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

public class MassiveIOTest {
    private static final Executor es = Executors.newCachedThreadPool();
    public static void main(String[] args) throws Exception {
        ServerSocket server = new ServerSocket(0);
        CompletableFuture.runAsync(() -> echoServer(server), es);
        IntStream.range(0, Math.max(1, Runtime.getRuntime().availableProcessors() / 2))
                .mapToObj(i -> CompletableFuture.runAsync(() -> client(server.getLocalPort()), es))
                .collect(Collectors.toList())
                .forEach(CompletableFuture::join);
    }
    private static void client(int serverPort) {
        try {
            Socket so = new Socket("localhost", serverPort);
            byte[] buffer = new byte[100];
            InputStream is = so.getInputStream();
            OutputStream os = so.getOutputStream();
            for (int i = 0; i < 100000; i++) {
                os.write(buffer);
                is.read(buffer);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
    private static void echoServer(ServerSocket server) {
        while (true) {
            try {
                Socket client = server.accept();
                CompletableFuture.runAsync(() -> echoHandler(client), es);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }
    private static void echoHandler(Socket client) {
        System.out.println("Start serving " + client);
        byte[] buffer = new byte[1024];
        try {
            InputStream is = client.getInputStream();
            OutputStream os = client.getOutputStream();
            while (true) {
                os.write(buffer, 0, is.read(buffer));
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}