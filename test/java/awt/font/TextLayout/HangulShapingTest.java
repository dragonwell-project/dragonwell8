// Copyright 2019 Azul Systems, Inc.  All Rights Reserved.
// DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
//
// This code is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 2 only, as published by
// the Free Software Foundation.
//
// This code is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License version 2 for more
// details (a copy is included in the LICENSE file that accompanied this code).
//
// You should have received a copy of the GNU General Public License version 2
// along with this work; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
//
// Please contact Azul Systems, 385 Moffett Park Drive, Suite 115, Sunnyvale,
// CA 94089 USA or visit www.azul.com if you need additional information or
// have any questions.

import java.awt.Font;
import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.image.BufferedImage;

/*
 * @test
 * @bug 8215210
 * @summary Downport of prr's fix to a certain ICU wrong condition breaking some Hangul shaping
 * @run main/othervm  -Dsun.font.layoutengine=icu HangulShapingTest
 */
public class HangulShapingTest {
    public static void main(String args[]) {
        if (!System.getProperty("os.name").startsWith("Mac")) {
            return;
        }

        // images of the strings as drawn should be identical
        String beforeString = "\u1100\u1161 \u1102\u1161";
        String afterString = "\uAC00 \uB098";
        int w = 100, h = 100;

        BufferedImage bi1 = drawit(w, h, beforeString);
        BufferedImage bi2 = drawit(w, h, afterString);

        boolean same = true;
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                int c1 = bi1.getRGB(x, y);
                int c2 = bi2.getRGB(x, y);
                same &= (c1 == c2);
            }
            if (!same) {
               break;
            }
        }
        if (!same) {
            throw new RuntimeException("Images differ");
        }
    }
    private static BufferedImage drawit(int w, int h, String toDraw) {
        BufferedImage bi = new BufferedImage(w, h, BufferedImage.TYPE_INT_RGB);
        Graphics2D biGraphics = bi.createGraphics();
        biGraphics.setColor(Color.white);
        biGraphics.fillRect(0, 0, w, h);
        biGraphics.setColor(Color.black);
        Font font = new Font("Dialog", Font.PLAIN, 20);
        biGraphics.setFont(font);
        biGraphics.drawString(toDraw, 10, 40);
        return bi;
    }
}
