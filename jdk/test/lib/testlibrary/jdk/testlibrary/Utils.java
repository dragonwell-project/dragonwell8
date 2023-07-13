/*
 * Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package jdk.testlibrary;

import static jdk.testlibrary.Asserts.assertTrue;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.UnknownHostException;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.LinkedList;
import java.util.Objects;
import java.util.Set;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.FileAttribute;

import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.util.concurrent.TimeUnit;
import java.util.function.Function;
import java.nio.file.Files;

/**
 * Common library for various test helper functions.
 */
public final class Utils {

    /**
     * Returns the sequence used by operating system to separate lines.
     */
    public static final String NEW_LINE = System.getProperty("line.separator");

    /**
     * Returns the value of 'test.vm.opts'system property.
     */
    public static final String VM_OPTIONS = System.getProperty("test.vm.opts", "").trim();

    /**
     * Returns the value of 'test.java.opts'system property.
     */
    public static final String JAVA_OPTIONS = System.getProperty("test.java.opts", "").trim();


    /**
    * Returns the value of 'test.timeout.factor' system property
    * converted to {@code double}.
    */
    public static final double TIMEOUT_FACTOR;
    static {
        String toFactor = System.getProperty("test.timeout.factor", "1.0");
        TIMEOUT_FACTOR = Double.parseDouble(toFactor);
    }

    /**
    * Returns the value of JTREG default test timeout in milliseconds
    * converted to {@code long}.
    */
    public static final long DEFAULT_TEST_TIMEOUT = TimeUnit.SECONDS.toMillis(120);

    private static final int MAX_BUFFER_SIZE = Integer.MAX_VALUE - 8;
    private static final int DEFAULT_BUFFER_SIZE = 8192;

    private Utils() {
        // Private constructor to prevent class instantiation
    }

    /**
     * Returns the list of VM options.
     *
     * @return List of VM options
     */
    public static List<String> getVmOptions() {
        return Arrays.asList(safeSplitString(VM_OPTIONS));
    }

    /**
     * Returns the list of VM options with -J prefix.
     *
     * @return The list of VM options with -J prefix
     */
    public static List<String> getForwardVmOptions() {
        String[] opts = safeSplitString(VM_OPTIONS);
        for (int i = 0; i < opts.length; i++) {
            opts[i] = "-J" + opts[i];
        }
        return Arrays.asList(opts);
    }

    /**
     * Returns the default JTReg arguments for a jvm running a test.
     * This is the combination of JTReg arguments test.vm.opts and test.java.opts.
     * @return An array of options, or an empty array if no opptions.
     */
    public static String[] getTestJavaOpts() {
        List<String> opts = new ArrayList<String>();
        Collections.addAll(opts, safeSplitString(VM_OPTIONS));
        Collections.addAll(opts, safeSplitString(JAVA_OPTIONS));
        return opts.toArray(new String[0]);
    }

    /**
     * Combines given arguments with default JTReg arguments for a jvm running a test.
     * This is the combination of JTReg arguments test.vm.opts and test.java.opts
     * @return The combination of JTReg test java options and user args.
     */
    public static String[] addTestJavaOpts(String... userArgs) {
        List<String> opts = new ArrayList<String>();
        Collections.addAll(opts, getTestJavaOpts());
        Collections.addAll(opts, userArgs);
        return opts.toArray(new String[0]);
    }

    /**
     * Removes any options specifying which GC to use, for example "-XX:+UseG1GC".
     * Removes any options matching: -XX:(+/-)Use*GC
     * Used when a test need to set its own GC version. Then any
     * GC specified by the framework must first be removed.
     * @return A copy of given opts with all GC options removed.
     */
    private static final Pattern useGcPattern = Pattern.compile("\\-XX\\:[\\+\\-]Use.+GC");
    public static List<String> removeGcOpts(List<String> opts) {
        List<String> optsWithoutGC = new ArrayList<String>();
        for (String opt : opts) {
            if (useGcPattern.matcher(opt).matches()) {
                System.out.println("removeGcOpts: removed " + opt);
            } else {
                optsWithoutGC.add(opt);
            }
        }
        return optsWithoutGC;
    }

    /**
     * Splits a string by white space.
     * Works like String.split(), but returns an empty array
     * if the string is null or empty.
     */
    private static String[] safeSplitString(String s) {
        if (s == null || s.trim().isEmpty()) {
            return new String[] {};
        }
        return s.trim().split("\\s+");
    }

