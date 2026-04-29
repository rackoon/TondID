
/**
  _____  ___   __ __  ____  ___    ____   ____  ___
 / ___/ /   \ |  |  ||    ||   \  |    \ |    ||   \
(   \_ |     ||  |  | |  | |    \ |  D  ) |  | |    \
 \__  ||  Q  ||  |  | |  | |  D  ||    /  |  | |  D  |
 /  \ ||     ||  :  | |  | |     ||    \  |  | |     |
 \    ||     ||     | |  | |     ||  .  \ |  | |     |
  \___| \__,_| \__,_||____||_____||__|\_||____||_____|

 *
 * This file is part of TondID
 *
 * Copyright (c) 2023 FLY&I (flyandi.net)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/
import { useEffect, useRef, useState } from "react";
import './App.css';
import 'leaflet/dist/leaflet.css';
import { MapContainer, TileLayer, useMapEvents, Circle, Polyline, FeatureGroup, Popup } from 'react-leaflet'
import Control from "react-leaflet-custom-control";
import { Dropdown, toCombinedPath, fromPath, toPath, inflatePath, deflatePath } from "./common";
import { idtype_t, uatype_t, external_t, ghost_id_scheme_t, ghost_flight_mode_t, ghost_altitude_mode_t } from "./squid";


class BrowserSerialCompat {
  constructor(serialOptions = {}) {
    this.serialOptions = {
      baudRate: 115200,
      dataBits: 8,
      stopBits: 1,
      parity: "none",
      bufferSize: 255,
      flowControl: "none",
      ...serialOptions,
    };
    this.port = null;
    this.reader = null;
    this.keepReading = false;
    this.readBuffer = "";
    this.decoder = new TextDecoder("utf-8");
    this.encoder = new TextEncoder();
  }

  async connect() {
    if (!navigator.serial) {
      throw new Error("WebSerial is not available in this browser.");
    }
    this.port = await navigator.serial.requestPort();
    await this.port.open(this.serialOptions);
    this.keepReading = true;
    this.readBuffer = "";
  }

  async disconnect() {
    this.keepReading = false;
    if (this.reader) {
      try {
        await this.reader.cancel();
      } catch (e) {
        console.log("[serial] reader cancel", e);
      }
      try {
        this.reader.releaseLock();
      } catch (e) {
        console.log("[serial] reader release", e);
      }
      this.reader = null;
    }
    if (this.port) {
      try {
        await this.port.close();
      } catch (e) {
        console.log("[serial] port close", e);
      }
      this.port = null;
    }
  }

  async write(cmd) {
    if (!this.port?.writable) {
      throw new Error("Serial port is not writable.");
    }
    const writer = this.port.writable.getWriter();
    try {
      await writer.write(this.encoder.encode(cmd + "\r\n"));
    } finally {
      writer.releaseLock();
    }
  }

  async readLoop(callable) {
    if (!this.port?.readable) {
      return;
    }
    this.reader = this.port.readable.getReader();
    try {
      while (this.keepReading) {
        const { value, done } = await this.reader.read();
        if (done) {
          break;
        }
        if (!value) {
          continue;
        }
        this.readBuffer += this.decoder.decode(value, { stream: true });
        const lines = this.readBuffer.split(/\r?\n/);
        this.readBuffer = lines.pop() || "";
        for (const line of lines) {
          if (callable(line, false) === true) {
            this.keepReading = false;
            break;
          }
        }
      }
      const tail = this.readBuffer + this.decoder.decode();
      if (tail.trim().length) {
        callable(tail.trim(), false);
      }
    } finally {
      try {
        this.reader.releaseLock();
      } catch (e) {
        console.log("[serial] reader release", e);
      }
      this.reader = null;
    }
  }
}

const serial = new BrowserSerialCompat();

const Commands = {
  version: "V",
  data: "D",
  reboot: "R",
  current: "C",
  store_data: "SD",
  get_slot_data: "GS",
  store_slot_data: "SS",
  store_mode: "SM",
  path: "P",
}

const MapMode = {
  Pan: 0,
  Path: 1,
  Origin: 2,
  Operator: 3,
}

const MapPestMode = {
  Pan: 0,
  Origin: 4,
}

const MapExternalMode = {
  Pan: 0,
  Operator: 3,
}

const AppMode = {
  Squid: 0,
  Pest: 1,
  External: 2,
}

const FlyMode = {
  Pause: 0,
  Fly: 1,
}

const PathMode = {
  Hold: 0,
  Random: 1,
  Follow: 2,
}

const RadiusMode = [500, 1000, 2000, 3000, 5000, 10000];
const MinRadius = 500;
const InitialPosition = { lat: 59.437, lng: 24.7536, op_lat: 59.437, op_lng: 24.7536, alt: 120, op_alt: 0, spd: 25, hdg: 0, sats: 10 };
const MaxTotalUas = 5;
const MaxGhostUas = MaxTotalUas - 1;
const InitialPest = { pe_lat: 59.437, pe_lng: 24.7536, pe_radius: MinRadius, pe_spawn: MaxTotalUas, ghost_id_scheme: 0, ghost_id_prefix: "1596GHOSTPREFIX000", ghost_flight_mode: 2, ghost_altitude_mode: 1 };
const InitialSlot = { mac: "", rid: "", operator: "", description: "", manufacturer: "", model: "", uatype: 2, idtype: 1 };
const SlotSharedFields = ["operator", "manufacturer", "model", "uatype", "idtype"];
const InitialSharedLocks = { operator: true, manufacturer: true, model: true, uatype: true, idtype: true };
const SlotFieldLabels = {
  operator: "Operator",
  manufacturer: "Manufacturer",
  model: "Model",
  uatype: "UA Type",
  idtype: "ID Type",
};


const Button = ({ name, secondary, selected, disabled, onPress }) => {
  const classes = ["app-button"];
  if (selected) classes.push("selected");
  if (secondary) classes.push("secondary");
  return (
    <button className={classes.join(" ")} disabled={disabled} onClick={onPress}>{name}</button>
  )
};

const Display = ({ condition, children }) => {
  if (!condition) return;
  return children;
}

const RemoteControlButton = ({ name, selected = false, steps, onChange }) => {
  const handle = value => () => onChange(value);
  return (
    <div className="remote-control">
      <button className="text" disabled>{name}</button>
      {(steps || []).map(step =>
        <Button selected={selected === step} key={step} name={step} onPress={handle(step)} />
      )}
    </div>
  )
}

const F = (n, p = 6) => {
  const roundedNumber = parseFloat(n || 0).toFixed(p);
  const paddedNumber = roundedNumber.padEnd(roundedNumber.indexOf('.') + p + 1, '0');
  return Number(paddedNumber);
};
const I = (n) => Number(parseInt(n) || 0);
const S = (s) => ((s || s === 0) && s.length ? s : " ").toUpperCase();
const hasZeroOrigin = (p) => F(p.lat) === 0 && F(p.lng) === 0;
const randomHex = (len) => Array.from({ length: len }, () => "0123456789ABCDEF"[Math.floor(Math.random() * 16)]).join("");
const padHex = (n) => n.toString(16).toUpperCase().padStart(2, "0");
const normalizeMac = (value = "") => value.toUpperCase().replace(/[^0-9A-F]/g, "").slice(0, 12).match(/.{1,2}/g)?.join(":") || "";
const isValidMac = (value = "") => /^[0-9A-F]{2}(:[0-9A-F]{2}){5}$/.test((value || "").toUpperCase());
const isValidRid = (value = "") => /^[A-Z0-9._ -]{1,20}$/.test((value || "").toUpperCase());
const randomLocalMac = () => {
  const parts = Array.from({ length: 6 }, () => Math.floor(Math.random() * 256));
  parts[0] = (parts[0] | 0x02) & 0xFE;
  return parts.map(padHex).join(":");
};
const defaultMacForSlot = (base, slot) => {
  const clean = (base || "").replace(/[^0-9A-F]/gi, "").toUpperCase();
  if (clean.length !== 12) {
    return "";
  }
  const parts = clean.match(/.{1,2}/g).map(part => parseInt(part, 16));
  parts[0] = (parts[0] | 0x02) & 0xFE;
  parts[5] = (parts[5] + slot) & 0xFF;
  return parts.map(padHex).join(":");
};
const makePrimarySlot = (state, baseMac = "") => ({
  ...InitialSlot,
  mac: normalizeMac(state.mac || baseMac),
  rid: state.rid || "",
  operator: state.operator || "",
  description: state.description || "",
  manufacturer: state.manufacturer || "",
  model: state.model || "",
  uatype: I(state.uatype) || 2,
  idtype: I(state.idtype) || 1,
});
const emptySlotList = () => Array.from({ length: MaxTotalUas }, (_, idx) => ({ ...InitialSlot, index: idx }));
const emptySlotErrors = () => Array.from({ length: MaxTotalUas }, () => []);


const LocationMarker = ({ onEvent }) => {
  useMapEvents({
    click(e) {
      onEvent(e);
    },
    locationfound(e) {
      onEvent(e);
    },
  })
  return null;
}



function App() {

  const [connected, setConnected] = useState(false);
  const [position, setPosition] = useState(InitialPosition);
  const [pest, setPest] = useState({});
  const [data, setData] = useState(InitialPest);
  const [slots, setSlots] = useState(emptySlotList());
  const [slotErrors, setSlotErrors] = useState(emptySlotErrors());
  const [slotClipboard, setSlotClipboard] = useState(null);
  const [sharedLocks, setSharedLocks] = useState(InitialSharedLocks);
  const [dataUpdated, setDataUpdated] = useState(false);
  const [map, setMap] = useState(null)
  const [mapMode, setMapMode] = useState(MapMode.Pan);
  const [flyMode, setFlyMode] = useState(FlyMode.Pause);
  const [pathMode, setPathMode] = useState(PathMode.Hold);
  const [zoom] = useState(16);
  const [lock, setLock] = useState(false);
  const [appMode, setAppMode] = useState(AppMode.Squid);
  const [trail, setTrail] = useState([]);
  const [path, setPath] = useState([]);
  const [showProfile, setShowProfile] = useState(false);
  const [showPath, setShowPath] = useState(false);
  const [error, setError] = useState(false);
  const [first, setFirst] = useState(true);
  const [geoInitialized, setGeoInitialized] = useState(false);
  const modeRef = useRef({ appMode: AppMode.Squid, flyMode: FlyMode.Pause, pathMode: PathMode.Hold });
  const serialQueueRef = useRef(Promise.resolve());
  const editStateRef = useRef({ showProfile: false, showPath: false, dataUpdated: false });
  const [debug, setDebug] = useState({
    lastLine: "",
    lastType: "",
    deviceAppMode: AppMode.Squid,
    deviceFlyMode: FlyMode.Pause,
    devicePathMode: PathMode.Hold,
  });

  const appModeLabel = value => Object.keys(AppMode).find(key => AppMode[key] === value) || `?${value}`;
  const flyModeLabel = value => Object.keys(FlyMode).find(key => FlyMode[key] === value) || `?${value}`;
  const pathModeLabel = value => Object.keys(PathMode).find(key => PathMode[key] === value) || `?${value}`;

  const serialCommand = (command, args = []) => {
    if (connected) {
      const cmd = ["$", command].join("") + (args.length ? "|" + args.join("|") : "");
      console.log("[serial]", "Writing", cmd);
      serialQueueRef.current = serialQueueRef.current
        .catch(() => undefined)
        .then(() => serial.write(cmd));
    }
  }

  const handleSync = (o = true) => {
    if (!o && (editStateRef.current.showProfile || editStateRef.current.showPath)) {
      return;
    }
    if (o && dataUpdated) {
      handleDataUpdate();
    } else {
      serialCommand(Commands.data);
      serialCommand(Commands.current);
      requestSlotSync();
    }
  }

  const handleSerialConnect = () => {
    setError(false);
    if (connected) {
      serial.disconnect().then(() => setConnected(false)).catch(e => {
        console.log(e);
        setError(e.toString());
        setConnected(false);
      });
    } else {
      serial.connect().then(() => {
        setPest({});
        setTrail([]);
        setSlots(emptySlotList());
        setSlotErrors(emptySlotErrors());
        setDebug({
          lastLine: "",
          lastType: "",
          deviceAppMode: AppMode.Squid,
          deviceFlyMode: FlyMode.Pause,
          devicePathMode: PathMode.Hold,
        });
        setConnected(true);
      }).catch(e => {
        console.log(e);
        setError(e.toString());
        setConnected(false);
      });
    }
  }

  const handleData = name => event => {
    setDataUpdated(true);
    setError(false);
    const value = event.target.value;
    setData({ ...data, [name]: value });
    if (SlotSharedFields.includes(name) && sharedLocks[name]) {
      setSlots(prev => prev.map(slot => ({ ...slot, [name]: value })));
    } else if (name === "rid") {
      setSlots(prev => prev.map((slot, idx) => idx === 0 ? { ...slot, rid: value } : slot));
    } else if (name === "description") {
      setSlots(prev => prev.map((slot, idx) => idx === 0 ? { ...slot, description: value } : slot));
    } else if (name === "mac") {
      const normalized = normalizeMac(value);
      setSlots(prev => prev.map((slot, idx) => idx === 0 ? { ...slot, mac: normalized } : slot));
    }
  }

  const handleGhostCount = event => {
    const ghosts = Math.min(MaxGhostUas, Math.max(0, Number.parseInt(event.target.value || "0", 10) || 0));
    setDataUpdated(true);
    setData({ ...data, pe_spawn: ghosts + 1 });
  }

  const requestSlotSync = () => {
    for (let slot = 0; slot < MaxTotalUas; slot++) {
      serialCommand(Commands.get_slot_data, [slot]);
    }
  };

  const syncLockedFields = (slotList) => {
    const primary = slotList[0] || InitialSlot;
    return slotList.map((slot, idx) => {
      if (idx === 0) {
        return slot;
      }
      const next = { ...slot };
      SlotSharedFields.forEach(field => {
        if (sharedLocks[field]) {
          next[field] = primary[field];
        }
      });
      return next;
    });
  };

  const handleSlotData = (slotIndex, key) => event => {
    const value = key === "mac" ? normalizeMac(event.target.value) : event.target.value;
    setDataUpdated(true);
    setError(false);
    setSlotErrors(prev => prev.map((entry, idx) => idx === slotIndex ? [] : entry));
    setSlots(prev => {
      const updated = prev.map((slot, idx) => idx === slotIndex ? { ...slot, [key]: value } : slot);
      if (slotIndex === 0 && SlotSharedFields.includes(key) && sharedLocks[key]) {
        return updated.map((slot, idx) => idx === 0 ? slot : { ...slot, [key]: value });
      }
      return updated;
    });
  };

  const handleClonePrimaryToSlots = () => {
    const primary = makePrimarySlot(data, data.mac);
    setDataUpdated(true);
    setSlots(prev => syncLockedFields(prev.map((slot, idx) => ({
      ...slot,
      mac: idx === 0 ? primary.mac : defaultMacForSlot(primary.mac || data.mac || "", idx),
      rid: idx === 0 ? primary.rid : `${primary.rid.slice(0, 17)}${String(idx).padStart(3, "0")}`.slice(0, 20),
      operator: primary.operator,
      description: idx === 0 ? primary.description : `${primary.description.slice(0, 17)} G${idx}`.slice(0, 20),
      manufacturer: primary.manufacturer,
      model: primary.model,
      uatype: primary.uatype,
      idtype: primary.idtype,
    }))));
  };

  const handleAutoMacs = () => {
    const baseMac = normalizeMac(slots[0]?.mac || data.mac || randomLocalMac());
    setDataUpdated(true);
    setData({ ...data, mac: baseMac });
    setSlots(prev => prev.map((slot, idx) => ({
      ...slot,
      mac: idx === 0 ? baseMac : defaultMacForSlot(baseMac, idx),
    })));
  };

  const handleRandomizeSlots = () => {
    const profiles = ["GUARD", "TRACK", "WATCH", "GRID", "SHIELD"];
    setDataUpdated(true);
    setSlots(prev => syncLockedFields(prev.map((slot, idx) => {
      const rid = `1596${randomHex(13)}`.slice(0, 20);
      const label = profiles[idx % profiles.length];
      return {
        ...slot,
        mac: defaultMacForSlot(slot.mac || data.mac || "", idx) || slot.mac,
        rid,
        operator: `EST${randomHex(12)}`.slice(0, 20),
        description: `${label} ${idx}`.slice(0, 20),
        manufacturer: idx === 0 ? (data.manufacturer || "TONDID") : `TONDID`,
        model: idx === 0 ? (data.model || "NODE-0") : `NODE-${idx}`,
        uatype: slot.uatype || 2,
        idtype: slot.idtype || 1,
      };
    })));
  };

  const handleCopySlot = slotIndex => () => {
    setSlotClipboard({ ...slots[slotIndex] });
  };

  const handlePasteSlot = slotIndex => () => {
    if (!slotClipboard) {
      return;
    }
    setDataUpdated(true);
    setSlots(prev => syncLockedFields(prev.map((slot, idx) => idx === slotIndex ? { ...slot, ...slotClipboard } : slot)));
  };

  const handleSharedLock = key => () => {
    setDataUpdated(true);
    setSharedLocks(prev => {
      const next = { ...prev, [key]: !prev[key] };
      if (next[key]) {
        setSlots(current => current.map((slot, idx) => idx === 0 ? slot : { ...slot, [key]: current[0]?.[key] ?? slot[key] }));
      }
      return next;
    });
  };

  const validateSlots = (slotList, activeSlots) => {
    const errorsBySlot = emptySlotErrors();
    const activeMacs = new Map();
    const activeRids = new Map();
    slotList.forEach((slot, idx) => {
      if (idx >= activeSlots) {
        return;
      }
      const mac = normalizeMac(slot.mac || "");
      const rid = (slot.rid || "").toUpperCase().trim();

      if (!mac || !isValidMac(mac)) {
        errorsBySlot[idx].push("MAC peab olema kujul AA:BB:CC:DD:EE:FF.");
      } else if (activeMacs.has(mac)) {
        errorsBySlot[idx].push(`MAC kattub slotiga ${activeMacs.get(mac) + 1}.`);
      } else {
        activeMacs.set(mac, idx);
      }

      if (!rid || !isValidRid(rid)) {
        errorsBySlot[idx].push("UAS ID peab olema 1-20 märki ja lubab A-Z, 0-9, tühikut, punkti, alakriipsu või sidekriipsu.");
      } else if (activeRids.has(rid)) {
        errorsBySlot[idx].push(`UAS ID kattub slotiga ${activeRids.get(rid) + 1}.`);
      } else {
        activeRids.set(rid, idx);
      }

      if (!(slot.operator || "").trim()) {
        errorsBySlot[idx].push("Operator ei tohi aktiivsel slotil tühi olla.");
      }
    });
    return errorsBySlot;
  };

  const handleRandomizeProfile = () => {
    const profiles = [
      { uatype: 2, description: "QUAD TEST" },
      { uatype: 1, description: "FIXED TEST" },
      { uatype: 4, description: "VTOL TEST" },
      { uatype: 3, description: "GYRO TEST" },
    ];
    const profile = profiles[Math.floor(Math.random() * profiles.length)];
    const rid = `1596${randomHex(13)}`.slice(0, 20);
    const operator = `EST${randomHex(12)}`.slice(0, 20);
    setDataUpdated(true);
    setData({
      ...data,
      idtype: 1,
      uatype: profile.uatype,
      rid,
      operator,
      description: profile.description,
      manufacturer: "ARUPILOT",
      model: profile.description.replace(" TEST", ""),
      ghost_id_prefix: rid.slice(0, 17),
    });
    setSlots(prev => prev.map((slot, idx) => idx === 0 ? {
      ...slot,
      mac: normalizeMac(data.mac || slot.mac),
      rid,
      operator,
      description: profile.description,
      manufacturer: "ARUPILOT",
      model: profile.description.replace(" TEST", ""),
      uatype: profile.uatype,
      idtype: 1,
    } : slot));
  }

  const handleUseIpHome = () => {
    const lat = F(position.lat);
    const lng = F(position.lng);
    const dd = {
      lat,
      lng,
      op_lat: lat,
      op_lng: lng,
      pe_lat: lat,
      pe_lng: lng,
    };
    setDataUpdated(true);
    setData({ ...data, ...dd });
  }

  const handleDataUpdate = (o = {}) => {
    const dt = {
      ...InitialPosition,
      ...InitialPest,
      ...position,
      ...data,
      ...o,
      appMode: o.appMode ?? modeRef.current.appMode,
    };
    const primarySlot = makePrimarySlot(dt, dt.mac);
    const slotPayload = syncLockedFields(emptySlotList().map((slot, idx) => {
      const current = slots[idx] || slot;
      if (idx === 0) {
        return { ...current, ...primarySlot };
      }
      return {
        ...InitialSlot,
        ...current,
        mac: normalizeMac(current.mac || defaultMacForSlot(primarySlot.mac || dt.mac || "", idx)),
        rid: current.rid || "",
        operator: current.operator || dt.operator || "",
        description: current.description || `${dt.description || "GHOST"} G${idx}`.slice(0, 20),
        manufacturer: current.manufacturer || dt.manufacturer || "",
        model: current.model || dt.model || "",
        uatype: I(current.uatype) || I(dt.uatype) || 2,
        idtype: I(current.idtype) || I(dt.idtype) || 1,
      };
    }));
    const activeSlots = Math.min(MaxTotalUas, I(dt.pe_spawn) || 1);
    const validation = validateSlots(slotPayload, activeSlots);
    setSlotErrors(validation);
    if (validation.some(entry => entry.length > 0)) {
      setError("Paranda slotide vead enne Apply vajutamist.");
      return;
    }
    setError(false);
    serialCommand(Commands.store_data, [
      // Defaults
      S(dt.rid), S(dt.operator), S(dt.description), dt.uatype, dt.idtype, dt.lat, dt.lng, dt.alt, dt.op_lat, dt.op_lng, dt.op_alt, dt.spd, dt.sats, dt.mac, dt.appMode, dt.pe_lat, dt.pe_lng, dt.pe_radius, dt.pe_spawn,
      // External
      dt.ext_mode || 0, dt.ext_baud || 0, dt.ext_rx_pin || 0, dt.ext_tx_pin || 0, dt.ext_shift_mode || 0, dt.ext_shift_radius || 0, dt.ext_shift_min || 0, dt.ext_shift_max || 0,
      S(dt.manufacturer || ""), S(dt.model || ""), dt.ghost_id_scheme || 0, S(dt.ghost_id_prefix || ""), dt.ghost_flight_mode || 0, dt.ghost_altitude_mode || 0
    ]);
    slotPayload.forEach((slot, idx) => serialCommand(Commands.store_slot_data, [
      idx,
      S(slot.mac || ""),
      S(slot.rid || ""),
      S(slot.operator || ""),
      S(slot.description || ""),
      S(slot.manufacturer || ""),
      S(slot.model || ""),
      slot.uatype || 2,
      slot.idtype || 1,
    ]));
    setSlots(slotPayload);
    serialCommand(Commands.data);
    serialCommand(Commands.current);
    requestSlotSync();
  }

  const handleModeUpdate = (o = {}) => {
    const dt = { ...modeRef.current, spd: position.spd, alt: position.alt, ...o }
    modeRef.current = { appMode: dt.appMode, flyMode: dt.flyMode, pathMode: dt.pathMode };
    handleDataUpdate({ appMode: dt.appMode });
    const pp = path.length ? toPath([...path]) : [];
    console.log(pp);
    serialCommand(Commands.store_mode, [dt.appMode + "", dt.flyMode + "", dt.pathMode + "", dt.spd + "", dt.alt + "", path.length, ...deflatePath(pp)]);
    serialCommand(Commands.data);
    serialCommand(Commands.current);
  }

  const handleRM = n => v => {
    handleModeUpdate({ [n]: v });
  }

  const handleAppMode = appMode => () => {
    setAppMode(appMode);
    handleModeUpdate({ appMode });
  }

  const handleFlyMode = flyMode => () => {
    setFlyMode(flyMode);
    handleModeUpdate({ flyMode });
  }

  const handlePathMode = pathMode => () => {
    setPathMode(pathMode);
    handleModeUpdate({ pathMode });
  }

  const handleMapCenter = () => {
    map.setView([parseFloat(position.lat || 0), parseFloat(position.lng || 0)], zoom);
  }

  const handleMapOPCenter = () => {
    map.setView([parseFloat(position.op_lat || 0), parseFloat(position.op_lng || 0)], zoom);
  }

  const handlePEMapCenter = () => {
    map.setView([parseFloat(data.pe_lat || 0), parseFloat(data.pe_lng || 0)], zoom);
  }

  const handleMapLocationCenter = () => {
    map.locate();
  }

  const handleShowProfile = d => () => {
    setShowProfile(d);
    if (!d) {
      setDataUpdated(false);
    }
  }

  const handleShowPath = d => () => {
    setShowPath(d);
  }

  const stopModalClose = e => {
    e.stopPropagation();
  }
  const handlePestRadius = v => {
    const dt = { pe_radius: RadiusMode[v - 1] || MinRadius };
    setData({ ...data, ...dt })
    handleDataUpdate(dt);
  }

  const handleClearPath = () => {
    setPath([...[]]);
  }

  const handleSyncPath = () => {
    handleShowPath(false)();
    handleModeUpdate();
  }

  const handleSerialValue = (value) => {
    if (!value) {
      return;
    }
    if (value.substr(0, 1) === "$") {
      const c = value.substr(1, 1);
      const p = value.split("|");
      console.log("[serial] received", value, " / ", p);
      setDebug(prev => ({ ...prev, lastLine: value, lastType: c }));
      if (c === "%") {
        setLock(false);
        setDataUpdated(false);
        handleSync(false);
      }
      if (c === "V") {
        setData({ ...data, version: p[1] });
      }
      if (c === "C") {
        const lat = F(p[1]);
        const lng = F(p[2]);
        const op_lat = F(p[3]);
        const op_lng = F(p[4]);
        const deviceFlyMode = I(p[10]);
        const keepOrigin = hasZeroOrigin({ lat, lng }) && !hasZeroOrigin(position);
        const keepOperator = hasZeroOrigin({ lat: op_lat, lng: op_lng }) && !hasZeroOrigin({ lat: position.op_lat, lng: position.op_lng });

        if (deviceFlyMode === FlyMode.Fly) {
          setTrail(prev => ([...prev, [p[1], p[2]]]));;
        } else {
          setPest({});
        }
        setPosition({
          ...position,
          lat: keepOrigin ? position.lat : lat,
          lng: keepOrigin ? position.lng : lng,
          op_lat: keepOperator ? position.op_lat : op_lat,
          op_lng: keepOperator ? position.op_lng : op_lng,
          alt: I(p[5]),
          op_alt: I(p[6]),
          spd: I(p[7]),
          hdg: I(p[8]),
          sats: I(p[9]),
        });
        setFlyMode(deviceFlyMode);
        setPathMode(I(p[11]));
        modeRef.current = { ...modeRef.current, flyMode: deviceFlyMode, pathMode: I(p[11]) };
        setDebug(prev => ({ ...prev, deviceFlyMode: deviceFlyMode, devicePathMode: I(p[11]) }));
        if (first) {
          //handleMapCenter(p[1], p[2]);
          setFirst(false);
        }
      }
      if (c === "T") {
        setPest(prev => {
          let mc = p[6];
          const u = {
            path: [],
            ...(prev[mc] || {}),
            lat: F(p[1]),
            lng: F(p[2]),
            alt: I(p[3]),
            spd: I(p[4]),
            hdg: I(p[5]),
            rid: p[7],
            operator: p[8],
            description: p[9],
            uatype: p[10],
            idtype: p[11],
            manufacturer: p[13] || "",
            model: p[14] || "",
          };
          u.path = [...(u.path || []), [u.lat, u.lng]].slice(-120);
          return { ...prev, [mc]: u };
        });
        setFlyMode(I(p[12]));
        setDebug(prev => ({ ...prev, deviceAppMode: AppMode.Pest, deviceFlyMode: I(p[12]) }));
      }
      if (c === "G") {
        const slotIndex = I(p[1]);
        if (slotIndex >= 0 && slotIndex < MaxTotalUas) {
          setSlots(prev => prev.map((slot, idx) => idx === slotIndex ? {
            ...slot,
            mac: p[2] || "",
            rid: p[3] || "",
            operator: p[4] || "",
            description: p[5] || "",
            manufacturer: p[6] || "",
            model: p[7] || "",
            uatype: I(p[8]) || 2,
            idtype: I(p[9]) || 1,
          } : slot));
          setSlotErrors(prev => prev.map((entry, idx) => idx === slotIndex ? [] : entry));
        }
      }
      if (c === "D") {
        if (editStateRef.current.showProfile && editStateRef.current.dataUpdated) {
          const deviceAppMode = I(p[16]);
          setAppMode(deviceAppMode);
          modeRef.current = { ...modeRef.current, appMode: deviceAppMode };
          setDebug(prev => ({ ...prev, deviceAppMode }));
          return;
        }
        const lat = F(p[7]);
        const lng = F(p[8]);
        const op_lat = F(p[10]);
        const op_lng = F(p[11]);
        const pe_lat = F(p[17]);
        const pe_lng = F(p[18]);
        const keepOrigin = hasZeroOrigin({ lat, lng }) && !hasZeroOrigin(position);
        const keepOperator = hasZeroOrigin({ lat: op_lat, lng: op_lng }) && !hasZeroOrigin({ lat: position.op_lat, lng: position.op_lng });
        const keepPest = hasZeroOrigin({ lat: pe_lat, lng: pe_lng }) && !hasZeroOrigin({ lat: data.pe_lat, lng: data.pe_lng });

        setData({ 
          ...data, 
          version: p[1], rid: p[2], operator: p[3], description: p[4], uatype: p[5], idtype: p[6],
          lat: keepOrigin ? data.lat || position.lat : lat,
          lng: keepOrigin ? data.lng || position.lng : lng,
          alt: p[9],
          op_lat: keepOperator ? data.op_lat || position.op_lat : op_lat,
          op_lng: keepOperator ? data.op_lng || position.op_lng : op_lng,
          op_alt: p[12], spd: p[13], sats: p[14], mac: p[15],
          pe_lat: keepPest ? data.pe_lat : pe_lat,
          pe_lng: keepPest ? data.pe_lng : pe_lng,
            pe_radius: I(p[19]), pe_spawn: Math.min(MaxTotalUas, I(p[20]) || 1), 
          ext_mode: I(p[21]), ext_baud: I(p[22]), ext_rx_pin: I(p[23]), ext_tx_pin: I(p[24]), ext_shift_mode: I(p[25]), ext_shift_radius: I(p[26]), ext_shift_min: I(p[27]), ext_shift_max: I(p[28]),
          manufacturer: p[30] || data.manufacturer || "",
          model: p[31] || data.model || "",
          ghost_id_scheme: I(p[32]),
          ghost_id_prefix: p[33] || data.ghost_id_prefix || "",
          ghost_flight_mode: I(p[34]),
          ghost_altitude_mode: I(p[35]),
         
         });
        const deviceAppMode = I(p[16]);
        setAppMode(deviceAppMode);
        modeRef.current = { ...modeRef.current, appMode: deviceAppMode };
        setDebug(prev => ({ ...prev, deviceAppMode }));
        const pc = I(p[29]);
        const pp = pc !== 0 ? fromPath([p[7], p[8]], inflatePath(p.slice(30))) : [];
        console.log("[path]", pc, pp);
        setPath(pp);

      }
    }
  }

  const handleMapEvent = e => {
    if (e.type === "click") {
      const lat = e.latlng.lat;
      const lng = e.latlng.lng;

      let dd = {};

      switch (mapMode) {
        case MapMode.Path:
          setPath(prev => [...prev, [F(lat), F(lng)]]);
          break;
        case MapMode.Origin:
          dd = { lat: F(lat), lng: F(lng) };
          setData({ ...data, ...dd });
          setPosition({ ...position, ...dd });
          handleDataUpdate(dd);
          break;
        case MapMode.Operator:
          dd = { op_lat: F(lat), op_lng: F(lng) };
          setData({ ...data, ...dd });
          setPosition({ ...position, ...dd });
          handleDataUpdate(dd);
          break;
        case MapPestMode.Origin:
          dd = { pe_lat: F(lat), pe_lng: F(lng) };
          setData({ ...data, ...dd });
          handleDataUpdate(dd);
          break;
        default:
          break;
      }
    }
    if (e.type === "locationfound") {
      map.setView(e.latlng, zoom)
    }
  }

  const handleMapMode = m => () => {
    setMapMode(m);
  }

  useEffect(() => {
    editStateRef.current = { showProfile, showPath, dataUpdated };
  }, [showProfile, showPath, dataUpdated]);

  useEffect(() => {
    let cancelled = false;

    const readIpLocation = async () => {
      const providers = [
        {
          url: "https://ipwho.is/",
          parse: geo => geo && geo.success !== false ? [geo.latitude, geo.longitude] : [],
        },
        {
          url: "https://ipapi.co/json/",
          parse: geo => [geo.latitude, geo.longitude],
        },
      ];

      for (const provider of providers) {
        try {
          const response = await fetch(provider.url);
          if (!response.ok) continue;

          const [rawLat, rawLng] = provider.parse(await response.json());
          const lat = F(rawLat);
          const lng = F(rawLng);

          if (Number.isFinite(lat) && Number.isFinite(lng) && (lat !== 0 || lng !== 0)) {
            return { lat, lng };
          }
        } catch (e) {
          console.warn("IP geolocation provider failed", provider.url, e);
        }
      }

      return null;
    };

    const applyIpLocation = async () => {
      const geo = await readIpLocation();
      if (cancelled) return;

      setGeoInitialized(true);
      if (!geo) return;

      setPosition(prev => hasZeroOrigin(prev) || prev === InitialPosition ? {
        ...prev,
        lat: geo.lat,
        lng: geo.lng,
        op_lat: geo.lat,
        op_lng: geo.lng,
      } : prev);

      setData(prev => ({
        ...prev,
        pe_lat: hasZeroOrigin({ lat: prev.pe_lat, lng: prev.pe_lng }) || prev === InitialPest ? geo.lat : prev.pe_lat,
        pe_lng: hasZeroOrigin({ lat: prev.pe_lat, lng: prev.pe_lng }) || prev === InitialPest ? geo.lng : prev.pe_lng,
      }));

      if (map) {
        map.setView([geo.lat, geo.lng], zoom);
      }
    };

    if (!connected && !geoInitialized && map) {
      applyIpLocation();
    }

    return () => {
      cancelled = true;
    };
  }, [connected, geoInitialized, map, zoom]);


  useEffect(() => {
    if (connected) {
      window.squid = {};
      Object.keys(Commands).forEach(cmd => window.squid[cmd] = () => serialCommand(Commands[cmd]));
      serial.readLoop((value, done) => handleSerialValue(value)).catch(e => {
        setConnected(false);
      });
      handleSync();
      const timer = window.setInterval(() => {
        if (!editStateRef.current.showProfile && !editStateRef.current.showPath && !editStateRef.current.dataUpdated) {
          handleSync(false);
        }
      }, 1500);
      return () => window.clearInterval(timer);
    }
  }, [connected]);

  useEffect(() => {
    const onKeyDown = e => {
      if (e.key === "Escape") {
        setShowProfile(false);
        setShowPath(false);
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, []);

  if (!connected) {
    return (
      <div id="screen">
        <img className="screen-logo" src={`${process.env.PUBLIC_URL}/tondid-mark.svg`} alt="TondID logo" />
        <div className="screen-badge">Remote ID control surface</div>
        <div className="screen-brand">
          <img className="screen-wordmark" src={`${process.env.PUBLIC_URL}/tondid-wordmark.svg`} alt="TondID wordmark" />
          <span className="screen-shortmark">TID</span>
        </div>
        <div className="screen-subtitle">Modernized local control for the ESP32-based Remote ID mock rig</div>
        <div className="action">
          <Button onPress={handleSerialConnect} name={"Connect Device"} />
        </div>
        <div className="screen-copy">Configure swarm behavior, persist settings to the board, and drive live test broadcasts over WebSerial.</div>
        <div className="screen-link">
          <span>`TID` is the compact mark for constrained spaces</span>
        </div>
        <div className="screen-version">Version 1.0</div>
        {error ? <div id="error"><small>{error}</small></div> : null}
      </div >
    )
  }

  return (
    <div id="map">
      <Display condition={showProfile === true}>
        <div id="modal" onClick={handleShowProfile(false)}>
          <div className="inner" onClick={stopModalClose}>
            <div className="controls">
              <Button disabled={lock} onPress={handleSync} secondary name={"Apply"} />
              <Button onPress={handleShowProfile(false)} name={"Cancel"} />
            </div>
            <div className="settings-grid">
              <div className="setting">
                <h3>Primary Identity</h3>
                <div className="controls">
                  <Button onPress={handleRandomizeProfile} secondary name={"Randomize"} />
                  <Button onPress={handleUseIpHome} name={"Use IP Home"} />
                </div>
                <Dropdown items={idtype_t} selected={data.idtype} label="ID Type" onChange={handleData("idtype")} />
                <div className="form">
                  <label className="w">Remote ID</label>
                  <input type="input" value={data.rid} onChange={handleData("rid")} size="24" maxLength="24" />
                </div>
                <Dropdown items={uatype_t} selected={data.uatype} label="UA Type" onChange={handleData("uatype")} />
                {["operator", "description", "manufacturer", "model"].map(key =>
                  <div className="form" key={key}>
                    <label className="w">{key}</label>
                    <input type="input" value={data[key]} size="24" maxLength="24" onChange={handleData(key)} />
                  </div>
                )}
              </div>

              <div className="setting">
                <h3>Swarm Defaults</h3>
                <div className="form">
                  <label className="w">Ghost Drones</label>
                  <input type="input" value={Math.max(0, Math.min(MaxGhostUas, (I(data.pe_spawn) || 1) - 1))} size="24" maxLength="24" onChange={handleGhostCount} />
                </div>
                <Dropdown items={ghost_id_scheme_t} selected={data.ghost_id_scheme || 0} label="Ghost ID Scheme" onChange={handleData("ghost_id_scheme")} />
                <div className="form">
                  <label className="w">Ghost ID Prefix</label>
                  <input type="input" value={data.ghost_id_prefix || ""} size="24" maxLength="24" onChange={handleData("ghost_id_prefix")} />
                </div>
                <Dropdown items={ghost_flight_mode_t} selected={data.ghost_flight_mode || 0} label="Ghost Flight Mode" onChange={handleData("ghost_flight_mode")} />
                <Dropdown items={ghost_altitude_mode_t} selected={data.ghost_altitude_mode || 0} label="Ghost Altitude Mode" onChange={handleData("ghost_altitude_mode")} />
              </div>

              <div className="setting">
                <h3>Tuning</h3>
                {[["Origin Lat", "lat"], ["Origin Lng", "lng"], ["Origin Alt", "alt"], ["Operator Lat", "op_lat"], ["Operator Lng", "op_lng"], ["Operator Alt", "op_alt"], ["Speed", "spd"], ["Satellites", "sats"], ["MAC Address", "mac"]].map(ref =>
                  <div className="form" key={ref[1]}>
                    <label className="w">{ref[0]}</label>
                    <input type="input" value={data[ref[1]] || ""} size="24" maxLength="24" onChange={handleData(ref[1])} />
                  </div>
                )}
              </div>

              <div className="setting section-span-full">
                <h3>Swarm Slots</h3>
                <div className="controls wrap">
                  <Button onPress={handleClonePrimaryToSlots} secondary name={"Clone Primary To All"} />
                  <Button onPress={handleAutoMacs} name={"Auto MACs"} />
                  <Button onPress={handleRandomizeSlots} name={"Randomize Slots"} />
                </div>
                <div className="controls wrap slot-locks">
                  {SlotSharedFields.map(field => (
                    <Button
                      key={field}
                      selected={sharedLocks[field]}
                      onPress={handleSharedLock(field)}
                      name={`${sharedLocks[field] ? "Lock" : "Free"} ${SlotFieldLabels[field]}`}
                    />
                  ))}
                </div>
                <div className="slot-grid">
                  {slots.map((slot, idx) => {
                    const activeSlots = Math.min(MaxTotalUas, I(data.pe_spawn) || 1);
                    const active = idx < activeSlots;
                    const errors = slotErrors[idx] || [];
                    return (
                      <div className="slot-card" key={idx}>
                        <div className="slot-card-header">
                          <strong>Slot {idx + 1}</strong>
                          <div className="slot-card-actions">
                            <span className={`slot-chip ${active ? "active" : ""}`}>{active ? "ACTIVE" : "IDLE"}</span>
                            <Button name={"Copy"} onPress={handleCopySlot(idx)} />
                            <Button name={"Paste"} disabled={!slotClipboard} onPress={handlePasteSlot(idx)} />
                          </div>
                        </div>
                        <div className="form">
                          <label className="w">MAC</label>
                          <input type="input" value={slot.mac || ""} size="24" maxLength="17" onChange={handleSlotData(idx, "mac")} />
                        </div>
                        <div className="form">
                          <label className="w">UAS ID</label>
                          <input type="input" value={slot.rid || ""} size="24" maxLength="20" onChange={handleSlotData(idx, "rid")} />
                        </div>
                        <div className="form">
                          <label className="w">Operator</label>
                          <input type="input" disabled={idx > 0 && sharedLocks.operator} value={slot.operator || ""} size="24" maxLength="20" onChange={handleSlotData(idx, "operator")} />
                        </div>
                        <div className="form">
                          <label className="w">Self ID</label>
                          <input type="input" value={slot.description || ""} size="24" maxLength="20" onChange={handleSlotData(idx, "description")} />
                        </div>
                        <div className="form">
                          <label className="w">Manufacturer</label>
                          <input type="input" disabled={idx > 0 && sharedLocks.manufacturer} value={slot.manufacturer || ""} size="24" maxLength="20" onChange={handleSlotData(idx, "manufacturer")} />
                        </div>
                        <div className="form">
                          <label className="w">Model</label>
                          <input type="input" disabled={idx > 0 && sharedLocks.model} value={slot.model || ""} size="24" maxLength="20" onChange={handleSlotData(idx, "model")} />
                        </div>
                        <Dropdown items={uatype_t} selected={slot.uatype || 2} label="UA Type" disabled={idx > 0 && sharedLocks.uatype} onChange={handleSlotData(idx, "uatype")} />
                        <Dropdown items={idtype_t} selected={slot.idtype || 1} label="ID Type" disabled={idx > 0 && sharedLocks.idtype} onChange={handleSlotData(idx, "idtype")} />
                        {errors.length ? <div className="slot-errors">{errors.map((message, errorIdx) => <div key={errorIdx}>{message}</div>)}</div> : null}
                      </div>
                    );
                  })}
                </div>
              </div>

              <div className="setting section-span-full">
                <h3>External</h3>
                <Dropdown items={external_t} selected={data.ext_mode} label="Protocol" onChange={handleData("ext_mode")} />
                {[["Baud Rate", "ext_baud"], ["RX Pin", "ext_rx_pin"], ["TX Pin", "ext_tx_pin"]].map(ref =>
                  <div className="form" key={ref[1]}>
                    <label className="w">{ref[0]}</label>
                    <input type="input" value={data[ref[1]] || ""} size="24" maxLength="24" onChange={handleData(ref[1])} />
                  </div>
                )}
              </div>
            </div>
            <div className="controls footer">
              <Button disabled={lock} onPress={handleSync} secondary name={"Apply"} />
              <Button onPress={handleShowProfile(false)} name={"Cancel"} />
            </div>
          </div>
        </div>
      </Display>
      <Display condition={showPath === true}>
        <div id="modal" onClick={handleShowPath(false)}>
          <div className="inner" onClick={stopModalClose}>
            <div className="controls">
              <Button disabled={lock} onPress={handleSyncPath} secondary name={"Apply"} />
              <Button disabled={lock} onPress={handleClearPath} name={"Clear"} />
              <Button onPress={handleShowPath(false)} name={"Cancel"} />
            </div>
            <div className="setting">
              <p className="secondary sl">Path</p>
              {toCombinedPath(path.length ? [[position.lat, position.lng], ...path, [position.lat, position.lng]] : []).map((item, index) => {
                return (
                  <div className="pathitem" key={index}>
                    <div>{index + 1}</div>
                    <div><span>LAT</span>{F(item.lat, 3)}</div>
                    <div><span>LNG</span>{F(item.lng, 3)}</div>
                    <div><span>HDG</span>{F(item.path[0])}</div>
                    <div><span>DST</span>{F(item.path[1])}</div>
                  </div>
                );
              })}
            </div>
            <div className="controls footer">
              <Button disabled={lock} onPress={handleSyncPath} secondary name={"Apply"} />
              <Button disabled={lock} onPress={handleClearPath} name={"Clear"} />
              <Button onPress={handleShowPath(false)} name={"Cancel"} />
            </div>
          </div>
        </div>
      </Display>
      <MapContainer center={[position.lat, position.lng]} zoom={zoom} scrollWheelZoom={true} zoomControl={false} ref={setMap}>
        <TileLayer
          attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
          className="basemap"
          maxNativeZoom={19}
          maxZoom={19}
          url="https://tile.openstreetmap.org/{z}/{x}/{y}.png"
        />
        <Display condition={appMode === AppMode.External}>
          <Circle color="green" center={[position.lat, position.lng]} radius={15} />
          <Circle color="blue" center={[position.op_lat, position.op_lng]} radius={10} />
          <Circle color="yellow" center={[data.pe_lat, data.pe_lng]} radius={data.pe_radius < 10 ? 10 : data.pe_radius} />
          {position.lat && trail.length ? <Polyline pathOptions={{ color: "red" }} positions={[[position.lat, position.lng], ...trail]} /> : null}
        </Display>
        <Display condition={appMode === AppMode.Squid}>
          <Circle color="red" center={[position.lat, position.lng]} radius={15} />
          <Circle color="blue" center={[position.op_lat, position.op_lng]} radius={10} />
          {position.lat && trail.length ? <Polyline pathOptions={{ color: "red" }} positions={[[position.lat, position.lng], ...trail]} /> : null}
          {position.lat && path.length ? <Polyline pathOptions={{ color: "green" }} positions={[[position.lat, position.lng], ...path, [position.lat, position.lng]]} /> : null}
        </Display>
        <Display condition={appMode === AppMode.Pest}>
          <Circle color="yellow" center={[data.pe_lat, data.pe_lng]} radius={data.pe_radius < 10 ? 10 : data.pe_radius} />
          {Object.keys(pest).map(mac => {
            let u = pest[mac];
            return (
              <FeatureGroup key={mac}>
                <Popup>
                  <sub>RemoteID</sub>
                  <div>{u.rid}</div>
                  <sub>Operator</sub>
                  <div>{u.operator}</div>
                  <sub>Manufacturer / Model</sub>
                  <div>{[u.manufacturer, u.model].filter(Boolean).join(" ") || u.description}</div>
                  <sub>Self ID</sub>
                  <div>{u.description}</div>
                  <sub>Speed / Alt / Heading / Lat / Lng</sub>
                  <div><span>{u.spd}</span><span>{u.alt}</span><span>{u.hdg}</span><span>{F(u.lat, 3)}</span><span>{F(u.lng, 3)}</span></div>
                </Popup>
                <Circle color="red" center={[u.lat, u.lng]} radius={15} />
                <Polyline pathOptions={{ color: "red" }} positions={[...u.path]} />
              </FeatureGroup >
            )
          })}
        </Display>

        <LocationMarker onEvent={handleMapEvent} />
        <Control position="bottomleft">
          <Display condition={mapMode === MapMode.Pan && (appMode === AppMode.External || appMode === AppMode.Squid)}>
            <div className="telemetry">
              <div><span>Lat</span>{F(position.lat)}</div>
              <div><span>Lng</span>{F(position.lng)}</div>
              <div><span>Alt</span>{F(position.alt, 1)}</div>
              <div><span>Speed</span>{F(position.spd, 0)}</div>
              <div><span>Heading</span>{F(position.hdg, 0)}</div>
              <div><span>Sats</span>{F(position.sats, 0)}</div>
              <div><span>OP Lat</span>{F(position.op_lat)}</div>
              <div><span>OP Lng</span>{F(position.op_lng)}</div>
              <div><span>OP Alt</span>{F(position.op_alt, 1)}</div>
            </div>
          </Display>
        </Control>
        <Control position="bottomright" prepend>
          <div className="map-control right">
            <Display condition={appMode === AppMode.Squid}>
              {Object.keys(MapMode).map(key =>
                <Button key={key} disabled={lock} selected={mapMode === MapMode[key]} name={key} onPress={handleMapMode(MapMode[key])} />
              )}
            </Display>
            <Display condition={appMode === AppMode.Pest}>
              {Object.keys(MapPestMode).map(key =>
                <Button key={key} disabled={lock} selected={mapMode === MapPestMode[key]} name={key} onPress={handleMapMode(MapPestMode[key])} />
              )}
            </Display>
            <Display condition={appMode === AppMode.External}>
              {Object.keys(MapExternalMode).map(key =>
                <Button key={key} disabled={lock} selected={mapMode === MapPestMode[key]} name={key} onPress={handleMapMode(MapPestMode[key])} />
              )}
            </Display>

          </div>
        </Control>
        <Control position="bottomleft">
          <Display condition={appMode === AppMode.Squid || appMode === AppMode.External}>
            <div className="map-control sl">
              <div>{data.operator || "No Name"}</div>
              <div>{data.rid || "No ID"}</div>
            </div>
          </Display>
          <div className="map-control inline">
            <Display condition={appMode === AppMode.Squid || appMode === AppMode.External}>
              <Button name="Center" onPress={handleMapCenter} />
              <Button name="OP" onPress={handleMapOPCenter} />
            </Display>
            <Display condition={appMode === AppMode.Pest}>
              <Button name="Center" onPress={handlePEMapCenter} />
            </Display>
            <Button name="Loc" onPress={handleMapLocationCenter} />
          </div>
        </Control>
        <Control position="topright" prepend>
          <div className="map-control right rail-card utility-card">
            <Button onPress={handleSerialConnect} name={connected ? "Disconnect" : "Connect"} />
            <Button disabled={lock} onPress={handleSync} name={"Sync"} />
            <Button name="Profile" onPress={handleShowProfile(true)} />
            <Button name="Path" onPress={handleShowPath(true)} />
          </div>
        </Control>
        <Control position="topleft" prepend>
          <div className="map-control inline rail-card">
            {Object.keys(AppMode).map(key =>
              <Button key={key} disabled={lock} selected={appMode === AppMode[key]} name={key} onPress={handleAppMode(AppMode[key])} />
            )}
          </div>
          <div className="map-control inline rail-card">
            {Object.keys(FlyMode).map(key =>
              <Button key={key} disabled={lock} selected={flyMode === FlyMode[key]} name={key} onPress={handleFlyMode(FlyMode[key])} />
            )}
          </div>
          <Display condition={mapMode === MapMode.Pan && appMode === AppMode.Squid}>
            <div className="map-control inline rail-card">
              {Object.keys(PathMode).map(key =>
                <Button key={key} disabled={lock} selected={pathMode === PathMode[key]} name={key} onPress={handlePathMode(PathMode[key])} />
              )}
            </div>
          </Display>
          <Display condition={mapMode === MapMode.Pan && appMode === AppMode.Squid}>
            <div className="map-control inline rail-card">
              <RemoteControlButton name="SPD" steps={[0, 50, 100, 200]} onChange={handleRM("spd")} />
              <RemoteControlButton name="ALT" steps={[0, 100, 500, 1000]} onChange={handleRM("alt")} />
            </div>
          </Display>
          <Display condition={mapMode === MapMode.Pan && appMode === AppMode.External}>
            <div className="map-control inline">
              {/*
              <RemoteControlButton name="MIN" steps={[0, 100, 500, 1000]} onChange={handleRM("alt")} />
              <RemoteControlButton name="MAX" steps={[0, 100, 500, 1000]} onChange={handleRM("alt")} />
              <RemoteControlButton name="RNG" steps={[1, 2, 3, 4, 5, 6]} onChange={handlePestRadius} />
              <p>RANGE {data.pe_radius || MinRadius}m MIN {data.pe_spawn}s</p> */}
            </div>
          </Display>
          <Display condition={appMode === AppMode.Pest}>
            <div className="map-control inline rail-card">
              <RemoteControlButton name="RNG" steps={[1, 2, 3, 4, 5, 6]} onChange={handlePestRadius} />
                <p>{Math.min(MaxTotalUas, I(data.pe_spawn) || 1)} UAS RANGE {data.pe_radius || MinRadius}m</p>
            </div>
          </Display>
          <div className="map-control sl debug-card">
            <div>DEVICE {appModeLabel(debug.deviceAppMode)} / {flyModeLabel(debug.deviceFlyMode)} / {pathModeLabel(debug.devicePathMode)}</div>
            <div>PLOT {Object.keys(pest).length} LAST ${debug.lastType}</div>
            <div style={{ maxWidth: 220, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>{debug.lastLine || "NO SERIAL RX"}</div>
          </div>
        </Control>
      </MapContainer >
    </div >
  );
}

export default App;



