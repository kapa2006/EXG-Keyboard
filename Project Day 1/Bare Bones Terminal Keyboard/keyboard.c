#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// Global target window handle
HWND targetHwnd;

// Send a Unicode character to the target window (using SendInput)
void sendChar(WCHAR ch) {
    // Attach to the foreground thread to allow SetForegroundWindow
    DWORD curThread = GetCurrentThreadId();
    DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    AttachThreadInput(curThread, fgThread, TRUE);
    SetForegroundWindow(targetHwnd);
    AttachThreadInput(curThread, fgThread, FALSE);

    // Prepare INPUT for Unicode character
    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = 0;
    inputs[0].ki.wScan = ch;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;        // Key down (unicode)
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0;
    inputs[1].ki.wScan = ch;
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP; // Key up
    SendInput(2, inputs, sizeof(INPUT));
}

// Send a virtual-key (special key) to the target window
void sendKey(WORD vk) {
    DWORD curThread = GetCurrentThreadId();
    DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    AttachThreadInput(curThread, fgThread, TRUE);
    SetForegroundWindow(targetHwnd);
    AttachThreadInput(curThread, fgThread, FALSE);

    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[0].ki.dwFlags = 0;             // Key down
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP; // Key up
    SendInput(2, inputs, sizeof(INPUT));
}

int main() {
    // Initialize serial port (COM)
    char portName[20];
    printf("Enter COM port (e.g., COM3): ");
    if (scanf("%s", portName) != 1) {
        fprintf(stderr, "Failed to read COM port\n");
        return 1;
    }
    // Clear any leftover newline from input buffer
    int c; while ((c = getchar()) != '\n' && c != EOF);

    // Build full port name, e.g. "\\\\.\\COM3"
    char fullPort[20];
    if (strlen(portName) > 0 && portName[0] != '\\') {
        sprintf(fullPort, "\\\\.\\%s", portName);
    } else {
        strncpy(fullPort, portName, sizeof(fullPort));
    }

    // Open serial port
    HANDLE hSerial = CreateFile(fullPort, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: could not open serial port %s\n", fullPort);
        return 1;
    }
    // Configure serial port: 115200 baud, 8 data bits, no parity, 1 stop bit
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error: GetCommState failed\n");
        CloseHandle(hSerial);
        return 1;
    }
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.Parity   = NOPARITY;
    dcbSerialParams.StopBits = ONESTOPBIT;
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error: SetCommState failed\n");
        CloseHandle(hSerial);
        return 1;
    }
    // Set timeouts (short timeouts so ReadFile is not blocking indefinitely)
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    SetCommTimeouts(hSerial, &timeouts);

    // Prompt user to focus target window and get its HWND
    printf("Focus the target application (e.g. Notepad) and press Enter...\n");
    while ((c = getchar()) != '\n' && c != EOF);
    targetHwnd = GetForegroundWindow();
    if (!targetHwnd) {
        fprintf(stderr, "Error: could not get target window\n");
        CloseHandle(hSerial);
        return 1;
    }
    printf("Target window handle: %p\n", (void*)targetHwnd);

    // Define scanning layout: rows of characters (including special keys in the last row)
    const char *rows[] = {
        "ABCDEFGHI",
        "JKLMNOPQR",
        "STUVWXYZ",
        "0123456789",
        ".,?!;:",
        " \b\r"  // space, backspace, enter
    };
    int numRows = sizeof(rows) / sizeof(rows[0]);
    int rowIdx = 0, colIdx = 0;
    bool charMode = false;

    printf("Begin scanning. Blink codes: 1=advance, 2=reverse, 3=select row, 4=select char.\n");

    // Main loop: read blink codes and update selection
    while (true) {
        // Read one byte from serial (blink code)
        char recv;
        DWORD bytesRead = 0;
        if (!ReadFile(hSerial, &recv, 1, &bytesRead, NULL)) {
            fprintf(stderr, "Error: serial read failed\n");
            break;
        }
        if (bytesRead == 0) {
            Sleep(10);
            continue;
        }
        if (recv < '1' || recv > '4') {
            // ignore invalid codes
            continue;
        }
        int code = recv - '0';

        if (!charMode) {
            // Row-scanning mode
            if (code == 1) {
                // advance to next row
                rowIdx = (rowIdx + 1) % numRows;
                colIdx = 0;
            } else if (code == 2) {
                // go back one row
                rowIdx = (rowIdx - 1 + numRows) % numRows;
                colIdx = 0;
            } else if (code == 3) {
                // select current row -> enter character-scanning mode
                charMode = true;
                colIdx = 0;
            }
            // code 4 has no effect in row mode
        } else {
            // Character-scanning mode within the selected row
            int rowLen = strlen(rows[rowIdx]);
            if (code == 1) {
                // advance to next character
                colIdx = (colIdx + 1) % rowLen;
            } else if (code == 2) {
                // previous character
                colIdx = (colIdx - 1 + rowLen) % rowLen;
            } else if (code == 3) {
                // cancel and go back to row mode
                charMode = false;
            } else if (code == 4) {
                // select this character: send it
                char ch = rows[rowIdx][colIdx];
                if (ch == '\b') {
                    // Backspace
                    sendKey(VK_BACK);
                    printf("Sent <Backspace>\n");
                } else if (ch == '\r') {
                    // Enter
                    sendKey(VK_RETURN);
                    printf("Sent <Enter>\n");
                } else {
                    // Regular character (including space, punctuation)
                    WCHAR wch = (WCHAR)ch;
                    sendChar(wch);
                    if (ch == ' ') {
                        printf("Sent <Space>\n");
                    } else {
                        printf("Sent '%c'\n", ch);
                    }
                }
                fflush(stdout);
                // return to row-scanning after sending
                charMode = false;
            }
        }

        // Console feedback on current selection
        if (!charMode) {
            // Display current row index and its content
            printf("Row %d: %s\n", rowIdx + 1, rows[rowIdx]);
        } else {
            // Display current character within the row
            char sel = rows[rowIdx][colIdx];
            if (sel == ' ') {
                printf("Row %d, char: <Space>\n", rowIdx + 1);
            } else if (sel == '\b') {
                printf("Row %d, char: <Backspace>\n", rowIdx + 1);
            } else if (sel == '\r') {
                printf("Row %d, char: <Enter>\n", rowIdx + 1);
            } else {
                printf("Row %d, char: '%c'\n", rowIdx + 1, sel);
            }
        }
        fflush(stdout);
    }

    CloseHandle(hSerial);
    return 0;
}
