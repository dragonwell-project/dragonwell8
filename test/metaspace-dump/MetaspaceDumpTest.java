/*
 * Copyright (c) 2021, Alibaba Group Holding Limited. All rights reserved.
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

import static com.oracle.java.testlibrary.Asserts.*;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.lang.reflect.Field;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.Set;

import com.oracle.java.testlibrary.JDKToolLauncher;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

/**
 * @test metaspace dump
 * @library /testlibrary
 * @run main/othervm MetaspaceDumpTest
 */
public class MetaspaceDumpTest {

    private static String               DUMP_FILE_NAME = "java_pid%d.mprof";

    private static List<SectionChecker> checkers       = new ArrayList<>();

    static {
        checkers.add(new BasicInformationChecker());
        checkers.add(new ClassSpaceChecker());
        checkers.add(new NonClassSpacesChecker());
        checkers.add(new ClassFreeChunksChecker());
        checkers.add(new NonClassFreeChunksChecker());
        checkers.add(new GlobalSharedDataChecker());
        checkers.add(new ClassLoaderDataChecker());
        checkers.add(new UnloadingClassLoaderDataChecker());
        checkers.add(new EndInformationChecker());
    }

    public static void main(String[] args) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-XX:+UnlockDiagnosticVMOptions",
            "-XX:+MetaspaceDumpBeforeFullGC", "-XX:+MetaspaceDumpAfterFullGC",
            "-XX:+MetaspaceDumpOnOutOfMemoryError", "-XX:MaxMetaspaceSize=50m", "MetaspaceOOM");
        Process p = pb.start();
        OutputAnalyzer analyzer = new OutputAnalyzer(p);
        int pid = getPid(p);
        String fileName = String.format(DUMP_FILE_NAME, pid);
        checkFile(fileName);
        checkFile(fileName + ".1");
        checkFile(fileName + ".2");

        String dumpFileName = "metaspace_dump.log";
        JDKToolLauncher jcmdDump = JDKToolLauncher.create("jcmd")
            .addToolArg(String.valueOf(ProcessTools.getProcessId())).addToolArg("Metaspace.dump")
            .addToolArg("filename=" + dumpFileName);
        p = new ProcessBuilder(jcmdDump.getCommand()).start();
        analyzer = new OutputAnalyzer(p);
        checkFile(dumpFileName);
    }

    private static void checkFile(String fileName) throws Exception {
        File file = new File(fileName);
        assertTrue(file.exists());

        try (BufferedReader br = new BufferedReader(new FileReader(file))) {
            for (SectionChecker checker : checkers) {
                checker.check(br);
            }
        }
    }

    private static int getPid(Process process) throws Exception {

        Class<?> ProcessImpl = process.getClass();
        Field field = ProcessImpl.getDeclaredField("pid");
        field.setAccessible(true);
        return field.getInt(process);
    }
}

