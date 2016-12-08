/*
 * Copyright (c) 2001, 2013, Oracle and/or its affiliates. All rights reserved.
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

package sun.reflect;

import java.io.Externalizable;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamClass;
import java.io.OptionalDataException;
import java.io.Serializable;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Field;
import java.lang.reflect.Executable;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.lang.reflect.Modifier;
import java.security.AccessController;
import java.security.Permission;
import java.security.PrivilegedAction;
import java.util.Objects;

import sun.reflect.misc.ReflectUtil;


/** <P> The master factory for all reflective objects, both those in
    java.lang.reflect (Fields, Methods, Constructors) as well as their
    delegates (FieldAccessors, MethodAccessors, ConstructorAccessors).
    </P>

    <P> The methods in this class are extremely unsafe and can cause
    subversion of both the language and the verifier. For this reason,
    they are all instance methods, and access to the constructor of
    this factory is guarded by a security check, in similar style to
    {@link sun.misc.Unsafe}. </P>
*/

public class ReflectionFactory {

    private static boolean initted = false;
    private static final Permission reflectionFactoryAccessPerm
        = new RuntimePermission("reflectionFactoryAccess");
    private static final ReflectionFactory soleInstance = new ReflectionFactory();
    // Provides access to package-private mechanisms in java.lang.reflect
    private static volatile LangReflectAccess langReflectAccess;

    /* Method for static class initializer <clinit>, or null */
    private static volatile Method hasStaticInitializerMethod;

    //
    // "Inflation" mechanism. Loading bytecodes to implement
    // Method.invoke() and Constructor.newInstance() currently costs
    // 3-4x more than an invocation via native code for the first
    // invocation (though subsequent invocations have been benchmarked
    // to be over 20x faster). Unfortunately this cost increases
    // startup time for certain applications that use reflection
    // intensively (but only once per class) to bootstrap themselves.
    // To avoid this penalty we reuse the existing JVM entry points
    // for the first few invocations of Methods and Constructors and
    // then switch to the bytecode-based implementations.
    //
    // Package-private to be accessible to NativeMethodAccessorImpl
    // and NativeConstructorAccessorImpl
    private static boolean noInflation        = false;
    private static int     inflationThreshold = 15;

    private ReflectionFactory() {}

    /**
     * A convenience class for acquiring the capability to instantiate
     * reflective objects.  Use this instead of a raw call to {@link
     * #getReflectionFactory} in order to avoid being limited by the
     * permissions of your callers.
     *
     * <p>An instance of this class can be used as the argument of
     * <code>AccessController.doPrivileged</code>.
     */
    public static final class GetReflectionFactoryAction
        implements PrivilegedAction<ReflectionFactory> {
        public ReflectionFactory run() {
            return getReflectionFactory();
        }
    }

    /**
     * Provides the caller with the capability to instantiate reflective
     * objects.
     *
     * <p> First, if there is a security manager, its
     * <code>checkPermission</code> method is called with a {@link
     * java.lang.RuntimePermission} with target
     * <code>"reflectionFactoryAccess"</code>.  This may result in a
     * security exception.
     *
     * <p> The returned <code>ReflectionFactory</code> object should be
     * carefully guarded by the caller, since it can be used to read and
     * write private data and invoke private methods, as well as to load
     * unverified bytecodes.  It must never be passed to untrusted code.
     *
     * @exception SecurityException if a security manager exists and its
     *             <code>checkPermission</code> method doesn't allow
     *             access to the RuntimePermission "reflectionFactoryAccess".  */
    public static ReflectionFactory getReflectionFactory() {
        SecurityManager security = System.getSecurityManager();
        if (security != null) {
            // TO DO: security.checkReflectionFactoryAccess();
            security.checkPermission(reflectionFactoryAccessPerm);
        }
        return soleInstance;
    }

    //--------------------------------------------------------------------------
    //
    // Routines used by java.lang.reflect
    //
    //

