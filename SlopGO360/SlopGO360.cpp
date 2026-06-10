#include <xtl.h>

#include <cstdint>
#include <string>

// Get the address of a function from a module by its ordinal
void *ResolveFunction(const std::string &moduleName, uint32_t ordinal)
{
    HMODULE moduleHandle = GetModuleHandle(moduleName.c_str());
    if (moduleHandle == nullptr)
        return nullptr;

    return GetProcAddress(moduleHandle, reinterpret_cast<const char *>(ordinal));
}

// Create a pointer to XNotifyQueueUI in xam.xex
typedef void (*XNOTIFYQUEUEUI)(uint32_t type, uint32_t userIndex, uint64_t areas, const wchar_t *displayText, void *pContextData);
XNOTIFYQUEUEUI XNotifyQueueUI = static_cast<XNOTIFYQUEUEUI>(ResolveFunction("xam.xex", 656));

static uint16_t defaultInstruction = 0;
static uintptr_t patchaddress = 0x816A3158;

// Enum for game title IDs
typedef enum _TitleId
{
    Title_Dashboard = 0xFFFE07D1,
    Title_CSGO = 0x5841125A,
} TitleId;

// Imports from the Xbox libraries
extern "C"
{
    uint32_t XamGetCurrentTitleId();

    uint32_t ExCreateThread(
        HANDLE *pHandle,
        uint32_t stackSize,
        uint32_t *pThreadId,
        void *pApiThreadStartup,
        PTHREAD_START_ROUTINE pStartAddress,
        void *pParameter,
        uint32_t creationFlags
    );
}

typedef void (*CmdExecute)(int unk, const char *cmd, int unk2);
CmdExecute execute = (CmdExecute)0x86A1A330;


bool incsgo = false;
bool g_Running = true;

uint32_t MonitorTitleId(void *pThreadParameter)
{
	uint32_t currenttitleid = 0;
	while (g_Running)
	{
		uint32_t newtitleid = XamGetCurrentTitleId();

		if (newtitleid != currenttitleid)
		{
		currenttitleid = newtitleid;
		switch (newtitleid)
		{
		case Title_Dashboard:
			incsgo = false;
			break;
		case Title_CSGO:
			incsgo = true;
			break;
		}
		}
		if (incsgo)
		{
			

			XINPUT_STATE state;
			XInputGetState(0, &state);
			if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) && (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER))
            {
				XOVERLAPPED overlapped = {}; 
				wchar_t command[256] = L"";
				DWORD dwresult = XShowKeyboardUI(0, VKBD_LATIN_FULL, L"say hi", L"Enter your command", L"", command, 256, &overlapped);

				if (dwresult == ERROR_IO_PENDING){
					while (!XHasOverlappedIoCompleted(&overlapped))
						Sleep(50);
					if (XGetOverlappedResult(&overlapped, nullptr, TRUE) == ERROR_SUCCESS)
					{
						char cmdstring[256];

						wcstombs(cmdstring, command, sizeof(cmdstring));
						strcat(cmdstring, "\n");

						execute(0, cmdstring, 0);
						Sleep(2000);
					}
				}
            }
		}
		Sleep(100);
	}
	return 0;
}

HANDLE g_ThreadHandle = INVALID_HANDLE_VALUE;


BOOL DllMain(HINSTANCE hModule, DWORD reason, void *pReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
		if (defaultInstruction == 0)
			defaultInstruction = *reinterpret_cast<uint16_t *>(patchaddress);
        *reinterpret_cast<uint16_t *>(patchaddress) = 0x4800;
		XNotifyQueueUI(0, 0, XNOTIFY_SYSTEM, L"slop loaded", nullptr);

		ExCreateThread(&g_ThreadHandle, 0, nullptr, nullptr, reinterpret_cast<PTHREAD_START_ROUTINE>(MonitorTitleId), nullptr, 2);
		break;
    case DLL_PROCESS_DETACH:
        if (defaultInstruction != 0)
            *reinterpret_cast<uint16_t *>(patchaddress) = defaultInstruction;

		g_Running = false;
        // Wait for the run thread to finish
        WaitForSingleObject(g_ThreadHandle, INFINITE);
        CloseHandle(g_ThreadHandle);

        break;
    }

    return TRUE;
}