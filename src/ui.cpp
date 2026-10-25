#include "ui.h"

#include <windows.h>

#include <deque>
#include <vector>

#include "natives.h"

namespace ui {
namespace {

constexpr float kX = 0.03f;
constexpr float kWidth = 0.28f;
constexpr float kTop = 0.08f;
constexpr float kRow = 0.034f;
constexpr int kVisibleRows = 14;

bool g_down[256] = {};
bool g_pressed[256] = {};

struct Frame {
    Page page;
    int selected = 0;
};
std::vector<Frame> g_stack;
std::vector<Page> g_pendingPush;
int g_pendingPops = 0;

// Per-frame build state.
std::string g_title;
int g_row = 0;          // selectable rows seen so far this frame
int g_drawn = 0;        // rows drawn (selectable or not)
int g_lastRowCount = 0;  // selectable rows last frame
bool g_accept = false, g_left = false, g_right = false;

struct Note {
    std::string text;
    int until;
};
std::deque<Note> g_notes;

bool GameFocused() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

void Sound(const char* name) { PLAY_SOUND_FRONTEND(name, "HUD_SHOP_SOUNDSET", TRUE, 0); }

int FirstVisible(int selected) {
    if (selected < kVisibleRows) return 0;
    return selected - kVisibleRows + 1;
}

bool RowVisible(int index, int selected) {
    int first = FirstVisible(selected);
    return index >= first && index < first + kVisibleRows;
}

void DrawRow(const std::string& left, const std::string& right, bool selected, bool dim) {
    float y = kTop + kRow * (1 + g_drawn);
    DRAW_RECT(kX + kWidth / 2, y + kRow / 2, kWidth, kRow,
              selected ? 180 : 0, selected ? 30 : 0, selected ? 30 : 0, 190, FALSE, FALSE);
    int c = dim ? 160 : 245;
    DrawLabel(left, kX + 0.008f, y + 0.004f, 0.32f, c, c, c, 255);
    if (!right.empty()) {
        // No right-align in debug text, so push long values a little left.
        float rx = kX + kWidth - 0.008f - 0.0062f * static_cast<float>(right.size());
        DrawLabel(right, rx, y + 0.004f, 0.32f, 230, 200, 140, 255);
    }
    g_drawn++;
}

}  // namespace

void UpdateInput() {
    bool focused = GameFocused();
    for (int vk = 1; vk < 256; ++vk) {
        bool down = focused && (GetAsyncKeyState(vk) & 0x8000) != 0;
        g_pressed[vk] = down && !g_down[vk];
        g_down[vk] = down;
    }
}

bool KeyPressed(int vk) { return g_pressed[vk & 0xFF]; }

bool IsOpen() { return !g_stack.empty(); }

void Open(Page root) {
    g_stack.clear();
    g_stack.push_back({root, 0});
    Sound("SELECT");
}

void Close() {
    g_stack.clear();
    g_lastRowCount = 0;
    Sound("BACK");
}

void Push(Page page) { g_pendingPush.push_back(page); }
void Pop() { g_pendingPops++; }

void Run() {
    if (g_stack.empty()) return;
    Frame& frame = g_stack.back();

    // Navigation uses last frame's row count.
    if (g_lastRowCount > 0) {
        if (KeyPressed(VK_UP) || KeyPressed(VK_NUMPAD8)) {
            frame.selected = (frame.selected + g_lastRowCount - 1) % g_lastRowCount;
            Sound("NAV_UP");
        }
        if (KeyPressed(VK_DOWN) || KeyPressed(VK_NUMPAD2)) {
            frame.selected = (frame.selected + 1) % g_lastRowCount;
            Sound("NAV_DOWN");
        }
        if (frame.selected >= g_lastRowCount) frame.selected = g_lastRowCount - 1;
    }
    g_accept = KeyPressed(VK_RETURN) || KeyPressed(VK_NUMPAD5);
    g_left = KeyPressed(VK_LEFT) || KeyPressed(VK_NUMPAD4);
    g_right = KeyPressed(VK_RIGHT) || KeyPressed(VK_NUMPAD6);
    bool back = KeyPressed(VK_BACK) || KeyPressed(VK_NUMPAD0);

    g_title.clear();
    g_row = 0;
    g_drawn = 0;
    Page page = frame.page;  // copy: the page may push/pop
    page();
    g_lastRowCount = g_row;
    if (g_stack.empty()) {  // the page closed the menu
        g_pendingPush.clear();
        g_pendingPops = 0;
        return;
    }

    // Title bar and footer.
    DRAW_RECT(kX + kWidth / 2, kTop + kRow / 2, kWidth, kRow, 120, 15, 15, 230, FALSE, FALSE);
    DrawLabel(g_title.empty() ? "Outlaw Mode" : g_title, kX + 0.008f, kTop + 0.003f, 0.38f, 255, 255, 255, 255);
    float footY = kTop + kRow * (1 + g_drawn);
    DRAW_RECT(kX + kWidth / 2, footY + kRow / 2, kWidth, kRow * 0.8f, 0, 0, 0, 170, FALSE, FALSE);
    DrawLabel("Arrows: move   Enter: select   Backspace: back", kX + 0.008f, footY + 0.004f, 0.25f,
             180, 180, 180, 255);

    if (back) g_pendingPops++;
    while (g_pendingPops > 0 && !g_stack.empty()) {
        g_stack.pop_back();
        g_pendingPops--;
        Sound("BACK");
    }
    g_pendingPops = 0;
    for (auto& p : g_pendingPush) {
        g_stack.push_back({p, 0});
        Sound("SELECT");
    }
    g_pendingPush.clear();
}

void Title(const std::string& title) { g_title = title; }

bool Item(const std::string& label, const std::string& right) {
    if (g_stack.empty()) return false;
    int index = g_row++;
    int selected = g_stack.back().selected;
    bool isSelected = index == selected;
    if (RowVisible(index, selected)) DrawRow(label, right, isSelected, false);
    if (isSelected && g_accept) {
        g_accept = false;
        Sound("SELECT");
        return true;
    }
    return false;
}

int Option(const std::string& label, const std::string& value) {
    if (g_stack.empty()) return 0;
    int index = g_row++;
    int selected = g_stack.back().selected;
    bool isSelected = index == selected;
    if (RowVisible(index, selected)) DrawRow(label, "< " + value + " >", isSelected, false);
    if (!isSelected) return 0;
    if (g_left) {
        g_left = false;
        Sound("NAV_LEFT");
        return -1;
    }
    if (g_right || g_accept) {
        g_right = g_accept = false;
        Sound("NAV_RIGHT");
        return 1;
    }
    return 0;
}

void Text(const std::string& label) {
    if (g_stack.empty()) return;
    if (g_drawn < kVisibleRows + 2) DrawRow(label, "", false, true);
}

void Notify(const std::string& text, int ms) {
    g_notes.push_back({text, GET_GAME_TIMER() + ms});
    while (g_notes.size() > 4) g_notes.pop_front();
}

void DrawNotifications() {
    int now = GET_GAME_TIMER();
    while (!g_notes.empty() && g_notes.front().until < now) g_notes.pop_front();
    float y = 0.03f;
    for (auto& n : g_notes) {
        DRAW_RECT(0.5f, y + 0.017f, 0.42f, 0.034f, 0, 0, 0, 170, FALSE, FALSE);
        DrawLabel(n.text, 0.5f - 0.2f, y + 0.004f, 0.33f, 255, 255, 255, 255);
        y += 0.038f;
    }
}

void DrawLabel(const std::string& text, float x, float y, float scale, int r, int g, int b, int a) {
    BG_SET_TEXT_SCALE(scale, scale);
    BG_SET_TEXT_COLOR(r, g, b, a);
    BG_DISPLAY_TEXT(LiteralString(text.c_str()), x, y);
}

}  // namespace ui
