#define UNICODE
#define _UNICODE
#include <windows.h>
#include <strsafe.h>
#include <ShlObj_core.h>
#include <Shlwapi.h>
#include "resource.h"

typedef struct {
	WCHAR FilePath[MAX_PATH];
	HINSTANCE Instance;
	HWND MainHandle;
	HWND EditHandle;
	HFONT Font;
	LOGFONT LogFont;
	LONG X;
	LONG Y;
	LONG Width;
	LONG Height;
	WCHAR IniPath[MAX_PATH + 17];
} APP_STATE;

WCHAR *CHOSHA_WNDCLASS = L"CHOSHA";
APP_STATE App;

VOID Chosha_SetFilePath(CONST WCHAR *FilePath) {
	if (FilePath != NULL) {
		StringCchCopy(App.FilePath, MAX_PATH, FilePath);

		WCHAR FileName[MAX_PATH];
		ZeroMemory(FileName, MAX_PATH * sizeof(*FileName));
		GetFileTitle(FilePath, FileName, MAX_PATH);

		WCHAR Title[MAX_PATH + 9];
		ZeroMemory(Title, (MAX_PATH + 9) * sizeof(*Title));
		StringCchCatW(Title, MAX_PATH + 9, FileName);
		StringCchCatW(Title, MAX_PATH + 9, L" - Chosha");
		SetWindowText(App.MainHandle, Title);
	} else {
		SetWindowText(App.MainHandle, L"Untitled - Chosha");
		ZeroMemory(App.FilePath, MAX_PATH * sizeof(*App.FilePath));
	}
}

