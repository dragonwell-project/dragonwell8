/*
 * @test
 * @summary Test load the class failed with EagerAppCDS flow
 * @library /lib/testlibrary /lib
 * @build Classes4CDS
 * @build trivial.TestSimple
 * @build TestSimpleWispUsage
 * @build TestLoadClassWithException
 * @requires os.arch=="amd64" | os.arch=="aarch64"
 * @run driver ClassFileInstaller -jar testSimple.jar trivial.TestSimple TestSimpleWispUsage
 * @run driver ClassFileInstaller -jar test.jar TestLoadClassWithException TestLoadClassWithException$1 TestLoadClassWithException$2
 * @run main/othervm -XX:+UnlockExperimentalVMOptions TestDumpAndLoadClassWithException
 */

import jdk.test.lib.cds.CDSTestUtils;
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

import java.util.ArrayList;
import java.util.List;
import java.io.*;

public class TestDumpAndLoadClassWithException {

    private static final String TESTJAR = "./test.jar";
    private static final String TESTNAME = "TestLoadClassWithException";
    private static final String TESTCLASS = TESTNAME + ".class";

    private static final String CLASSLIST_FILE = "./TestDumpAndLoadClassWithException.classlist";
    private static final String CLASSLIST_FILE_2 = "./TestDumpAndLoadClassWithException.classlist2";
    private static final String ARCHIVE_FILE = "./TestDumpAndLoadClassWithException.jsa";
    private static final String BOOTCLASS = "java.lang.Class";
    private static final String TEST_CLASS = System.getProperty("test.classes");

    public static void main(String[] args) throws Exception {

        // dump loaded classes into a classlist file
        dumpLoadedClasses(new String[] { BOOTCLASS, TESTNAME });

        convertClassList();

        // create an archive using the classlist
        dumpArchive();

        // change source path
        changeSourcePath();

        // start the java process with shared archive file
        startWithJsa();
    }

    static void dumpLoadedClasses(String[] expectedClasses) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true,
            "-Dtest.classes=" + TEST_CLASS,
            "-XX:DumpLoadedClassList=" + CLASSLIST_FILE,
            // trigger JVMCI runtime init so that JVMCI classes will be
            // included in the classlist
            "-XX:+UseCompressedOops",
            "-XX:+UseCompressedClassPointers",
            "-XX:+EagerAppCDS",
            "-cp",
            TESTJAR,
            TESTNAME);

        OutputAnalyzer output = CDSTestUtils.executeAndLog(pb, "dump-loaded-classes")
            .shouldHaveExitValue(0);
    }

    static void convertClassList() throws Exception {
        ProcessBuilder pb = Classes4CDS.invokeClasses4CDS(CLASSLIST_FILE, CLASSLIST_FILE_2);

        OutputAnalyzer output = CDSTestUtils.executeAndLog(pb, "convert-class-list")
            .shouldHaveExitValue(0);

    }
    static void dumpArchive() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true,
            "-cp",
            TESTJAR,
            "-XX:+UseCompressedOops",
            "-XX:+UseCompressedClassPointers",
            "-XX:+EagerAppCDS",
            "-XX:SharedClassListFile=" + CLASSLIST_FILE_2,
            "-XX:SharedArchiveFile=" + ARCHIVE_FILE,
            "-Xshare:dump",
            "-XX:MetaspaceSize=50M",
            "-XX:MaxMetaspaceSize=50M");

        OutputAnalyzer output = CDSTestUtils.executeAndLog(pb, "dump-archive");
        int exitValue = output.getExitValue();
        if (exitValue == 1) {
            output.shouldContain("Failed allocating metaspace object type");
        } else if (exitValue == 0) {
            output.shouldContain("Loading classes to share");
        } else {
            throw new RuntimeException("Unexpected exit value " + exitValue);
        }
    }

    static void changeSourcePath() throws Exception {
        String classPath = System.getProperty("test.classes");
        String newClassPath = "newpath";
        File tmpFile = new File(newClassPath);
        if (!tmpFile.exists()) {
            tmpFile.mkdirs();
        }
        File afile = new File("./testSimple.jar");
        afile.renameTo(new File(newClassPath + "/testSimple.jar"));
    }

    static void startWithJsa() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(true,
            "-Dtest.classes=" + TEST_CLASS,
            "-XX:+UseCompressedOops",
            "-XX:+UseCompressedClassPointers",
            "-XX:+EagerAppCDS",
            "-Xshare:on",
            "-XX:SharedArchiveFile=" + ARCHIVE_FILE,
            "-cp",
            TESTJAR,
            TESTNAME);

        OutputAnalyzer output = CDSTestUtils.executeAndLog(pb, "start-with-shared-archive")
            .shouldHaveExitValue(0);
        output.shouldNotContain("java.lang.ClassNotFoundException");
    }

}
