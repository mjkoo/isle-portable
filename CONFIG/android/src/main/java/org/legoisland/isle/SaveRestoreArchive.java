package org.legoisland.isle;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Enumeration;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.CancellationException;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/** Reads an archive without using any entry name as an extraction path. */
final class SaveRestoreArchive {
    interface Cancellation { boolean cancelled(); }
    static final class Contents {
        final Map<String, byte[]> files;
        final int players;
        Contents(Map<String, byte[]> files, int players) { this.files = files; this.players = players; }
    }
    private SaveRestoreArchive() { }
    static void check(Cancellation cancel) {
        if (cancel.cancelled()) throw new CancellationException("Restore cancelled.");
    }

    // Owns the provider stream. This temporary copy is never a confirmed restore request.
    static Contents read(InputStream source, File cache, Cancellation cancel) throws IOException {
        if (source == null) throw new IOException("The selected document could not be opened.");
        File archive = null;
        try (InputStream input = source) {
            archive = File.createTempFile("save-restore-", ".zip", cache);
            try (FileOutputStream output = new FileOutputStream(archive)) {
                byte[] buffer = new byte[16384];
                int total = 0, count;
                while ((count = input.read(buffer)) != -1) {
                    check(cancel);
                    total += count;
                    SaveValidation.require(total <= 20 * 1024 * 1024, "The archive exceeds the 20 MiB input limit.");
                    output.write(buffer, 0, count);
                }
            }
            // Close the provider before reporting validation success, including close errors.
        } catch (IOException | RuntimeException | Error e) {
            if (archive != null) archive.delete();
            throw e;
        }
        try { return readLocal(archive, cancel); }
        finally { archive.delete(); }
    }

    static Contents readLocal(File archive, Cancellation cancel) throws IOException {
        Map<String, byte[]> files = new LinkedHashMap<>();
        int total = 0;
        try (ZipFile zip = new ZipFile(archive)) {
            SaveValidation.require(zip.size() >= 3 && zip.size() <= 11, "Expected a complete save set with at most eleven files.");
            Enumeration<? extends ZipEntry> entries = zip.entries();
            while (entries.hasMoreElements()) {
                check(cancel);
                ZipEntry entry = entries.nextElement();
                String path = entry.getName();
                SaveValidation.require(path.matches("saves/(G[0-8]\\.GS|Players\\.gsi|History\\.gsi)"),
                    "Unsupported archive entry: " + path + ".");
                String name = path.substring(6);
                SaveValidation.require(!files.containsKey(name), "Duplicate save entry.");
                SaveValidation.require(entry.getMethod() == ZipEntry.STORED || entry.getMethod() == ZipEntry.DEFLATED,
                    "Unsupported ZIP compression.");
                SaveValidation.require(entry.getSize() >= 0 && entry.getSize() <= SaveValidation.LIMIT - total,
                    "The save files exceed the 16 MiB limit.");
                CRC32 crc = new CRC32();
                ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                try (InputStream input = zip.getInputStream(entry)) {
                    byte[] buffer = new byte[16384];
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        check(cancel);
                        total += count;
                        SaveValidation.require(total <= SaveValidation.LIMIT, "The save files exceed the 16 MiB limit.");
                        bytes.write(buffer, 0, count);
                        crc.update(buffer, 0, count);
                    }
                }
                SaveValidation.require(bytes.size() == entry.getSize() && crc.getValue() == entry.getCrc(),
                    "The archive is damaged or incomplete.");
                files.put(name, bytes.toByteArray());
            }
        }
        check(cancel);
        return new Contents(files, SaveValidation.validate(files));
    }
}
