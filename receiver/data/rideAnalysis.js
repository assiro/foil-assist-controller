// ============================================================
//  rideAnalysis.js  –  TecnoFly shared analysis module
//  Algoritmo v2 – usa tilt, roll, pitch, rms, peak, throttle, watts
//  Formato dati: BINARIO vescDat<n>.bin  (VescRecord packed 22 byte)
//  Usato da: ride.html (dettaglio singola sessione)
//            index.html (sintesi tutte le sessioni)
// ============================================================

// ---- Soglie takeoff ----
// Soglia DINAMICA (fix 18/08/2026): potenza max VESC (l_watt_max da
// /vescPower) - margine, come nel firmware. Fallback 1800 se non raggiungibile.
let   RA_TAKEOFF_WATTS      = 1800;
const RA_TAKEOFF_MARGIN     = 200;   // W sotto la max VESC
const RA_TAKEOFF_MIN        = 300;   // floor anti-falsi positivi
const RA_TAKEOFF_MIN_SAMP   = 3;

// Aggiorna RA_TAKEOFF_WATTS dalla potenza max corrente del VESC
async function refreshTakeoffWatts() {
    try {
        const res = await fetch('/vescPower');
        if (!res.ok) return;
        const wmax = parseInt(await res.text(), 10);
        if (wmax > 0) RA_TAKEOFF_WATTS = Math.max(RA_TAKEOFF_MIN, wmax - RA_TAKEOFF_MARGIN);
    } catch (e) { /* fallback: resta 1800 */ }
}

// ---- Soglie foil / surf ----
const RA_FOIL_RMS_MIN      = 0.18;
const RA_FOIL_PEAK_MIN     = 0.30;
const RA_FOIL_TILT_MAX     = 25.0;
const RA_FOIL_ROLL_MAX     = 20.0;
const RA_FOIL_PITCH_MAX    = 20.0;

// ---- Rilevamento caduta ----
// NOTA: rms basso da solo NON indica caduta (può essere pump quieto o glide).
// La caduta vera richiede che gli angoli siano fuori soglia.
// RA_FALL_RMS_LOW è mantenuto ma usato SOLO se combinato con angolo anomalo.
const RA_FALL_TILT_THR     = 28.0;
const RA_FALL_ROLL_THR     = 25.0;
const RA_FALL_PITCH_THR    = 25.0;
const RA_FALL_RMS_LOW      = 0.12;   // usato solo insieme ad angolo anomalo
const RA_FALL_CONFIRM      = 2;

// ---- Durata sessione foil ----
const RA_SURF_MIN_SEC      = 8;
// 3600 s = 1 h (era 180 s): i ride pump&glide oltre 3 minuti venivano
// scartati del tutto — niente wave, niente tempo totale (fix 18/08/2026,
// ora allineato al firmware ride_analysis_binary.ino)
const RA_SURF_MAX_SEC      = 3600;

// ---- Gap tolerance ----
// Numero massimo di campioni consecutivi "non-foil" (throttle=0, senza caduta)
// tollerati senza chiudere la sessione. Durante il gap waveTime si incrementa:
// il rider è ancora in volo (glide liscio o pump poco energico) e quei secondi
// fanno parte della sessione a tutti gli effetti.
// 30 campioni (~30 s a 1 Hz): un glide liscio tra pompate dura 10-20 s,
// con 5 campioni la sessione si chiudeva in pieno volo (ride tagliati).
// Allineato al firmware FOIL_GAP_MAX (fix 18/08/2026).
const RA_FOIL_GAP_MAX      = 30;

// ============================================================
//  VescRecord layout  –  22 byte packed (deve corrispondere al C)
//
//  offset  type      name
//  0       uint8     tMin
//  1       uint8     tSec
//  2       int16     rpm
//  4       int16     voltage10   (V × 10)
//  6       int16     current10  (A × 10, 1 decimale — fix 10/08/2026)
//  8       uint16    power
//  10      uint8     throttle    (0-99)
//  11      uint8     batt
//  12      uint16    watthour
//  14      int8      tempMosfet
//  15      uint16    powerAvg
//  17      uint8     rms100      (g × 100)
//  18      uint8     peak100     (g × 100)
//  19      int8      tilt
//  20      int8      pitch
//  21      int8      roll
// ============================================================
const VESC_RECORD_SIZE = 22;

