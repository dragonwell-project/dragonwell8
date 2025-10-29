/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @bug 8348760
 * @summary Verify if RadioButtonMenuItem bullet and
 *          JCheckboxMenuItem checkmark is shown if
 *          JRadioButtonMenuItem and JCheckboxMenuItem
 *          is rendered with ImageIcon in WindowsLookAndFeel
 * @requires (os.family == "windows")
 * @run main/manual TestRadioAndCheckMenuItemWithIcon
 */

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.FlowLayout;
import java.awt.Graphics;
import java.awt.GridLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.image.BufferedImage;
import java.util.concurrent.LinkedBlockingQueue;

import javax.swing.AbstractButton;
import javax.swing.ButtonGroup;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JCheckBoxMenuItem;
import javax.swing.JFrame;
import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JPanel;
import javax.swing.JRadioButtonMenuItem;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.KeyStroke;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;

public class TestRadioAndCheckMenuItemWithIcon {

    private static final String INSTRUCTIONS =
         "Clicking on the Menu above will show a\n"
         + "JRadioButtonMenuItem group with 3 radiobutton menuitems\n"
         + "and a JCheckBoxMenuItem group with 3 checkbox menuitems.\n"
         + "\n"
         + "First radiobutton menuitem is selected with imageicon of a red square.\n"
         + "Second radiobutton menuitem is unselected with imageicon.\n"
         + "Third radiobutton menuItem is unselected without imageicon.\n"
         + "\n"
         + "First checkbox menuitem is selected with imageicon.\n"
         + "Second checkbox menuitem is unselected with imageicon.\n"
         + "Third checkbox menuItem is unselected without imageicon.\n"
         + "\n"
         + "Verify that for first JRadioButtonMenuItem with imageicon,\n"
         + "a bullet is shown alongside the imageicon and\n"
         + "for first JCheckBoxMenuItem with imageicon\n"
         + "a checkmark is shown alongside the imageicon.\n"
         + "\n"
         + "If bullet and checkmark is shown, test passes else fails.";

    private static final LinkedBlockingQueue<Boolean> resultQueue = new LinkedBlockingQueue<Boolean>(1);

    private static void endTest(boolean result) {
        SwingUtilities.invokeLater(
                new Runnable() {
                    public void run() {
                        try {
                            resultQueue.put(result);
                        } catch (Exception ignored) {
                        }
                    }
                });
    }

    public static void main(String[] args) throws Exception {
        UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());

        SwingUtilities.invokeLater(new Runnable() {
            public void run() {
                TestRadioAndCheckMenuItemWithIcon.doTest();
            }
        });

        Boolean testPasses = resultQueue.take();

        if (!testPasses) {
            throw new Exception("Test failed!");
        }
    }

    public static void doTest() {
        BufferedImage img = new BufferedImage(16, 16, BufferedImage.TYPE_INT_ARGB);
        Graphics g = img.getGraphics();
        g.setColor(Color.red);
        g.fillRect(0, 0, img.getWidth(), img.getHeight());
        g.dispose();

        BufferedImage img2 = new BufferedImage(64, 64, BufferedImage.TYPE_INT_ARGB);
        Graphics g2 = img2.getGraphics();
        g2.setColor(Color.red);
        g2.fillRect(0, 0, img2.getWidth(), img2.getHeight());
        g2.dispose();

        JFrame frame = new JFrame("RadioButtonWithImageIcon");
        ImageIcon imageIcon1 = new ImageIcon(img);
        ImageIcon imageIcon2 = new ImageIcon(img2);
        AbstractButton button1;
        JRadioButtonMenuItem m1 = new JRadioButtonMenuItem("JRadioButtonMenuItem 1",
                imageIcon1);
        m1.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_F4, ActionEvent.ALT_MASK|ActionEvent.CTRL_MASK|ActionEvent.SHIFT_MASK));
        button1 = m1;
        button1.setSelected(true);
        AbstractButton button2 = new JRadioButtonMenuItem("JRadioButtonMenuItem 2", imageIcon2);
        AbstractButton button3 = new JRadioButtonMenuItem("JRadioButtonMenuItem 3");

        ButtonGroup buttonGroup = new ButtonGroup();
        buttonGroup.add(button1);
        buttonGroup.add(button2);
        buttonGroup.add(button3);

        AbstractButton check1 = new JCheckBoxMenuItem("JCheckBoxMenuItem 1",
                imageIcon1);
        check1.setSelected(true);
        AbstractButton check2 = new JCheckBoxMenuItem("JCheckBoxMenuItem 2", imageIcon1);
        JCheckBoxMenuItem c3;
        AbstractButton check3 = c3 = new JCheckBoxMenuItem("JCheckBoxMenuItem 3");
        c3.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_F5, ActionEvent.ALT_MASK|ActionEvent.CTRL_MASK|ActionEvent.SHIFT_MASK));

        JMenu topLevel = new JMenu("Menu");

        topLevel.add(button1);
        topLevel.add(button2);
        topLevel.add(button3);

        topLevel.addSeparator();

        topLevel.add(check1);
        topLevel.add(check2);
        topLevel.add(check3);

        AbstractButton menuitem1 = new JMenuItem("MenuItem1");
        AbstractButton menuitem2 = new JMenuItem("MenuItem2", imageIcon1);
        topLevel.addSeparator();
        topLevel.add(menuitem1);
        topLevel.add(menuitem2);

        JMenuBar menuBar = new JMenuBar();
        menuBar.add(topLevel);

        frame.setJMenuBar(menuBar);

        JTextArea instructions = new JTextArea();
        instructions.setText(INSTRUCTIONS);
        instructions.setEditable(false);
        instructions.setColumns(80);
        instructions.setRows(24);

        JScrollPane scrInstructions = new JScrollPane(instructions);
        frame.getContentPane().setLayout(new BorderLayout());

        frame.getContentPane().add(scrInstructions, BorderLayout.CENTER);

        JPanel yesno = new JPanel();
        yesno.setLayout(new GridLayout(1, 2, 40, 40));

        JButton yes = new JButton("Passes");
        JButton no = new JButton("Fails");
        yesno.add(yes);
        yesno.add(no);

        yes.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                frame.dispose();
                endTest(true);
            }
        });

        no.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                frame.dispose();
                endTest(false);
            }
        });

        frame.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
        frame.addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosed(WindowEvent e) {
                frame.dispose();
                endTest(false);
            }
        });

        frame.getContentPane().add(yesno, BorderLayout.SOUTH);

        frame.pack();
        frame.setLocationRelativeTo(null);
        frame.setVisible(true);
    }
}
