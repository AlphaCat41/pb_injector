#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <iostream>
#include <string> 

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

	std::wcout << L"Target process name: ";
	std::getline (std::wcin, targetProcessName);

	DWORD pid = GetProcessIdByName (targetProcessName);
	if (!pid) {
		std::cerr << "[-] Cannot find the process" << std::endl;
		system ("pause");
		return 1;
	}
	std::cout << "DLL path (ex: C:\\hack\\wallhack.dll): ";
	std::getline (std::cin, dllPath);

	if (!InjectDLL (pid, dllPath)) {
		std::cerr << "[-] Inject DLL failed" << std::endl;
		system ("pause");
		return 1;
	}
	system ("pause"); // Pause to see the output before closing
	return 0;
}
