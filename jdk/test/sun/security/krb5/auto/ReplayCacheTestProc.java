/*
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @bug 7152176 8168518
 * @summary More krb5 tests
 * @library ../../../../java/security/testlibrary/
 * @compile -XDignore.symbol.file ReplayCacheTestProc.java
 * @run main/othervm/timeout=300 -Dsun.net.spi.nameservice.provider.1=ns,mock ReplayCacheTestProc
 */

import java.io.*;
import java.nio.BufferUnderflowException;
import java.nio.channels.SeekableByteChannel;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import jdk.test.lib.Platform;
import sun.security.jgss.GSSUtil;
import sun.security.krb5.internal.rcache.AuthTime;

/**
 * This test runs multiple acceptor Procs to mimic AP-REQ replays.
 * These system properties are supported:
 *
 * - test.libs on what types of acceptors to use
 *   Format: CSV of (J|N|N<suffix>=<libname>|J<suffix>=<launcher>)
 *   Default: J,N on Solaris and Linux where N is available, or J
 *   Example: J,N,N14=/krb5-1.14/lib/libgssapi_krb5.so,J8=/java8/bin/java
 *
 * - test.runs on manual runs. If empty, a iterate through all pattern
 *   Format: (req# | client# service#) acceptor# expected, ...
 *   Default: null
 *   Example: c0s0Jav,c1s1N14av,r0Jbx means 0th req is new c0->s0 sent to Ja,
 *            1st req is new c1 to s1 sent to N14a,
 *            2nd req is old (0th replayed) sent to Jb.
 *            a/b at the end of acceptor is different acceptors of the same lib
 *
 * - test.autoruns on number of automatic runs
 *   Format: number
 *   Default: 100
 */
public class ReplayCacheTestProc {

    private static Proc[] pa;   // all acceptors
    private static Proc pi;     // the single initiator
    private static List<Req> reqs = new ArrayList<>();
    private static String HOST = "localhost";

    // Where should the rcache be saved. It seems KRB5RCACHEDIR is not
    // recognized on Solaris. Maybe version too low? I see 1.6.
    private static String cwd =
            System.getProperty("os.name").startsWith("SunOS") ?
                "/var/krb5/rcache/" :
                System.getProperty("user.dir");

    private static MessageDigest md5, sha256;

    static {
        try {
            md5 = MessageDigest.getInstance("MD5");
            sha256 = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException nsae) {
            throw new AssertionError("Impossible", nsae);
        }
    }

    private static long uid;

