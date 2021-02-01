public class IntArithTests {

    private static final int IIMM12_0 = 0x1;   // first imm value
    private static final int IIMM12_1 = 0xfff; // last 12bit imm value
    private static final int IIMM12_2 = 0x1001; // Should not encode as imm
    private static final int IIMM24_3 = 0x1000; // first 12 bit shifted imm
    private static final int IIMM24_4 = 0xfff000; // Last 12 bit shifted imm
    private static final int IIMM24_5 = 0x1001000; // Should not encode as imm

    private static int test_neg(int a) {
        return -a;
    }

    private static int test_addi(int a, int b) {
        return a + b;
    }

    private static int test_addic0(int a) {
        return a + IIMM12_0;
    }

    private static int test_addic1(int a) {
        return a + IIMM12_1;
    }

    private static int test_addic2(int a) {
        return a + IIMM12_2;
    }

    private static int test_addic3(int a) {
        return a + IIMM24_3;
    }

    private static int test_addic4(int a) {
        return a + IIMM24_4;
    }

    private static int test_addic5(int a) {
        return a + IIMM24_5;
    }

    private static int test_subi(int a, int b) {
        return a - b;
    }

    private static int test_subc1(int a) {
        return a - 11;
    }

    private static int test_mulic1(int a) {
        // Generates shl.
        return a * 8;
    }

    private static int test_mulic2(int a) {
        // Generates shl followed by add.
        return a * 9;
    }

    private static int test_mulic3(int a) {
        // Generates shl followed by sub.
        return a * 7;
    }

    private static int test_mulic4(int a) {
        // Generates normal mul.
        return a * 10;
    }

    private static int test_muli(int a, int b) {
        // Generates normal mul.
        return a * b;
    }

    private static int test_divi(int a, int b) {
        return a / b;
    }

    private static int test_remi(int a, int b) {
        return a % b;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {
        assertThat(test_neg(10) == -10);
        assertThat(test_addi(3, 2) == 5);
        assertThat(test_addi(Integer.MAX_VALUE, 1) == Integer.MIN_VALUE);
        assertThat(test_addic0(3) == 4);
        assertThat(test_addic1(3) == 0x1002);
        assertThat(test_addic2(3) == 0x1004);
        assertThat(test_addic3(3) == 0x1003);
        assertThat(test_addic4(3) == 0xfff003);
        assertThat(test_addic5(3) == 0x1001003);

        assertThat(test_subi(40, 13) == 27);
        assertThat(test_subi(Integer.MIN_VALUE, 1) == Integer.MAX_VALUE);
        assertThat(test_subc1(40) == 29);

        assertThat(test_mulic1(5) == 40);
        assertThat(test_mulic2(5) == 45);
        assertThat(test_mulic3(5) == 35);
        assertThat(test_mulic4(5) == 50);
        assertThat(test_muli(5, 200) == 1000);

        assertThat(test_divi(30, 3) == 10);
        assertThat(test_divi(29, 3) == 9);
        assertThat(test_divi(Integer.MIN_VALUE, -1) == Integer.MIN_VALUE);
        try {
            test_divi(30, 0);
            throw new AssertionError();
        } catch (ArithmeticException ex) {
            // Pass.
        }

        assertThat(test_remi(30, 3) == 0);
        assertThat(test_remi(29, 3) == 2);
        assertThat(test_remi(Integer.MIN_VALUE, -1) == 0);
        try {
            test_remi(30, 0);
            throw new AssertionError();
        } catch (ArithmeticException ex) {
            // Pass.
        }
    }
}
