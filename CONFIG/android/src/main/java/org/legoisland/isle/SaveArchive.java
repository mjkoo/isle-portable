package org.legoisland.isle;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashSet;
import java.util.concurrent.CancellationException;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipOutputStream;

/** Creates and checks a portable archive from immutable bytes, never from live saves. */
final class SaveArchive {
    interface Cancellation { boolean isCancelled(); }

    private SaveArchive() { }

    static void checkCancelled(Cancellation cancelled) {
        if (cancelled.isCancelled()) throw new CancellationException("Export cancelled.");
    }

    static File create(File cache, String[] names, byte[][] contents, Cancellation cancelled) throws IOException {
        if (contents == null || names.length == 0 || names.length != contents.length || names.length > 11) {
            throw new IOException("The save snapshot is unavailable. Reopen the game menu and try again.");
        }
        long total = 0;
        HashSet<String> seen = new HashSet<>();
        for (int i = 0; i < names.length; i++) {
            if (!names[i].matches("G[0-8]\\.GS|Players\\.gsi|History\\.gsi") || !seen.add(names[i]) || contents[i] == null) {
                throw new IOException("Invalid save snapshot.");
            }
            total += contents[i].length;
        }
        if (total > 16 * 1024 * 1024) throw new IOException("The save files exceed the 16 MiB export limit.");
        File archive = File.createTempFile("save-export-", ".zip", cache);
        boolean complete = false;
        try {
            try (ZipOutputStream output = new ZipOutputStream(new FileOutputStream(archive))) {
                for (int i = 0; i < names.length; i++) {
                    checkCancelled(cancelled);
                    output.putNextEntry(new ZipEntry("saves/" + names[i]));
                    for (int offset = 0; offset < contents[i].length; offset += 16384) {
                        checkCancelled(cancelled);
                        output.write(contents[i], offset, Math.min(16384, contents[i].length - offset));
                    }
                    output.closeEntry();
                }
            }
            verify(archive, names, contents, cancelled);
            complete = true;
            return archive;
        } finally {
            if (!complete) archive.delete();
        }
    }

    static void verify(File archive, String[] names, byte[][] contents, Cancellation cancelled) throws IOException {
        try (ZipFile zip = new ZipFile(archive)) {
            if (zip.size() != names.length) throw new IOException("The export archive is incomplete.");
            byte[] buffer = new byte[16384];
            for (int i = 0; i < names.length; i++) {
                ZipEntry entry = zip.getEntry("saves/" + names[i]);
                if (entry == null || entry.getSize() != contents[i].length) throw new IOException("The export archive is incomplete.");
                CRC32 expected = new CRC32();
                expected.update(contents[i]);
                CRC32 actual = new CRC32();
                long count = 0;
                try (InputStream input = zip.getInputStream(entry)) {
                    int read;
                    while ((read = input.read(buffer)) != -1) {
                        checkCancelled(cancelled);
                        count += read;
                        if (count > contents[i].length) throw new IOException("The export archive failed verification.");
                        actual.update(buffer, 0, read);
                    }
                }
                if (count != contents[i].length || actual.getValue() != expected.getValue() || entry.getCrc() != expected.getValue()) {
                    throw new IOException("The export archive failed verification.");
                }
            }
        }
    }

    // Owns and closes the destination, including on cancellation or a read failure.
    static void transfer(File archive, OutputStream destination, Cancellation cancelled) throws IOException {
        if (destination == null) throw new IOException("The destination could not be opened.");
        try (OutputStream output = destination; InputStream input = new FileInputStream(archive)) {
            byte[] buffer = new byte[16384];
            int count;
            while ((count = input.read(buffer)) != -1) {
                checkCancelled(cancelled);
                output.write(buffer, 0, count);
            }
            output.flush();
            checkCancelled(cancelled);
        }
    }
}
