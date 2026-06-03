import os
import csv
import time
import json
import queue
import threading
import asyncio
import numpy as np
import joblib
from datetime import datetime
from contextlib import asynccontextmanager

import serial
import serial.tools.list_ports
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from sklearn.cluster import KMeans
from sklearn.neighbors import KNeighborsClassifier
from sklearn.ensemble import RandomForestRegressor
from sklearn.preprocessing import StandardScaler

# --- HARDWARE & DATA CONFIGURATION ---
BAUD_RATE = 921600
ESP32S3_VID = 0x303A
ESP32S3_PID = 0x1001
DATA_FILE = "wsn_telemetry_history.csv"

# --- GLOBAL INTER-THREAD STATE ENGINE ---
class GlobalState:
    def __init__(self):
        self.latest_payload = {
            "telemetry": {
                "Voltage": 0.0, "Current": 0.0, "Temp": 0.0, "Humidity": 0.0, "Pressure": 0.0
            },
            "ai_insights": {
                "State": "Initializing", "Rain_Prob": 0.0, "High": 0.0, "Low": 0.0
            }
        }
        self.hardware_status = "Searching for ESP32-S3 Gateway..."
        self.new_data_event = threading.Event()  # Signals async loop from OS thread
        self.lock = threading.Lock()

state_engine = GlobalState()
hardware_command_queue = queue.Queue()  # Thread-safe pipeline for UI -> HW commands

