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
 * @summary Fix to 8215210 should not break RTL with AAT fonts.
 * @run main/othervm  -Dsun.font.layoutengine=icu HebrewIsRTLTest
 */
public class HebrewIsRTLTest {
    static final String hebrewString = "\u05E9\u059E\u05E9\u0595\u05E9\u05A9\u05E9\u0592\u05E9\u0599\u05E9\u059E\u05E9\u0595\u05E9\u05A9\u05E9\u0592\u05E9\u0599  .  \u05E9\u0599\u05E9\u05A1\u05E9\u0595\u05E9\u0593";
    public static void main(String args[]) {
        if (!System.getProperty("os.name").startsWith("Mac")) {
            return;
        }

        // calculate text size
        BufferedImage biMetrics = new BufferedImage(1000, 1000, BufferedImage.TYPE_INT_RGB);
        Graphics2D biMetricsGraphics = biMetrics.createGraphics();
        Font font = new Font("TimesRoman", Font.PLAIN, 40);
        biMetricsGraphics.setFont(font);
        int width = biMetricsGraphics.getFontMetrics().stringWidth(hebrewString);
        int height = biMetricsGraphics.getFontMetrics().getHeight();

        // create minimal image
        BufferedImage bi = new BufferedImage(width, height, BufferedImage.TYPE_INT_RGB);
        Graphics2D biGraphics = bi.createGraphics();
        biGraphics.setColor(Color.white);
        biGraphics.fillRect(0, 0, width, height);
        biGraphics.setColor(Color.black);
        biGraphics.setFont(font);
        biGraphics.drawString(hebrewString, 0, height);

        int y = bi.getHeight() / 2;
        int x;
        int rgb, rgbLeftCount = 0, rgbRightCount = 0;

        for (x = 0; x < bi.getWidth()/2; x++) {
            rgb = bi.getRGB(x, y);
            if (rgb == Color.BLACK.getRGB()) {
                rgbLeftCount++;
            }
        }
        for (x = bi.getWidth()/2 + 1; x < bi.getWidth(); x++) {
            rgb = bi.getRGB(x, y);
            if (rgb == Color.BLACK.getRGB()) {
                rgbRightCount++;
            }
        }
        if (rgbLeftCount > rgbRightCount) {
            throw new RuntimeException("Hebrew text seems drawn LTR");
        }
    }
}
