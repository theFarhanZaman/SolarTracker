import { useState, useEffect, useRef, useMemo, useCallback } from 'react';
import {
  LayoutDashboard, BarChart3, Sliders, Terminal,
  Activity, Zap, Thermometer, Droplets, Gauge,
  Radio, Cpu, RefreshCw,
  TrendingUp, CheckCircle2
} from 'lucide-react';
import {
  ResponsiveContainer, AreaChart, Area, XAxis, YAxis,
  CartesianGrid, Tooltip, LineChart, Line, Legend,
  BarChart, Bar, ComposedChart
} from 'recharts';

// Strict Type Contracts for Edge/Gateway Infrastructure
interface TelemetryFrame {
  Timestamp: string;
  Voltage: number;
  Current: number;
  Temp: number;
  Humidity: number;
  Pressure: number;
  Power_W: number;
}

interface AIInsights {
  State: string;
  Rain_Prob: number;
  High: number;
  Low: number;
}

interface LogEntry {
  id: string;
  timestamp: string;
  type: 'info' | 'success' | 'warn' | 'critical';
  message: string;
}

type TabKey = 'dashboard' | 'analytics' | 'controls' | 'logs';

const MAX_HISTORY = 40;
const MAX_LOGS = 100;
const SIM_INTERVAL_MS = 1000;
const RECONNECT_BASE_MS = 4000;
const RECONNECT_MAX_MS = 15000;

const formatClock = (date = new Date()) =>
  date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

const clamp = (value: number, min: number, max: number) => Math.min(max, Math.max(min, value));

const rand = (min: number, max: number) => min + Math.random() * (max - min);

const round = (value: number, digits = 3) => Number(value.toFixed(digits));

const createFrame = (
  timestamp: string,
  Voltage: number,
  Current: number,
  Temp: number,
  Humidity: number,
  Pressure: number
): TelemetryFrame => ({
  Timestamp: timestamp,
  Voltage: round(Voltage, 2),
  Current: round(Current, 3),
  Temp: round(Temp, 1),
  Humidity: round(Humidity, 1),
  Pressure: round(Pressure, 1),
  Power_W: round(Voltage * Current, 3)
});

const createSeedHistory = (): TelemetryFrame[] => {
  const seedData: TelemetryFrame[] = [];
  const now = new Date();

  for (let i = MAX_HISTORY; i > 0; i--) {
    const pastTime = new Date(now.getTime() - i * 4000);
    const timeStr = formatClock(pastTime);
    const voltage = 18.5 + Math.random() * 2.5;
    const current = 1.2 + Math.random() * 0.8;
    seedData.push(
      createFrame(
        timeStr,
        voltage,
        current,
        32.0 + Math.random() * 4.5,
        65.0 + Math.random() * 10,
        1008.0 + Math.random() * 4
      )
    );
  }

  return seedData;
};

const createSeedInsights = (telemetry: TelemetryFrame): AIInsights => ({
  State: 'STABLE_TRACKING',
  Rain_Prob: 12,
  High: round(Math.max(telemetry.Temp + 10.4, 42.4), 1),
  Low: round(Math.min(telemetry.Temp - 12.2, 19.8), 1)
});

const normalizeTelemetryPayload = (rawTel: any): TelemetryFrame => {
  const voltage = Number(rawTel?.voltage ?? rawTel?.Voltage ?? rawTel?.v ?? 0);
  const current = Number(rawTel?.current ?? rawTel?.Current ?? rawTel?.c ?? 0);
  const temp = Number(rawTel?.temp ?? rawTel?.Temp ?? rawTel?.temperature ?? 0);
  const humidity = Number(rawTel?.humidity ?? rawTel?.Humidity ?? 0);
  const pressure = Number(rawTel?.pressure ?? rawTel?.Pressure ?? 0);

  return createFrame(formatClock(), voltage, current, temp, humidity, pressure);
};

const createSyntheticStep = (previous: TelemetryFrame, tick: number): TelemetryFrame => {
  const trendPhase = tick / 12;
  const driftPhase = tick / 27;

  const targetVoltage = 19.8 + Math.sin(trendPhase) * 0.45 + Math.cos(driftPhase) * 0.15;
  const targetCurrent = 1.55 + Math.cos(trendPhase * 0.92) * 0.18 + Math.sin(driftPhase * 0.7) * 0.05;
  const targetTemp = 34.5 + Math.sin(tick / 18) * 1.4 + Math.cos(tick / 41) * 0.5;
  const targetHumidity = 68.0 + Math.cos(tick / 22) * 2.8 + Math.sin(tick / 37) * 1.2;
  const targetPressure = 1009.5 + Math.sin(tick / 26) * 0.75 + Math.cos(tick / 19) * 0.35;

  const voltage = clamp(previous.Voltage + (targetVoltage - previous.Voltage) * 0.18 + rand(-0.03, 0.03), 17.9, 22.8);
  const current = clamp(previous.Current + (targetCurrent - previous.Current) * 0.22 + rand(-0.012, 0.012), 0.8, 2.25);
  const temp = clamp(previous.Temp + (targetTemp - previous.Temp) * 0.14 + rand(-0.08, 0.08), 29, 45);
  const humidity = clamp(previous.Humidity + (targetHumidity - previous.Humidity) * 0.1 + rand(-0.16, 0.16), 55, 82);
  const pressure = clamp(previous.Pressure + (targetPressure - previous.Pressure) * 0.12 + rand(-0.06, 0.06), 1004, 1015);

  return createFrame(formatClock(), voltage, current, temp, humidity, pressure);
};

