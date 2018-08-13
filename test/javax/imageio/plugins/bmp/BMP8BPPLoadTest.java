/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

/**
 * @test
 * @bug     8182461
 * @summary Test verifies that the 8BPP indexed color BMP image file is read properly
 * @requires BMP8BPPLoadTest.PNG
 * @run     main BMP8BPPLoadTest
 * @author  fairoz.matte
 */
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import javax.imageio.ImageIO;


public class BMP8BPPLoadTest {

    public BMP8BPPLoadTest() throws IOException {
        try {
            BufferedImage image = ImageIO.read(new File("BMP8BPPLoadTest.bmp"));
            System.out.println("Test Passed ImageIO.read able to read the file");
        } catch (IndexOutOfBoundsException iobe) {
            System.out.println("Test Failed with ImageIO.read throwing IndexOutOfBoundsException");
        }
    }


    public static void main(String args[]) throws IOException{
        BMP8BPPLoadTest test = new BMP8BPPLoadTest();
    }
}
