package com.alibaba.util;

import com.alibaba.cds.CDSDumperHelper;

public class CDSDumpHook {

    public static class Info {
        public String originClassListName;
        public String finalClassListName;
        public String jsaName;
        public final String agent;
        public final boolean eager;
        public Info(String cdsOriginClassList,
                    String cdsFinalClassList,
                    String cdsJsa,
                    boolean useEagerAppCDS,
                    String eagerAppCDSAgent) {
            this.originClassListName = cdsOriginClassList;
            this.finalClassListName = cdsFinalClassList;
            this.jsaName = cdsJsa;
            this.eager = useEagerAppCDS;
            this.agent = eagerAppCDSAgent;
        }
    }
    private static Info info;
    public static Info getInfo() { return info; }

    // called by JVM
    private static void initialize(String cdsOriginClassList, String cdsFinalClassList, String cdsJSA, String agent, boolean useEagerAppCDS) {
        info = new Info(cdsOriginClassList, cdsFinalClassList, cdsJSA, useEagerAppCDS, agent);

        CDSDumpHook.setup();
    }

    private static void setup() {
        QuickStart.addDumpHook(() -> {
            try {
                CDSDumperHelper.invokeCDSDumper();
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
    }

}
