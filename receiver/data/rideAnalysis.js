// ============================================================
//  rideAnalysis.js  –  TecnoFly shared analysis module
//  Algoritmo v2 – usa tilt, roll, pitch, rms, peak, throttle, watts
//  Usato da: ride.html (dettaglio singola sessione)
//            index.html (sintesi tutte le sessioni)
// ============================================================

// ---- Soglie takeoff ----
const RA_TAKEOFF_WATTS     = 1800;   // W
const RA_TAKEOFF_MIN_SAMP  = 3;      // campioni consecutivi

// ---- Soglie foil / surf ----
const RA_FOIL_RMS_MIN      = 0.18;   // g
const RA_FOIL_PEAK_MIN     = 0.30;   // g
const RA_FOIL_TILT_MAX     = 25.0;   // °
const RA_FOIL_ROLL_MAX     = 20.0;   // °
const RA_FOIL_PITCH_MAX    = 20.0;   // °

// ---- Rilevamento caduta ----
const RA_FALL_TILT_THR     = 28.0;   // °
const RA_FALL_ROLL_THR     = 25.0;   // °
const RA_FALL_PITCH_THR    = 25.0;   // °
const RA_FALL_RMS_LOW      = 0.12;   // g – fermo in acqua
const RA_FALL_CONFIRM      = 2;      // campioni per confermare caduta

// ---- Durata sessione foil ----
const RA_SURF_MIN_SEC      = 6;
const RA_SURF_MAX_SEC      = 180;

// ============================================================
//  parseVESCLine(line, hasAdxl)
//  Ritorna un oggetto con tutti i campi, o null se la riga non è valida.
//  Se l'accelerometro non è presente (hasAdxl=false) tilt/pitch/roll
//  restano a 0 e le soglie angolari vengono ignorate.
// ============================================================
function parseVESCLine(line) {
    const p = line.trim().split(",");
    if (p.length < 12) return null;

    const obj = {
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
    };
    if (isNaN(obj.watts) || isNaN(obj.throttle)) return null;
    return obj;
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
    return (Math.abs(d.tilt)  > RA_FALL_TILT_THR ||
            Math.abs(d.roll)  > RA_FALL_ROLL_THR  ||
            Math.abs(d.pitch) > RA_FALL_PITCH_THR ||
            d.rms < RA_FALL_RMS_LOW);
}

function _timeDiff(t1, t2) {
    const [m1, s1] = t1.split(":").map(Number);
    const [m2, s2] = t2.split(":").map(Number);
    return Math.abs((m1 * 60 + s1) - (m2 * 60 + s2));
}

// ============================================================
//  analyzeRideText(csvText, hasAdxl)
//
//  Elabora il testo CSV di un file vescDat e restituisce:
//  {
//    takeoffs: [ {number, duration, start, end}, … ],
//    foils:    [ {number, duration, start, end}, … ],
//    falls:    [ {time, tilt, pitch, roll, rms}, … ],
//    waveBestTime,   // secondi
//    totalCount      // righe valide
//  }
// ============================================================
function analyzeRideText(csvText, hasAdxl = true) {
    const lines = csvText.split("\n");

    const takeoffs = [], foils = [], falls = [];

    let consecHP  = 0, waveTime = 0, fallSamp = 0;
    let inTakeoff = false, inFoil = false, inFall = false, afterTakeoff = false;
    let takeoffN  = 0, foilN = 0;
    let startTakeoff = "", startFoil = "";
    let waveBestTime = 0, totalCount = 0, longestTakeoff = 0;

    for (const raw of lines) {
        if (!raw.trim()) continue;
        const d = parseVESCLine(raw);
        if (!d) continue;

        // ── 1. Rilevamento caduta ──────────────────────────────
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

        // ── 2. Takeoff ─────────────────────────────────────────
        const highPower = d.watts > RA_TAKEOFF_WATTS && d.throttle > 10;
        if (highPower) {
            if (!inTakeoff) startTakeoff = d.time;
            consecHP++;
            inTakeoff    = true;
            afterTakeoff = true;
            // nuovo takeoff interrompe eventuale foil aperta
            if (inFoil) {
                const dur = _timeDiff(startFoil, d.time);
                if (!inFall && dur >= RA_SURF_MIN_SEC && dur <= RA_SURF_MAX_SEC) {
                    foilN++;
                    foils.push({ number: foilN, duration: dur, start: startFoil, end: d.time });
                    if (dur > waveBestTime) waveBestTime = dur;
                }
                inFoil = false; waveTime = 0;
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

        // ── 3. Foil / surf ─────────────────────────────────────
        const motorOff = d.throttle <= 5 && d.watts < 50;
        const surfing  = afterTakeoff && motorOff && _isFoilRiding(d, hasAdxl) && !inFall;

        if (surfing) {
            if (!inFoil) { startFoil = d.time; waveTime = 0; }
            inFoil = true;
            waveTime++;
        } else {
            if (inFoil) {
                const dur = _timeDiff(startFoil, d.time);
                if (!inFall && dur >= RA_SURF_MIN_SEC && dur <= RA_SURF_MAX_SEC) {
                    foilN++;
                    foils.push({ number: foilN, duration: dur, start: startFoil, end: d.time });
                    if (dur > waveBestTime) waveBestTime = dur;
                }
                inFoil = false; waveTime = 0;
            }
        }

        totalCount++;
    }

    // Chiudi foil aperta a fine file
    if (inFoil && !inFall) {
        const dur = waveTime;
        if (dur >= RA_SURF_MIN_SEC && dur <= RA_SURF_MAX_SEC) {
            foilN++;
            foils.push({ number: foilN, duration: dur, start: startFoil, end: "EOF" });
            if (dur > waveBestTime) waveBestTime = dur;
        }
    }

    return { takeoffs, foils, falls, waveBestTime, longestTakeoff, totalCount };
}

// ============================================================
//  fetchAndAnalyze(fileIndex, hasAdxl)
//  Scarica vescDat<fileIndex>.txt e restituisce Promise<result>
// ============================================================
async function fetchAndAnalyze(fileIndex, hasAdxl = true) {
    const fileName = `vescDat${fileIndex}.txt`;
    const response = await fetch(fileName);
    if (!response.ok) throw new Error(`${fileName} not found`);
    const text = await response.text();
    return analyzeRideText(text, hasAdxl);
}
