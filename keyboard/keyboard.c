// Blink-Controlled Keyboard Interface (Windows, C)
// Uses Win32 serial and input APIs to implement an eye-blink controlled scanning keyboard.
// Baud rate: 115200, no parity, 8 data bits, 1 stop bit.
// Blink codes: 1=advance, 2=toggle direction, 3=confirm row, 4=confirm char.

#define _WIN32_WINNT 0x0500  // ensure SendInput is available
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// Helper: send one keystroke (virtual-key) via SendInput
void sendKey(WORD vkCode) {
    INPUT ip = {0};
    ip.type = INPUT_KEYBOARD;
    ip.ki.wVk = vkCode;
    // key down
    SendInput(1, &ip, sizeof(INPUT));
    // key up
    ip.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));
}

// Helper: send a character, handling SHIFT if needed
void sendChar(char ch) {
    SHORT vk = VkKeyScan(ch);
    BYTE vkCode = LOBYTE(vk);
    BYTE shiftState = HIBYTE(vk);
    
    // Press SHIFT, CTRL, ALT if required
    if (shiftState & 1) {  // SHIFT flag
        INPUT shiftInput = {0};
        shiftInput.type = INPUT_KEYBOARD;
        shiftInput.ki.wVk = VK_SHIFT;
        SendInput(1, &shiftInput, sizeof(INPUT));
    }
    if (shiftState & 2) {  // CTRL flag
        INPUT ctrlInput = {0};
        ctrlInput.type = INPUT_KEYBOARD;
        ctrlInput.ki.wVk = VK_CONTROL;
        SendInput(1, &ctrlInput, sizeof(INPUT));
    }
    if (shiftState & 4) {  // ALT flag
        INPUT altInput = {0};
        altInput.type = INPUT_KEYBOARD;
        altInput.ki.wVk = VK_MENU;
        SendInput(1, &altInput, sizeof(INPUT));
    }
    
    // Press the key
    INPUT keyInput = {0};
    keyInput.type = INPUT_KEYBOARD;
    keyInput.ki.wVk = vkCode;
    SendInput(1, &keyInput, sizeof(INPUT));
    
    // Release the key
    keyInput.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &keyInput, sizeof(INPUT));
    
    // Release modifiers in reverse order
    if (shiftState & 4) {
        INPUT altInput = {0};
        altInput.type = INPUT_KEYBOARD;
        altInput.ki.wVk = VK_MENU;
        altInput.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &altInput, sizeof(INPUT));
    }
    if (shiftState & 2) {
        INPUT ctrlInput = {0};
        ctrlInput.type = INPUT_KEYBOARD;
        ctrlInput.ki.wVk = VK_CONTROL;
        ctrlInput.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ctrlInput, sizeof(INPUT));
    }
    if (shiftState & 1) {
        INPUT shiftInput = {0};
        shiftInput.type = INPUT_KEYBOARD;
        shiftInput.ki.wVk = VK_SHIFT;
        shiftInput.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &shiftInput, sizeof(INPUT));
    }
}

