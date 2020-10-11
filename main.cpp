#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>

namespace po = boost::program_options;
namespace fs = boost::filesystem;
typedef LONG (NTAPI *NtSuspendProcess)(HANDLE ProcessHandle);
typedef LONG (NTAPI *NtResumeProcess)(HANDLE ProcessHandle);


void OpenConsole()
{
	static bool isConsoleShown = false;
	if (!isConsoleShown) {
		AttachConsole(ATTACH_PARENT_PROCESS);
		AllocConsole();
		freopen("CONIN$","r",stdin);
		freopen("CONOUT$","w",stdout);
		freopen("CONOUT$","w",stderr);

		isConsoleShown = true;
	}
}

void Notify(const char* message,...)
{
	OpenConsole();

	va_list args;
	va_start(args,message);	
	vprintf(message,args);
	va_end(args);

	system("pause");
}

void ErrorOccured(const char* message,...)
{
	OpenConsole();

	std::cout << "Error has been occured!" << std::endl;
	std::cout << "Last error code: " << GetLastError() << std::endl;
	va_list args;
	va_start(args,message);	
	vprintf(message,args);
	va_end(args);

	system("pause");
}

bool InjectLibrary(HANDLE hProcess, const char* filePath,DWORD dwMilliseconds = 10 * 1e3)
{
	static FARPROC pLoadLibraryA = NULL;
	if (!pLoadLibraryA) {
		HMODULE hModule = GetModuleHandleA("kernel32");
		if (hModule == NULL) return false;

		pLoadLibraryA = GetProcAddress(hModule,"LoadLibraryA");
		if (!pLoadLibraryA) return false;
	}

	bool result = false;
	size_t szBuffer = strlen(filePath) + 1;
	if (void* lpBuffer = VirtualAllocEx(hProcess,NULL,szBuffer,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE)) {
		SIZE_T bytesWritten = 0;
		if (WriteProcessMemory(hProcess,lpBuffer,filePath,szBuffer,&bytesWritten) && bytesWritten == szBuffer) {
			if (HANDLE hThread = CreateRemoteThread(hProcess,NULL,0,(LPTHREAD_START_ROUTINE)pLoadLibraryA,lpBuffer,0,NULL))
				result = (WaitForSingleObject(hThread,dwMilliseconds) == WAIT_OBJECT_0);
		}
		
		VirtualFreeEx(hProcess,lpBuffer,szBuffer,MEM_RELEASE);
	}

	return result;
}

void InjectLibraryList(HANDLE hProcess, const std::vector<std::string>& dllList)
{
	for (const auto& pathDll : dllList) {
		if (!fs::is_regular_file(pathDll) || fs::extension(pathDll).compare(".dll") != 0) {
			ErrorOccured("Library file invalid! File name: %s\n",pathDll.c_str());
			continue;
		}

		std::string absolutePathDll = boost::filesystem::absolute(pathDll).string();
		if (!InjectLibrary(hProcess,absolutePathDll.c_str())) {
			ErrorOccured("InjectLibrary() fail! File path: %s\n",absolutePathDll.c_str());
			continue;
		}
	}
}

bool ExecFile(const std::string& exe, const std::string& cmdline, PROCESS_INFORMATION& pInfo)
{
	memset(&pInfo,NULL,sizeof(pInfo));

	STARTUPINFOA sInfo;
	memset(&sInfo,NULL,sizeof(sInfo));
	sInfo.cb = sizeof(sInfo);

	std::string formatedCmdLine;
	formatedCmdLine.append("\"").append(exe).append("\" ").append(cmdline);

	std::string pathParent = fs::absolute(exe).parent_path().string();
	return NULL != CreateProcessA(exe.c_str(),(char*)formatedCmdLine.c_str(),NULL,NULL,FALSE,CREATE_SUSPENDED,NULL,pathParent.c_str(),&sInfo,&pInfo);
}

std::vector<DWORD> GetPidListByName(const std::string& processName)
{
	std::vector<DWORD> result;
	PROCESSENTRY32 processEntry;
	memset(&processEntry,NULL,sizeof(processEntry));
	processEntry.dwSize = sizeof(processEntry);

	if (HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,NULL); hSnapshot != INVALID_HANDLE_VALUE) {
		if (Process32First(hSnapshot,&processEntry)) {
			do {
				if (strcmp(processEntry.szExeFile,processName.c_str()) == 0)
					result.push_back(processEntry.th32ProcessID);
			} while(Process32Next(hSnapshot,&processEntry));
		}

		CloseHandle(hSnapshot);
	}

	return result;
}

int main(int argc, char** argv)
{
	std::vector<std::string> dllList;
	std::string exe,cmdline;
	bool isInLauncherMode;
	{
		po::variables_map vm;
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h","show help")
			("launchermode,l","mode with start exe file")
			("pause,p","pause beetwen start exe and injecting (used in launchermode only)")
			("exe,e",po::value<std::string>(&exe),"launchermode - path to exe file, overtwice process name [REQUIRED]")
			("dll,d",po::value<std::vector<std::string>>(&dllList)->multitoken(),"path to dll library (can be used one more times)")
			("cmdline,c",po::value<std::string>(&cmdline),"cmdline for pass to executable");

		po::store(po::parse_command_line(argc,argv,desc),vm);
		po::notify(vm);

		if (vm.count("help") || !vm.count("exe")) {
			OpenConsole();
			std::cout << desc << std::endl;
			system("pause");
			return 0;
		}

		isInLauncherMode = vm.count("launchermode") != 0;
	}

	if (isInLauncherMode) {
		PROCESS_INFORMATION pInfo;
		if (!ExecFile(exe,cmdline,pInfo)) {
			ErrorOccured("CreateProcess() fail!");
			return 0;
		}
		
		InjectLibraryList(pInfo.hProcess,dllList);
		ResumeThread(pInfo.hThread);
		CloseHandle(pInfo.hThread);
		CloseHandle(pInfo.hProcess);
	}
	else {
		HMODULE hNtDll = GetModuleHandleA("ntdll");
		auto pNtSuspendProcess = (NtSuspendProcess)GetProcAddress(hNtDll,"NtSuspendProcess");
		auto pNtResumeProcess = (NtResumeProcess)GetProcAddress(hNtDll,"NtResumeProcess");

		std::vector<DWORD> pIdList = GetPidListByName(exe);
		while (pIdList.empty()) {
			Notify("Process not found! First run \"%s\"\n",exe.c_str());
			pIdList = GetPidListByName(exe);
		}

		for (const auto& pId : pIdList) {
			if (HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS,FALSE,pId); hProcess != INVALID_HANDLE_VALUE) {
				pNtSuspendProcess(hProcess);
				InjectLibraryList(hProcess,dllList);
				pNtResumeProcess(hProcess);
			}
		}
	}

	return 0;
}