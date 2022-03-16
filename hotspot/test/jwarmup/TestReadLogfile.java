/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

import sun.hotspot.WhiteBox;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.*;

import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;
/*
 * @test TestReadLogfile
 * @library /testlibrary /testlibrary/whitebox
 * @build TestReadLogfile
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm TestReadLogfile
 * @summary test jitwarmup read class init section & method section from log file
 */
public class TestReadLogfile {
    private static String classPath;

    private static final int HEADER_SIZE = 36;
    private static final int APPID_OFFSET = 16;

    public static String generateOriginLogfile() throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        File logfile = new File("./jitwarmup.log");
        classPath = System.getProperty("test.class.path");
        try {
            output = ProcessTools.executeProcess("pwd");
            System.out.println(output.getOutput());
        } catch (Throwable e) {
            e.printStackTrace();
        }

        String className = InnerA.class.getName();
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:+CompilationWarmUpRecording",
                "-XX:-ClassUnloading",
                "-XX:+UseConcMarkSweepGC",
                "-XX:-CMSClassUnloadingEnabled",
                "-XX:-UseSharedSpaces",
                "-XX:CompilationWarmUpLogfile=./" + logfile.getName(),
                "-XX:CompilationWarmUpRecordTime=10",
                "-XX:CompilationWarmUpAppID=123",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                className, "collection");
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("[JitWarmUp] output profile info has done");
        output.shouldContain("process is done!");
        output.shouldHaveExitValue(0);
        System.out.println(output.getOutput());

        if (!logfile.exists()) {
            throw new Error("jit log not exist");
        }
        return logfile.getName();
    }

    public static OutputAnalyzer testReadLogfileAndGetResult(String filename) throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        // test read logfile
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:-UseSharedSpaces",
                "-XX:+CompilationWarmUp",
                "-XX:CompilationWarmUpLogfile=./" + filename,
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpAppID=123",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                TestReadLogfile.InnerA.class.getName(), "compilation");
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        return output;
    }

    private static byte[] getFileContent(String name) throws IOException{
        ByteBuffer byteBuffer = null;
        FileChannel fc = null;
        File originFile = null;
        FileInputStream ifs = null;
        try {
            originFile = new File("./" + name);
            ifs = new FileInputStream(originFile);
            fc = ifs.getChannel();
            byteBuffer = ByteBuffer.allocate((int)fc.size());
            while (fc.read(byteBuffer) > 0) {
                // read
            }
        } finally {
            if (fc != null) {
                fc.close();
                ifs.close();
            }
        }
        return byteBuffer.array();
    }

    private static File createNewFile(String fileName) throws IOException {
        File f = new File("./" + fileName);
        if (f.exists()) {
            f.delete();
        }
        f.createNewFile();
        return f;
    }

    private static int readIntAsJavaInteger(RandomAccessFile f) throws IOException {
        byte b1 = f.readByte();
        byte b2 = f.readByte();
        byte b3 = f.readByte();
        byte b4 = f.readByte();
        int result = (b1 & 0xff) | ((b2 << 8) & 0xff00)
                | ((b3 << 16) & 0xff0000) | (b4 << 24);
        return result;
    }

    private static byte[] IntegerAsBytes(Integer res) {
        byte[] targets = new byte[4];
        targets[0] = (byte) (res & 0xff);
        targets[1] = (byte) ((res >> 8) & 0xff);
        targets[2] = (byte) ((res >> 16) & 0xff);
        targets[3] = (byte) (res >>> 24);
        return targets;
    }

    // illegal header
    public static String generateIllegalLogfile1(String originLogfileName) throws IOException {
        String fileName = "jitwarmup_1.log";
        File f = createNewFile(fileName);
        byte[] originContent = getFileContent(originLogfileName);
        RandomAccessFile raf = new RandomAccessFile(f, "rw");
        raf.write(originContent, 0, originContent.length);
        byte[] illegalHeader = {0x1,0x2,0x0,0x0,(byte)0xba,(byte)0xbb};
        raf.seek(0);
        raf.write(illegalHeader, 0, illegalHeader.length);
        raf.close();
        return fileName;
    }

    // illegal class init section size, size too large
    public static String generateIllegalLogfile2(String originLogfileName) throws IOException {
        String fileName = "jitwarmup_2.log";
        File f = createNewFile(fileName);
        byte[] originContent = getFileContent(originLogfileName);
        RandomAccessFile raf = new RandomAccessFile(f, "rw");
        raf.write(originContent, 0, originContent.length);
        raf.seek(HEADER_SIZE);
        int origin_size = readIntAsJavaInteger(raf);
        raf.seek(HEADER_SIZE);
        System.out.println("origin size is" + origin_size);
        raf.write(IntegerAsBytes(origin_size + 18));  // illegal size
        raf.close();
        return fileName;
    }

    // illegal record section size, size too large
    public static String generateIllegalLogfile3(String originLogfileName) throws IOException {
        String fileName = "jitwarmup_3.log";
        File f = createNewFile(fileName);
        byte[] originContent = getFileContent(originLogfileName);
        RandomAccessFile raf = new RandomAccessFile(f, "rw");
        raf.write(originContent, 0, originContent.length);
        raf.seek(HEADER_SIZE);
        int init_section_size = readIntAsJavaInteger(raf);
        raf.seek(HEADER_SIZE + init_section_size);
        int record_section_size = readIntAsJavaInteger(raf);
        System.out.println("record_section size is" + record_section_size);
        raf.write(IntegerAsBytes(Integer.MAX_VALUE - 1));  // illegal size, too big
        raf.close();
        return fileName;
    }

    // illegal app id
    public static String generateIllegalLogfile4(String originLogfileName) throws IOException {
        String fileName = "jitwarmup_4.log";
        File f = createNewFile(fileName);
        byte[] originContent = getFileContent(originLogfileName);
        RandomAccessFile raf = new RandomAccessFile(f, "rw");
        raf.write(originContent, 0, originContent.length);
        raf.seek(APPID_OFFSET);
        int origin_appid = readIntAsJavaInteger(raf);
        raf.seek(APPID_OFFSET);
        System.out.println("origin appid is" + origin_appid);
        raf.write(IntegerAsBytes(origin_appid + 1));  // illegal appid
        raf.close();
        return fileName;
    }

    public static void main(String[] args) throws Exception {
        OutputAnalyzer output = null;
        String originLogfileName = generateOriginLogfile();
        // test origin log file
        output = testReadLogfileAndGetResult(originLogfileName);
        output.shouldContain("read log file OK");
        output.shouldHaveExitValue(0);
        // generate and test illegal log file header
        output = testReadLogfileAndGetResult(generateIllegalLogfile1(originLogfileName));
        output.shouldNotContain("read log file OK");
        // generate and test illegal class init section size
        output = testReadLogfileAndGetResult(generateIllegalLogfile2(originLogfileName));
        output.shouldNotContain("read log file OK");
        // generate and test illegal record section size
        output = testReadLogfileAndGetResult(generateIllegalLogfile3(originLogfileName));
        output.shouldNotContain("read log file OK");
        // generate and test illegal appid
        output = testReadLogfileAndGetResult(generateIllegalLogfile4(originLogfileName));
        output.shouldNotContain("read log file OK");
    }

    public static class InnerA {
        private static WhiteBox whiteBox;
        static {
            System.out.println("InnerA initialize");
        }

        public static String[] aa = new String[0];
        public static List<String> ls = new ArrayList<String>();
        public String foo() {
            for (int i = 0; i < 12000; i++) {
                foo2(aa);
            }
            ls.add("x");
            return ls.get(0);
        }
        public void foo2(String[] a) {
            String s = "aa";
            if (ls.size() > 100 && a.length < 100) {
                ls.clear();
            } else {
                ls.add(s);
            }
        }

        public static void main(String[] args) throws Exception {
            if (args[0].equals("collection")) {
                InnerA a = new InnerA();
                a.foo();
                Thread.sleep(15000);
                a.foo();
                System.out.println("process is done!");
            } else if (args[0].equals("compilation")) {
                whiteBox = WhiteBox.getWhiteBox();
                String className = InnerA.class.getName();
                String[] classList = whiteBox.getClassListFromLogfile();
                System.out.println("class list length is " + classList.length);
                boolean containClass = false;
                for (int i = 0; i < classList.length; i++) {
                    if (classList[i].equals(className)) {
                        containClass = true;
                        System.out.println("contain class OK");
                    }
                }

                String methodName = "foo2";
                String[] methodList = whiteBox.getMethodListFromLogfile();
                System.out.println("method list length is " + methodList.length);
                assertTrue(methodList.length > 0);
                boolean containMethod = false;
                for (int i = 0; i < methodList.length; i++) {
                    if (methodList[i].equals(methodName)) {
                        containMethod = true;
                        System.out.println("contain method OK");
                    }
                }

                if (containClass && containMethod) {
                    System.out.println("read log file OK");
                }
            }
        }
    }
}