    /** Called only by java.lang.reflect.Modifier's static initializer */
    public void setLangReflectAccess(LangReflectAccess access) {
        langReflectAccess = access;
    }

    /**
     * Note: this routine can cause the declaring class for the field
     * be initialized and therefore must not be called until the
     * first get/set of this field.
     * @param field the field
     * @param override true if caller has overridden aaccessibility
     */
    public FieldAccessor newFieldAccessor(Field field, boolean override) {
        checkInitted();
        return UnsafeFieldAccessorFactory.newFieldAccessor(field, override);
    }

    public MethodAccessor newMethodAccessor(Method method) {
        checkInitted();

        if (noInflation && !ReflectUtil.isVMAnonymousClass(method.getDeclaringClass())) {
            return new MethodAccessorGenerator().
                generateMethod(method.getDeclaringClass(),
                               method.getName(),
                               method.getParameterTypes(),
                               method.getReturnType(),
                               method.getExceptionTypes(),
                               method.getModifiers());
        } else {
            NativeMethodAccessorImpl acc =
                new NativeMethodAccessorImpl(method);
            DelegatingMethodAccessorImpl res =
                new DelegatingMethodAccessorImpl(acc);
            acc.setParent(res);
            return res;
        }
    }

    public ConstructorAccessor newConstructorAccessor(Constructor<?> c) {
        checkInitted();

        Class<?> declaringClass = c.getDeclaringClass();
        if (Modifier.isAbstract(declaringClass.getModifiers())) {
            return new InstantiationExceptionConstructorAccessorImpl(null);
        }
        if (declaringClass == Class.class) {
            return new InstantiationExceptionConstructorAccessorImpl
                ("Can not instantiate java.lang.Class");
        }
        // Bootstrapping issue: since we use Class.newInstance() in
        // the ConstructorAccessor generation process, we have to
        // break the cycle here.
        if (Reflection.isSubclassOf(declaringClass,
                                    ConstructorAccessorImpl.class)) {
            return new BootstrapConstructorAccessorImpl(c);
        }

        if (noInflation && !ReflectUtil.isVMAnonymousClass(c.getDeclaringClass())) {
            return new MethodAccessorGenerator().
                generateConstructor(c.getDeclaringClass(),
                                    c.getParameterTypes(),
                                    c.getExceptionTypes(),
                                    c.getModifiers());
        } else {
            NativeConstructorAccessorImpl acc =
                new NativeConstructorAccessorImpl(c);
            DelegatingConstructorAccessorImpl res =
                new DelegatingConstructorAccessorImpl(acc);
            acc.setParent(res);
            return res;
        }
    }

    //--------------------------------------------------------------------------
    //
    // Routines used by java.lang
    //
    //

    /** Creates a new java.lang.reflect.Field. Access checks as per
        java.lang.reflect.AccessibleObject are not overridden. */
    public Field newField(Class<?> declaringClass,
                          String name,
                          Class<?> type,
                          int modifiers,
                          int slot,
                          String signature,
                          byte[] annotations)
    {
        return langReflectAccess().newField(declaringClass,
                                            name,
                                            type,
                                            modifiers,
                                            slot,
                                            signature,
                                            annotations);
    }

    /** Creates a new java.lang.reflect.Method. Access checks as per
        java.lang.reflect.AccessibleObject are not overridden. */
    public Method newMethod(Class<?> declaringClass,
                            String name,
                            Class<?>[] parameterTypes,
                            Class<?> returnType,
                            Class<?>[] checkedExceptions,
                            int modifiers,
                            int slot,
                            String signature,
                            byte[] annotations,
                            byte[] parameterAnnotations,
                            byte[] annotationDefault)
    {
        return langReflectAccess().newMethod(declaringClass,
                                             name,
                                             parameterTypes,
                                             returnType,
                                             checkedExceptions,
                                             modifiers,
                                             slot,
                                             signature,
                                             annotations,
                                             parameterAnnotations,
                                             annotationDefault);
    }

