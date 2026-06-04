import React, { useState, useEffect, useRef, useCallback } from 'react';
import { 
  LayoutDashboard, Sliders, Terminal, Activity, Zap, 
  Thermometer, Droplets, Gauge, ShieldAlert, Cpu, RefreshCw,
  SlidersHorizontal, Power, Download, Settings, Layers
} from 'lucide-react';
import { 
  ResponsiveContainer, AreaChart, Area, XAxis, YAxis, 
  CartesianGrid, Tooltip, LineChart, Line, Legend 
} from 'recharts';

// =========================================================================
// RIGOROUS TYPING & SYSTEM CORE CONTRACTS
// =========================================================================
interface AIInference {
  state: string;
  rain_prob: number;
  envelope_high: number;
  envelope_low: number;
}

interface TelemetryNode {
  node_id: string;        // Hex-encoded string descriptor (e.g., "2A")
  voltage: number;        // Solar Panel Bus Potential (V)
  current: number;        // Dynamic Load Profile (mA)
  temp: number;           // Enclosure Thermal Reading (°C)
  humidity: number;       // Relative Ambient Moisture (%)
  pressure: number;       // Barometric Core Air Pressure (hPa)
  pan_angle: number;      // Actuator Alignment Coordinates
  tilt_angle: number;     // Actuator Alignment Coordinates
  ai_insights: AIInference;
  history: Array<{ 
    time: string; 
    power: number; 
    voltage: number;
    current: number;
    temp: number; 
  }>;
}

interface LogMessage {
  id: string;
  time: string;
  type: 'info' | 'success' | 'warn' | 'error';
  body: string;
}

const MAX_HISTORY_LENGTH = 30;

