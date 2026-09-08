package org.legoisland.isle;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.util.Arrays;

public final class SaveExportJournalTest {
    // Like SharedPreferences, failed commits update memory but leave durable state unchanged.
    private static final class Store implements SaveExportJournal.Store {
        boolean active;
        String uri;
        boolean durableActive;
        String durableUri;
        boolean writesSucceed = true;

        @Override public boolean isActive() { return active; }
        @Override public String destination() { return uri; }
        @Override public boolean begin() { active = true; uri = null; return persist(); }
        @Override public boolean recordDestination(String value) { uri = value; return persist(); }
        @Override public boolean clear() { active = false; uri = null; return persist(); }

        private boolean persist() {
            if (!writesSucceed) return false;
            durableActive = active;
            durableUri = uri;
            return true;
        }

        void restart() { active = durableActive; uri = durableUri; }
    }

    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("save-export-journal-test").toFile();
        File destination = new File(root, "export.zip");
        File archive = null;
        try {
            Store store = new Store();
            SaveExportJournal journal = new SaveExportJournal(store);
            check(journal.recover(false) == null, "Fresh journal reported interruption");
            journal.begin();
            journal.recordDestination(destination.toURI().toString());
            archive = SaveArchive.create(root, new String[] {"G0.GS"}, new byte[][] {{1, 2, 3}}, () -> false);
            SaveArchive.transfer(archive, new FileOutputStream(destination), () -> false);
            byte[] exported = Files.readAllBytes(destination.toPath());

            store.writesSucceed = false;
            check(journal.clear() != null, "Failed completion update was not reported");
            check(!store.active && store.durableActive, "Failed commit did not simulate stale disk state");
            store.restart();
            journal = new SaveExportJournal(store);
            String recovery = journal.recover(false);
            check(recovery.contains(destination.toURI().toString()), "Recovery omitted the uncertain destination");
            check(recovery.contains("Could not clear"), "Recovery hid a second persistence failure");
            check(Arrays.equals(exported, Files.readAllBytes(destination.toPath())), "Recovery changed a completed export");

            store.restart();
            store.writesSucceed = true;
            check(journal.recover(false) != null, "Repeated recovery lost the durable record");
            store.restart();
            check(journal.recover(false) == null, "Successful recovery did not clear the journal");
            check(Arrays.equals(exported, Files.readAllBytes(destination.toPath())), "Repeated recovery changed the export");

            journal.begin();
            journal.recordDestination(destination.toURI().toString());
            Files.write(destination.toPath(), new byte[] {1});
            store.restart();
            check(journal.recover(false).contains("incomplete"), "Partial output was not identified as uncertain");
            check(destination.length() == 1, "Recovery removed uncertain partial output");
            check(journal.recover(true) != null, "Saved activity state did not trigger interruption reporting");

            journal.begin();
            journal.recordDestination(destination.toURI().toString());
            check(journal.clear() == null, "Successful completion produced a warning");
            store.restart();
            check(journal.recover(false) == null, "Completed export was reported as interrupted");

            store.writesSucceed = false;
            SaveArchiveTest.fails(java.io.IOException.class, journal::begin);
            SaveArchiveTest.fails(java.io.IOException.class, () -> new SaveExportJournal(store).recordDestination("test"));
        } finally {
            if (archive != null) Files.deleteIfExists(archive.toPath());
            Files.deleteIfExists(destination.toPath());
            Files.delete(root.toPath());
        }
        System.out.println("Save export journal persistence and conservative recovery tests passed.");
    }
}