    /**
     * @return The full command line for the ProcessBuilder.
     */
    public static String getCommandLine(ProcessBuilder pb) {
        StringBuilder cmd = new StringBuilder();
        for (String s : pb.command()) {
            cmd.append(s).append(" ");
        }
        return cmd.toString();
    }

    /**
     * Returns the free port on the local host.
     * The function will spin until a valid port number is found.
     *
     * @return The port number
     * @throws InterruptedException if any thread has interrupted the current thread
     * @throws IOException if an I/O error occurs when opening the socket
     */
    public static int getFreePort() throws InterruptedException, IOException {
        int port = -1;

        while (port <= 0) {
            Thread.sleep(100);

            ServerSocket serverSocket = null;
            try {
                serverSocket = new ServerSocket(0);
                port = serverSocket.getLocalPort();
            } finally {
                serverSocket.close();
            }
        }

        return port;
    }

    /**
     * Returns the name of the local host.
     *
     * @return The host name
     * @throws UnknownHostException if IP address of a host could not be determined
     */
    public static String getHostname() throws UnknownHostException {
        InetAddress inetAddress = InetAddress.getLocalHost();
        String hostName = inetAddress.getHostName();

        assertTrue((hostName != null && !hostName.isEmpty()),
                "Cannot get hostname");

        return hostName;
    }

    /**
     * Uses "jcmd -l" to search for a jvm pid. This function will wait
     * forever (until jtreg timeout) for the pid to be found.
     * @param key Regular expression to search for
     * @return The found pid.
     */
    public static int waitForJvmPid(String key) throws Throwable {
        final long iterationSleepMillis = 250;
        System.out.println("waitForJvmPid: Waiting for key '" + key + "'");
        System.out.flush();
        while (true) {
            int pid = tryFindJvmPid(key);
            if (pid >= 0) {
                return pid;
            }
            Thread.sleep(iterationSleepMillis);
        }
    }

    /**
     * Searches for a jvm pid in the output from "jcmd -l".
     *
     * Example output from jcmd is:
     * 12498 sun.tools.jcmd.JCmd -l
     * 12254 /tmp/jdk8/tl/jdk/JTwork/classes/com/sun/tools/attach/Application.jar
     *
     * @param key A regular expression to search for.
     * @return The found pid, or -1 if Enot found.
     * @throws Exception If multiple matching jvms are found.
     */
    public static int tryFindJvmPid(String key) throws Throwable {
        OutputAnalyzer output = null;
        try {
            JDKToolLauncher jcmdLauncher = JDKToolLauncher.create("jcmd");
            jcmdLauncher.addToolArg("-l");
            output = ProcessTools.executeProcess(jcmdLauncher.getCommand());
            output.shouldHaveExitValue(0);

            // Search for a line starting with numbers (pid), follwed by the key.
            Pattern pattern = Pattern.compile("([0-9]+)\\s.*(" + key + ").*\\r?\\n");
            Matcher matcher = pattern.matcher(output.getStdout());

            int pid = -1;
            if (matcher.find()) {
                pid = Integer.parseInt(matcher.group(1));
                System.out.println("findJvmPid.pid: " + pid);
                if (matcher.find()) {
                    throw new Exception("Found multiple JVM pids for key: " + key);
                }
            }
            return pid;
        } catch (Throwable t) {
            System.out.println(String.format("Utils.findJvmPid(%s) failed: %s", key, t));
            throw t;
        }
    }

    /**
     * Returns file content as a list of strings
     *
     * @param file File to operate on
     * @return List of strings
     * @throws IOException
     */
    public static List<String> fileAsList(File file) throws IOException {
        assertTrue(file.exists() && file.isFile(),
                file.getAbsolutePath() + " does not exist or not a file");
        List<String> output = new ArrayList<>();
        try (BufferedReader reader = new BufferedReader(new FileReader(file.getAbsolutePath()))) {
            while (reader.ready()) {
                output.add(reader.readLine().replace(NEW_LINE, ""));
            }
        }
        return output;
    }

