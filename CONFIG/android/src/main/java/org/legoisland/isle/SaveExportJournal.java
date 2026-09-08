package org.legoisland.isle;

import java.io.IOException;

/** Persistent export bookkeeping. Called only on the export worker. */
final class SaveExportJournal {
    interface Store {
        boolean isActive();
        String destination();
        boolean begin();
        boolean recordDestination(String uri);
        boolean clear();
    }

    private final Store store;

    SaveExportJournal(Store store) { this.store = store; }

    void begin() throws IOException {
        if (!store.begin()) throw new IOException("Could not record export state. Try again.");
    }

    void recordDestination(String uri) throws IOException {
        if (!store.recordDestination(uri)) throw new IOException("Could not record the export destination.");
    }

    String recover(boolean wasBusy) {
        if (!store.isActive() && !wasBusy) return null;
        String message = "Export was interrupted. Reopen the game and its menu to capture saves again.";
        String destination = store.destination();
        // A closed output and its journal cannot be committed atomically. A stale record
        // may refer to a complete export, so recovery must never delete that document.
        if (destination != null) {
            message += " A document was left at " + destination + ". It may be complete or incomplete; inspect it before deleting it.";
        }
        String warning = clear();
        return warning == null ? message : message + warning;
    }

    String clear() {
        return store.clear() ? null
            : " Could not clear export recovery information. On restart, the app may report this export as interrupted;"
                + " it will leave the document in place.";
    }
}
