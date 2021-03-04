/*
 * Copyright (c) 2019, Red Hat, Inc.
 *
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

import java.io.File;
import java.io.FilePermission;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;

import java.security.AccessControlContext;
import java.security.AccessControlException;
import java.security.AccessController;
import java.security.AllPermission;
import java.security.CodeSource;
import java.security.Permission;
import java.security.PermissionCollection;
import java.security.Permissions;
import java.security.Policy;
import java.security.PrivilegedAction;
import java.security.ProtectionDomain;

import java.time.Duration;

import java.util.ArrayList;
import java.util.Comparator;

import jdk.jfr.AnnotationElement;
import jdk.jfr.Configuration;
import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordingFile;
import jdk.jfr.Event;
import jdk.jfr.EventFactory;
import jdk.jfr.EventType;
import jdk.jfr.FlightRecorder;
import jdk.jfr.FlightRecorderListener;
import jdk.jfr.FlightRecorderPermission;
import jdk.jfr.internal.JVM;
import jdk.jfr.Label;
import jdk.jfr.Name;
import jdk.jfr.Recording;
import jdk.jfr.ValueDescriptor;

import jdk.test.lib.Platform;

/*
 * @test
 * @requires (jdk.version.major >= 8)
 * @library /lib /
 * @key jfr
 * @run main/othervm/timeout=30 -XX:+FlightRecorder -XX:StartFlightRecording JFRSecurityTestSuite
 * @author Martin Balao (mbalao@redhat.com)
 */

public class JFRSecurityTestSuite {

    private static boolean DEBUG = true;
    private static SecurityManager sm;
    private static String failureMessage = null;
    private static Path jfrTmpDirPath = null;
    private static Path recFilePath = null;
    private static String protectedLocationPath;

    interface F {
        void call() throws Exception;
    }

    @Label("MyEvent")
    static class MyEvent extends Event {
    }

    @Label("MyEvent2")
    static class MyEvent2 extends Event {
    }

    @Label("MyEvent3")
    static class MyEvent3 extends Event {
    }

    public static class MyPolicy extends Policy {
        public MyPolicy() {
            super();
        }
        @Override
        public PermissionCollection getPermissions(ProtectionDomain domain) {
            PermissionCollection perms = new Permissions();
            perms.add(new AllPermission());
            return perms;
        }
    }

    private static class MyProtectionDomain extends ProtectionDomain {
        MyProtectionDomain(CodeSource source, Permissions permissions) {
            super(source, permissions);
        }

        public boolean implies(Permission permission) {
            if (DEBUG) {
                System.out.println("Permission checked: " + permission);
            }
            return super.implies(permission);
        }
    }

    private static final Runnable periodicEvent = new Runnable() {
                @Override
                public void run() {
                    if (DEBUG) {
                        System.out.println("Periodic event");
                    }
                }
            };
    private static final FlightRecorderListener frl =
            new FlightRecorderListener() { };

