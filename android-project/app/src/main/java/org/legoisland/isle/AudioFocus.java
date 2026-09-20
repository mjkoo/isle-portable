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
    // Whatever AudioManager reported, passed on untranslated. Native reads the AUDIOFOCUS_*
    // numbering itself, so there is no second copy of the mapping here to fall out of step, and a
    // value this class never thought about still reaches somewhere that decides what to do.
    private static native void reportNativeChange(int focusChange);

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
            reportNativeChange(AudioManager.AUDIOFOCUS_GAIN);
        }
        else {
            // The system refuses focus while the phone is ringing or in a call, and to anything
            // asking under a holder that locked the stack. Say so rather than leaving the game at
            // whatever gain it happened to be at: silent when it was already silent, and not
            // playing over the call when it was not. A refusal comes with no registration, so
            // nothing would ever arrive to undo it - onWindowFocusChanged asks again instead.
            reportNativeChange(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);
        }
    }

    /**
     * Called from onStop, where SDL stops mixing. Holding focus past that point would stop another
     * app playing over a game that is no longer making a sound.
     */
    void abandon() {
        if (manager == null) {
            return;
        }
        // Not conditional on holding it. After a permanent loss the framework has already dropped
        // us, but the listener stays registered on the application's AudioManager until this call,
        // and with it this object and anything a later dispatch for that id would reach.
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
        // Only cleared, never set. Callbacks are posted from a binder thread, so a gain the
        // framework sent just before it processed an abandon can arrive after it; setting the flag
        // there would convince the next onStart that focus was already held and skip the request,
        // leaving the game a whole session in the foreground holding none.
        if (focusChange == AudioManager.AUDIOFOCUS_LOSS) {
            // Taken for good. Only a fresh request gets it back, and onStart is where that is.
            held = false;
        }
        reportNativeChange(focusChange);
    }
}
