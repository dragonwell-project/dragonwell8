
public class IntCmpTests {

    private static boolean test_isEq(int a, int b) {
        return a == b;
    }

    private static boolean test_isNe(int a, int b) {
        return a != b;
    }

    private static boolean test_isLt(int a, int b) {
        return a < b;
    }

    private static boolean test_isLe(int a, int b) {
        return a <= b;
    }

    private static boolean test_isGe(int a, int b) {
        return a >= b;
    }

    private static boolean test_isGt(int a, int b) {
        return a > b;
    }

    private static boolean test_isEqC(int a) {
        return a == 7;
    }

    private static boolean test_isNeC(int a) {
        return a != 7;
    }

    private static boolean test_isLtC(int a) {
        return a < 7;
    }

    private static boolean test_isLeC(int a) {
        return a <= 7;
    }

    private static boolean test_isGeC(int a) {
        return a >= 7;
    }

    private static boolean test_isGtC(int a) {
        return a > 7;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {
        assertThat(test_isEq(7, 7));
        assertThat(! test_isEq(70, 7));
        assertThat(! test_isNe(7, 7));
        assertThat(test_isNe(70, 7));

        assertThat(test_isLt(7, 70));
        assertThat(! test_isLt(70, 7));
        assertThat(! test_isLt(7, 7));

        assertThat(test_isLe(7, 70));
        assertThat(! test_isLe(70, 7));
        assertThat(test_isLe(7, 7));

        assertThat(!test_isGe(7, 70));
        assertThat(test_isGe(70, 7));
        assertThat(test_isGe(7, 7));

        assertThat(!test_isGt(7, 70));
        assertThat(test_isGt(70, 7));
        assertThat(! test_isGt(7, 7));

        assertThat(test_isEqC(7));
        assertThat(! test_isEqC(70));
        assertThat(! test_isNeC(7));
        assertThat(test_isNeC(70));

        assertThat(test_isLtC(6));
        assertThat(! test_isLtC(70));
        assertThat(! test_isLtC(7));

        assertThat(test_isLeC(6));
        assertThat(! test_isLeC(70));
        assertThat(test_isLeC(7));

        assertThat(!test_isGeC(6));
        assertThat(test_isGeC(70));
        assertThat(test_isGeC(7));

        assertThat(!test_isGtC(6));
        assertThat(test_isGtC(70));
        assertThat(! test_isGtC(7));

    }
}
