/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.InvalidPathException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;

/**
 * Common library for various test jar file utility functions.
 */
public final class JarUtils {

    /**
     * Creates a JAR file.
     *
     * Equivalent to {@code jar cfm <jarfile> <manifest> -C <dir> file...}
     *
     * The input files are resolved against the given directory. Any input
     * files that are directories are processed recursively.
     */
    public static void createJarFile(Path jarfile, Manifest man, Path dir, Path... file)
        throws IOException
    {
        // create the target directory
        Path parent = jarfile.getParent();
        if (parent != null)
            Files.createDirectories(parent);

        List<Path> entries = new ArrayList<>();
        for (Path entry : file) {
            Files.find(dir.resolve(entry), Integer.MAX_VALUE,
                        (p, attrs) -> attrs.isRegularFile())
                    .map(e -> dir.relativize(e))
                    .forEach(entries::add);
        }

        try (OutputStream out = Files.newOutputStream(jarfile);
             JarOutputStream jos = new JarOutputStream(out))
        {
            if (man != null) {
                JarEntry je = new JarEntry(JarFile.MANIFEST_NAME);
                jos.putNextEntry(je);
                man.write(jos);
                jos.closeEntry();
            }

            for (Path entry : entries) {
                String name = toJarEntryName(entry);
                jos.putNextEntry(new JarEntry(name));
                Files.copy(dir.resolve(entry), jos);
                jos.closeEntry();
            }
        }
    }

   /**
     * Creates a JAR file.
     *
     * Equivalent to {@code jar cf <jarfile>  -C <dir> file...}
     *
     * The input files are resolved against the given directory. Any input
     * files that are directories are processed recursively.
     */
    public static void createJarFile(Path jarfile, Path dir, Path... file)
        throws IOException
    {
        createJarFile(jarfile, null, dir, file);
    }

    /**
     * Creates a JAR file from the contents of a directory.
     *
     * Equivalent to {@code jar cf <jarfile> -C <dir> .}
     */
    public static void createJarFile(Path jarfile, Path dir) throws IOException {
        createJarFile(jarfile, dir, Paths.get("."));
    }

    /**
     * Create jar file with specified files. If a specified file does not exist,
     * a new jar entry will be created with the file name itself as the content.
     */
    public static void createJar(String dest, String... files)
            throws IOException {
        try (JarOutputStream jos = new JarOutputStream(
                new FileOutputStream(dest), new Manifest())) {
            for (String file : files) {
                System.out.println(String.format("Adding %s to %s",
                        file, dest));

                // add an archive entry, and write a file
                jos.putNextEntry(new JarEntry(file));
                try (FileInputStream fis = new FileInputStream(file)) {
                    Utils.transferTo(fis, jos);
                } catch (FileNotFoundException e) {
                    jos.write(file.getBytes());
                }
            }
        }
        System.out.println();
    }

    /**
     * Add or remove specified files to existing jar file. If a specified file
     * to be updated or added does not exist, the jar entry will be created
     * with the file name itself as the content.
     *
     * @param src the original jar file name
     * @param dest the new jar file name
     * @param files the files to update. The list is broken into 2 groups
     *              by a "-" string. The files before in the 1st group will
     *              be either updated or added. The files in the 2nd group
     *              will be removed. If no "-" exists, all files belong to
     *              the 1st group.
     */
    public static void updateJar(String src, String dest, String... files)
            throws IOException {
        Map<String,Object> changes = new HashMap<>();
        boolean update = true;
        for (String file : files) {
            if (file.equals("-")) {
                update = false;
            } else if (update) {
                try {
                    Path p = Paths.get(file);
                    if (Files.exists(p)) {
                        changes.put(file, p);
                    } else {
                        changes.put(file, file);
                    }
                } catch (InvalidPathException e) {
                    // Fallback if file not a valid Path.
                    changes.put(file, file);
                }
            } else {
                changes.put(file, Boolean.FALSE);
            }
        }
        updateJar(src, dest, changes);
    }

    /**
     * Update content of a jar file.
     *
     * @param src the original jar file name
     * @param dest the new jar file name
     * @param changes a map of changes, key is jar entry name, value is content.
     *                Value can be Path, byte[] or String. If key exists in
     *                src but value is Boolean FALSE. The entry is removed.
     *                Existing entries in src not a key is unmodified.
     * @throws IOException
     */
    public static void updateJar(String src, String dest,
                                 Map<String,Object> changes)
            throws IOException {

        // What if input changes is immutable?
        changes = new HashMap<>(changes);

        System.out.printf("Creating %s from %s...\n", dest, src);

        if (dest.equals(src)) {
            throw new IOException("src and dest cannot be the same");
        }

        try (JarOutputStream jos = new JarOutputStream(
                new FileOutputStream(dest))) {

            try (JarFile srcJarFile = new JarFile(src)) {
                Enumeration<JarEntry> entries = srcJarFile.entries();
                while (entries.hasMoreElements()) {
                    JarEntry entry = entries.nextElement();
                    String name = entry.getName();
                    if (changes.containsKey(name)) {
                        System.out.println(String.format("- Update %s", name));
                        updateEntry(jos, name, changes.get(name));
                        changes.remove(name);
                    } else {
                        System.out.println(String.format("- Copy %s", name));
                        jos.putNextEntry(entry);
                        Utils.transferTo(srcJarFile.getInputStream(entry), jos);
                    }
                }
            }
            for (Map.Entry<String, Object> e : changes.entrySet()) {
                System.out.println(String.format("- Add %s", e.getKey()));
                updateEntry(jos, e.getKey(), e.getValue());
            }
        }
        System.out.println();
    }

    /**
     * Update the Manifest inside a jar.
     *
     * @param src the original jar file name
     * @param dest the new jar file name
     * @param man the Manifest
     *
     * @throws IOException
     */
    public static void updateManifest(String src, String dest, Manifest man)
            throws IOException {
        ByteArrayOutputStream bout = new ByteArrayOutputStream();
        man.write(bout);
        Map<String, Object> map = new HashMap<>();
        map.put(JarFile.MANIFEST_NAME, bout.toByteArray());
        updateJar(src, dest, map);
    }

    private static void updateEntry(JarOutputStream jos, String name, Object content)
           throws IOException {
        if (content instanceof Boolean) {
            if (((Boolean) content).booleanValue()) {
                throw new RuntimeException("Boolean value must be FALSE");
            }
        } else {
            jos.putNextEntry(new JarEntry(name));
            if (content instanceof Path) {
                Utils.transferTo(Files.newInputStream((Path) content), jos);
            } else if (content instanceof byte[]) {
                jos.write((byte[]) content);
            } else if (content instanceof String) {
                jos.write(((String) content).getBytes());
            } else {
                throw new RuntimeException("Unknown type " + content.getClass());
            }
        }
    }

    /**
     * Map a file path to the equivalent name in a JAR file
     */
    private static String toJarEntryName(Path file) {
        Path normalized = file.normalize();
        return normalized.subpath(0, normalized.getNameCount())  // drop root
                .toString()
                .replace(File.separatorChar, '/');
    }
}
