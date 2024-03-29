#define UNICODE
#define _UNICODE
#include <windows.h>
#include <strsafe.h>
#include <ShlObj_core.h>
#include <Shlwapi.h>
#include <windowsx.h>
#include "resource.h"

typedef struct {
	WCHAR FilePath[MAX_PATH];
	HINSTANCE Instance;
	HWND MainHandle;
	HWND EditHandle;
	HACCEL Accel;
	BOOL UnsavedChanges;
	HFONT Font;
	LOGFONT LogFont;
	LONG X;
	LONG Y;
	LONG Width;
	LONG Height;
	WCHAR IniPath[MAX_PATH + 17];
} APP_STATE;

WCHAR *CHOSHA_WNDCLASS = L"CHOSHA";
APP_STATE App = { 0 };

/* - Updates window title bar to reflect the currently open file's name
   - Updates internal file path used for the save functionality
   - Adds little star to title when there are unsaved changes
*/
VOID Chosha_SetFilePath(CONST WCHAR *FilePath) {
	// FilePath can be set to NULL to reset the title bar as well as the internal file path.
	// The save function will automatically prompt the user for a location when he tries to save this file.
	BOOL EmptyPath = FilePath == NULL || FilePath[0] == 0;

	WCHAR FileName[MAX_PATH];
	if (!EmptyPath) {
		StringCchCopy(App.FilePath, MAX_PATH, FilePath);

		GetFileTitle(FilePath, FileName, MAX_PATH);
	} else {
		StringCchCopy(FileName, MAX_PATH, L"Untitled");
		ZeroMemory(App.FilePath, MAX_PATH * sizeof(*App.FilePath));
	}

	// Append ' - Chosha' to file name and set it as the window title.
	WCHAR Title[MAX_PATH + 10];
	StringCchPrintf(Title, MAX_PATH + 10, L"%s%s - Chosha", App.UnsavedChanges ? L"*" : L"", FileName);

	SetWindowText(App.MainHandle, Title);
}

/* Read content from a file into the editor
*/
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

	// Window text is the textfield's content
	SetWindowText(App.EditHandle, (WCHAR*)FileBuffer);

	HeapFree(GetProcessHeap(), 0, FileBuffer);
	CloseHandle(File);

	return TRUE;
}

/* Write content from the editor to a file
*/
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
				App.UnsavedChanges = FALSE;
				Chosha_SetFilePath(FilePath);
			}
		}
		CloseHandle(File);
	}
	HeapFree(GetProcessHeap(), 0, FileBuffer);

	return Success;
}

VOID Chosha_Copy(BOOL Cut) {
	DWORD Start, End, Length;
	SendMessage(App.EditHandle, EM_GETSEL, (WPARAM)&Start, (LPARAM)&End);
	if (Start == End) {
		return;
	}
	End += 1;
	Length = End - Start;

	// The number of unicode chars does not match the number of bytes, so just
	// allocate two bytes for every char for now.
	// TODO: Make this prettier.
	WCHAR *Buffer = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, 2 * End);
	GetWindowText(App.EditHandle, Buffer, End);

	WCHAR *ClipboardText = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, 2 * Length);
	StringCchCopy(ClipboardText, 2 * Length, Buffer + Start);

	if (OpenClipboard(App.MainHandle)) {
		if (EmptyClipboard()) {
			SetClipboardData(CF_UNICODETEXT, (HANDLE)ClipboardText);
			if (Cut) {
				Edit_ReplaceSel(App.EditHandle, L"");
			}
		}

		CloseClipboard();
	}

	HeapFree(GetProcessHeap(), 0, Buffer);
}