int main() {
    // Detect Arduino COM port: scan COM1..COM20
    char portName[32];
    printf("Searching for Arduino COM port...\n");
    int foundPorts = 0, portNumber = 0;
    
    for (int i = 1; i <= 20; i++) {
        sprintf(portName, "\\\\.\\COM%d", i);
        HANDLE hTest = CreateFile(portName, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hTest != INVALID_HANDLE_VALUE) {
            printf("Found port COM%d\n", i);
            CloseHandle(hTest);
            foundPorts++;
            if (foundPorts == 1) {
                portNumber = i;
            }
        }
    }
    
    if (foundPorts == 0) {
        fprintf(stderr, "No COM ports found. Ensure Arduino is connected.\n");
        return 1;
    }
    
    if (foundPorts > 1) {
        printf("Multiple ports found. Select port number to use (1..20): ");
        int sel = 0;
        if (scanf("%d", &sel) != 1 || sel < 1 || sel > 20) {
            fprintf(stderr, "Invalid selection.\n");
            return 1;
        }
        portNumber = sel;
    }
    
    sprintf(portName, "\\\\.\\COM%d", portNumber);

    // Open the selected COM port
    HANDLE hSerial = CreateFile(portName, GENERIC_READ|GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open %s (Error %lu).\n", portName, GetLastError());
        return 1;
    }
    
    // Configure serial port: 115200-8N1
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(hSerial, &dcb)) {
        fprintf(stderr, "GetCommState failed (Error %lu)\n", GetLastError());
        CloseHandle(hSerial);
        return 1;
    }
    
    dcb.BaudRate = 115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    
    if (!SetCommState(hSerial, &dcb)) {
        fprintf(stderr, "SetCommState failed (Error %lu)\n", GetLastError());
        CloseHandle(hSerial);
        return 1;
    }
    
    // Set read timeouts
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        fprintf(stderr, "SetCommTimeouts failed (Error %lu)\n", GetLastError());
        CloseHandle(hSerial);
        return 1;
    }

    printf("Connected to %s at 115200 baud.\n", portName);

    // Define keyboard layout rows
    const char *rows[] = {
        "1234567890",    // number row
        "QWERTYUIOP",    // top letter row
        "ASDFGHJKL",     // middle row
        "ZXCVBNM"        // bottom row
    };
    const int letterRows = 4;
    const int totalRows = 5;
    
    // Special keys row
    const char *specials[] = {
        "Space", "Backspace", "Enter",
        ".", ",", "!", "@", "#", "Shift", "CapsLock"
    };
    const int numSpecials = 10;

    bool scanningRows = true;
    int rowIndex = 0, colIndex = 0;
    int direction = 1;
    bool shiftActive = false;
    bool capsLock = false;

    // Buffer for incoming serial data
    char buf[16];
    int bufPos = 0;
    
    printf("Blink-controlled keyboard ready. Use your Arduino to send blink codes.\n");
    printf("Codes: 1=advance, 2=reverse, 3=select row, 4=select character\n");
    printf("Current mode: Row scanning\n");
    
    while (1) {
        char byte;
        DWORD bytesRead;
        
        if (!ReadFile(hSerial, &byte, 1, &bytesRead, NULL) || bytesRead == 0) {
            DWORD error = GetLastError();
            if (error != ERROR_SUCCESS && error != ERROR_IO_PENDING && error != ERROR_TIMEOUT) {
                fprintf(stderr, "Serial read error %lu. Exiting.\n", error);
                break;
            }
            continue;
        }
        
        if (byte == '\r') continue;
        
        if (byte == '\n') {
            if (bufPos == 0) continue; // ignore empty lines
            buf[bufPos] = '\0';
            int blink = atoi(buf);
            bufPos = 0;
            
            printf("Received blink code: %d\n", blink);
            
            if (blink == 1) {
                // Advance highlight
                if (scanningRows) {
                    rowIndex = (rowIndex + direction + totalRows) % totalRows;
                    printf("Highlighting row %d\n", rowIndex);
                } else {
                    if (rowIndex < letterRows) {
                        int len = (int)strlen(rows[rowIndex]);
                        colIndex = (colIndex + direction + len) % len;
                        printf("Highlighting character '%c'\n", rows[rowIndex][colIndex]);
                    } else {
                        colIndex = (colIndex + direction + numSpecials) % numSpecials;
                        printf("Highlighting special key '%s'\n", specials[colIndex]);
                    }
                }
            } else if (blink == 2) {
                direction = -direction;
                printf("Direction toggled to %s\n", (direction == 1) ? "forward" : "backward");
            } else if (blink == 3) {
                if (scanningRows) {
                    scanningRows = false;
                    colIndex = 0;
                    if (rowIndex < letterRows) {
                        printf("Selected row %d (%s). Now scanning characters.\n", rowIndex, rows[rowIndex]);
                    } else {
                        printf("Selected special keys row. Now scanning special functions.\n");
                    }
                }
            } else if (blink == 4) {
                if (!scanningRows) {
                    if (rowIndex < letterRows) {
                        if (rowIndex == 0) {
                            // Number row
                            char digit = rows[rowIndex][colIndex];
                            char outChar;
                            if (shiftActive) {
                                const char *symbols = "!@#$%^&*()";
                                outChar = (digit == '0') ? ')' : symbols[digit - '1'];
                            } else {
                                outChar = digit;
                            }
                            sendChar(outChar);
                            printf("Typed: '%c'\n", outChar);
                            shiftActive = false;
                        } else {
                            // Letter row
                            char letter = rows[rowIndex][colIndex];
                            bool upper = capsLock ^ shiftActive;
                            char outChar = upper ? letter : (char)tolower(letter);
                            sendChar(outChar);
                            printf("Typed: '%c'\n", outChar);
                            shiftActive = false;
                        }
                    } else {
                        // Special row
                        const char *item = specials[colIndex];
                        printf("Executing special action: %s\n", item);
                        
                        if (strcmp(item, "Space") == 0) {
                            sendKey(VK_SPACE);
                            printf("Typed: [SPACE]\n");
                        } else if (strcmp(item, "Backspace") == 0) {
                            sendKey(VK_BACK);
                            printf("Typed: [BACKSPACE]\n");
                        } else if (strcmp(item, "Enter") == 0) {
                            sendKey(VK_RETURN);
                            printf("Typed: [ENTER]\n");
                        } else if (strcmp(item, ".") == 0) {
                            sendChar('.');
                            printf("Typed: '.'\n");
                        } else if (strcmp(item, ",") == 0) {
                            sendChar(',');
                            printf("Typed: ','\n");
                        } else if (strcmp(item, "!") == 0) {
                            sendChar('!');
                            printf("Typed: '!'\n");
                        } else if (strcmp(item, "@") == 0) {
                            sendChar('@');
                            printf("Typed: '@'\n");
                        } else if (strcmp(item, "#") == 0) {
                            sendChar('#');
                            printf("Typed: '#'\n");
                        } else if (strcmp(item, "Shift") == 0) {
                            shiftActive = !shiftActive;
                            printf("Shift mode: %s\n", shiftActive ? "ON" : "OFF");
                        } else if (strcmp(item, "CapsLock") == 0) {
                            sendKey(VK_CAPITAL);
                            capsLock = !capsLock;
                            printf("CapsLock: %s\n", capsLock ? "ON" : "OFF");
                        }
                    }
                }
                // Return to row scanning
                scanningRows = true;
                rowIndex = 0;
                colIndex = 0;
                direction = 1;
                printf("Returning to row scanning mode\n");
            } else {
                printf("Unknown blink code: %d (ignored)\n", blink);
            }
        } else if (isdigit(byte)) {  // Only accept digit characters
            if (bufPos < 15) {
                buf[bufPos++] = byte;
            }
        }
        // Ignore non-digit characters except \r and \n
    }

    printf("Closing serial connection...\n");
    CloseHandle(hSerial);
    return 0;
}
