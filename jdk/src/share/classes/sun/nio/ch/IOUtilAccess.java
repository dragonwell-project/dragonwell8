package sun.nio.ch;

import java.io.FileDescriptor;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.SocketException;
import java.nio.ByteBuffer;

public interface IOUtilAccess {
    int write(FileDescriptor fd, ByteBuffer src, long position,
              NativeDispatcher nd) throws IOException;

    long write(FileDescriptor fd, ByteBuffer[] bufs, NativeDispatcher nd) throws IOException;

    long write(FileDescriptor fd, ByteBuffer[] bufs, int offset, int length,
               NativeDispatcher nd) throws IOException;

    int read(FileDescriptor fd, ByteBuffer dst, long position,
             NativeDispatcher nd) throws IOException;

    long read(FileDescriptor fd, ByteBuffer[] bufs, NativeDispatcher nd) throws IOException;

    long read(FileDescriptor fd, ByteBuffer[] bufs, int offset, int length,
              NativeDispatcher nd) throws IOException;


}
