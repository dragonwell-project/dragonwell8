
public class DoubleCmpTests {

    private static boolean test_isEq(double a, double b) {
        return a == b;
    }

    private static boolean test_isNe(double a, double b) {
        return a != b;
    }

    private static boolean test_isLt(double a, double b) {
        return a < b;
    }

    private static boolean test_isLe(double a, double b) {
        return a <= b;
    }

    private static boolean test_isGe(double a, double b) {
        return a >= b;
    }

    private static boolean test_isGt(double a, double b) {
        return a > b;
    }

    private static boolean test_isEqC(double a) {
        return a == 7.;
    }

    private static boolean test_isNeC(double a) {
        return a != 7.;
    }

    private static boolean test_isLtC(double a) {
        return a < 7.;
    }

    private static boolean test_isLeC(double a) {
        return a <= 7.;
    }

    private static boolean test_isGeC(double a) {
        return a >= 7.;
    }

    private static boolean test_isGtC(double a) {
        return a > 7.;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {
        assertThat(test_isEq(7., 7.));
        assertThat(! test_isEq(70., 7.));
        assertThat(! test_isNe(7., 7.));
        assertThat(test_isNe(70., 7.));

        assertThat(test_isLt(7., 70.));
        assertThat(! test_isLt(70., 7.));
        assertThat(! test_isLt(7., 7.));

        assertThat(test_isLe(7., 70.));
        assertThat(! test_isLe(70., 7.));
        assertThat(test_isLe(7., 7.));

        assertThat(!test_isGe(7., 70.));
        assertThat(test_isGe(70., 7.));
        assertThat(test_isGe(7., 7.));

        assertThat(!test_isGt(7., 70.));
        assertThat(test_isGt(70., 7.));
        assertThat(! test_isGt(7., 7.));


        assertThat(test_isEqC(7.));
        assertThat(! test_isEqC(70.));
        assertThat(! test_isNeC(7.));
        assertThat(test_isNeC(70.));

        assertThat(test_isLtC(6.));
        assertThat(! test_isLtC(70.));
        assertThat(! test_isLtC(7.));

        assertThat(test_isLeC(6.));
        assertThat(! test_isLeC(70.));
        assertThat(test_isLeC(7.));

        assertThat(!test_isGeC(6.));
        assertThat(test_isGeC(70.));
        assertThat(test_isGeC(7.));

        assertThat(!test_isGtC(6.));
        assertThat(test_isGtC(70.));
        assertThat(! test_isGtC(7.));
    }
}
