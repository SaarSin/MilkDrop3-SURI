// log.cpp

#include "log.h"

#define LOG_SIZE 512

// SURI: besides OutputDebugString, append every message to MilkDrop3.log next to the exe,
// so startup failures (e.g. audio capture errors) can be diagnosed without a debugger.
static void WriteLogFile(LPCWSTR msg) {
	static wchar_t path[MAX_PATH] = L"";
	if (!path[0]) {
		GetModuleFileNameW(NULL, path, MAX_PATH);
		wchar_t* slash = wcsrchr(path, L'\\');
		if (slash) wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"MilkDrop3.log");
	}
	FILE* f = NULL;
	if (_wfopen_s(&f, path, L"a, ccs=UTF-8") == 0 && f) {
		SYSTEMTIME t;
		GetLocalTime(&t);
		fwprintf(f, L"%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, msg);
		fclose(f);
	}
}

//LPCTSTR UNICODE _UNICODE

//--------------------------------------------------

//LPCSTR
void LOGA(LPCSTR format, ...) {
	char output_buff[LOG_SIZE];

	char err[20] = ""; //"Warn: ";
	int sl = strnlen_s(err, 20);
	strncpy_s(output_buff, err, sl);

	va_list args_list;
	va_start(args_list, format);
	vsprintf_s(&output_buff[sl], sizeof(output_buff) / sizeof(output_buff[0]) - sl, format, args_list);
	va_end(args_list);

	OutputDebugStringA(output_buff);
}

//LPCWSTR
void LOG(LPCWSTR format, ...) {
	wchar_t output_buff[LOG_SIZE];

	wchar_t err[20] = L""; //L"Warn: ";
	int sl = wcsnlen_s(err, 20);
	wcsncpy_s(output_buff, err, sl);

	va_list args_list;
	va_start(args_list, format);
	vswprintf_s(&output_buff[sl], sizeof(output_buff) / sizeof(output_buff[0]) - sl, format, args_list);
	va_end(args_list);

	OutputDebugStringW(output_buff);
	WriteLogFile(output_buff);
}

//--------------------------------------------------

//LPCSTR
void ERRA(LPCSTR format, ...) {
	char output_buff[LOG_SIZE];

	char err[20] = "Error: ";
	int sl = strnlen_s(err, 20);
	strncpy_s(output_buff, err, sl);

	va_list args_list;
	va_start(args_list, format);
	vsprintf_s(&output_buff[sl], sizeof(output_buff) / sizeof(output_buff[0]) - sl, format, args_list);
	va_end(args_list);

	OutputDebugStringA(output_buff);
}

//LPCWSTR
void ERR(LPCWSTR format, ...) {
	wchar_t output_buff[LOG_SIZE];

	wchar_t err[20] = L"Error: ";
	int sl = wcsnlen_s(err, 20);
	wcsncpy_s(output_buff, err, sl);

	va_list args_list;
	va_start(args_list, format);
	vswprintf_s(&output_buff[sl], sizeof(output_buff) / sizeof(output_buff[0]) - sl, format, args_list);
	va_end(args_list);

	OutputDebugStringW(output_buff);
	WriteLogFile(output_buff);
}

//--------------------------------------------------
