import java.util.List;

public class TrivialArrayUtil {
    public static long[] convertListToArray(List<Long> list) {
        if (null == list || list.isEmpty()) {
            return null;
        }
        int size = list.size();
        long[] arrays = new long[size];
        for (int i = 0; i < size; i++) {
            arrays[i] = list.get(i);
        }
        return arrays;
    }

    public static long[] getLongFromString(String[] strs) {
        if (null == strs || strs.length < 1) {
            return null;
        }
        long[] res = new long[strs.length];
        int index = 0;
        for (String ss : strs) {
            try {
                res[index] = Long.parseLong(ss);
                index++;
            } catch (NumberFormatException e) {
            }
        }
        return res;
    }


}
