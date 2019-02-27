/*
 * Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.
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

package jdk.jfr.internal;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import jdk.internal.org.xml.sax.Attributes;
import jdk.internal.org.xml.sax.EntityResolver;
import jdk.internal.org.xml.sax.SAXException;
import jdk.internal.org.xml.sax.helpers.DefaultHandler;
import jdk.internal.util.xml.SAXParser;
import jdk.internal.util.xml.impl.SAXParserImpl;
import jdk.jfr.AnnotationElement;
import jdk.jfr.Category;
import jdk.jfr.Description;
import jdk.jfr.Enabled;
import jdk.jfr.Experimental;
import jdk.jfr.Label;
import jdk.jfr.MemoryAddress;
import jdk.jfr.DataAmount;
import jdk.jfr.Percentage;
import jdk.jfr.Period;
import jdk.jfr.Relational;
import jdk.jfr.StackTrace;
import jdk.jfr.Threshold;
import jdk.jfr.Timespan;
import jdk.jfr.Timestamp;
import jdk.jfr.TransitionFrom;
import jdk.jfr.TransitionTo;
import jdk.jfr.Unsigned;
import jdk.jfr.ValueDescriptor;

final class TraceHandler extends DefaultHandler implements EntityResolver {

    private static class ValueDeclaration {
        private final String typeName;
        private final String field;
        private final String label;
        private final boolean experimental;
        private final String description;
        private final String relation;
        private final int dimension;
        private final Type type;
        private final boolean transitionFrom;
        private final boolean transitionTo;

        public ValueDeclaration(Type type, int dimension, Attributes attributes) {
            typeName = attributes.getValue(ATTRIBUTE_TYPE);
            field = attributes.getValue(ATTRIBUTE_FIELD);
            label = attributes.getValue(ATTRIBUTE_LABEL);
            experimental = "true".equals(attributes.getValue(ATTRIBUTE_EXPERIMENTAL));
            description = attributes.getValue(ATTRIBUTE_DESCRIPTION);
            String t = attributes.getValue(ATTRIBUTE_RELATION);
            relation = "NOT_AVAILABLE".equals(t) ? null : t;
            String transition = attributes.getValue(ATTRIBUTE_TRANSITION);
            transitionFrom = "FROM".equals(transition);
            transitionTo = "TO".equals(transition);
            this.dimension = dimension;
            this.type = type;
        }
    }
    private static final Map<String, String> knownCategorySegments = new HashMap<>();
    static {
        knownCategorySegments.put("os", "Operating System");
        knownCategorySegments.put("java", "Java Application");
        knownCategorySegments.put("vm", "Java Virtual Machine");
        knownCategorySegments.put("class", "Class Loading");
        knownCategorySegments.put("gc", "GC");
        knownCategorySegments.put("prof", "Profiling");
        knownCategorySegments.put("flight_recorder", "Flight Recorder");
    }
    private static final String ELEMENT_CONTENT_TYPE = "content_type";
    private static final String ELEMENT_PRIMARY_TYPE = "primary_type";
    private static final String ELEMENT_EVENT = "event";
    private static final String ELEMENT_VALUE = "value";
    private static final String ELEMENT_STRUCT_VALUE ="structvalue";
    private static final String ELEMENT_STRUCT_TYPE = "struct_type";
    private static final String ELEMENT_STRUCT_ARRAY = "structarray";
    private static final String ELEMENT_STRUCT = "struct";
    private static final String ATTRIBUTE_LABEL = "label";
    private static final String ATTRIBUTE_EXPERIMENTAL = "experimental";
    private static final String ATTRIBUTE_TRANSITION = "transition";
    private static final String ATTRIBUTE_ID = "id";
    private static final String ATTRIBUTE_IS_CONSTANT = "is_constant";
    private static final String IS_REQUESTABLE = "is_requestable";
    private static final String ATTRIBUTE_IS_REQUESTABLE = IS_REQUESTABLE;
    private static final String ATTRIBUTE_IS_INSTANT = "is_instant";
    private static final String ATTRIBUTE_CUTOFF = "cutoff";
    private static final String ATTRIBUTE_URI = "uri";
    private static final String ATTRIBUTE_CONTENTTYPE = "contenttype";
    private static final String ATTRIBUTE_FIELD = "field";
    private static final String ATTRIBUTE_TYPE = "type";
    private static final String ATTRIBUTE_DESCRIPTION = "description";
    private static final String ATTRIBUTE_HR_NAME = "hr_name";
    private static final String ATTRIBUTE_RELATION = "relation";
    private static final String ATTRIBUTE_DATATYPE = "datatype";
    private static final String ATTRIBUTE_SYMBOL = "symbol";
    private static final String ATTRIBUTE_BUILTIN_TYPE = "builtin_type";
    private static final String ATTRIBUTE_JVM_TYPE = "jvm_type";
    private static final String ATTRIBUTE_PATH = "path";
    private static final String ATTRIBUTE_VALUE_NONE = "NONE";
    private static final String ELEMENT_RELATION_DECL = "relation_decl";
    private static final String ATTRIBUTE_HAS_STACKTRACE = "has_stacktrace";
    private static final String ATTRIBUTE_HAS_THREAD = "has_thread";

    private final Map<String, Type> types = new LinkedHashMap<>(200);
    private final Map<String, String> typedef = new HashMap<>(100);
    private final Map<String, AnnotationElement> annotationTypes = new HashMap<>();
    private final Map<String, Type> unsignedTypes = new HashMap<>();
    private final List<ValueDeclaration> valueDeclarations = new ArrayList<>(1000);

    private Type type;
    private long structTypeId = 255;
    private long jvmTypeId = 33; // content_type with jvm_type attribute, others hard wired

    TraceHandler() {

        // Content types handled using annotation in Java
        annotationTypes.put("BYTES64", new AnnotationElement(DataAmount.class, DataAmount.BYTES));
        annotationTypes.put("BYTES", new AnnotationElement(DataAmount.class, DataAmount.BYTES));
        annotationTypes.put("MILLIS", new AnnotationElement(Timespan.class, Timespan.MILLISECONDS));
        annotationTypes.put("EPOCHMILLIS", new AnnotationElement(Timestamp.class, Timestamp.MILLISECONDS_SINCE_EPOCH));
        annotationTypes.put("NANOS", new AnnotationElement(Timespan.class, Timespan.NANOSECONDS));
        annotationTypes.put("TICKSPAN",  new AnnotationElement(Timespan.class, Timespan.TICKS));
        annotationTypes.put("TICKS",  new AnnotationElement(Timestamp.class, Timespan.TICKS));
        annotationTypes.put("ADDRESS", new AnnotationElement(MemoryAddress.class));
        annotationTypes.put("PERCENTAGE",  new AnnotationElement(Percentage.class));

        // Add known unsigned types, and their counter part in Java
        unsignedTypes.put("U8", Type.LONG);
        unsignedTypes.put("U4", Type.INT);
        unsignedTypes.put("U2", Type.SHORT);
        unsignedTypes.put("U1", Type.BYTE);

        // Map trace.xml primitive to Java type
        typedef.put("U8", Type.LONG.getName());
        typedef.put("U4", Type.INT.getName());
        typedef.put("U2", Type.SHORT.getName());
        typedef.put("U1", Type.BYTE.getName());
        typedef.put("LONG", Type.LONG.getName());
        typedef.put("INT", Type.INT.getName());
        typedef.put("SHORT", Type.SHORT.getName());
        typedef.put("BYTE", Type.BYTE.getName());
        typedef.put("DOUBLE", Type.DOUBLE.getName());
        typedef.put("BOOLEAN", Type.BOOLEAN.getName());
        typedef.put("FLOAT", Type.FLOAT.getName());
        typedef.put("CHAR", Type.CHAR.getName());
        typedef.put("STRING", Type.STRING.getName());
        typedef.put("THREAD", Type.THREAD.getName());
        typedef.put("CLASS", Type.CLASS.getName());
        // Add known types
        for (Type type : Type.getKnownTypes()) {
            types.put(type.getName(), type);
        }
    }

    @Override
    public void startElement(String uri, String localName, String qName, Attributes attributes) throws SAXException {
        switch (qName) {
        case ELEMENT_RELATION_DECL:
            String relationalURI = attributes.getValue(ATTRIBUTE_URI);
            String id = attributes.getValue(ATTRIBUTE_ID);
            typedef.put(id, relationalURI);
            break;
        case ELEMENT_PRIMARY_TYPE:
            addTypedef(attributes);
            break;
        case ELEMENT_STRUCT_ARRAY:
            valueDeclarations.add(new ValueDeclaration(type, 1, attributes));
            break;
        case ELEMENT_VALUE:
        case ELEMENT_STRUCT_VALUE:
            valueDeclarations.add(new ValueDeclaration(type, 0, attributes));
            break;
        case ELEMENT_EVENT:
            type = createType(attributes, true, structTypeId++, false);
            boolean stackTrace = getBoolean(attributes, ATTRIBUTE_HAS_STACKTRACE);
            boolean thread = getBoolean(attributes, (ATTRIBUTE_HAS_THREAD));
            boolean instant = getBoolean(attributes, (ATTRIBUTE_IS_INSTANT));
            boolean requestable = getBoolean(attributes, (ATTRIBUTE_IS_REQUESTABLE));
            boolean constant = getBoolean(attributes, (ATTRIBUTE_IS_CONSTANT));
            boolean duration = !requestable && !instant;
            boolean experimental = getBoolean(attributes, ATTRIBUTE_EXPERIMENTAL);
            boolean cutoff =  getBoolean(attributes, ATTRIBUTE_CUTOFF);
            TypeLibrary.addImplicitFields(type, requestable, duration, thread, stackTrace, cutoff);
            ArrayList<AnnotationElement> aes = new ArrayList<>();
            aes.addAll(type.getAnnotationElements());
            if (requestable) {
                String period = constant ? "endChunk" : "everyChunk";
                aes.add(new AnnotationElement(Period.class, period));
            } else {
                if (!instant) {
                    aes.add(new AnnotationElement(Threshold.class, "0 ns"));
                }
                if (stackTrace) {
                    aes.add(new AnnotationElement(StackTrace.class, true));
                }
            }
            if (cutoff) {
                aes.add(new AnnotationElement(Cutoff.class, Cutoff.INIFITY));
            }
            if (experimental) {
                aes.add(new AnnotationElement(Experimental.class));
            }
            aes.add(new AnnotationElement(Enabled.class, false));
            aes.trimToSize();
            type.setAnnotations(aes);
            break;
        case ELEMENT_CONTENT_TYPE:
            if (attributes.getValue(ATTRIBUTE_BUILTIN_TYPE) != null) {
                type = createType(attributes, false, builtInId(attributes), true);
            } else {
                type = createType(attributes, false, jvmTypeId++, true);
            }
            break;
        case ELEMENT_STRUCT_TYPE:
        case ELEMENT_STRUCT:
            type = createType(attributes, false, structTypeId++, false);
            break;
        }
    }

    private long builtInId(Attributes attributes) {
        String t = attributes.getValue(ATTRIBUTE_ID);
        if ("Thread".equals(t)) {
            return Type.THREAD.getId();
        }
        if ("STRING".equals(t)) {
            return Type.STRING.getId();
        }
        if ("Class".equals(t)) {
            return Type.CLASS.getId();
        }
        for (Type type : Type.getKnownTypes()) {
            if (type.getName().equals(Type.ORACLE_TYPE_PREFIX + t)) {
               return type.getId();
            }
        }
        throw new IllegalStateException("Built-in type " + t + " not defined in Java");
    }

    private boolean getBoolean(Attributes attributes, String attribute) {
        return "true".equals(attributes.getValue(attribute));
    }

    @Override
    public void endElement(String uri, String localName, String qName) {
        switch (qName) {
        case ELEMENT_CONTENT_TYPE:
        case ELEMENT_EVENT:
        case ELEMENT_STRUCT_TYPE:
        case ELEMENT_STRUCT:
            types.put(type.getName(), type);
            type = null;
            break;
        }
    }

    private void addTypedef(Attributes attributes) {
        String symbol = attributes.getValue(ATTRIBUTE_SYMBOL);
        String contentType = attributes.getValue(ATTRIBUTE_CONTENTTYPE);
        String dataType = attributes.getValue(ATTRIBUTE_DATATYPE);
        if (unsignedTypes.containsKey(dataType)) {
            unsignedTypes.put(symbol, unsignedTypes.get(dataType));
        }
        if (annotationTypes.containsKey(contentType)) {
            typedef.put(symbol, dataType);
            return;
        }
        if (typedef.containsKey(symbol)) {
            return;
        }
        typedef.put(symbol, ATTRIBUTE_VALUE_NONE.equals(contentType) ? dataType : contentType);
    }

    private void defineValues() {
        for (ValueDeclaration decl : valueDeclarations) {
            decl.type.add(createValueDescriptor(decl));
        }
    }

    private ValueDescriptor createValueDescriptor(ValueDeclaration declaration) {
        Type referencedType = null;
        String typeAlias = declaration.typeName;
        while (referencedType == null && typeAlias != null) {
            referencedType = types.get(typeAlias);
            typeAlias = typedef.get(typeAlias);
        }
        if (referencedType == null) {
            throw new InternalError("Trace files contains incompatible type definition");
        }
        List<AnnotationElement> annos = new ArrayList<>();
        if (unsignedTypes.containsKey(declaration.typeName) && !referencedType.isConstantPool()) {
            annos.add(new AnnotationElement(Unsigned.class));
        }
        AnnotationElement contentType = annotationTypes.get(declaration.typeName);
        if (contentType != null) {
            annos.add(contentType);
        }
        if (declaration.relation != null) {
            Type relationType = types.get(typedef.get(declaration.relation));
            if (relationType == null) {
                String typeName = Type.ORACLE_TYPE_PREFIX + declaration.relation;
                typedef.put(declaration.relation, typeName);
                relationType = new Type(typeName, Type.SUPER_TYPE_ANNOTATION, structTypeId++);
                relationType.setAnnotations(Collections.singletonList(new AnnotationElement(Relational.class)));
                types.put(typeName, relationType);
            }
            annos.add(PrivateAccess.getInstance().newAnnotation(relationType, new ArrayList<>(), true));
        }

        if (declaration.label != null) {
            annos.add(new AnnotationElement(Label.class, declaration.label));
        }

        if (declaration.experimental) {
            annos.add(new AnnotationElement(Experimental.class));
        }

        if (declaration.description != null) {
            annos.add(new AnnotationElement(Description.class, declaration.description));
        }
        if (declaration.transitionFrom) {
            annos.add(new AnnotationElement(TransitionFrom.class));
        }
        if (declaration.transitionTo) {
            annos.add(new AnnotationElement(TransitionTo.class));
        }

        return PrivateAccess.getInstance().newValueDescriptor(declaration.field, referencedType, annos, declaration.dimension, referencedType.isConstantPool(), null);
    }

    private Type createType(Attributes attributes, boolean eventType, long typeId, boolean contantPool) {
        String labelAttribute = ATTRIBUTE_LABEL;
        String id = attributes.getValue(ATTRIBUTE_ID);
        String path = attributes.getValue(ATTRIBUTE_PATH);
        String builtInType = attributes.getValue(ATTRIBUTE_BUILTIN_TYPE);
        String jvmType = attributes.getValue(ATTRIBUTE_JVM_TYPE);

        String typeName = makeTypeName(id, path);
        Type t;
        if (eventType) {
            t = new PlatformEventType(typeName, typeId, false, true);
        } else {
            t = new Type(typeName, null, typeId, contantPool);
        }
        typedef.put(id, typeName);
        if (contantPool) {
            labelAttribute = ATTRIBUTE_HR_NAME; // not "label" for some reason?
            if (builtInType != null) {
                typedef.put(builtInType, typeName);
            }
            if (jvmType != null) {
                typedef.put(jvmType, typeName);
            }
        }
        ArrayList<AnnotationElement> aes = new ArrayList<>();
        if (path != null) {
            aes.add(new AnnotationElement(Category.class, makeCategory(path)));
        }
        String label = attributes.getValue(labelAttribute);
        if (label != null) {
            aes.add(new AnnotationElement(Label.class, label));
        }
        String description = attributes.getValue(ATTRIBUTE_DESCRIPTION);
        if (description != null) {
            aes.add(new AnnotationElement(Description.class, description));
        }
        aes.trimToSize();
        t.setAnnotations(aes);
        return t;
    }

    private String makeTypeName(String id, String path) {
        if ("Thread".equals(id)) {
            return Type.THREAD.getName();
        }
        if ("Class".equals(id)) {
            return Type.CLASS.getName();
        }
        return path == null ? Type.ORACLE_TYPE_PREFIX + id : Type.ORACLE_EVENT_PREFIX + id;
    }

    private static String[] makeCategory(String path) {
        List<String> categoryNames = new ArrayList<>();
        int index = 0;
        int segmentEnd;
        while ((segmentEnd = path.indexOf("/", index)) != -1) {
            String pathSegment = path.substring(index, segmentEnd);
            String known = knownCategorySegments.get(pathSegment);
            if (known != null) {
                categoryNames.add(known);
            } else {
                String categorySegment = humanifySegmentPath(pathSegment);
                categoryNames.add(categorySegment);
                knownCategorySegments.put(pathSegment, categorySegment);
            }
            index = segmentEnd + 1;
        }
        String[] result = new String[categoryNames.size()];
        for (int i = 0; i< result.length;i++) {
            result[i] = categoryNames.get(i);
        }
        return result;
    }

    private static String humanifySegmentPath(String pathSegment) {
        char[] chars = pathSegment.toCharArray();
        char lastCharacter = ' '; // signals new word
        for (int i = 0; i < chars.length; i++) {
            char c = pathSegment.charAt(i);
            if (c == '_') {
                chars[i] = ' ';
            }
            if (lastCharacter == ' ') {
                chars[i] = Character.toUpperCase(c);
            }
            lastCharacter = c;
        }
        return new String(chars);
    }

    public static List<Type> createTypes() throws IOException {
        String[] xmls = { "trace.xml", "tracerelationdecls.xml", "traceevents.xml",
                          "tracetypes.xml"};
        TraceHandler t = new TraceHandler();
        try {
             SAXParser parser = new SAXParserImpl();
            for (String xml : xmls) {
                Logger.log(LogTag.JFR_SYSTEM, LogLevel.DEBUG, () -> "Parsing " + xml);
                parser.parse(createInputStream(xml), t);
            }
            t.defineValues();
            return new ArrayList<>(t.types.values());
        }  catch (SAXException  e) {
            throw new IOException(e);
        }
    }
    private static InputStream createInputStream(String name) throws IOException {
        return SecuritySupport.getResourceAsStream("/jdk/jfr/internal/types/" + name);
    }
}
