public enum QuickStartFeature {
    EAGER_APPCDS("", new String[]{"-XX:+IgnoreAppCDSDirCheck", "-XX:+TraceCDSLoading"}),
    FULL("+eagerappcds", new String[]{"-XX:+IgnoreAppCDSDirCheck", "-XX:+TraceCDSLoading"});
    String quickstartSubOption;
    String[] appendixOption;

    QuickStartFeature(String quickstartSubOption, String[] appendixOption) {
        this.quickstartSubOption = quickstartSubOption;
        this.appendixOption = appendixOption;
    }

    public String getQuickstartSubOption() {
        return quickstartSubOption;
    }

    public String[] getAppendixOption() {
        return appendixOption;
    }
}