    /** Creates a new java.lang.reflect.Constructor. Access checks as
        per java.lang.reflect.AccessibleObject are not overridden. */
    public Constructor<?> newConstructor(Class<?> declaringClass,
                                         Class<?>[] parameterTypes,
                                         Class<?>[] checkedExceptions,
                                         int modifiers,
                                         int slot,
                                         String signature,
                                         byte[] annotations,
                                         byte[] parameterAnnotations)
    {
        return langReflectAccess().newConstructor(declaringClass,
                                                  parameterTypes,
                                                  checkedExceptions,
                                                  modifiers,
                                                  slot,
                                                  signature,
                                                  annotations,
                                                  parameterAnnotations);
    }

    /** Gets the MethodAccessor object for a java.lang.reflect.Method */
    public MethodAccessor getMethodAccessor(Method m) {
        return langReflectAccess().getMethodAccessor(m);
    }

    /** Sets the MethodAccessor object for a java.lang.reflect.Method */
    public void setMethodAccessor(Method m, MethodAccessor accessor) {
        langReflectAccess().setMethodAccessor(m, accessor);
    }

    /** Gets the ConstructorAccessor object for a
        java.lang.reflect.Constructor */
    public ConstructorAccessor getConstructorAccessor(Constructor<?> c) {
        return langReflectAccess().getConstructorAccessor(c);
    }

    /** Sets the ConstructorAccessor object for a
        java.lang.reflect.Constructor */
    public void setConstructorAccessor(Constructor<?> c,
                                       ConstructorAccessor accessor)
    {
        langReflectAccess().setConstructorAccessor(c, accessor);
    }

    /** Makes a copy of the passed method. The returned method is a
        "child" of the passed one; see the comments in Method.java for
        details. */
    public Method copyMethod(Method arg) {
        return langReflectAccess().copyMethod(arg);
    }

    /** Makes a copy of the passed field. The returned field is a
        "child" of the passed one; see the comments in Field.java for
        details. */
    public Field copyField(Field arg) {
        return langReflectAccess().copyField(arg);
    }

    /** Makes a copy of the passed constructor. The returned
        constructor is a "child" of the passed one; see the comments
        in Constructor.java for details. */
    public <T> Constructor<T> copyConstructor(Constructor<T> arg) {
        return langReflectAccess().copyConstructor(arg);
    }

    /** Gets the byte[] that encodes TypeAnnotations on an executable.
     */
    public byte[] getExecutableTypeAnnotationBytes(Executable ex) {
        return langReflectAccess().getExecutableTypeAnnotationBytes(ex);
    }

    //--------------------------------------------------------------------------
    //
    // Routines used by serialization
    //
    //

    /**
     * Returns an accessible constructor capable of creating instances
     * of the given class, initialized by the given constructor.
     *
     * @param classToInstantiate the class to instantiate
     * @param constructorToCall the constructor to call
     * @return an accessible constructor
     */
    public Constructor<?> newConstructorForSerialization
        (Class<?> classToInstantiate, Constructor<?> constructorToCall)
    {
        // Fast path
        if (constructorToCall.getDeclaringClass() == classToInstantiate) {
            return constructorToCall;
        }
        return generateConstructor(classToInstantiate, constructorToCall);
    }

    /**
     * Returns an accessible no-arg constructor for a class.
     * The no-arg constructor is found searching the class and its supertypes.
     *
     * @param cl the class to instantiate
     * @return a no-arg constructor for the class or {@code null} if
     *     the class or supertypes do not have a suitable no-arg constructor
     */
    public final Constructor<?> newConstructorForSerialization(Class<?> cl) {
        Class<?> initCl = cl;
        while (Serializable.class.isAssignableFrom(initCl)) {
            if ((initCl = initCl.getSuperclass()) == null) {
                return null;
            }
        }
        Constructor<?> constructorToCall;
        try {
            constructorToCall = initCl.getDeclaredConstructor();
            int mods = constructorToCall.getModifiers();
            if ((mods & Modifier.PRIVATE) != 0 ||
                    ((mods & (Modifier.PUBLIC | Modifier.PROTECTED)) == 0 &&
                            !packageEquals(cl, initCl))) {
                return null;
            }
        } catch (NoSuchMethodException ex) {
            return null;
        }
        return generateConstructor(cl, constructorToCall);
    }

