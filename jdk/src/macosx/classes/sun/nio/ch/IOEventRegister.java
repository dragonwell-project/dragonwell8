package sun.nio.ch;

import sun.misc.SharedSecrets;

import java.io.IOException;

public class IOEventRegister {

    static {
        SharedSecrets.setIOEventAccess(new IOEventAccess() {

            @Override
            public int eventCtlAdd() {
                throw new UnsupportedOperationException();
            }

            @Override
            public int eventCtlDel() {
                throw new UnsupportedOperationException();
            }

            @Override
            public int eventCtlMod() {
                throw new UnsupportedOperationException();
            }

            @Override
            public int eventOneShot() {
                throw new UnsupportedOperationException();
            }

            @Override
            public int noEvent() {
                throw new UnsupportedOperationException();
            }

            @Override
            public long allocatePollArray(int count) {
                throw new UnsupportedOperationException();
            }

            @Override
            public void freePollArray(long address) {
                throw new UnsupportedOperationException();
            }

            @Override
            public long getEvent(long address, int i) {
                throw new UnsupportedOperationException();
            }

            @Override
            public int getDescriptor(long eventAddress) {
                throw new UnsupportedOperationException();
            }

            @Override
            public int getEvents(long eventAddress) {
                throw new UnsupportedOperationException();
            }

            @Override
            public int eventCreate() throws IOException {
                throw new UnsupportedOperationException();
            }

            @Override
            public int eventCtl(int epfd, int opcode, int fd, int events) {
                throw new UnsupportedOperationException();
            }

            @Override
            public int eventWait(int epfd, long pollAddress, int numfds, int timeout) throws IOException {
                throw new UnsupportedOperationException();
            }

            @Override
            public void socketpair(int[] sv) throws IOException {
                throw new UnsupportedOperationException();
            }

            @Override
            public void interrupt(int fd) throws IOException {
                throw new UnsupportedOperationException();
            }

            @Override
            public void drain(int fd) throws IOException {
                throw new UnsupportedOperationException();
            }

            @Override
            public void close(int fd) {
                throw new UnsupportedOperationException();
            }
        });
    }

}