#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <ApplicationServices/ApplicationServices.h>

// --- Send a single key press ---
void sendKey(CGKeyCode key) {
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, key, true);
    CGEventRef up   = CGEventCreateKeyboardEvent(NULL, key, false);
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(down);
    CFRelease(up);
}

// Map a character to a keycode (basic letters/numbers)
CGKeyCode charToKeyCode(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') {
        const CGKeyCode numbers[] = {29,18,19,20,21,23,22,26,28,25};
        return numbers[c - '0'];
    }
    if (c == ' ') return 49; // Space
    if (c == '\n') return 36; // Return
    return 0; // fallback
}

int main() {
    // --- Open Serial Port ---
    const char* portName = "/dev/cu.usbmodem1301"; // change if needed
    int fd = open(portName, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "Failed to open " << portName << "\n";
        return 1;
    }

    termios tty{};
    tcgetattr(fd, &tty);
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSANOW, &tty);

    std::vector<std::string> rows = {
        "1234567890",
        "QWERTYUIOP",
        "ASDFGHJKL",
        "ZXCVBNM",
        " <RET>"
    };
    size_t row = 0, col = 0;
    bool selectingRow = true;

    std::cout << "Blink keyboard ready. Single=advance, Double=confirm.\n";

    std::string buf;
    char ch;
    while (true) {
        if (read(fd, &ch, 1) > 0) {
            if (ch == '\n' || ch == '\r') {
                if (buf.empty()) continue;
                int blink = std::stoi(buf);
                buf.clear();

                if (blink == 1) { // single blink: advance
                    if (selectingRow) {
                        row = (row + 1) % rows.size();
                        std::cout << "Row: " << rows[row] << "\n";
                    } else {
                        col = (col + 1) % rows[row].size();
                        std::cout << "Char: " << rows[row][col] << "\n";
                    }
                } else if (blink == 2) { // double blink: confirm
                    if (selectingRow) {
                        selectingRow = false;
                        col = 0;
                        std::cout << "Selected row: " << rows[row] << "\n";
                    } else {
                        char out = rows[row][col];
                        if (out == '<') { // special case for return
                            sendKey(36);
                            std::cout << "[ENTER]\n";
                        } else {
                            sendKey(charToKeyCode(out));
                            std::cout << "Typed: " << out << "\n";
                        }
                        selectingRow = true;
                        row = 0; col = 0;
                        std::cout << "Back to row selection.\n";
                    }
                }
            } else if (isdigit(ch)) {
                buf += ch;
            }
        }
    }
    close(fd);
    return 0;
}
