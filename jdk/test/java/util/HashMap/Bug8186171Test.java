/*
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
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.concurrent.ThreadLocalRandom;

import org.testng.annotations.Test;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertTrue;

/*
 * @test
 * @bug 8186171
 * @run testng Bug8186171Test
 * @summary Verify the fix for scenario reported in JDK-8186171
 * @author deepak.kejriwal@oracle.com
 */
@Test
public class Bug8186171Test{

    /**
     * Tests and extends the scenario reported in
     * https://bugs.openjdk.java.net/browse/JDK-8186171
     * HashMap: Entry.setValue may not work after Iterator.remove() called for previous entries
     * Runs 1000 times as it is based on randomization.
     */
    @Test(invocationCount = 1000)
    static void testBug8186171NonDeterministic()
    {
        final ThreadLocalRandom rnd = ThreadLocalRandom.current();

        final Object v1 = rnd.nextBoolean() ? null : 1;
        final Object v2 = (rnd.nextBoolean() && v1 != null) ? null : 2;

        /** If true, always lands in first bucket in hash tables. */
        final boolean poorHash = rnd.nextBoolean();

        class Key implements Comparable<Key> {
            final int i;
            Key(int i) { this.i = i; }
            public int hashCode() { return poorHash ? 0 : super.hashCode(); }
            public int compareTo(Key x) {
                return Integer.compare(this.i, x.i);
            }
        }

        // HashMap and ConcurrentHashMap have:
        // TREEIFY_THRESHOLD = 8; UNTREEIFY_THRESHOLD = 6;
        final int size = rnd.nextInt(1, 25);

        List<Key> keys = new ArrayList<>();
        for (int i = size; i-->0; ) keys.add(new Key(i));
        Key keyToFrob = keys.get(rnd.nextInt(keys.size()));

        Map<Key, Object> m = new HashMap<Key, Object>();

        for (Key key : keys) m.put(key, v1);

        for (Iterator<Map.Entry<Key, Object>> it = m.entrySet().iterator();
             it.hasNext(); ) {
            Map.Entry<Key, Object> entry = it.next();
            if (entry.getKey() == keyToFrob)
                entry.setValue(v2); // does this have the expected effect?
            else
                it.remove();
        }

        assertFalse(m.containsValue(v1));
        assertTrue(m.containsValue(v2));
        assertTrue(m.containsKey(keyToFrob));
        assertEquals(1, m.size());
    }


    /**
     * Tests and extends the scenario reported in
     * https://bugs.openjdk.java.net/browse/JDK-8186171
     * HashMap: Entry.setValue may not work after Iterator.remove() called for previous entries
     * Runs single time by reproducing exact scenario for issue mentioned in 8186171
     */
    @Test()
    static void testBug8186171Deterministic(){
        class Key implements Comparable<Key>
        {
            final int i;
            Key(int i) { this.i = i; }

            @Override
            public int hashCode() { return 0; } //Returning same hashcode so that all keys landup to same bucket

            @Override
            public int compareTo(Key x){
                if(this.i == x.i){
                    return 0;
                }
                else {
                    return Integer.compare(this.i, x.i);
                }
            }
            @Override
            public String toString()
            {
                return "Key_" + i;
            }
        }

        // HashMap have TREEIFY_THRESHOLD = 8; UNTREEIFY_THRESHOLD = 6;
        final int size = 11;
        List<Key> keys = new ArrayList<>();

        for (int i = 0; i < size; i++){
            keys.add(new Key(i));
        }

        Key keyToFrob = keys.get(9);
        Map<Key, Object> m = new HashMap<Key, Object>();
        for (Key key : keys) m.put(key, null);

        for (Iterator<Map.Entry<Key, Object>> it = m.entrySet().iterator(); it.hasNext(); ){
            Map.Entry<Key, Object> entry = it.next();
            if (entry.getKey() == keyToFrob){
                entry.setValue(2);
            }
            else{
                it.remove();
            }
        }

        assertFalse(m.containsValue(null));
        assertTrue(m.containsValue(2));
        assertTrue(m.containsKey(keyToFrob));
        assertEquals(1, m.size());
    }
}