    public static void main(String[] args) throws Throwable {

        // Temporary directory for JFR files
        jfrTmpDirPath = Files.createTempDirectory("jfr_test");

        try {
            setProtectedLocation();

            File recFile = new File(jfrTmpDirPath.toString(), "rec");
            recFile.createNewFile();
            recFilePath = recFile.toPath();
            if (DEBUG) {
                System.out.println("Test JFR tmp directory: " + jfrTmpDirPath);
            }

            // Create a SecurityManager with all permissions enabled
            Policy.setPolicy(new MyPolicy());
            sm = new SecurityManager();
            System.setSecurityManager(sm);

            CodeSource source =
                    Thread.currentThread().getClass().
                            getProtectionDomain().getCodeSource();

            // Create a constrained execution environment
            Permissions noPermissions = new Permissions();
            ProtectionDomain constrainedPD = new MyProtectionDomain(source,
                    noPermissions);
            AccessControlContext constrainedContext =
                    new AccessControlContext(new ProtectionDomain[] {
                            constrainedPD });

            AccessController.doPrivileged(new PrivilegedAction<Void>() {
                @Override
                public Void run() {
                    try {
                        doInConstrainedEnvironment();
                    } catch (Throwable t) {
                        if (DEBUG) {
                            t.printStackTrace();
                        }
                        failureMessage = t.toString();
                    }
                    return null;
                }
            }, constrainedContext);

            // Create a JFR execution environment
            Permissions JFRPermissions = new Permissions();
            JFRPermissions.add(new FilePermission(recFilePath.toString(),
                    "read,write"));
            JFRPermissions.add(new FlightRecorderPermission(
                    "accessFlightRecorder"));
            JFRPermissions.add(new FlightRecorderPermission(
                    "registerEvent"));
            JFRPermissions.add(new RuntimePermission(
                    "accessDeclaredMembers"));
            ProtectionDomain JFRPD = new MyProtectionDomain(source,
                    JFRPermissions);
            AccessControlContext JFRContext =
                    new AccessControlContext(new ProtectionDomain[] { JFRPD });

            AccessController.doPrivileged(new PrivilegedAction<Void>() {
                @Override
                public Void run() {
                    try {
                        doInJFREnvironment();
                    } catch (Throwable t) {
                        if (DEBUG) {
                            t.printStackTrace();
                        }
                        failureMessage = t.toString();
                    }
                    return null;
                }
            }, JFRContext);

            if (failureMessage != null) {
                throw new Exception("TEST FAILED" + System.lineSeparator() +
                        failureMessage);
            }
            System.out.println("TEST PASS - OK");

        } finally {
            Files.walk(jfrTmpDirPath)
                    .sorted(Comparator.reverseOrder())
                    .map(Path::toFile)
                    .forEach(File::delete);
        }
    }

    private static void setProtectedLocation() {
        if (Platform.isWindows()) {
            protectedLocationPath = System.getenv("%WINDIR%");
            if (protectedLocationPath == null) {
                // fallback
                protectedLocationPath = "c:\\windows";
            }
        } else {
            protectedLocationPath = "/etc";
        }
    }

    private static void doInConstrainedEnvironment() throws Throwable {
        checkNoDirectAccess();

        assertPermission(() -> {
                    FlightRecorder.getFlightRecorder();
                }, FlightRecorderPermission.class, "accessFlightRecorder", false);

        assertPermission(() -> {
                    FlightRecorder.register(MyEvent.class);
                }, FlightRecorderPermission.class, "registerEvent", false);

        assertPermission(() -> {
                    FlightRecorder.unregister(MyEvent.class);
                }, FlightRecorderPermission.class, "registerEvent", false);

        assertPermission(() -> {
                    FlightRecorder.addPeriodicEvent(MyEvent.class, periodicEvent);
                }, FlightRecorderPermission.class, "registerEvent", false);

        assertPermission(() -> {
                    FlightRecorder.removePeriodicEvent(periodicEvent);
                }, FlightRecorderPermission.class, "registerEvent", false);

        assertPermission(() -> {
                    FlightRecorder.addListener(frl);
                }, FlightRecorderPermission.class, "accessFlightRecorder", false);

        assertPermission(() -> {
                FlightRecorder.removeListener(frl);
                }, FlightRecorderPermission.class, "accessFlightRecorder", false);

        assertPermission(() -> {
                    new MyEvent().commit();
                }, FlightRecorderPermission.class, "registerEvent", true);

        assertPermission(() -> {
                        Configuration.create(Paths.get(protectedLocationPath));
                }, FilePermission.class, protectedLocationPath, false);

        assertPermission(() -> {
                    EventFactory.create(new ArrayList<AnnotationElement>(),
                            new ArrayList<ValueDescriptor>());
                }, FlightRecorderPermission.class, "registerEvent", false);

        assertPermission(() -> {
                    new AnnotationElement(Name.class, "com.example.HelloWorld");
                }, FlightRecorderPermission.class, "registerEvent", false);

        assertPermission(() -> {
                new ValueDescriptor(MyEvent.class, "",
                        new ArrayList<AnnotationElement>());
                }, FlightRecorderPermission.class, "registerEvent", false);

        assertPermission(() -> {
                    new Recording();
                }, FlightRecorderPermission.class, "accessFlightRecorder", false);

        assertPermission(() -> {
                        new RecordingFile(Paths.get(protectedLocationPath));
                }, FilePermission.class, protectedLocationPath, false);

        assertPermission(() -> {
                        RecordingFile.readAllEvents(Paths.get(protectedLocationPath));
                }, FilePermission.class, protectedLocationPath, false);

        assertPermission(() -> {
                        EventType.getEventType(MyEvent2.class);
                }, FlightRecorderPermission.class, "registerEvent", true);
    }

