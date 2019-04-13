#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

std::string prog, args;
std::vector<std::string> dlls;

inline auto Error(const char* format, ...)
{
	AllocConsole();
	freopen("CONOUT$", "w", stdout);

	va_list argptr;
	va_start(argptr, format);
	printf(format, argptr);
	va_end(argptr);
	printf("\r\n");

	getchar();
	exit(0);
}

inline auto InjectDll(LPTHREAD_START_ROUTINE loadlibrarya, HANDLE hProcess, const std::string& dllname)
{
	bool res = false;
	if (auto lpAddr = VirtualAllocEx(hProcess, NULL, dllname.size(), MEM_COMMIT, PAGE_READWRITE)) {
		if (WriteProcessMemory(hProcess, lpAddr, dllname.c_str(), dllname.size(), NULL)) {
			if (auto hThread = CreateRemoteThread(hProcess, FALSE, 0, loadlibrarya, lpAddr, 0, NULL)) {
				res = WaitForSingleObject(hThread, 10 * 1000) == WAIT_OBJECT_0;
				CloseHandle(hThread);
			}
		}
		VirtualFreeEx(hProcess, lpAddr, dllname.size(), MEM_FREE);
	}
	return res;
}

inline auto strsplit(const std::string& str, char delim)
{
	std::stringstream strstream(str);
	std::vector<std::string> res;
	for (std::string item; std::getline(strstream, item, delim);)
		res.push_back(item);
	
	return res;
}

inline auto PutToContainer(const std::string& strpath)
{
	if (std::filesystem::exists(strpath)) {
		auto abspath = std::filesystem::absolute(strpath);
		if (std::filesystem::is_directory(abspath)) {
			for (auto& file : std::filesystem::directory_iterator(abspath)) {
				auto filepath = file.path();
				if (filepath.extension().compare(".dll") == 0)
					dlls.push_back(filepath.string());
			}
		} else if(std::filesystem::is_regular_file(abspath)) {
			if (abspath.extension().compare(".dll") == 0)
				dlls.push_back(abspath.string());
			else if ((abspath.extension().compare(".exe") == 0)) {
				if (prog.empty())
					prog = abspath.string();
				else
					Error("%s\r\nProgram alredy assigned!", strpath);
			}
			else
				Error("%s\r\nUnknown file extension. Expected \".exe\" or \".dll\"", abspath.c_str());
		}else
			Error("%s\r\nUnknown file type.", abspath.c_str());
	}
	else {
		if (std::filesystem::exists(strpath + ".dll"))
			dlls.push_back(strpath + ".dll");
		else if(std::filesystem::exists(strpath + ".exe")){
			if (prog.empty())
				prog = std::filesystem::absolute(strpath + ".exe").string();
			else
				Error("%s\r\nProgram alredy assigned!", strpath);
		}
		else
			args.append(strpath).append(" ");
	}
}

int main(int argc, char* argv[])
{
	auto launch = std::filesystem::path(argv[0]);

	auto vector = strsplit(launch.stem().string(), '_');
	if (vector.size() <= 1) {
		vector.clear();
		for (int8_t i = 1; i < argc; i++)
			vector.push_back(argv[i]);
	}
	else {
		for (int8_t i = 1; i < argc; i++)
			args.append(argv[i]).append(" ");
	}


	for (auto& val : vector)
		PutToContainer(val);

	if (prog.empty())
		Error("Program is not assigned");

	args.insert(0, std::filesystem::absolute(prog).string().append(" "));

	STARTUPINFOA sInfo = { sizeof(STARTUPINFOA) };
	PROCESS_INFORMATION pInfo;
	
	if (!CreateProcessA(prog.c_str(), args.data(), NULL, NULL, FALSE, 0, NULL, NULL, &sInfo, &pInfo))
		Error("Program launch fail");

	auto loadlibrarya = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32"), "LoadLibraryA");
	for (auto& dll : dlls) {
		if (!InjectDll(loadlibrarya, pInfo.hProcess, dll))
			Error("%s\r\nInjection fail.", dll.c_str());
	}

	WaitForSingleObject(pInfo.hProcess, INFINITE);
	CloseHandle(pInfo.hProcess);
	CloseHandle(pInfo.hThread);

	return 0;
}
