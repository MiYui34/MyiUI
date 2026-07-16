const { app, BrowserWindow, ipcMain, screen } = require('electron');
const path = require('path');
const WebSocket = require('ws');

const WS_URL = 'ws://127.0.0.1:25566';
const RECONNECT_MS = 1500;

let mainWindow = null;
let ws = null;
let reconnectTimer = null;
let reqSeq = 0;
const pending = new Map();

/** @type {{ kind: string }} */
let screenState = { kind: 'UNKNOWN' };
let mousePassthrough = true;

function createWindow() {
  const display = screen.getPrimaryDisplay();
  const { width, height } = display.bounds;

  mainWindow = new BrowserWindow({
    width,
    height,
    x: display.bounds.x,
    y: display.bounds.y,
    transparent: true,
    frame: false,
    alwaysOnTop: true,
    hasShadow: false,
    resizable: true,
    skipTaskbar: false,
    focusable: true,
    backgroundColor: '#00000000',
    webPreferences: {
      preload: path.join(__dirname, '../preload/preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false,
    },
  });

  mainWindow.setAlwaysOnTop(true, 'screen-saver');
  mainWindow.setVisibleOnAllWorkspaces(true, { visibleOnFullScreen: true });
  setPassthrough(true);

  mainWindow.loadFile(path.join(__dirname, '../renderer/index.html'));

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

function setPassthrough(enabled) {
  mousePassthrough = enabled;
  if (!mainWindow || mainWindow.isDestroyed()) return;
  if (enabled) {
    mainWindow.setIgnoreMouseEvents(true, { forward: true });
  } else {
    mainWindow.setIgnoreMouseEvents(false);
  }
}

function applyScreenKind(kind) {
  screenState.kind = kind || 'UNKNOWN';
  // Menu / intro / pause need clicks; in-game HUD stays click-through
  const interactive = kind === 'TITLE' || kind === 'OTHER' || kind === 'PAUSE' || kind === 'UNKNOWN';
  setPassthrough(!interactive);
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('screen', { kind });
  }
}

function connectWs() {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
    return;
  }
  try {
    ws = new WebSocket(WS_URL);
  } catch (e) {
    scheduleReconnect();
    return;
  }

  ws.on('open', () => {
    console.log('[MyiUI] WS connected');
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('ws-status', { connected: true });
    }
    // Acknowledge overlay so Mod can suppress vanilla UI
    sendAction('OVERLAY_READY').catch(() => {});
  });

  ws.on('message', (buf) => {
    let msg;
    try {
      msg = JSON.parse(buf.toString());
    } catch {
      return;
    }

    if (msg.id && pending.has(msg.id)) {
      const { resolve } = pending.get(msg.id);
      pending.delete(msg.id);
      resolve(msg);
      return;
    }

    if (msg.type === 'window' && msg.data) {
      const { x, y, width, height, fullscreen } = msg.data;
      if (mainWindow && !mainWindow.isDestroyed() && width > 0 && height > 0) {
        if (fullscreen) {
          // Keep covering primary display; exclusive fullscreen may still fight Z-order
          const display = screen.getDisplayNearestPoint({ x: x || 0, y: y || 0 });
          mainWindow.setBounds(display.bounds);
        } else {
          mainWindow.setBounds({ x, y, width, height });
        }
      }
    }

    if (msg.type === 'screen' && msg.data) {
      applyScreenKind(msg.data.kind);
    }

    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('ws-message', msg);
    }
  });

  ws.on('close', () => {
    console.log('[MyiUI] WS closed');
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('ws-status', { connected: false });
    }
    // Fall back to passthrough and let vanilla UI show
    setPassthrough(true);
    scheduleReconnect();
  });

  ws.on('error', (err) => {
    console.warn('[MyiUI] WS error', err.message);
  });
}

function scheduleReconnect() {
  if (reconnectTimer) return;
  reconnectTimer = setTimeout(() => {
    reconnectTimer = null;
    connectWs();
  }, RECONNECT_MS);
}

function sendRaw(obj) {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    return Promise.reject(new Error('not connected'));
  }
  ws.send(JSON.stringify(obj));
  return Promise.resolve();
}

function sendAction(cmd, data = {}) {
  const id = String(++reqSeq);
  const payload = { id, type: 'action', cmd, data };
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pending.delete(id);
      reject(new Error('timeout'));
    }, 8000);
    pending.set(id, {
      resolve: (msg) => {
        clearTimeout(timer);
        resolve(msg);
      },
    });
    sendRaw(payload).catch((e) => {
      clearTimeout(timer);
      pending.delete(id);
      reject(e);
    });
  });
}

ipcMain.handle('myiui:action', async (_e, cmd, data) => sendAction(cmd, data || {}));
ipcMain.handle('myiui:set-passthrough', async (_e, enabled) => {
  setPassthrough(!!enabled);
  return { ok: true };
});
ipcMain.handle('myiui:get-screen', async () => screenState);

app.whenReady().then(() => {
  createWindow();
  connectWs();
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) createWindow();
});

app.on('before-quit', () => {
  try {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ id: 'bye', type: 'action', cmd: 'OVERLAY_SUSPEND', data: {} }));
    }
  } catch {}
});
