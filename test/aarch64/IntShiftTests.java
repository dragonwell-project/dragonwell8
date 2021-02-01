public class IntShiftTests {

    private static int test_shl(int a, int b) {
        return a << b;
    }

    private static int test_shlc1(int a) {
        return a << 1;
    }

    private static int test_shlc33(int a) {
        return a << 33;
    }

    private static int test_shr(int a, int b) {
        return a >> b;
    }

    private static int test_shrc1(int a) {
        return a >> 1;
    }

    private static int test_shrc33(int a) {
        return a >> 33;
    }

    private static int test_ushr(int a, int b) {
        return a >>> b;
    }

    private static int test_ushrc1(int a) {
        return a >>> 1;
    }

    private static int test_ushrc33(int a) {
        return a >>> 33;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {

        assertThat(test_shl(32, 2) == 128);
        assertThat(test_shl(0x80000000, 1) == 0);
        assertThat(test_shl(0x40000000, 1) == 0x80000000);
        assertThat(test_shl(0x40000000, 33) == 0x80000000);

        assertThat(test_shr(32, 2) == 8);
        assertThat(test_shr(1, 1) == 0);
        assertThat(test_shr(0x80000000, 1) == 0xc0000000);
        assertThat(test_shr(0x40000000, 33) == 0x20000000);

        assertThat(test_ushr(32, 2) == 8);
        assertThat(test_ushr(1, 1) == 0);
        assertThat(test_ushr(0x80000000, 1) == 0x40000000);
        assertThat(test_ushr(0x40000000, 33) == 0x20000000);

        assertThat(test_shlc1(32) == 64);
        assertThat(test_shlc1(0x80000000) == 0);
        assertThat(test_shlc1(0x40000000) == 0x80000000);
        assertThat(test_shlc33(0x40000000) == 0x80000000);

        assertThat(test_shrc1(32) == 16);
        assertThat(test_shrc1(1) == 0);
        assertThat(test_shrc1(0x80000000) == 0xc0000000);
        assertThat(test_shrc33(0x40000000) == 0x20000000);

        assertThat(test_ushrc1(32) == 16);
        assertThat(test_ushrc1(1) == 0);
        assertThat(test_ushrc1(0x80000000) == 0x40000000);
        assertThat(test_ushrc33(0x40000000) == 0x20000000);
    }
}
