package sun.nio.ch;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.SocketException;

public interface NetAccess {
    boolean isIPv6Available();

    void translateToSocketException(Exception x)
            throws SocketException;

    void translateException(Exception x,
                                   boolean unknownHostForUnresolved)
            throws IOException;

    void translateException(Exception x)
            throws IOException;

    InetSocketAddress getRevealedLocalAddress(InetSocketAddress addr);

    String getRevealedLocalAddressAsString(InetSocketAddress addr);
}
