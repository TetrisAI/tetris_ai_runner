#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

void attach_init();

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
USING_NS_CC;

typedef LONG(__stdcall * NT_SET_TIMER_RESOLUTION)
(
    IN ULONG DesiredTime,
    IN BOOLEAN SetResolution,
    OUT PULONG ActualTime
    );

typedef LONG(__stdcall * NT_QUERY_TIMER_RESOLUTION)
(
    OUT PULONG MaximumTime,
    OUT PULONG MinimumTime,
    OUT PULONG CurrentTime
    );

void enable_high_resolution()
{
    HMODULE hNtDll = LoadLibrary(TEXT("NtDll.dll"));
    if(hNtDll)
    {
        NT_QUERY_TIMER_RESOLUTION _NtQueryTimerResolution = (NT_QUERY_TIMER_RESOLUTION)GetProcAddress(hNtDll, "NtQueryTimerResolution");
        NT_SET_TIMER_RESOLUTION _NtSetTimerResolution = (NT_SET_TIMER_RESOLUTION)GetProcAddress(hNtDll, "NtSetTimerResolution");
        if(_NtQueryTimerResolution && _NtSetTimerResolution)
        {
            ULONG MaximumTime = 0;
            ULONG MinimumTime = 0;
            ULONG CurrentTime = 0;
            if(!_NtQueryTimerResolution(&MaximumTime, &MinimumTime, &CurrentTime))
            {
                ULONG ActualTime = 0;
                if(!_NtSetTimerResolution(MinimumTime, TRUE, &ActualTime))
                {
                    FreeLibrary(hNtDll);
                    return;
                }
            }
        }
        FreeLibrary(hNtDll);
    }
    timeBeginPeriod(1);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
        enable_high_resolution();
        attach_init();
        break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

