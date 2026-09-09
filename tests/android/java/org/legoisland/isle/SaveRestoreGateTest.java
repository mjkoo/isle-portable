package org.legoisland.isle;

import java.util.concurrent.CountDownLatch;

public final class SaveRestoreGateTest {
    public static void main(String[] args) throws Exception {
        SaveRestoreGate beforeStart = new SaveRestoreGate();
        beforeStart.abandon();
        assert !beforeStart.begin();
        assert beforeStart.status() == SaveRestoreGate.CLOSED;

        SaveRestoreGate queuedCompletion = new SaveRestoreGate();
        assert queuedCompletion.begin();
        queuedCompletion.workerFinished();
        assert queuedCompletion.status() == SaveRestoreGate.PENDING;
        // The UI result callback has not run when activity destruction begins.
        queuedCompletion.abandon();
        assert queuedCompletion.status() == SaveRestoreGate.CLOSED;
        assert !queuedCompletion.begin();

        SaveRestoreGate duringRecovery = new SaveRestoreGate();
        assert duringRecovery.begin();
        CountDownLatch releaseWorker = new CountDownLatch(1);
        Thread worker = new Thread(() -> {
            try { releaseWorker.await(); }
            catch (InterruptedException e) { throw new AssertionError(e); }
            duringRecovery.workerFinished();
        });
        worker.start();
        duringRecovery.abandon();
        assert duringRecovery.status() == SaveRestoreGate.PENDING;
        releaseWorker.countDown();
        worker.join();
        // No main-thread callback or event-loop drain is needed to close the gate.
        assert duringRecovery.status() == SaveRestoreGate.CLOSED;

        SaveRestoreGate normal = new SaveRestoreGate();
        assert normal.begin();
        assert !normal.begin();
        normal.workerFinished();
        assert normal.status() == SaveRestoreGate.PENDING;
        assert normal.begin(); // An error dialog can retry.
        normal.workerFinished();
        normal.finish(SaveRestoreGate.READY);
        assert normal.status() == SaveRestoreGate.READY;
        assert !normal.begin();
        normal.abandon();
        assert normal.status() == SaveRestoreGate.CLOSED;
        System.out.println("Save restore gate tests passed");
    }
}