    public static void main0(String[] args) throws Exception {
        System.setProperty("java.security.krb5.conf", OneKDC.KRB5_CONF);
        if (args.length == 0) { // The controller
            int nc = 5;     // number of clients
            int ns = 5;     // number of services
            String[] libs;  // available acceptor types:
                            // J: java
                            // J<suffix>=<java launcher>: another java
                            // N: default native lib
                            // N<suffix>=<libname>: another native lib
            Ex[] result;
            int numPerType = 2; // number of acceptors per type

            try {
                Class<?> clazz = Class.forName(
                        "com.sun.security.auth.module.UnixSystem");
                uid = (int)(long)(Long)
                        clazz.getMethod("getUid").invoke(clazz.newInstance());
            } catch (Exception e) {
                uid = -1;
            }

            KDC kdc = KDC.create(OneKDC.REALM, HOST, 0, true);
            for (int i=0; i<nc; i++) {
                kdc.addPrincipal(client(i), OneKDC.PASS);
            }
            kdc.addPrincipalRandKey("krbtgt/" + OneKDC.REALM);
            for (int i=0; i<ns; i++) {
                kdc.addPrincipalRandKey(service(i));
            }

            kdc.writeKtab(OneKDC.KTAB);
            KDC.saveConfig(OneKDC.KRB5_CONF, kdc);

            // User-provided libs
            String userLibs = System.getProperty("test.libs");

            if (userLibs != null) {
                libs = userLibs.split(",");
            } else {
                if (Platform.isOSX() || Platform.isWindows()) {
                    // macOS uses Heimdal and Windows has no native lib
                    libs = new String[]{"J"};
                } else {
                    if (acceptor("N", "sanity").waitFor() != 0) {
                        Proc.d("Native mode sanity check failed, only java");
                        libs = new String[]{"J"};
                    } else {
                        libs = new String[]{"J", "N"};
                    }
                }
            }

            pi = Proc.create("ReplayCacheTestProc").debug("C")
                    .args("initiator")
                    .start();

            int na = libs.length * numPerType;  // total number of acceptors
            pa = new Proc[na];

            // Acceptors, numPerType for 1st, numForType for 2nd, ...
            for (int i=0; i<na; i++) {
                pa[i] = acceptor(libs[i/numPerType],
                        "" + (char)('a' + i%numPerType));
            }

            // Manual runs
            String userRuns = System.getProperty("test.runs");

            if (userRuns == null) {
                result = new Ex[Integer.parseInt(
                        System.getProperty("test.autoruns", "100"))];
                Random r = new Random();
                for (int i = 0; i < result.length; i++) {
                    boolean expected = reqs.isEmpty() || r.nextBoolean();
                    result[i] = new Ex(
                            i,
                            expected ?
                                    req(r.nextInt(nc), r.nextInt(ns)) :
                                    r.nextInt(reqs.size()),
                            pa[r.nextInt(na)],
                            expected);
                }
            } else if (userRuns.isEmpty()) {
                int count = 0;
                result = new Ex[libs.length * libs.length];
                for (int i = 0; i < libs.length; i++) {
                    result[count] = new Ex(
                            count,
                            req(0, 0),
                            pa[i * numPerType],
                            true);
                    count++;
                    for (int j = 0; j < libs.length; j++) {
                        if (i == j) {
                            continue;
                        }
                        result[count] = new Ex(
                                count,
                                i,
                                pa[j * numPerType],
                                false);
                        count++;
                    }
                }
            } else {
                String[] runs = userRuns.split(",");
                result = new Ex[runs.length];
                for (int i = 0; i < runs.length; i++) {
                    UserRun run = new UserRun(runs[i]);
                    result[i] = new Ex(
                            i,
                            run.req() == -1 ?
                                    req(run.client(), run.service()) :
                                    result[run.req()].req,
                            Arrays.stream(pa)
                                    .filter(p -> p.debug().equals(run.acceptor()))
                                    .findFirst()
                                    .orElseThrow(() -> new Exception(
                                            "no acceptor named " + run.acceptor())),
                            run.success());
                }
            }

            for (Ex x : result) {
                x.run();
            }

            pi.println("END");
            for (int i=0; i<na; i++) {
                pa[i].println("END");
            }
            System.out.println("\nAll Test Results\n================");
            boolean finalOut = true;
            System.out.println("        req**  client    service  acceptor   Result");
            System.out.println("----  -------  ------  ---------  --------  -------");
            for (int i=0; i<result.length; i++) {
                boolean out = result[i].expected==result[i].actual;
                finalOut &= out;
                System.out.printf("%3d:    %3d%s      c%d    s%d %4s  %8s   %s  %s\n",
                        i,
                        result[i].req,
                        result[i].expected ? "**" : "  ",
                        reqs.get(result[i].req).client,
                        reqs.get(result[i].req).service,
                        "(" + result[i].csize + ")",
                        result[i].acceptor.debug(),
                        result[i].actual ? "++" : "--",
                        out ? "   " : "xxx");
            }

            System.out.println("\nPath of Reqs\n============");
            for (int j=0; ; j++) {
                boolean found = false;
                for (int i=0; i<result.length; i++) {
                    if (result[i].req == j) {
                        if (!found) {
                            System.out.printf("%3d (c%s -> s%s): ", j,
                                    reqs.get(j).client, reqs.get(j).service);
                        }
                        System.out.printf("%s%s(%d)%s",
                                found ? " -> " : "",
                                result[i].acceptor.debug(),
                                i,
                                result[i].actual != result[i].expected ?
                                        "xxx" : "");
                        found = true;
                    }
                }
                System.out.println();
                if (!found) {
                    break;
                }
            }
            if (!finalOut) throw new Exception();
        } else if (args[0].equals("Nsanity")) {
            // Native mode sanity check
            Proc.d("Detect start");
            Context s = Context.fromUserKtab("*", OneKDC.KTAB, true);
            s.startAsServer(GSSUtil.GSS_KRB5_MECH_OID);
        } else if (args[0].equals("initiator")) {
            while (true) {
                String title = Proc.textIn();
                Proc.d("Client see " + title);
                if (title.equals("END")) break;
                String[] cas = title.split(" ");
                Context c = Context.fromUserPass(cas[0], OneKDC.PASS, false);
                c.startAsClient(cas[1], GSSUtil.GSS_KRB5_MECH_OID);
                c.x().requestCredDeleg(true);
                byte[] token = c.take(new byte[0]);
                Proc.d("Client AP-REQ generated");
                Proc.binOut(token);
            }
        } else {
            Proc.d(System.getProperty("java.vm.version"));
            Proc.d(System.getProperty("sun.security.jgss.native"));
            Proc.d(System.getProperty("sun.security.jgss.lib"));
            Proc.d("---------------------------------\n");
            Proc.d("Server start");
            Context s = Context.fromUserKtab("*", OneKDC.KTAB, true);
            Proc.d("Server login");
            while (true) {
                String title = Proc.textIn();
                Proc.d("Server sees " + title);
                if (title.equals("END")) break;
                s.startAsServer(GSSUtil.GSS_KRB5_MECH_OID);
                byte[] token = Proc.binIn();
                try {
                    s.take(token);
                    Proc.textOut("true");
                    Proc.d("Good");
                } catch (Exception e) {
                    Proc.textOut("false");
                    Proc.d("Bad");
                }
            }
        }
    }