/**
 * Parsa un ArrayBuffer contenente N record VescRecord packed.
 * Ritorna un array di oggetti con gli stessi campi di parseVESCLine().
 */
function parseVESCBinary(buffer) {
    const view    = new DataView(buffer);
    const LE      = true;
    const n       = Math.floor(buffer.byteLength / VESC_RECORD_SIZE);
    const records = [];

    for (let i = 0; i < n; i++) {
        const off = i * VESC_RECORD_SIZE;

        const tMin      = view.getUint8 (off);
        const tSec      = view.getUint8 (off + 1);
        const rpm       = view.getInt16 (off + 2,  LE);
        const volt10    = view.getInt16 (off + 4,  LE);
        const current   = view.getInt16 (off + 6,  LE) / 10;   // fix 10/08/2026: campo ×10
        const power     = view.getUint16(off + 8,  LE);
        const throttle  = view.getUint8 (off + 10);
        const batt      = view.getUint8 (off + 11);
        const watthour  = view.getUint16(off + 12, LE);
        const temp      = view.getInt8  (off + 14);
        const powerAvg  = view.getUint16(off + 15, LE);
        const rms100    = view.getUint8 (off + 17);
        const peak100   = view.getUint8 (off + 18);
        const tilt      = view.getInt8  (off + 19);
        const pitch     = view.getInt8  (off + 20);
        const roll      = view.getInt8  (off + 21);

        records.push({
            time:     String(tMin).padStart(2, '0') + ':' + String(tSec).padStart(2, '0'),
            tMin,
            tSec,
            rpm,
            voltage:  volt10 / 10,
            current,
            watts:    power,
            throttle,
            battery:  batt,
            wh:       watthour,
            temp,
            powerAvg,
            rms:      rms100  / 100,
            peak:     peak100 / 100,
            tilt,
            pitch,
            roll,
        });
    }
    return records;
}

// ============================================================
//  Helpers booleani (speculari al firmware C++)
// ============================================================
function _isFoilRiding(d, hasAdxl) {
    if (!hasAdxl) return true;
    const motionOk  = d.rms >= RA_FOIL_RMS_MIN || d.peak >= RA_FOIL_PEAK_MIN;
    const postureOk = Math.abs(d.tilt)  <= RA_FOIL_TILT_MAX &&
                      Math.abs(d.pitch) <= RA_FOIL_PITCH_MAX &&
                      Math.abs(d.roll)  <= RA_FOIL_ROLL_MAX;
    return motionOk && postureOk;
}

function _isFall(d, hasAdxl) {
    if (!hasAdxl) return false;
    // Caduta vera: angoli fuori soglia.
    // rms basso da solo NON basta: durante pump quieto o glide il rider
    // può avere rms < 0.12 con tilt/roll normali (tavola dritta = non è caduta).
    // rms basso è usato solo come conferma aggiuntiva quando UN angolo è già
    // vicino alla soglia (tilt/roll/pitch > metà soglia).
    const anglesAnomaly = Math.abs(d.tilt)  > RA_FALL_TILT_THR  ||
                          Math.abs(d.roll)  > RA_FALL_ROLL_THR   ||
                          Math.abs(d.pitch) > RA_FALL_PITCH_THR;
    const anglesWarning = Math.abs(d.tilt)  > RA_FALL_TILT_THR  / 2 ||
                          Math.abs(d.roll)  > RA_FALL_ROLL_THR   / 2 ||
                          Math.abs(d.pitch) > RA_FALL_PITCH_THR  / 2;
    return anglesAnomaly || (anglesWarning && d.rms < RA_FALL_RMS_LOW);
}

