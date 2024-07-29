#include<chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <Windows.h>

namespace gui {
	bool g_Hotkeys[512];
	bool g_KeysPressed[256];
	bool g_KeysReleased[256];
	bool g_KeysDown[256];
	HWND g_CurrentFocused;
	HWND g_MainWindow;
	bool try_close = false;
	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg) {
		case WM_CLOSE: {
			try_close = true;
			return 0;
		}
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	WNDCLASSEXW wc;
	HWND show_window(std::wstring title)
	{
		for (auto i = 0; i < 256; i++)
		{
			g_KeysReleased[i] = true;
		}
		if (IsWindow(g_MainWindow)) {
			ShowWindow(g_MainWindow, SW_SHOW);
			return g_MainWindow;
		}
		HINSTANCE hInstance = GetModuleHandle(NULL);

		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.style = 0;
		wc.lpfnWndProc = WndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hInstance;
		wc.hIcon = NULL;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = L"NGTApp";
		wc.hIconSm = NULL;

		if (!RegisterClassExW(&wc))
		{
			return nullptr;
		}

		HWND hwnd = CreateWindowExW(256, L"NGTApp", title.c_str(), WS_VISIBLE | WS_CAPTION | WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);
		if (!hwnd)
		{
			return nullptr;
		}
		ShowWindow(hwnd, SW_SHOW);
		UpdateWindow(hwnd);
		SetForegroundWindow(hwnd);
		g_MainWindow = hwnd;
		return hwnd;
	}
	bool hide_window(HWND window) {
		if (window == nullptr)return false;
		return ShowWindow(window, SW_HIDE);
	}
	void update_window(HWND window, bool wait_event)
	{
		MSG msg;
		UpdateWindow(window);
		if (wait_event)
			GetMessage(&msg, NULL, 0, 0);
		else
			PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		switch (msg.message) {
		case WM_HOTKEY: {
			g_Hotkeys[msg.wParam] = true;
		}
		case WM_KEYDOWN:
			g_KeysDown[msg.wParam] = true;
			if (g_KeysPressed[msg.wParam] == false && g_KeysReleased[msg.wParam] == true && g_KeysDown[msg.wParam] == true)
			{
				g_KeysPressed[msg.wParam] = true;
				g_KeysReleased[msg.wParam] = false;
			}
			else if (g_KeysReleased[msg.wParam] == false)
			{
				g_KeysPressed[msg.wParam] = false;
			}
			break;
		case WM_KEYUP:
			g_KeysDown[msg.wParam] = false;
			g_KeysPressed[msg.wParam] = false;
			g_KeysReleased[msg.wParam] = true;
			break;
		}
		if (!IsDialogMessage(window, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (GetForegroundWindow() == window) {
				g_CurrentFocused = GetFocus();
			}
		}
	}

	bool key_pressed(int keyCode)
	{
		if (g_KeysPressed[keyCode] == true)
		{
			g_KeysPressed[keyCode] = false;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool key_released(int keyCode)
	{
		if (g_KeysPressed[keyCode] == false && g_KeysReleased[keyCode] == true)
		{
			g_KeysReleased[keyCode] = g_KeysPressed[keyCode];
			return true;
		}
		else
		{
			return false;
		}
	}

	bool key_down(int keyCode)
	{
		return g_KeysDown[keyCode];
	}

	int alert(std::wstring title, std::wstring text, int flags)
	{
		HWND windowHandle = GetForegroundWindow();
		return MessageBoxW(windowHandle, text.c_str(), title.c_str(), flags);
	}
	int get_vk_code(const std::string& key) {
		static const std::unordered_map<std::string, int> keyMap = {
			{ "Backspace", VK_BACK },
			{ "Tab", VK_TAB },
			{ "Enter", VK_RETURN },
			{ "Shift", VK_SHIFT },
			{ "Control", VK_CONTROL },
			{ "Alt", VK_MENU },
			{ "Pause", VK_PAUSE },
			{ "CapsLock", VK_CAPITAL },
			{ "Escape", VK_ESCAPE },
			{ "Space", VK_SPACE },
			{ "PageUp", VK_PRIOR },
			{ "PageDown", VK_NEXT },
			{ "End", VK_END },
			{ "Home", VK_HOME },
			{ "Left", VK_LEFT },
			{ "Up", VK_UP },
			{ "Right", VK_RIGHT },
			{ "Down", VK_DOWN },
			{ "Insert", VK_INSERT },
			{ "Delete", VK_DELETE },
			{ "F1", VK_F1 },
			{ "F2", VK_F2 },
			{ "F3", VK_F3 },
			{ "F4", VK_F4 },
			{ "F5", VK_F5 },
			{ "F6", VK_F6 },
			{ "F7", VK_F7 },
			{ "F8", VK_F8 },
			{ "F9", VK_F9 },
			{ "F10", VK_F10 },
			{ "F11", VK_F11 },
			{ "F12", VK_F12 },
			{ "0", '0' },
			{ "1", '1' },
			{ "2", '2' },
			{ "3", '3' },
			{ "4", '4' },
			{ "5", '5' },
			{ "6", '6' },
			{ "7", '7' },
			{ "8", '8' },
			{ "9", '9' },
			{ "A", 'A' },
			{ "B", 'B' },
			{ "C", 'C' },
			{ "D", 'D' },
			{ "E", 'E' },
			{ "F", 'F' },
			{ "G", 'G' },
			{ "H", 'H' },
			{ "I", 'I' },
			{ "J", 'J' },
			{ "K", 'K' },
			{ "L", 'L' },
			{ "M", 'M' },
			{ "N", 'N' },
			{ "O", 'O' },
			{ "P", 'P' },
			{ "Q", 'Q' },
			{ "R", 'R' },
			{ "S", 'S' },
			{ "T", 'T' },
			{ "U", 'U' },
			{ "V", 'V' },
			{ "W", 'W' },
			{ "X", 'X' },
			{ "Y", 'Y' },
			{ "Z", 'Z' },
			{ "`", VK_OEM_3 },
			{ "-", VK_OEM_MINUS },
			{ "=", VK_OEM_PLUS },
			{ "[", VK_OEM_4 },
			{ "]", VK_OEM_5 },
			{ "\\\\", VK_OEM_6 },
			{ ";", VK_OEM_1 },
			{ "'", VK_OEM_7 },
			{ ",", VK_OEM_COMMA },
			{ ".", VK_OEM_PERIOD },
			{ "/", VK_OEM_2 }
		};

		auto it = keyMap.find(key);
		if (it != keyMap.end()) {
			return it->second;
		}
		return 0;
	}

	DWORD get_mod_code(const std::string& key) {
		if (key == "Ctrl") {
			return MOD_CONTROL;
		}
		else if (key == "Alt") {
			return MOD_ALT;
		}
		else if (key == "Shift") {
			return MOD_SHIFT;
		}
		else if (key == "Windows") {
			return MOD_WIN;
		}

		return 0; // No modifier
	}

	// Function to parse hotkey string
	bool parse_hotkey(const std::string& hotkeyStr, DWORD& modKeys, int& vkKey) {
		std::stringstream ss(hotkeyStr);
		std::string keyPart;
		modKeys = 0;
		vkKey = 0;

		while (std::getline(ss, keyPart, '+')) {

			DWORD mod = get_mod_code(keyPart);
			if (mod != 0) {
				modKeys |= mod;
			}
			else {
				vkKey = get_vk_code(keyPart);
				if (vkKey == 0) {
					return false; // Invalid key
				}
			}
		}

		return true; // Success
	}

	bool hotkey_pressed(int id) {
		if (g_Hotkeys[id] == true) {
			g_Hotkeys[id] = false;
			return true;
		}
		return false;
	}
	void wait(int time)
	{
		std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
		int elapsed = 0;

		while (elapsed < time)
		{
			update_window(g_MainWindow, false);
			elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
		}
	}

	class timer
	{
	public:
		std::chrono::steady_clock::time_point initTime = std::chrono::steady_clock::now();

		int paused = false;
		timer()
		{
			restart();
		}

		int elapsed()
		{
			if (paused != 0)
			{
				return paused;
			}
			else
			{
				return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - initTime).count();
			}
		}

		void elapsed(int amount)
		{
			if (paused == 0)
			{
				initTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(amount);
			}
			else
			{
				paused = amount;
			}
		}

		void restart()
		{
			initTime = std::chrono::steady_clock::now();
			paused = 0;
		}

		void pause()
		{
			paused = elapsed();
		}

		void resume()
		{
			initTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(paused);
			paused = 0;
		}
	};

	void add_list_item(HWND hwndList, LPCTSTR lpszItem)
	{
		SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)lpszItem);
	}

	void add_text(HWND hwndEdit, LPCTSTR lpszText)
	{
		int nLength = GetWindowTextLength(hwndEdit);
		SendMessage(hwndEdit, EM_SETSEL, (WPARAM)nLength, (LPARAM)nLength);
		SendMessage(hwndEdit, EM_REPLACESEL, 0, (LPARAM)lpszText);
	}
	std::wstring get_text(HWND hwndEdit)
	{
		int nLength = GetWindowTextLength(hwndEdit);
		std::wstring text;
		if (nLength > 0)
		{
			text.resize(nLength + 1);
			GetWindowText(hwndEdit, &text[0], nLength + 1);
		}
		return text;
	}
	void clear_list(HWND hwndList)
	{
		SendMessage(hwndList, LB_RESETCONTENT, 0, 0);
	}

	HWND create_button(HWND hwndParent, LPCTSTR lpszCaption, int x, int y, int width, int height, int id)
	{
		return CreateWindow(
			TEXT("BUTTON"),
			lpszCaption,
			WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
			x,
			y,
			width,
			height,
			hwndParent,
			(HMENU)id,
			NULL,
			NULL
		);
	}

	HWND create_text(HWND hwndParent, LPCTSTR lpszCaption, int x, int y, int width, int height, int id)
	{
		return CreateWindow(
			TEXT("STATIC"),
			lpszCaption,
			WS_VISIBLE | WS_CHILD,
			x,
			y,
			width,
			height,
			hwndParent,
			(HMENU)id,
			NULL,
			NULL
		);
	}

	HWND create_checkbox(HWND hwndParent, LPCTSTR lpszCaption, int x, int y, int width, int height, int id)
	{
		return CreateWindow(
			TEXT("BUTTON"),
			lpszCaption,
			WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_TABSTOP,
			x,
			y,
			width,
			height,
			hwndParent,
			(HMENU)id,
			NULL,
			NULL
		);
	}

	HWND create_input_box(HWND hwndParent, bool secure, int x, int y, int width, int height, int id)
	{
		DWORD style = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP;
		if (secure)
		{
			style |= ES_PASSWORD;
		}

		return CreateWindow(
			TEXT("EDIT"),
			NULL,
			style,
			x,
			y,
			width,
			height,
			hwndParent,
			(HMENU)id,
			NULL,
			NULL
		);
	}

	HWND create_list(HWND hwndParent, int x, int y, int width, int height, int id)
	{
		return CreateWindow(
			TEXT("LISTBOX"),
			NULL,
			WS_VISIBLE | WS_CHILD | WS_TABSTOP,
			x,
			y,
			width,
			height,
			hwndParent,
			(HMENU)id,
			NULL,
			NULL
		);
	}

	void delete_control(HWND hwnd)
	{
		DestroyWindow(hwnd);
	}

	void delete_list_item(HWND hwndList, int index)
	{
		SendMessage(hwndList, LB_DELETESTRING, (WPARAM)index, 0);
	}

	void delete_list_selections(HWND hwndList)
	{
		int count = SendMessage(hwndList, LB_GETSELCOUNT, 0, 0);
		if (count > 0)
		{
			std::vector<int> selections(count);
			SendMessage(hwndList, LB_GETSELITEMS, (WPARAM)count, (LPARAM)selections.data());

			for (int i = count - 1; i >= 0; i--)
			{
				SendMessage(hwndList, LB_DELETESTRING, (WPARAM)selections[i], 0);
			}
		}
	}

	void edit_list_item(HWND hwndList, int index, LPCTSTR lpszItem)
	{
		SendMessage(hwndList, LB_DELETESTRING, (WPARAM)index, 0);
		SendMessage(hwndList, LB_INSERTSTRING, (WPARAM)index, (LPARAM)lpszItem);
	}

	void edit_text(HWND hwndEdit, LPCTSTR lpszText)
	{
		SetWindowText(hwndEdit, lpszText);
	}

	void focus(HWND hwnd)
	{
		SetFocus(hwnd);
	}

	HWND get_current_focus()
	{
		return GetFocus();
	}

	int get_list_position(HWND hwndList)
	{
		return SendMessage(hwndList, LB_GETCURSEL, 0, 0);
	}

	std::wstring get_focused_list_item_name(HWND hwndList)
	{
		int focusedIndex = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
		if (focusedIndex != LB_ERR)
		{
			int textLength = SendMessage(hwndList, LB_GETTEXTLEN, focusedIndex, 0);
			if (textLength != LB_ERR)
			{
				std::wstring listItemText;
				listItemText.resize(textLength + 1);  // Add 1 for null terminator
				SendMessage(hwndList, LB_GETTEXT, focusedIndex, reinterpret_cast<LPARAM>(listItemText.data()));
				listItemText.pop_back();  // Remove the null terminator
				return listItemText;
			}
		}
		return L"";  // Return an empty string if there's an error
	}

	bool is_checked(HWND hwndCheckbox)
	{
		return SendMessage(hwndCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
	}

	bool is_enabled(HWND hwnd)
	{
		return IsWindowEnabled(hwnd) != FALSE;
	}

	bool is_multiline(HWND hwndEdit)
	{
		return (GetWindowLong(hwndEdit, GWL_STYLE) & ES_MULTILINE) != 0;
	}

	bool is_pressed(HWND hwndButton)
	{
		return (SendMessage(hwndButton, BM_GETSTATE, 0, 0) & BST_PUSHED) or (GetFocus() == hwndButton && key_pressed(VK_RETURN));
	}

	bool is_readOnly(HWND hwndEdit)
	{
		return (GetWindowLong(hwndEdit, GWL_STYLE) & ES_READONLY) != 0;
	}

	bool is_visible(HWND hwnd)
	{
		return IsWindowVisible(hwnd) != FALSE;
	}

	void set_checkboxMark(HWND hwndCheckbox, bool checked)
	{
		SendMessage(hwndCheckbox, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
	}

	void set_list_position(HWND hwndList, int index)
	{
		SendMessage(hwndList, LB_SETCURSEL, (WPARAM)index, 0);
	}

	void set_text(HWND hwndEdit, LPCTSTR lpszText)
	{
		SetWindowText(hwndEdit, lpszText);
	}
}