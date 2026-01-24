import sys
import re
import serial
from collections import deque
from PyQt5 import QtWidgets, QtCore
import pyqtgraph as pg
import os
from datetime import datetime
sys.path.append('/home/tpt-finder/Desktop/brl_data/brl_data')
import brl_data

today = datetime.now().strftime("%Y-%m-%d")
data_subfolder = os.path.join("data", today)
os.makedirs(data_subfolder, exist_ok=True)

df = brl_data.datafile(
    descrip_str="ESP32_Impedance_Monitor",
    inv_init="TPT",
    testtype="single"
)

df.set_folders(
    datafolder=data_subfolder,
    gitfolder="."
)

df.set_metadata(
    names=["Impedance", "Phase", "Material", "Trial", "TestTaker"],
    types=[float, float, str, int, str],
    notes=[
        "Measured impedance magnitude (Ω)",
        "Measured phase (°)",
        "Material under test",
        "Trial number for this material",
        "Name of test taker (from GUI)"
    ]
)

df.open(mode="w")


class SerialReader(QtCore.QThread):
    impedance_received = QtCore.pyqtSignal(float, float)

    def __init__(self, port, baudrate=115200):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.running = True

    def run(self):
        try:
            ser = serial.Serial(self.port, self.baudrate, timeout=1)
        except Exception as e:
            print("Failed to open serial port:", e)
            return

        impedance_pattern = re.compile(r"impedance magnitude: ([0-9]+\.[0-9]+)")
        phase_pattern = re.compile(r"Calculated phase: (-?[0-9]+\.[0-9]+)")

        impedance = None
        phase = None

        while self.running:
            try:
                if not ser.is_open:
                    break
                line = ser.readline().decode('utf-8', errors='ignore').strip()

                imp_match = impedance_pattern.search(line)
                if imp_match:
                    impedance = float(imp_match.group(1))

                phase_match = phase_pattern.search(line)
                if phase_match:
                    phase = float(phase_match.group(1))

                if impedance is not None and phase is not None:
                    self.impedance_received.emit(impedance, phase)
                    impedance = None
                    phase = None

            except Exception as e:
                print("Serial read error:", e)
                break

        if ser.is_open:
            ser.close()

    def stop(self):
        self.running = False
        self.wait(1000)


class ImpedanceGUI(QtWidgets.QMainWindow):
    def __init__(self, port):
        super().__init__()
        self.setWindowTitle("ESP32 Impedance Monitor")
        self.resize(600, 500)

        taker_layout = QtWidgets.QHBoxLayout()
        self.taker_label = QtWidgets.QLabel("Test Taker:")
        self.taker_input = QtWidgets.QLineEdit()
        self.taker_input.setPlaceholderText("Enter your name")
        taker_layout.addWidget(self.taker_label)
        taker_layout.addWidget(self.taker_input)

        material_layout = QtWidgets.QHBoxLayout()
        self.material_label = QtWidgets.QLabel("Material:")
        self.material_input = QtWidgets.QLineEdit()
        self.material_input.setPlaceholderText("Enter material name")
        material_layout.addWidget(self.material_label)
        material_layout.addWidget(self.material_input)

        self.label_impedance = QtWidgets.QLabel("Latest Impedance: -- Ω")
        self.label_impedance.setAlignment(QtCore.Qt.AlignCenter)
        self.label_impedance.setStyleSheet("font-size: 20px;")

        self.label_phase = QtWidgets.QLabel("Latest Phase: -- °")
        self.label_phase.setAlignment(QtCore.Qt.AlignCenter)
        self.label_phase.setStyleSheet("font-size: 16px;")

        self.label_recording = QtWidgets.QLabel("Recording: OFF")
        self.label_recording.setAlignment(QtCore.Qt.AlignCenter)
        self.label_recording.setStyleSheet("font-size: 16px; color: red;")

        self.plot_widget = pg.PlotWidget(title="Real-time Impedance Plot")
        self.plot_widget.setLabel('left', 'Impedance (Ω)')
        self.plot_widget.setLabel('bottom', 'Sample Index')
        self.plot_widget.showGrid(x=True, y=True)
        self.plot_curve = self.plot_widget.plot(pen=pg.mkPen(color='y', width=2))
        self.plot_data = deque(maxlen=200)

        layout = QtWidgets.QVBoxLayout()
        layout.addLayout(taker_layout)
        layout.addLayout(material_layout)
        layout.addWidget(self.label_impedance)
        layout.addWidget(self.label_phase)
        layout.addWidget(self.label_recording)
        layout.addWidget(self.plot_widget)

        central_widget = QtWidgets.QWidget()
        central_widget.setLayout(layout)
        self.setCentralWidget(central_widget)

        self.serial_reader = SerialReader(port)
        self.serial_reader.impedance_received.connect(self.handle_new_data)
        self.serial_reader.start()

        self.recording = False
        self.trial_counter = {} 
        self.trial_number = 0  

    def handle_new_data(self, impedance, phase):
        phase = phase - 293.738
        self.label_impedance.setText(f"Latest Impedance: {impedance:.2f} Ω")
        self.label_phase.setText(f"Latest Phase: {phase:.2f} °")
        self.plot_data.append(impedance)
        self.plot_curve.setData(list(self.plot_data))

        if self.recording:
            material_name = self.material_input.text().strip() or "Unknown"
            df.write([impedance, phase, material_name, self.trial_number,
                      df.metadata.d.get("TestTaker", "Unknown")])

    def keyPressEvent(self, event):
        if event.key() == QtCore.Qt.Key_Space:
            self.recording = not self.recording
            material_name = self.material_input.text().strip() or "Unknown"

            if self.recording:
                test_taker = self.taker_input.text().strip() or "Unknown"
                df.metadata.d["TestTaker"] = test_taker
                print(f"Metadata updated: TestTaker = {test_taker}")

                if material_name not in self.trial_counter:
                    self.trial_counter[material_name] = 1
                else:
                    self.trial_counter[material_name] += 1

                self.trial_number = self.trial_counter[material_name]

                self.label_recording.setText(f"Recording: ON (Trial {self.trial_number})")
                self.label_recording.setStyleSheet("font-size: 16px; color: green;")

                print(f"Recording started for material: {material_name}, Trial {self.trial_number}")

            else:
                self.label_recording.setText("Recording: OFF")
                self.label_recording.setStyleSheet("font-size: 16px; color: red;")
                print("Recording stopped")

    def closeEvent(self, event):
        self.serial_reader.stop()
        df.close()
        event.accept()


if __name__ == "__main__":
    port = "/dev/ttyACM0"
    app = QtWidgets.QApplication(sys.argv)
    window = ImpedanceGUI(port)
    window.show()
    sys.exit(app.exec_())
