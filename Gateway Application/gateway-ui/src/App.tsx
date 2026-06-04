import { useState, useEffect, useRef } from 'react';
import {
  LayoutDashboard, BarChart3, Sliders, Terminal,
  Activity, Zap, Thermometer, Droplets, Gauge,
  Power, Radio, Cpu, Download, Settings
} from 'lucide-react';
import {
  ResponsiveContainer, AreaChart, Area, XAxis, YAxis,
  CartesianGrid, Tooltip, LineChart, Line, Legend
} from 'recharts';

// --- TYPES & CONTRACTS FOR FULL-DUPLEX TELEMETRY MATRIX ---
interface TelemetryFrame {
  Timestamp: string;
  Voltage: number;
  Current: number;
  Temp: number;
  Humidity: number;
  Pressure: number;
  Power_W: number;
  PanAngle: number;   // Tracked tracking orientation
  TiltAngle: number;  // Tracked tracking orientation
  NodeID: string;     // Active identifying key
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

const MAX_HISTORY = 30;

export default function App() {
  // Navigation & System Status States
  const [activeTab, setActiveTab] = useState<'dashboard' | 'analytics' | 'controls' | 'logs'>('dashboard');
  const [status, setStatus] = useState<'Connecting' | 'Live' | 'Disconnected'>('Connecting');

  // Telemetry Pipelines
  const [currentData, setCurrentData] = useState<{ telemetry: TelemetryFrame; ai_insights: AIInsights }>({
    telemetry: { 
      Timestamp: '--:--:--', Voltage: 0, Current: 0, Temp: 0, Humidity: 0, Pressure: 0, Power_W: 0,
      PanAngle: 90, TiltAngle: 0, NodeID: '--'
    },
    ai_insights: { State: 'Initializing...', Rain_Prob: 0, High: 0, Low: 0 }
  });
  const [history, setHistory] = useState<TelemetryFrame[]>([]);
  const [logs, setLogs] = useState<LogEntry[]>([]);

  // --- HARDWARE INTERACTION CONTROL STATES ---
  const [ctrlTargetID, setCtrlTargetID] = useState<number>(255); // 255 = Global Broadcast Swarm
  const [ctrlMode, setCtrlMode] = useState<number>(0);          // 0 = Auto LDR, 1 = Lock, 2 = Remote Expert
  const [ctrlBiasPan, setCtrlBiasPan] = useState<number>(0);     // Manual Pan Offset Shift
  const [ctrlBiasTilt, setCtrlBiasTilt] = useState<number>(0);   // Manual Tilt Offset Shift

  // Core Reference Hooks
  const wsRef = useRef<WebSocket | null>(null);

  // System Logging Utility
  const addLog = (type: LogEntry['type'], message: string) => {
    const newLog: LogEntry = {
      id: Math.random().toString(36).substring(2, 9),
      timestamp: new Date().toLocaleTimeString(),
      type,
      message
    };
    setLogs(prev => [newLog, ...prev].slice(0, 100));
  };

  // --- BI-DIRECTIONAL COMMAND TRANSMISSION ENGINE ---
  const sendHardwareCommand = (targetID: number, mode: number, biasPan: number, biasTilt: number) => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      // Constructs the structured frame expected by main.py
      const commandPayload = {
        targetID: Number(targetID),
        mode: Number(mode),
        biasPan: Number(biasPan),
        biasTilt: Number(biasTilt)
      };

      // Dispatches raw JSON string across open WebSocket line
      wsRef.current.send(JSON.stringify(commandPayload));
      
      addLog('success', `Command matrix flushed down-pipe: Node ${targetID === 255 ? 'ALL' : targetID} -> Mode ${mode} [Pan Bias: ${biasPan}°, Tilt Bias: ${biasTilt}°]`);
    } else {
      addLog('critical', 'Transmission Aborted: Active hardware gateway websocket channel is offline.');
    }
  };

  // Full Dual-Mode Full-Duplex Connection Engine
  useEffect(() => {
    let mockInterval: NodeJS.Timeout;

    const connectWebSocket = () => {
      setStatus('Connecting');
      addLog('info', 'Initializing socket handshake to Python API pipeline...');

      const ws = new WebSocket('ws://localhost:8000/ws/telemetry');
      wsRef.current = ws;

      ws.onopen = () => {
        setStatus('Live');
        addLog('success', 'Full-duplex telemetry channel established with Gateway.');
      };

      ws.onmessage = (event) => {
        try {
          const rawPayload = JSON.parse(event.data);
          if (rawPayload && rawPayload.telemetry) {
            const now = new Date();
            const timeStr = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

            const frame: TelemetryFrame = {
              ...rawPayload.telemetry,
              Timestamp: timeStr,
              Power_W: rawPayload.telemetry.Voltage * rawPayload.telemetry.Current,
              // Safe fallbacks to prevent rendering faults if keys drop
              PanAngle: rawPayload.telemetry.PanAngle ?? 90,
              TiltAngle: rawPayload.telemetry.TiltAngle ?? 0,
              NodeID: rawPayload.telemetry.NodeID ?? '--'
            };

            setCurrentData({ telemetry: frame, ai_insights: rawPayload.ai_insights });
            setHistory(prev => [...prev, frame].slice(-MAX_HISTORY));

            if (Math.random() > 0.90) {
              addLog('info', `Telemetry processed from Node ${frame.NodeID}. System Gen: ${frame.Power_W.toFixed(2)}W`);
            }
          }
        } catch (err) {
          addLog('critical', `Data serialization structural fault: ${err}`);
        }
      };

      ws.onclose = () => {
        setStatus('Disconnected');
        addLog('warn', 'Hardware socket offline. Activating UI Mock Simulation Engine...');

        mockInterval = setInterval(() => {
          const now = new Date();
          const timeStr = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

          const mockTelemetry: TelemetryFrame = {
            Timestamp: timeStr,
            Voltage: 5.0 + Math.sin(now.getTime() / 4000) * 0.15 + Math.random() * 0.04,
            Current: 0.5 + Math.cos(now.getTime() / 6000) * 0.1 + Math.random() * 0.02,
            Temp: 28.2 + Math.sin(now.getTime() / 20000) * 0.8 + Math.random() * 0.2,
            Humidity: 71.5 + Math.cos(now.getTime() / 12000) * 4.0,
            Pressure: 1009.4 + Math.random() * 0.3,
            Power_W: 0,
            PanAngle: Math.floor(90 + Math.sin(now.getTime() / 10000) * 25),
            TiltAngle: Math.floor(15 + Math.cos(now.getTime() / 10000) * 10),
            NodeID: "SIM-42"
          };
          mockTelemetry.Power_W = mockTelemetry.Voltage * mockTelemetry.Current;

          const mockAI: AIInsights = {
            State: mockTelemetry.Humidity > 74 ? "Atmospheric Shifts" : "Nominal Stability",
            Rain_Prob: Math.min(100, Math.max(0, parseFloat(((mockTelemetry.Humidity - 50) * 2.5).toFixed(1)))),
            High: 32.5,
            Low: 24.1
          };

          setCurrentData({ telemetry: mockTelemetry, ai_insights: mockAI });
          setHistory(prev => [...prev, mockTelemetry].slice(-MAX_HISTORY));
        }, 1000);
      };

      ws.onerror = () => {
        ws.close();
      };
    };

    connectWebSocket();

    return () => {
      if (wsRef.current) wsRef.current.close();
      if (mockInterval) clearInterval(mockInterval);
    };
  }, []);

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col font-sans">
      
      {/* Top Global Command Bar Header */}
      <header className="border-b border-slate-900 bg-slate-900/40 backdrop-blur px-6 py-4 flex flex-col sm:flex-row justify-between items-center gap-4">
        <div className="flex items-center gap-3">
          <div className="p-2 bg-indigo-600/10 rounded-xl border border-indigo-500/20 text-indigo-400">
            <Radio className="w-6 h-6 animate-pulse" />
          </div>
          <div>
            <h1 className="text-lg font-bold tracking-tight text-white">Sky-Link Core Gateway</h1>
            <p className="text-xs text-slate-400 font-mono">
              ACTIVE SOURCE: <span className="text-indigo-400 font-bold">NODE-{currentData.telemetry.NodeID}</span> // DHAKA_EAST
            </p>
          </div>
        </div>

        {/* Global Navigation Matrix */}
        <div className="flex bg-slate-900 p-1 rounded-xl border border-slate-800 gap-1">
          {/* Dashboard Button */}
          <button
            onClick={() => setActiveTab('dashboard')}
            className={`px-4 py-2 rounded-lg text-xs font-semibold tracking-wide transition-all flex items-center gap-2 ${activeTab === 'dashboard' ? 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20' : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'}`}
          >
            <LayoutDashboard className="w-3.5 h-3.5" /> Dashboard
          </button>
          {/* Analytics Button */}
          <button
            onClick={() => setActiveTab('analytics')}
            className={`px-4 py-2 rounded-lg text-xs font-semibold tracking-wide transition-all flex items-center gap-2 ${activeTab === 'analytics' ? 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20' : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'}`}
          >
            <BarChart3 className="w-3.5 h-3.5" /> Data Analytics
          </button>
          {/* Controls Button */}
          <button
            onClick={() => setActiveTab('controls')}
            className={`px-4 py-2 rounded-lg text-xs font-semibold tracking-wide transition-all flex items-center gap-2 ${activeTab === 'controls' ? 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20' : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'}`}
          >
            <Sliders className="w-3.5 h-3.5" /> Control Matrix
          </button>
          {/* Logs Button */}
          <button
            onClick={() => setActiveTab('logs')}
            className={`px-4 py-2 rounded-lg text-xs font-semibold tracking-wide transition-all flex items-center gap-2 ${activeTab === 'logs' ? 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20' : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'}`}
          >
            <Terminal className="w-3.5 h-3.5" /> System Logs
          </button>
        </div>

        {/* Dynamic State Badging */}
        <div className="flex items-center gap-3">
          <div className={`flex items-center gap-2 px-3 py-1.5 rounded-full border text-xs font-mono font-medium ${
            status === 'Live' ? 'bg-emerald-500/10 border-emerald-500/30 text-emerald-400' :
            status === 'Connecting' ? 'bg-amber-500/10 border-amber-500/30 text-amber-400' :
            'bg-rose-500/10 border-rose-500/30 text-rose-400'
          }`}>
            <span className={`w-2 h-2 rounded-full ${
              status === 'Live' ? 'bg-emerald-400 animate-pulse' :
              status === 'Connecting' ? 'bg-amber-400 animate-spin' :
              'bg-rose-400'
            }`} />
            {status}
          </div>
        </div>
      </header>

      {/* Main Panel View Port */}
      <main className="flex-1 p-6 max-w-7xl mx-auto w-full transition-all">

        {/* TAB 1: CORE TELEMETRY DASHBOARD */}
        {activeTab === 'dashboard' && (
          <div className="space-y-6">

            {/* Metric Instrumentation Grid */}
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-5 gap-4">
              <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl shadow-xl flex items-center justify-between">
                <div>
                  <p className="text-xs text-slate-400 font-medium tracking-wider uppercase">Bus Voltage</p>
                  <p className="text-2xl font-black text-white tracking-tight mt-1 font-mono">{currentData.telemetry.Voltage.toFixed(2)}<span className="text-slate-400 text-sm font-normal ml-0.5">V</span></p>
                </div>
                <div className="p-3 bg-amber-500/10 text-amber-400 border border-amber-500/20 rounded-xl"><Zap className="w-5 h-5" /></div>
              </div>

              <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl shadow-xl flex items-center justify-between">
                <div>
                  <p className="text-xs text-slate-400 font-medium tracking-wider uppercase">Draw Current</p>
                  <p className="text-2xl font-black text-white tracking-tight mt-1 font-mono">{currentData.telemetry.Current.toFixed(3)}<span className="text-slate-400 text-sm font-normal ml-0.5">A</span></p>
                </div>
                <div className="p-3 bg-sky-500/10 text-sky-400 border border-sky-500/20 rounded-xl"><Activity className="w-5 h-5" /></div>
              </div>

              <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl shadow-xl flex items-center justify-between">
                <div>
                  <p className="text-xs text-slate-400 font-medium tracking-wider uppercase">System Temp</p>
                  <p className="text-2xl font-black text-white tracking-tight mt-1 font-mono">{currentData.telemetry.Temp.toFixed(1)}<span className="text-slate-400 text-sm font-normal ml-0.5">°C</span></p>
                </div>
                <div className="p-3 bg-rose-500/10 text-rose-400 border border-rose-500/20 rounded-xl"><Thermometer className="w-5 h-5" /></div>
              </div>

              <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl shadow-xl flex items-center justify-between">
                <div>
                  <p className="text-xs text-slate-400 font-medium tracking-wider uppercase">Rel. Humidity</p>
                  <p className="text-2xl font-black text-white tracking-tight mt-1 font-mono">{currentData.telemetry.Humidity.toFixed(1)}<span className="text-slate-400 text-sm font-normal ml-0.5">%</span></p>
                </div>
                <div className="p-3 bg-blue-500/10 text-blue-400 border border-blue-500/20 rounded-xl"><Droplets className="w-5 h-5" /></div>
              </div>

              <div className="bg-slate-900 border border-slate-800 p-4 rounded-xl shadow-xl flex items-center justify-between">
                <div>
                  <p className="text-xs text-slate-400 font-medium tracking-wider uppercase">Baro Pressure</p>
                  <p className="text-2xl font-black text-white tracking-tight mt-1 font-mono">{currentData.telemetry.Pressure.toFixed(1)}<span className="text-slate-400 text-sm font-normal ml-0.5">hPa</span></p>
                </div>
                <div className="p-3 bg-emerald-500/10 text-emerald-400 border border-emerald-500/20 rounded-xl"><Gauge className="w-5 h-5" /></div>
              </div>
            </div>

            {/* Main Charts & Edge-AI Layout */}
            <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">

              {/* Telemetry Tracking Wave */}
              <div className="bg-slate-900 border border-slate-800 p-5 rounded-2xl shadow-xl lg:col-span-2">
                <div className="flex justify-between items-center mb-4">
                  <div>
                    <h2 className="text-sm font-bold text-white tracking-wide">Live Power & Consumption Signature</h2>
                    <p className="text-xs text-slate-400">Computed real-time array watt profiles (W = V * I)</p>
                  </div>
                  <div className="text-xs font-mono bg-slate-950 px-3 py-1 rounded-md border border-slate-800 text-indigo-400">
                    Active Load: {currentData.telemetry.Power_W.toFixed(2)} W
                  </div>
                </div>
                <div className="h-64">
                  <ResponsiveContainer width="100%" height="100%">
                    <AreaChart data={history} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                      <defs>
                        <linearGradient id="powerGlow" x1="0" y1="0" x2="0" y2="1">
                          <stop offset="5%" stopColor="#6366f1" stopOpacity={0.3}/>
                          <stop offset="95%" stopColor="#6366f1" stopOpacity={0}/>
                        </linearGradient>
                      </defs>
                      <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                      <XAxis dataKey="Timestamp" stroke="#64748b" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                      <YAxis stroke="#64748b" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                      <Tooltip contentStyle={{ backgroundColor: '#0f172a', borderColor: '#334155', color: '#fff', fontSize: '12px' }} />
                      <Area type="monotone" dataKey="Power_W" stroke="#6366f1" strokeWidth={2} fillOpacity={1} fill="url(#powerGlow)" name="Power (Watts)" />
                    </AreaChart>
                  </ResponsiveContainer>
                </div>
              </div>

              {/* Advanced Local Intelligence & Spatial Vectors Card */}
              <div className="bg-slate-900 border border-slate-800 p-5 rounded-2xl shadow-xl flex flex-col justify-between">
                <div>
                  <h2 className="text-sm font-bold text-white tracking-wide mb-3 flex items-center gap-2">
                    <Cpu className="w-4 h-4 text-indigo-400" /> System Metrics & Vectors
                  </h2>
                  <div className="bg-slate-950 p-4 rounded-xl border border-slate-800 font-mono text-xs space-y-2.5">
                    <div className="flex justify-between border-b border-slate-900 pb-2">
                      <span className="text-slate-500">Environmental State:</span>
                      <span className="text-indigo-400 font-bold">{currentData.ai_insights.State}</span>
                    </div>
                    <div className="flex justify-between border-b border-slate-900 pb-2">
                      <span className="text-slate-500">Precipitation Prob:</span>
                      <span className="text-emerald-400">{currentData.ai_insights.Rain_Prob}%</span>
                    </div>
                    {/* Live Vector Tracking Readouts */}
                    <div className="flex justify-between border-b border-slate-900 pb-2">
                      <span className="text-slate-500">Current Pan Gimbal:</span>
                      <span className="text-amber-400 font-bold">{currentData.telemetry.PanAngle}°</span>
                    </div>
                    <div className="flex justify-between">
                      <span className="text-slate-500">Current Tilt Gimbal:</span>
                      <span className="text-amber-400 font-bold">{currentData.telemetry.TiltAngle}°</span>
                    </div>
                  </div>
                </div>

                <div className="space-y-2 mt-4">
                  <div className="flex justify-between text-xs font-medium">
                    <span className="text-slate-400">Thermal Envelope Target</span>
                    <span className="text-slate-300 font-mono">{currentData.telemetry.Temp.toFixed(1)}°C / {currentData.ai_insights.High}°C</span>
                  </div>
                  <div className="w-full bg-slate-950 h-2 rounded-full overflow-hidden border border-slate-800">
                    <div
                      className="bg-indigo-500 h-full transition-all duration-500"
                      style={{ width: `${Math.min(100, (currentData.telemetry.Temp / (currentData.ai_insights.High || 1)) * 100)}%` }}
                    />
                  </div>
                </div>
              </div>

            </div>
          </div>
        )}

        {/* TAB 2: DETAILED ANALYTICS VIEW */}
        {activeTab === 'analytics' && (
          <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl shadow-xl space-y-6">
            <div>
              <h2 className="text-md font-bold text-white">Full Bus Comparative Node Mapping</h2>
              <p className="text-xs text-slate-400">Concurrent voltage and current sensor analysis timelines</p>
            </div>
            <div className="h-80">
              <ResponsiveContainer width="100%" height="100%">
                <LineChart data={history} margin={{ top: 5, right: 20, left: -20, bottom: 5 }}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                  <XAxis dataKey="Timestamp" stroke="#64748b" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                  <YAxis yAxisId="left" stroke="#amber-400" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                  <YAxis yAxisId="right" orientation="right" stroke="#sky-400" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                  <Tooltip contentStyle={{ backgroundColor: '#0f172a', borderColor: '#334155' }} />
                  <Legend wrapperStyle={{ fontSize: '11px', paddingTop: '10px' }} />
                  <Line yAxisId="left" type="monotone" dataKey="Voltage" stroke="#f59e0b" strokeWidth={2} dot={false} name="Voltage (V)" />
                  <Line yAxisId="right" type="monotone" dataKey="Current" stroke="#0ea5e9" strokeWidth={2} dot={false} name="Current (A)" />
                </LineChart>
              </ResponsiveContainer>
            </div>
          </div>
        )}

        {/* TAB 3: CONTROLS & COMMAND MATRIX */}
        {activeTab === 'controls' && (
          <div className="bg-slate-900 border border-slate-800 p-6 rounded-2xl shadow-xl space-y-6">
            <div>
              <h2 className="text-md font-bold text-white flex items-center gap-2">
                <Sliders className="w-5 h-5 text-indigo-400" /> Gateway Registry Overrides
              </h2>
              <p className="text-xs text-slate-400">Issue direct spatial orientations and configurations down to the array network</p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              
              {/* Target & Mode Selection Matrix */}
              <div className="bg-slate-950 p-5 rounded-xl border border-slate-800 space-y-4">
                <h3 className="text-xs font-bold uppercase tracking-wider text-slate-300 flex items-center gap-2">
                  <Cpu className="w-3.5 h-3.5 text-indigo-400" /> Target Configuration
                </h3>
                
                {/* 1. Target ID Field Selection */}
                <div className="space-y-1.5">
                  <label className="text-xs text-slate-400 block">Target Field Tracker ID</label>
                  <div className="flex gap-2">
                    <input 
                      type="number" 
                      min="0" 
                      max="255"
                      value={ctrlTargetID}
                      onChange={(e) => setCtrlTargetID(Number(e.target.value))}
                      className="bg-slate-900 border border-slate-800 rounded-lg px-3 py-1.5 text-xs font-mono text-white w-24 focus:outline-none focus:border-indigo-500"
                    />
                    <button 
                      onClick={() => setCtrlTargetID(255)}
                      className={`px-3 py-1.5 rounded-lg text-xs font-semibold font-mono transition-all border ${ctrlTargetID === 255 ? 'bg-indigo-600 border-indigo-500 text-white' : 'bg-slate-900 border-slate-800 text-slate-400 hover:text-slate-200'}`}
                    >
                      Universal Broadcast (255)
                    </button>
                  </div>
                </div>

                {/* 2. Operational Mode Selection Flag */}
                <div className="space-y-1.5 pt-2">
                  <label className="text-xs text-slate-400 block">Gimbal Mode Protocol</label>
                  <select 
                    value={ctrlMode} 
                    onChange={(e) => setCtrlMode(Number(e.target.value))}
                    className="w-full bg-slate-900 border border-slate-800 rounded-lg px-3 py-2 text-xs font-semibold text-slate-200 focus:outline-none focus:border-indigo-500"
                  >
                    <option value={0}>Mode 0: Autonomous Algorithmic LDR Tracking</option>
                    <option value={1}>Mode 1: High-Wind Emergency Mechanical Lock</option>
                    <option value={2}>Mode 2: Remote Expert Predictive Micro-Bias Override</option>
                  </select>
                </div>
              </div>

              {/* Dynamic Offset Adjustment Sliders */}
              <div className="bg-slate-950 p-5 rounded-xl border border-slate-800 space-y-4 flex flex-col justify-between">
                <div className="space-y-4">
                  <h3 className="text-xs font-bold uppercase tracking-wider text-slate-300 flex items-center gap-2">
                    <Settings className="w-3.5 h-3.5 text-indigo-400" /> Correction Biases
                  </h3>
                  
                  {/* Pan Adjust Slider */}
                  <div className="space-y-1">
                    <div className="flex justify-between text-xs font-mono">
                      <span className="text-slate-400">Horizontal Pan Bias</span>
                      <span className={ctrlBiasPan >= 0 ? "text-emerald-400" : "text-rose-400"}>{ctrlBiasPan}°</span>
                    </div>
                    <input 
                      type="range" min="-45" max="45" 
                      value={ctrlBiasPan} 
                      onChange={(e) => setCtrlBiasPan(Number(e.target.value))}
                      className="w-full accent-indigo-500 h-1.5 bg-slate-800 rounded-lg cursor-pointer" 
                    />
                  </div>

                  {/* Tilt Adjust Slider */}
                  <div className="space-y-1 pt-1">
                    <div className="flex justify-between text-xs font-mono">
                      <span className="text-slate-400">Vertical Tilt Bias</span>
                      <span className={ctrlBiasTilt >= 0 ? "text-emerald-400" : "text-rose-400"}>{ctrlBiasTilt}°</span>
                    </div>
                    <input 
                      type="range" min="-45" max="45" 
                      value={ctrlBiasTilt} 
                      onChange={(e) => setCtrlBiasTilt(Number(e.target.value))}
                      className="w-full accent-indigo-500 h-1.5 bg-slate-800 rounded-lg cursor-pointer" 
                    />
                  </div>
                </div>

                {/* Dispatch Trigger Execution Button */}
                <button
                  onClick={() => sendHardwareCommand(ctrlTargetID, ctrlMode, ctrlBiasPan, ctrlBiasTilt)}
                  className="w-full py-2.5 mt-4 bg-indigo-600 hover:bg-indigo-500 text-white rounded-lg text-xs font-bold tracking-wide transition-all shadow-lg shadow-indigo-600/10 flex items-center justify-center gap-2"
                >
                  <Power className="w-4 h-4" /> Transmit Command Packet Frame
                </button>
              </div>

            </div>
          </div>
        )}

        {/* TAB 4: SYSTEM TERMINAL LOG TRACKING */}
        {activeTab === 'logs' && (
          <div className="bg-slate-900 border border-slate-800 rounded-2xl shadow-xl overflow-hidden flex flex-col h-[400px]">
            <div className="bg-slate-900 border-b border-slate-800 px-5 py-3.5 flex justify-between items-center">
              <div className="flex items-center gap-2">
                <Terminal className="w-4 h-4 text-indigo-400" />
                <h2 className="text-xs font-bold uppercase tracking-wider text-slate-200">System Gateway Pipeline Logs</h2>
              </div>
              <span className="text-[10px] font-mono bg-slate-950 px-2 py-0.5 rounded border border-slate-800 text-slate-400">
                Buffer: {logs.length} Lines
              </span>
            </div>

            <div className="p-4 bg-slate-950 flex-1 overflow-y-auto font-mono text-xs space-y-2 scrollbar-thin scrollbar-thumb-slate-800">
              {logs.length === 0 ? (
                <p className="text-slate-600 italic">Listening for system registration strings...</p>
              ) : (
                logs.map(log => (
                  <div key={log.id} className="flex gap-2 items-start leading-relaxed animate-fadeIn">
                    <span className="text-slate-600 select-none">[{log.timestamp}]</span>
                    <span className={`font-bold select-none uppercase text-[10px] px-1 rounded ${
                      log.type === 'success' ? 'bg-emerald-500/10 text-emerald-400' :
                      log.type === 'warn' ? 'bg-amber-500/10 text-amber-400' :
                      log.type === 'critical' ? 'bg-rose-500/10 text-rose-400' :
                      'bg-slate-800 text-slate-400'
                    }`}>
                      {log.type}
                    </span>
                    <span className="text-slate-300">{log.message}</span>
                  </div>
                ))
              )}
            </div>
          </div>
        )}

      </main>
    </div>
  );
}
