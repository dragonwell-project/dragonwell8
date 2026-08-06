import java.io.IOException;
import java.net.ServerSocket;
import java.net.*;
import java.net.Socket;

public class SimpleClient {

    public static void main(String[] args) throws Exception {
        Socket socket = new Socket();
        socket.connect(new InetSocketAddress(InetAddress.getLocalHost(), Integer.parseInt(args[0])));
        System.out.println(socket.getRemoteSocketAddress());
    }

}
