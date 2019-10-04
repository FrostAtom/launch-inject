#include <Windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#define TIMEFORWAIT 10 * 1000 // 10 seconds


namespace fs = std::filesystem;

std::string cmdLine, exePath;
std::vector<std::string> dllPaths;

void error(const char* format, ...)
{
	printf("fail: ");
	va_list argptr;
	va_start(argptr, format);
	printf(format, argptr);
	va_end(argptr);

	exit(0);
}

void error_usage()
{
	error("Usage:"
		"\t-c \"cmd line\" -- pass command line to executable\n"
		"\t-d \"path\" -- insert dll to injecting queue\n"
		"\t-e \"path\" -- assing executable file\n"
	);
}

void ParseCommandLineOptions(int argc, char* argv[])
{
	for (int i = 1; i < argc; i += 2){
		if (argv[i+1][0] && (argv[i][0] == '-' || argv[i][0] == '/')){
			switch(tolower(argv[i][1])){
			case 'c':
				cmdLine.append(argv[i+1]);
				continue;
			case 'd':
				dllPaths.push_back(argv[i+1]);
				continue;
			case 'e':
				exePath.append(argv[i+1]);
				continue;
			}
		}

		error_usage();
	}
}

bool Inject(HANDLE hProcess, const std::string& dllPath, FARPROC procLoadLibraryA)
{
	bool res = false;
	if (auto lpAddr = VirtualAllocEx(hProcess, NULL, dllPath.size(), MEM_COMMIT, PAGE_READWRITE)) {
		if (WriteProcessMemory(hProcess, lpAddr, dllPath.c_str(), dllPath.size(), NULL)) {
			if (auto hThread = CreateRemoteThread(hProcess, FALSE, 0, (LPTHREAD_START_ROUTINE)procLoadLibraryA, lpAddr , 0, NULL)) {
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

	ParseCommandLineOptions(argc,argv);

	if (exePath.empty())
		error("path of executable file is not assigned.\n");

	if (!fs::exists(exePath))
		error("executable file is not exists.\n");

	for (const auto& val : dllPaths){
		if (!fs::exists(val))
			error("dll file %s is not exists!\n",val.c_str());
	}


	STARTUPINFOA sInfo = { sizeof(STARTUPINFOA) };
	PROCESS_INFORMATION pInfo;

	if (!CreateProcessA(exePath.c_str(), cmdLine.data(), NULL, NULL, FALSE, 0, NULL, NULL, &sInfo, &pInfo)){
		DWORD* exitCode = NULL;
		GetExitCodeProcess(pInfo.hProcess, exitCode);
		error("process creating error code: %lu\n", exitCode);
	}

	HMODULE hKernel = GetModuleHandleA("kernel32");
	FARPROC procLoadLibraryA = GetProcAddress(hKernel, "LoadLibraryA");

	for (const auto& val : dllPaths){
		if (!Inject(pInfo.hProcess, val, procLoadLibraryA))
			error("can't inject %s\n", val.c_str());
	}

	return 0;
}