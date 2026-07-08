package com.myiui.agent;

final class ScreenStateMachine {
    private ScreenStateMachine() {}

    static boolean canSkipVanillaTitleRender(boolean menuActive, int screenSeq, int overlayAckSeq) {
        return menuActive && screenSeq > 0 && overlayAckSeq == screenSeq;
    }

    static boolean isAckForCurrentScreen(boolean menuActive, int screenSeq, int ackSeq) {
        return menuActive && screenSeq > 0 && ackSeq == screenSeq;
    }
}
