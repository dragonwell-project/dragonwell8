/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * @test
 * @library /lib/testlibrary
 * @summary Test TimeOut.Queue's offer and remove function, make sure it's consistent with the behavior of the jdk's priority queue
 * @requires os.family == "linux"
 * @run main/othervm PriorityQueueSortTest
 */

import com.alibaba.wisp.engine.TimeOut;
import com.alibaba.wisp.engine.WispTask;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.*;

import static jdk.testlibrary.Asserts.assertTrue;

public class PriorityQueueSortTest {

    static Class<?> classQ;
    static Class<?> classT;

    static Method poll = null;
    static Method offer = null;

    static Class<TimeOut> clazz;
    static Constructor<TimeOut> constructor;

    static {
        try {
            clazz = (Class<TimeOut>) Class.forName("com.alibaba.wisp.engine.TimeOut");
            constructor = clazz.getDeclaredConstructor(Class.forName("com.alibaba.wisp.engine.WispTask"), long.class,
                    Class.forName("com.alibaba.wisp.engine.TimeOut$Action"));
            ILLEGAL_FLAG = constructor.newInstance(null, 1,
                    Class.forName("com.alibaba.wisp.engine.TimeOut$Action").getEnumConstants()[1]);
        } catch (Exception e) {
            assertTrue(false, e.toString());
        }
    }

    static TimeOut ILLEGAL_FLAG;

    static class MyComparator implements Comparator<TimeOut> {
        public int compare(TimeOut x, TimeOut y) {
            return Long.compare(getDeadNano(x), getDeadNano(y));
        }
    }

    static Long getDeadNano(TimeOut t) {
        try {
            Field deadline = TimeOut.class.getDeclaredField("deadlineNano");
            deadline.setAccessible(true);
            return (Long) deadline.get(t);
        } catch (Exception e) {
            throw new Error(e);
        }
    }

    static Object getTimerQueue() {
        try {
            classQ = Class.forName("com.alibaba.wisp.engine.TimeOut$TimerManager$Queue");
            classT = Class.forName("com.alibaba.wisp.engine.TimeOut$TimerManager");
            Constructor c1 = classQ.getDeclaredConstructor();
            c1.setAccessible(true);
            Constructor c2 = classT.getDeclaredConstructor();
            c2.setAccessible(true);
            poll = classQ.getDeclaredMethod("poll");
            poll.setAccessible(true);
            offer = classQ.getDeclaredMethod("offer", TimeOut.class);
            offer.setAccessible(true);
            return c1.newInstance();
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    static boolean add(Object pq, TimeOut timeOut) {
        try {
            offer.invoke(pq, timeOut);
            return true;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    static TimeOut poll(Object pq) {
        try {
            return (TimeOut) poll.invoke(pq);
        } catch (Exception e) {
            e.printStackTrace();
            return ILLEGAL_FLAG;
        }
    }

    public static void main(String[] args) {
        int n = 10000;

        List<Long> sorted = new ArrayList<>(n);
        for (int i = 0; i < n; i++)
            sorted.add(new Long(i));
        List<Long> shuffled = new ArrayList<>(sorted);
        Collections.shuffle(shuffled);

        Object pq = getTimerQueue();

        try {
            for (Iterator<Long> i = shuffled.iterator(); i.hasNext();)
                add(pq, constructor.newInstance(null, i.next(),
                        Class.forName("com.alibaba.wisp.engine.TimeOut$Action").getEnumConstants()[1]));

            List<Long> recons = new ArrayList<>();
            while (true) {
                TimeOut t = poll(pq);
                if (t == null) {
                    break;
                }
                recons.add(getDeadNano(t));
            }
            assertTrue(recons.equals(sorted), "Sort failed");

            for (Long val : recons) {
                add(pq, constructor.newInstance(null, val,
                        Class.forName("com.alibaba.wisp.engine.TimeOut$Action").getEnumConstants()[1]));
            }
            recons.clear();
            while (true) {
                TimeOut timeOut = poll(pq);
                if (timeOut == null) {
                    break;
                }
                if (getDeadNano(timeOut) % 2 == 1) {
                    recons.add(getDeadNano(timeOut));
                }
            }

            Collections.sort(recons);

            for (Iterator<Long> i = sorted.iterator(); i.hasNext();)
                if ((i.next().intValue() % 2) != 1)
                    i.remove();

            assertTrue(recons.equals(sorted), "Odd Sort failed");
        } catch (Exception e) {
            assertTrue(false, e.toString());
        }
    }
}