    private static void doInJFREnvironment() throws Throwable {

        checkNoDirectAccess();

        FlightRecorder fr = FlightRecorder.getFlightRecorder();

        Configuration c = Configuration.getConfiguration("default");

        Recording recordingDefault = new Recording(c);

        recordingDefault.start();

        FlightRecorder.addListener(frl);

        FlightRecorder.register(MyEvent3.class);

        FlightRecorder.addPeriodicEvent(MyEvent3.class, periodicEvent);

        new MyEvent3().commit();

        FlightRecorder.removePeriodicEvent(periodicEvent);

        FlightRecorder.unregister(MyEvent3.class);

        FlightRecorder.removeListener(frl);

        recordingDefault.stop();

        try (Recording snapshot = fr.takeSnapshot()) {
            if (snapshot.getSize() > 0) {
                snapshot.setMaxSize(100_000_000);
                snapshot.setMaxAge(Duration.ofMinutes(5));
                snapshot.dump(recFilePath);
            }
        }
        checkRecording();

        Files.write(recFilePath, new byte[0],
                StandardOpenOption.TRUNCATE_EXISTING);
        recordingDefault.dump(recFilePath);
        checkRecording();

        try {
            class MyEventHandler extends jdk.jfr.internal.handlers.EventHandler {
                MyEventHandler() {
                    super(true, null, null);
                }
            }
            MyEventHandler myEv = new MyEventHandler();
            throw new Exception("EventHandler must not be subclassable");
         } catch (SecurityException e) {}
    }

    private static void checkRecording() throws Throwable {
        boolean myEvent3Found = false;
        try (RecordingFile rf = new RecordingFile(recFilePath)) {
            while (rf.hasMoreEvents()) {
                RecordedEvent event = rf.readEvent();
                if (event.getEventType().getName().equals(
                        "JFRSecurityTestSuite$MyEvent3")) {
                    if (DEBUG) {
                        System.out.println("Recording of MyEvent3 event:");
                        System.out.println(event);
                    }
                    myEvent3Found = true;
                    break;
                }
            }
        }
        if (!myEvent3Found) {
            throw new Exception("MyEvent3 event was expected in JFR recording");
        }
    }

    private static void checkNoDirectAccess() throws Throwable {
        assertPermission(() -> {
                    sm.checkPackageAccess("jdk.jfr.internal");
                }, RuntimePermission.class, null, false);

        assertPermission(() -> {
                    Class.forName("jdk.jfr.internal.JVM");
                }, RuntimePermission.class, null, false);

        assertPermission(() -> {
                    JVM.getJVM();
                }, RuntimePermission.class, null, false);

        assertPermission(() -> {
                    sm.checkPackageAccess("jdk.jfr.events");
                }, RuntimePermission.class, null, false);

        assertPermission(() -> {
                    Class.forName("jdk.jfr.events.AbstractJDKEvent");
                }, RuntimePermission.class, null, false);

        assertPermission(() -> {
                    sm.checkPackageAccess("jdk.management.jfr.internal");
                }, RuntimePermission.class, null, false);
    }

    private static void assertPermission(F f, Class<?> expectedPermClass,
            String expectedPermName, boolean expectedIsCause)
            throws Throwable {
        String exceptionMessage = expectedPermClass.toString() +
                (expectedPermName != null && !expectedPermName.isEmpty() ?
                        " " + expectedPermName : "") +
                " permission check expected.";
        try {
            f.call();
            throw new Exception(exceptionMessage);
        } catch (Throwable t) {
            Throwable t2 = null;
            if (expectedIsCause) {
                t2 = t.getCause();
            } else {
                t2 = t;
            }
            if (t2 instanceof AccessControlException) {
                AccessControlException ace = (AccessControlException)t2;
                Permission p = ace.getPermission();
                if (!p.getClass().equals(expectedPermClass) ||
                        (expectedPermName != null && !expectedPermName.isEmpty() &&
                                !p.getName().equals(expectedPermName))) {
                    throw new Exception(exceptionMessage, ace);
                }
            } else {
                throw t;
            }
        }
    }
}

