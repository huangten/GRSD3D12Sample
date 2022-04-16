#pragma once
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <atlconv.h>
#include <atlcoll.h>

#define GRS_MAX_OUTPUT_BUFFER_LEN 1024

__inline void GRSInitConsole(LPCTSTR pszTitle)
{
	::AllocConsole(); 
	SetConsoleTitle(pszTitle);
}

__inline void GRSUninitConsole()
{
	::_tsystem(_T("PAUSE")); 
	::FreeConsole();
}

__inline void GRSPrintfW(LPCWSTR pszFormat, ...)
{
	WCHAR pBuffer[GRS_MAX_OUTPUT_BUFFER_LEN] = {};
	size_t szStrLen = 0;
	va_list va;
	va_start(va, pszFormat);
	if (S_OK != ::StringCchVPrintfW(pBuffer, _countof(pBuffer), pszFormat, va))
	{
		va_end(va);
		return;
	}
	va_end(va);
	
	StringCchLengthW(pBuffer, GRS_MAX_OUTPUT_BUFFER_LEN, &szStrLen);

	(szStrLen > 0) ?
		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), pBuffer, (DWORD)szStrLen, nullptr, nullptr)
		: 0;
}

__inline void GRSPrintfA(LPCSTR pszFormat, ...)
{
	CHAR pBuffer[GRS_MAX_OUTPUT_BUFFER_LEN] = {};
	size_t szStrLen = 0;

	va_list va;
	va_start(va, pszFormat);
	if (S_OK != ::StringCchVPrintfA(pBuffer, _countof(pBuffer), pszFormat, va))
	{
		va_end(va);
		return;
	}
	va_end(va);

	StringCchLengthA(pBuffer, GRS_MAX_OUTPUT_BUFFER_LEN, &szStrLen);

	(szStrLen > 0) ?
		WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), pBuffer, (DWORD)szStrLen, nullptr, nullptr)
		: 0;
}

__inline void GRSSetConsoleMax()
{
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	COORD NewSize = GetLargestConsoleWindowSize(hStdout);//��ÿ���̨������꣬�������ַ���Ϊ��λ
	NewSize.X -= 1;
	NewSize.Y -= 1;
	SMALL_RECT DisplayArea = { 0,0,0,0 };
	DisplayArea.Right = NewSize.X;
	DisplayArea.Bottom = NewSize.Y;
	SetConsoleWindowInfo(hStdout, TRUE, &DisplayArea);    //���ÿ���̨��С

	CONSOLE_SCREEN_BUFFER_INFO stCSBufInfo = {};
	GetConsoleScreenBufferInfo(hStdout, &stCSBufInfo);

	stCSBufInfo.dwSize.Y = 9999;
	SetConsoleScreenBufferSize(hStdout, stCSBufInfo.dwSize);

	//����̨�Ѿ���󻯣����ǳ�ʼλ�ò�����Ļ���Ͻǣ�������´���
	HWND hwnd = GetConsoleWindow();
	ShowWindow(hwnd, SW_MAXIMIZE);    //�������
}

__inline void GRSConsolePause()
{
	::_tsystem(_T("PAUSE"));
}
// ���õ�ǰ�����ʾ��λ��
__inline void GRSSetConsoleCursorLocate(int x, int y)
{//��λ��(x,y)
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD loc;
	loc.X = x;
	loc.Y = y;
	SetConsoleCursorPosition(out_handle, loc);
}

// �ƶ���굽ָ����λ��
__inline void GRSMoveConsoleCursor(int x, int y)
//�ƶ����
{
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(out_handle, &info);
	COORD loc = {};
	loc.X = info.dwCursorPosition.X + x;
	loc.Y = info.dwCursorPosition.Y + y;
	SetConsoleCursorPosition(out_handle, loc);
}


__inline int GRSGetConsoleCurrentY()
{
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info = {};
	GetConsoleScreenBufferInfo(out_handle, &info);
	return info.dwCursorPosition.Y;
}

__inline void GRSPrintBlank(int iBlank)
{
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info = {};
	GetConsoleScreenBufferInfo(out_handle, &info);
	for (int i = 0; i < iBlank; i++)
	{
		info.dwCursorPosition.X += 4;
	}
	SetConsoleCursorPosition(out_handle, info.dwCursorPosition);
}
// ���λ�õ�ջ��������ģ��
void GRSPushConsoleCursor();
void GRSPopConsoleCursor();

// ��ĳ��ָ��������к�λ�ð󶨲�����
void GRSSaveConsoleLine(void* p, SHORT sLine);
// ����ָ��ֵ��ȡ����Ӧ���к�
BOOL GRSFindConsoleLine(void* p, SHORT& sLine);