    private final Constructor<?> generateConstructor(Class<?> classToInstantiate,
                                                     Constructor<?> constructorToCall) {


        ConstructorAccessor acc = new MethodAccessorGenerator().
            generateSerializationConstructor(classToInstantiate,
                                             constructorToCall.getParameterTypes(),
                                             constructorToCall.getExceptionTypes(),
                                             constructorToCall.getModifiers(),
                                             constructorToCall.getDeclaringClass());
        Constructor<?> c = newConstructor(constructorToCall.getDeclaringClass(),
                                          constructorToCall.getParameterTypes(),
                                          constructorToCall.getExceptionTypes(),
                                          constructorToCall.getModifiers(),
                                          langReflectAccess().
                                          getConstructorSlot(constructorToCall),
                                          langReflectAccess().
                                          getConstructorSignature(constructorToCall),
                                          langReflectAccess().
                                          getConstructorAnnotations(constructorToCall),
                                          langReflectAccess().
                                          getConstructorParameterAnnotations(constructorToCall));
        setConstructorAccessor(c, acc);
        c.setAccessible(true);
        return c;
    }

    /**
     * Returns an accessible no-arg constructor for an externalizable class to be
     * initialized using a public no-argument constructor.
     *
     * @param cl the class to instantiate
     * @return A no-arg constructor for the class; returns {@code null} if
     *     the class does not implement {@link java.io.Externalizable}
     */
    public final Constructor<?> newConstructorForExternalization(Class<?> cl) {
        if (!Externalizable.class.isAssignableFrom(cl)) {
            return null;
        }
        try {
            Constructor<?> cons = cl.getConstructor();
            cons.setAccessible(true);
            return cons;
        } catch (NoSuchMethodException ex) {
            return null;
        }
    }

    /**
     * Returns a direct MethodHandle for the {@code readObject} method on
     * a Serializable class.
     * The first argument of {@link MethodHandle#invoke} is the serializable
     * object and the second argument is the {@code ObjectInputStream} passed to
     * {@code readObject}.
     *
     * @param cl a Serializable class
     * @return  a direct MethodHandle for the {@code readObject} method of the class or
     *          {@code null} if the class does not have a {@code readObject} method
     */
    public final MethodHandle readObjectForSerialization(Class<?> cl) {
        return findReadWriteObjectForSerialization(cl, "readObject", ObjectInputStream.class);
    }

    /**
     * Returns a direct MethodHandle for the {@code readObjectNoData} method on
     * a Serializable class.
     * The first argument of {@link MethodHandle#invoke} is the serializable
     * object and the second argument is the {@code ObjectInputStream} passed to
     * {@code readObjectNoData}.
     *
     * @param cl a Serializable class
     * @return  a direct MethodHandle for the {@code readObjectNoData} method
     *          of the class or {@code null} if the class does not have a
     *          {@code readObjectNoData} method
     */
    public final MethodHandle readObjectNoDataForSerialization(Class<?> cl) {
        return findReadWriteObjectForSerialization(cl, "readObjectNoData", ObjectInputStream.class);
    }

