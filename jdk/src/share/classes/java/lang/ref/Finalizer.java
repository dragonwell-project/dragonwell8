/*
 * Copyright (c) 1997, 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

package java.lang.ref;

import java.security.PrivilegedAction;
import java.security.AccessController;

import com.alibaba.tenant.TenantState;
import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;
import sun.misc.VM;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantData;
import com.alibaba.tenant.TenantGlobals;

final class Finalizer extends FinalReference<Object> { /* Package-private; must be in
                                                          same package as the Reference
                                                          class */

    private static ReferenceQueue<Object> queue = new ReferenceQueue<>();
    private static Finalizer unfinalized = null;
    private static final Object lock = new Object();

    private static final String ID_UNFINALIZED = "unfinalized";

    private Finalizer
        next = null,
        prev = null;

    private static ReferenceQueue<Object> initTenantReferenceQueue() {
        if (!TenantGlobals.isDataIsolationEnabled() || TenantContainer.current() == null) {
            throw new UnsupportedOperationException();
        }
        // spawn a new finalizer thread for this tenant, put it into system thread group
        // will be terminated after destruction of tenant container
        ThreadGroup tg = Thread.currentThread().getThreadGroup();
        for (ThreadGroup tgn = tg;
             tgn != null;
             tg = tgn, tgn = tg.getParent());
        Thread finalizer = new FinalizerThread(tg);
        SharedSecrets.getTenantAccess()
                .registerServiceThread(TenantContainer.current(), finalizer);
        finalizer.setName("TenantFinalizer-" + TenantContainer.current().getTenantId());
        finalizer.setPriority(Thread.MAX_PRIORITY - 2);
        finalizer.setDaemon(true);
        finalizer.start();

        return new ReferenceQueue<>();
    }

    private boolean hasBeenFinalized() {
        return (next == this);
    }

    private void add() {
        synchronized (lock) {
            if (VM.isBooted() && TenantGlobals.isDataIsolationEnabled() && TenantContainer.current() != null) {
                TenantData td = TenantContainer.current().getTenantData();
                Finalizer tenantUnfinalized = td.getFieldValue(Finalizer.class, ID_UNFINALIZED);
                if (tenantUnfinalized != null) {
                    this.next = tenantUnfinalized;
                    tenantUnfinalized.prev = this;
                }
                td.setFieldValue(Finalizer.class, ID_UNFINALIZED, this);
            } else {
                if (unfinalized != null) {
                    this.next = unfinalized;
                    unfinalized.prev = this;
                }
                unfinalized = this;
            }
        }
    }

    private void remove() {
        synchronized (lock) {
            if (TenantGlobals.isDataIsolationEnabled() && TenantContainer.current() != null) {
                TenantData td = TenantContainer.current().getTenantData();
                if (td.getFieldValue(Finalizer.class, ID_UNFINALIZED) == this) {
                    if (this.next != null) {
                        td.setFieldValue(Finalizer.class, ID_UNFINALIZED, this.next);
                    } else {
                        td.setFieldValue(Finalizer.class, ID_UNFINALIZED, this.prev);
                    }
                }
            } else {
                if (unfinalized == this) {
                    if (this.next != null) {
                        unfinalized = this.next;
                    } else {
                        unfinalized = this.prev;
                    }
                }
            }
            if (this.next != null) {
                this.next.prev = this.prev;
            }
            if (this.prev != null) {
                this.prev.next = this.next;
            }
            this.next = this;   /* Indicates that this has been finalized */
            this.prev = this;
        }
    }

    private Finalizer(Object finalizee) {
        super(finalizee, getQueue());
        add();
    }

    static ReferenceQueue<Object> getQueue() {
        if (VM.isBooted() && TenantGlobals.isDataIsolationEnabled() && TenantContainer.current() != null) {
            return TenantContainer.current().getFieldValue(Finalizer.class, "queue",
                    Finalizer::initTenantReferenceQueue);
        } else {
            return queue;
        }
    }

    /* Invoked by VM */
    static void register(Object finalizee) {
        new Finalizer(finalizee);
    }

    private void runFinalizer(JavaLangAccess jla) {
        synchronized (this) {
            if (hasBeenFinalized()) return;
            remove();
        }
        try {
            Object finalizee = this.get();
            if (finalizee != null && !(finalizee instanceof java.lang.Enum)) {
                jla.invokeFinalize(finalizee);

                /* Clear stack slot containing this variable, to decrease
                   the chances of false retention with a conservative GC */
                finalizee = null;
            }
        } catch (Throwable x) { }
        super.clear();
    }

    /* Create a privileged secondary finalizer thread in the system thread
       group for the given Runnable, and wait for it to complete.

       This method is used by runFinalization.

       It could have been implemented by offloading the work to the
       regular finalizer thread and waiting for that thread to finish.
       The advantage of creating a fresh thread, however, is that it insulates
       invokers of that method from a stalled or deadlocked finalizer thread.
     */
    private static void forkSecondaryFinalizer(final Runnable proc) {
        AccessController.doPrivileged(
            new PrivilegedAction<Void>() {
                public Void run() {
                    ThreadGroup tg = Thread.currentThread().getThreadGroup();
                    for (ThreadGroup tgn = tg;
                         tgn != null;
                         tg = tgn, tgn = tg.getParent());
                    Thread sft = new Thread(tg, proc, "Secondary finalizer");

                    if (TenantGlobals.isDataIsolationEnabled() && TenantContainer.current() != null) {
                        SharedSecrets.getTenantAccess()
                                .registerServiceThread(TenantContainer.current(), sft);
                    }

                    sft.start();
                    try {
                        sft.join();
                    } catch (InterruptedException x) {
                        Thread.currentThread().interrupt();
                    }
                    return null;
                }});
    }

    /* Called by Runtime.runFinalization() */
    static void runFinalization() {
        if (!VM.isBooted()) {
            return;
        }

        forkSecondaryFinalizer(new Runnable() {
            private volatile boolean running;
            public void run() {
                // in case of recursive call to run()
                if (running)
                    return;
                final JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
                running = true;
                ReferenceQueue<Object> q = getQueue();
                for (;;) {
                    Finalizer f = (Finalizer)q.poll();
                    if (f == null) break;
                    f.runFinalizer(jla);
                }
            }
        });
    }

    /* Invoked by java.lang.Shutdown */
    static void runAllFinalizers() {
        if (!VM.isBooted()) {
            return;
        }

        forkSecondaryFinalizer(new Runnable() {
            private volatile boolean running;
            public void run() {
                // in case of recursive call to run()
                if (running)
                    return;
                final JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
                running = true;
                for (;;) {
                    Finalizer f;
                    if (TenantGlobals.isDataIsolationEnabled() && TenantContainer.current() != null) {
                        TenantData td = TenantContainer.current().getTenantData();
                        synchronized (lock) {
                            f = td.getFieldValue(Finalizer.class, ID_UNFINALIZED);
                            if (f == null) break;
                            td.setFieldValue(Finalizer.class, ID_UNFINALIZED, f.next);
                        }
                    } else {
                        synchronized (lock) {
                            f = unfinalized;
                            if (f == null) break;
                            unfinalized = f.next;
                        }
                    }
                    f.runFinalizer(jla);
                }}});
    }

