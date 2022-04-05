#include <Windows.h>
#include <stdio.h>
#include "..\PriorityBooster\PriorityBoosterCommon.hpp"

int Error(const char* message) {
	printf("%s (err=%d)\n", message, GetLastError());
	return 1;
}

int main(int argc, const char* argv[]) {
	if (argc < 3) {
		printf("Usage: Booster <threadid> <priority>\n");
		return 0;
	}

	// Open a handle to our driver
	HANDLE hDevice = CreateFile(L"\\\\.\\PriorityBooster", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE)
		return Error("Failed to open device");

	// Structure that contains all data to send to our driver
	ThreadData data;
	data.ThreadId = atoi(argv[1]);
	data.Priority = atoi(argv[2]);

	// Send data to our driver
	DWORD returned;
	BOOL success = DeviceIoControl(hDevice, IOCTL_PRIORITY_BOOSTER_SET_PRIORITY, &data, sizeof(data), nullptr, 0, &returned, nullptr);
	if (success)
		printf("Priority change succeeded!\n");
	else
		Error("Priority change failed\n");

	// Close the previously created handle
	CloseHandle(hDevice);
}