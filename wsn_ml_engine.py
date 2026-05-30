import os
import time
import serial
import threading
import numpy as np
import pandas as pd
from sklearn.cluster import KMeans
from sklearn.neighbors import KNeighborsClassifier
from sklearn.preprocessing import StandardScaler
import joblib

# =======================================================================
# HARDWARE GRAPH INTERFACE CONFIGURATION MATRIX
# =======================================================================
SERIAL_PORT = '/dev/ttyACM0'  # Windows: 'COM3' | Linux: '/dev/ttyACM0'
BAUD_RATE = 921600            # Must match CentralGateway.ino setup baud
DATA_FILE = "wsn_telemetry_history.csv"
SCALER_FILE = "scaler.pkl"
KMEANS_FILE = "kmeans.pkl"
KNN_FILE = "knn.pkl"

class WSNIntelligenceHub:
    def __init__(self):
        self.serial_conn = None
        self.is_running = True
        self.lock = threading.Lock()
        
        # Features used to define environmental/efficiency cluster spaces
        self.features = ["Voltage", "Current", "Temp", "Humidity", "Pressure"]
        
        # Core Machine Learning State Persistent Registers
        self.scaler = None
        self.kmeans = None
        self.knn = None
        self.model_ready = False
        
        self.init_storage_layer()
        self.load_or_train_models()

    def init_storage_layer(self):
        """Initializes raw telemetry log containers on local storage partition."""
        if not os.path.exists(DATA_FILE):
            with open(DATA_FILE, "w") as f:
                f.write("Timestamp,NodeID,Voltage,Current,Temp,Humidity,Pressure,Pan,Tilt\n")
            print(f"[STORAGE] Created structural logging system matrix: {DATA_FILE}")

    def load_or_train_models(self):
        """Attempts to load pre-trained models from disk; falls back to auto-bootstrap if logs exist."""
        try:
            if os.path.exists(SCALER_FILE) and os.path.exists(KMEANS_FILE) and os.path.exists(KNN_FILE):
                self.scaler = joblib.load(SCALER_FILE)
                self.kmeans = joblib.load(KMEANS_FILE)
                self.knn = joblib.load(KNN_FILE)
                self.model_ready = True
                print("[ML ENGINE] Production classification models successfully loaded from disk storage.")
            else:
                if os.path.exists(DATA_FILE) and os.path.getsize(DATA_FILE) > 500:
                    self.retrain_predictive_models()
                else:
                    print("[ML ENGINE] Insufficient data history. Running in Bootstrapping Mode to collect initial profiles.")
                    self.model_ready = False
        except Exception as e:
            print(f"[ML ENGINE] Critical error parsing tracking modules: {e}. Defaulting to data capture.")
            self.model_ready = False

    def retrain_predictive_models(self):
        """Runs the complete training pipeline using K-Means and k-NN wrappers to optimize parameters."""
        with self.lock:
            try:
                print("[ML ENGINE] Training data parsing cycle active...")
                df = pd.read_csv(DATA_FILE)
                
                # Check for minimum statistical density requirement
                if len(df) < 100:
                    print(f"[ML ENGINE] Log depth insufficient ({len(df)}/100 profiles). Postponing training optimization.")
                    return

                # Prevent unbounded memory creep by training on a rolling window of the last 50,000 points
                if len(df) > 50000:
                    df = df.tail(50000).reset_index(drop=True)

                X = df[self.features].values
                
                # Normalize values to prevent pressure variations from skewing your models
                self.scaler = StandardScaler()
                X_scaled = self.scaler.fit_transform(X)

                # Unsupervised State Discovery
                # Cluster mappings: 0=Normal, 1=High Overcast, 2=Severe Storm Front, 3=Thermal/Efficiency Anomaly
                self.kmeans = KMeans(n_clusters=4, random_state=42, n_init=10)
                cluster_labels = self.kmeans.fit_predict(X_scaled)

                # Train a live k-NN Classifier to calculate fast real-time boundary mappings
                self.knn = KNeighborsClassifier(n_neighbors=5, weights='distance')
                self.knn.fit(X_scaled, cluster_labels)

                # Export parameters to maintain system state persistence across runtime reboots
                joblib.dump(self.scaler, SCALER_FILE)
                joblib.dump(self.kmeans, KMEANS_FILE)
                joblib.dump(self.knn, KNN_FILE)
                
                self.model_ready = True
                print(f"[ML ENGINE] Model optimization sequence completed. Active dataset length: {len(df)} rows.")
            except Exception as e:
                print(f"[ML ENGINE] Critical error encountered during pipeline optimization execution: {e}")

    def evaluate_inference(self, node_id, telemetry_data):
        """Performs real-time pattern analysis on incoming metrics and sends hardware overrides when needed."""
        if not self.model_ready:
            return

        try:
            # Vector shape matching: [[Voltage, Current, Temp, Humidity, Pressure]]
            raw_vector = np.array([[
                telemetry_data["Voltage"], telemetry_data["Current"],
                telemetry_data["Temp"], telemetry_data["Humidity"], telemetry_data["Pressure"]
            ]])
            
            scaled_vector = self.scaler.transform(raw_vector)
            predicted_state = self.knn.predict(scaled_vector)[0]
            probabilities = self.knn.predict_proba(scaled_vector)[0]
            confidence = float(np.max(probabilities))

            # --- PREDICTIVE REASONING SYSTEM INTERFACE ---
            # Matches modes defined in EdgeNodeReceiver.h: 0=Normal, 1=Park Flat, 2=Applied Bias
            if predicted_state == 2 and confidence > 0.85:
                print(f"[🚨 ALARM] Approaching Storm Front on Node {node_id} (Confidence: {confidence:.2%})")
                # Structural Latch: Lock panels flat horizontally to handle high wind loading vectors
                self.transmit_hardware_override(node_id, mode=1, bias_pan=0, bias_tilt=0)
                
            elif predicted_state == 3 and confidence > 0.75:
                print(f"[💡 OPTIMIZATION] Micro-climate cloud/dust delta tracked on Node {node_id}.")
                # Adaptive Bias: Inject offset parameters to search for optimal scattered illumination
                self.transmit_hardware_override(node_id, mode=2, bias_pan=12, bias_tilt=-5)
                
            else:
                # Local Autonomy: Revert hardware execution safely back to photodiode control logic
                self.transmit_hardware_override(node_id, mode=0, bias_pan=0, bias_tilt=0)

        except Exception as e:
            print(f"[ML INFERENCE ERROR] Anomaly parsing operational loop array vectors: {e}")

    def transmit_hardware_override(self, node_id, mode, bias_pan, bias_tilt):
        """Serializes and sends structured commands back to the gateway bridge via the USB pipe."""
        if self.serial_conn and self.serial_conn.is_open:
            try:
                # Format perfectly matches CentralGateway.ino parsing requirements
                # Schema: CMD,TargetNodeID,Mode,BiasPan,BiasTilt\n
                node_int = int(node_id, 16)
                command_string = f"CMD,{node_int},{mode},{bias_pan},{bias_tilt}\n"
                
                self.serial_conn.write(command_string.encode('utf-8'))
                self.serial_conn.flush()
            except Exception as e:
                print(f"[SERIAL TX ERROR] Failed to transmit override command packet down USB link: {e}")

    def connect_serial_pipeline(self):
        """Establishes and maintains the physical serial connection with the central ESP32 bridge."""
        while self.is_running:
            try:
                print(f"[SERIAL] Attempting connection to hardware gateway interface on port {SERIAL_PORT}...")
                self.serial_conn = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.5)
                print(f"[SERIAL] Gateway link successfully established at {BAUD_RATE} baud.")
                
                # Ingestion execution loop
                while self.is_running and self.serial_conn.is_open:
                    if self.serial_conn.in_waiting > 0:
                        raw_line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                        
                        if not raw_line:
                            continue
                        
                        if raw_line.startswith("STATUS"):
                            print(f"[GATEWAY STATUS] {raw_line}")
                            
                        elif raw_line.startswith("DATA"):
                            # Parser Index Layout: DATA,NodeID,Voltage,Current,Temp,Humidity,Pressure,Pan,Tilt
                            parts = raw_line.split(',')
                            if len(parts) == 9:
                                node_id = parts[1]
                                t_metrics = {
                                    "Voltage": float(parts[2]), "Current": float(parts[3]),
                                    "Temp": float(parts[4]), "Humidity": float(parts[5]),
                                    "Pressure": float(parts[6])
                                }
                                
                                # Log telemetry data asynchronously to disk storage
                                timestamp = time.strftime('%Y-%m-%d %H:%M:%S')
                                with open(DATA_FILE, "a") as f:
                                    f.write(f"{timestamp},{raw_line[5:]}\n")
                                
                                # Process real-time machine learning verification bounds
                                self.evaluate_inference(node_id, t_metrics)
                                
            except (serial.SerialException, OSError) as e:
                print(f"[SERIAL DISCONNECT] Device link lost: {e}. Re-indexing interface in 5 seconds...")
                if self.serial_conn:
                    try: self.serial_conn.close()
                    except: pass
                time.sleep(5)

    def trigger_periodic_training_loop(self):
        """Background loop that periodically retrains models to account for seasonal weather changes."""
        # 86400 seconds = 24-hour training execution intervals
        interval = 86400 
        while self.is_running:
            for _ in range(interval):
                if not self.is_running: break
                time.sleep(1)
            if self.is_running:
                print("[AUTOMATION LOG] Running automated daily scheduled model retraining pipeline...")
                self.retrain_predictive_models()

    def boot(self):
        """Spins up background processing threads and begins main system operations."""
        self.serial_thread = threading.Thread(target=self.connect_serial_pipeline)
        self.training_thread = threading.Thread(target=self.trigger_periodic_training_loop)
        
        self.serial_thread.daemon = True
        self.training_thread.daemon = True
        
        self.serial_thread.start()
        self.training_thread.start()
        
        print("[SYSTEM STARTED] Central Python Intelligence Framework Engine Active.")
        try:
            while self.is_running:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\n[SHUTDOWN] Terminating ML processing loops gracefully. Disconnecting handles...")
            self.is_running = False
            if self.serial_conn:
                try: self.serial_conn.close()
                except: pass
            print("[SHUTDOWN] WSN Master Thread Stopped.")

if __name__ == "__main__":
    hub = WSNIntelligenceHub()
    hub.boot()