    /**
     * Helper method to read all bytes from InputStream
     *
     * @param is InputStream to read from
     * @return array of bytes
     * @throws IOException
     */
    public static byte[] readAllBytes(InputStream is) throws IOException {
        byte[] buf = new byte[DEFAULT_BUFFER_SIZE];
        int capacity = buf.length;
        int nread = 0;
        int n;
        for (;;) {
            // read to EOF which may read more or less than initial buffer size
            while ((n = is.read(buf, nread, capacity - nread)) > 0)
                nread += n;

            // if the last call to read returned -1, then we're done
            if (n < 0)
                break;

            // need to allocate a larger buffer
            if (capacity <= MAX_BUFFER_SIZE - capacity) {
                capacity = capacity << 1;
            } else {
                if (capacity == MAX_BUFFER_SIZE)
                    throw new OutOfMemoryError("Required array size too large");
                capacity = MAX_BUFFER_SIZE;
            }
            buf = Arrays.copyOf(buf, capacity);
        }
        return (capacity == nread) ? buf : Arrays.copyOf(buf, nread);
    }

    /**
     * Adjusts the provided timeout value for the TIMEOUT_FACTOR
     * @param tOut the timeout value to be adjusted
     * @return The timeout value adjusted for the value of "test.timeout.factor"
     *         system property
     */
    public static long adjustTimeout(long tOut) {
        return Math.round(tOut * Utils.TIMEOUT_FACTOR);
    }

    /**
     * Interface same as java.lang.Runnable but with
     * method {@code run()} able to throw any Throwable.
     */
    public static interface ThrowingRunnable {
        void run() throws Throwable;
    }

    /**
     * Filters out an exception that may be thrown by the given
     * test according to the given filter.
     *
     * @param test - method that is invoked and checked for exception.
     * @param filter - function that checks if the thrown exception matches
     *                 criteria given in the filter's implementation.
     * @return - exception that matches the filter if it has been thrown or
     *           {@code null} otherwise.
     * @throws Throwable - if test has thrown an exception that does not
     *                     match the filter.
     */
    public static Throwable filterException(ThrowingRunnable test,
            Function<Throwable, Boolean> filter) throws Throwable {
        try {
            test.run();
        } catch (Throwable t) {
            if (filter.apply(t)) {
                return t;
            } else {
                throw t;
            }
        }
        return null;
    }

    private static final int BUFFER_SIZE = 1024;

    /**
     * Reads all bytes from the input stream and writes the bytes to the
     * given output stream in the order that they are read. On return, the
     * input stream will be at end of stream. This method does not close either
     * stream.
     * <p>
     * This method may block indefinitely reading from the input stream, or
     * writing to the output stream. The behavior for the case where the input
     * and/or output stream is <i>asynchronously closed</i>, or the thread
     * interrupted during the transfer, is highly input and output stream
     * specific, and therefore not specified.
     * <p>
     * If an I/O error occurs reading from the input stream or writing to the
     * output stream, then it may do so after some bytes have been read or
     * written. Consequently the input stream may not be at end of stream and
     * one, or both, streams may be in an inconsistent state. It is strongly
     * recommended that both streams be promptly closed if an I/O error occurs.
     *
     * @param  in the input stream, non-null
     * @param  out the output stream, non-null
     * @return the number of bytes transferred
     * @throws IOException if an I/O error occurs when reading or writing
     * @throws NullPointerException if {@code in} or {@code out} is {@code null}
     *
     */
    public static long transferTo(InputStream in, OutputStream out)
            throws IOException  {
        long transferred = 0;
        byte[] buffer = new byte[BUFFER_SIZE];
        int read;
        while ((read = in.read(buffer, 0, BUFFER_SIZE)) >= 0) {
            out.write(buffer, 0, read);
            transferred += read;
        }
        return transferred;
    }

    // Parses the specified source file for "@{id} breakpoint" tags and returns
    // list of the line numbers containing the tag.
    // Example:
    //   System.out.println("BP is here");  // @1 breakpoint
    public static List<Integer> parseBreakpoints(String filePath, int id) {
        final String pattern = "@" + id + " breakpoint";
        int lineNum = 1;
        List<Integer> result = new LinkedList<>();
        try {
            for (String line: Files.readAllLines(Paths.get(filePath))) {
                if (line.contains(pattern)) {
                    result.add(lineNum);
                }
                lineNum++;
            }
        } catch (IOException ex) {
            throw new RuntimeException("failed to parse " + filePath, ex);
        }
        return result;
    }

