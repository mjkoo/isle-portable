package org.legoisland.isle;

/** Coordinates native startup and worker completion without depending on UI callbacks. */
final class SaveRestoreGate {
    static final int PENDING = -1;
    static final int READY = 0;
    static final int CLOSED = 1;

    private boolean running;
    private boolean abandoned;
    private int result = PENDING;

    synchronized boolean begin() {
        if (abandoned || running || result != PENDING) return false;
        running = true;
        return true;
    }
    synchronized void workerFinished() { running = false; }
    synchronized void abandon() { abandoned = true; }
    synchronized boolean isAbandoned() { return abandoned; }
    synchronized void finish(int result) {
        if (running) throw new IllegalStateException("Recovery is still running.");
        this.result = result;
    }
    synchronized int status() {
        return abandoned && !running ? CLOSED : result;
    }
}
