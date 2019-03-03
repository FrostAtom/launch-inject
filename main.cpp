#include <windows.h>
#include <sys/stat.h>
#include <string>

inline auto IsFileExists(char* filename)
{
    struct stat buffer;   
    return (stat (filename, &buffer) == 0);
}

inline auto ArgvToCmdLine(char* app, int argc, char* argv[])
{
    std::string buffer;
    (buffer = app) += ' ';
    for (int i = 0; i < argc; i++)
        (buffer += argv[i]) += ' ';

    return (char*)buffer.c_str();
}

inline auto PathToName(std::string path)
{
    auto last = path.find_last_of("\\/");
    if (last != std::string::npos)
        path.erase(0, last + 1);
    
    return path;
}

inline auto error(const char* text)
{
    MessageBoxA(NULL, text, NULL, MB_OK);
    exit(0);
}

inline auto InjectDll(HANDLE hProc, std::string dllname)
{
    auto lpLoadLibraryA = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32"), "LoadLibraryA");

    auto lpAddr = VirtualAllocEx(hProc, NULL, dllname.size(), MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProc, lpAddr, dllname.c_str(), dllname.size(), NULL);
    auto hThread = CreateRemoteThread(hProc, NULL, 0, lpLoadLibraryA, lpAddr, 0, NULL);
    WaitForSingleObject(hThread,10000);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, lpAddr, dllname.size(), MEM_FREE);
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::string str;
        ((str = "Usage: ") += PathToName(*argv)) += " program.exe library.dll [args]";
        error(str.c_str());
    }

    for (int i = 1; i < 3; i++)
    {
        if (!IsFileExists(argv[i]))
        {
            std::string str;
            (str = "Unexist file: ") += argv[i];
            error(str.c_str());
        }
    }

    STARTUPINFOA sInfo = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pInfo;
    if (!CreateProcessA(argv[1],(argc > 3) ? ArgvToCmdLine(argv[1], argc - 3, &argv[3]) : NULL, NULL, NULL, FALSE, 0, NULL, NULL, &sInfo, &pInfo))
        error("CreateProcess fail");

    InjectDll(pInfo.hProcess, argv[2]);

    WaitForSingleObject(pInfo.hProcess, INFINITE);
    CloseHandle(pInfo.hProcess);
    CloseHandle(pInfo.hThread);
    
    return 0;
}