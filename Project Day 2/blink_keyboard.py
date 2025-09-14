import sys
import glob
import platform
try:
    import serial
except ImportError:
    print("pyserial not installed. Install it with: pip install pyserial")
    sys.exit(1)

from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout, QHBoxLayout, QLabel
from PySide6.QtCore import Qt, QTimer
from pynput.keyboard import Controller, Key

kb = Controller()

keyboard_rows = [
    list("1234567890"),
    list("QWERTYUIOP"),
    list("ASDFGHJKL"),
    list("ZXCVBNM"),
    ["SPACE", "DEL", "ENTER"]
]

SCAN_SPEED = 20  # milliseconds

# -----------------------------
# Auto-detect Arduino serial port (cross-platform)
# -----------------------------
def find_arduino_port():
    system = platform.system()
    ports = []
    if system == "Darwin":  # macOS
        ports = glob.glob('/dev/cu.*')
    elif system == "Linux":
        ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
    elif system == "Windows":
        import serial.tools.list_ports
        ports = [p.device for p in serial.tools.list_ports.comports()]

    for p in ports:
        if any(keyword in p.lower() for keyword in ["usbmodem", "usbserial", "arduino"]):
            return p
    # fallback: return first port if available
    return ports[0] if ports else None

SERIAL_PORT = find_arduino_port()
if SERIAL_PORT is None:
    print("No Arduino serial port found. Plug in your Arduino and restart.")
    sys.exit(1)
else:
    print(f"Using serial port: {SERIAL_PORT}")

# -----------------------------
# GUI class
# -----------------------------
class CenteredBlinkKeyboard(QWidget):
    def __init__(self, serial_port):
        super().__init__()
        self.setWindowTitle("Blink Keyboard")
        self.setStyleSheet("background-color: black;")

        try:
            self.serial = serial.Serial(serial_port, 115200, timeout=0.1)
        except Exception as e:
            print(f"Error opening serial port {serial_port}: {e}")
            self.serial = None

        self.current_word = ""
        self.row, self.col = 1, 0
        self.selecting_row = True

        layout = QVBoxLayout()
        self.word_label = QLabel("Current word:")
        self.word_label.setAlignment(Qt.AlignCenter)
        self.word_label.setStyleSheet("font-size: 28px; color: white;")
        layout.addWidget(self.word_label)

        self.labels = []
        for row_items in keyboard_rows:
            hbox = QHBoxLayout()
            label_row = []
            for item in row_items:
                lbl = QLabel(item)
                lbl.setFixedSize(60, 60)
                lbl.setAlignment(Qt.AlignCenter)
                lbl.setStyleSheet("color:white; border:1px solid #555; background:#111;")
                hbox.addWidget(lbl)
                label_row.append(lbl)
            layout.addLayout(hbox)
            self.labels.append(label_row)

        self.setLayout(layout)

        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)
        self.timer.start(SCAN_SPEED)
        self.update_display()

    def read_serial(self):
        if self.serial and self.serial.in_waiting:
            try:
                line = self.serial.readline().decode(errors="ignore").strip()
                if line.isdigit():
                    self.process_blink(int(line))
            except Exception as e:
                print("Serial read error:", e)

    def process_blink(self, blink):
        if blink == 1:
            if self.selecting_row:
                self.row = (self.row + 1) % len(keyboard_rows)
                if self.row == 0: self.row = 1
            else:
                self.col = (self.col + 1) % len(keyboard_rows[self.row])
        elif blink == 2:
            if self.selecting_row:
                self.selecting_row = False
                self.col = 0
            else:
                item = keyboard_rows[self.row][self.col]
                if item == "SPACE":
                    self.current_word += " "
                    kb.press(Key.space); kb.release(Key.space)
                elif item == "DEL" and self.current_word:
                    self.current_word = self.current_word[:-1]
                    kb.press(Key.backspace); kb.release(Key.backspace)
                elif item == "ENTER":
                    print("Final word:", self.current_word)
                    self.current_word = ""
                    kb.press(Key.enter); kb.release(Key.enter)
                else:
                    self.current_word += item
                    kb.press(item.lower()); kb.release(item.lower())
                self.selecting_row, self.row, self.col = True, 1, 0

        self.update_display()

    def update_display(self):
        self.word_label.setText(f"Current word: {self.current_word}")
        for r, row_items in enumerate(self.labels):
            for c, lbl in enumerate(row_items):
                highlight = (self.selecting_row and r == self.row) or (not self.selecting_row and r == self.row and c == self.col)
                lbl.setStyleSheet("color:white; border:2px solid #0f0; background:#333;" if highlight else "color:white; border:1px solid #555; background:#111;")

# -----------------------------
# Main
# -----------------------------
if __name__ == "__main__":
    app = QApplication(sys.argv)
    gui = CenteredBlinkKeyboard(SERIAL_PORT)
    gui.show()
    sys.exit(app.exec())