# =====================================================================
# 1. MACHINE LEARNING ENGINE ARCHITECTURE
# =====================================================================
class WSNIntelligenceHub:
    def __init__(self):
        self.features = ["Voltage", "Current", "Temp", "Humidity", "Pressure"]
        self.scaler = None
        self.kmeans = None
        self.knn = None
        self.rf_high = None
        self.rf_low = None
        self.model_ready = False

        self._init_csv()
        self.load_or_bootstrap_models()

    def _init_csv(self):
        if not os.path.exists(DATA_FILE):
            with open(DATA_FILE, 'w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(["Timestamp"] + self.features)

    def load_or_bootstrap_models(self):
        try:
            if (os.path.exists("scaler.pkl") and os.path.exists("kmeans.pkl") and
                os.path.exists("knn.pkl") and os.path.exists("rf_high.pkl") and os.path.exists("rf_low.pkl")):
                self.scaler = joblib.load("scaler.pkl")
                self.kmeans = joblib.load("kmeans.pkl")
                self.knn = joblib.load("knn.pkl")
                self.rf_high = joblib.load("rf_high.pkl")
                self.rf_low = joblib.load("rf_low.pkl")
                self.model_ready = True
                print("[AI CORE] Pre-trained pipeline loaded successfully.")
            else:
                print("[AI CORE] Weights missing. Initiating local pipeline bootstrap...")
                self._bootstrap_synthetic_models()
        except Exception as e:
            print(f"[AI CORE ERROR] Load failure: {e}. Falling back to bootstrap.")
            self._bootstrap_synthetic_models()

    def _bootstrap_synthetic_models(self):
        np.random.seed(42)
        X = np.random.rand(500, 5)
        X[:, 0] = X[:, 0] * 5.0 + 3.0    # Volt (3-8V)
        X[:, 1] = X[:, 1] * 1.5          # Curr (0-1.5A)
        X[:, 2] = X[:, 2] * 20 + 20      # Temp (20-40C)
        X[:, 3] = X[:, 3] * 60 + 40      # Hum (40-100%)
        X[:, 4] = X[:, 4] * 40 + 980     # Press (980-1020hPa)

        y_rain = ((X[:, 3] > 80) & (X[:, 4] < 1005)).astype(int)
        y_high = X[:, 2] + np.random.normal(2, 1, 500)
        y_low = X[:, 2] - np.random.normal(3, 1, 500)

        self.scaler = StandardScaler().fit(X)
        X_scaled = self.scaler.transform(X)

        self.kmeans = KMeans(n_clusters=3, n_init=10, random_state=42).fit(X_scaled)
        self.knn = KNeighborsClassifier(n_neighbors=5).fit(X_scaled, y_rain)
        self.rf_high = RandomForestRegressor(n_estimators=20).fit(X_scaled, y_high)
        self.rf_low = RandomForestRegressor(n_estimators=20).fit(X_scaled, y_low)

        joblib.dump(self.scaler, "scaler.pkl")
        joblib.dump(self.kmeans, "kmeans.pkl")
        joblib.dump(self.knn, "knn.pkl")
        joblib.dump(self.rf_high, "rf_high.pkl")
        joblib.dump(self.rf_low, "rf_low.pkl")

        self.model_ready = True
        print("[AI CORE] Local baseline weights stabilized and written to disk.")

    def execute_inference(self, metrics: dict) -> dict:
        if not self.model_ready:
            return {"State": "Cold Boot", "Rain_Prob": 0.0, "High": 0.0, "Low": 0.0}

        raw = np.array([[metrics["Voltage"], metrics["Current"], metrics["Temp"],
                         metrics["Humidity"], metrics["Pressure"]]])
        scaled = self.scaler.transform(raw)

        cluster = self.kmeans.predict(scaled)[0]
        regime_map = {0: "Clear & Stable", 1: "High Humidity", 2: "Storm Precursor"}

        return {
            "State": regime_map.get(cluster, "Unstable Frame"),
            "Rain_Prob": float(self.knn.predict_proba(scaled)[0][1] * 100),
            "High": float(self.rf_high.predict(scaled)[0]),
            "Low": float(self.rf_low.predict(scaled)[0])
        }

# =====================================================================
# 2. BIDIRECTIONAL HARDWARE INTERFACE ENGINE (BACKGROUND OS THREAD)
# =====================================================================
def hardware_serial_worker(hub: WSNIntelligenceHub):
    """Monitors, reads telemetry from, and writes commands to the physical ESP32-S3."""
    while True:
        ports = serial.tools.list_ports.comports()
        target_port = next((p.device for p in ports if p.vid == ESP32S3_VID and p.pid == ESP32S3_PID), None)

        if target_port:
            print(f"[HARDWARE] Node discovered on {target_port}. Attaching serial link...")
            try:
                ser = serial.Serial(target_port, BAUD_RATE, timeout=0.1)
                ser.reset_input_buffer()
                ser.reset_output_buffer()

                while True:
                    # --- 1. TELEMETRY INGRESS (READ) ---
                    if ser.in_waiting > 0:
                        raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
                        if raw_line.startswith("DATA"):
                            parts = raw_line.split(',')
                            if len(parts) >= 7:
                                try:
                                    metrics = {
                                        "Voltage": float(parts[2]),
                                        "Current": float(parts[3]),
                                        "Temp": float(parts[4]),
                                        "Humidity": float(parts[5]),
                                        "Pressure": float(parts[6])
                                    }
                                    ai_insights = hub.execute_inference(metrics)

                                    with state_engine.lock:
                                        state_engine.latest_payload = {
                                            "telemetry": metrics,
                                            "ai_insights": ai_insights
                                        }
                                        state_engine.hardware_status = "Live"

                                    # Release async wait loops simultaneously
                                    state_engine.new_data_event.set()

                                    with open(DATA_FILE, 'a', newline='') as f:
                                        writer = csv.writer(f)
                                        writer.writerow([datetime.now().isoformat()] + list(metrics.values()))
                                except (ValueError, IndexError):
                                    continue

                    # --- 2. COMMAND EGRESS (WRITE) ---
                    try:
                        cmd_packet = hardware_command_queue.get_nowait()
                        action = cmd_packet.get("action")
                        value = cmd_packet.get("value")

                        serial_cmd = f"CMD,{action},{value}\n"
                        ser.write(serial_cmd.encode('utf-8'))
                        ser.flush()
                        print(f"[HARDWARE OUTBOUND] Flushed downstream: {serial_cmd.strip()}")
                        hardware_command_queue.task_done()
                    except queue.Empty:
                        pass

                    time.sleep(0.005) # Prevent CPU core pinning

            except (serial.SerialException, OSError):
                print("[HARDWARE WARNING] Connection broken. Scanning for interface drop...")
                with state_engine.lock:
                    state_engine.hardware_status = "Disconnected"
                time.sleep(2)
        else:
            with state_engine.lock:
                state_engine.hardware_status = "Searching..."
            time.sleep(2)

# =====================================================================
# 3. HIGH-SPEED ASYNC WEBSOCKET INFRASTRUCTURE & LIFESPAN
# =====================================================================
@asynccontextmanager
async def lifespan(app: FastAPI):
    """Handles thread generation and memory lifecycle safely on startup/shutdown."""
    ai_hub = WSNIntelligenceHub()
    worker_thread = threading.Thread(target=hardware_serial_worker, args=(ai_hub,), daemon=True)
    worker_thread.start()
    print("[SYSTEM] Background hardware abstraction layer successfully spawned via Lifespan.")
    yield
    print("[SYSTEM] Shutting down full-duplex gateway pipelines...")

app = FastAPI(
    title="Sky-Link Gateway AI Core",
    version="2.0.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/api/health")
async def health_check():
    return {
        "status": "online",
        "hardware_link": state_engine.hardware_status,
        "timestamp": datetime.now().isoformat()
    }

@app.websocket("/ws/telemetry")
async def websocket_telemetry_stream(websocket: WebSocket):
    """Establishes full-duplex non-blocking channel to push metrics and ingest UI controls."""
    await websocket.accept()
    print(f"[WEBSOCKET] Frontend UI Client connected from {websocket.client.host}")

    async def send_handler():
        with state_engine.lock:
            await websocket.send_json(state_engine.latest_payload)

        loop = asyncio.get_running_loop()
        while True:
            # Offloads native thread waiting to threadpool, eliminating CPU spin logs completely
            await loop.run_in_executor(None, state_engine.new_data_event.wait)
            state_engine.new_data_event.clear()

            with state_engine.lock:
                payload = state_engine.latest_payload
            await websocket.send_json(payload)

    async def receive_handler():
        try:
            while True:
                ui_command = await websocket.receive_json()
                print(f"[WEBSOCKET INBOUND] Received action framework: {ui_command}")
                hardware_command_queue.put(ui_command)
        except WebSocketDisconnect:
            raise

    try:
        await asyncio.gather(send_handler(), receive_handler())
    except WebSocketDisconnect:
        print(f"[WEBSOCKET] Client browser session disconnected cleanly.")
    except Exception as e:
        print(f"[WEBSOCKET ERROR] Stream exception handled: {e}")
