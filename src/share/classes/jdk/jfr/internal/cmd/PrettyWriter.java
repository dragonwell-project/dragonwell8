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

package jdk.jfr.internal.cmd;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.StringJoiner;

import jdk.jfr.AnnotationElement;
import jdk.jfr.ValueDescriptor;
import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordedObject;
import jdk.jfr.consumer.RecordingFile;
import jdk.jfr.internal.PrivateAccess;
import jdk.jfr.internal.Type;
import jdk.jfr.internal.consumer.ChunkHeader;
import jdk.jfr.internal.consumer.RecordingInput;

public final class PrettyWriter extends StructuredWriter {

    public PrettyWriter(PrintWriter destination) {
        super(destination);
    }

    void print(Path source) throws FileNotFoundException, IOException {
        try (RecordingInput input = new RecordingInput(source.toFile())) {
            HashSet<Type> typeSet = new HashSet<>();
            for (ChunkHeader ch = new ChunkHeader(input); !ch.isLastChunk(); ch = ch.nextHeader()) {
                typeSet.addAll(ch.readMetadata().getTypes());
            }
            List<Type> types = new ArrayList<>(typeSet);
            Collections.sort(types, (c1, c2) -> Long.compare(c1.getId(), c2.getId()));
            for (Type t : types) {
                printType(t);
            }
            flush();
        }

        try (RecordingFile es = new RecordingFile(source)) {
            while (es.hasMoreEvents()) {
                print(es.readEvent());
                flush();
            }
        }
        flush();
    }

    private void printType(Type t) throws IOException {
        print("// id: ");
        println(String.valueOf(t.getId()));
        int commentIndex = t.getName().length() + 10;
        String typeName = t.getName();
        int index = typeName.lastIndexOf(".");
        if (index != -1) {
            println("package " + typeName.substring(0, index) + ";");
        }
        printAnnotations(commentIndex, t.getAnnotationElements());
        print("class " + typeName.substring(index + 1));
        String superType = t.getSuperType();
        if (superType != null) {
            print(" extends " + superType);
        }
        println(" {");
        indent();
        for (ValueDescriptor v : t.getFields()) {
            printField(commentIndex, v);
        }
        retract();
        println("}");
        println();
    }

    private void printField(int commentIndex, ValueDescriptor v) throws IOException {
        println();
        printAnnotations(commentIndex, v.getAnnotationElements());
        printIndent();
        Type vType = PrivateAccess.getInstance().getType(v);
        if (Type.SUPER_TYPE_SETTING.equals(vType.getSuperType())) {
            print("static ");
        }
        print(makeSimpleType(v.getTypeName()));
        if (v.isArray()) {
            print("[]");
        }
        print(" ");
        print(v.getName());
        print(";");
        printCommentRef(commentIndex, v.getTypeId());
    }

    private void printCommentRef(int commentIndex, long typeId) throws IOException {
        int column = getColumn();
        if (column > commentIndex) {
            print("  ");
        } else {
            while (column < commentIndex) {
                print(" ");
                column++;
            }
        }
        println(" // id=" + typeId);
    }

    private void printAnnotations(int commentIndex, List<AnnotationElement> annotations) throws IOException {
        for (AnnotationElement a : annotations) {
            printIndent();
            print("@");
            print(makeSimpleType(a.getTypeName()));
            List<ValueDescriptor> vs = a.getValueDescriptors();
            if (!vs.isEmpty()) {
                print("(");
                printAnnotation(a);
                print(")");
                printCommentRef(commentIndex, a.getTypeId());
            } else {
                println();
            }
        }
    }

    private void printAnnotation(AnnotationElement a) throws IOException {
        StringJoiner sj = new StringJoiner(", ");
        for (ValueDescriptor v : a.getValueDescriptors()) {
            StringBuilder sb = new StringBuilder();
            Object o = a.getValue(v.getName());
            if (o instanceof String) {
                sb.append("\"");
                sb.append((String) o);
                sb.append("\"");
            } else {
                sb.append(o);
            }
            sj.add(sb.toString());
        }
        print(sj.toString());
    }

    private String makeSimpleType(String typeName) {
        int index = typeName.lastIndexOf(".");
        return typeName.substring(index + 1);
    }

    public void print(RecordedEvent event) throws IOException {
        print(makeSimpleType(event.getEventType().getName()), " ");
        print((RecordedObject) event, "");
    }

    public void print(RecordedObject struct, String postFix) throws IOException {
        println("{");
        indent();
        for (ValueDescriptor v : struct.getFields()) {
            printIndent();
            print(v.getName(), " = ");
            printValue(struct.getValue(v.getName()), "");
        }
        retract();
        printIndent();
        println("}" + postFix);
    }

    private void printArray(Object[] array) throws IOException {
        println("[");
        indent();
        for (int i = 0; i < array.length; i++) {
            printIndent();
            printValue(array[i], i + 1 < array.length ? ", " : "");
        }
        retract();
        printIndent();
        println("]");
    }

    private void printValue(Object value, String postFix) throws IOException {
        if (value == null) {
            println("null" + postFix);
        } else if (value instanceof RecordedObject) {
            print((RecordedObject) value, postFix);
        } else if (value.getClass().isArray()) {
            printArray((Object[]) value);
        } else {
            String text = String.valueOf(value);
            if (value instanceof String) {
                text = "\"" + text + "\"";
            }
            println(text);
        }
    }
}
