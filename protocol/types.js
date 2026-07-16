/**
 * MyiUI WebSocket protocol types (shared contract for Electron).
 * @typedef {'action'|'query'} RequestType
 * @typedef {'hello'|'pong'|'screen'|'window'|'hud'|'island'|'tablist'|'music'|'info'|'player'|'toast'|'chat'|'bg_video'} PushType
 *
 * @typedef {Object} PushMessage
 * @property {PushType} type
 * @property {Record<string, unknown>} data
 *
 * @typedef {Object} RequestMessage
 * @property {string} id
 * @property {RequestType} type
 * @property {string} cmd
 * @property {Record<string, unknown>} [data]
 *
 * @typedef {Object} ResponseMessage
 * @property {string} id
 * @property {boolean} ok
 * @property {Record<string, unknown>} [data]
 *
 * Push types (Mod → Electron):
 * - hello: { modVersion, minecraft, port }
 * - screen: { kind: TITLE|IN_GAME|PAUSE|OTHER|UNKNOWN, hasWorld, screenClass }
 * - window: { x, y, width, height, guiScale, fullscreen }
 * - hud: { health, healthMax, healthPct, absorption, food, hungerPct, saturation, armor, air, selectedSlot, xpLevel, xpProgress, lowHealth, damaged, creative }
 * - island / tablist / music / chat / bg_video / toast
 *
 * Request cmds (Electron → Mod):
 * - OVERLAY_READY, OVERLAY_SUSPEND, QUIT
 * - OPEN_SINGLEPLAYER, OPEN_MULTIPLAYER, OPEN_OPTIONS
 * - GET_PLAYER, GET_WORLDS, GET_SERVERS, GET_OPTIONS, SET_OPTION
 * - JOIN_WORLD, CONNECT_SERVER, SET_BG_VIDEO, UI_FLAGS
 * - ISLAND_*, NE_* (NetEase)
 */

export const WS_URL = 'ws://127.0.0.1:25566';

export const ScreenKind = Object.freeze({
  UNKNOWN: 'UNKNOWN',
  TITLE: 'TITLE',
  IN_GAME: 'IN_GAME',
  PAUSE: 'PAUSE',
  OTHER: 'OTHER',
});
