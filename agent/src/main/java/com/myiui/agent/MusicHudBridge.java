package com.myiui.agent;

import com.myiui.agent.netease.MusicPlayer;

/** Pushes now-playing + waveform state to overlay HUD. */
public final class MusicHudBridge {
    static final int WAVEFORM_BINS = 32;

    private MusicHudBridge() {}

    public static void onClientTick() {
        try {
            MusicSnapshot snap = snapshot();
            SharedState.writeMusicHudState(snap);
        } catch (Throwable t) {
            AgentLog.error("MusicHudBridge.onClientTick failed", t);
        }
    }

    static MusicSnapshot snapshot() {
        MusicSnapshot snap = new MusicSnapshot();
        snap.playing = MusicPlayer.isPlaying();
        snap.paused = MusicPlayer.isPaused();
        if (!snap.playing && !snap.paused) {
            return snap;
        }
        var song = MusicPlayer.currentSong();
        if (song != null) {
            snap.title = song.name != null ? song.name : "";
            snap.artist = song.artist != null ? song.artist : "";
            snap.coverUrl = song.coverUrl != null ? song.coverUrl : "";
        }
        snap.positionMs = MusicPlayer.positionMs();
        snap.durationMs = MusicPlayer.durationMs();
        snap.waveform = MusicPlayer.copyWaveform();
        return snap;
    }

    static final class MusicSnapshot {
        boolean playing;
        boolean paused;
        long positionMs;
        long durationMs;
        String title = "";
        String artist = "";
        String coverUrl = "";
        float[] waveform = new float[WAVEFORM_BINS];
    }
}
