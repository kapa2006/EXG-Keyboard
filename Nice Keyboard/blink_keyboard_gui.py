import sys
import serial
from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout, QHBoxLayout, QLabel, QSpacerItem, QSizePolicy
from PySide6.QtCore import Qt, QTimer
from pynput.keyboard import Controller, Key

SERIAL_PORT = '/dev/cu.usbmodem21301'  # Adjust as needed
BAUD_RATE = 115200

keyboard_rows = [
    list("1234567890"),
    list("QWERTYUIOP"),
    list("ASDFGHJKL"),
    list("ZXCVBNM"),
    ["SPACE", "DEL", "ENTER"]
]

kb = Controller()

class CenteredBlinkKeyboard(QWidget):
    def __init__(self, serial_port):
        super().__init__()
        self.setWindowTitle("Blink Keyboard (Centered)")
        self.setWindowFlags(self.windowFlags() | Qt.WindowStaysOnTopHint)
        self.setStyleSheet("background-color: black;")

        self.serial = serial.Serial(serial_port, BAUD_RATE, timeout=0.1)
        self.current_word = ""
        self.row = 1  # start scanning top letters
        self.col = 0
        self.selecting_row = True

        self.layout = QVBoxLayout()
        self.layout.setContentsMargins(20, 20, 20, 20)
        self.layout.setSpacing(10)

        # Current word display
        self.word_label = QLabel("Current word: ")
        self.word_label.setAlignment(Qt.AlignCenter)
        self.word_label.setStyleSheet("font-size: 28px; font-weight: bold; color: white;")
        self.layout.addWidget(self.word_label)

        # Centered keyboard layout
        self.labels = []
        for r, row_items in enumerate(keyboard_rows):
            hbox = QHBoxLayout()
            hbox.addStretch()  # left spacer
            label_row = []
            for item in row_items:
                lbl = QLabel(str(item))
                lbl.setAlignment(Qt.AlignCenter)
                lbl.setFixedSize(60, 60)
                lbl.setStyleSheet(
                    "color: white; font-size: 20px; border: 1px solid #555; border-radius: 8px; background-color: #111;"
                )
                hbox.addWidget(lbl)
                label_row.append(lbl)
            hbox.addStretch()  # right spacer
            self.layout.addLayout(hbox)
            self.labels.append(label_row)

        self.setLayout(self.layout)

        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)
        self.timer.start(20)

        self.update_display()

    def read_serial(self):
        try:
            if self.serial.in_waiting:
                line = self.serial.readline().decode().strip()
                if line.isdigit():
                    blink = int(line)
                    if blink in [1, 2]:
                        self.process_blink(blink)
        except Exception as e:
            print("Serial error:", e)

    def process_blink(self, blink):
        if blink == 1:  # advance
            if self.selecting_row:
                self.row = (self.row + 1) % len(keyboard_rows)
                if self.row == 0:
                    self.row = 1
            else:
                self.col = (self.col + 1) % len(keyboard_rows[self.row])
        elif blink == 2:  # confirm
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
                # Return to row scanning
                self.selecting_row = True
                self.row = 1
                self.col = 0

        self.update_display()

    def update_display(self):
        self.word_label.setText(f"Current word: {self.current_word}")
        for r, row_items in enumerate(self.labels):
            for c, lbl in enumerate(row_items):
                base_style = "color: white; font-size: 20px; border-radius: 8px;"
                if self.selecting_row and r == self.row:
                    bg = "#222" if r != 0 else "#333"
                    lbl.setStyleSheet(f"{base_style} border: 2px solid #00f; background-color: {bg};")
                elif not self.selecting_row and r == self.row and c == self.col:
                    bg = "#008000" if r !=0 else "#556B2F"
                    lbl.setStyleSheet(f"{base_style} border: 2px solid #0f0; background-color: {bg};")
                else:
                    lbl.setStyleSheet(f"{base_style} border: 1px solid #555; background-color: #111;")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    gui = CenteredBlinkKeyboard(SERIAL_PORT)
    gui.show()
    sys.exit(app.exec())