const deriveSyntheticInsights = (telemetry: TelemetryFrame): AIInsights => {
  const humidityPressureScore = (telemetry.Humidity - 60) * 0.9 - (telemetry.Pressure - 1008) * 1.5;
  const heatScore = telemetry.Temp - 35.5;
  const powerScore = telemetry.Power_W - 31;

  let state = 'STABLE_TRACKING';
  if (heatScore > 4.2) {
    state = 'THERMAL_ADJUST';
  } else if (humidityPressureScore > 14) {
    state = 'WEATHER_RISK';
  } else if (powerScore > 6) {
    state = 'OPTIMIZING';
  } else if (powerScore < -4) {
    state = 'IDLE_RECOVERY';
  }

  const rainProb = clamp(Math.round(8 + (telemetry.Humidity - 58) * 1.35 + (1011 - telemetry.Pressure) * 2.2), 3, 92);
  const high = round(telemetry.Temp + clamp(7.5 + rand(0, 2.4), 7.5, 10.5), 1);
  const low = round(telemetry.Temp - clamp(11.0 + rand(0, 2.2), 11.0, 13.5), 1);

  return {
    State: state,
    Rain_Prob: rainProb,
    High: high,
    Low: low
  };
};

export default function App() {
  const [activeTab, setActiveTab] = useState<TabKey>('dashboard');
  const [status, setStatus] = useState<'Connecting' | 'Live' | 'Disconnected'>('Connecting');

  // Controls state definitions
  const [overrideMode, setOverrideMode] = useState<number>(0);
  const [panBias, setPanBias] = useState<number>(0);
  const [tiltBias, setTiltBias] = useState<number>(0);

  // Telemetry Pipeline Buffers
  const [currentData, setCurrentData] = useState<{ telemetry: TelemetryFrame; ai_insights: AIInsights }>({
    telemetry: { Timestamp: '--:--:--', Voltage: 0, Current: 0, Temp: 0, Humidity: 0, Pressure: 0, Power_W: 0 },
    ai_insights: { State: 'OPTIMIZING', Rain_Prob: 0, High: 0, Low: 0 }
  });
  const [history, setHistory] = useState<TelemetryFrame[]>([]);
  const [logs, setLogs] = useState<LogEntry[]>([]);

  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<number | null>(null);
  const reconnectAttemptRef = useRef(0);
  const simulatorIntervalRef = useRef<number | null>(null);
  const simulatorActiveRef = useRef(false);
  const simulatorTickRef = useRef(0);
  const lastTelemetryRef = useRef<TelemetryFrame>(currentData.telemetry);
  const isUnmountingRef = useRef(false);
  const socketGenerationRef = useRef(0);
  const connectGatewayRef = useRef<() => void>(() => undefined);

  // Unified Central Logging System
  const addLog = useCallback((type: LogEntry['type'], message: string) => {
    const timestamp = formatClock();
    setLogs(prev => [{ id: Math.random().toString(36).slice(2, 11), timestamp, type, message }, ...prev].slice(0, MAX_LOGS));
  }, []);

  const commitFrame = useCallback((frame: TelemetryFrame, aiInsights: AIInsights, source: 'ws' | 'simulation') => {
    lastTelemetryRef.current = frame;
    setCurrentData({ telemetry: frame, ai_insights: aiInsights });
    setHistory(prev => [...prev, frame].slice(-MAX_HISTORY));

    if (source === 'ws' && Math.random() > 0.9) {
      addLog('info', `Frame received. Generation Vector: ${frame.Power_W.toFixed(2)} W`);
    }
  }, [addLog]);

  const calculateMetrics = useCallback((series: TelemetryFrame[]) => {
    if (series.length === 0) {
      return { avgPower: 0, maxPower: 0, totalVoltageDelta: 0, peakCurrent: 0 };
    }

    const powers = series.map(h => h.Power_W);
    const currents = series.map(h => h.Current);
    const voltages = series.map(h => h.Voltage);

    const avgPower = powers.reduce((a, b) => a + b, 0) / series.length;
    const maxPower = Math.max(...powers);
    const peakCurrent = Math.max(...currents);
    const totalVoltageDelta = voltages.reduce((acc, value, idx) => {
      if (idx === 0) return acc;
      return acc + Math.abs(value - voltages[idx - 1]);
    }, 0);

    return { avgPower, maxPower, totalVoltageDelta, peakCurrent };
  }, []);

  // Seed initial dummy dataset history for cold-start visualization density
  useEffect(() => {
    const seedData = createSeedHistory();
    const seedTelemetry = seedData[seedData.length - 1];
    const seedInsights = createSeedInsights(seedTelemetry);

    simulatorTickRef.current = seedData.length;
    lastTelemetryRef.current = seedTelemetry;

    setHistory(seedData);
    setCurrentData({
      telemetry: seedTelemetry,
      ai_insights: seedInsights
    });

    addLog('info', 'Telemetry visualization matrix loaded successfully.');
  }, [addLog]);

  // Persistent Gateway Communications Pipeline Loop
  const scheduleReconnect = useCallback(() => {
    if (isUnmountingRef.current) return;
    if (reconnectTimeoutRef.current !== null) return;

    reconnectAttemptRef.current += 1;
    const delay = Math.min(RECONNECT_BASE_MS * reconnectAttemptRef.current, RECONNECT_MAX_MS);
    addLog('warn', `Gateway reconnect queued in ${Math.ceil(delay / 1000)}s (attempt ${reconnectAttemptRef.current}).`);

    reconnectTimeoutRef.current = window.setTimeout(() => {
      reconnectTimeoutRef.current = null;
      if (!isUnmountingRef.current) {
        connectGatewayRef.current();
      }
    }, delay);
  }, [addLog]);

  const connectGateway = useCallback(() => {
    if (isUnmountingRef.current) return;

    if (reconnectTimeoutRef.current !== null) {
      clearTimeout(reconnectTimeoutRef.current);
      reconnectTimeoutRef.current = null;
    }

    const generation = socketGenerationRef.current + 1;
    socketGenerationRef.current = generation;

    try {
      wsRef.current?.close();
    } catch {
      // Ignore shutdown races from previous sockets.
    }

    setStatus('Connecting');
    addLog('info', 'Initiating high-speed tracking pipeline downstream handshake...');

    const wsProtocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
    const wsUrl = `${wsProtocol}://${window.location.hostname}:8000/ws/telemetry`;
    const ws = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      if (isUnmountingRef.current || socketGenerationRef.current !== generation) return;
      reconnectAttemptRef.current = 0;
      setStatus('Live');
      addLog('success', 'Active operational tracking stream bound to gateway node.');
    };

    ws.onmessage = (event) => {
      if (isUnmountingRef.current || socketGenerationRef.current !== generation) return;

      try {
        const rawPayload = JSON.parse(event.data);

        if (rawPayload && rawPayload.telemetry) {
          const frame = normalizeTelemetryPayload(rawPayload.telemetry);
          const aiInsights = rawPayload.ai_insights || deriveSyntheticInsights(frame);
          commitFrame(frame, aiInsights, 'ws');
        }
      } catch (err) {
        addLog('critical', `Data serialization structural fault: ${String(err)}`);
      }
    };

    ws.onerror = () => {
      if (isUnmountingRef.current || socketGenerationRef.current !== generation) return;
      setStatus('Disconnected');
    };

    ws.onclose = () => {
      if (isUnmountingRef.current || socketGenerationRef.current !== generation) return;

      setStatus('Disconnected');
      addLog('warn', 'Gateway socket dropped. Activating automatic line recovery...');
      scheduleReconnect();
    };
  }, [addLog, commitFrame, scheduleReconnect]);

  useEffect(() => {
    connectGatewayRef.current = connectGateway;
  }, [connectGateway]);

  // Fallback Simulation Engine: keeps the charts alive whenever the socket is not OPEN.
  useEffect(() => {
    const socketOpen = wsRef.current?.readyState === WebSocket.OPEN;
    const shouldSimulate = !socketOpen;

    if (shouldSimulate) {
      if (simulatorIntervalRef.current === null) {
        if (!simulatorActiveRef.current) {
          addLog('warn', 'WebSocket not OPEN. Synthetic telemetry simulator engaged.');
          simulatorActiveRef.current = true;
        }

        simulatorIntervalRef.current = window.setInterval(() => {
          if (wsRef.current?.readyState === WebSocket.OPEN) {
            return;
          }

          simulatorTickRef.current += 1;
          const nextFrame = createSyntheticStep(lastTelemetryRef.current, simulatorTickRef.current);
          const nextInsights = deriveSyntheticInsights(nextFrame);
          commitFrame(nextFrame, nextInsights, 'simulation');
        }, SIM_INTERVAL_MS);
      }
    } else {
      if (simulatorIntervalRef.current !== null) {
        clearInterval(simulatorIntervalRef.current);
        simulatorIntervalRef.current = null;
      }

      if (simulatorActiveRef.current) {
        addLog('success', 'Live gateway stream restored. Synthetic simulator standing by.');
        simulatorActiveRef.current = false;
      }
    }

    return () => {
      if (simulatorIntervalRef.current !== null) {
        clearInterval(simulatorIntervalRef.current);
        simulatorIntervalRef.current = null;
      }
    };
  }, [status, addLog, commitFrame]);

  useEffect(() => {
    isUnmountingRef.current = false;
    connectGateway();

    return () => {
      isUnmountingRef.current = true;
      socketGenerationRef.current += 1;

      if (reconnectTimeoutRef.current !== null) {
        clearTimeout(reconnectTimeoutRef.current);
        reconnectTimeoutRef.current = null;
      }

      if (simulatorIntervalRef.current !== null) {
        clearInterval(simulatorIntervalRef.current);
        simulatorIntervalRef.current = null;
      }

      try {
        wsRef.current?.close();
      } catch {
        // Safe cleanup.
      }

      wsRef.current = null;
    };
  }, [connectGateway]);

  const metrics = useMemo(() => calculateMetrics(history), [calculateMetrics, history]);

  const transmitControlPacket = useCallback((updatedMode = overrideMode, pBias = panBias, tBias = tiltBias) => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      const packet = {
        command: 'HARDWARE_OVERRIDE_PACKET',
        structuralMode: updatedMode,
        appliedBiasPan: pBias,
        appliedBiasTilt: tBias
      };
      wsRef.current.send(JSON.stringify(packet));
      addLog('info', `Uplink packet sent -> Mode: ${updatedMode}, Pan: ${pBias}°, Tilt: ${tBias}°`);
    } else {
      addLog('warn', 'Uplink transmission rejected: Gateway line is offline.');
    }
  }, [addLog, overrideMode, panBias, tiltBias]);

  const navTabs = useMemo(() => ([
    { id: 'dashboard' as const, label: 'Command Grid', icon: LayoutDashboard },
    { id: 'analytics' as const, label: 'Data Analytics Deck', icon: BarChart3 },
    { id: 'controls' as const, label: 'Hardware Controls', icon: Sliders },
    { id: 'logs' as const, label: 'System Logs Feed', icon: Terminal }
  ]), []);

  const dashboardCards = useMemo(() => ([
    { title: 'Bus Voltage', val: `${currentData.telemetry.Voltage.toFixed(2)} V`, sub: 'Bus Input', icon: Cpu, color: 'text-blue-400', bg: 'bg-blue-500/5' },
    { title: 'Current Draw', val: `${currentData.telemetry.Current.toFixed(3)} A`, sub: 'Load Consumption', icon: Activity, color: 'text-amber-400', bg: 'bg-amber-500/5' },
    { title: 'Active Wattage', val: `${currentData.telemetry.Power_W.toFixed(3)} W`, sub: 'Calculated Yield', icon: Zap, color: 'text-emerald-400', bg: 'bg-emerald-500/5' },
    { title: 'Temperature', val: `${currentData.telemetry.Temp.toFixed(1)} °C`, sub: 'Core Thermo', icon: Thermometer, color: 'text-rose-400', bg: 'bg-rose-500/5' },
    { title: 'Relative Humidity', val: `${currentData.telemetry.Humidity.toFixed(1)} %`, sub: 'Atmosphere Sensor', icon: Droplets, color: 'text-teal-400', bg: 'bg-teal-500/5' },
    { title: 'Baro Pressure', val: `${currentData.telemetry.Pressure.toFixed(0)} hPa`, sub: 'Absolute Static', icon: Gauge, color: 'text-indigo-400', bg: 'bg-indigo-500/5' }
  ]), [currentData.telemetry]);

  const controlModes = useMemo(() => ([
    { id: 0, label: 'Fully Autonomous PID', desc: 'Standard local routine execution loops' },
    { id: 1, label: 'Mechanical Safe-Lock', desc: 'Lock flat and isolate tracker drivers' },
    { id: 2, label: 'Expert Bias Calibration', desc: 'Inject custom micro-adjustments' }
  ]), []);

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col antialiased font-sans selection:bg-indigo-500/30">
      {/* Top Header Deck Bar Layout */}
      <header className="border-b border-slate-900 bg-slate-900/40 backdrop-blur-md sticky top-0 z-50 px-6 py-4 flex flex-col sm:flex-row gap-4 justify-between items-center">
        <div className="flex items-center gap-3">
          <div className="bg-gradient-to-tr from-indigo-600 to-violet-500 p-2 rounded-xl shadow-lg shadow-indigo-500/20">
            <Radio className="w-6 h-6 text-white animate-pulse" />
          </div>
          <div>
            <h1 className="text-lg font-bold bg-gradient-to-r from-white via-slate-200 to-slate-400 bg-clip-text text-transparent tracking-tight">
              SolarTracker Edge Node Command Deck
            </h1>
            <p className="text-xs text-slate-500 font-mono">Telemetry Node Framework v0.1.4</p>
          </div>
        </div>

        {/* Global Connection Pipeline State Indicator */}
        <div className="flex items-center gap-4">
          <div className={`flex items-center gap-2 px-3 py-1.5 rounded-full border text-xs font-mono font-semibold transition-all duration-300 ${
            status === 'Live' ? 'bg-emerald-500/10 border-emerald-500/30 text-emerald-400 shadow-[0_0_15px_rgba(16,185,129,0.05)]' :
            status === 'Connecting' ? 'bg-amber-500/10 border-amber-500/30 text-amber-400 animate-pulse' :
            'bg-rose-500/10 border-rose-500/30 text-rose-400'
          }`}>
            <span className={`w-2 h-2 rounded-full ${status === 'Live' ? 'bg-emerald-400' : status === 'Connecting' ? 'bg-amber-400' : 'bg-rose-400'}`} />
            GW_STATUS: {status.toUpperCase()}
          </div>
        </div>
      </header>

      <div className="flex-1 flex flex-col md:flex-row">
        {/* Navigation Control Panel Core Dock Rail */}
        <nav className="w-full md:w-64 border-r border-slate-900 bg-slate-900/10 p-4 space-y-2 flex flex-row md:flex-col md:justify-start overflow-x-auto md:overflow-x-visible">
          {navTabs.map(tab => {
            const Icon = tab.icon;
            const isTarget = activeTab === tab.id;

            return (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl font-medium text-sm transition-all duration-200 whitespace-nowrap min-w-[150px] md:min-w-0 ${
                  isTarget
                    ? 'bg-gradient-to-r from-indigo-600/20 to-violet-600/5 border border-indigo-500/30 text-indigo-400 shadow-inner'
                    : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900/50 border border-transparent'
                }`}
              >
                <Icon className={`w-4 h-4 ${isTarget ? 'text-indigo-400' : 'text-slate-400'}`} />
                {tab.label}
              </button>
            );
          })}
        </nav>

        {/* Primary Viewspace Canvas Section */}
        <main className="flex-1 p-6 md:p-8 overflow-y-auto max-w-7xl mx-auto w-full space-y-6">

          {/* VIEW: MAIN CORE DASHBOARD GRID */}
          {activeTab === 'dashboard' && (
            <div className="space-y-6 animate-fadeIn">

              {/* High Density Status KPI Overview Row */}
              <div className="grid grid-cols-2 lg:grid-cols-6 gap-4">
                {dashboardCards.map((card, idx) => {
                  const Icon = card.icon;
                  return (
                    <div key={idx} className="bg-slate-900/40 border border-slate-900 p-4 rounded-2xl flex flex-col justify-between hover:border-slate-800 transition-all shadow-md group">
                      <div className="flex justify-between items-start mb-2">
                        <span className="text-xs font-medium text-slate-500 tracking-wider uppercase">{card.title}</span>
                        <div className={`${card.color} ${card.bg} p-1.5 rounded-lg border border-slate-800 group-hover:scale-105 transition-transform`}>
                          <Icon className="w-3.5 h-3.5" />
                        </div>
                      </div>
                      <div>
                        <div className="text-xl font-bold font-mono text-slate-100 tracking-tight">{card.val}</div>
                        <p className="text-[10px] text-slate-500 font-mono mt-0.5">{card.sub}</p>
                      </div>
                    </div>
                  );
                })}
              </div>

              {/* Main Realtime High-Resolution Telemetry Visualization Deck Grid */}
              <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">

                {/* Visualizer 1: Energy Yield Power Generation Waveform */}
                <div className="bg-slate-900/30 border border-slate-900/80 rounded-2xl p-5 flex flex-col lg:col-span-2">
                  <div className="flex justify-between items-center mb-4">
                    <div className="flex items-center gap-2">
                      <Zap className="w-4 h-4 text-emerald-400" />
                      <h3 className="text-sm font-semibold text-slate-200">Real-time Solar Generation Array Curve</h3>
                    </div>
                    <span className="text-[11px] font-mono bg-emerald-500/10 text-emerald-400 px-2 py-0.5 rounded border border-emerald-500/20">Live Generation Vector</span>
                  </div>
                  <div className="h-64 w-full">
                    <ResponsiveContainer width="100%" height="100%">
                      <AreaChart data={history} margin={{ top: 5, right: 5, left: -20, bottom: 0 }}>
                        <defs>
                          <linearGradient id="powerGlow" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="5%" stopColor="#10b981" stopOpacity={0.2}/>
                            <stop offset="95%" stopColor="#10b981" stopOpacity={0}/>
                          </linearGradient>
                        </defs>
                        <CartesianGrid strokeDasharray="3 3" stroke="#0f172a" vertical={false} />
                        <XAxis dataKey="Timestamp" stroke="#475569" fontSize={10} tickLine={false} />
                        <YAxis stroke="#475569" fontSize={10} tickLine={false} />
                        <Tooltip
                          contentStyle={{ backgroundColor: '#020617', borderColor: '#1e293b', borderRadius: '12px' }}
                          labelStyle={{ color: '#94a3b8', fontSize: '11px', fontFamily: 'monospace' }}
                          itemStyle={{ color: '#10b981', fontSize: '12px' }}
                        />
                        <Area type="monotone" dataKey="Power_W" name="Yield (W)" stroke="#10b981" strokeWidth={2} fillOpacity={1} fill="url(#powerGlow)" />
                      </AreaChart>
                    </ResponsiveContainer>
                  </div>
                </div>

                {/* Microclimate AI Analysis Side Engine Display */}
                <div className="bg-slate-900/30 border border-slate-900/80 rounded-2xl p-5 flex flex-col justify-between">
                  <div>
                    <div className="flex items-center gap-2 mb-4 pb-3 border-b border-slate-900">
                      <Cpu className="w-4 h-4 text-indigo-400" />
                      <h3 className="text-sm font-semibold text-slate-200">Edge Inference Analytics</h3>
                    </div>

                    <div className="space-y-4">
                      <div>
                        <span className="text-[10px] text-slate-500 block uppercase font-mono tracking-wider">Target Automation Vector</span>
                        <div className="text-md font-bold font-mono text-indigo-300 mt-1 flex items-center gap-2">
                          <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                          {currentData.ai_insights.State}
                        </div>
                      </div>

                      <div className="grid grid-cols-2 gap-3 pt-2">
                        <div className="bg-slate-950 p-3 rounded-xl border border-slate-900">
                          <span className="text-[10px] text-slate-500 block font-mono">Precipitation Risk</span>
                          <span className="text-lg font-bold font-mono text-slate-200">{currentData.ai_insights.Rain_Prob}%</span>
                        </div>
                        <div className="bg-slate-950 p-3 rounded-xl border border-slate-900">
                          <span className="text-[10px] text-slate-500 block font-mono">Core Ceiling Delta</span>
                          <span className="text-lg font-bold font-mono text-slate-200">{currentData.ai_insights.High}°C</span>
                        </div>
                      </div>
                    </div>
                  </div>

                  <div className="mt-4 p-3 bg-indigo-500/5 rounded-xl border border-indigo-500/10 flex items-start gap-2.5">
                    <TrendingUp className="w-4 h-4 text-indigo-400 shrink-0 mt-0.5" />
                    <p className="text-[11px] text-slate-400 leading-relaxed">
                      Tracking optimizations indicate stable localized illumination. Core thermals and absolute ambient pressures are within acceptable parameter guidelines.
                    </p>
                  </div>
                </div>

              </div>

              {/* SECONDARY DASHBOARD EXPANSION GRID: Environmental Environmental Deck Matrices */}
              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">

                {/* Visualizer 2: Temperature & Relative Humidity Dual Axis Overlap */}
                <div className="bg-slate-900/30 border border-slate-900/80 rounded-2xl p-5">
                  <div className="flex justify-between items-center mb-4">
                    <div className="flex items-center gap-2">
                      <Thermometer className="w-4 h-4 text-rose-400" />
                      <h3 className="text-sm font-semibold text-slate-200">Atmospheric Profile Matrix</h3>
                    </div>
                    <span className="text-[11px] font-mono text-slate-500">Dual Channel Core Tracking</span>
                  </div>
                  <div className="h-56 w-full">
                    <ResponsiveContainer width="100%" height="100%">
                      <LineChart data={history} margin={{ top: 5, right: 5, left: -20, bottom: 0 }}>
                        <CartesianGrid strokeDasharray="3 3" stroke="#0f172a" vertical={false} />
                        <XAxis dataKey="Timestamp" stroke="#475569" fontSize={9} />
                        <YAxis stroke="#475569" fontSize={9} />
                        <Tooltip contentStyle={{ backgroundColor: '#020617', borderColor: '#1e293b', borderRadius: '12px' }} />
                        <Legend wrapperStyle={{ fontSize: '11px', paddingTop: '10px' }} />
                        <Line type="monotone" dataKey="Temp" name="Temp (°C)" stroke="#f43f5e" strokeWidth={1.5} dot={false} activeDot={{ r: 4 }} />
                        <Line type="monotone" dataKey="Humidity" name="Humidity (%)" stroke="#06b6d4" strokeWidth={1.5} dot={false} activeDot={{ r: 4 }} />
                      </LineChart>
                    </ResponsiveContainer>
                  </div>
                </div>

                {/* Visualizer 3: Absolute Barometric Static Pressure Trend Profile */}
                <div className="bg-slate-900/30 border border-slate-900/80 rounded-2xl p-5">
                  <div className="flex justify-between items-center mb-4">
                    <div className="flex items-center gap-2">
                      <Gauge className="w-4 h-4 text-indigo-400" />
                      <h3 className="text-sm font-semibold text-slate-200">Absolute Static Pressure Gradient</h3>
                    </div>
                    <span className="text-[11px] font-mono text-slate-500">hPa Static Absolute Log</span>
                  </div>
                  <div className="h-56 w-full">
                    <ResponsiveContainer width="100%" height="100%">
                      <AreaChart data={history} margin={{ top: 5, right: 5, left: -20, bottom: 0 }}>
                        <defs>
                          <linearGradient id="pressureGlow" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="5%" stopColor="#6366f1" stopOpacity={0.15}/>
                            <stop offset="95%" stopColor="#6366f1" stopOpacity={0}/>
                          </linearGradient>
                        </defs>
                        <CartesianGrid strokeDasharray="3 3" stroke="#0f172a" vertical={false} />
                        <XAxis dataKey="Timestamp" stroke="#475569" fontSize={9} />
                        <YAxis domain={['dataMin - 1', 'dataMax + 1']} stroke="#475569" fontSize={9} />
                        <Tooltip contentStyle={{ backgroundColor: '#020617', borderColor: '#1e293b', borderRadius: '12px' }} />
                        <Area type="monotone" dataKey="Pressure" name="Pressure (hPa)" stroke="#6366f1" strokeWidth={1.5} fillOpacity={1} fill="url(#pressureGlow)" />
                      </AreaChart>
                    </ResponsiveContainer>
                  </div>
                </div>

              </div>
            </div>
          )}

          {/* VIEW: ADVANCED RESTRUCTURED DATA ANALYTICS DECK */}
          {activeTab === 'analytics' && (
            <div className="space-y-6 animate-fadeIn">

              {/* Statistical Engineering Aggregation Bar Cards */}
              <div className="grid grid-cols-1 sm:grid-cols-3 gap-5">
                <div className="bg-slate-900/40 border border-slate-900 p-5 rounded-2xl">
                  <span className="text-[11px] text-slate-500 block font-mono uppercase tracking-wider">Dynamic Integrated Power Output</span>
                  <div className="text-2xl font-bold font-mono text-slate-100 mt-1">{metrics.avgPower.toFixed(3)} W</div>
                  <p className="text-[10px] text-emerald-400 flex items-center gap-1 mt-1 font-mono">
                    <TrendingUp className="w-3 h-3" /> Arithmetic mean across buffer window
                  </p>
                </div>
                <div className="bg-slate-900/40 border border-slate-900 p-5 rounded-2xl">
                  <span className="text-[11px] text-slate-500 block font-mono uppercase tracking-wider">Observed Peak Load Wattage</span>
                  <div className="text-2xl font-bold font-mono text-indigo-400 mt-1">{metrics.maxPower.toFixed(3)} W</div>
                  <p className="text-[10px] text-slate-500 mt-1 font-mono">Max instantaneous vector captured</p>
                </div>
                <div className="bg-slate-900/40 border border-slate-900 p-5 rounded-2xl">
                  <span className="text-[11px] text-slate-500 block font-mono uppercase tracking-wider">Peak Amperage Outflow</span>
                  <div className="text-2xl font-bold font-mono text-amber-400 mt-1">{metrics.peakCurrent.toFixed(3)} A</div>
                  <p className="text-[10px] text-slate-500 mt-1 font-mono">Maximum structural load current draw</p>
                </div>
              </div>

              {/* Multi-Dimensional Analytics Graphical Control Matrix */}
              <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">

                {/* Visualizer A: Complex IV Waveform Balancing Map */}
                <div className="bg-slate-900/30 border border-slate-900/80 rounded-2xl p-5">
                  <div className="mb-4">
                    <h4 className="text-sm font-semibold text-slate-200">Volumetric Electrical Curve Characteristics (I-V Profile)</h4>
                    <p className="text-xs text-slate-500">Tracks co-dependence of voltage inputs and current workloads</p>
                  </div>
                  <div className="h-64 w-full">
                    <ResponsiveContainer width="100%" height="100%">
                      <ComposedChart data={history} margin={{ top: 5, right: -5, left: -20, bottom: 0 }}>
                        <CartesianGrid stroke="#0f172a" vertical={false} />
                        <XAxis dataKey="Timestamp" stroke="#475569" fontSize={9} />
                        <YAxis yAxisId="left" stroke="#3b82f6" fontSize={9} />
                        <YAxis yAxisId="right" orientation="right" stroke="#f59e0b" fontSize={9} />
                        <Tooltip contentStyle={{ backgroundColor: '#020617', borderColor: '#1e293b' }} />
                        <Legend wrapperStyle={{ fontSize: '11px' }} />
                        <Bar yAxisId="left" dataKey="Voltage" name="Voltage Amplitude (V)" fill="#3b82f6" fillOpacity={0.15} radius={[4, 4, 0, 0]} barSize={8} />
                        <Line yAxisId="right" type="monotone" dataKey="Current" name="Current Intensity (A)" stroke="#f59e0b" strokeWidth={2} dot={false} />
                      </ComposedChart>
                    </ResponsiveContainer>
                  </div>
                </div>

                {/* Visualizer B: Energy Density Yield Aggregation Stream */}
                <div className="bg-slate-900/30 border border-slate-900/80 rounded-2xl p-5">
                  <div className="mb-4">
                    <h4 className="text-sm font-semibold text-slate-200">Accumulative Integration Tracking Matrix</h4>
                    <p className="text-xs text-slate-500">Area profile modeling instantaneous power dissipation distributions</p>
                  </div>
                  <div className="h-64 w-full">
                    <ResponsiveContainer width="100%" height="100%">
                      <AreaChart data={history} margin={{ top: 5, right: 5, left: -20, bottom: 0 }}>
                        <defs>
                          <linearGradient id="analyticsGlow" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="5%" stopColor="#6366f1" stopOpacity={0.25}/>
                            <stop offset="95%" stopColor="#6366f1" stopOpacity={0}/>
                          </linearGradient>
                        </defs>
                        <CartesianGrid stroke="#0f172a" vertical={false} />
                        <XAxis dataKey="Timestamp" stroke="#475569" fontSize={9} />
                        <YAxis stroke="#475569" fontSize={9} />
                        <Tooltip contentStyle={{ backgroundColor: '#020617', borderColor: '#1e293b' }} />
                        <Area type="monotone" dataKey="Power_W" name="Integrated Power (W)" stroke="#6366f1" strokeWidth={2} fillOpacity={1} fill="url(#analyticsGlow)" />
                      </AreaChart>
                    </ResponsiveContainer>
                  </div>
                </div>

                {/* Visualizer C: Local Environmental Variance Delta logs */}
                <div className="bg-slate-900/30 border border-slate-900/80 rounded-2xl p-5 lg:col-span-2">
                  <div className="mb-3 flex justify-between items-center">
                    <div>
                      <h4 className="text-sm font-semibold text-slate-200">Environmental Profile Metrics Integration Log</h4>
                      <p className="text-xs text-slate-500">Cross-correlating physical anomalies against efficiency arrays</p>
                    </div>
                    <span className="text-[10px] font-mono text-slate-400 bg-slate-950 px-2 py-1 border border-slate-900 rounded-lg">High Density Engine Link</span>
                  </div>
                  <div className="h-60 w-full">
                    <ResponsiveContainer width="100%" height="100%">
                      <BarChart data={history} margin={{ top: 5, right: 5, left: -20, bottom: 0 }}>
                        <CartesianGrid stroke="#0f172a" vertical={false} />
                        <XAxis dataKey="Timestamp" stroke="#475569" fontSize={9} />
                        <YAxis stroke="#475569" fontSize={9} />
                        <Tooltip contentStyle={{ backgroundColor: '#020617', borderColor: '#1e293b' }} />
                        <Legend wrapperStyle={{ fontSize: '11px' }} />
                        <Bar dataKey="Temp" name="Core Air Temperature (°C)" fill="#e11d48" fillOpacity={0.6} radius={[3, 3, 0, 0]} />
                        <Bar dataKey="Humidity" name="Relative Humidity Vector (%)" fill="#0891b2" fillOpacity={0.6} radius={[3, 3, 0, 0]} />
                      </BarChart>
                    </ResponsiveContainer>
                  </div>
                </div>

              </div>
            </div>
          )}

          {/* VIEW: HARDWARE REMOTE CONTROLS INTERACTION INTERFACE */}
          {activeTab === 'controls' && (
            <div className="bg-slate-900/20 border border-slate-900 rounded-2xl p-6 max-w-2xl mx-auto space-y-6 animate-fadeIn">
              <div className="pb-4 border-b border-slate-900">
                <h3 className="text-base font-semibold text-slate-200">Hardware Actuator Optimization Core</h3>
                <p className="text-xs text-slate-500 mt-0.5">Override edge firmware tracking loop rules and force spatial constraints directly</p>
              </div>

              <div className="space-y-4">
                <label className="text-xs font-semibold font-mono text-slate-400 uppercase tracking-wider block">Operational Routine Configuration</label>
                <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
                  {controlModes.map(mode => (
                    <button
                      key={mode.id}
                      onClick={() => { setOverrideMode(mode.id); transmitControlPacket(mode.id, panBias, tiltBias); }}
                      className={`p-4 rounded-xl border text-left transition-all flex flex-col justify-between ${
                        overrideMode === mode.id
                          ? 'bg-indigo-600/10 border-indigo-500/50 text-indigo-400 ring-2 ring-indigo-500/20'
                          : 'bg-slate-950 border-slate-900 hover:border-slate-800 text-slate-400'
                      }`}
                    >
                      <span className="text-xs font-bold font-mono mb-1">MODE_0{mode.id}</span>
                      <div>
                        <h4 className="text-xs font-semibold text-slate-200 mb-0.5">{mode.label}</h4>
                        <p className="text-[10px] text-slate-500 leading-normal">{mode.desc}</p>
                      </div>
                    </button>
                  ))}
                </div>
              </div>

              {overrideMode === 2 && (
                <div className="space-y-5 p-4 bg-slate-950 rounded-xl border border-slate-900 animate-slideDown">
                  <div className="space-y-2">
                    <div className="flex justify-between font-mono text-xs text-slate-400">
                      <span>Horizontal Pan Offset Vector</span>
                      <span className="text-indigo-400 font-bold">{panBias}°</span>
                    </div>
                    <input
                      type="range" min="-45" max="45" value={panBias}
                      onChange={(e) => { const v = parseInt(e.target.value); setPanBias(v); transmitControlPacket(overrideMode, v, tiltBias); }}
                      className="w-full accent-indigo-500 bg-slate-900 rounded-lg appearance-none h-2 cursor-pointer"
                    />
                  </div>

                  <div className="space-y-2">
                    <div className="flex justify-between font-mono text-xs text-slate-400">
                      <span>Vertical Tilt Offset Vector</span>
                      <span className="text-indigo-400 font-bold">{tiltBias}°</span>
                    </div>
                    <input
                      type="range" min="-20" max="20" value={tiltBias}
                      onChange={(e) => { const v = parseInt(e.target.value); setTiltBias(v); transmitControlPacket(overrideMode, panBias, v); }}
                      className="w-full accent-indigo-500 bg-slate-900 rounded-lg appearance-none h-2 cursor-pointer"
                    />
                  </div>
                </div>
              )}

              <div className="pt-4 border-t border-slate-900 flex justify-end gap-3">
                <button
                  onClick={() => { setPanBias(0); setTiltBias(0); transmitControlPacket(overrideMode, 0, 0); }}
                  className="px-4 py-2 border border-slate-800 rounded-xl text-xs font-medium hover:bg-slate-900 transition-all text-slate-300"
                >
                  Reset Biases
                </button>
                <button
                  onClick={() => transmitControlPacket()}
                  className="px-4 py-2 bg-indigo-600 hover:bg-indigo-500 text-white rounded-xl text-xs font-medium shadow-md transition-all flex items-center gap-2"
                >
                  <RefreshCw className="w-3.5 h-3.5" /> Force Packet Sync
                </button>
              </div>
            </div>
          )}

          {/* VIEW: SYSTEM LOGS PIPELINE FEED */}
          {activeTab === 'logs' && (
            <div className="bg-slate-900/30 border border-slate-900/80 rounded-2xl flex flex-col h-[520px] overflow-hidden animate-fadeIn">
              <div className="p-4 bg-slate-900/50 border-b border-slate-900 flex justify-between items-center">
                <div className="flex items-center gap-2">
                  <Terminal className="w-4 h-4 text-slate-400" />
                  <h2 className="text-xs font-semibold font-mono text-slate-200 uppercase tracking-wider">Telemetry Downlink Stream Registers</h2>
                </div>
                <div className="flex items-center gap-2">
                  <span className="text-[10px] font-mono bg-slate-950 px-2 py-0.5 rounded border border-slate-900 text-slate-400">
                    Buffer: {logs.length} Vectors
                  </span>
                  <button
                    onClick={() => setLogs([])}
                    className="text-[10px] text-slate-500 hover:text-slate-300 font-mono underline ml-2"
                  >
                    Clear Stream
                  </button>
                </div>
              </div>

              <div className="p-4 bg-slate-950 flex-1 overflow-y-auto font-mono text-xs space-y-1.5 scrollbar-thin scrollbar-thumb-slate-900">
                {logs.length === 0 ? (
                  <p className="text-slate-700 italic text-center pt-8">Listening for registration strings down the socket channel...</p>
                ) : (
                  logs.map(log => (
                    <div key={log.id} className="flex gap-3 items-start leading-relaxed border-b border-slate-900/30 pb-1.5 hover:bg-slate-900/20 transition-colors px-1">
                      <span className="text-slate-600 select-none font-medium">[{log.timestamp}]</span>
                      <span className={`font-bold select-none uppercase text-[9px] tracking-wide px-1.5 py-0.5 rounded shrink-0 ${
                        log.type === 'success' ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/20' :
                        log.type === 'warn' ? 'bg-amber-500/10 text-amber-400 border border-amber-500/20' :
                        log.type === 'critical' ? 'bg-rose-500/10 text-rose-400 border border-rose-500/20' :
                        'bg-slate-900 text-slate-400 border border-slate-800'
                      }`}>
                        {log.type}
                      </span>
                      <span className="text-slate-300 font-mono tracking-normal">{log.message}</span>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}

        </main>
      </div>
    </div>
  );
}
