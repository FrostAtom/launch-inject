#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <windows.h>
#include <string>
#include <iostream>

namespace po = boost::program_options;
namespace fs = boost::filesystem;


void ErrorOccured(const char* message = NULL,const char* additionalInfo = NULL)
{
	AttachConsole(ATTACH_PARENT_PROCESS);
	AllocConsole();

	std::cout << "Error has been occured!" << std::endl;
	if (message) std::cout << "Message: " << message << std::endl;
	if (additionalInfo) std::cout << "Additional info: " << additionalInfo << std::endl;
	std::cout << "Last error code: " << GetLastError() << std::endl;
		
	system("pause");
	FreeConsole();
	exit(EXIT_FAILURE);
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
	if (void* lpBuffer = VirtualAllocEx(hProcess,NULL,szBuffer,MEM_COMMIT,PAGE_READWRITE)) {
		SIZE_T bytesWritten = 0;
		if (WriteProcessMemory(hProcess,lpBuffer,filePath,szBuffer,&bytesWritten)) {
			if (bytesWritten == szBuffer) {
				if (HANDLE hThread = CreateRemoteThread(hProcess,NULL,0,(LPTHREAD_START_ROUTINE)pLoadLibraryA,lpBuffer,0,NULL))
					result = (WaitForSingleObject(hThread,dwMilliseconds) == WAIT_OBJECT_0);
			}
		}
		
		VirtualFreeEx(hProcess,lpBuffer,szBuffer,MEM_DECOMMIT);
	}

	return result;
}

int main(int argc, char** argv)
{
	std::vector<std::string> pathDllList;
	std::string pathExe,cmdline;
	{
		po::variables_map vm;
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h","show help")
			("exe,e",po::value<std::string>(&pathExe),"path to exe file [REQUIRED]")
			("dll,d",po::value<std::vector<std::string>>(&pathDllList)->multitoken(),"path to dll library (can be used one more times)")
			("cmdline,c",po::value<std::string>(&cmdline),"cmdline for pass to executable");

		po::store(po::parse_command_line(argc,argv,desc),vm);
		po::notify(vm);

		if (vm.count("help") || !vm.count("exe")) {
			std::cout << desc << std::endl;
			system("pause");
			return 0;
		}
	}

	PROCESS_INFORMATION pInfo;
	memset(&pInfo,NULL,sizeof(pInfo));

	STARTUPINFOA sInfo;
	memset(&sInfo,NULL,sizeof(sInfo));
	sInfo.cb = sizeof(sInfo);

	std::string formatedCmdLine;
	formatedCmdLine.append("\"").append(pathExe).append("\" ").append(cmdline);

	std::string pathParent = fs::absolute(pathExe).parent_path().string();
	if (!CreateProcessA(pathExe.c_str(),(char*)formatedCmdLine.c_str(),NULL,NULL,FALSE,CREATE_SUSPENDED,NULL,pathParent.c_str(),&sInfo,&pInfo))
		ErrorOccured("CreateProcess() fail!");

	for (const auto& pathDll : pathDllList) {
		if (!fs::is_regular_file(pathDll) || fs::extension(pathDll).compare(".dll") != 0)
			ErrorOccured("Library file invalid!",pathDll.c_str());

		std::string absolutePathDll = boost::filesystem::absolute(pathDll).string();
		if (!InjectLibrary(pInfo.hProcess,absolutePathDll.c_str()))
			ErrorOccured("InjectLibrary() fail!",pathDll.c_str());
	}

	ResumeThread(pInfo.hThread);
	CloseHandle(pInfo.hThread);
	CloseHandle(pInfo.hProcess);

	return 0;
}