    /**
     * Returns a direct MethodHandle for the {@code writeObject} method on
     * a Serializable class.
     * The first argument of {@link MethodHandle#invoke} is the serializable
     * object and the second argument is the {@code ObjectOutputStream} passed to
     * {@code writeObject}.
     *
     * @param cl a Serializable class
     * @return  a direct MethodHandle for the {@code writeObject} method of the class or
     *          {@code null} if the class does not have a {@code writeObject} method
     */
    public final MethodHandle writeObjectForSerialization(Class<?> cl) {
        return findReadWriteObjectForSerialization(cl, "writeObject", ObjectOutputStream.class);
    }

    private final MethodHandle findReadWriteObjectForSerialization(Class<?> cl,
                                                                   String methodName,
                                                                   Class<?> streamClass) {
        if (!Serializable.class.isAssignableFrom(cl)) {
            return null;
        }

        try {
            Method meth = cl.getDeclaredMethod(methodName, streamClass);
            int mods = meth.getModifiers();
            if (meth.getReturnType() != Void.TYPE ||
                    Modifier.isStatic(mods) ||
                    !Modifier.isPrivate(mods)) {
                return null;
            }
            meth.setAccessible(true);
            return MethodHandles.lookup().unreflect(meth);
        } catch (NoSuchMethodException ex) {
            return null;
        } catch (IllegalAccessException ex1) {
            throw new InternalError("Error", ex1);
        }
    }

    /**
     * Returns a direct MethodHandle for the {@code readResolve} method on
     * a serializable class.
     * The single argument of {@link MethodHandle#invoke} is the serializable
     * object.
     *
     * @param cl the Serializable class
     * @return  a direct MethodHandle for the {@code readResolve} method of the class or
     *          {@code null} if the class does not have a {@code readResolve} method
     */
    public final MethodHandle readResolveForSerialization(Class<?> cl) {
        return getReplaceResolveForSerialization(cl, "readResolve");
    }

    /**
     * Returns a direct MethodHandle for the {@code writeReplace} method on
     * a serializable class.
     * The single argument of {@link MethodHandle#invoke} is the serializable
     * object.
     *
     * @param cl the Serializable class
     * @return  a direct MethodHandle for the {@code writeReplace} method of the class or
     *          {@code null} if the class does not have a {@code writeReplace} method
     */
    public final MethodHandle writeReplaceForSerialization(Class<?> cl) {
        return getReplaceResolveForSerialization(cl, "writeReplace");
    }

    /**
     * Returns a direct MethodHandle for the {@code writeReplace} method on
     * a serializable class.
     * The single argument of {@link MethodHandle#invoke} is the serializable
     * object.
     *
     * @param cl the Serializable class
     * @return  a direct MethodHandle for the {@code writeReplace} method of the class or
     *          {@code null} if the class does not have a {@code writeReplace} method
     */
    private MethodHandle getReplaceResolveForSerialization(Class<?> cl,
                                                           String methodName) {
        if (!Serializable.class.isAssignableFrom(cl)) {
            return null;
        }

        Class<?> defCl = cl;
        while (defCl != null) {
            try {
                Method m = defCl.getDeclaredMethod(methodName);
                if (m.getReturnType() != Object.class) {
                    return null;
                }
                int mods = m.getModifiers();
                if (Modifier.isStatic(mods) | Modifier.isAbstract(mods)) {
                    return null;
                } else if (Modifier.isPublic(mods) | Modifier.isProtected(mods)) {
                    // fall through
                } else if (Modifier.isPrivate(mods) && (cl != defCl)) {
                    return null;
                } else if (!packageEquals(cl, defCl)) {
                    return null;
                }
                try {
                    // Normal return
                    m.setAccessible(true);
                    return MethodHandles.lookup().unreflect(m);
                } catch (IllegalAccessException ex0) {
                    // setAccessible should prevent IAE
                    throw new InternalError("Error", ex0);
                }
            } catch (NoSuchMethodException ex) {
                defCl = defCl.getSuperclass();
            }
        }
        return null;
    }

