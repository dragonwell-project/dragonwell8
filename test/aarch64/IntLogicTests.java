public class IntLogicTests {

    private static int test_and(int a, int b) {
        return a & b;
    }

    private static int test_andc1(int a) {
        // Generates immediate instruction.
        return a & 0xf0f0f0f0;
    }

    private static int test_andc2(int a) {
        // Generates non-immediate instruction.
        return a & 0x123456d5;
    }

    private static int test_or(int a, int b) {
        return a | b;
    }

    private static int test_orc1(int a) {
        // Generates immediate instruction.
        return a | 0xf0f0f0f0;
    }

    private static int test_orc2(int a) {
        // Generates non-immediate instruction.
        return a | 0x123456d5;
    }

    private static int test_xor(int a, int b) {
        return a ^ b;
    }

    private static int test_xorc1(int a) {
        // Generates immediate instruction.
        return a ^ 0xf0f0f0f0;
    }

    private static int test_xorc2(int a) {
        // Generates non-immediate instruction.
        return a ^ 0x123456d5;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {

        assertThat(test_and(0x21, 0x31) == 0x21);
        assertThat(test_andc1(0xaaaaaaaa) == 0xa0a0a0a0);
        assertThat(test_andc2(0xaaaaaaaa) == 0x02200280);

        assertThat(test_or(0x21, 0x31) == 0x31);
        assertThat(test_orc1(0xaaaaaaaa) == 0xfafafafa);
        assertThat(test_orc2(0xaaaaaaaa) == 0xbabefeff);

        assertThat(test_xor(0x21, 0x31) == 16);
        assertThat(test_xorc1(0xaaaaaaaa) == 0x5a5a5a5a);
        assertThat(test_xorc2(0xaaaaaaaa) == 0xb89efc7f);
    }
}
