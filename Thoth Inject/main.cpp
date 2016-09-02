#include <windows.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <stdlib.h>
#include <conio.h>
#include <psapi.h> //GetModuleFileNameEx

const char* DLLFile = "Thoth.dll";
const char* ClassName = "Notepad";

uint32_t Crc32(const void* Data, size_t Length, uint32_t PrevCrc = 0)
{
    const uint32_t Polynomial = 0xEDB88320;
    uint32_t Crc = ~PrevCrc;
    const uint8_t* Current = reinterpret_cast<const uint8_t*>(Data);
    while( Length-- )
    {
        Crc ^= *Current++;
        for( uint8_t i = 0; i < 8; i++ )
        {
            Crc = (Crc >> 1) ^ (-int32_t(Crc & 1) & Polynomial);
        }
    }

    return ~Crc;
}

bool DLLInjectRemote(uint32_t ProcessID, const std::string& DLLpath)
{
    if( !ProcessID )
    {
        return false;
    }

    if( GetFileAttributes(DLLpath.c_str()) == INVALID_FILE_ATTRIBUTES )
    {
        std::cout << "DLL file: " << DLLpath << " does not exists" << std::endl;
        return false;
    }

    void* ProcLoadLibrary = reinterpret_cast<void*>(GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA"));
    if( !ProcLoadLibrary )
    {
        return false;
    }

    void* Process = OpenProcess(
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_READ,
        false,
        ProcessID);
    if( !Process )
    {
        return false;
    }
    void* Alloc = reinterpret_cast<void*>(VirtualAllocEx(
        Process,
        nullptr,
        DLLpath.length() + 1,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE));
    if( !Alloc )
    {
        return false;
    }
    WriteProcessMemory(Process, Alloc, DLLpath.c_str(), DLLpath.length() + 1, nullptr);

    void* RemoteThread =
        CreateRemoteThread(
            Process,
            nullptr,
            0,
            (LPTHREAD_START_ROUTINE)ProcLoadLibrary,
            Alloc,
            0,
            0);

    bool Result = false;

    // Wait for remote thread to finish
    if( RemoteThread )
    {
        Result = WaitForSingleObject(RemoteThread, 10000) != WAIT_TIMEOUT;
    }

    VirtualFreeEx(Process, Alloc, 0, MEM_RELEASE);
    CloseHandle(Process);
    return Result;
}

int main()
{
    void* hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(437);

    size_t ConsoleWidth = 80;
    CONSOLE_SCREEN_BUFFER_INFO ConsoleBuf;
    if( GetConsoleScreenBufferInfo(hStdout, &ConsoleBuf) )
    {
        ConsoleWidth = ConsoleBuf.dwSize.X;
    }

    SetConsoleTextAttribute(hStdout,
        FOREGROUND_RED |
        FOREGROUND_GREEN |
        FOREGROUND_INTENSITY);
    std::cout << "Thoth Injector Build date (" << __DATE__ << " : " << __TIME__ << ")" << std::endl;
    SetConsoleTextAttribute(hStdout,
        FOREGROUND_BLUE |
        FOREGROUND_GREEN |
        FOREGROUND_INTENSITY);
    std::cout << "\t-Wunkolo (Wunkolo@gmail.com)\n";
    SetConsoleTextAttribute(hStdout,
        FOREGROUND_RED |
        FOREGROUND_BLUE);
    std::cout << std::string(ConsoleWidth - 1, '-') << std::endl;
    SetConsoleTextAttribute(hStdout,
        FOREGROUND_RED |
        FOREGROUND_GREEN |
        FOREGROUND_INTENSITY);

    SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::cout << "Looking for open process..." << std::endl;
    std::string CurrentDirectory(MAX_PATH, 0);
    CurrentDirectory.resize(GetCurrentDirectoryA(MAX_PATH, &CurrentDirectory[0]));

    DWORD ProcessID;
    HWND ProcessWindow;
    ProcessWindow = FindWindowA(ClassName, nullptr);
    if( ProcessWindow == NULL )
    {
        SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "Process not found (Error code: " << std::dec << GetLastError() << ")" << std::endl;
        system("pause");
        return 1;
    }
    SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::cout << "Process found" << std::endl;
    GetWindowThreadProcessId(ProcessWindow, &ProcessID);

    SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::cout << "Injecting "
        << CurrentDirectory << "\\" << DLLFile
        << " into process ID " << ProcessID << ": ";
    if( DLLInjectRemote(ProcessID, CurrentDirectory + "\\" + DLLFile) )
    {
        SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "Success!" << std::endl;
        return 0;
    }
    else
    {
        SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "Failed" << std::endl;
    }
    system("pause");
    return 0;
}