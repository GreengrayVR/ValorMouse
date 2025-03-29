#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <fstream>
#include <string>
#include <atomic>
#include <thread>
#include <cmath>
#include <map>
#include <winreg.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// Configuration
constexpr float MOUSE_BASE_SPEED = 2.0f;
constexpr float MOUSE_ACCELERATION = 0.2f;
constexpr float MAX_SPEED = 15.0f;
constexpr int UPDATE_INTERVAL_MS = 10;
constexpr wchar_t APP_NAME[] = L"ValorMouse";
constexpr wchar_t STARTUP_REG_PATH[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t CONFIG_REG_PATH[] = L"Software\\ValorMouse";

// Key Mappings
struct KeyBindings {
    int modifier = VK_RCONTROL;
    int moveUp = 'W';
    int moveDown = 'S';
    int moveLeft = 'A';
    int moveRight = 'D';
    int leftClick = VK_OEM_PERIOD;
    int rightClick = VK_OEM_2;
    int speedBoost = VK_LSHIFT;
    int scrollUp = 'Q';
    int scrollDown = 'E';
    int backButton = 'Z';
    int forwardButton = 'X';
} g_keyBindings;

// All available keys for binding
const std::map<int, std::wstring> g_allKeys = {
    // Modifiers
    {VK_LSHIFT, L"Left Shift"}, {VK_RSHIFT, L"Right Shift"},
    {VK_LCONTROL, L"Left Ctrl"}, {VK_RCONTROL, L"Right Ctrl"},
    {VK_LMENU, L"Left Alt"}, {VK_RMENU, L"Right Alt"},

    // Main alphabet
    {'Q', L"Q"}, {'W', L"W"}, {'E', L"E"}, {'R', L"R"}, {'T', L"T"}, {'Y', L"Y"}, {'U', L"U"}, {'I', L"I"}, {'O', L"O"}, {'P', L"P"},
    {'A', L"A"}, {'S', L"S"}, {'D', L"D"}, {'F', L"F"}, {'G', L"G"}, {'H', L"H"}, {'J', L"J"}, {'K', L"K"}, {'L', L"L"},
    {'Z', L"Z"}, {'X', L"X"}, {'C', L"C"}, {'V', L"V"}, {'B', L"B"}, {'N', L"N"}, {'M', L"M"},

    // Numbers
    {'0', L"0"}, {'1', L"1"}, {'2', L"2"}, {'3', L"3"}, {'4', L"4"},
    {'5', L"5"}, {'6', L"6"}, {'7', L"7"}, {'8', L"8"}, {'9', L"9"},

    // Symbols
    {VK_OEM_1, L";"}, {VK_OEM_2, L"/"}, {VK_OEM_3, L"`"},
    {VK_OEM_4, L"["}, {VK_OEM_5, L"\\"}, {VK_OEM_6, L"]"}, {VK_OEM_7, L"'"},
    {VK_OEM_COMMA, L","}, {VK_OEM_PERIOD, L"."}, {VK_OEM_MINUS, L"-"}, {VK_OEM_PLUS, L"="},

    // Function keys
    {VK_F1, L"F1"}, {VK_F2, L"F2"}, {VK_F3, L"F3"}, {VK_F4, L"F4"},
    {VK_F5, L"F5"}, {VK_F6, L"F6"}, {VK_F7, L"F7"}, {VK_F8, L"F8"},
    {VK_F9, L"F9"}, {VK_F10, L"F10"}, {VK_F11, L"F11"}, {VK_F12, L"F12"},

    // Special keys
    {VK_ESCAPE, L"Esc"}, {VK_TAB, L"Tab"}, {VK_CAPITAL, L"Caps Lock"}, {VK_SPACE, L"Space"},
    {VK_RETURN, L"Enter"}, {VK_BACK, L"Backspace"}, {VK_INSERT, L"Insert"}, {VK_DELETE, L"Delete"},
    {VK_HOME, L"Home"}, {VK_END, L"End"}, {VK_PRIOR, L"Page Up"}, {VK_NEXT, L"Page Down"},
    {VK_UP, L"Up Arrow"}, {VK_DOWN, L"Down Arrow"}, {VK_LEFT, L"Left Arrow"}, {VK_RIGHT, L"Right Arrow"},

    // Mouse buttons
    {VK_XBUTTON1, L"Mouse Back"}, {VK_XBUTTON2, L"Mouse Forward"}
};

// Global state
std::atomic<bool> g_modifierActive{ false };
std::atomic<bool> g_exitProgram{ false };
std::atomic<bool> g_movementKeys[4] = { false };
std::atomic<bool> g_leftClickActive{ false };
std::atomic<bool> g_rightClickActive{ false };
std::atomic<bool> g_leftClickDown{ false };
std::atomic<bool> g_rightClickDown{ false };
std::atomic<bool> g_speedBoostActive{ false };
std::atomic<bool> g_scrollUpActive{ false };
std::atomic<bool> g_scrollDownActive{ false };
std::atomic<bool> g_backButtonActive{ false };
std::atomic<bool> g_forwardButtonActive{ false };
std::atomic<bool> g_backButtonDown{ false };
std::atomic<bool> g_forwardButtonDown{ false };
std::atomic<float> g_currentSpeed{ 0.0f };
HHOOK g_keyboardHook = nullptr;

// GUI handles
HWND g_hwnd = nullptr;
HMENU g_hMenu = nullptr;
HMENU g_hSubMenu = nullptr;
NOTIFYICONDATA g_notifyIconData = {};

// Forward declarations
void LoadConfig();
void SaveConfig();
void CreateTrayIcon();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);
void ShowSettingsDialog();
void MouseMovementThread();
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
void SetStartup(bool enable);
bool IsStartupEnabled();
void UpdateStartupMenuItem();

