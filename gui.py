import sys
import re
import serial
import threading
from collections import deque
from PyQt5 import QtWidgets, QtCore
import pyqtgraph as pg
import logging
import sys
sys.path.append('/home/tpt-finder/Desktop/brl_data/brl_data')
import brl_data


'''
Instructions:
run deactivate if (venv) is active
run source esp32-env/bin/activate

calibrate by clicking on the monitor at the bottom of the ide to run the c code
make sure to exit the serial monitor before running this gui
run python gui.py
'''

df = brl_data.datafile(
    descrip_str="ESP32_Impedance_Monitor",
    inv_init="TPT",               # your initials
    testtype="single"             # or 'simulation' or 'longterm'
)

# You must set folders before opening
df.set_folders(
    datafolder="data",            # folder where CSV data will be saved
    gitfolder="."                 # path to your Git repo (or '.' if not using git)
)

# Set metadata columns
df.set_metadata(
    names=["Impedance", "Phase"],
    types=[float, float],
    notes=["Measured impedance magnitude (Ω)", "Measured phase (°)"]
)

df.open(mode="w")


class SerialReader(QtCore.QObject):
    impedance_received = QtCore.pyqtSignal(float, float)

    def __init__(self, port, baudrate=115200):
        super().__init__()
        self.ser = serial.Serial(port, baudrate, timeout=1)
        self.running = True
        self.thread = threading.Thread(target=self.read_serial)
        self.thread.daemon = True
        self.thread.start()

    def read_serial(self):
        impedance_pattern = re.compile(r"impedance magnitude: ([0-9]+\.[0-9]+)")
        phase_pattern = re.compile(r"Calculated phase: (-?[0-9]+\.[0-9]+)")

        impedance = None
        phase = None

        while self.running:
            try:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()

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

    def stop(self):
        self.running = False
        if self.ser.is_open:
            self.ser.close()


class ImpedanceGUI(QtWidgets.QMainWindow):
    def __init__(self, port):
        super().__init__()
        self.setWindowTitle("ESP32 Impedance Monitor")
        self.resize(600, 400)

        self.label_impedance = QtWidgets.QLabel("Latest Impedance: -- Ω", self)
        self.label_impedance.setAlignment(QtCore.Qt.AlignCenter)
        self.label_impedance.setStyleSheet("font-size: 20px;")

        self.label_phase = QtWidgets.QLabel("Latest Phase: -- °", self)
        self.label_phase.setAlignment(QtCore.Qt.AlignCenter)
        self.label_phase.setStyleSheet("font-size: 16px;")

        self.plot_widget = pg.PlotWidget(title="Real-time Impedance Plot")
        self.plot_widget.setLabel('left', 'Impedance (Ω)')
        self.plot_widget.setLabel('bottom', 'Sample Index')
        self.plot_widget.showGrid(x=True, y=True)
        self.plot_curve = self.plot_widget.plot(pen=pg.mkPen(color='y', width=2))
        self.plot_data = deque(maxlen=200)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.label_impedance)
        layout.addWidget(self.label_phase)
        layout.addWidget(self.plot_widget)

        central_widget = QtWidgets.QWidget()
        central_widget.setLayout(layout)
        self.setCentralWidget(central_widget)

        self.serial_reader = SerialReader(port)
        self.serial_reader.impedance_received.connect(self.handle_new_data)
        self.last_log_time = 0

    def handle_new_data(self, impedance, phase):
        phase = phase - 293.738
        self.label_impedance.setText(f"Latest Impedance: {impedance:.2f} Ω")
        self.label_phase.setText(f"Latest Phase: {phase:.2f} °")
        self.plot_data.append(impedance)
        self.plot_curve.setData(list(self.plot_data))

        # log to brl_data
        df.write([impedance, phase])


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
