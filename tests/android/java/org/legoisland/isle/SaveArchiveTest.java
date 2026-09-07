package org.legoisland.isle;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.concurrent.CancellationException;
import java.util.concurrent.atomic.AtomicBoolean;

public final class SaveArchiveTest {
    interface Operation { void run() throws Exception; }
    static void fails(Class<? extends Throwable> expected, Operation operation) throws Exception {
        try { operation.run(); }
        catch (Throwable error) {
            if (expected.isInstance(error)) return;
            throw error;
        }
        throw new AssertionError("Expected " + expected.getName());
    }

    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("save-archive-test").toFile();
        String[] names = {"G0.GS", "Players.gsi", "History.gsi"};
        byte[][] data = {new byte[] {0, 1, -1, 9}, new byte[0], new byte[48000]};
        File archive = SaveArchive.create(root, names, data, () -> false);
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        SaveArchive.transfer(archive, output, () -> false);
        if (!Arrays.equals(Files.readAllBytes(archive.toPath()), output.toByteArray())) throw new AssertionError();
        data[0][0] = 5;
        fails(IOException.class, () -> SaveArchive.verify(archive, names, data, () -> false));
        fails(IOException.class, () -> SaveArchive.create(root, new String[] {"../isle.ini"}, new byte[][] {{1}}, () -> false));
        fails(IOException.class, () -> SaveArchive.create(root, new String[] {"G0.GS", "G0.GS"}, new byte[][] {{1}, {2}}, () -> false));
        fails(IOException.class, () -> SaveArchive.create(root, new String[] {"G0.GS"}, new byte[][] {new byte[16 * 1024 * 1024 + 1]}, () -> false));
        fails(CancellationException.class, () -> SaveArchive.create(root, names, data, () -> true));
        if (root.list().length != 1) throw new AssertionError("Cancelled preparation left a file");
        AtomicBoolean closed = new AtomicBoolean();
        fails(IOException.class, () -> SaveArchive.transfer(archive, new OutputStream() {
            @Override public void write(int value) throws IOException { throw new IOException("Full destination"); }
            @Override public void close() { closed.set(true); }
        }, () -> false));
        if (!closed.get()) throw new AssertionError("Failed write did not close destination");
        fails(IOException.class, () -> SaveArchive.transfer(archive, new ByteArrayOutputStream() {
            @Override public void close() throws IOException { throw new IOException("Failed close"); }
        }, () -> false));
        closed.set(false);
        fails(CancellationException.class, () -> SaveArchive.transfer(archive, new ByteArrayOutputStream() {
            @Override public void close() { closed.set(true); }
        }, () -> true));
        if (!closed.get()) throw new AssertionError("Cancellation did not close destination");
        archive.delete();
        root.delete();
        System.out.println("Save archive creation, verification, cancellation and stream-failure tests passed.");
    }
}
