#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <iostream>
#include <string>

static DWORD findPid(const wchar_t* exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    DWORD result = 0;
    if (Process32FirstW(snap, &pe)) {
        do { if (_wcsicmp(pe.szExeFile, exe) == 0) { result = pe.th32ProcessID; break; } }
        while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return result;
}

int wmain() {
    const wchar_t* target = L"Control_DX12.exe";
    wchar_t pathBuf[32768]{};
    if (!GetModuleFileNameW(nullptr, pathBuf, 32768)) { std::wcerr << L"Cannot resolve injector location.\n"; return 1; }
    const auto dll = std::filesystem::absolute(std::filesystem::path(pathBuf).parent_path() / L"ControlToolkit.dll");
    if (!std::filesystem::exists(dll)) { std::wcerr << L"Missing DLL next to injector: " << dll.wstring() << L"\n"; return 2; }
    DWORD pid = findPid(target);
    if (!pid) { std::wcerr << L"Control_DX12.exe is not running. Launch CONTROL DX12 first.\n"; return 3; }
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!process) { std::wcerr << L"OpenProcess failed: " << GetLastError() << L" (try same elevation as CONTROL).\n"; return 4; }
    const std::wstring full = dll.wstring();
    const SIZE_T bytes = (full.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { std::wcerr << L"VirtualAllocEx failed: " << GetLastError() << L"\n"; CloseHandle(process); return 5; }
    bool ok = WriteProcessMemory(process, remote, full.c_str(), bytes, nullptr) != 0;
    if (!ok) std::wcerr << L"WriteProcessMemory failed: " << GetLastError() << L"\n";
    HANDLE thread = nullptr;
    if (ok) {
        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        FARPROC load = kernel ? GetProcAddress(kernel, "LoadLibraryW") : nullptr;
        if (load) thread = CreateRemoteThread(process, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(load), remote, 0, nullptr);
        if (!thread) std::wcerr << L"CreateRemoteThread failed: " << GetLastError() << L"\n";
    }
    DWORD result = 0;
    if (thread) {
        DWORD wait = WaitForSingleObject(thread, 10000);
        if (wait == WAIT_OBJECT_0) {
            DWORD exitCode = 0;
            if (GetExitCodeThread(thread, &exitCode) && exitCode) { std::wcout << L"DLL load call completed for PID " << pid << L". Check overlay in game.\n"; result = 0; }
            else { std::wcerr << L"LoadLibraryW failed inside the game.\n"; result = 7; }
        } else { std::wcerr << L"Injection timed out; restart CONTROL before retrying.\n"; result = 8; }
        CloseHandle(thread);
        if (wait == WAIT_OBJECT_0) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    } else { result = 6; VirtualFreeEx(process, remote, 0, MEM_RELEASE); }
    CloseHandle(process);
    return static_cast<int>(result);
}