void SetStartup(bool enable) {
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, STARTUP_REG_PATH, 0, KEY_WRITE, &hKey);

    if (result == ERROR_SUCCESS) {
        if (enable) {
            wchar_t path[MAX_PATH];
            GetModuleFileName(NULL, path, MAX_PATH);
            RegSetValueEx(hKey, APP_NAME, 0, REG_SZ, (BYTE*)path, (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
        }
        else {
            RegDeleteValue(hKey, APP_NAME);
        }
        RegCloseKey(hKey);
    }
    UpdateStartupMenuItem();
}

bool IsStartupEnabled() {
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, STARTUP_REG_PATH, 0, KEY_READ, &hKey);

    if (result == ERROR_SUCCESS) {
        DWORD type, size;
        result = RegQueryValueEx(hKey, APP_NAME, NULL, &type, NULL, &size);
        RegCloseKey(hKey);
        return (result == ERROR_SUCCESS);
    }
    return false;
}

void UpdateStartupMenuItem() {
    if (g_hSubMenu) {
        MENUITEMINFO mii = { 0 };
        mii.cbSize = sizeof(MENUITEMINFO);
        mii.fMask = MIIM_STRING;

        wchar_t buffer[64];
        if (IsStartupEnabled()) {
            wcscpy_s(buffer, L"Disable Auto-Startup");
        }
        else {
            wcscpy_s(buffer, L"Enable Auto-Startup");
        }

        mii.dwTypeData = buffer;
        SetMenuItemInfo(g_hSubMenu, 2, FALSE, &mii);
    }
}

void SendMouseEvent(DWORD flags, DWORD mouseData = 0) {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    input.mi.mouseData = mouseData;
    SendInput(1, &input, sizeof(INPUT));
}

