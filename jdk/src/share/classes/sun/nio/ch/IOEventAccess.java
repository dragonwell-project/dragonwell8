package sun.nio.ch;

import sun.misc.Unsafe;

import java.io.IOException;

public interface IOEventAccess {

    int eventCtlAdd();

    int eventCtlDel();

    int eventCtlMod();

    int eventOneShot();

    int noEvent();

    long allocatePollArray(int count);

    void freePollArray(long address);

    long getEvent(long address, int i);

    int getDescriptor(long eventAddress);

    int getEvents(long eventAddress);

    int eventCreate() throws IOException;

    int eventCtl(int epfd, int opcode, int fd, int events);

    int eventWait(int epfd, long pollAddress, int numfds, int timeout) throws IOException;

    void socketpair(int[] sv) throws IOException;

    void interrupt(int fd) throws IOException;

    void drain(int fd) throws IOException;

    void close(int fd);

    static void initializeEvent() {
        Unsafe.getUnsafe().ensureClassInitialized(IOEvent.eventClass());
    }

}