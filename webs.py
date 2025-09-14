import streamlit as st
import serial
import time as pytime
import random

# -----------------------------
# Page Config
# -----------------------------
st.set_page_config(page_title="BlinkShift - EOG Keyboard", layout="wide")
st.sidebar.header("Connection / Settings")

# --- Robust Session State Initialization ---
defaults = {
    "connected": False,
    "serial": None,
    "current_word": "",
    "row": 0,
    "col": 0,
    "selecting_row": True,
    "scanning": False,
    "typed_box": "",
    "typing_mode": False,
    "typing_text": "",
    "typed_input": "",
    "start_time": None,
    "end_time": None,
}
for k, v in defaults.items():
    st.session_state.setdefault(k, v)

# -----------------------------
# Sidebar controls
# -----------------------------
port = st.sidebar.text_input("Serial port (e.g. COM3 or /dev/ttyUSB0)", value="COM3")

# Baud dropdown
baud_rates = [300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600]
baud = st.sidebar.selectbox("Baud rate", baud_rates, index=baud_rates.index(115200))

poll_interval = st.sidebar.slider(
    "Poll interval (s)", min_value=0.05, max_value=1.0, value=0.2, step=0.05
)

# Mode toggle
mode = st.sidebar.radio("Mode", ["Blink Keyboard", "Typing Test"])
st.session_state.typing_mode = (mode == "Typing Test")

# Connect / Disconnect buttons
if st.sidebar.button("Connect"):
    try:
        if st.session_state.serial and st.session_state.serial.is_open:
            st.session_state.serial.close()
        st.session_state.serial = serial.Serial(port, baud, timeout=0.1)
        pytime.sleep(0.5)
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
# Typing Test Mode
# -----------------------------
if st.session_state.typing_mode:
    st.title("Monkeytype-Style Typing Test")
    if not st.session_state.typing_text or st.button("🔄 New Text"):
        words = [
            "stream", "blink", "keyboard", "python", "practice", "speed",
            "serial", "arduino", "focus", "accuracy", "typing", "test", "session", "state"
        ]
        st.session_state.typing_text = " ".join(random.sample(words, 8))
        st.session_state.typed_input = ""
        st.session_state.start_time = None
        st.session_state.end_time = None

    st.markdown(f"### Target:\n```{st.session_state.typing_text}```")

    typed = st.text_area("Type here:", value=st.session_state.typed_input, height=120, key="typed_input")
    if typed and st.session_state.start_time is None:
        st.session_state.start_time = pytime.time()

    if st.button("✅ Finish"):
        st.session_state.end_time = pytime.time()
        duration = (st.session_state.end_time - st.session_state.start_time) if st.session_state.start_time else 1
        words_typed = len(st.session_state.typing_text.split())
        wpm = (words_typed / duration) * 60
        target_words = st.session_state.typing_text.split()
        typed_words = typed.split()
        correct = sum(t == y for t, y in zip(typed_words, target_words))
        accuracy = correct / max(len(target_words), 1) * 100
        st.success(f"⏱ **Time:** {duration:.1f}s | 📈 **WPM:** {wpm:.1f} | 🎯 **Accuracy:** {accuracy:.1f}%")

    st.stop()  # End here if typing mode is active

# -----------------------------
# Blink Keyboard Mode
# -----------------------------
keyboard_rows = [
    ["1","2","3","4","5","6","7","8","9","0"],
    ["Q","W","E","R","T","Y","U","I","O","P"],
    ["A","S","D","F","G","H","J","K","L"],
    ["Z","X","C","V","B","N","M"],
    ["SPACE","DEL","ENTER"]
]

st.title("BlinkShift — EOG / EMG Keyboard")
st.write("Use your Arduino EOG blink detector to navigate and select keys. Connect, Start Scanning, then blink.")

st.markdown("**Typed text**")
st.text_area("Typed Output", value=st.session_state.current_word, height=140, key="current_word")

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

def read_serial_line():
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
    try:
        blink = int(code)
    except:
        return
    if blink == 1:
        if st.session_state.selecting_row:
            st.session_state.row = (st.session_state.row + 1) % len(keyboard_rows)
        else:
            st.session_state.col = (st.session_state.col + 1) % len(keyboard_rows[st.session_state.row])
    elif blink == 2:
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
            st.session_state.selecting_row = True
            st.session_state.row = 0
            st.session_state.col = 0
    elif blink == 3:
        if st.session_state.selecting_row:
            st.session_state.row = (st.session_state.row - 1) % len(keyboard_rows)
        else:
            st.session_state.col = (st.session_state.col - 1) % len(keyboard_rows[st.session_state.row])

placeholder = st.empty()
with placeholder.container():
    st.write("### Virtual Keyboard")
    for r, row_items in enumerate(keyboard_rows):
        cols = st.columns(len(row_items))
        for c, key in enumerate(row_items):
            style = "background-color:#111; color:white; font-size:18px; border-radius:6px; padding:10px; text-align:center; margin:2px;"
            if st.session_state.selecting_row and r == st.session_state.row:
                style = style.replace("#111", "#222") + " border:2px solid #1E90FF;"
            elif (not st.session_state.selecting_row) and r == st.session_state.row and c == st.session_state.col:
                style = style.replace("#111", "#163F13") + " border:2px solid #32CD32;"
            cols[c].markdown(f'<div style="{style}">{key}</div>', unsafe_allow_html=True)

if st.session_state.scanning and st.session_state.connected and st.session_state.serial:
    serial_line = read_serial_line()
    if serial_line:
        st.sidebar.info(f"Serial: {serial_line}")
        process_blink_code(serial_line)
    st.session_state.current_word = st.session_state.current_word
    pytime.sleep(poll_interval)
    st.rerun()
else:
    if not st.session_state.connected:
        st.info("Not connected. Use the sidebar to connect to the Arduino.")
    elif not st.session_state.scanning:
        st.info("Scanning is stopped. Click ▶ Start Scanning to begin reading blinks from Arduino.")
