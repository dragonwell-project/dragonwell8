/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 4906370
 * @summary Tests to excercise padding on int and double values,
 *      with various flag combinations.
 */

public class Padding {

    private static class Argument {
        final String expected;
        final String format;
        final Object value;

        Argument(String expected, String format, Object value) {
            this.expected = expected;
            this.format = format;
            this.value = value;
        }
    }

    static Argument[] arguments = {
        /* blank padding, right adjusted, optional plus sign */
        new Argument("12", "%1d", 12),
        new Argument("12", "%2d", 12),
        new Argument(" 12", "%3d", 12),
        new Argument("  12", "%4d", 12),
        new Argument("   12", "%5d", 12),
        new Argument("        12", "%10d", 12),

        new Argument("-12", "%1d", -12),
        new Argument("-12", "%2d", -12),
        new Argument("-12", "%3d", -12),
        new Argument(" -12", "%4d", -12),
        new Argument("  -12", "%5d", -12),
        new Argument("       -12", "%10d", -12),

        new Argument("1.2", "%1.1f", 1.2),
        new Argument("1.2", "%2.1f", 1.2),
        new Argument("1.2", "%3.1f", 1.2),
        new Argument(" 1.2", "%4.1f", 1.2),
        new Argument("  1.2", "%5.1f", 1.2),
        new Argument("       1.2", "%10.1f", 1.2),

        new Argument("-1.2", "%1.1f", -1.2),
        new Argument("-1.2", "%2.1f", -1.2),
        new Argument("-1.2", "%3.1f", -1.2),
        new Argument("-1.2", "%4.1f", -1.2),
        new Argument(" -1.2", "%5.1f", -1.2),
        new Argument("      -1.2", "%10.1f", -1.2),

        /* blank padding, right adjusted, mandatory plus sign */
        new Argument("+12", "%+1d", 12),
        new Argument("+12", "%+2d", 12),
        new Argument("+12", "%+3d", 12),
        new Argument(" +12", "%+4d", 12),
        new Argument("  +12", "%+5d", 12),
        new Argument("       +12", "%+10d", 12),

        new Argument("-12", "%+1d", -12),
        new Argument("-12", "%+2d", -12),
        new Argument("-12", "%+3d", -12),
        new Argument(" -12", "%+4d", -12),
        new Argument("  -12", "%+5d", -12),
        new Argument("       -12", "%+10d", -12),

        new Argument("+1.2", "%+1.1f", 1.2),
        new Argument("+1.2", "%+2.1f", 1.2),
        new Argument("+1.2", "%+3.1f", 1.2),
        new Argument("+1.2", "%+4.1f", 1.2),
        new Argument(" +1.2", "%+5.1f", 1.2),
        new Argument("      +1.2", "%+10.1f", 1.2),

        new Argument("-1.2", "%+1.1f", -1.2),
        new Argument("-1.2", "%+2.1f", -1.2),
        new Argument("-1.2", "%+3.1f", -1.2),
        new Argument("-1.2", "%+4.1f", -1.2),
        new Argument(" -1.2", "%+5.1f", -1.2),
        new Argument("      -1.2", "%+10.1f", -1.2),

        /* blank padding, right adjusted, mandatory blank sign */
        new Argument(" 12", "% 1d", 12),
        new Argument(" 12", "% 2d", 12),
        new Argument(" 12", "% 3d", 12),
        new Argument("  12", "% 4d", 12),
        new Argument("   12", "% 5d", 12),
        new Argument("        12", "% 10d", 12),

        new Argument("-12", "% 1d", -12),
        new Argument("-12", "% 2d", -12),
        new Argument("-12", "% 3d", -12),
        new Argument(" -12", "% 4d", -12),
        new Argument("  -12", "% 5d", -12),
        new Argument("       -12", "% 10d", -12),

        new Argument(" 1.2", "% 1.1f", 1.2),
        new Argument(" 1.2", "% 2.1f", 1.2),
        new Argument(" 1.2", "% 3.1f", 1.2),
        new Argument(" 1.2", "% 4.1f", 1.2),
        new Argument("  1.2", "% 5.1f", 1.2),
        new Argument("       1.2", "% 10.1f", 1.2),

        new Argument("-1.2", "% 1.1f", -1.2),
        new Argument("-1.2", "% 2.1f", -1.2),
        new Argument("-1.2", "% 3.1f", -1.2),
        new Argument("-1.2", "% 4.1f", -1.2),
        new Argument(" -1.2", "% 5.1f", -1.2),
        new Argument("      -1.2", "% 10.1f", -1.2),

        /* blank padding, left adjusted, optional sign */
        new Argument("12", "%-1d", 12),
        new Argument("12", "%-2d", 12),
        new Argument("12 ", "%-3d", 12),
        new Argument("12  ", "%-4d", 12),
        new Argument("12   ", "%-5d", 12),
        new Argument("12        ", "%-10d", 12),

        new Argument("-12", "%-1d", -12),
        new Argument("-12", "%-2d", -12),
        new Argument("-12", "%-3d", -12),
        new Argument("-12 ", "%-4d", -12),
        new Argument("-12  ", "%-5d", -12),
        new Argument("-12       ", "%-10d", -12),

        new Argument("1.2", "%-1.1f", 1.2),
        new Argument("1.2", "%-2.1f", 1.2),
        new Argument("1.2", "%-3.1f", 1.2),
        new Argument("1.2 ", "%-4.1f", 1.2),
        new Argument("1.2  ", "%-5.1f", 1.2),
        new Argument("1.2       ", "%-10.1f", 1.2),

        new Argument("-1.2", "%-1.1f", -1.2),
        new Argument("-1.2", "%-2.1f", -1.2),
        new Argument("-1.2", "%-3.1f", -1.2),
        new Argument("-1.2", "%-4.1f", -1.2),
        new Argument("-1.2 ", "%-5.1f", -1.2),
        new Argument("-1.2      ", "%-10.1f", -1.2),

        /* blank padding, left adjusted, mandatory plus sign */
        new Argument("+12", "%-+1d", 12),
        new Argument("+12", "%-+2d", 12),
        new Argument("+12", "%-+3d", 12),
        new Argument("+12 ", "%-+4d", 12),
        new Argument("+12  ", "%-+5d", 12),
        new Argument("+12       ", "%-+10d", 12),

        new Argument("-12", "%-+1d", -12),
        new Argument("-12", "%-+2d", -12),
        new Argument("-12", "%-+3d", -12),
        new Argument("-12 ", "%-+4d", -12),
        new Argument("-12  ", "%-+5d", -12),
        new Argument("-12       ", "%-+10d", -12),

        new Argument("+1.2", "%-+1.1f", 1.2),
        new Argument("+1.2", "%-+2.1f", 1.2),
        new Argument("+1.2", "%-+3.1f", 1.2),
        new Argument("+1.2", "%-+4.1f", 1.2),
        new Argument("+1.2 ", "%-+5.1f", 1.2),
        new Argument("+1.2      ", "%-+10.1f", 1.2),

        new Argument("-1.2", "%-+1.1f", -1.2),
        new Argument("-1.2", "%-+2.1f", -1.2),
        new Argument("-1.2", "%-+3.1f", -1.2),
        new Argument("-1.2", "%-+4.1f", -1.2),
        new Argument("-1.2 ", "%-+5.1f", -1.2),
        new Argument("-1.2      ", "%-+10.1f", -1.2),

        /* blank padding, left adjusted, mandatory blank sign */
        new Argument(" 12", "%- 1d", 12),
        new Argument(" 12", "%- 2d", 12),
        new Argument(" 12", "%- 3d", 12),
        new Argument(" 12 ", "%- 4d", 12),
        new Argument(" 12  ", "%- 5d", 12),
        new Argument(" 12       ", "%- 10d", 12),

        new Argument("-12", "%- 1d", -12),
        new Argument("-12", "%- 2d", -12),
        new Argument("-12", "%- 3d", -12),
        new Argument("-12 ", "%- 4d", -12),
        new Argument("-12  ", "%- 5d", -12),
        new Argument("-12       ", "%- 10d", -12),

        new Argument(" 1.2", "%- 1.1f", 1.2),
        new Argument(" 1.2", "%- 2.1f", 1.2),
        new Argument(" 1.2", "%- 3.1f", 1.2),
        new Argument(" 1.2", "%- 4.1f", 1.2),
        new Argument(" 1.2 ", "%- 5.1f", 1.2),
        new Argument(" 1.2      ", "%- 10.1f", 1.2),

        new Argument("-1.2", "%- 1.1f", -1.2),
        new Argument("-1.2", "%- 2.1f", -1.2),
        new Argument("-1.2", "%- 3.1f", -1.2),
        new Argument("-1.2", "%- 4.1f", -1.2),
        new Argument("-1.2 ", "%- 5.1f", -1.2),
        new Argument("-1.2      ", "%- 10.1f", -1.2),

        /* zero padding, right adjusted, optional sign */
        new Argument("12", "%01d", 12),
        new Argument("12", "%02d", 12),
        new Argument("012", "%03d", 12),
        new Argument("0012", "%04d", 12),
        new Argument("00012", "%05d", 12),
        new Argument("0000000012", "%010d", 12),

        new Argument("-12", "%01d", -12),
        new Argument("-12", "%02d", -12),
        new Argument("-12", "%03d", -12),
        new Argument("-012", "%04d", -12),
        new Argument("-0012", "%05d", -12),
        new Argument("-000000012", "%010d", -12),

        new Argument("1.2", "%01.1f", 1.2),
        new Argument("1.2", "%02.1f", 1.2),
        new Argument("1.2", "%03.1f", 1.2),
        new Argument("01.2", "%04.1f", 1.2),
        new Argument("001.2", "%05.1f", 1.2),
        new Argument("00000001.2", "%010.1f", 1.2),

        new Argument("-1.2", "%01.1f", -1.2),
        new Argument("-1.2", "%02.1f", -1.2),
        new Argument("-1.2", "%03.1f", -1.2),
        new Argument("-1.2", "%04.1f", -1.2),
        new Argument("-01.2", "%05.1f", -1.2),
        new Argument("-0000001.2", "%010.1f", -1.2),

        /* zero padding, right adjusted, mandatory plus sign */
        new Argument("+12", "%+01d", 12),
        new Argument("+12", "%+02d", 12),
        new Argument("+12", "%+03d", 12),
        new Argument("+012", "%+04d", 12),
        new Argument("+0012", "%+05d", 12),
        new Argument("+000000012", "%+010d", 12),

        new Argument("-12", "%+01d", -12),
        new Argument("-12", "%+02d", -12),
        new Argument("-12", "%+03d", -12),
        new Argument("-012", "%+04d", -12),
        new Argument("-0012", "%+05d", -12),
        new Argument("-000000012", "%+010d", -12),

        new Argument("+1.2", "%+01.1f", 1.2),
        new Argument("+1.2", "%+02.1f", 1.2),
        new Argument("+1.2", "%+03.1f", 1.2),
        new Argument("+1.2", "%+04.1f", 1.2),
        new Argument("+01.2", "%+05.1f", 1.2),
        new Argument("+0000001.2", "%+010.1f", 1.2),

        new Argument("-1.2", "%+01.1f", -1.2),
        new Argument("-1.2", "%+02.1f", -1.2),
        new Argument("-1.2", "%+03.1f", -1.2),
        new Argument("-1.2", "%+04.1f", -1.2),
        new Argument("-01.2", "%+05.1f", -1.2),
        new Argument("-0000001.2", "%+010.1f", -1.2),

        /* zero padding, right adjusted, mandatory blank sign */
        new Argument(" 12", "% 01d", 12),
        new Argument(" 12", "% 02d", 12),
        new Argument(" 12", "% 03d", 12),
        new Argument(" 012", "% 04d", 12),
        new Argument(" 0012", "% 05d", 12),
        new Argument(" 000000012", "% 010d", 12),

        new Argument("-12", "% 01d", -12),
        new Argument("-12", "% 02d", -12),
        new Argument("-12", "% 03d", -12),
        new Argument("-012", "% 04d", -12),
        new Argument("-0012", "% 05d", -12),
        new Argument("-000000012", "% 010d", -12),

        new Argument(" 1.2", "% 01.1f", 1.2),
        new Argument(" 1.2", "% 02.1f", 1.2),
        new Argument(" 1.2", "% 03.1f", 1.2),
        new Argument(" 1.2", "% 04.1f", 1.2),
        new Argument(" 01.2", "% 05.1f", 1.2),
        new Argument(" 0000001.2", "% 010.1f", 1.2),

        new Argument("-1.2", "% 01.1f", -1.2),
        new Argument("-1.2", "% 02.1f", -1.2),
        new Argument("-1.2", "% 03.1f", -1.2),
        new Argument("-1.2", "% 04.1f", -1.2),
        new Argument("-01.2", "% 05.1f", -1.2),
        new Argument("-0000001.2", "% 010.1f", -1.2),
    };

    public static void main(String [] args) {
        for (Argument arg : arguments) {
            if (!arg.expected.equals(String.format(arg.format, arg.value))) {
                throw new RuntimeException("Expected value " + arg.expected +
                " not returned from String.format(" + arg.format + ", " + arg.value + ")");
            }
        }
    }
}