    /**
     * Returns true if the class has a static initializer.
     * The presence of a static initializer is used to compute the serialVersionUID.
     * @param cl a serializable classLook
     * @return {@code true} if the class has a static initializer,
     *          otherwise {@code false}
     */
    public final boolean hasStaticInitializerForSerialization(Class<?> cl) {
        Method m = hasStaticInitializerMethod;
        if (m == null) {
            try {
                m = ObjectStreamClass.class.getDeclaredMethod("hasStaticInitializer",
                        new Class<?>[]{Class.class});
                m.setAccessible(true);
                hasStaticInitializerMethod = m;
            } catch (NoSuchMethodException ex) {
                throw new InternalError("No such method hasStaticInitializer on "
                        + ObjectStreamClass.class, ex);
            }
        }
        try {
            return (Boolean) m.invoke(null, cl);
        } catch (InvocationTargetException | IllegalAccessException ex) {
            throw new InternalError("Exception invoking hasStaticInitializer", ex);
        }
    }

    /**
     * Returns a new OptionalDataException with {@code eof} set to {@code true}
     * or {@code false}.
     * @param bool the value of {@code eof} in the created OptionalDataException
     * @return  a new OptionalDataException
     */
    public final OptionalDataException newOptionalDataExceptionForSerialization(boolean bool) {
        try {
            Constructor<OptionalDataException> boolCtor =
                    OptionalDataException.class.getDeclaredConstructor(Boolean.TYPE);
            boolCtor.setAccessible(true);
            return boolCtor.newInstance(bool);
        } catch (NoSuchMethodException | InstantiationException|
                IllegalAccessException|InvocationTargetException ex) {
            throw new InternalError("unable to create OptionalDataException", ex);
        }
    }

    //--------------------------------------------------------------------------
    //
    // Internals only below this point
    //

    static int inflationThreshold() {
        return inflationThreshold;
    }

    /** We have to defer full initialization of this class until after
        the static initializer is run since java.lang.reflect.Method's
        static initializer (more properly, that for
        java.lang.reflect.AccessibleObject) causes this class's to be
        run, before the system properties are set up. */
    private static void checkInitted() {
        if (initted) return;
        AccessController.doPrivileged(
            new PrivilegedAction<Void>() {
                public Void run() {
                    // Tests to ensure the system properties table is fully
                    // initialized. This is needed because reflection code is
                    // called very early in the initialization process (before
                    // command-line arguments have been parsed and therefore
                    // these user-settable properties installed.) We assume that
                    // if System.out is non-null then the System class has been
                    // fully initialized and that the bulk of the startup code
                    // has been run.

                    if (System.out == null) {
                        // java.lang.System not yet fully initialized
                        return null;
                    }

                    String val = System.getProperty("sun.reflect.noInflation");
                    if (val != null && val.equals("true")) {
                        noInflation = true;
                    }

                    val = System.getProperty("sun.reflect.inflationThreshold");
                    if (val != null) {
                        try {
                            inflationThreshold = Integer.parseInt(val);
                        } catch (NumberFormatException e) {
                            throw new RuntimeException("Unable to parse property sun.reflect.inflationThreshold", e);
                        }
                    }

                    initted = true;
                    return null;
                }
            });
    }

    private static LangReflectAccess langReflectAccess() {
        if (langReflectAccess == null) {
            // Call a static method to get class java.lang.reflect.Modifier
            // initialized. Its static initializer will cause
            // setLangReflectAccess() to be called from the context of the
            // java.lang.reflect package.
            Modifier.isPublic(Modifier.PUBLIC);
        }
        return langReflectAccess;
    }

    /**
     * Returns true if classes are defined in the classloader and same package, false
     * otherwise.
     * @param cl1 a class
     * @param cl2 another class
     * @returns true if the two classes are in the same classloader and package
     */
    private static boolean packageEquals(Class<?> cl1, Class<?> cl2) {
        return cl1.getClassLoader() == cl2.getClassLoader() &&
                Objects.equals(cl1.getPackage(), cl2.getPackage());
    }

}
