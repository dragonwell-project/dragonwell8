/*
 * @test
 * @library /lib/testlibrary
 * @summary Test different coroutines waiting on the same socket's read and write events.
 * @requires os.family == "linux"
 * @run main/timeout=10/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.separateIOPoller=true TestIssue311
 */

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;

public class TestIssue311 {
    public static void main(String[] args) throws Exception {
        Socket client = startEchoServer();
        new Thread(() -> {
            try {
                byte[] buf = new byte[1024];
                while (true) { // discard all data
                    client.getInputStream().read(buf, 0, buf.length);
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }).start();

        byte[] buf = new byte[1024 * 1024];
        for (int i = 0; i < 100; i++) {
            client.getOutputStream().write(buf, 0, buf.length);
        }
    }


    private static Socket startEchoServer() throws Exception {
        ServerSocket serverSocket = new ServerSocket(0);
        Socket socket = new Socket("localhost", serverSocket.getLocalPort());
        new Thread(() -> {
            try {
                Socket clientSocket = serverSocket.accept();
                byte[] buf = new byte[1024];
                int len;
                while ((len = clientSocket.getInputStream().read(buf)) > 0) {
                    clientSocket.getOutputStream().write(buf, 0, len);
                }
                clientSocket.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }).start();
        return socket;
    }
}