// Differenza in secondi tra due timestamp "MM:SS"
function _timeDiff(t1, t2) {
    if (!t1 || !t2 || t2 === "EOF") return 0;
    const [m1, s1] = t1.split(":").map(Number);
    const [m2, s2] = t2.split(":").map(Number);
    return Math.abs((m1 * 60 + s1) - (m2 * 60 + s2));
}

// ============================================================
//  analyzeRideRecords(records, hasAdxl)
//
//  Riceve l'array di oggetti già parsati (da parseVESCBinary)
//  e restituisce il risultato dell'analisi. Logica identica
//  alla vecchia analyzeRideText(), ma opera su oggetti invece
//  che su righe CSV.
// ============================================================
function analyzeRideRecords(records, hasAdxl = true, minSurfSec = RA_SURF_MIN_SEC) {
    const takeoffs = [], foils = [], falls = [];

    let consecHP  = 0, waveTime = 0, fallSamp = 0, gapCount = 0;
    let inTakeoff = false, inFoil = false, inFall = false, afterTakeoff = false;
    let takeoffN  = 0, foilN = 0;
    let startTakeoff = "", startFoil = "";
    let waveBestTime = 0, longestTakeoff = 0;
    let totalFoilTime = 0;   // tempo foil totale, conta SEMPRE (speculare al firmware)

    for (const d of records) {
        // ── 1. Caduta ─────────────────────────────────────────
        if (_isFall(d, hasAdxl)) {
            fallSamp++;
            if (fallSamp >= RA_FALL_CONFIRM && !inFall) {
                inFall = true;
                falls.push({ time: d.time, tilt: d.tilt, pitch: d.pitch, roll: d.roll, rms: d.rms });
            }
        } else {
            fallSamp = 0;
            inFall   = false;
        }

        // ── 2. Takeoff ────────────────────────────────────────
        const highPower = d.watts > RA_TAKEOFF_WATTS && d.throttle > 10;
        if (highPower) {
            if (!inTakeoff) startTakeoff = d.time;
            consecHP++;
            inTakeoff    = true;
            afterTakeoff = true;
            // motore acceso: chiudi la sessione foil se aperta.
            // Come nel firmware: il tempo conta sempre, il cap limita solo
            // conteggio sessioni e record (RA_SURF_MAX_SEC = 3600).
            if (inFoil) {
                if (waveTime >= minSurfSec) {
                    totalFoilTime += waveTime;   // il tempo conta sempre
                    if (waveTime <= RA_SURF_MAX_SEC) {
                        foilN++;
                        foils.push({ number: foilN, duration: waveTime, start: startFoil, end: d.time });
                        if (waveTime > waveBestTime) waveBestTime = waveTime;
                    }
                }
                inFoil = false; waveTime = 0; gapCount = 0;
            }
        } else {
            if (inTakeoff && consecHP >= RA_TAKEOFF_MIN_SAMP) {
                const dur = _timeDiff(startTakeoff, d.time);
                takeoffN++;
                takeoffs.push({ number: takeoffN, duration: dur, start: startTakeoff, end: d.time });
                if (dur > longestTakeoff) longestTakeoff = dur;
            }
            consecHP  = 0;
            inTakeoff = false;
        }

        // ── 3. Foil / surf con gap tolerance ─────────────────
        //
        // Un "gap" è un campione a throttle=0, senza caduta, in cui rms/peak
        // scende momentaneamente sotto soglia (glide liscio, pump quieto).
        // Non chiudiamo la sessione: aspettiamo fino a RA_FOIL_GAP_MAX campioni
        // consecutivi. In entrambi i casi (foil valido o gap) waveTime cresce:
        // il rider è ancora in volo e quei secondi appartengono alla sessione.

        const motorOff = d.throttle <= 5 && d.watts < 50;
        const surfing  = afterTakeoff && motorOff && _isFoilRiding(d, hasAdxl) && !inFall;

        if (surfing) {
            // campione foil valido
            if (!inFoil) { startFoil = d.time; waveTime = 0; }
            inFoil   = true;
            gapCount = 0;   // gap precedente assorbito
            waveTime++;
        } else if (inFoil && motorOff && !inFall && gapCount < RA_FOIL_GAP_MAX) {
            // campione di glide/gap: teniamo aperta la sessione e contiamo il secondo
            gapCount++;
            waveTime++;
        } else {
            // chiusura definitiva: motore acceso, caduta, o gap troppo lungo
            if (inFoil) {
                if (waveTime >= minSurfSec) {
                    totalFoilTime += waveTime;   // il tempo conta sempre
                    if (waveTime <= RA_SURF_MAX_SEC) {
                        foilN++;
                        foils.push({ number: foilN, duration: waveTime, start: startFoil, end: d.time });
                        if (waveTime > waveBestTime) waveBestTime = waveTime;
                    }
                }
                inFoil = false; waveTime = 0; gapCount = 0;
            }
        }
    }

    // Chiudi sessione foil aperta a fine file
    if (inFoil) {
        if (waveTime >= minSurfSec) {
            totalFoilTime += waveTime;   // il tempo conta sempre
            if (waveTime <= RA_SURF_MAX_SEC) {
                foilN++;
                foils.push({ number: foilN, duration: waveTime, start: startFoil, end: "EOF" });
                if (waveTime > waveBestTime) waveBestTime = waveTime;
            }
        }
    }

    return {
        takeoffs,
        foils,
        falls,
        waveBestTime,
        totalFoilTime,
        longestTakeoff,
        totalCount: records.length,
        records,    // esposto per il grafico (ride.html lo usa)
    };
}

