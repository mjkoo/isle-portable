package org.legoisland.isle;

public final class AboutTextTest {
    public static void main(String[] args) {
        boolean assertions = false;
        assert assertions = true;
        if (!assertions) throw new AssertionError("Run with java -ea");

        // Spelled out rather than read from AboutText, so a wrong constant cannot pass.
        assert AboutText.UPSTREAM_URL.equals("https://github.com/isledecomp/isle-portable");
        assert AboutText.DECOMPILATION_URL.equals("https://github.com/isledecomp/isle");
        assert AboutText.REPOSITORY_URL.equals("https://github.com/mjkoo/isle-portable");

        // The commit build.gradle appends to the version name picks the source of this build.
        assert AboutText.sourceUrl("0.1.2729+g437b5956").equals("https://github.com/mjkoo/isle-portable/tree/437b5956");
        assert AboutText.sourceUrl("0.1.2729+g437b5956abcdef0123456789abcdef01234567")
            .equals("https://github.com/mjkoo/isle-portable/tree/437b5956abcdef0123456789abcdef01234567");
        // A build that does not know its commit falls back to the repository rather than guessing.
        assert AboutText.sourceUrl("0.1+unknown").equals("https://github.com/mjkoo/isle-portable");
        assert AboutText.sourceUrl("0.1.12").equals("https://github.com/mjkoo/isle-portable");
        assert AboutText.sourceUrl("").equals("https://github.com/mjkoo/isle-portable");
        assert AboutText.sourceUrl(null).equals("https://github.com/mjkoo/isle-portable");
        assert AboutText.sourceUrl("0.1.2729+g437b595").equals("https://github.com/mjkoo/isle-portable/tree/437b595");
        assert AboutText.sourceUrl("0.1.2729+g437b59").equals("https://github.com/mjkoo/isle-portable");
        assert AboutText.sourceUrl("0.1.2729+g437B5956").equals("https://github.com/mjkoo/isle-portable");

        AboutText.Row[] rows = AboutText.rows("0.1.2729+g437b5956");
        String[] titles = {"Version", "Based on isle-portable", "LEGO Island decompilation", "Source code",
            "License", "AI disclosure", "Not affiliated"};
        assert rows.length == titles.length;
        for (int i = 0; i < titles.length; i++) {
            assert rows[i].title.equals(titles[i]) : rows[i].title;
            assert rows[i].summary != null && !rows[i].summary.isEmpty() : rows[i].title;
            assert rows[i].url == null || rows[i].url.startsWith("https://") : rows[i].title;
        }

        assert rows[0].summary.equals("0.1.2729+g437b5956") && rows[0].url == null;
        assert AboutText.rows(null)[0].summary.equals("Unknown");
        assert AboutText.rows("")[0].summary.equals("Unknown");

        // The credit rows name whose work this is and link to it.
        assert rows[1].summary.contains("modified version of isle-portable");
        assert rows[1].summary.contains("isledecomp contributors");
        assert rows[1].url.equals("https://github.com/isledecomp/isle-portable");
        assert rows[2].summary.contains("decompilation") && rows[2].url.equals("https://github.com/isledecomp/isle");
        assert rows[3].url.equals("https://github.com/mjkoo/isle-portable/tree/437b5956");
        assert rows[4].summary.contains("Lesser General Public License, version 3");
        assert rows[4].url.equals("https://github.com/mjkoo/isle-portable/blob/main/LICENSE");
        assert rows[5].summary.contains("Claude Code") && rows[5].summary.contains("AI");
        assert rows[5].url.equals("https://github.com/mjkoo/isle-portable#ai-disclosure");
        assert rows[6].summary.contains("not affiliated with or endorsed by the isledecomp project");
        assert rows[6].summary.contains("trademark of the LEGO Group") && rows[6].url == null;
    }
}
