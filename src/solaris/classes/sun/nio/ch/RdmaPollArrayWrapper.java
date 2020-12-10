package sun.nio.ch;

public class RdmaPollArrayWrapper extends PollArrayWrapper {
    static final short SIZE_POLLFD   = 8;


    public RdmaPollArrayWrapper(int initialCapacity) {
        super(initialCapacity);
    }

    @Override
    public int poll(int numfds, int offset, long timeout) {
        return poll0(pollArrayAddress + (offset * SIZE_POLLFD),
                numfds, timeout);
    }

    @Override
    public void interrupt() {
        this.interrupt(interruptFD);
    }


    private static native int poll0(long pollAddress, int numfds, long timeout);

    private static native void interrupt(int fd);

}
