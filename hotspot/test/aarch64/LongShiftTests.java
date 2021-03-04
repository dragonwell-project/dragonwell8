public class LongShiftTests {

    private static long test_shl(long a, long b) {
        return a << b;
    }

    private static long test_shlc1(long a) {
        return a << 1;
    }

    private static long test_shlc65(long a) {
        return a << 65;
    }

    private static long test_shr(long a, long b) {
        return a >> b;
    }

    private static long test_shrc1(long a) {
        return a >> 1;
    }

    private static long test_shrc65(long a) {
        return a >> 65;
    }

    private static long test_ushr(long a, long b) {
        return a >>> b;
    }

    private static long test_ushrc1(long a) {
        return a >>> 1;
    }

    private static long test_ushrc65(long a) {
        return a >>> 65;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {

        assertThat(test_shl(32, 2) == 128);
        assertThat(test_shl(0x8000000000000000L, 1) == 0);
        assertThat(test_shl(0x4000000000000000L, 1) == 0x8000000000000000L);
        assertThat(test_shl(0x4000000000000000L, 65) == 0x8000000000000000L);

        assertThat(test_shr(32, 2) == 8);
        assertThat(test_shr(1, 1) == 0);
        assertThat(test_shr(0x8000000000000000L, 1) == 0xc000000000000000L);
        assertThat(test_shr(0x4000000000000000L, 65) == 0x2000000000000000L);

        assertThat(test_ushr(32, 2) == 8);
        assertThat(test_ushr(1, 1) == 0);
        assertThat(test_ushr(0x8000000000000000L, 1) == 0x4000000000000000L);
        assertThat(test_ushr(0x4000000000000000L, 65) == 0x2000000000000000L);

        assertThat(test_shlc1(32) == 64);
        assertThat(test_shlc1(0x8000000000000000L) == 0);
        assertThat(test_shlc1(0x4000000000000000L) == 0x8000000000000000L);
        assertThat(test_shlc65(0x4000000000000000L) == 0x8000000000000000L);

        assertThat(test_shrc1(32) == 16);
        assertThat(test_shrc1(1) == 0);
        assertThat(test_shrc1(0x8000000000000000L) == 0xc000000000000000L);
        assertThat(test_shrc65(0x4000000000000000L) == 0x2000000000000000L);

        assertThat(test_ushrc1(32) == 16);
        assertThat(test_ushrc1(1) == 0);
        assertThat(test_ushrc1(0x8000000000000000L) == 0x4000000000000000L);
        assertThat(test_ushrc65(0x4000000000000000L) == 0x2000000000000000L);
    }
}