    // gets full test source path for the given test filename
    public static String getTestSourcePath(String fileName) {
        return Paths.get(System.getProperty("test.src")).resolve(fileName).toString();
    }

    /**
     * Creates an empty directory in "user.dir" or "."
     * <p>
     * This method is meant as a replacement for {@code Files#createTempDirectory(String, String, FileAttribute...)}
     * that doesn't leave files behind in /tmp directory of the test machine
     * <p>
     * If the property "user.dir" is not set, "." will be used.
     *
     * @param prefix
     * @param attrs
     * @return the path to the newly created directory
     * @throws IOException
     *
     * @see {@link Files#createTempDirectory(String, String, FileAttribute...)}
     */
    public static Path createTempDirectory(String prefix, FileAttribute<?>... attrs) throws IOException {
        Path dir = Paths.get(System.getProperty("user.dir", "."));
        return Files.createTempDirectory(dir, prefix);
    }

    /*
       List.of implementations
       These methods are intended to provide replacements
       for the use of List.of() methods in backports,
       using existing 8u methods. The returned collections
       share the key property of the List.of collections
       in being unmodifiable, but may not be equivalent
       with regard to other properties such as serialization
       and access order.
    */

    /**
     * Returns an unmodifiable list containing zero elements.
     *
     * @param <E> the {@code List}'s element type
     * @return an empty {@code List}
     */
    public static <E> List<E> listOf() {
        return Collections.emptyList();
    }

    /**
     * Returns an unmodifiable list containing one element.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the single element
     * @return a {@code List} containing the specified element
     * @throws NullPointerException if the element is {@code null}
     */
    public static <E> List<E> listOf(E e1) {
        Objects.requireNonNull(e1, "e1");
        return Collections.singletonList(e1);
    }

