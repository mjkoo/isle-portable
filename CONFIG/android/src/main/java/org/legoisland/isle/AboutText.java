package org.legoisland.isle;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * What Settings > About says: which build this is, whose work the game port is, where this
 * build's source is, its license, and how this fork was written. Plain Java so it can be tested
 * without a device.
 */
final class AboutText {
    static final String UPSTREAM_URL = "https://github.com/isledecomp/isle-portable";
    static final String DECOMPILATION_URL = "https://github.com/isledecomp/isle";
    static final String REPOSITORY_URL = "https://github.com/mjkoo/isle-portable";
    static final String LICENSE_URL = REPOSITORY_URL + "/blob/main/LICENSE";
    static final String AI_DISCLOSURE_URL = REPOSITORY_URL + "#ai-disclosure";

    /** The commit a version name ends with, as build.gradle writes it: 0.1.2729+g437b5956. */
    private static final Pattern COMMIT = Pattern.compile("\\+g([0-9a-f]{7,40})$");

    /** One row. A row with no URL is shown but cannot be chosen. */
    static final class Row {
        final String title;
        final String summary;
        final String url;

        Row(String title, String summary, String url) {
            this.title = title;
            this.summary = summary;
            this.url = url;
        }
    }

    private AboutText() {}

    /** The source this build was made from, when its version names a commit; the repository otherwise. */
    static String sourceUrl(String versionName) {
        Matcher commit = versionName == null ? null : COMMIT.matcher(versionName);
        return commit != null && commit.find() ? REPOSITORY_URL + "/tree/" + commit.group(1) : REPOSITORY_URL;
    }

    static Row[] rows(String versionName) {
        boolean known = versionName != null && !versionName.isEmpty();
        return new Row[] {
            new Row("Version", known ? versionName : "Unknown", null),
            new Row("Based on isle-portable",
                "This is a modified version of isle-portable, the portable LEGO Island by the isledecomp "
                    + "contributors. The game port is their work.",
                UPSTREAM_URL),
            new Row("LEGO Island decompilation",
                "isle-portable is built on the isledecomp project's decompilation of the original game.",
                DECOMPILATION_URL),
            new Row("Source code",
                "This Android-focused fork. Opens the source this build was made from.",
                sourceUrl(versionName)),
            new Row("License", "GNU Lesser General Public License, version 3", LICENSE_URL),
            new Row("AI disclosure",
                "Most of this fork's changes were written with Claude Code, an AI coding assistant, "
                    + "and tested mainly on emulators.",
                AI_DISCLOSURE_URL),
            new Row("Not affiliated",
                "This fork is not affiliated with or endorsed by the isledecomp project. LEGO® is a "
                    + "trademark of the LEGO Group, which does not sponsor, authorize or endorse this project.",
                null),
        };
    }
}
