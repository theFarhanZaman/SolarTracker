import os
import time
import threading
import serial
import serial.tools.list_ports
import pandas as pd
import numpy as np
import joblib
import tkinter as tk
from tkinter import ttk
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
from sklearn.cluster import KMeans
from sklearn.neighbors import KNeighborsClassifier
from sklearn.preprocessing import StandardScaler

# --- CONFIGURATION ---
BAUD_RATE = 921600
ESP32S3_VID = 0x303A
ESP32S3_PID = 0x1001
DATA_FILE = "wsn_telemetry_history.csv"

class WSNIntelligenceHub:
    def __init__(self):
        self.features = ["Voltage", "Current", "Temp", "Humidity", "Pressure"]
        self.scaler = None
        self.knn = None
        self.model_ready = False
        self.current_data = {"Voltage": 0, "Current": 0, "Temp": 0, "Humidity": 0, "Pressure": 0}
        self.status = "Initializing..."
        self.load_models()

    def load_models(self):
        if os.path.exists("scaler.pkl"):
            self.scaler = joblib.load("scaler.pkl")
            self.knn = joblib.load("knn.pkl")
            self.model_ready = True
            self.status = "Model Loaded"

    def process_telemetry(self, t_metrics):
        self.current_data = t_metrics
        if self.model_ready:
            # Simple Inference
            raw = np.array([[t_metrics["Voltage"], t_metrics["Current"], t_metrics["Temp"], 
                             t_metrics["Humidity"], t_metrics["Pressure"]]])
            scaled = self.scaler.transform(raw)
            pred = self.knn.predict(scaled)[0]
            self.status = f"State Mode: {pred}"
            return pred
        return 0

class DashboardGUI:
    def __init__(self, root, hub):
        self.root = root
        self.hub = hub
        self.root.title("WSN Intelligence Solar Tracker")
        self.root.geometry("800x600")

        # Setup Plotting
        self.fig = Figure(figsize=(5, 4), dpi=100)
        self.ax = self.fig.add_subplot(111)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.root)
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=1)

        # Labels
        self.status_label = ttk.Label(root, text="System: Starting...", font=("Arial", 12))
        self.status_label.pack(pady=10)

        self.update_gui()

    def update_gui(self):
        # Clear and plot latest
        self.ax.clear()
        self.ax.plot([self.hub.current_data["Voltage"]], [self.hub.current_data["Current"]], 'ro')
        self.ax.set_title("Live Energy Harvest (V vs A)")
        self.ax.set_xlabel("Voltage")
        self.ax.set_ylabel("Current")
        self.canvas.draw()
        
        self.status_label.config(text=f"Status: {self.hub.status} | V:{self.hub.current_data['Voltage']} | I:{self.hub.current_data['Current']}")
        self.root.after(1000, self.update_gui)

def serial_worker(hub):
    """Background hardware listener."""
    while True:
        ports = serial.tools.list_ports.comports()
        port = next((p.device for p in ports if p.vid == ESP32S3_VID and p.pid == ESP32S3_PID), None)
        
        if port:
            try:
                ser = serial.Serial(port, BAUD_RATE, timeout=1)
                while True:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line.startswith("DATA"):
                        parts = line.split(',')
                        if len(parts) == 9:
                            metrics = {"Voltage": float(parts[2]), "Current": float(parts[3]), 
                                       "Temp": float(parts[4]), "Humidity": float(parts[5]), "Pressure": float(parts[6])}
                            hub.process_telemetry(metrics)
            except:
                time.sleep(2)
        else:
            hub.status = "Searching for Gateway..."
            time.sleep(2)

if __name__ == "__main__":
    hub = WSNIntelligenceHub()
    root = tk.Tk()
    app = DashboardGUI(root, hub)
    
    # Start Hardware Listener in Background
    thread = threading.Thread(target=serial_worker, args=(hub,), daemon=True)
    thread.start()
    
    root.mainloop()
