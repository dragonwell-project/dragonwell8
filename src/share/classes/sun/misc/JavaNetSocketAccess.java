package sun.misc;

import java.net.Socket;
import java.nio.channels.SocketChannel;

public interface JavaNetSocketAccess {

    /**
     * Create a socket from nio channel implementation.
     */
    Socket createSocketFromChannel(SocketChannel channel);
}