    public static void main(String[] args) throws Exception {
        try {
            main0(args);
        } catch (Exception e) {
            Proc.d(e);
            throw e;
        }
    }

    // returns the client name
    private static String client(int p) {
        return "client" + p;
    }

    // returns the service name
    private static String service(int p) {
        return "service" + p + "/" + HOST;
    }

    // returns the dfl name for a service
    private static String dfl(int p) {
        return "service" + p + (uid == -1 ? "" : ("_"+uid));
    }

    // generates an ap-req and save into reqs, returns the index
    private static int req(int client, int service) throws Exception {
        pi.println(client(client) + " " + service(service));
        Req req = new Req(client, service, pi.readData());
        reqs.add(req);
        return reqs.size() - 1;
    }

    // create a acceptor
    private static Proc acceptor(String type, String suffix) throws Exception {
        Proc p;
        String label;
        String lib;
        int pos = type.indexOf('=');
        if (pos < 0) {
            label = type;
            lib = null;
        } else {
            label = type.substring(0, pos);
            lib = type.substring(pos + 1);
        }
        if (type.startsWith("J")) {
            if (lib == null) {
                p = Proc.create("ReplayCacheTestProc");
            } else {
                p = Proc.create("ReplayCacheTestProc", lib);
            }
            p.prop("sun.security.krb5.rcache", "dfl")
                    .prop("java.io.tmpdir", cwd)
                    .prop("sun.net.spi.nameservice.provider.1", "ns,mock");
            String useMD5 = System.getProperty("jdk.krb5.rcache.useMD5");
            if (useMD5 != null) {
                p.prop("jdk.krb5.rcache.useMD5", useMD5);
            }
        } else {
            p = Proc.create("ReplayCacheTestProc")
                    .env("KRB5_CONFIG", OneKDC.KRB5_CONF)
                    .env("KRB5_KTNAME", OneKDC.KTAB)
                    .env("KRB5RCACHEDIR", cwd)
                    .prop("sun.security.jgss.native", "true")
                    .prop("javax.security.auth.useSubjectCredsOnly", "false")
                    .prop("sun.security.nativegss.debug", "true")
                    .prop("sun.net.spi.nameservice.provider.1", "ns,mock");
            if (lib != null) {
                String libDir = lib.substring(0, lib.lastIndexOf('/'));
                p.prop("sun.security.jgss.lib", lib)
                        .env("DYLD_LIBRARY_PATH", libDir)
                        .env("LD_LIBRARY_PATH", libDir);
            }
        }
        Proc.d(label+suffix+" started");
        return p.args(label+suffix).debug(label+suffix).start();
    }

