package org.legoisland.isle;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;

/**
 * Holds the system's audio focus while the game is in front, so that starting the game stops
 * whatever else was playing and the game gets told when something needs the sound back.
 *
 * <p>SDL silences the audio device when the activity stops, so this covers everything short of
 * that: a notification, a navigation prompt, a call that is ringing but not yet answered, and any
 * pause that never becomes a stop.
 */
final class AudioFocus {
    // Mirrored by the AudioFocus::Change enum in ISLE/android/audiofocus.h. Native refuses
    // anything else, so adding a case here means adding it there.
    private static final int GAIN = 0, LOSS = 1, LOSS_TRANSIENT = 2, LOSS_TRANSIENT_CAN_DUCK = 3;

    private static native void reportNativeChange(int change);

    private final AudioManager manager;
    private final AudioManager.OnAudioFocusChangeListener listener = this::onFocusChange;
    private final Object request; // AudioFocusRequest on API 26+, null below it.
    private boolean held;

    AudioFocus(Context context) {
        manager = (AudioManager) context.getApplicationContext().getSystemService(Context.AUDIO_SERVICE);
        if (manager != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            request = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build())
                // Left at its default false on purpose: the game would rather be talked over than
                // stopped, since most of what it plays is dialogue.
                .setWillPauseWhenDucked(false)
                .setOnAudioFocusChangeListener(listener)
                .build();
        }
        else {
            request = null;
        }
    }

    /** Called from onStart, and again whenever the window regains focus. */
    void request() {
        if (manager == null || held) {
            return;
        }
        // Branching on SDK_INT rather than on the request being there: the two say the same
        // thing, but only this one tells lint the API 26 call is reachable only above 26.
        int result;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            result = manager.requestAudioFocus((AudioFocusRequest) request);
        }
        else {
            result = requestLegacy();
        }
        held = result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
        if (held) {
            // A request granted after a loss gets no callback of its own, so say so here.
            reportNativeChange(GAIN);
        }
        else {
            // The system refuses focus while the phone is ringing or in a call, and to anything
            // asking under a holder that locked the stack. Say so rather than leaving the game at
            // whatever gain it happened to be at: silent when it was already silent, and not
            // playing over the call when it was not. A refusal comes with no registration, so
            // nothing would ever arrive to undo it - onWindowFocusChanged asks again instead.
            reportNativeChange(LOSS_TRANSIENT);
        }
    }

    /**
     * Called from onStop, where SDL stops mixing. Holding focus past that point would stop another
     * app playing over a game that is no longer making a sound.
     */
    void abandon() {
        if (manager == null || !held) {
            return;
        }
        held = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            manager.abandonAudioFocusRequest((AudioFocusRequest) request);
        }
        else {
            abandonLegacy();
        }
        // Not reported: SDL stops mixing as this returns, and leaving the gain where it is lets
        // the next granted request put it back.
    }

    // The API 21 to 25 route. AudioFocusRequest replaced it at 26, and this project's floor is
    // where SDL3 puts it, so both have to be carried.
    @SuppressWarnings("deprecation")
    private int requestLegacy() {
        return manager.requestAudioFocus(listener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
    }

    @SuppressWarnings("deprecation")
    private void abandonLegacy() {
        manager.abandonAudioFocus(listener);
    }

    private void onFocusChange(int focusChange) {
        switch (focusChange) {
            case AudioManager.AUDIOFOCUS_GAIN:
                held = true;
                reportNativeChange(GAIN);
                break;
            case AudioManager.AUDIOFOCUS_LOSS:
                // The system has taken it for good; a fresh request on the next resume is the only
                // way back.
                held = false;
                reportNativeChange(LOSS);
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                reportNativeChange(LOSS_TRANSIENT);
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                reportNativeChange(LOSS_TRANSIENT_CAN_DUCK);
                break;
            default:
                break;
        }
    }
}
