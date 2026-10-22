import React, { useState, useEffect, useRef } from 'react';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Tooltip
} from 'chart.js';
import { Line } from 'react-chartjs-2';
import { Heart, Activity, Thermometer, ShieldAlert, Volume2, VolumeX, AlertTriangle } from 'lucide-react';

ChartJS.register(CategoryScale, LinearScale, PointElement, LineElement, Tooltip);

export default function ClinicianView({ vitals }) {
  const [thresholds, setThresholds] = useState({
    minSpO2: 92,
    maxHR: 110,
    minHR: 50,
    maxTemp: 37.8
  });

  const [history, setHistory] = useState({
    labels: [],
    hr: [],
    spo2: [],
    temp: [],
    motionMag: []
  });

  const [alerts, setAlerts] = useState([]);
  const [soundEnabled, setSoundEnabled] = useState(true);
  const [isAlarmActive, setIsAlarmActive] = useState(false);
  const audioCtxRef = useRef(null);

  const triggerAlarmTone = () => {
    if (!soundEnabled) return;
    try {
      if (!audioCtxRef.current) {
        audioCtxRef.current = new (window.AudioContext || window.webkitAudioContext)();
      }
      const ctx = audioCtxRef.current;
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();

      osc.type = 'sawtooth';
      osc.frequency.setValueAtTime(880, ctx.currentTime);
      gain.gain.setValueAtTime(0.12, ctx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.0001, ctx.currentTime + 0.35);

      osc.connect(gain);
      gain.connect(ctx.destination);
      osc.start();
      osc.stop(ctx.currentTime + 0.35);
    } catch (e) {
      console.warn("Audio blocked by browser policy until page interaction", e);
    }
  };

  useEffect(() => {
    const timestamp = new Date().toLocaleTimeString();

    setHistory((prev) => ({
      labels: [...prev.labels, timestamp].slice(-20),
      hr: [...prev.hr, vitals.fingerDetected ? vitals.heartRate : null].slice(-20),
      spo2: [...prev.spo2, vitals.fingerDetected ? vitals.spo2 : null].slice(-20),
      temp: [...prev.temp, vitals.temp_c].slice(-20),
      motionMag: [...prev.motionMag, vitals.accel_mag || 9.81].slice(-20)
    }));

    let violated = false;
    let reasons = [];

    if (vitals.fingerDetected && vitals.spo2 > 0 && vitals.spo2 < thresholds.minSpO2) {
      violated = true;
      reasons.push(`SpO2 ${vitals.spo2}% (<${thresholds.minSpO2}%)`);
    }
    if (vitals.fingerDetected && vitals.heartRate > thresholds.maxHR) {
      violated = true;
      reasons.push(`HR ${vitals.heartRate} BPM (>${thresholds.maxHR})`);
    }
    if (vitals.fingerDetected && vitals.heartRate > 0 && vitals.heartRate < thresholds.minHR) {
      violated = true;
      reasons.push(`HR ${vitals.heartRate} BPM (<${thresholds.minHR})`);
    }
    if (vitals.temp_c > thresholds.maxTemp) {
      violated = true;
      reasons.push(`Temp ${vitals.temp_c}°C (>${thresholds.maxTemp}°C)`);
    }
    if (vitals.motion === 'FALL_DETECTED') {
      violated = true;
      reasons.push('Patient Fall Detected');
    }

    if (violated) {
      setIsAlarmActive(true);
      triggerAlarmTone();
      setAlerts((prev) => [
        { time: timestamp, message: reasons.join(" • ") },
        ...prev.slice(0, 15)
      ]);
    } else {
      setIsAlarmActive(false);
    }
  }, [vitals]);

  const makeChartData = (label, dataSeries, color) => ({
    labels: history.labels,
    datasets: [
      {
        label,
        data: dataSeries,
        borderColor: color,
        backgroundColor: color,
        tension: 0.35,
        pointRadius: 2,
        borderWidth: 2
      }
    ]
  });

  const chartOpts = {
    responsive: true,
    maintainAspectRatio: false,
    animation: false,
    scales: {
      x: { display: false },
      y: {
        grid: { color: 'rgba(255, 255, 255, 0.05)' },
        ticks: { color: '#94a3b8', font: { size: 10 } }
      }
    }
  };

  return (
    <div className="space-y-6">
      {isAlarmActive && (
        <div className="bg-rose-600 text-white p-4 rounded-xl flex items-center justify-between shadow-xl animate-pulse">
          <div className="flex items-center gap-3">
            <AlertTriangle className="w-6 h-6 shrink-0" />
            <div>
              <p className="font-bold text-sm">TELEMETRY THRESHOLD BREACH: PATIENT P-101</p>
              <p className="text-xs opacity-90">{alerts[0]?.message}</p>
            </div>
          </div>
          <button
            onClick={() => setIsAlarmActive(false)}
            className="px-3 py-1 bg-white text-rose-700 font-bold rounded text-xs"
          >
            Acknowledge
          </button>
        </div>
      )}

      {!vitals.fingerDetected && (
        <div className="bg-amber-950/40 border border-amber-500 text-amber-300 p-3 rounded-xl flex items-center gap-3">
          <AlertTriangle className="w-5 h-5 shrink-0" />
          <div>
            <p className="font-bold text-sm">NO FINGER DETECTED ON HEART-RATE SENSOR</p>
            <p className="text-xs opacity-90">Heart rate and SpO2 are unavailable until the patient's finger is placed on the MAX30102 sensor.</p>
          </div>
        </div>
      )}

      <div className="flex flex-wrap justify-between items-center bg-slate-900 border border-slate-800 p-4 rounded-xl gap-4">
        <div>
          <h2 className="text-lg font-bold">Central Clinical Monitor</h2>
          <p className="text-xs text-slate-400">Device ID: CSENSE-001 • Patient: K. G. Perera (P-101)</p>
        </div>
        <button
          onClick={() => setSoundEnabled(!soundEnabled)}
          className="flex items-center gap-2 px-3 py-1.5 bg-slate-800 hover:bg-slate-700 rounded-lg text-xs font-medium"
        >
          {soundEnabled ? <Volume2 className="w-4 h-4 text-emerald-400" /> : <VolumeX className="w-4 h-4 text-rose-400" />}
          {soundEnabled ? 'Audio Alert Active' : 'Audio Alert Muted'}
        </button>
      </div>

      <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl">
          <div className="flex justify-between text-slate-400 text-xs">
            <span>Heart Rate</span>
            <Heart className="w-4 h-4 text-rose-500" />
          </div>
          <p className="text-3xl font-black text-rose-400 mt-2">{vitals.fingerDetected ? vitals.heartRate : '--'} <span className="text-xs font-normal text-slate-500">BPM</span></p>
        </div>

        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl">
          <div className="flex justify-between text-slate-400 text-xs">
            <span>SpO2 Saturation</span>
            <Activity className="w-4 h-4 text-sky-500" />
          </div>
          <p className="text-3xl font-black text-sky-400 mt-2">{vitals.fingerDetected ? vitals.spo2 : '--'} <span className="text-xs font-normal text-slate-500">%</span></p>
        </div>

        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl">
          <div className="flex justify-between text-slate-400 text-xs">
            <span>Body Temp</span>
            <Thermometer className="w-4 h-4 text-amber-500" />
          </div>
          <p className="text-3xl font-black text-amber-400 mt-2">{vitals.temp_c}° <span className="text-xs font-normal text-slate-500">C</span></p>
        </div>

        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl">
          <div className="flex justify-between text-slate-400 text-xs">
            <span>Activity Status</span>
            <ShieldAlert className="w-4 h-4 text-emerald-500" />
          </div>
          <p className="text-xl font-bold text-emerald-400 mt-3 truncate">{vitals.motion}</p>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl">
          <p className="text-xs font-bold text-rose-400 mb-2">Heart Rate Trend (BPM)</p>
          <div className="h-32">
            <Line data={makeChartData('HR', history.hr, '#f43f5e')} options={chartOpts} />
          </div>
        </div>

        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl">
          <p className="text-xs font-bold text-sky-400 mb-2">Oxygen Saturation Trend (% SpO2)</p>
          <div className="h-32">
            <Line data={makeChartData('SpO2', history.spo2, '#38bdf8')} options={chartOpts} />
          </div>
        </div>

        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl">
          <p className="text-xs font-bold text-amber-400 mb-2">Surface Temp Trend (°C)</p>
          <div className="h-32">
            <Line data={makeChartData('Temp', history.temp, '#fbbf24')} options={chartOpts} />
          </div>
        </div>

        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl">
          <p className="text-xs font-bold text-emerald-400 mb-2">Accelerometer Magnitude (m/s²)</p>
          <div className="h-32">
            <Line data={makeChartData('Motion', history.motionMag, '#34d399')} options={chartOpts} />
          </div>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl">
          <h4 className="text-xs font-bold text-teal-400 mb-3 uppercase tracking-wider">Clinical Thresholds</h4>
          <div className="grid grid-cols-2 gap-3 text-xs">
            <div>
              <label className="text-slate-400 block mb-1">Min SpO2 (%)</label>
              <input
                type="number"
                value={thresholds.minSpO2}
                onChange={(e) => setThresholds({ ...thresholds, minSpO2: Number(e.target.value) })}
                className="w-full bg-slate-800 border border-slate-700 rounded p-1.5 text-white"
              />
            </div>
            <div>
              <label className="text-slate-400 block mb-1">Max Heart Rate (BPM)</label>
              <input
                type="number"
                value={thresholds.maxHR}
                onChange={(e) => setThresholds({ ...thresholds, maxHR: Number(e.target.value) })}
                className="w-full bg-slate-800 border border-slate-700 rounded p-1.5 text-white"
              />
            </div>
            <div>
              <label className="text-slate-400 block mb-1">Min Heart Rate (BPM)</label>
              <input
                type="number"
                value={thresholds.minHR}
                onChange={(e) => setThresholds({ ...thresholds, minHR: Number(e.target.value) })}
                className="w-full bg-slate-800 border border-slate-700 rounded p-1.5 text-white"
              />
            </div>
            <div>
              <label className="text-slate-400 block mb-1">Max Temp (°C)</label>
              <input
                type="number"
                step="0.1"
                value={thresholds.maxTemp}
                onChange={(e) => setThresholds({ ...thresholds, maxTemp: Number(e.target.value) })}
                className="w-full bg-slate-800 border border-slate-700 rounded p-1.5 text-white"
              />
            </div>
          </div>
        </div>

        <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl flex flex-col justify-between">
          <div className="flex justify-between items-center mb-2">
            <h4 className="text-xs font-bold text-rose-400 uppercase tracking-wider">Breach Incident Log</h4>
            <span className="text-[10px] text-slate-500">{alerts.length} logged</span>
          </div>
          <div className="h-28 overflow-y-auto space-y-1 text-xs font-mono">
            {alerts.length === 0 ? (
              <p className="text-slate-600 text-center py-6 text-xs">All parameters within acceptable limits.</p>
            ) : (
              alerts.map((item, index) => (
                <div key={index} className="flex justify-between bg-slate-950 p-1.5 rounded border border-slate-800 text-[11px]">
                  <span className="text-rose-400">{item.message}</span>
                  <span className="text-slate-500">{item.time}</span>
                </div>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
