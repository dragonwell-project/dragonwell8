public class LongLogicTests {

    private static final long IMM = 0xf0f0f0f0f0f0f0f0L;
    private static final long NO_IMM = 0x123456d5123456d5L;
    private static long test_and(long a, long b) {
        return a & b;
    }

    private static long test_andc1(long a) {
        // Generates immediate instruction.
        return a & IMM;
    }

    private static long test_andc2(long a) {
        // Generates non-immediate instruction.
        return a & NO_IMM;
    }

    private static long test_or(long a, long b) {
        return a | b;
    }

    private static long test_orc1(long a) {
        // Generates immediate instruction.
        return a | IMM;
    }

    private static long test_orc2(long a) {
        // Generates non-immediate instruction.
        return a | NO_IMM;
    }

    private static long test_xor(long a, long b) {
        return a ^ b;
    }

    private static long test_xorc1(long a) {
        // Generates immediate instruction.
        return a ^ IMM;
    }

    private static long test_xorc2(long a) {
        // Generates non-immediate instruction.
        return a ^ NO_IMM;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {

        assertThat(test_and(0x21, 0x31) == 0x21);
        assertThat(test_andc1(0xaaaaaaaaaaaaaaaaL) == 0xa0a0a0a0a0a0a0a0L);
        assertThat(test_andc2(0xaaaaaaaaaaaaaaaaL) == 0x0220028002200280L);

        assertThat(test_or(0x21, 0x31) == 0x31);
        assertThat(test_orc1(0xaaaaaaaaaaaaaaaaL) == 0xfafafafafafafafaL);
        assertThat(test_orc2(0xaaaaaaaaaaaaaaaaL) == 0xbabefeffbabefeffL);

        assertThat(test_xor(0x21, 0x31) == 16);
        assertThat(test_xorc1(0xaaaaaaaaaaaaaaaaL) == 0x5a5a5a5a5a5a5a5aL);
        assertThat(test_xorc2(0xaaaaaaaaaaaaaaaaL) == 0xb89efc7fb89efc7fL);
    }
}