    /**
     * Returns an unmodifiable list containing two elements.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> List<E> listOf(E e1, E e2) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        List<E> l = new ArrayList<>(2);
        l.add(e1);
        l.add(e2);
        return Collections.unmodifiableList(l);
    }

    /**
     * Returns an unmodifiable list containing three elements.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> List<E> listOf(E e1, E e2, E e3) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        List<E> l = new ArrayList<>(3);
        l.add(e1);
        l.add(e2);
        l.add(e3);
        return Collections.unmodifiableList(l);
    }

    /**
     * Returns an unmodifiable list containing four elements.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> List<E> listOf(E e1, E e2, E e3, E e4) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        List<E> l = new ArrayList<>(4);
        l.add(e1);
        l.add(e2);
        l.add(e3);
        l.add(e4);
        return Collections.unmodifiableList(l);
    }

    /**
     * Returns an unmodifiable list containing five elements.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> List<E> listOf(E e1, E e2, E e3, E e4, E e5) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        List<E> l = new ArrayList<>(5);
        l.add(e1);
        l.add(e2);
        l.add(e3);
        l.add(e4);
        l.add(e5);
        return Collections.unmodifiableList(l);
    }

    /**
     * Returns an unmodifiable list containing six elements.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> List<E> listOf(E e1, E e2, E e3, E e4, E e5, E e6) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");
        List<E> l = new ArrayList<>(6);
        l.add(e1);
        l.add(e2);
        l.add(e3);
        l.add(e4);
        l.add(e5);
        l.add(e6);
        return Collections.unmodifiableList(l);
    }

    /**
     * Returns an unmodifiable list containing seven elements.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @param e7 the seventh element
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> List<E> listOf(E e1, E e2, E e3, E e4, E e5, E e6, E e7) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");
        Objects.requireNonNull(e7, "e7");
        List<E> l = new ArrayList<>(7);
        l.add(e1);
        l.add(e2);
        l.add(e3);
        l.add(e4);
        l.add(e5);
        l.add(e6);
        l.add(e7);
        return Collections.unmodifiableList(l);
    }

    /**
     * Returns an unmodifiable list containing eight elements.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @param e7 the seventh element
     * @param e8 the eighth element
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> List<E> listOf(E e1, E e2, E e3, E e4, E e5, E e6, E e7, E e8) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");
        Objects.requireNonNull(e7, "e7");
        Objects.requireNonNull(e8, "e8");
        List<E> l = new ArrayList<>(8);
        l.add(e1);
        l.add(e2);
        l.add(e3);
        l.add(e4);
        l.add(e5);
        l.add(e6);
        l.add(e7);
        l.add(e8);
        return Collections.unmodifiableList(l);
    }

    /**
     * Returns an unmodifiable list containing nine elements.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @param e7 the seventh element
     * @param e8 the eighth element
     * @param e9 the ninth element
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> List<E> listOf(E e1, E e2, E e3, E e4, E e5, E e6, E e7, E e8, E e9) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");
        Objects.requireNonNull(e7, "e7");
        Objects.requireNonNull(e8, "e8");
        Objects.requireNonNull(e9, "e9");
        List<E> l = new ArrayList<>(9);
        l.add(e1);
        l.add(e2);
        l.add(e3);
        l.add(e4);
        l.add(e5);
        l.add(e6);
        l.add(e7);
        l.add(e8);
        l.add(e9);
        return Collections.unmodifiableList(l);
    }

    /**
     * Returns an unmodifiable list containing ten elements.
     *
     * @param <E> the {@code List}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @param e7 the seventh element
     * @param e8 the eighth element
     * @param e9 the ninth element
     * @param e10 the tenth element
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> List<E> listOf(E e1, E e2, E e3, E e4, E e5, E e6, E e7, E e8, E e9, E e10) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");
        Objects.requireNonNull(e7, "e7");
        Objects.requireNonNull(e8, "e8");
        Objects.requireNonNull(e9, "e9");
        Objects.requireNonNull(e10, "e10");
        List<E> l = new ArrayList<>(10);
        l.add(e1);
        l.add(e2);
        l.add(e3);
        l.add(e4);
        l.add(e5);
        l.add(e6);
        l.add(e7);
        l.add(e8);
        l.add(e9);
        l.add(e10);
        return Collections.unmodifiableList(l);
    }

    /**
     * Returns an unmodifiable list containing an arbitrary number of elements.
     *
     * @apiNote
     * This method also accepts a single array as an argument. The element type of
     * the resulting list will be the component type of the array, and the size of
     * the list will be equal to the length of the array. To create a list with
     * a single element that is an array, do the following:
     *
     * <pre>{@code
     *     String[] array = ... ;
     *     List<String[]> list = Utils.<String[]>listOf(array);
     * }</pre>
     *
     * This will cause the {@link Utils#listOf(Object) listOf(E)} method
     * to be invoked instead.
     *
     * @param <E> the {@code List}'s element type
     * @param elements the elements to be contained in the list
     * @return a {@code List} containing the specified elements
     * @throws NullPointerException if an element is {@code null} or if the array is {@code null}
     */
    @SafeVarargs
    @SuppressWarnings("varargs")
    public static <E> List<E> listOf(E... elements) {
        switch (elements.length) { // implicit null check of elements
            case 0:
                return listOf();
            case 1:
                return listOf(elements[0]);
            case 2:
                return listOf(elements[0], elements[1]);
            default:
                for (int a = 0; a < elements.length; ++a) {
                    Objects.requireNonNull(elements[a], "e" + a);
                }
                return Collections.unmodifiableList(Arrays.asList(elements));
        }
    }

    /*
       Set.of implementations
       These methods are intended to provide replacements
       for the use of Set.of() methods in backports,
       using existing 8u methods. The returned collections
       share the key property of the Set.of collections
       in being unmodifiable, but may not be equivalent
       with regard to other properties such as serialization
       and access order.
    */

    /**
     * Returns an unmodifiable set containing zero elements.
     *
     * @param <E> the {@code Set}'s element type
     * @return an empty {@code Set}
     */
    public static <E> Set<E> setOf() {
        return Collections.emptySet();
    }