void LoadConfig() {
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, CONFIG_REG_PATH, 0, KEY_READ, &hKey);

    if (result != ERROR_SUCCESS) {
        // First run - create key with default values
        SaveConfig();
        MessageBox(0, L"Right click on tray icon for settings", APP_NAME, 0);
        return;
    }

    DWORD value;
    DWORD size = sizeof(DWORD);

    if (RegQueryValueEx(hKey, L"Modifier", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.modifier = value;
    }

    if (RegQueryValueEx(hKey, L"MoveUp", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.moveUp = value;
    }

    if (RegQueryValueEx(hKey, L"MoveDown", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.moveDown = value;
    }

    if (RegQueryValueEx(hKey, L"MoveLeft", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.moveLeft = value;
    }

    if (RegQueryValueEx(hKey, L"MoveRight", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.moveRight = value;
    }

    if (RegQueryValueEx(hKey, L"LeftClick", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.leftClick = value;
    }

    if (RegQueryValueEx(hKey, L"RightClick", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.rightClick = value;
    }

    if (RegQueryValueEx(hKey, L"SpeedBoost", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.speedBoost = value;
    }

    if (RegQueryValueEx(hKey, L"ScrollUp", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.scrollUp = value;
    }

    if (RegQueryValueEx(hKey, L"ScrollDown", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.scrollDown = value;
    }

    if (RegQueryValueEx(hKey, L"BackButton", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.backButton = value;
    }

    if (RegQueryValueEx(hKey, L"ForwardButton", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
        g_keyBindings.forwardButton = value;
    }

    RegCloseKey(hKey);
}

void SaveConfig() {
    HKEY hKey;
    LONG result = RegCreateKeyEx(HKEY_CURRENT_USER, CONFIG_REG_PATH, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);

    if (result == ERROR_SUCCESS) {
        RegSetValueEx(hKey, L"Modifier", 0, REG_DWORD, (const BYTE*)&g_keyBindings.modifier, sizeof(DWORD));
        RegSetValueEx(hKey, L"MoveUp", 0, REG_DWORD, (const BYTE*)&g_keyBindings.moveUp, sizeof(DWORD));
        RegSetValueEx(hKey, L"MoveDown", 0, REG_DWORD, (const BYTE*)&g_keyBindings.moveDown, sizeof(DWORD));
        RegSetValueEx(hKey, L"MoveLeft", 0, REG_DWORD, (const BYTE*)&g_keyBindings.moveLeft, sizeof(DWORD));
        RegSetValueEx(hKey, L"MoveRight", 0, REG_DWORD, (const BYTE*)&g_keyBindings.moveRight, sizeof(DWORD));
        RegSetValueEx(hKey, L"LeftClick", 0, REG_DWORD, (const BYTE*)&g_keyBindings.leftClick, sizeof(DWORD));
        RegSetValueEx(hKey, L"RightClick", 0, REG_DWORD, (const BYTE*)&g_keyBindings.rightClick, sizeof(DWORD));
        RegSetValueEx(hKey, L"SpeedBoost", 0, REG_DWORD, (const BYTE*)&g_keyBindings.speedBoost, sizeof(DWORD));
        RegSetValueEx(hKey, L"ScrollUp", 0, REG_DWORD, (const BYTE*)&g_keyBindings.scrollUp, sizeof(DWORD));
        RegSetValueEx(hKey, L"ScrollDown", 0, REG_DWORD, (const BYTE*)&g_keyBindings.scrollDown, sizeof(DWORD));
        RegSetValueEx(hKey, L"BackButton", 0, REG_DWORD, (const BYTE*)&g_keyBindings.backButton, sizeof(DWORD));
        RegSetValueEx(hKey, L"ForwardButton", 0, REG_DWORD, (const BYTE*)&g_keyBindings.forwardButton, sizeof(DWORD));

        RegCloseKey(hKey);
    }
}

void CreateTrayIcon() {
    g_notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    g_notifyIconData.hWnd = g_hwnd;
    g_notifyIconData.uID = 1;
    g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_notifyIconData.uCallbackMessage = WM_APP + 1;
    g_notifyIconData.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1337));
    wcscpy_s(g_notifyIconData.szTip, L"ValorMouse - Keyboard Mouse Emulator");
    Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        bool keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

        // Check modifier key
        if (kb->vkCode == g_keyBindings.modifier) {
            g_modifierActive.store(keyDown);
            return 1; // Block modifier key from other apps
        }

        // Only process if modifier is held
        if (g_modifierActive.load()) {
            if (kb->vkCode == g_keyBindings.moveUp) {
                g_movementKeys[0].store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.moveDown) {
                g_movementKeys[1].store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.moveLeft) {
                g_movementKeys[2].store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.moveRight) {
                g_movementKeys[3].store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.leftClick) {
                g_leftClickActive.store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.rightClick) {
                g_rightClickActive.store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.speedBoost) {
                g_speedBoostActive.store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.scrollUp) {
                g_scrollUpActive.store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.scrollDown) {
                g_scrollDownActive.store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.backButton) {
                g_backButtonActive.store(keyDown);
                return 1;
            }
            else if (kb->vkCode == g_keyBindings.forwardButton) {
                g_forwardButtonActive.store(keyDown);
                return 1;
            }
        }
    }
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hModifier, hUp, hDown, hLeft, hRight, hLClick, hRClick, hSpeedBoost, hScrollUp, hScrollDown, hBackButton, hForwardButton;

    if (msg == WM_INITDIALOG) {
        hModifier = GetDlgItem(hDlg, 1001);
        hUp = GetDlgItem(hDlg, 1002);
        hDown = GetDlgItem(hDlg, 1003);
        hLeft = GetDlgItem(hDlg, 1004);
        hRight = GetDlgItem(hDlg, 1005);
        hLClick = GetDlgItem(hDlg, 1006);
        hRClick = GetDlgItem(hDlg, 1007);
        hSpeedBoost = GetDlgItem(hDlg, 1008);
        hScrollUp = GetDlgItem(hDlg, 1009);
        hScrollDown = GetDlgItem(hDlg, 1010);
        hBackButton = GetDlgItem(hDlg, 1011);
        hForwardButton = GetDlgItem(hDlg, 1012);

        // Add all available keys to each dropdown
        for (const auto& key : g_allKeys) {
            SendMessage(hModifier, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hUp, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hDown, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hLeft, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hRight, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hLClick, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hRClick, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hSpeedBoost, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hScrollUp, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hScrollDown, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hBackButton, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
            SendMessage(hForwardButton, CB_ADDSTRING, 0, (LPARAM)key.second.c_str());
        }

        // Set current selections
        auto setComboSelection = [](HWND hCombo, int vkCode) {
            for (const auto& key : g_allKeys) {
                if (key.first == vkCode) {
                    int index = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)key.second.c_str());
                    if (index != CB_ERR) {
                        SendMessage(hCombo, CB_SETCURSEL, index, 0);
                    }
                    break;
                }
            }
            };

        setComboSelection(hModifier, g_keyBindings.modifier);
        setComboSelection(hUp, g_keyBindings.moveUp);
        setComboSelection(hDown, g_keyBindings.moveDown);
        setComboSelection(hLeft, g_keyBindings.moveLeft);
        setComboSelection(hRight, g_keyBindings.moveRight);
        setComboSelection(hLClick, g_keyBindings.leftClick);
        setComboSelection(hRClick, g_keyBindings.rightClick);
        setComboSelection(hSpeedBoost, g_keyBindings.speedBoost);
        setComboSelection(hScrollUp, g_keyBindings.scrollUp);
        setComboSelection(hScrollDown, g_keyBindings.scrollDown);
        setComboSelection(hBackButton, g_keyBindings.backButton);
        setComboSelection(hForwardButton, g_keyBindings.forwardButton);

        return TRUE;
    }
    else if (msg == WM_COMMAND && LOWORD(wParam) == IDOK) {
        auto getSelectedKey = [](HWND hCombo) {
            int index = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (index != CB_ERR) {
                wchar_t buffer[256];
                SendMessage(hCombo, CB_GETLBTEXT, index, (LPARAM)buffer);

                for (const auto& key : g_allKeys) {
                    if (key.second == buffer) {
                        return key.first;
                    }
                }
            }
            return 0;
            };

        g_keyBindings.modifier = getSelectedKey(hModifier);
        g_keyBindings.moveUp = getSelectedKey(hUp);
        g_keyBindings.moveDown = getSelectedKey(hDown);
        g_keyBindings.moveLeft = getSelectedKey(hLeft);
        g_keyBindings.moveRight = getSelectedKey(hRight);
        g_keyBindings.leftClick = getSelectedKey(hLClick);
        g_keyBindings.rightClick = getSelectedKey(hRClick);
        g_keyBindings.speedBoost = getSelectedKey(hSpeedBoost);
        g_keyBindings.scrollUp = getSelectedKey(hScrollUp);
        g_keyBindings.scrollDown = getSelectedKey(hScrollDown);
        g_keyBindings.backButton = getSelectedKey(hBackButton);
        g_keyBindings.forwardButton = getSelectedKey(hForwardButton);

        SaveConfig();
        EndDialog(hDlg, IDOK);
        return TRUE;
    }
    else if (msg == WM_COMMAND && LOWORD(wParam) == IDCANCEL) {
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void ShowSettingsDialog() {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(1), g_hwnd, SettingsDlgProc);
}

void MouseMovementThread() {
    while (!g_exitProgram.load()) {
        if (g_modifierActive.load()) {
            // Handle mouse movement
            POINT cursorPos;
            GetCursorPos(&cursorPos);

            int dx = 0, dy = 0;
            if (g_movementKeys[0].load()) dy -= 1;
            if (g_movementKeys[1].load()) dy += 1;
            if (g_movementKeys[2].load()) dx -= 1;
            if (g_movementKeys[3].load()) dx += 1;

            bool anyMovementKeyPressed = g_movementKeys[0].load() || g_movementKeys[1].load() ||
                g_movementKeys[2].load() || g_movementKeys[3].load();

            // Instant speed boost when holding speed boost key
            float targetSpeed = g_speedBoostActive.load() ? MAX_SPEED * 5.0f : MAX_SPEED;

            if (anyMovementKeyPressed) {
                if (g_speedBoostActive.load()) {
                    // Instantly reach max speed when boost is active
                    g_currentSpeed.store(targetSpeed);
                }
                else {
                    // Normal acceleration when boost is not active
                    g_currentSpeed.store(std::fmin(g_currentSpeed.load() + MOUSE_ACCELERATION, targetSpeed));
                }
            }
            else {
                g_currentSpeed.store(0.0f);
            }

            if (dx != 0 && dy != 0) {
                constexpr float diagFactor = 0.7071f;
                dx = static_cast<int>(dx * diagFactor * g_currentSpeed.load());
                dy = static_cast<int>(dy * diagFactor * g_currentSpeed.load());
            }
            else {
                dx = static_cast<int>(dx * g_currentSpeed.load());
                dy = static_cast<int>(dy * g_currentSpeed.load());
            }

            if (dx != 0 || dy != 0) {
                SetCursorPos(cursorPos.x + dx, cursorPos.y + dy);
            }

            // Handle scrolling
            if (g_scrollUpActive.load()) {
                SendMouseEvent(MOUSEEVENTF_WHEEL, WHEEL_DELTA);
            }

            if (g_scrollDownActive.load()) {
                SendMouseEvent(MOUSEEVENTF_WHEEL, -WHEEL_DELTA);
            }

            // Handle back/forward buttons
            if (g_backButtonActive.load() && !g_backButtonDown.load()) {
                SendMouseEvent(MOUSEEVENTF_XDOWN, XBUTTON1);
                g_backButtonDown.store(true);
            }
            else if (!g_backButtonActive.load() && g_backButtonDown.load()) {
                SendMouseEvent(MOUSEEVENTF_XUP, XBUTTON1);
                g_backButtonDown.store(false);
            }

            if (g_forwardButtonActive.load() && !g_forwardButtonDown.load()) {
                SendMouseEvent(MOUSEEVENTF_XDOWN, XBUTTON2);
                g_forwardButtonDown.store(true);
            }
            else if (!g_forwardButtonActive.load() && g_forwardButtonDown.load()) {
                SendMouseEvent(MOUSEEVENTF_XUP, XBUTTON2);
                g_forwardButtonDown.store(false);
            }
        }

        // Handle clicks
        if (g_leftClickActive.load() != g_leftClickDown.load()) {
            if (g_leftClickActive.load()) {
                SendMouseEvent(MOUSEEVENTF_LEFTDOWN);
            }
            else {
                SendMouseEvent(MOUSEEVENTF_LEFTUP);
            }
            g_leftClickDown.store(g_leftClickActive.load());
        }

        if (g_rightClickActive.load() != g_rightClickDown.load()) {
            if (g_rightClickActive.load()) {
                SendMouseEvent(MOUSEEVENTF_RIGHTDOWN);
            }
            else {
                SendMouseEvent(MOUSEEVENTF_RIGHTUP);
            }
            g_rightClickDown.store(g_rightClickActive.load());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL_MS));
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hMenu = CreatePopupMenu();
        g_hSubMenu = CreatePopupMenu();
        AppendMenu(g_hSubMenu, MF_STRING, 1, L"Settings");
        AppendMenu(g_hSubMenu, MF_STRING, 2, IsStartupEnabled() ? L"Disable Auto-Startup" : L"Enable Auto-Startup");
        AppendMenu(g_hSubMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(g_hSubMenu, MF_STRING, 3, L"Exit");
        AppendMenu(g_hMenu, MF_POPUP, (UINT_PTR)g_hSubMenu, APP_NAME);
        return 0;

    case WM_APP + 1:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(g_hSubMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            PostMessage(hwnd, WM_NULL, 0, 0);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            ShowSettingsDialog();
        }
        else if (LOWORD(wParam) == 2) {
            SetStartup(!IsStartupEnabled());
        }
        else if (LOWORD(wParam) == 3) {
            g_exitProgram.store(true);
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LoadConfig();

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ValorMouseWindowClass";

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    g_hwnd = CreateWindowEx(0, L"ValorMouseWindowClass", APP_NAME, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, NULL, NULL, hInstance, NULL);

    if (g_hwnd == NULL) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    CreateTrayIcon();
    g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    std::thread mouseThread(MouseMovementThread);

    MSG msg;
    while (!g_exitProgram.load() && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_exitProgram.store(true);
    mouseThread.join();
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
    }
    return (int)msg.wParam;
}