/* Handle event messages
*/
LRESULT CALLBACK Chosha_WndProc(HWND Handle, UINT Msg, WPARAM WParam, LPARAM LParam) {
	switch (Msg) {
		case WM_CREATE: {
			// The EDIT window class belongs to a simple textfield
			App.EditHandle = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
				WS_CHILD | WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
				0, 0, 0, 0, Handle, (HMENU)ID_EDIT_CONTROL, App.Instance, NULL);
			ShowWindow(Handle, SW_SHOW);
			ShowWindow(App.EditHandle, SW_SHOW);

			// Immediately apply font settings read from the settings file
			App.Font = CreateFontIndirect(&App.LogFont);
			SendMessage(App.EditHandle, WM_SETFONT, (WPARAM)App.Font, TRUE);

			break;
		}
		case WM_SIZE: {
			// Get the window's dimensions to fit textfield into whole window
			RECT Rect;
			GetClientRect(Handle, &Rect);
			INT Width = Rect.right - Rect.left;
			INT Height = Rect.bottom - Rect.top;

			MoveWindow(App.EditHandle, 0, 0, Width, Height, FALSE);
			break;
		}
		// The WM_COMMAND Message gets sent when the user has clicked on one of the items in the menu bar.
		// Also handles messages from child-windows, namely the textfield (edit control).
		case WM_COMMAND: {
			// The Id specifies which exactly menu item was selected.
			UINT Id = LOWORD(WParam);
			switch (Id) {
				case ID_EDIT_CONTROL: {
					UINT Notif = HIWORD(WParam);
					if (Notif == EN_CHANGE) {
						App.UnsavedChanges = TRUE;
						Chosha_SetFilePath(App.FilePath);
					}
					break;
				}
				case ID_FILE_NEW: {
					// Create a new empty editor and reset the file path.
					SetWindowText(App.EditHandle, L"");
					Chosha_SetFilePath(NULL);
					break;
				}
				case ID_FILE_OPEN: {
					// Open a dialog window to let the user select a file to open.
					WCHAR FilePath[MAX_PATH] = { 0 };
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
					// If the app's filepath is zeroed an untitled file is open.
					// In that case ask the user for a location, otherwise just use the path we already have.
					if (App.FilePath[0] == 0) {
						WCHAR FilePath[MAX_PATH] = { 0 };
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
					} else {
						Chosha_SaveFile(App.FilePath);
					}
					break;
				}
				case ID_FILE_SAVEAS: {
					// Similar to the 'save' action, but always prompts the user for a file location.
					WCHAR FilePath[MAX_PATH] = { 0 };
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
					// Opens the settings file in whichever program the user has set as the default for this kind of file.
					ShellExecute(NULL, NULL, App.IniPath, NULL, NULL, SW_SHOWNORMAL);
					break;
				}
				case ID_FILE_EXIT: {
					// Simply closes the window.
					DestroyWindow(Handle);
					break;
				}
				case ID_EDIT_UNDO: {
					Edit_Undo(App.EditHandle);
					break;
				}
				case ID_EDIT_CUT: {
					Chosha_Copy(TRUE);
					break;
				}
				case ID_EDIT_COPY: {
					Chosha_Copy(FALSE);
					break;
				}
				case ID_EDIT_PASTE: {
					if (!OpenClipboard(App.MainHandle)) {
						return;
					}

					UINT Formats[] = { CF_UNICODETEXT, CF_TEXT };
					UINT Format = GetPriorityClipboardFormat(Formats, 2);
					if (Format) {
						HANDLE Text = GetClipboardData(Format);
						if (!Text) {
							return;
						}
						Edit_ReplaceSel(App.EditHandle, Text);
					}
					break;
				}
				case ID_EDIT_DELETE: {
					Edit_ReplaceSel(App.EditHandle, L"");
					break;
				}
				case ID_EDIT_SELECTALL: {
					DWORD End = Edit_GetTextLength(App.EditHandle);
					Edit_SetSel(App.EditHandle, 0, End);
					break;
				}
				case ID_FORMAT_FONT: {
					// Open a dialog to let the user choose various settings relating to the font.
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
						// Store the font handle so that it can be destroyed (cleaned up) at a later time.
						App.Font = CreateFontIndirect(&LogFont);
						// Also store the LOGFONT such that it can be written to the settings file.
						memcpy(&App.LogFont, &LogFont, sizeof(LogFont));
						// Lastly tell the textfield to update its font.
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
			if (App.UnsavedChanges) {
				INT Answer = MessageBox(App.MainHandle, L"There are unsaved changes, any unsaved changes will be lost.\nExit anyway?",
					L"Unsaved changes", MB_OKCANCEL | MB_ICONWARNING);

				if (Answer == IDCANCEL) {
					return;
				}
			}

			// Before destroying the window, store dimensions and position so they can be written to the settings file.
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
			// Clean up potential a font handle and stop the message loop
			if (App.Font) {
				DeleteObject(App.Font);
			}
			PostQuitMessage(0);
			break;
		}
		default: {
			return DefWindowProc(Handle, Msg, WParam, LParam);
		}
	}

	return 0;
}

/* Helper function to register the window class
*/
BOOL Chosha_RegisterClass(HINSTANCE Instance) {
	WNDCLASSEX WC = { 0 };
	WC.cbSize = sizeof(WC);
	WC.style = CS_HREDRAW | CS_VREDRAW;
	WC.lpfnWndProc = Chosha_WndProc;
	WC.hInstance = Instance;
	WC.lpszClassName = CHOSHA_WNDCLASS;
	WC.lpszMenuName = MAKEINTRESOURCE(ID_MENU);
	WC.hIcon = LoadIcon(Instance, MAKEINTRESOURCE(IDI_ICON));

	return RegisterClassEx(&WC) != 0;
}

/* Simple helper function to write integers to an .ini file; the winapi does not provide one
*/
BOOL WritePrivateProfileInt(CONST WCHAR *Section, CONST WCHAR *Key, INT Value, CONST WCHAR *FilePath) {
	WCHAR ValueStr[32];
	StringCchPrintf(ValueStr, 32, L"%d", Value);
	return WritePrivateProfileString(Section, Key, ValueStr, FilePath);
}

VOID Chosha_SaveSettings(CONST WCHAR *IniPath) {
	/* Font */
	BOOL Success = WritePrivateProfileString(L"Font", L"Font", App.LogFont.lfFaceName, IniPath);
	WritePrivateProfileInt(L"Font", L"Height", App.LogFont.lfHeight, IniPath);
	WritePrivateProfileInt(L"Font", L"Weight", App.LogFont.lfWeight, IniPath);
	WritePrivateProfileInt(L"Font", L"Italic", App.LogFont.lfItalic, IniPath);

	/* Position */
	WritePrivateProfileInt(L"Position", L"X", App.X, IniPath);
	WritePrivateProfileInt(L"Position", L"Y", App.Y, IniPath);
	WritePrivateProfileInt(L"Position", L"Width", App.Width, IniPath);
	WritePrivateProfileInt(L"Position", L"Height", App.Height, IniPath);
}

VOID Chosha_LoadSettings(CONST WCHAR *IniPath) {
	/* Font */
	GetPrivateProfileString(L"Font", L"Font", L"Tahoma", App.LogFont.lfFaceName, LF_FACESIZE, IniPath);
	App.LogFont.lfHeight = GetPrivateProfileInt(L"Font", L"Height", -27, IniPath);
	App.LogFont.lfWeight = GetPrivateProfileInt(L"Font", L"Weight", 400, IniPath);
	App.LogFont.lfItalic = GetPrivateProfileInt(L"Font", L"Italic", 0, IniPath);

	/* Position */
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

	SHGetSpecialFolderPath(App.MainHandle, App.IniPath, CSIDL_APPDATA, FALSE);
	PathCombine(App.IniPath, App.IniPath, L"chosha");
	// INVALID_FILE_ATTRIBUTES tells us the folder for the config file does not yet exist and has to be created.
	DWORD Attr = GetFileAttributes(App.IniPath);
	if (Attr == INVALID_FILE_ATTRIBUTES) {
		CreateDirectory(App.IniPath, NULL);
	}
	PathCombine(App.IniPath, App.IniPath, L"config.ini");
	Chosha_LoadSettings(App.IniPath);
	App.Instance = Instance;
	App.MainHandle = CreateWindowEx(0, CHOSHA_WNDCLASS, L"Untitled - Chosha", WS_OVERLAPPEDWINDOW, App.X, App.Y, App.Width, App.Height, NULL, NULL, Instance, NULL);
	App.Accel = LoadAccelerators(App.Instance, MAKEINTRESOURCE(ID_ACCEL));

	/* Event loop */
	MSG Msg = { 0 };
	while (GetMessage(&Msg, NULL, 0, 0)) {
		// TranslateAccelerator dispatches messages itself.
		// If it does not find a matching accelerator run the standard message loop.
		if (!TranslateAccelerator(App.MainHandle, App.Accel, &Msg)) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	}

	// After the program has been quit, save all necessary settings
	Chosha_SaveSettings(App.IniPath);

	return 0;
}
