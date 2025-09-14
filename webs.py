# app.py
import streamlit as st
import serial
import time

# -----------------------------
# Config & UI: sidebar controls
# -----------------------------
st.set_page_config(page_title="BlinkShift - EOG Keyboard", layout="wide")
st.sidebar.header("Connection / Settings")

port = st.sidebar.text_input("Serial port (e.g. COM3 or /dev/ttyUSB0)", value="COM3")

# ✅ Replace number_input with a dropdown of common baud rates
baud_rates = [300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600]
baud = st.sidebar.selectbox("Baud rate", baud_rates, index=baud_rates.index(115200))

poll_interval = st.sidebar.slider(
    "Poll interval (s)", 
    min_value=0.05, max_value=1.0, value=0.2, step=0.05
)

if "connected" not in st.session_state:
    st.session_state.connected = False
if "serial" not in st.session_state:
    st.session_state.serial = None

# Connect button
if st.sidebar.button("Connect"):
    try:
        # close existing if open
        if st.session_state.serial and st.session_state.serial.is_open:
            st.session_state.serial.close()
        st.session_state.serial = serial.Serial(port, baud, timeout=0.1)
        time.sleep(0.5)  # give device time to initialize
        st.session_state.connected = True
        st.sidebar.success(f"Connected to {port} @ {baud}")
    except Exception as e:
        st.session_state.connected = False
        st.session_state.serial = None
        st.sidebar.error(f"Failed to connect: {e}")

if st.sidebar.button("Disconnect"):
    if st.session_state.serial:
        try:
            st.session_state.serial.close()
        except:
            pass
    st.session_state.serial = None
    st.session_state.connected = False
    st.sidebar.info("Disconnected")

# -----------------------------
# App state init
# -----------------------------
if "current_word" not in st.session_state:
    st.session_state.current_word = ""
if "row" not in st.session_state:
    st.session_state.row = 0
if "col" not in st.session_state:
    st.session_state.col = 0
if "selecting_row" not in st.session_state:
    st.session_state.selecting_row = True
if "scanning" not in st.session_state:
    st.session_state.scanning = False

# -----------------------------
# Keyboard layout (QWERTY)
# -----------------------------
keyboard_rows = [
    ["1","2","3","4","5","6","7","8","9","0"],
    ["Q","W","E","R","T","Y","U","I","O","P"],
    ["A","S","D","F","G","H","J","K","L"],
    ["Z","X","C","V","B","N","M"],
    ["SPACE","DEL","ENTER"]
]
flat_keys = [k for row in keyboard_rows for k in row]

# -----------------------------
# Top-level UI
# -----------------------------
st.title("BlinkShift — EOG / EMG Keyboard")
st.write("Use your Arduino EOG blink detector to navigate and select keys. Connect, Start Scanning, then blink.")

# show typed text box
st.markdown("**Typed text**")
st.text_area("Typed Output", value=st.session_state.current_word, height=140, key="typed_box")

# start/stop scanning
col1, col2 = st.columns(2)
with col1:
    if st.button("▶ Start Scanning"):
        if not st.session_state.connected or st.session_state.serial is None:
            st.warning("Please connect to the serial port first (sidebar).")
        else:
            st.session_state.scanning = True
with col2:
    if st.button("⏹ Stop Scanning"):
        st.session_state.scanning = False

# -----------------------------
# Helper functions
# -----------------------------
def read_serial_line():
    """Read one line from serial if available and return trimmed string, else None."""
    ser = st.session_state.serial
    if ser is None:
        return None
    try:
        if ser.in_waiting:
            raw = ser.readline().decode(errors="ignore").strip()
            if raw != "":
                return raw
    except Exception as e:
        st.sidebar.error(f"Serial read error: {e}")
    return None

def process_blink_code(code):
    """
    Process blink code from Arduino.
    Mapping convention (example):
      1 -> advance (move right / next)
      2 -> confirm (select)
      3 -> move left (optional)
    You can adapt mapping if your Arduino prints different numbers.
    """
    try:
        blink = int(code)
    except:
        return

    if blink == 1:
        # advance
        if st.session_state.selecting_row:
            st.session_state.row = (st.session_state.row + 1) % len(keyboard_rows)
        else:
            st.session_state.col = (st.session_state.col + 1) % len(keyboard_rows[st.session_state.row])
    elif blink == 2:
        # confirm / select
        if st.session_state.selecting_row:
            st.session_state.selecting_row = False
            st.session_state.col = 0
        else:
            item = keyboard_rows[st.session_state.row][st.session_state.col]
            if item == "SPACE":
                st.session_state.current_word += " "
            elif item == "DEL":
                st.session_state.current_word = st.session_state.current_word[:-1]
            elif item == "ENTER":
                st.session_state.current_word += "\n"
            else:
                st.session_state.current_word += item
            # reset to row selection
            st.session_state.selecting_row = True
            st.session_state.row = 0
            st.session_state.col = 0
    elif blink == 3:
        # move left / previous
        if st.session_state.selecting_row:
            st.session_state.row = (st.session_state.row - 1) % len(keyboard_rows)
        else:
            st.session_state.col = (st.session_state.col - 1) % len(keyboard_rows[st.session_state.row])
    # else: ignore unknown codes

# -----------------------------
# Keyboard rendering placeholder
# -----------------------------
placeholder = st.empty()
with placeholder.container():
    st.write("### Virtual Keyboard")
    for r, row_items in enumerate(keyboard_rows):
        cols = st.columns(len(row_items))
        for c, key in enumerate(row_items):
            # base style
            style = "background-color:#111; color:white; font-size:18px; border-radius:6px; padding:10px; text-align:center; margin:2px;"
            if st.session_state.selecting_row and r == st.session_state.row:
                style = style.replace("#111", "#222") + " border:2px solid #1E90FF;"
            elif (not st.session_state.selecting_row) and r == st.session_state.row and c == st.session_state.col:
                style = style.replace("#111", "#163F13") + " border:2px solid #32CD32;"
            cols[c].markdown(f'<div style="{style}">{key}</div>', unsafe_allow_html=True)

# -----------------------------
# Scanning / polling loop (non-blocking single-run per refresh)
# -----------------------------
if st.session_state.scanning and st.session_state.connected and st.session_state.serial:
    # read any available serial lines and process them
    serial_line = read_serial_line()
    if serial_line:
        # Debug: show last detected code in sidebar
        st.sidebar.info(f"Serial: {serial_line}")
        process_blink_code(serial_line)

    # update the displayed typed text by writing to session_state typed_box (Streamlit will show updated value on rerun)
    st.session_state.typed_box = st.session_state.current_word

    # small sleep to control poll frequency, then re-run the script to continue scanning
    time.sleep(poll_interval)
    st.rerun()

# If not scanning, just show current info
else:
    if not st.session_state.connected:
        st.info("Not connected. Use the sidebar to connect to the Arduino.")
    elif not st.session_state.scanning:
        st.info("Scanning is stopped. Click ▶ Start Scanning to begin reading blinks from Arduino.")



