#define UNICODE
#define _UNICODE
#include <windows.h>
#include <strsafe.h>
#include "resource.h"

typedef struct {
	WCHAR FilePath[MAX_PATH];
	HINSTANCE Instance;
	HWND MainHandle;
	HWND EditHandle;
	HFONT Font;
} APP_STATE;

WCHAR *CHOSHA_WNDCLASS = L"CHOSHA";
APP_STATE App;

VOID Chosha_SetFilePath(CONST WCHAR *FilePath) {
	StringCchCopy(App.FilePath, MAX_PATH, FilePath);

	WCHAR FileName[MAX_PATH];
	ZeroMemory(FileName, MAX_PATH * sizeof(*FileName));
	GetFileTitle(FilePath, FileName, MAX_PATH);

	WCHAR Title[MAX_PATH + 9];
	ZeroMemory(Title, (MAX_PATH + 9) * sizeof(*Title));
	StringCchCatW(Title, MAX_PATH + 9, FileName);
	StringCchCatW(Title, MAX_PATH + 9, L" - Chosha");
	SetWindowText(App.MainHandle, Title);
}

BOOL Chosha_OpenFile(CONST WCHAR *FilePath) {
	Chosha_SetFilePath(FilePath);

	HANDLE File = CreateFile(FilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (File == INVALID_HANDLE_VALUE) {
		MessageBox(App.MainHandle, L"Could not open the specified file.", L"Error opening file", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	BYTE *FileBuffer = HeapAlloc(GetProcessHeap(), 0, 1024 * 1024);
	if (FileBuffer == NULL) {
		MessageBox(App.MainHandle, L"Failed to allocate memory.", L"Error allocating memory", MB_OK | MB_ICONERROR);
		CloseHandle(File);
		return FALSE;
	}

	BYTE *FilePos = FileBuffer;
	enum { BUFFER_SIZE = 4096 };
	BYTE Buffer[BUFFER_SIZE];
	DWORD BytesRead = -1;
	while (BytesRead != 0) {
		ReadFile(File, Buffer, BUFFER_SIZE, &BytesRead, NULL);
		memcpy(FilePos, Buffer, BytesRead);
		FilePos += BytesRead;
	}
	*FilePos = 0;

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
			DWORD BytesWritten = -1;
			BOOL Success = WriteFile(File, FileBuffer, BUFFER_SIZE, &BytesWritten, NULL);
			
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
					Chosha_SaveFile(App.FilePath);
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
						App.Font = CreateFontIndirect(&LogFont);
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

INT WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CmdLine, INT Show) {
	if (!Chosha_RegisterClass(Instance)) {
		MessageBox(NULL, L"Could not create window class.", L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}

	ZeroMemory(&App, sizeof(App));
	App.Instance = Instance;
	App.MainHandle = CreateWindowEx(0, CHOSHA_WNDCLASS, L"Untitled - Chosha", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, NULL, NULL, Instance, NULL);

	MSG Msg = { 0 };
	while (GetMessage(&Msg, NULL, 0, 0)) {
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	return 0;
}
