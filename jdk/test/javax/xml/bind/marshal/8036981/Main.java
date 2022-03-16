/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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

package testjaxbcontext;

import javax.xml.bind.annotation.*;

/**
 * <p>
 * Java class for main complex type.
 *
 * <p>
 * The following schema fragment specifies the expected content contained within
 * this class.
 *
 * <pre>
 * &lt;complexType name="main">
 *   &lt;complexContent>
 *     &lt;restriction base="{http://www.w3.org/2001/XMLSchema}anyType">
 *       &lt;sequence>
 *         &lt;element name="Root" type="{}Root"/>
 *         &lt;element name="Good" type="{}Good"/>
 *       &lt;/sequence>
 *     &lt;/restriction>
 *   &lt;/complexContent>
 * &lt;/complexType>
 * </pre>
 *
 *
 */
@XmlAccessorType(XmlAccessType.FIELD)
@XmlType(name = "main", propOrder = {
    "root",
    "good"
})
@XmlRootElement
public class Main {

    @XmlElement(name = "Root", required = true)
    protected Root root;
    @XmlElement(name = "Good", required = true)
    protected Good good;

    /**
     * Gets the value of the root property.
     *
     * @return possible object is {@link Root }
     *
     */
    public Root getRoot() {
        return root;
    }

    /**
     * Sets the value of the root property.
     *
     * @param value allowed object is {@link Root }
     *
     */
    public void setRoot(Root value) {
        this.root = value;
    }

    /**
     * Gets the value of the good property.
     *
     * @return possible object is {@link Good }
     *
     */
    public Good getGood() {
        return good;
    }

    /**
     * Sets the value of the good property.
     *
     * @param value allowed object is {@link Good }
     *
     */
    public void setGood(Good value) {
        this.good = value;
    }

}