// ============================================================
//  fetchAndAnalyze(fileIndex, hasAdxl)
//  Scarica vescDat<fileIndex>.bin, lo parsa e restituisce
//  Promise<result>  (stessa interfaccia di prima)
// ============================================================
async function fetchAndAnalyze(fileIndex, hasAdxl = true, minSurfSec = RA_SURF_MIN_SEC) {
    await refreshTakeoffWatts();   // soglia dinamica aggiornata prima dell'analisi
    const url      = `/vescbin?number=${fileIndex}`;
    const response = await fetch(url);
    if (!response.ok) throw new Error(`vescDat${fileIndex}.bin not found (HTTP ${response.status})`);
    const buffer  = await response.arrayBuffer();
    if (buffer.byteLength === 0) throw new Error(`vescDat${fileIndex}.bin è vuoto`);
    if (buffer.byteLength % VESC_RECORD_SIZE !== 0) {
        console.warn(`vescDat${fileIndex}.bin: dimensione anomala (${buffer.byteLength} byte, attesi multipli di ${VESC_RECORD_SIZE})`);
    }
    const records = parseVESCBinary(buffer);
    return analyzeRideRecords(records, hasAdxl, minSurfSec);
}

// ============================================================
//  Funzione di compatibilità: se qualche pagina chiama ancora
//  analyzeRideText() con il vecchio CSV, continua a funzionare.
// ============================================================
function analyzeRideText(csvText, hasAdxl = true) {
    const lines = csvText.split("\n");
    const records = [];
    for (const raw of lines) {
        const p = raw.trim().split(",");
        if (p.length < 12) continue;
        records.push({
            time:     p[0],
            rpm:      parseInt(p[1])    || 0,
            voltage:  parseFloat(p[2])  || 0,
            current:  parseFloat(p[3])  || 0,
            watts:    parseInt(p[4])    || 0,
            throttle: parseInt(p[5])    || 0,
            battery:  parseInt(p[6])    || 0,
            wh:       parseInt(p[7])    || 0,
            temp:     parseFloat(p[8])  || 0,
            powerAvg: parseInt(p[9])    || 0,
            rms:      parseFloat(p[10]) || 0,
            peak:     parseFloat(p[11]) || 0,
            tilt:     p.length >= 13 ? (parseFloat(p[12]) || 0) : 0,
            pitch:    p.length >= 14 ? (parseFloat(p[13]) || 0) : 0,
            roll:     p.length >= 15 ? (parseFloat(p[14]) || 0) : 0,
        });
    }
    return analyzeRideRecords(records, hasAdxl);
}