export default function App() {
  // Navigation & Workspace Architecture Coordination States
  const [activeTab, setActiveTab] = useState<'dashboard' | 'analytics' | 'controls' | 'logs'>('dashboard');
  const [status, setStatus] = useState<'Connecting' | 'Live' | 'Disconnected'>('Disconnected');
  const [nodes, setNodes] = useState<Record<string, TelemetryNode>>({});
  const [selectedNode, setSelectedNode] = useState<string>('all');
  const [logs, setLogs] = useState<LogMessage[]>([]);

  // Interactive Guidance Injection Control Registers
  const [targetID, setTargetID] = useState<number>(0);
  const [mode, setMode] = useState<number>(0);
  const [biasPan, setBiasPan] = useState<number>(0);
  const [biasTilt, setBiasTilt] = useState<number>(0);

  // Core Connection References
  const socketRef = useRef<WebSocket | null>(null);
  const reconnectTimer = useRef<number | null>(null);

  // Atomic Real-Time System Trace Logger
  const pushLog = useCallback((type: LogMessage['type'], body: string) => {
    const newLog: LogMessage = {
      id: Math.random().toString(36).substring(2, 9),
      time: new Date().toLocaleTimeString(),
      type,
      body
    };
    setLogs(prev => [newLog, ...prev.slice(0, 149)]);
  }, []);

  // =========================================================================
  // DUAL-MODE FULL-DUPLEX DATA PIPELINE MANAGEMENT ENGINE
  // =========================================================================
  const connectHardwareGridPipeline = useCallback(() => {
    if (socketRef.current) socketRef.current.close();
    setStatus('Connecting');

    const networkHost = window.location.hostname === 'localhost' ? 'localhost:8000' : `${window.location.hostname}:8000`;
    const socketUrl = `ws://${networkHost}/ws/telemetry`;

    pushLog('info', `Connecting uplink channel to streaming workspace hub: ws://${networkHost}`);
    const ws = new WebSocket(socketUrl);
    socketRef.current = ws;

    ws.onopen = () => {
      setStatus('Live');
      pushLog('success', 'Uplink synchronization protocol established. Telemetry matrix is active.');
    };

    ws.onmessage = (event) => {
      try {
        const dataFrame = JSON.parse(event.data);
        if (dataFrame.node_id === undefined) return;

        // Standardize Node Addresses as neat, searchable Hex Tokens
        const idHex = dataFrame.node_id.toString(16).toUpperCase().padStart(2, '0');
        const instantPowerW = dataFrame.voltage * (dataFrame.currentmA / 1000.0);
        const timestamp = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

        // Synthesize inference logic if downstream pipeline lacks it
        const fallbackAI: AIInference = dataFrame.ai_insights || {
          state: dataFrame.humidity > 74 ? "Atmospheric Shifts" : "Nominal Stability",
          rain_prob: Math.min(100, Math.max(0, parseFloat(((dataFrame.humidity - 50) * 2.5).toFixed(1)))),
          envelope_high: 35.0,
          envelope_low: 22.0
        };

        setNodes(prev => {
          const matchedNode = prev[idHex] || {
            node_id: idHex,
            voltage: 0,
            current: 0,
            temp: 0,
            humidity: 0,
            pressure: 0,
            pan_angle: 90,
            tilt_angle: 0,
            ai_insights: fallbackAI,
            history: []
          };

          const dynamicHistory = [
            ...matchedNode.history,
            { 
              time: timestamp, 
              power: instantPowerW, 
              voltage: dataFrame.voltage, 
              current: dataFrame.currentmA / 1000.0,
              temp: dataFrame.temperature 
            }
          ].slice(-MAX_HISTORY_LENGTH);

          return {
            ...prev,
            [idHex]: {
              node_id: idHex,
              voltage: dataFrame.voltage,
              current: dataFrame.currentmA,
              temp: dataFrame.temperature,
              humidity: dataFrame.humidity,
              pressure: dataFrame.pressure,
              pan_angle: dataFrame.panAngle || 90,
              tilt_angle: dataFrame.tiltAngle || 0,
              ai_insights: fallbackAI,
              history: dynamicHistory
            }
          };
        });
      } catch (err) {
        // Handle stream serialization fragments gracefully
      }
    };

    ws.onclose = () => {
      setStatus('Disconnected');
      pushLog('warn', 'Hardware link dropped. Activating automatic recovery loop in 4000ms...');
      
      // Dynamic Visual Fallback Core Loop (Simulates data directly on loss)
      reconnectTimer.current = window.setTimeout(() => {
        connectHardwareGridPipeline();
        simulateFieldSwarmActivity();
      }, 4000);
    };

    ws.onerror = () => ws.close();
  }, [pushLog]);

  // UI Simulation Core Engine: Runs localized testing arrays if physical server drops
  const simulateFieldSwarmActivity = () => {
    const simulationIDs = ["2A", "0B"];
    const now = new Date();
    const timeStr = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

    setNodes(prev => {
      const updatedState = { ...prev };
      simulationIDs.forEach((id, index) => {
        const offset = index * 4000;
        const mockV = 4.8 + Math.sin((now.getTime() + offset) / 5000) * 0.3 + Math.random() * 0.05;
        const mockI = 310.0 + Math.cos((now.getTime() + offset) / 7000) * 40.0 + Math.random() * 5;
        const mockT = 29.5 + Math.sin((now.getTime() + offset) / 15000) * 1.2;
        const mockH = 68.2 + Math.cos((now.getTime() + offset) / 10000) * 5.0;

        const node = updatedState[id] || {
          node_id: id, voltage: 0, current: 0, temp: 0, humidity: 0, pressure: 1011.4,
          pan_angle: 75, tilt_angle: 22, ai_insights: { state: "Nominal Stability", rain_prob: 24, envelope_high: 38, envelope_low: 20 },
          history: []
        };

        node.voltage = mockV;
        node.current = mockI;
        node.temp = mockT;
        node.humidity = mockH;
        node.history = [...node.history, { time: timeStr, power: mockV * (mockI / 1000.0), voltage: mockV, current: mockI / 1000.0, temp: mockT }].slice(-MAX_HISTORY_LENGTH);
        
        updatedState[id] = node;
      });
      return updatedState;
    });
  };

  useEffect(() => {
    connectHardwareGridPipeline();
    return () => {
      if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
      if (socketRef.current) socketRef.current.close();
    };
  }, [connectHardwareGridPipeline]);

  // =========================================================================
  // TRANSMISSION ROUTER INJECTION PORT
  // =========================================================================
  const dispatchCommandArray = (e: React.FormEvent) => {
    e.preventDefault();
    if (!socketRef.current || socketRef.current.readyState !== WebSocket.OPEN) {
      pushLog('error', 'Transmission rejected: Dynamic hardware pipeline is offline.');
      return;
    }

    // MATCHES CENTRALGATEWAY.INO SERIAL ROUTING FOOTPRINT: CMD,targetID,mode,biasPan,biasTilt
    const serialPayloadStr = `CMD,${targetID},${mode},${biasPan},${biasTilt}`;
    
    socketRef.current.send(JSON.stringify({ raw_packet: serialPayloadStr }));
    pushLog('info', `Dispatched array downlink over web pipeline -> [${serialPayloadStr}]`);
  };

  const activeNodeArray = Object.values(nodes);
  const activeFocusNode = selectedNode === 'all' ? activeNodeArray[0] : nodes[selectedNode];

  return (
    <div className="flex h-screen bg-slate-950 text-slate-100 font-sans overflow-hidden select-none">
      
      {/* Structural Desktop Enterprise Navigation Panel */}
      <nav className="w-64 bg-slate-900 border-r border-slate-800 flex flex-col justify-between">
        <div>
          <div className="p-6 border-b border-slate-800 flex items-center gap-3">
            <Cpu className="h-6 w-6 text-indigo-500 animate-pulse" />
            <div>
              <h1 className="font-black text-sm tracking-wider uppercase text-slate-200">Sky-Link System</h1>
              <span className="text-[10px] text-slate-500 font-mono font-bold">MESH ARCHITECTURE v2.5</span>
            </div>
          </div>
          <div className="p-4 space-y-1">
            <button onClick={() => setActiveTab('dashboard')} className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl text-xs font-bold tracking-wide uppercase transition-all ${activeTab === 'dashboard' ? 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20' : 'text-slate-400 hover:bg-slate-800/50 hover:text-slate-200'}`}>
              <LayoutDashboard className="h-4 w-4" /> Swarm Dashboard
            </button>
            <button onClick={() => setActiveTab('analytics')} className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl text-xs font-bold tracking-wide uppercase transition-all ${activeTab === 'analytics' ? 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20' : 'text-slate-400 hover:bg-slate-800/50 hover:text-slate-200'}`}>
              <Activity className="h-4 w-4" /> Multi-Axis Analytics
            </button>
            <button onClick={() => setActiveTab('controls')} className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl text-xs font-bold tracking-wide uppercase transition-all ${activeTab === 'controls' ? 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20' : 'text-slate-400 hover:bg-slate-800/50 hover:text-slate-200'}`}>
              <Sliders className="h-4 w-4" /> Guidance Matrix
            </button>
            <button onClick={() => setActiveTab('logs')} className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl text-xs font-bold tracking-wide uppercase transition-all ${activeTab === 'logs' ? 'bg-indigo-600 text-white shadow-lg shadow-indigo-600/20' : 'text-slate-400 hover:bg-slate-800/50 hover:text-slate-200'}`}>
              <Terminal className="h-4 w-4" /> Network Stream
            </button>
          </div>
        </div>

        {/* Local Backbone Health Interface Badge */}
        <div className="p-4 border-t border-slate-800 bg-slate-950/40 m-4 rounded-xl border">
          <div className="flex items-center justify-between mb-2">
            <span className="text-[10px] text-slate-500 font-bold uppercase tracking-wider">Uplink Backbone</span>
            <span className={`h-2 w-2 rounded-full ${status === 'Live' ? 'bg-emerald-500 shadow-lg shadow-emerald-500/50' : status === 'Connecting' ? 'bg-amber-500 animate-ping' : 'bg-rose-500'}`} />
          </div>
          <div className="flex justify-between items-center">
            <span className="text-sm font-bold text-slate-200 font-mono">{status}</span>
            <button onClick={connectHardwareGridPipeline} className="p-1 text-slate-500 hover:text-slate-300 rounded transition">
              <RefreshCw className="h-3 w-3" />
            </button>
          </div>
        </div>
      </nav>

      {/* Primary Application Workspace Viewport */}
      <main className="flex-1 flex flex-col overflow-hidden bg-slate-950">
        <header className="h-16 border-b border-slate-800 bg-slate-900/40 backdrop-blur flex items-center justify-between px-8">
          <div className="flex items-center gap-4">
            <h2 className="text-sm font-black uppercase tracking-wider text-slate-300">{activeTab} Viewport</h2>
            {activeTab !== 'controls' && activeTab !== 'logs' && (
              <select value={selectedNode} onChange={(e) => setSelectedNode(e.target.value)} className="bg-slate-900 border border-slate-800 rounded-lg px-3 py-1.5 text-xs font-mono font-bold text-indigo-400 outline-none focus:border-indigo-500 transition">
                <option value="all">Global Multi-Node Composite</option>
                {activeNodeArray.map(node => (
                  <option key={node.node_id} value={node.node_id}>Field Node Terminal: 0x{node.node_id}</option>
                ))}
              </select>
            )}
          </div>
          <div className="text-xs text-slate-400 font-mono">
            Active Mesh Targets Online: <span className="text-indigo-400 font-bold">{activeNodeArray.length}</span>
          </div>
        </header>

        <div className="flex-1 overflow-y-auto p-8">
          {/* TAB 1: REAL-TIME SWARM INSTRUMENTATION MONITOR */}
          {activeTab === 'dashboard' && (
            <div className="space-y-6">
              {activeFocusNode ? (
                <>
                  {/* Metric Instrumentation Array Core Grid */}
                  <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-5 gap-4">
                    <div className="bg-slate-900 border border-slate-800/80 p-5 rounded-xl shadow-xl flex items-center justify-between">
                      <div>
                        <span className="text-[10px] font-bold text-slate-400 uppercase tracking-wider">Bus Potential</span>
                        <h3 className="text-2xl font-black text-white mt-1 font-mono">{activeFocusNode.voltage.toFixed(2)}<span className="text-xs font-normal text-slate-500 ml-0.5">V</span></h3>
                      </div>
                      <div className="p-3 rounded-xl bg-amber-500/10 text-amber-400 border border-amber-500/10"><Zap className="h-5 w-5" /></div>
                    </div>

                    <div className="bg-slate-900 border border-slate-800/80 p-5 rounded-xl shadow-xl flex items-center justify-between">
                      <div>
                        <span className="text-[10px] font-bold text-slate-400 uppercase tracking-wider">Draw Profile</span>
                        <h3 className="text-2xl font-black text-white mt-1 font-mono">{activeFocusNode.current.toFixed(1)}<span className="text-xs font-normal text-slate-500 ml-0.5">mA</span></h3>
                      </div>
                      <div className="p-3 rounded-xl bg-sky-500/10 text-sky-400 border border-sky-500/10"><Activity className="h-5 w-5" /></div>
                    </div>

                    <div className="bg-slate-900 border border-slate-800/80 p-5 rounded-xl shadow-xl flex items-center justify-between">
                      <div>
                        <span className="text-[10px] font-bold text-slate-400 uppercase tracking-wider">Thermal Envelope</span>
                        <h3 className="text-2xl font-black text-white mt-1 font-mono">{activeFocusNode.temp.toFixed(1)}<span className="text-xs font-normal text-slate-500 ml-0.5">°C</span></h3>
                      </div>
                      <div className="p-3 rounded-xl bg-rose-500/10 text-rose-400 border border-rose-500/10"><Thermometer className="h-5 w-5" /></div>
                    </div>

                    <div className="bg-slate-900 border border-slate-800/80 p-5 rounded-xl shadow-xl flex items-center justify-between">
                      <div>
                        <span className="text-[10px] font-bold text-slate-400 uppercase tracking-wider">Moisture Matrix</span>
                        <h3 className="text-2xl font-black text-white mt-1 font-mono">{activeFocusNode.humidity.toFixed(1)}<span className="text-xs font-normal text-slate-500 ml-0.5">%</span></h3>
                      </div>
                      <div className="p-3 rounded-xl bg-blue-500/10 text-blue-400 border border-blue-500/10"><Droplets className="h-5 w-5" /></div>
                    </div>

                    <div className="bg-slate-900 border border-slate-800/80 p-5 rounded-xl shadow-xl flex items-center justify-between">
                      <div>
                        <span className="text-[10px] font-bold text-slate-400 uppercase tracking-wider">Barometric Index</span>
                        <h3 className="text-xl font-black text-white mt-1.5 font-mono">{activeFocusNode.pressure.toFixed(1)}<span className="text-[10px] font-normal text-slate-500 ml-0.5">hPa</span></h3>
                      </div>
                      <div className="p-3 rounded-xl bg-emerald-500/10 text-emerald-400 border border-emerald-500/10"><Gauge className="h-5 w-5" /></div>
                    </div>
                  </div>

                  {/* Real-Time Power Signature Canvas + Embedded TinyML Insights Panel */}
                  <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
                    <div className="bg-slate-900/40 border border-slate-800 p-6 rounded-2xl shadow-xl lg:col-span-2">
                      <div className="flex justify-between items-center mb-6">
                        <div>
                          <h4 className="text-xs font-bold uppercase tracking-wider text-slate-200">Live Computational Array Power Waveform</h4>
                          <p className="text-[11px] text-slate-500">Real-time solar panel transformation footprint (Watts = Volts * Amps)</p>
                        </div>
                        {activeFocusNode.history.length > 0 && (
                          <div className="text-xs font-mono bg-slate-950 px-3 py-1 rounded-lg border border-slate-800 text-indigo-400 font-bold">
                            Current Draw: {activeFocusNode.history[activeFocusNode.history.length - 1].power.toFixed(3)} W
                          </div>
                        )}
                      </div>
                      <div className="h-64">
                        <ResponsiveContainer width="100%" height="100%">
                          <AreaChart data={activeFocusNode.history} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
                            <defs>
                              <linearGradient id="powerIndi" x1="0" y1="0" x2="0" y2="1">
                                <stop offset="5%" stopColor="#6366f1" stopOpacity={0.25}/>
                                <stop offset="95%" stopColor="#6366f1" stopOpacity={0}/>
                              </linearGradient>
                            </defs>
                            <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                            <XAxis dataKey="time" stroke="#64748b" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                            <YAxis stroke="#64748b" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                            <Tooltip contentStyle={{ backgroundColor: '#0f172a', borderColor: '#334155' }} />
                            <Area type="monotone" dataKey="power" name="Power Signature (W)" stroke="#6366f1" fillOpacity={1} fill="url(#powerIndi)" strokeWidth={2} />
                          </AreaChart>
                        </ResponsiveContainer>
                      </div>
                    </div>

                    {/* Edge-AI / Local Intelligence Monitoring Card */}
                    <div className="bg-slate-900/40 border border-slate-800 p-6 rounded-2xl shadow-xl flex flex-col justify-between">
                      <div>
                        <h4 className="text-xs font-bold uppercase tracking-wider text-slate-200 flex items-center gap-2 mb-4">
                          <Cpu className="w-4 h-4 text-indigo-400 animate-spin" style={{ animationDuration: '6s' }} /> TinyML Inference Engine
                        </h4>
                        <div className="bg-slate-950 p-4 rounded-xl border border-slate-800 font-mono text-xs space-y-3">
                          <div className="flex justify-between border-b border-slate-900 pb-2">
                            <span className="text-slate-500">Model Pipeline:</span>
                            <span className="text-slate-400 font-bold">EdgeForest v4.1</span>
                          </div>
                          <div className="flex justify-between border-b border-slate-900 pb-2">
                            <span className="text-slate-500">Inference State:</span>
                            <span className="text-indigo-400 font-bold uppercase">{activeFocusNode.ai_insights.state}</span>
                          </div>
                          <div className="flex justify-between">
                            <span className="text-slate-500">Precipitation Index:</span>
                            <span className="text-emerald-400 font-bold">{activeFocusNode.ai_insights.rain_prob}%</span>
                          </div>
                        </div>
                      </div>

                      <div className="space-y-2 mt-4">
                        <div className="flex justify-between text-[11px] font-bold uppercase tracking-wide">
                          <span className="text-slate-500">Thermal Envelope Cap</span>
                          <span className="text-slate-300 font-mono">{activeFocusNode.temp.toFixed(1)}°C / {activeFocusNode.ai_insights.envelope_high}°C</span>
                        </div>
                        <div className="w-full bg-slate-950 h-2 rounded-full overflow-hidden border border-slate-800">
                          <div 
                            className="bg-indigo-500 h-full transition-all duration-500"
                            style={{ width: `${Math.min(100, (activeFocusNode.temp / activeFocusNode.ai_insights.envelope_high) * 100)}%` }}
                          />
                        </div>
                      </div>
                    </div>
                  </div>
                </>
              ) : (
                <div className="border border-dashed border-slate-800 rounded-2xl p-16 text-center text-slate-500">
                  <ShieldAlert className="h-8 w-8 mx-auto text-slate-600 mb-3" />
                  No direct telemetry frames found. Verify gateway USB serial parser stream configurations.
                </div>
              )}
            </div>
          )}

          {/* TAB 2: DETAILED SYSTEM LINEAR ANALYTICS */}
          {activeTab === 'analytics' && (
            <div className="bg-slate-900/40 border border-slate-800 p-6 rounded-2xl shadow-xl space-y-6">
              <div>
                <h4 className="text-xs font-bold uppercase tracking-wider text-slate-200">Dual-Axis Bus Vector Analytical Chart</h4>
                <p className="text-[11px] text-slate-500">Comparative inspection of panel charge profiles and cross-current load variations</p>
              </div>
              {activeFocusNode && activeFocusNode.history.length > 0 ? (
                <div className="h-96">
                  <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={activeFocusNode.history} margin={{ top: 5, right: 10, left: -20, bottom: 5 }}>
                      <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                      <XAxis dataKey="time" stroke="#64748b" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                      <YAxis yAxisId="left" stroke="#f59e0b" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                      <YAxis yAxisId="right" orientation="right" stroke="#0ea5e9" tick={{ fontSize: 10, fontFamily: 'monospace' }} />
                      <Tooltip contentStyle={{ backgroundColor: '#0f172a', borderColor: '#334155' }} />
                      <Legend wrapperStyle={{ fontSize: '11px', textTransform: 'uppercase', letterSpacing: '0.05em', fontWeight: 'bold' }} />
                      <Line yAxisId="left" type="monotone" dataKey="voltage" stroke="#f59e0b" strokeWidth={2.5} dot={false} name="Bus Potential (V)" />
                      <Line yAxisId="right" type="monotone" dataKey="current" stroke="#0ea5e9" strokeWidth={2.5} dot={false} name="Dynamic Load (A)" />
                    </LineChart>
                  </ResponsiveContainer>
                </div>
              ) : (
                <p className="text-xs text-slate-600 italic">Insufficient historic matrix buffers logged to paint canvas timelines.</p>
              )}
            </div>
          )}

          {/* TAB 3: COMMAND ENTRY AND SETPOINT INTERCEPT CONFIGURATION */}
          {activeTab === 'controls' && (
            <div className="max-w-4xl grid grid-cols-1 md:grid-cols-3 gap-6">
              
              {/* Manual Downlink Injection Port Form */}
              <div className="md:col-span-2 bg-slate-900/40 border border-slate-800 p-8 rounded-2xl shadow-2xl">
                <h3 className="text-xs font-black uppercase text-slate-200 tracking-wider mb-1">Calibration Injection Panel</h3>
                <p className="text-[11px] text-slate-500 mb-6">Route low-level structural overrides down to active hardware arrays over ESP-NOW.</p>
                
                <form onSubmit={dispatchCommandArray} className="space-y-5">
                  <div>
                    <label className="block text-[10px] font-bold uppercase text-slate-400 tracking-wider mb-2">Target Tracker Addressing Byte (Hex / Dec Index)</label>
                    <input type="number" value={targetID} onChange={(e) => setTargetID(Number(e.target.value))} min="0" max="255" className="w-full bg-slate-950 border border-slate-800 rounded-xl px-4 py-3 font-mono text-xs text-slate-200 focus:border-indigo-500 outline-none transition" />
                    <span className="text-[10px] text-slate-600 mt-1.5 block">Set identifier to <span className="font-bold text-indigo-400">0</span> to execute a global network broadcast to all nodes concurrently.</span>
                  </div>

                  <div>
                    <label className="block text-[10px] font-bold uppercase text-slate-400 tracking-wider mb-2">Operational State Allocation Loop</label>
                    <select value={mode} onChange={(e) => setMode(Number(e.target.value))} className="w-full bg-slate-950 border border-slate-800 rounded-xl px-4 py-3 text-xs font-bold text-slate-300 focus:border-indigo-500 outline-none transition">
                      <option value={0}>State 00: Autonomous Analytical Photodiode Alignment</option>
                      <option value={1}>State 01: Low-Power Security Countermeasure Lock (Safe Stow)</option>
                      <option value={2}>State 02: Expert Mode Machine Learning Active Drift Bias</option>
                    </select>
                  </div>

                  <div className="grid grid-cols-2 gap-4">
                    <div>
                      <label className="block text-[10px] font-bold uppercase text-slate-400 tracking-wider mb-2">Dynamic Pan Bias Offset (°)</label>
                      <input type="number" value={biasPan} onChange={(e) => setBiasPan(Number(e.target.value))} min="-45" max="45" className="w-full bg-slate-950 border border-slate-800 rounded-xl px-4 py-3 font-mono text-xs text-slate-200 focus:border-indigo-500 outline-none transition" />
                    </div>
                    <div>
                      <label className="block text-[10px] font-bold uppercase text-slate-400 tracking-wider mb-2">Dynamic Tilt Bias Offset (°)</label>
                      <input type="number" value={biasTilt} onChange={(e) => setBiasTilt(Number(e.target.value))} min="-45" max="45" className="w-full bg-slate-950 border border-slate-800 rounded-xl px-4 py-3 font-mono text-xs text-slate-200 focus:border-indigo-500 outline-none transition" />
                    </div>
                  </div>

                  <button type="submit" className="w-full mt-4 bg-indigo-600 hover:bg-indigo-500 text-white font-bold text-xs uppercase tracking-widest py-3.5 px-4 rounded-xl shadow-lg shadow-indigo-600/10 transition outline-none">
                    Transmit Matrix Control Block
                  </button>
                </form>
              </div>

              {/* Auxiliary System Interlocks Panel (From the Uploaded Template) */}
              <div className="space-y-6">
                <div className="bg-slate-900/40 border border-slate-800 p-6 rounded-2xl shadow-xl space-y-4">
                  <h3 className="text-xs font-black uppercase tracking-wider text-slate-300 flex items-center gap-2">
                    <Settings className="w-4 h-4 text-indigo-400" /> Safety Interlocks
                  </h3>
                  <div className="flex justify-between items-center py-2.5 border-b border-slate-900">
                    <div>
                      <p className="text-xs font-bold text-slate-300">Overcurrent Protection</p>
                      <p className="text-[10px] text-slate-500">Trip hardware relays if current &gt; 1.5A</p>
                    </div>
                    <button className="p-2 bg-indigo-500/10 text-indigo-400 border border-indigo-500/20 rounded-xl"><Power className="w-4 h-4" /></button>
                  </div>
                  <div className="flex justify-between items-center py-2.5">
                    <div>
                      <p className="text-xs font-bold text-slate-300">Backup Local SD Flash Dump</p>
                      <p className="text-[10px] text-slate-500">Sync peripheral history dumps locally</p>
                    </div>
                    <button className="px-3 py-2 bg-slate-950 hover:bg-slate-900 text-slate-300 border border-slate-800 rounded-xl text-[10px] font-bold uppercase tracking-wider flex items-center gap-1.5 transition"><Download className="w-3.5 h-3.5" /> Sync</button>
                  </div>
                </div>

                <div className="bg-slate-900/40 border border-slate-800 p-6 rounded-2xl shadow-xl">
                  <h3 className="text-xs font-black uppercase tracking-wider text-slate-300 flex items-center gap-2 mb-3">
                    <Layers className="w-4 h-4 text-indigo-400" /> Mesh Constants
                  </h3>
                  <p className="text-[10px] text-slate-500 leading-relaxed">
                    Uplink configurations utilize standard JSON wrappers parsing into raw CSV lines on downstream platforms over dynamic UART interfaces.
                  </p>
                </div>
              </div>

            </div>
          )}

          {/* TAB 4: SYSTEM BACKBONE TRACE DIAGNOSTICS */}
          {activeTab === 'logs' && (
            <div className="bg-slate-950 border border-slate-800 rounded-2xl shadow-xl h-[calc(100vh-12rem)] flex flex-col overflow-hidden">
              <div className="px-6 py-4 border-b border-slate-800 bg-slate-900/40 flex justify-between items-center">
                <span className="text-xs font-black uppercase text-slate-400 tracking-wider">Live Network Interface Monitor</span>
                <span className="text-[10px] font-mono bg-slate-900 px-2 py-0.5 border border-slate-700 text-slate-400 rounded-md">Buffer Size: {logs.length} Lines</span>
              </div>
              <div className="p-6 flex-1 overflow-y-auto font-mono text-xs space-y-2.5 bg-slate-950 scrollbar-thin">
                {logs.length === 0 ? (
                  <p className="text-slate-600 italic">Awaiting structural connection strings inside telemetry interface lines...</p>
                ) : (
                  logs.map(log => (
                    <div key={log.id} className="flex gap-4 items-start select-text leading-relaxed">
                      <span className="text-slate-600">[{log.time}]</span>
                      <span className={`font-bold uppercase text-[9px] tracking-wider px-1.5 py-0.5 rounded border ${
                        log.type === 'success' ? 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20' : 
                        log.type === 'warn' ? 'bg-amber-500/10 text-amber-400 border-amber-500/20' : 
                        log.type === 'error' ? 'bg-rose-500/10 text-rose-400 border-rose-500/20' : 
                        'bg-slate-800/40 text-slate-400 border-slate-700'
                      }`}>{log.type}</span>
                      <span className="text-slate-300 flex-1">{log.body}</span>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}
        </div>
      </main>
    </div>
  );
}
