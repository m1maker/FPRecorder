#include<chrono>
#include <iostream>
#include <string>
#include <vector>
#include <Windows.h>

bool g_KeysPressed[256];
bool g_KeysReleased[256];
bool g_KeysDown[256];
bool g_WindowShown = false;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		exit(0);
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND show_window(std::wstring title)
{
	for (auto i = 0; i < 256; i++)
	{
		g_KeysReleased[i] = true;
	}

	HINSTANCE hInstance = GetModuleHandle(NULL);

	WNDCLASSEXW wc;
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
	wc.lpszClassName = L"bgtgame";
	wc.hIconSm = NULL;

	if (!RegisterClassExW(&wc))
	{
		return nullptr;
	}

	HWND hwnd = CreateWindowExW(256, L"bgtgame", title.c_str(), WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);
	if (!hwnd)
	{
		return nullptr;
	}

	UpdateWindow(hwnd);
	return hwnd;
}

void update_window(HWND window)
{
	if (!g_WindowShown)
	{
		ShowWindow(window, SW_SHOW);
		g_WindowShown = true;
	}

	MSG msg;
	UpdateWindow(window);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (IsDialogMessage(window, &msg))
		{
			switch (msg.message)
			{
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
			default:
				break;
			}
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
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

void wait(int time)
{
	std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
	int elapsed = 0;

	while (elapsed < time)
	{
		update_window(GetActiveWindow());
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

HWND create_input_box(HWND hwndParent, int x, int y, int width, int height, int id)
{
	return CreateWindow(
		TEXT("EDIT"),
		NULL,
		WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP,
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
	return SendMessage(hwndButton, BM_GETSTATE, 0, 0) & BST_PUSHED;
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
