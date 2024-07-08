#pragma once
#ifndef MUIAPI_H
#define MUIAPI_H

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <Windows.h>

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
HWND show_window(std::wstring title);
void update_window(HWND window);
bool key_pressed(int keyCode);
bool key_released(int keyCode);
bool key_down(int keyCode);
int alert(std::wstring title, std::wstring text, int flags = 0);
void wait(int time);
class timer;
class timer {
public:
	std::chrono::steady_clock::time_point initTime;
	int paused;
	timer();
	int elapsed();
	void elapsed(int amount);
	void restart();
	void pause();
	void resume();
};

void add_list_item(HWND hwndList, LPCTSTR lpszItem);
void add_text(HWND hwndEdit, LPCTSTR lpszText);
void clear_list(HWND hwndList);
HWND create_button(HWND hwndParent, LPCTSTR lpszCaption, int x, int y, int width, int height, int id);
HWND create_text(HWND hwndParent, LPCTSTR lpszCaption, int x, int y, int width, int height, int id);
HWND create_checkbox(HWND hwndParent, LPCTSTR lpszCaption, int x, int y, int width, int height, int id);
HWND create_input_box(HWND hwndParent, int x, int y, int width, int height, int id);
HWND create_list(HWND hwndParent, int x, int y, int width, int height, int id);
void delete_control(HWND hwnd);
void delete_list_item(HWND hwndList, int index);
void delete_list_selections(HWND hwndList);
void edit_list_item(HWND hwndList, int index, LPCTSTR lpszItem);
void edit_text(HWND hwndEdit, LPCTSTR lpszText);
void focus(HWND hwnd);
HWND get_current_focus();
int get_list_position(HWND hwndList);
std::wstring get_focused_list_item_name(HWND hwndList);
bool is_checked(HWND hwndCheckbox);
bool is_enabled(HWND hwnd);
bool is_multiline(HWND hwndEdit);
bool is_pressed(HWND hwndButton);
bool is_readOnly(HWND hwndEdit);
bool is_visible(HWND hwnd);
void set_checkboxMark(HWND hwndCheckbox, bool checked);
void set_list_position(HWND hwndList, int index);
void set_text(HWND hwndEdit, LPCTSTR lpszText);

#endif // MUIAPI_H