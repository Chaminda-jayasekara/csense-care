import React, { useState } from 'react';
import { Heart, Activity, Thermometer, ShieldAlert, PhoneCall } from 'lucide-react';

const TRANSLATIONS = {
  en: {
    title: "Patient Daily Status",
    stable: "STABLE",
    warning: "ATTENTION NEEDED",
    critical: "CRITICAL ALERT",
    hr: "Heart Rate",
    spo2: "Oxygen Level",
    temp: "Body Temp",
    motion: "Movement Status",
    callAmbulance: "Call 1990 Suwa Seriya",
    patchActive: "Csense Chest Patch Active",
    noFinger: "No finger on the heart sensor. Please place a finger on it.",
    noReading: "NO HEART READING"
  },
  si: {
    title: "දෛනික රෝගී තත්ත්වය",
    stable: "සාමාන්‍යයි",
    warning: "අවධානය අවශ්‍යයි",
    critical: "හදිසි අනතුරු ඇඟවීමයි",
    hr: "හදවත් ස්පන්දනය",
    spo2: "ඔක්සිජන් මට්ටම",
    temp: "ශරීර උෂ්ණත්වය",
    motion: "චලනය",
    callAmbulance: "1990 සුවසැරිය අමතන්න",
    patchActive: "පැච් උපකරණය සක්‍රියයි",
    noFinger: "හෘද සංවේදකය මත ඇඟිල්ලක් නැත. කරුණාකර ඇඟිල්ල තබන්න.",
    noReading: "හෘද කියවීම නැත"
  },
  ta: {
    title: "நோயாளி தினசரி நிலை",
    stable: "வழமை",
    warning: "கவனம் தேவை",
    critical: "அவசர எச்சரிக்கை",
    hr: "இதய துடிப்பு",
    spo2: "ஒக்சிசன் அளவு",
    temp: "உடல் வெப்பநிலை",
    motion: "அசைவு",
    callAmbulance: "1990 சுவசரியை அழைக்கவும்",
    patchActive: "கருவி இணைக்கப்பட்டுள்ளது",
    noFinger: "இதய சென்சாரில் விரல் இல்லை. தயவுசெய்து விரலை வைக்கவும்.",
    noReading: "இதய அளவீடு இல்லை"
  }
};

export default function CaregiverView({ vitals }) {
  const [lang, setLang] = useState('en');
  const t = TRANSLATIONS[lang];

  let statusClass = "bg-emerald-600/20 border-emerald-500 text-emerald-400";
  let statusBadge = "bg-emerald-500 text-white";
  let statusText = t.stable;

  const isCritical = (vitals.spo2 > 0 && vitals.spo2 < 90) || 
                     vitals.heartRate > 120 || 
                     vitals.temp_c > 38.5 || 
                     vitals.motion === 'FALL_DETECTED';

  const isWarning = (vitals.spo2 >= 90 && vitals.spo2 < 94) || 
                    (vitals.heartRate > 100 && vitals.heartRate <= 120) || 
                    (vitals.temp_c > 37.5 && vitals.temp_c <= 38.5);

  if (isCritical) {
    statusClass = "bg-rose-950/40 border-rose-500 text-rose-400 animate-pulse";
    statusBadge = "bg-rose-600 text-white";
    statusText = t.critical;
  } else if (isWarning) {
    statusClass = "bg-amber-950/30 border-amber-500 text-amber-400";
    statusBadge = "bg-amber-500 text-slate-900";
    statusText = t.warning;
  } else if (!vitals.fingerDetected) {
    statusClass = "bg-slate-800/60 border-slate-500 text-slate-300";
    statusBadge = "bg-slate-600 text-white";
    statusText = t.noReading;
  }

  return (
    <div className="max-w-md mx-auto bg-slate-900 border border-slate-800 rounded-3xl p-5 shadow-2xl flex flex-col justify-between min-h-[580px]">
      <div>
        <div className="flex justify-between items-center mb-6">
          <span className="text-xs font-semibold text-teal-400">Caregiver View</span>
          <div className="flex gap-1 bg-slate-800 p-1 rounded-lg">
            {['en', 'si', 'ta'].map((l) => (
              <button
                key={l}
                onClick={() => setLang(l)}
                className={`px-2.5 py-0.5 rounded text-xs font-bold transition-all ${
                  lang === l ? 'bg-teal-600 text-white' : 'text-slate-400 hover:text-white'
                }`}
              >
                {l.toUpperCase()}
              </button>
            ))}
          </div>
        </div>

        <div className={`p-6 rounded-2xl border-2 text-center transition-all ${statusClass}`}>
          <p className="text-xs font-medium uppercase tracking-wider mb-2">{t.title}</p>
          <div className={`inline-block px-4 py-1.5 rounded-full font-black text-xl tracking-wide shadow-md ${statusBadge}`}>
            {statusText}
          </div>
          <p className="text-[11px] mt-3 opacity-80">{t.patchActive}</p>
        </div>

        {!vitals.fingerDetected && (
          <div className="mt-4 p-3 rounded-xl border border-amber-500 bg-amber-950/30 text-amber-300 text-xs font-medium text-center">
            {t.noFinger}
          </div>
        )}

        <div className="grid grid-cols-2 gap-3 mt-4">
          <div className="bg-slate-800/80 p-3 rounded-xl border border-slate-700">
            <div className="flex items-center gap-1.5 text-slate-400 text-xs mb-1">
              <Heart className="w-3.5 h-3.5 text-rose-400" />
              <span>{t.hr}</span>
            </div>
            <p className="text-2xl font-black text-rose-400">
              {vitals.fingerDetected ? vitals.heartRate : '--'} <span className="text-xs font-normal text-slate-400">BPM</span>
            </p>
          </div>

          <div className="bg-slate-800/80 p-3 rounded-xl border border-slate-700">
            <div className="flex items-center gap-1.5 text-slate-400 text-xs mb-1">
              <Activity className="w-3.5 h-3.5 text-sky-400" />
              <span>{t.spo2}</span>
            </div>
            <p className="text-2xl font-black text-sky-400">
              {vitals.fingerDetected ? vitals.spo2 : '--'} <span className="text-xs font-normal text-slate-400">%</span>
            </p>
          </div>

          <div className="bg-slate-800/80 p-3 rounded-xl border border-slate-700">
            <div className="flex items-center gap-1.5 text-slate-400 text-xs mb-1">
              <Thermometer className="w-3.5 h-3.5 text-amber-400" />
              <span>{t.temp}</span>
            </div>
            <p className="text-2xl font-black text-amber-400">
              {vitals.temp_c}° <span className="text-xs font-normal text-slate-400">C</span>
            </p>
          </div>

          <div className="bg-slate-800/80 p-3 rounded-xl border border-slate-700">
            <div className="flex items-center gap-1.5 text-slate-400 text-xs mb-1">
              <ShieldAlert className="w-3.5 h-3.5 text-emerald-400" />
              <span>{t.motion}</span>
            </div>
            <p className="text-sm font-bold text-emerald-400 mt-2 truncate">
              {vitals.motion}
            </p>
          </div>
        </div>
      </div>

      <a
        href="tel:1990"
        className="w-full py-3.5 bg-rose-600 hover:bg-rose-700 text-white font-bold rounded-xl flex items-center justify-center gap-2 mt-6 shadow-lg shadow-rose-950/50 transition-all text-sm"
      >
        <PhoneCall className="w-4 h-4" />
        <span>{t.callAmbulance}</span>
      </a>
    </div>
  );
}
