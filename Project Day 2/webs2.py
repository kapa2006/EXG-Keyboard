import streamlit as st
import glob
import platform
import json
import os

SETTINGS_FILE = "blink_settings.json"

# -----------------------------
# Cross-platform auto serial detection
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
    return ports[0] if ports else None

# -----------------------------
# Load / Save Settings
# -----------------------------
def load_settings():
    if os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "r") as f:
            return json.load(f)
    return {}

def save_settings(settings):
    with open(SETTINGS_FILE, "w") as f:
        json.dump(settings, f)

# -----------------------------
# Streamlit UI
# -----------------------------
st.set_page_config(page_title="Blink Keyboard Settings", layout="wide")
st.title("Blink Keyboard — Settings")

# Load previous settings
settings = load_settings()
default_port = settings.get("serial_port", find_arduino_port() or "")
default_baud = settings.get("baud_rate", 115200)

# Serial port selection (auto-detected)
st.write("### Serial Port")
st.write("Automatically detected port (if available) is pre-selected.")
ports = [default_port] if default_port else []
user_port = st.text_input("Serial Port:", value=default_port)

# Baud rate dropdown
st.write("### Baud Rate")
baud_options = [9600, 19200, 38400, 57600, 115200, 230400]
baud = st.selectbox("Select Baud Rate:", baud_options, index=baud_options.index(default_baud))

# Save button
if st.button("Save Settings"):
    settings["serial_port"] = user_port
    settings["baud_rate"] = baud
    save_settings(settings)
    st.success(f"Settings saved! Serial port: {user_port}, Baud rate: {baud}")

# Display current detected port info
st.write("---")
st.write(f"Auto-detected serial port: {find_arduino_port() or 'None found'}")
st.write("On Windows, it will list COM ports. On macOS/Linux, it will list /dev/ttyUSB* or /dev/cu.* devices.")
