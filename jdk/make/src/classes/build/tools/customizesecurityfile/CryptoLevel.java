/*
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

package build.tools.customizesecurityfile;

import java.io.*;

/**
 * Alters the crypto.policy security property
 * if --enable-unlimited-crypto is enabled.
 */
public class CryptoLevel {

    private static final String PROP_NAME = "crypto.policy";

    public static void main(String[] args) throws Exception {
        boolean fileModified = false;

        if (args.length < 3) {
            System.err.println("Usage: java CryptoLevel" +
                               "[input java.security file name] " +
                               "[output java.security file name] " +
                               "[unlimited|limited]");
            System.exit(1);
        }
        if (!args[2].equals("unlimited") && !args[2].equals("limited")) {
            System.err.println("CryptoLevel error: Unexpected " +
                "input: " + args[2]);
            System.exit(1);
        }

        try (FileReader fr = new FileReader(args[0]);
             BufferedReader br = new BufferedReader(fr);
             FileWriter fw = new FileWriter(args[1]);
             BufferedWriter bw = new BufferedWriter(fw))
        {
            // parse the file line-by-line, looking for crypto.policy
            String line = br.readLine();
            while (line != null) {
                if (line.startsWith('#' + PROP_NAME) ||
                    line.startsWith(PROP_NAME)) {
                    writeLine(bw, PROP_NAME + "=" + args[2]);
                    fileModified = true;
                } else {
                    writeLine(bw, line);
                }
                line = br.readLine();
            }
            if (!fileModified) {
                //no previous setting seen. Insert at end
                writeLine(bw, PROP_NAME + "=" + args[2]);
            }
            bw.flush();
        }
    }

    private static void writeLine(BufferedWriter bw, String line)
        throws IOException
    {
        bw.write(line);
        bw.newLine();
    }
}
