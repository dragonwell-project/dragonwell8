/*
 * @test
 * @summary Test files assemblied into JDK
 * @library /test/lib
 * @library /lib/testlibrary
 * @requires os.arch=="amd64" | os.arch=="aarch64"
 * @run main/othervm TestAssemblyFiles
 */

import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.jar.JarFile;

import static jdk.testlibrary.Asserts.assertTrue;

public class TestAssemblyFiles {

    private static final String SERVERLESS_ADAPTER    = "serverless/serverless-adapter.jar";
    private static final String QUICKSTART_CONFIG     = "quickstart.properties";

    public static void main(String[] args) throws Exception {
        verifyAssemblyFiles();
    }

    private static Path getJDKHome() {
        String jdkHome = System.getProperty("java.home");
        if (!new File(jdkHome).exists()) {
            throw new Error("Fatal error, cannot find jdk path: [" + jdkHome + "] doesn't exist!");
        }
        return Paths.get(jdkHome);
    }

    private static Path getJDKLibDir() {
        return getJDKHome().resolve("lib");
    }

    private static void verifyAssemblyFiles() throws Exception {
        verifyQuickStartProperties();
        verifyServerlessAdapter();
    }

    private static void verifyQuickStartProperties() {
        assertTrue(getJDKLibDir().resolve(QUICKSTART_CONFIG).toFile().exists(), getJDKLibDir().resolve(QUICKSTART_CONFIG) + " doesn't have " + QUICKSTART_CONFIG);
    }

    private static void verifyServerlessAdapter() {
        String osArch = System.getProperty("os.arch");
        assertTrue(getJDKLibDir().resolve(osArch).resolve(SERVERLESS_ADAPTER).toFile().exists(), "must have " + SERVERLESS_ADAPTER);
    }

}
