#include <Windows.h>
#include <CommCtrl.h>
#include <tchar.h>
#include "resource.h"

/*
*   Copyright (C) 2017,2018 by Andy Uribe CA6JAU
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' "\
  "version='6.0.0.0' "\
  "processorArchitecture='*' "\
  "publicKeyToken='6595b64144ccf1df' "\
  "language='*'\"")

#pragma comment(lib, "ComCtl32.lib")

wchar_t curr_path[MAX_PATH];
wchar_t file_path[MAX_PATH];
wchar_t comm_port[100];

void SavePreferences(HWND hDlg)
{
	GetDlgItemText(hDlg, IDCOMM, comm_port, 100);
	GetDlgItemText(hDlg, IDFILE, file_path, MAX_PATH);
	SetKeyData(HKEY_CURRENT_USER, L"Software\\ZUMRadio\\ZUMspotFW", REG_SZ, L"COMPort", comm_port, 2 * wcslen(comm_port));
	SetKeyData(HKEY_CURRENT_USER, L"Software\\ZUMRadio\\ZUMspotFW", REG_SZ, L"FWFile", file_path, 2 * wcslen(file_path));
}

void onBrowse(HWND hDlg)
{
	wchar_t filename[MAX_PATH];
	wchar_t bin_dir[MAX_PATH];
	wchar_t tmp_path[MAX_PATH];

	_wsplitpath(file_path, bin_dir, tmp_path, NULL, NULL);
	_tcscat(bin_dir, tmp_path);

	OPENFILENAME ofn;
	ZeroMemory(&filename, sizeof(filename));
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hDlg;
	ofn.lpstrInitialDir = bin_dir;
	ofn.lpstrFilter = L"BIN Files\0*.bin\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = L"Select a ZUMspot firmware bin file";
	ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&ofn))
		SetDlgItemText(hDlg, IDFILE, filename);
}

void onClose(HWND hDlg)
{
	SavePreferences(hDlg);
	if (MessageBox(hDlg, TEXT("Close ZUMspotFW ?"), TEXT("Close"), MB_ICONQUESTION | MB_YESNO) == IDYES) {
		DestroyWindow(hDlg);
	}
}

void onCancel(HWND hDlg)
{
	onClose(hDlg);
}

int ResetZUMspot(LPCWSTR commPort)
{
	HANDLE hComm;
	DWORD dNoOfBytesWritten = 0;
	wchar_t commPortTmp[100] = L"\\\\.\\\\";

	_tcscat(commPortTmp, commPort);

	hComm = CreateFile(commPortTmp,   //port name
		GENERIC_READ | GENERIC_WRITE, //Read/Write
		0,                            // No Sharing
		NULL,                         // No Security
		OPEN_EXISTING,// Open existing port only
		0,            // Non Overlapped I/O
		NULL);        // Null for Comm Devices

	if (hComm == INVALID_HANDLE_VALUE) {
		MessageBox(0, L"Error opening serial port. You can ignore this error if you have just installed the bootloader.", L"Info", MB_OK | MB_ICONINFORMATION);
		CloseHandle(hComm);
		return 1;
	}

	// Send magic sequence to USB bootloader (reset - enter to DFU mode)

	EscapeCommFunction(hComm, CLRRTS);
	EscapeCommFunction(hComm, CLRDTR);
	EscapeCommFunction(hComm, SETDTR);

	Sleep(50);

	EscapeCommFunction(hComm, CLRDTR);
	EscapeCommFunction(hComm, SETRTS);
	EscapeCommFunction(hComm, SETDTR);

	Sleep(50);

	EscapeCommFunction(hComm, CLRDTR);

	Sleep(50);

	WriteFile(hComm,
		"1EAF",
		4,
		&dNoOfBytesWritten,
		NULL);

	if(dNoOfBytesWritten == 0) {
		MessageBox(0, L"Error sending magic sequence !", L"Error", MB_OK | MB_ICONEXCLAMATION);
		CloseHandle(hComm);
		return 1;
	}

	CloseHandle(hComm);

	Sleep(1200);

	return 1;
}

void onUpload(HWND hDlg)
{
	if (!GetDlgItemText(hDlg, IDFILE, file_path, MAX_PATH)) {
		MessageBox(hDlg, L"No firmware file specified !", L"Error", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	
	if (!GetDlgItemText(hDlg, IDCOMM, comm_port, 100)) {
	MessageBox(hDlg, L"No serial COM specified !", L"Error", MB_OK | MB_ICONEXCLAMATION);
	return;
	}
	
	SavePreferences(hDlg);

	if (MessageBox(hDlg, TEXT("Are you sure to upload firmware ?"), TEXT("Information"), MB_ICONQUESTION | MB_YESNO) == IDYES) {

		if (ResetZUMspot(comm_port)) {
			wchar_t cmd_arg[100] = L"-D \"";
			_tcscat(cmd_arg, file_path);
			_tcscat(cmd_arg, L"\" -d 1eaf:0003 -a 2 -R -R");

			HINSTANCE result;
			result = ShellExecute(hDlg, L"open", L"dfu-util.exe", cmd_arg, curr_path, SW_SHOW);
			if ((int)result <= 32) {
				MessageBox(hDlg, L"dfu-util.exe error !", L"Error", MB_OK | MB_ICONEXCLAMATION);
				return;
			}

			//MessageBox(hDlg, L"Press the reset button of your ZUMspot", L"Information", MB_OK);
		}
	}
}

void onSTlink(HWND hDlg)
{
	if (MessageBox(hDlg, TEXT("Are you sure to upload DFU Bootloader ?"), TEXT("Information"), MB_ICONQUESTION | MB_YESNO) == IDYES) {

		wchar_t cmd_arg[100] = L"write generic_boot20_pc13.bin 0x08000000";

		HINSTANCE result;
		result = ShellExecute(hDlg, L"open", L"st-flash.exe", L"erase", curr_path, SW_SHOW);
		if ((int)result <= 32) {
			MessageBox(hDlg, L"st-flash.exe erase error !", L"Error", MB_OK | MB_ICONEXCLAMATION);
		}

		Sleep(1200);

		result = ShellExecute(hDlg, L"open", L"st-flash.exe", cmd_arg, curr_path, SW_SHOW);
		if ((int)result <= 32) {
			MessageBox(hDlg, L"st-flash.exe write error !", L"Error", MB_OK | MB_ICONEXCLAMATION);
			return;
		}

		MessageBox(hDlg, L"Re-connect your ZUMspot USB", L"Information", MB_OK | MB_ICONINFORMATION);
	}
}

void onSerial(HWND hDlg)
{
	if (MessageBox(hDlg, TEXT("Are you sure to upload DFU Bootloader ?"), TEXT("Information"), MB_ICONQUESTION | MB_YESNO) == IDYES) {

		SavePreferences(hDlg);

		MessageBox(hDlg, L"Configure jumper BOOT0=1 and press the reset button of your ZUMspot.", L"Information", MB_OK | MB_ICONINFORMATION);

		wchar_t cmd_arg[100] = L"-v -w generic_boot20_pc13.bin -g 0x0 ";
		_tcscat(cmd_arg, comm_port);

		HINSTANCE result;
		result = ShellExecute(hDlg, L"open", L"stm32flash.exe", cmd_arg, curr_path, SW_SHOW);
		if ((int)result <= 32) {
			MessageBox(hDlg, L"stm32flash.exe error !", L"Error", MB_OK | MB_ICONEXCLAMATION);
			return;
		}

		MessageBox(hDlg, L"Configure jumper BOOT0=0 and re-connect your ZUMspot.", L"Information", MB_OK | MB_ICONINFORMATION);
	}
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDCANCEL:
				onCancel(hDlg);
				return TRUE;

			case IDUPLOAD:
				onUpload(hDlg);
				return TRUE;

			case IDBROWSE:
				onBrowse(hDlg);
				return TRUE;

			case IDSTLINK:
				onSTlink(hDlg);
				return TRUE;

			case IDSERIAL:
				onSerial(hDlg);
				return TRUE;
		}
		break;

	case WM_CLOSE:
		onClose(hDlg);
		return TRUE;

	case WM_DESTROY:
		PostQuitMessage(0);
		return TRUE;
	}

	return FALSE;
}

int GetKeyData(HKEY hRootKey, wchar_t *subKey, wchar_t *value, LPBYTE data, DWORD cbData)
{
	HKEY hKey;
	if (RegOpenKeyEx(hRootKey, subKey, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
		return 0;

	if (RegQueryValueEx(hKey, value, NULL, NULL, data, &cbData) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return 0;
	}

	RegCloseKey(hKey);
	return 1;
}

int SetKeyData(HKEY hRootKey, wchar_t *subKey, DWORD dwType, wchar_t *value, LPBYTE data, DWORD cbData)
{
	HKEY hKey;
	if (RegCreateKey(hRootKey, subKey, &hKey) != ERROR_SUCCESS)
		return 0;

	if (RegSetValueEx(hKey, value, 0, dwType, data, cbData) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return 0;
	}

	RegCloseKey(hKey);
	return 1;
}

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE h0, LPTSTR lpCmdLine, int nCmdShow)
{
	HWND hDlg;
	MSG msg;
	BOOL ret;
	wchar_t tmp2_path[MAX_PATH];
	
	InitCommonControls();
	hDlg = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_ZUMSPOTFW), 0, DialogProc, 0);
	ShowWindow(hDlg, nCmdShow);

	wchar_t tmp_path[MAX_PATH];
	
	GetModuleFileName(NULL, tmp_path, MAX_PATH);

	_wsplitpath(tmp_path, curr_path, tmp2_path, NULL, NULL);
	_tcscat(curr_path, tmp2_path);

	if (!GetKeyData(HKEY_CURRENT_USER, L"Software\\ZUMRadio\\ZUMspotFW", L"COMPort", comm_port, 100)) {
		wcscpy(comm_port, L"COM1");
		SetKeyData(HKEY_CURRENT_USER, L"Software\\ZUMRadio\\ZUMspotFW", REG_SZ, L"COMPort", comm_port, 2 * wcslen(comm_port));
	}

	if (!GetKeyData(HKEY_CURRENT_USER, L"Software\\ZUMRadio\\ZUMspotFW", L"FWFile", file_path, MAX_PATH)) {
		wcscpy(file_path, curr_path);
		_tcscat(file_path, L"zumspot_libre_fw.bin");
		SetKeyData(HKEY_CURRENT_USER, L"Software\\ZUMRadio\\ZUMspotFW", REG_SZ, L"FWFile", file_path, 2 * wcslen(file_path));
	}
		
	SetDlgItemText(hDlg, IDFILE, file_path);
	SetDlgItemText(hDlg, IDCOMM, comm_port);

	while ((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
		if (ret == -1)
			return -1;

		if (!IsDialogMessage(hDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}
