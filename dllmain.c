#ifndef WINVER                          // ָ��Ҫ������ƽ̨�� Windows Vista��
#define WINVER 0x0500           // ����ֵ����Ϊ��Ӧ��ֵ���������� Windows �������汾��
#endif

#ifndef _WIN32_WINNT            // ָ��Ҫ������ƽ̨�� Windows Vista��
#define _WIN32_WINNT 0x0501     // ����ֵ����Ϊ��Ӧ��ֵ���������� Windows �������汾��
#endif
#define WIN32_LEAN_AND_MEAN             // �� Windows ͷ���ų�����ʹ�õ�����

// Windows ͷ�ļ�:
#include <windows.h>

void attach_init();

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
        attach_init();
        break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

