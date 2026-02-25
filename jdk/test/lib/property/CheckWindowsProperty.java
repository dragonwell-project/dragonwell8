/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8345358
 * @run main/othervm CheckWindowsProperty
 * @summary file property check for Windows .exe/.dll
 * @requires os.family == "windows"
 */

import java.io.BufferedReader;
import java.io.InputStreamReader;

public class CheckWindowsProperty {
    public static void main(String[] args) throws Exception {
        String targetDir = System.getProperty("test.jdk") + "\\";
        String psCommand = String.format(
            "Get-ChildItem -Path '%s' -include *.exe,*.dll -Recurse -File | " +
            "ForEach-Object { " +
                "$vi = $_.VersionInfo; " +
                "@($vi.FileName, $vi.CompanyName, $vi.FileDescription, $vi.FileVersion, " +
                "$vi.InternalName, $vi.Language, $vi.LegalCopyright, $vi.OriginalFilename, " +
                "$vi.ProductName, $vi.ProductVersion) -join ';'" +
            "}",
            targetDir
        );

        ProcessBuilder pb = new ProcessBuilder("powershell", "-NoLogo", "-NoProfile",
            "-Command", psCommand).redirectErrorStream(true);
        Process p = pb.start();
        p.getOutputStream().close();

        try (BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()))) {
            for (String line = br.readLine(); line != null; line = br.readLine()) {
                checkFileProperty(line);
            }
        }
        p.waitFor();
        int exitCode = p.exitValue();
        if (exitCode != 0) {
            throw new RuntimeException("ExitCode is " + exitCode + ". PowerShell command failed.");
        }
    }

    static void checkFileProperty(String line) {
        // line format
        // FileName;CompanyName;FileDescription;FileVersion;InternalName;Language;LegalCopyright;
        // OriginalFilename;ProductName;ProductVersion
        String[] data = line.split(";", -1);
        String filename = data[0].substring(data[0].lastIndexOf(System.getProperty("file.separator")) + 1);

        // skip Microsoft redist dll
        if (filename.startsWith("api-ms-win") ||
            filename.startsWith("msvcp") ||
            filename.startsWith("ucrtbase") ||
            filename.startsWith("vcruntime")) {
            return;
        }

        for (int i = 1; i < data.length; i++) {
            if (data[i] == null || data[i].isEmpty()) {
                if (!filename.equals("freetype.dll") && !filename.equals("sawindbg.dll")) {
                    throw new RuntimeException("data[" + i + "] should not be empty for " + filename);
                }
            }
        }
    }
}
