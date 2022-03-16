/*
 * @test
 * @library /lib/testlibrary
 * @summary test blocking accept
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -Dcom.alibaba.wisp.carrierEngines=1 -XX:+UseWisp2 BlockingNIOTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.channels.Pipe;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static jdk.testlibrary.Asserts.*;


public class BlockingNIOTest {
    static CountDownLatch latch = new CountDownLatch(1);

    public static void main(String[] args) throws Exception {
        BlockingSemantic();
        Accept2Test();
        PipeTest();
        AdaptorTest();

        boolean te = false;
        try {
            TimeOutTest();
        } catch (SocketTimeoutException e) {
            te = true;
        }

        assertTrue(te);
        assertTrue(latch.getCount() == 1);
    }

    static void BlockingSemantic() throws Exception {

        WispEngine.dispatch(() -> {
            try {
                ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
                serverSocketChannel.bind(new InetSocketAddress(12312));
                serverSocketChannel.configureBlocking(true);
                serverSocketChannel.accept();
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
    }

    static void Accept2Test() throws Exception {
        CountDownLatch serverReady = new CountDownLatch(1);
        CountDownLatch reading = new CountDownLatch(1);

        Thread t = new Thread(() -> {
            try {
                serverReady.await(1, TimeUnit.SECONDS);
                SocketChannel s = SocketChannel.open();
                s.connect(new InetSocketAddress(12388));
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
        t.start();
        Thread t2 = new Thread(() -> {
            try {
                ServerSocketChannel ssc = ServerSocketChannel.open();
                ssc.configureBlocking(true);
                ssc.bind(new InetSocketAddress(12388));
                serverReady.countDown();
                SocketChannel channel = ssc.accept();
                reading.countDown();
                channel.read(ByteBuffer.allocate(203));
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
        t2.start();

        reading.await();
        CountDownLatch suc = new CountDownLatch(1);
        new Thread(() -> {
            try {
                Thread.sleep(2_000L);
            } catch (Exception e) {
                e.printStackTrace();
            }
            suc.countDown();
        }).start();

        suc.await();
    }


    static void PipeTest() throws Exception {
        Pipe pipe = Pipe.open();
        Pipe.SinkChannel sinkChannel = pipe.sink();
        Pipe.SourceChannel sourceChannel = pipe.source();

        CountDownLatch blocking = new CountDownLatch(1);

        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    blocking.countDown();
                    sourceChannel.read(ByteBuffer.allocate(44));
                    // should block
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }).start();

        blocking.await();

        CountDownLatch suc = new CountDownLatch(1);

        new Thread(new Runnable() {
            @Override
            public void run() {
                suc.countDown();
            }
        }).start();

        suc.await();// if carrier is blocking in nio this may hang here
    }


    static void AdaptorTest() throws Exception {

        CountDownLatch serverStarted = new CountDownLatch(1);
        Thread t2 = new Thread(() -> {
            try {
                ServerSocketChannel ssc = ServerSocketChannel.open();
                ssc.configureBlocking(true);
                ssc.bind(new InetSocketAddress(12388));
                ssc.accept();
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
        t2.start();


        Thread t1 = new Thread(() -> {
            try {
                SocketChannel channel = SocketChannel.open();
                channel.configureBlocking(true);
                channel.socket().setSoTimeout(200000);
                channel.socket().connect(new InetSocketAddress(12388));
                int v = channel.socket().getInputStream().read(new byte[2031]);
                System.out.println(v);
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
        t1.start();


        CountDownLatch latch = new CountDownLatch(1);
        WispEngine.dispatch(latch::countDown);

        latch.await();
    }

    static void TimeOutTest() throws Exception {
        ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
        serverSocketChannel.configureBlocking(true);
        serverSocketChannel.socket().setSoTimeout(200);
        serverSocketChannel.socket().bind(new InetSocketAddress(2333));
        serverSocketChannel.socket().accept();
    }
}

