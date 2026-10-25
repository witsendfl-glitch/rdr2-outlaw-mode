// Immediate-mode menu drawn with the game's debug text and rectangles.
//
// Each frame the active page function calls Item()/Option() for every row;
// those calls both draw the row and report whether it was activated.
#pragma once
#include <functional>
#include <string>

namespace ui {

using Page = std::function<void()>;

// Reads the keyboard. Call once per frame before anything else.
void UpdateInput();
bool KeyPressed(int vk);  // went down this frame (game window focused only)

bool IsOpen();
void Open(Page root);
void Close();
void Push(Page page);  // go into a submenu
void Pop();            // back out

// Draws the menu (if open) and runs the current page. Call once per frame.
void Run();

// Inside a page:
void Title(const std::string& title);
bool Item(const std::string& label, const std::string& right = "");
// A row with a value you change with Left/Right. Returns -1, 0 or +1.
int Option(const std::string& label, const std::string& value);
void Text(const std::string& label);  // non-selectable info row

// A short message at the top of the screen.
void Notify(const std::string& text, int ms = 4500);
void DrawNotifications();

// One-off text, e.g. a "Press E" prompt.
void DrawLabel(const std::string& text, float x, float y, float scale, int r, int g, int b, int a);

}  // namespace ui
