package com.alibaba.tenant;

import java.util.HashMap;
import java.util.Map;
import java.util.WeakHashMap;
import java.util.function.Supplier;

/**
 * Used to store all the data isolated per tenant.
 */
public class TenantData {
    private Map<Object, Map<String, FieldReference<?>>> dataMap = new WeakHashMap<>();

    private Map<String, FieldReference<?>> mapForObject(Object obj) {
        Map<String, FieldReference<?>> map = null;
        if (obj != null) {
            map = dataMap.get(obj);
            if (null == map) {
                map = new HashMap<>();
                dataMap.put(obj, map);
            }
        }
        return map;
    }

    /**
     * Retrieves the value of {@code obj}'s field isolated per tenant
     * @param obj           Object the field associates with
     * @param fieldName     Field name
     * @param supplier      Responsible for creating the initial field value
     * @return              Value of field.
     */
    @SuppressWarnings("unchecked")
    public synchronized <K, T> T getFieldValue(K obj, String fieldName, Supplier<T> supplier) {
        if(null == obj || null == fieldName || null == supplier) {
            throw new IllegalArgumentException("Failed to get the field value, argument is null.");
        }

        Map<String, FieldReference<?>>  objMap = mapForObject(obj);
        FieldReference<?> data = objMap.get(fieldName);
        if(null == data) {
            T initialValue = supplier.get();
            // Only store non-null value to the map
            if (initialValue != null) {
                data = new FieldReference<T>(initialValue);
                objMap.put(fieldName, data);
            }
        }
        return (data == null ? null : (T)data.get());
    }

    /**
     * Retrieves the value of {@code obj}'s field isolated per tenant
     * @param obj           Object the field associates with
     * @param fieldName     Field name
     * @return              Value of field, null if not found
     */
    public synchronized <K, T> T getFieldValue(K obj, String fieldName) {
        return getFieldValue(obj, fieldName, () -> null);
    }

    /**
     * Sets the value for tenant isolated field
     * @param key           Object associated with field.
     * @param fieldName     name of field.
     * @param value         new field value
     */
    public synchronized <K, T> void setFieldValue(K key, String fieldName, T value) {
        if(null == key || null == fieldName) {
            throw new IllegalArgumentException("Failed to get the field value, argument is null.");
        }
        Map<String, FieldReference<?>>  objMap = mapForObject(key);
        FieldReference<?> ref = objMap.get(fieldName);
        if(null != ref) {
            ref.set(value);
        } else {
            objMap.put(fieldName, new FieldReference<>(value));
        }
    }

    /**
     * Clear all recorded isolation data in this tenant
     */
    void clear() {
        dataMap.clear();
    }
}
