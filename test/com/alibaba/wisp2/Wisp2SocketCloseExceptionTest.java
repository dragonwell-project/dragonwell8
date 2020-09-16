/*
 * @test
 * @summary Verify that the behavior of read a closed socket is consistent
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 Wisp2SocketCloseExceptionTest
 */

import java.io.IOException;
import java.io.InputStream;
import java.net.ServerSocket;
import java.net.Socket;

public class Wisp2SocketCloseExceptionTest {
    public static void main(String[] args) throws IOException {
        ServerSocket serverSocket = new ServerSocket(0);
        Socket fd = new Socket("localhost", serverSocket.getLocalPort());
        serverSocket.accept().close();
        InputStream is = fd.getInputStream();
        is.read();
        fd.close();
        is.read();
    }
}

