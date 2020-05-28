#include <Windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#define TIMEFORWAIT 10 * 1000 // 10 seconds


namespace fs = boost::filesystem;

LPTHREAD_START_ROUTINE lpLoadLibraryA = NULL;
DWORD delay = 0;
std::string cmdline, exe;
std::vector<std::string> dlls;

inline void error(const char* format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	printf(format, argptr);
	va_end(argptr);

	exit(0);
}

inline void error_usage()
{
	error("Usage:\n"
		  "    -c \"cmd line\" // pass command line to executable\n"
		  "    -d \"delay\"    // set delay in miliseconds beetwen program start and inject\n"
	);
}

inline void error_unknown_argument(const char* arg)
{
	error("unknown argument: %s\n",arg);
}

void HandleCommandLine(int argc, char* argv[])
{
	for (int i = 1; i < argc; i++){
		if (strlen(argv[i]) == 2) {
			if ((argv[i][0] == '-' || argv[i][0] == '/') && i+1 <= argc) {
				switch(tolower(argv[i][1])) {
				case 'c':
					cmdline = argv[i+1];
					break;
				case 'd':
					delay = std::stoul(argv[i+1]);
					break;
				default:
					error_unknown_argument(argv[i]);
				}
				i++;
			} else
				error_unknown_argument(argv[i]);
		} else {
			if (fs::exists(argv[i])) {
				if (fs::is_regular_file(argv[i])) {
					auto extension = fs::extension(argv[i]);
					if (extension == ".dll")
						dlls.push_back(argv[i]);
					else if (extension == ".exe")
						exe = argv[i];
				} else if (fs::is_directory(argv[i])) {
					for (const auto& file : fs::directory_iterator(argv[i])) {
						if (file.path().extension() == ".dll")
							dlls.push_back(file.path().string());
					}
				}
			} else
				error_unknown_argument(argv[i]);
		}
	}
}

bool Inject(HANDLE hProcess, const std::string& dllPath)
{
	bool res = false;
	if (auto lpAddr = VirtualAllocEx(hProcess, NULL, dllPath.size(), MEM_COMMIT, PAGE_READWRITE)) {
		if (WriteProcessMemory(hProcess, lpAddr, dllPath.c_str(), dllPath.size(), NULL)) {
			if (auto hThread = CreateRemoteThread(hProcess, FALSE, 0, lpLoadLibraryA, lpAddr , 0, NULL)) {
				res = WaitForSingleObject(hThread, TIMEFORWAIT) == WAIT_OBJECT_0;
				CloseHandle(hThread);
			}
		}
		VirtualFreeEx(hProcess, lpAddr, dllPath.size(), MEM_FREE);
	}

	return res;
}

int main(int argc, char* argv[])
{
	printf("Visit program homepage https://github.com/FrostAtom/launch-inject\n");

	HandleCommandLine(argc,argv);

	if (exe.empty() || dlls.empty())
		error("exe or dll file(s) isn't assigned.\n");


	STARTUPINFOA sInfo = { sizeof(STARTUPINFOA) };
	PROCESS_INFORMATION pInfo;

	if (!CreateProcessA(exe.c_str(),(char*)cmdline.data(),NULL,NULL,FALSE,0,NULL,NULL,&sInfo,&pInfo)) {
		DWORD dwExitCode;
		GetExitCodeProcess(pInfo.hProcess, &dwExitCode);
		error("CreateProcess exit code: %lu\n",dwExitCode);
	}

	HMODULE hKernel = GetModuleHandleA("kernel32");
	lpLoadLibraryA = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel, "LoadLibraryA");
	
	if (delay)
		Sleep(delay);

	for (const auto& dll : dlls){
		if (!Inject(pInfo.hProcess, dll)) 
			error("can't inject %s\n", dll.c_str());
	}

	return 0;
}