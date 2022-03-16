package com.alibaba.tenant;

public class FieldReference<T> {
    private T referent;
    /**
     * Constructs a new FieldReference with r as the referent
     * @param r The object to which this FieldReference points
     */
    public FieldReference(T r) {
        this.referent = r;
    }
    /**
     * Returns this FieldReference referent
     * @return The referent
     */
    public T get() {
        return referent;
    }

    /**
     * Sets {@code referent}
     * @param t The new referent value
     */
    public void set(Object t) {
        referent = (T)t;
    }
}