BOOL Chosha_OpenFile(CONST WCHAR *FilePath) {
	Chosha_SetFilePath(FilePath);

	HANDLE File = CreateFile(FilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (File == INVALID_HANDLE_VALUE) {
		MessageBox(App.MainHandle, L"Could not open the specified file.", L"Error opening file", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	SIZE_T BufferSize = 1024 * 1024;
	BYTE *FileBuffer = HeapAlloc(GetProcessHeap(), 0, BufferSize);
	if (FileBuffer == NULL) {
		MessageBox(App.MainHandle, L"Failed to allocate memory.", L"Error allocating memory", MB_OK | MB_ICONERROR);
		CloseHandle(File);
		return FALSE;
	}

	DWORD BytesRead = 0;
	ReadFile(File, FileBuffer, BufferSize, &BytesRead, NULL);
	FileBuffer[BytesRead] = 0;

	SetWindowText(App.EditHandle, (WCHAR*)FileBuffer);

	HeapFree(GetProcessHeap(), 0, FileBuffer);
	CloseHandle(File);

	return TRUE;
}

BOOL Chosha_SaveFile(CONST WCHAR *FilePath) {
	BOOL Success = FALSE;

	enum { BUFFER_SIZE = 1024 * 1024 };
	BYTE *FileBuffer = HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
	if (FileBuffer == NULL) {
		MessageBox(App.MainHandle, L"Failed to allocate memory.", L"Error allocating memory", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	INT Length = GetWindowText(App.EditHandle, (WCHAR*)FileBuffer, BUFFER_SIZE);
	if (Length) {
		HANDLE File = CreateFile(FilePath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (File == INVALID_HANDLE_VALUE) {
			MessageBox(App.MainHandle, L"Could not save the file.", L"Error saving file", MB_OK | MB_ICONERROR);
			Success = FALSE;
		} else {
			BOOL Success = WriteFile(File, FileBuffer, BUFFER_SIZE, NULL, NULL);
			
			if (Success) {
				Chosha_SetFilePath(FilePath);
			}
		}
		CloseHandle(File);
	}
	HeapFree(GetProcessHeap(), 0, FileBuffer);

	return Success;
}

LRESULT CALLBACK Chosha_WndProc(HWND Handle, UINT Msg, WPARAM WParam, LPARAM LParam) {
	switch (Msg) {
		case WM_CREATE: {
			App.EditHandle = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_MULTILINE, 0, 0, 0, 0, Handle, NULL, App.Instance, NULL);
			ShowWindow(Handle, SW_SHOW);
			ShowWindow(App.EditHandle, SW_SHOW);

			App.Font = CreateFontIndirect(&App.LogFont);
			SendMessage(App.EditHandle, WM_SETFONT, (WPARAM)App.Font, TRUE);

			break;
		}
		case WM_SIZE: {
			RECT Rect;
			GetClientRect(Handle, &Rect);
			INT Width = Rect.right - Rect.left;
			INT Height = Rect.bottom - Rect.top;

			MoveWindow(App.EditHandle, 0, 0, Width, Height, FALSE);
			break;
		}
		case WM_COMMAND: {
			UINT Id = LOWORD(WParam);
			switch (Id) {
				case ID_FILE_NEW: {
					SetWindowText(App.EditHandle, L"");
					Chosha_SetFilePath(NULL);
					break;
				}
				case ID_FILE_OPEN: {
					WCHAR FilePath[MAX_PATH];
					ZeroMemory(FilePath, MAX_PATH * sizeof(*FilePath));
					OPENFILENAME Open = { 0 };
					Open.lStructSize = sizeof(Open);
					Open.hInstance = App.Instance;
					Open.hwndOwner = Handle;
					Open.lpstrTitle = L"Open";
					Open.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
					Open.nMaxFile = MAX_PATH;
					Open.lpstrFile = FilePath;
					if (GetOpenFileName(&Open)) {
						Chosha_OpenFile(FilePath);
					}
					break;
				}
				case ID_FILE_SAVE: {
					if (App.FilePath[0] != 0) {
						Chosha_SaveFile(App.FilePath);
					} else {
						WCHAR FilePath[MAX_PATH];
						ZeroMemory(FilePath, MAX_PATH * sizeof(*FilePath));
						OPENFILENAME Open = { 0 };
						Open.lStructSize = sizeof(Open);
						Open.hInstance = App.Instance;
						Open.hwndOwner = Handle;
						Open.lpstrTitle = L"Save As";
						Open.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
						Open.nMaxFile = MAX_PATH;
						Open.lpstrFile = FilePath;
						if (GetSaveFileName(&Open)) {
							Chosha_SaveFile(FilePath);
						}
						break;
					}
					break;
				}
				case ID_FILE_SAVEAS: {
					WCHAR FilePath[MAX_PATH];
					ZeroMemory(FilePath, MAX_PATH * sizeof(*FilePath));
					OPENFILENAME Open = { 0 };
					Open.lStructSize = sizeof(Open);
					Open.hInstance = App.Instance;
					Open.hwndOwner = Handle;
					Open.lpstrTitle = L"Save As";
					Open.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
					Open.nMaxFile = MAX_PATH;
					Open.lpstrFile = FilePath;
					if (GetSaveFileName(&Open)) {
						Chosha_SaveFile(FilePath);
					}
					break;
				}
				case ID_FILE_SETTINGS: {
					ShellExecute(NULL, NULL, App.IniPath, NULL, NULL, SW_SHOWNORMAL);
					break;
				}
				case ID_FILE_EXIT: {
					DestroyWindow(Handle);
					break;
				}
				case ID_FORMAT_FONT: {
					LOGFONT LogFont = { 0 };
					CHOOSEFONT Dialog = { 0 };
					Dialog.lStructSize = sizeof(Dialog);
					Dialog.hwndOwner = Handle;
					Dialog.hInstance = App.Instance;
					Dialog.lpLogFont = &LogFont;
					if (ChooseFont(&Dialog)) {
						if (App.Font != NULL) {
							DeleteObject(App.Font);
						}
						App.Font = CreateFontIndirect(&LogFont);
						memcpy(&App.LogFont, &LogFont, sizeof(LogFont));
						SendMessage(App.EditHandle, WM_SETFONT, (WPARAM)App.Font, TRUE);
					}
					break;
				}
				case ID_HELP_ABOUT: {
					MessageBox(Handle, L"Chosha v0.1\nAuthor: vzwGrey\nInspiration: @rogerclark",
						L"About Chosha", MB_OK);
					break;
				}
			}
			break;
		}
		case WM_CLOSE: {
			RECT Rect;
			GetWindowRect(App.MainHandle, &Rect);
			App.X = Rect.left;
			App.Y = Rect.top;
			App.Width = Rect.right - Rect.left;
			App.Height = Rect.bottom - Rect.top;

			DestroyWindow(Handle);
			break;
		}
		case WM_DESTROY: {
			if (App.Font) {
				DeleteObject(App.Font);
			}
			PostQuitMessage(0);
			break;
		}
	}

	return DefWindowProc(Handle, Msg, WParam, LParam);
}

BOOL Chosha_RegisterClass(HINSTANCE Instance) {
	WNDCLASSEX WC = { 0 };
	ZeroMemory(&WC, sizeof(WC));
	WC.cbSize = sizeof(WC);
	WC.style = CS_HREDRAW | CS_VREDRAW;
	WC.lpfnWndProc = Chosha_WndProc;
	WC.hInstance = Instance;
	WC.lpszClassName = CHOSHA_WNDCLASS;
	WC.lpszMenuName = MAKEINTRESOURCE(ID_MENU);

	return RegisterClassEx(&WC) != 0;
}

BOOL WritePrivateProfileInt(CONST WCHAR *Section, CONST WCHAR *Key, INT Value, CONST WCHAR *FilePath) {
	WCHAR ValueStr[32];
	StringCchPrintf(ValueStr, 32, L"%d", Value);
	return WritePrivateProfileString(Section, Key, ValueStr, FilePath);
}

VOID Chosha_SaveSettings(CONST WCHAR *IniPath) {
	BOOL Success = WritePrivateProfileString(L"Font", L"Font", App.LogFont.lfFaceName, IniPath);
	WritePrivateProfileInt(L"Font", L"Height", App.LogFont.lfHeight, IniPath);
	WritePrivateProfileInt(L"Font", L"Weight", App.LogFont.lfWeight, IniPath);
	WritePrivateProfileInt(L"Font", L"Italic", App.LogFont.lfItalic, IniPath);

	WritePrivateProfileInt(L"Position", L"X", App.X, IniPath);
	WritePrivateProfileInt(L"Position", L"Y", App.Y, IniPath);
	WritePrivateProfileInt(L"Position", L"Width", App.Width, IniPath);
	WritePrivateProfileInt(L"Position", L"Height", App.Height, IniPath);
}

VOID Chosha_LoadSettings(CONST WCHAR *IniPath) {
	GetPrivateProfileString(L"Font", L"Font", L"Tahoma", App.LogFont.lfFaceName, LF_FACESIZE, IniPath);
	App.LogFont.lfHeight = GetPrivateProfileInt(L"Font", L"Height", -27, IniPath);
	App.LogFont.lfWeight = GetPrivateProfileInt(L"Font", L"Weight", 400, IniPath);
	App.LogFont.lfItalic = GetPrivateProfileInt(L"Font", L"Italic", 0, IniPath);

	App.X = GetPrivateProfileInt(L"Position", L"X", CW_USEDEFAULT, IniPath);
	App.Y = GetPrivateProfileInt(L"Position", L"Y", CW_USEDEFAULT, IniPath);
	App.Width = GetPrivateProfileInt(L"Position", L"Width", 640, IniPath);
	App.Height = GetPrivateProfileInt(L"Position", L"Height", 480, IniPath);
}

INT WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CmdLine, INT Show) {
	if (!Chosha_RegisterClass(Instance)) {
		MessageBox(NULL, L"Could not create window class.", L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}

	ZeroMemory(&App, sizeof(App));
	SHGetSpecialFolderPath(App.MainHandle, App.IniPath, CSIDL_APPDATA, FALSE);
	PathCombine(App.IniPath, App.IniPath, L"chosha");
	DWORD Attr = GetFileAttributes(App.IniPath);
	if (Attr = INVALID_FILE_ATTRIBUTES) {
		CreateDirectory(App.IniPath, NULL);
	}
	PathCombine(App.IniPath, App.IniPath, L"config.ini");
	Chosha_LoadSettings(App.IniPath);
	App.Instance = Instance;
	App.MainHandle = CreateWindowEx(0, CHOSHA_WNDCLASS, L"Untitled - Chosha", WS_OVERLAPPEDWINDOW, App.X, App.Y, App.Width, App.Height, NULL, NULL, Instance, NULL);

	MSG Msg = { 0 };
	while (GetMessage(&Msg, NULL, 0, 0)) {
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	Chosha_SaveSettings(App.IniPath);

	return 0;
}