    // generates hash of authenticator inside ap-req inside initsectoken
    private static void record(String label, Req req) throws Exception {
        byte[] data = Base64.getDecoder().decode(req.msg);
        data = Arrays.copyOfRange(data, 17, data.length);

        try (PrintStream ps = new PrintStream(
                new FileOutputStream("log.txt", true))) {
            ps.printf("%s:\nmsg: %s\nMD5: %s\nSHA-256: %s\n\n",
                    label,
                    req.msg,
                    hex(md5.digest(data)),
                    hex(sha256.digest(data)));
        }
    }

    // Returns a compact hexdump for a byte array
    private static String hex(byte[] hash) {
        char[] h = new char[hash.length * 2];
        char[] hexConst = "0123456789ABCDEF".toCharArray();
        for (int i=0; i<hash.length; i++) {
            h[2*i] = hexConst[(hash[i]&0xff)>>4];
            h[2*i+1] = hexConst[hash[i]&0xf];
        }
        return new String(h);
    }

    // return size of dfl file, excluding the null hash ones
    private static int csize(int p) throws Exception {
        try (SeekableByteChannel chan = Files.newByteChannel(
                Paths.get(cwd, dfl(p)), StandardOpenOption.READ)) {
            chan.position(6);
            int cc = 0;
            while (true) {
                try {
                    if (AuthTime.readFrom(chan) != null) cc++;
                } catch (BufferUnderflowException e) {
                    break;
                }
            }
            return cc;
        } catch (IOException ioe) {
            return 0;
        }
    }

    // models an experiement
    private static class Ex {
        int i;              // #
        int req;            // which ap-req to send
        Proc acceptor;      // which acceptor to send to
        boolean expected;   // expected result

        boolean actual;     // actual output
        int csize;          // size of rcache after test
        String hash;        // the hash of req

        Ex(int i, int req, Proc acceptor, boolean expected) {
            this.i = i;
            this.req = req;
            this.acceptor = acceptor;
            this.expected = expected;
        }

        void run() throws Exception {
            Req r = reqs.get(req);
            acceptor.println("TEST");
            acceptor.println(r.msg);
            String reply = acceptor.readData();

            actual = Boolean.valueOf(reply);
            csize = csize(r.service);

            String label = String.format("%03d-CLIENT%d-SERVICE%d-%s-%s",
                    i, r.client, r.service, acceptor.debug(), actual);

            record(label, r);
            if (new File(cwd, dfl(r.service)).exists()) {
                Files.copy(Paths.get(cwd, dfl(r.service)), Paths.get(label),
                        StandardCopyOption.COPY_ATTRIBUTES);
            }
        }
    }

    // models a saved ap-req msg
    private static class Req {
        String msg;         // based64-ed req
        int client;         // which client
        int service;        // which service
        Req(int client, int service, String msg) {
            this.msg = msg;
            this.client= client;
            this.service = service;
        }
    }

    private static class UserRun {
        static final Pattern p
                = Pattern.compile("(c(\\d)+s(\\d+)|r(\\d+))(.*)(.)");
        final Matcher m;

        UserRun(String run) { m = p.matcher(run); m.find(); }

        int req() { return group(4); }
        int client() { return group(2); }
        int service() { return group(3); }
        String acceptor() { return m.group(5); }
        boolean success() { return m.group(6).equals("v"); }

        int group(int i) {
            String g = m.group(i);
            return g == null ? -1 : Integer.parseInt(g);
        }
    }
}
