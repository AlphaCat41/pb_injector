#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <iostream>

DWORD GetProcessIdByName (const std::wstring& processName) {
	DWORD pid = 0;
	HANDLE snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);

	if (snapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32W entry;
		entry.dwSize = sizeof (entry);

		if (Process32FirstW (snapshot, &entry)) {
			do {
				if (!_wcsicmp (entry.szExeFile, processName.c_str ())) {
					pid = entry.th32ProcessID;
					break;
				}
			} while (Process32NextW (snapshot, &entry));
		}
		CloseHandle (snapshot);
	}

	return pid;
}

bool InjectDLL (DWORD processId, const std::string& dllPath) {
	HANDLE hProcess = OpenProcess (PROCESS_ALL_ACCESS, FALSE, processId);
	if (!hProcess) {
		std::cerr << "[-] Failed to open process: " << GetLastError () << std::endl;
		return false;
	}

	// Allocate memory in the target process
	void* pRemoteMemory = VirtualAllocEx (hProcess, nullptr, dllPath.size () + 1,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!pRemoteMemory) {
		std::cerr << "[-] Failed to allocate memory in target process." << std::endl;
		CloseHandle (hProcess);
		return false;
	}

	// Write DLL path to the target process memory
	WriteProcessMemory (hProcess, pRemoteMemory, dllPath.c_str (), dllPath.size () + 1, nullptr);

	// Get LoadLibraryA address	
	void* pLoadLibraryA = nullptr;
	HMODULE hKernel32 = GetModuleHandleA ("kernel32.dll");
	if (hKernel32) {
		pLoadLibraryA = GetProcAddress (hKernel32, "LoadLibraryA");
	}
	else {
		std::cerr << "[-] Failed to get handle for kernel32.dll." << std::endl;
		return false; // Or handle the error appropriately
	}

	// Create remote thread to load the DLL
	HANDLE hThread = CreateRemoteThread (hProcess, nullptr, 0,
		(LPTHREAD_START_ROUTINE)pLoadLibraryA,
		pRemoteMemory, 0, nullptr);
	if (!hThread) {
		std::cerr << "[-] Failed to create remote thread." << std::endl;
		VirtualFreeEx (hProcess, pRemoteMemory, 0, MEM_RELEASE);
		CloseHandle (hProcess);
		return false;
	}

	// Wait for the thread to finish
	WaitForSingleObject (hThread, INFINITE);

	// Cleanup
	VirtualFreeEx (hProcess, pRemoteMemory, 0, MEM_RELEASE);
	CloseHandle (hThread);
	CloseHandle (hProcess);

	std::cout << "[+] DLL successfully injected." << std::endl;
	return true;
}

int main () {
	std::wstring targetProcessName;
	std::string dllPath;

	/*std::wcout << L"ชื่อโปรเซส (เช่น: PointBlank.exe): ";
	std::getline (std::wcin, targetProcessName);

	std::cout << "เส้นทาง DLL (เช่น: C:\\hack\\wallhack.dll): ";
	std::getline (std::cin, dllPath);*/
	targetProcessName = L"PointBlank.exe";
	dllPath = "C:\\Users\\DELL\\Desktop\\cpp_repos\\pb_hack\\Debug\\pb_hack.dll";

	DWORD pid = GetProcessIdByName (targetProcessName);
	if (!pid) {
		std::cerr << "[-] ไม่พบโปรเซสที่ระบุ" << std::endl;
		return 1;
	}

	if (!InjectDLL (pid, dllPath)) {
		std::cerr << "[-] Inject DLL ไม่สำเร็จ" << std::endl;
		return 1;
	}

	return 0;
}
