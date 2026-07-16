const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('myiui', {
  action: (cmd, data) => ipcRenderer.invoke('myiui:action', cmd, data),
  setPassthrough: (enabled) => ipcRenderer.invoke('myiui:set-passthrough', enabled),
  getScreen: () => ipcRenderer.invoke('myiui:get-screen'),
  onMessage: (cb) => {
    const handler = (_e, msg) => cb(msg);
    ipcRenderer.on('ws-message', handler);
    return () => ipcRenderer.removeListener('ws-message', handler);
  },
  onStatus: (cb) => {
    const handler = (_e, status) => cb(status);
    ipcRenderer.on('ws-status', handler);
    return () => ipcRenderer.removeListener('ws-status', handler);
  },
  onScreen: (cb) => {
    const handler = (_e, screen) => cb(screen);
    ipcRenderer.on('screen', handler);
    return () => ipcRenderer.removeListener('screen', handler);
  },
});
