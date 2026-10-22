import React, { useEffect, useState } from 'react';
import { supabase } from './supabaseClient';
import CaregiverView from './CaregiverView';
import ClinicianView from './ClinicianView';

export default function App() {
  const [activeTab, setActiveTab] = useState('clinician');
  const [vitals, setVitals] = useState({
    heartRate: 74,
    spo2: 98,
    temp_c: 36.6,
    motion: 'RESTING',
    accel_mag: 9.81,
    fingerDetected: true
  });

  useEffect(() => {
    // 1. Fetch initial latest reading
    supabase
      .from('patient_vitals')
      .select('*')
      .order('id', { ascending: false })
      .limit(1)
      .then(({ data }) => {
        if (data && data.length > 0) {
          setVitals({
            heartRate: data[0].heart_rate,
            spo2: data[0].spo2,
            temp_c: data[0].temp_c,
            motion: data[0].motion,
            accel_mag: data[0].accel_mag,
            fingerDetected: data[0].finger_detected !== false
          });
        }
      });

    // 2. Real-time postgres subscriber
    const subscription = supabase
      .channel('vitals-realtime')
      .on(
        'postgres_changes',
        { event: 'INSERT', schema: 'public', table: 'patient_vitals' },
        (payload) => {
          const row = payload.new;
          setVitals({
            heartRate: row.heart_rate,
            spo2: row.spo2,
            temp_c: row.temp_c,
            motion: row.motion,
            accel_mag: row.accel_mag,
            fingerDetected: row.finger_detected !== false
          });
        }
      )
      .subscribe();

    return () => {
      supabase.removeChannel(subscription);
    };
  }, []);

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col">
      <header className="px-6 py-3 bg-slate-900 border-b border-slate-800 flex flex-wrap justify-between items-center gap-3">
        <div className="flex items-center gap-2">
          <div className="w-3 h-3 rounded-full bg-teal-400 animate-pulse"></div>
          <span className="font-bold text-teal-400 text-lg">Csense Care</span>
          <span className="text-xs bg-slate-800 px-2 py-0.5 rounded text-slate-400">Post-Discharge Hub</span>
        </div>

        <div className="flex bg-slate-800 p-1 rounded-lg">
          <button
            onClick={() => setActiveTab('clinician')}
            className={`px-3 py-1.5 rounded-md text-xs font-semibold transition-all ${
              activeTab === 'clinician' ? 'bg-teal-600 text-white shadow' : 'text-slate-400 hover:text-white'
            }`}
          >
            Clinician Dashboard (Desktop)
          </button>
          <button
            onClick={() => setActiveTab('caregiver')}
            className={`px-3 py-1.5 rounded-md text-xs font-semibold transition-all ${
              activeTab === 'caregiver' ? 'bg-teal-600 text-white shadow' : 'text-slate-400 hover:text-white'
            }`}
          >
            Caregiver App (Mobile)
          </button>
        </div>
      </header>

      <main className="flex-1 p-4 md:p-6 max-w-7xl mx-auto w-full">
        {activeTab === 'clinician' ? (
          <ClinicianView vitals={vitals} />
        ) : (
          <CaregiverView vitals={vitals} />
        )}
      </main>
    </div>
  );
}