=======
>>>>>>> dragonwell_extended_upstream/master
    private static class FinalizerThread extends Thread {
        private volatile boolean running;
        FinalizerThread(ThreadGroup g) {
            super(g, "Finalizer");
        }
        public void run() {
            // in case of recursive call to run()
            if (running)
                return;

            // Finalizer thread starts before System.initializeSystemClass
            // is called.  Wait until JavaLangAccess is available
            while (!VM.isBooted()) {
                // delay until VM completes initialization
                try {
                    VM.awaitBooted();
                } catch (InterruptedException x) {
                    // ignore and continue
                }
            }
            final JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
            running = true;
            ReferenceQueue<Object> q = getQueue();
            for (;q != null;) {
                try {
                    Finalizer f = (Finalizer)q.remove();
                    f.runFinalizer(jla);
                } catch (InterruptedException x) {
                    // ignore and continue
                }

                // terminate Finalizer thread proactively after TenantContainer.destroy()
                if (TenantGlobals.isDataIsolationEnabled()
                        && TenantContainer.current() != null
                        && TenantContainer.current().getState() == TenantState.DEAD) {
                    break;
                }
            }
        }
    }

    static {
        ThreadGroup tg = Thread.currentThread().getThreadGroup();
        for (ThreadGroup tgn = tg;
             tgn != null;
             tg = tgn, tgn = tg.getParent());
        Thread finalizer = new FinalizerThread(tg);
        finalizer.setPriority(Thread.MAX_PRIORITY - 2);
        finalizer.setDaemon(true);
        finalizer.start();
    }

}