    /**
     * Returns an unmodifiable set containing one element.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the single element
     * @return a {@code Set} containing the specified element
     * @throws NullPointerException if the element is {@code null}
     */
    public static <E> Set<E> setOf(E e1) {
        Objects.requireNonNull(e1, "e1");

        Set<E> s = new HashSet<>(1);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing two elements.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if the elements are duplicates
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> Set<E> setOf(E e1, E e2) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");

        Set<E> s = new HashSet<>(2);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }
        if (!s.add(e2)) { throw new IllegalArgumentException("duplicate 2"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing three elements.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if there are any duplicate elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> Set<E> setOf(E e1, E e2, E e3) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");

        Set<E> s = new HashSet<>(10);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }
        if (!s.add(e2)) { throw new IllegalArgumentException("duplicate 2"); }
        if (!s.add(e3)) { throw new IllegalArgumentException("duplicate 3"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing four elements.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if there are any duplicate elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> Set<E> setOf(E e1, E e2, E e3, E e4) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");

        Set<E> s = new HashSet<>(10);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }
        if (!s.add(e2)) { throw new IllegalArgumentException("duplicate 2"); }
        if (!s.add(e3)) { throw new IllegalArgumentException("duplicate 3"); }
        if (!s.add(e4)) { throw new IllegalArgumentException("duplicate 4"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing five elements.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if there are any duplicate elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> Set<E> setOf(E e1, E e2, E e3, E e4, E e5) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");

        Set<E> s = new HashSet<>(10);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }
        if (!s.add(e2)) { throw new IllegalArgumentException("duplicate 2"); }
        if (!s.add(e3)) { throw new IllegalArgumentException("duplicate 3"); }
        if (!s.add(e4)) { throw new IllegalArgumentException("duplicate 4"); }
        if (!s.add(e5)) { throw new IllegalArgumentException("duplicate 5"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing six elements.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if there are any duplicate elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> Set<E> setOf(E e1, E e2, E e3, E e4, E e5, E e6) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");

        Set<E> s = new HashSet<>(10);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }
        if (!s.add(e2)) { throw new IllegalArgumentException("duplicate 2"); }
        if (!s.add(e3)) { throw new IllegalArgumentException("duplicate 3"); }
        if (!s.add(e4)) { throw new IllegalArgumentException("duplicate 4"); }
        if (!s.add(e5)) { throw new IllegalArgumentException("duplicate 5"); }
        if (!s.add(e6)) { throw new IllegalArgumentException("duplicate 6"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing seven elements.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @param e7 the seventh element
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if there are any duplicate elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> Set<E> setOf(E e1, E e2, E e3, E e4, E e5, E e6, E e7) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");
        Objects.requireNonNull(e7, "e7");

        Set<E> s = new HashSet<>(10);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }
        if (!s.add(e2)) { throw new IllegalArgumentException("duplicate 2"); }
        if (!s.add(e3)) { throw new IllegalArgumentException("duplicate 3"); }
        if (!s.add(e4)) { throw new IllegalArgumentException("duplicate 4"); }
        if (!s.add(e5)) { throw new IllegalArgumentException("duplicate 5"); }
        if (!s.add(e6)) { throw new IllegalArgumentException("duplicate 6"); }
        if (!s.add(e7)) { throw new IllegalArgumentException("duplicate 7"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing eight elements.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @param e7 the seventh element
     * @param e8 the eighth element
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if there are any duplicate elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> Set<E> setOf(E e1, E e2, E e3, E e4, E e5, E e6, E e7, E e8) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");
        Objects.requireNonNull(e7, "e7");
        Objects.requireNonNull(e8, "e8");

        Set<E> s = new HashSet<>(10);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }
        if (!s.add(e2)) { throw new IllegalArgumentException("duplicate 2"); }
        if (!s.add(e3)) { throw new IllegalArgumentException("duplicate 3"); }
        if (!s.add(e4)) { throw new IllegalArgumentException("duplicate 4"); }
        if (!s.add(e5)) { throw new IllegalArgumentException("duplicate 5"); }
        if (!s.add(e6)) { throw new IllegalArgumentException("duplicate 6"); }
        if (!s.add(e7)) { throw new IllegalArgumentException("duplicate 7"); }
        if (!s.add(e8)) { throw new IllegalArgumentException("duplicate 8"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing nine elements.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @param e7 the seventh element
     * @param e8 the eighth element
     * @param e9 the ninth element
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if there are any duplicate elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> Set<E> setOf(E e1, E e2, E e3, E e4, E e5, E e6, E e7, E e8, E e9) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");
        Objects.requireNonNull(e7, "e7");
        Objects.requireNonNull(e8, "e8");
        Objects.requireNonNull(e9, "e9");

        Set<E> s = new HashSet<>(10);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }
        if (!s.add(e2)) { throw new IllegalArgumentException("duplicate 2"); }
        if (!s.add(e3)) { throw new IllegalArgumentException("duplicate 3"); }
        if (!s.add(e4)) { throw new IllegalArgumentException("duplicate 4"); }
        if (!s.add(e5)) { throw new IllegalArgumentException("duplicate 5"); }
        if (!s.add(e6)) { throw new IllegalArgumentException("duplicate 6"); }
        if (!s.add(e7)) { throw new IllegalArgumentException("duplicate 7"); }
        if (!s.add(e8)) { throw new IllegalArgumentException("duplicate 8"); }
        if (!s.add(e9)) { throw new IllegalArgumentException("duplicate 9"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing ten elements.
     *
     * @param <E> the {@code Set}'s element type
     * @param e1 the first element
     * @param e2 the second element
     * @param e3 the third element
     * @param e4 the fourth element
     * @param e5 the fifth element
     * @param e6 the sixth element
     * @param e7 the seventh element
     * @param e8 the eighth element
     * @param e9 the ninth element
     * @param e10 the tenth element
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if there are any duplicate elements
     * @throws NullPointerException if an element is {@code null}
     */
    public static <E> Set<E> setOf(E e1, E e2, E e3, E e4, E e5, E e6, E e7, E e8, E e9, E e10) {
        Objects.requireNonNull(e1, "e1");
        Objects.requireNonNull(e2, "e2");
        Objects.requireNonNull(e3, "e3");
        Objects.requireNonNull(e4, "e4");
        Objects.requireNonNull(e5, "e5");
        Objects.requireNonNull(e6, "e6");
        Objects.requireNonNull(e7, "e7");
        Objects.requireNonNull(e8, "e8");
        Objects.requireNonNull(e9, "e9");
        Objects.requireNonNull(e10, "e10");

        Set<E> s = new HashSet<>(10);
        if (!s.add(e1)) { throw new IllegalArgumentException("duplicate 1"); }
        if (!s.add(e2)) { throw new IllegalArgumentException("duplicate 2"); }
        if (!s.add(e3)) { throw new IllegalArgumentException("duplicate 3"); }
        if (!s.add(e4)) { throw new IllegalArgumentException("duplicate 4"); }
        if (!s.add(e5)) { throw new IllegalArgumentException("duplicate 5"); }
        if (!s.add(e6)) { throw new IllegalArgumentException("duplicate 6"); }
        if (!s.add(e7)) { throw new IllegalArgumentException("duplicate 7"); }
        if (!s.add(e8)) { throw new IllegalArgumentException("duplicate 8"); }
        if (!s.add(e9)) { throw new IllegalArgumentException("duplicate 9"); }
        if (!s.add(e10)) { throw new IllegalArgumentException("duplicate 10"); }

        return Collections.unmodifiableSet(s);
    }

    /**
     * Returns an unmodifiable set containing an arbitrary number of elements.
     *
     * @apiNote
     * This method also accepts a single array as an argument. The element type of
     * the resulting set will be the component type of the array, and the size of
     * the set will be equal to the length of the array. To create a set with
     * a single element that is an array, do the following:
     *
     * <pre>{@code
     *     String[] array = ... ;
     *     Set<String[]> list = Utils.<String[]>setOf(array);
     * }</pre>
     *
     * This will cause the {@link Utils#setOf(Object) Utils.setOf(E)} method
     * to be invoked instead.
     *
     * @param <E> the {@code Set}'s element type
     * @param elements the elements to be contained in the set
     * @return a {@code Set} containing the specified elements
     * @throws IllegalArgumentException if there are any duplicate elements
     * @throws NullPointerException if an element is {@code null} or if the array is {@code null}
     */
    @SafeVarargs
    @SuppressWarnings("varargs")
    public static <E> Set<E> setOf(E... elements) {
        switch (elements.length) { // implicit null check of elements
            case 0:
                return setOf();
            case 1:
                return setOf(elements[0]);
            case 2:
                return setOf(elements[1]);
            default:
                Set<E> s = new HashSet<>(elements.length);
                for (int a = 0; a < elements.length; ++a) {
                    Objects.requireNonNull(elements[a], "e" + a);
                    if (!s.add(elements[a])) {
                        throw new IllegalArgumentException("duplicate " + a);
                    }
                }
                return Collections.unmodifiableSet(s);
        }
    }
}
