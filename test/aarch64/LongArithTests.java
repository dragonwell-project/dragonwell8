public class LongArithTests {


    private static final long IIMM12_0 = 0x1;   // first imm value
    private static final long IIMM12_1 = 0xfff; // last 12bit imm value
    private static final long IIMM12_2 = 0x1001; // Should not encode as imm
    private static final long IIMM24_3 = 0x1000; // first 12 bit shifted imm
    private static final long IIMM24_4 = 0xfff000; // Last 12 bit shifted imm
    private static final long IIMM24_5 = 0x1001000; // Should not encode as imm

    private static long test_neg(long a) {
        return -a;
    }

    private static long test_add(long a, long b) {
        return a + b;
    }

    private static long test_addc0(long a) {
        return a + IIMM12_0;
    }

    private static long test_addc1(long a) {
        return a + IIMM12_1;
    }

    private static long test_addc2(long a) {
        return a + IIMM12_2;
    }

    private static long test_addc3(long a) {
        return a + IIMM24_3;
    }

    private static long test_addc4(long a) {
        return a + IIMM24_4;
    }

    private static long test_addc5(long a) {
        return a + IIMM24_5;
    }

    private static long test_sub(long a, long b) {
        return a - b;
    }

    private static long test_subc1(long a) {
        return a - 11;
    }

    private static long test_mulc1(long a) {
        // Generates shl.
        return a * 8;
    }

    private static long test_mulc2(long a) {
        // Generates shl followed by add.
        return a * 9;
    }

    private static long test_mulc3(long a) {
        // Generates shl followed by sub.
        return a * 7;
    }

    private static long test_mulc4(long a) {
        // Generates normal mul.
        return a * 10;
    }

    private static long test_mul(long a, long b) {
        // Generates normal mul.
        return a * b;
    }

    private static long test_div(long a, long b) {
        return a / b;
    }

    private static long test_rem(long a, long b) {
        return a % b;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {
        assertThat(test_neg(10) == -10);
        assertThat(test_add(3, 2) == 5);
        assertThat(test_add(Long.MAX_VALUE, 1) == Long.MIN_VALUE);
        assertThat(test_addc0(3) == 4);
        assertThat(test_addc1(3) == 0x1002);
        assertThat(test_addc2(3) == 0x1004);
        assertThat(test_addc3(3) == 0x1003);
        assertThat(test_addc4(3) == 0xfff003);
        assertThat(test_addc5(3) == 0x1001003);

        assertThat(test_sub(40, 13) == 27);
        assertThat(test_sub(Long.MIN_VALUE, 1) == Long.MAX_VALUE);
        assertThat(test_subc1(40) == 29);

        assertThat(test_mulc1(5) == 40);
        assertThat(test_mulc2(5) == 45);
        assertThat(test_mulc3(5) == 35);
        assertThat(test_mulc4(5) == 50);
        assertThat(test_mul(5, 200) == 1000);

        assertThat(test_div(30, 3) == 10);
        assertThat(test_div(29, 3) == 9);
        assertThat(test_div(Long.MIN_VALUE, -1) == Long.MIN_VALUE);
        try {
            test_div(30, 0);
            throw new AssertionError();
        } catch (ArithmeticException ex) {
            // Pass.
        }

        assertThat(test_rem(30, 3) == 0);
        assertThat(test_rem(29, 3) == 2);
        assertThat(test_rem(Long.MIN_VALUE, -1) == 0);
        try {
            test_rem(30, 0);
            throw new AssertionError();
        } catch (ArithmeticException ex) {
            // Pass.
        }

    }
}