class MetaspaceOOM {
    public static void main(String[] args) throws Exception {
        File file = new File(MetaspaceOOM.class.getResource("/").getPath());
        URL url = file.toURI().toURL();
        List<Class> classList = new ArrayList<>();
        try {
            for (int i = 0; i < 1000000; i++) {
                if (new File(String.format("java_pid%d.mprof.2", ProcessTools.getProcessId())).exists()) {
                    return;
                }
                URLClassLoader loader = new URLClassLoader(new URL[] { url }, MetaspaceOOM.class
                    .getClassLoader().getParent());
                classList.add(loader.loadClass("MetaspaceOOM"));
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}

interface SectionChecker {
    void check(BufferedReader reader) throws Exception;
}

abstract class AbstractSectionChecker implements SectionChecker {

    public void check(BufferedReader reader) throws Exception {
        checkTitle(reader.readLine());
        checkContent(reader);
    }

    void checkTitle(String line) {
        assertTrue(title().equals(line));
    }

    abstract String title();

    abstract void checkContent(BufferedReader reader) throws Exception;
}

class BasicInformationChecker extends AbstractSectionChecker {

    String title() {
        return "[Basic Information]";
    }

    void checkContent(BufferedReader reader) throws Exception {
        // version
        String version = reader.readLine();
        Pattern pattern = Pattern.compile("Version : .+");
        assertTrue(pattern.matcher(version).matches());
        // dump source
        String source = reader.readLine();
        pattern = Pattern
            .compile("Dump Reason : (JCMD|BeforeFullGC|AfterFullGC|OnOutOfMemoryError)");
        Matcher matcher = pattern.matcher(source);
        assertTrue(matcher.matches());
        source = matcher.group(1);
        // MaxMetaspaceSize/CompressedClassSpaceSize
        pattern = Pattern.compile("MaxMetaspaceSize : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        pattern = Pattern.compile("CompressedClassSpaceSize : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        // OOM Info
        if (source.equals("OnOutOfMemoryError")) {
            pattern = Pattern.compile("Out Of Memory Area : (Class Metaspace|NonClass Metaspace)");
            assertTrue(pattern.matcher(reader.readLine()).matches());
            assertTrue("Out Of Memory Thread Information : [".equals(reader.readLine()));
            StringBuilder threadInfo = new StringBuilder();
            String tmp = null;
            while (!"]".equals(tmp = reader.readLine())) {
                threadInfo.append(tmp).append("\n");
            }
            if (threadInfo.toString().contains("\"main\"")) {
                assertTrue(threadInfo.toString()
                    .contains("at MetaspaceOOM.main(MetaspaceDumpTest.java"));
            }
        }
        // space basic info
        pattern = Pattern.compile("Class Space Used : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        pattern = Pattern.compile("Class Space Capacity : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        pattern = Pattern.compile("Class Space Committed : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        pattern = Pattern.compile("Class Space Reserved : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        pattern = Pattern.compile("NonClass Spaces Used : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        pattern = Pattern.compile("NonClass Spaces Capacity : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        pattern = Pattern.compile("NonClass Spaces Committed : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        pattern = Pattern.compile("NonClass Spaces Reserved : \\d+ B");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        // blnak line
        assertTrue("".equals(reader.readLine()));
    }
}

class ClassSpaceChecker extends AbstractSectionChecker {

    String title() {
        return "[Class Space]";
    }

    void checkContent(BufferedReader reader) throws Exception {
        Pattern pattern = Pattern
            .compile("\\* Space : \\[0x[0-9a-f]{16}, 0x[0-9a-f]{16}, 0x[0-9a-f]{16}, 0x[0-9a-f]{16}\\)");
        assertTrue(pattern.matcher(reader.readLine()).matches());
        // blnak line
        assertTrue("".equals(reader.readLine()));
    }
}

class NonClassSpacesChecker extends AbstractSectionChecker {

    String title() {
        return "[NonClass Spaces]";
    }

    void checkContent(BufferedReader reader) throws Exception {
        String tmp = null;
        boolean findInUsed = false;
        Pattern pattern = Pattern
            .compile("Space : \\[0x[0-9a-f]{16}, 0x[0-9a-f]{16}, 0x[0-9a-f]{16}, 0x[0-9a-f]{16}\\)");
        Pattern inUsedPattern = Pattern
            .compile("\\* Space : \\[0x[0-9a-f]{16}, 0x[0-9a-f]{16}, 0x[0-9a-f]{16}, 0x[0-9a-f]{16}\\)");
        while (!"".equals(tmp = reader.readLine())) {
            if (!pattern.matcher(tmp).matches()) {
                assertTrue(!findInUsed);
                assertTrue(inUsedPattern.matcher(tmp).matches());
                findInUsed = true;
            }
        }
    }
}

class ClassFreeChunksChecker extends AbstractSectionChecker {
    String title() {
        return "[Class Free Chunks]";
    }

    Pattern chunksPattern = Pattern.compile("Chunks : size = \\d+ B, count = (\\d+)");
    Pattern chunkPattern = Pattern.compile("  Chunk : 0x[0-9a-f]{16}");

    void checkContent(BufferedReader reader) throws Exception {
        String tmp = null;
        while (!"".equals(tmp = reader.readLine())) {
            Matcher matcher = chunksPattern.matcher(tmp);
            assertTrue(matcher.matches());
            int count = Integer.valueOf(matcher.group(1));
            while (count-- > 0) {
                assertTrue(chunkPattern.matcher(reader.readLine()).matches());
            }
        }
    }
}

class NonClassFreeChunksChecker extends ClassFreeChunksChecker {
    String title() {
        return "[NonClass Free Chunks]";
    }
}

class GlobalSharedDataChecker extends AbstractSectionChecker {
    String title() {
        return "[Global Shared Data]";
    }

    void checkContent(BufferedReader reader) throws Exception {
        assertTrue(Pattern
            .compile(
                "Array \\(_the_array_interfaces_array\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = \\d+")
            .matcher(reader.readLine()).matches());
        assertTrue(Pattern
            .compile(
                "Array \\(_the_empty_int_array\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = \\d+")
            .matcher(reader.readLine()).matches());
        assertTrue(Pattern
            .compile(
                "Array \\(_the_empty_short_array\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = \\d+")
            .matcher(reader.readLine()).matches());
        assertTrue(Pattern
            .compile(
                "Array \\(_the_empty_method_array\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = \\d+")
            .matcher(reader.readLine()).matches());
        assertTrue(Pattern
            .compile(
                "Array \\(_the_empty_klass_array\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = \\d+")
            .matcher(reader.readLine()).matches());
        // blnak line
        assertTrue("".equals(reader.readLine()));
    }
}

class ClassLoaderDataChecker extends AbstractSectionChecker {

    Pattern cld                     = Pattern
                                        .compile("(\\* )?ClassLoaderData : loader = 0x[0-9a-f]{16}, loader_klass = 0x[0-9a-f]{16}, loader_klass_name = .+, label = .+");
    Pattern chunk                   = Pattern
                                        .compile("    (\\* )?Chunk : \\[0x([0-9a-f]{16}), 0x[0-9a-f]{16}, 0x[0-9a-f]{16}\\)");
    Pattern blocks                  = Pattern.compile("    Blocks : size = \\d+ B, count = (\\d+)");
    Pattern block                   = Pattern.compile("      Block : 0x[0-9a-f]{16}");
    Pattern klass                   = Pattern
                                        .compile("    Klass : 0x[0-9a-f]{16}, name = .+, size = \\d+ B");
    Pattern constantPool            = Pattern
                                        .compile("      ConstantPool : 0x[0-9a-f]{16}, size = \\d+ B");
    Pattern constantPoolArray       = Pattern
                                        .compile("        Array \\((tags|jwp_tags|operands|reference_map)\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = \\d+");
    Pattern constantPoolCache       = Pattern
                                        .compile("        ConstantPoolCache : 0x[0-9a-f]{16}, size = \\d+ B");
    Pattern annotations             = Pattern
                                        .compile("      Annotations : 0x[0-9a-f]{16}, size = \\d+ B");
    Pattern annotationsArray        = Pattern
                                        .compile("        Array \\((class_annotations|fields_annotations|class_type_annotations|fields_type_annotations)\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = \\d+");
    Pattern annotationsArrayElement = Pattern
                                        .compile("          Array \\((fields_annotations_array|fields_type_annotations_array)\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = \\d+");
    Pattern klassArray              = Pattern
                                        .compile("      Array \\((local_interfaces|transitive_interfaces|secondary_supers|inner_classes|method_ordering|default_vtable_indices|fields|methods|default_methods)\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = (\\d+)");
    Pattern method                  = Pattern
                                        .compile("        Method : 0x[0-9a-f]{16}, name = .+, size = \\d+ B");
    Pattern constMethod             = Pattern
                                        .compile("          ConstMethod : 0x[0-9a-f]{16}, size = \\d+ B");
    Pattern constMethodArray        = Pattern
                                        .compile("            Array \\((stackmap_data|method_annotations|parameter_annotations|type_annotations|default_annotations)\\) : 0x[0-9a-f]{16}, size = \\d+ B, length = \\d+");
    Pattern methodData              = Pattern
                                        .compile("          MethodData : 0x[0-9a-f]{16}, size = \\d+ B");
    Pattern methodCounters          = Pattern
                                        .compile("          MethodCounters : 0x[0-9a-f]{16}, size = \\d+ B");

    Set<String> chunkBases          = new HashSet<>();

    String title() {
        return "[Class Loader Data]";
    }

    void checkContent(BufferedReader reader) throws Exception {
        String tmp = reader.readLine();
        while (!"".equals(tmp)) {
            assertTrue(cld.matcher(tmp).matches());
            String nextLine = reader.readLine();
            if ("  Class Used Chunks :".equals(nextLine)) {
                chunkBases.clear();
                checkChunk(reader, true);
                checkChunk(reader, false);
                tmp = checkKlass(reader);
            }
        }
    }

    String checkKlass(BufferedReader reader) throws Exception {
        String tmp = reader.readLine();
        while (!cld.matcher(tmp).matches() && !"".equals(tmp)) {
            assertTrue(klass.matcher(tmp).matches(), tmp);
            tmp = reader.readLine();
            if (tmp.contains("ConstantPool :")) {
                assertTrue(constantPool.matcher(tmp).matches());
                while ((tmp = reader.readLine()).startsWith("        ")) {
                    assertTrue(constantPoolArray.matcher(tmp).matches()
                               || constantPoolCache.matcher(tmp).matches());
                }
            }
            if (tmp.contains("Annotations :")) {
                assertTrue(annotations.matcher(tmp).matches(), tmp);
                while ((tmp = reader.readLine()).startsWith("        ")) {
                    assertTrue(annotationsArray.matcher(tmp).matches()
                               || annotationsArrayElement.matcher(tmp).matches(), tmp);
                }
            }
            while (tmp.contains("Array (")) {
                Matcher matcher = klassArray.matcher(tmp);
                assertTrue(matcher.matches());
                String arrayName = matcher.group(1);
                if (arrayName.equals("methods") || arrayName.equals("default_methods")) {
                    int length = Integer.valueOf(matcher.group(2));
                    tmp = reader.readLine();
                    while (length-- > 0) {
                        assertTrue(method.matcher(tmp).matches());
                        tmp = reader.readLine();
                        if (tmp.contains("ConstMethod :")) {
                            assertTrue(constMethod.matcher(tmp).matches());
                            while ((tmp = reader.readLine()).startsWith("            ")) {
                                assertTrue(constMethodArray.matcher(tmp).matches());
                            }
                        }
                        if (tmp.contains("MethodData :")) {
                            assertTrue(methodData.matcher(tmp).matches());
                            tmp = reader.readLine();
                        }
                        if (tmp.contains("MethodCounters :")) {
                            assertTrue(methodCounters.matcher(tmp).matches());
                            tmp = reader.readLine();
                        }
                    }
                } else {
                    tmp = reader.readLine();
                }
            }
        }
        return tmp;
    }

    void checkChunk(BufferedReader reader, boolean classUsed) throws Exception {
        String tmp = reader.readLine();
        while ((classUsed && !tmp.equals("  NonClass Used Chunks :"))
               || (!classUsed && !(tmp.equals("  Deallocate List :") || tmp.equals("  Klasses :")))) {
            Matcher chunkMatcher = chunk.matcher(tmp);
            if (chunkMatcher.matches()) {
                String chunkBase = chunkMatcher.group(2);
                assertTrue(chunkBases.add(chunkBase), chunkBase);
            } else {
                Matcher matcher = blocks.matcher(tmp);
                assertTrue(matcher.matches());
                int count = Integer.valueOf(matcher.group(1));
                while (count-- > 0) {
                    assertTrue(block.matcher(reader.readLine()).matches());
                }
            }
            tmp = reader.readLine();
        }
        if (tmp.equals("  Deallocate List :")) {
            while (!(tmp = reader.readLine()).equals("  Klasses :"))
                ;
        }
    }
}

class UnloadingClassLoaderDataChecker extends ClassLoaderDataChecker {
    String title() {
        return "[Unloading Class Loader Data]";
    }
}

class EndInformationChecker extends AbstractSectionChecker {

    Pattern pattern = Pattern.compile("Elapsed Time: \\d+ milliseconds");

    String title() {
        return "[End Information]";
    }

    void checkContent(BufferedReader reader) throws Exception {
        String time = reader.readLine();
        assertTrue(pattern.matcher(time).matches());
    }
}
