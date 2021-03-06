// MemMap.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "..\KExploreHelper\KExploreHelper.h"
#include "..\KExplore\KExploreClient.h"

int Error(const char* message, int error = ::GetLastError()) {
	printf("%s (error=%d)\n", message, error);
	return 1;
}

int Usage() {
	printf("Usage: memmap <pid>\n");
	return 0;
}

const char* ProtectionToString(DWORD protect) {
	static char text[256];
	bool guard = (protect & PAGE_GUARD) == PAGE_GUARD;
	protect &= ~PAGE_GUARD;

	const char* protection = "Unknown";
	switch (protect) {
		case PAGE_EXECUTE: protection = "Execute"; break;
		case PAGE_EXECUTE_READ: protection = "Execute/Read";
		case PAGE_READONLY: protection = "Read Only";
		case PAGE_NOACCESS: protection = "No Access";
		case PAGE_READWRITE: protection = "Read/Write";
		case PAGE_WRITECOPY: protection = "Write Copy";
		case PAGE_EXECUTE_READWRITE: protection = "Execute/Read/Write";
		case PAGE_EXECUTE_WRITECOPY: protection = "Execute/Write Copy";
	}
	strcpy_s(text, protection);
	if (guard)
		strcat_s(text, "/Guard");

	return text;
}

const char* StateToString(DWORD state) {
	switch (state) {
		case MEM_COMMIT: return "Committed";
		case MEM_RESERVE: return "Reserved";
		case MEM_FREE: return "Free";
	}
	return "";
}

const char* MemoryTypeToString(DWORD type) {

	switch (type) {
		case MEM_IMAGE: return "Image";
		case MEM_MAPPED: return "Mapped";
		case MEM_PRIVATE: return "Private";
	}
	return "";
}

const char* GetExtraData(HANDLE hProcess, MEMORY_BASIC_INFORMATION& mbi) {
	static char text[512];
	if (mbi.Type == MEM_IMAGE) {
		if(::GetMappedFileNameA(hProcess, mbi.BaseAddress, text, sizeof(text)) > 0)
			return text;
	}
	return "";
}

void ShowMemoryMap(HANDLE hProcess) {
	long long address = 0;
	printf("  %-16s %-16s %15s %16s %26s %10s\n", "Address", "Size (bytes)", "Protection", "State", "Allocation Protection", "Type");
	printf("-----------------------------------------------------------------------------------------------------------\n");

	do {
		MEMORY_BASIC_INFORMATION mbi = { 0 };
		auto size = ::VirtualQueryEx(hProcess, reinterpret_cast<PVOID>(address), &mbi, sizeof(mbi));
		if (size == 0)
			break;

		printf("%16p %16llX %-27s %-10s %-27s %-8s %s\n", mbi.BaseAddress, mbi.RegionSize, 
			mbi.State == MEM_COMMIT ? ProtectionToString(mbi.Protect) : "",
			StateToString(mbi.State), 
			mbi.State == MEM_FREE ? "" : ProtectionToString(mbi.AllocationProtect), 
			mbi.State == MEM_FREE ? "" : MemoryTypeToString(mbi.Type), GetExtraData(hProcess, mbi));
		address += mbi.RegionSize;
	} while (true);
}

int main(int argc, const char* argv[]) {
	printf("MemMap v0.2 - process memory map (C)2017 Pavel Yosifovich\n");

	if (argc < 2)
		return Usage();

	int pid = atoi(argv[1]);

	auto hDevice = KExploreHelper::OpenDriverHandle();
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Failed to open driver handle. Starting driver...\n");
		if (!KExploreHelper::LoadDriver(L"Kexplore")) {
			printf("Failed to start driver. Installing...\n");
			WCHAR path[MAX_PATH];
			// assume driver is in EXE's path
			::GetModuleFileName(nullptr, path, MAX_PATH);
			*(wcsrchr(path, L'\\') + 1) = 0;
			wcscat_s(path, L"Kexplore.sys");
			if (!KExploreHelper::InstallDriver(L"KExplore", path)) {
				if (::GetLastError() == ERROR_ACCESS_DENIED) {
					printf("Failed to install driver - access denied. Try running with evelated privileges\n");
					return 1;
				}
				return Error("Failed to install driver");
			}
			if (!KExploreHelper::LoadDriver(L"Kexplore"))
				return Error("Failed to start driver. Try re-running with elevated privileges");
		}
		hDevice = KExploreHelper::OpenDriverHandle();
	}
	if (!hDevice)
		return Error("Failed to open driver handle");

	HANDLE hProcess;
	OpenProcessData data;
	data.ProcessId = pid;
	data.AccessMask = PROCESS_ALL_ACCESS;
	DWORD returned;
	if (!::DeviceIoControl(hDevice, KEXPLORE_IOCTL_OPEN_PROCESS, &data, sizeof(data), &hProcess, sizeof(hProcess), &returned, nullptr))
		return Error("Failed to open process object:");

	printf("\n");

	ShowMemoryMap(hProcess);
	::CloseHandle(hProcess);

	::CloseHandle(hDevice);

	return 0;
}

