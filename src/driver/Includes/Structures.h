#define COBJMACROS
#include "../../util/common.h"
#include <ctime>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <winternl.h>
#include <cstdint>
#include <DbgHelp.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <functional>
#include <cassert>
#include <corecrt_math.h>
#include <limits>
#include <numbers>
#include <shared_mutex>
#include <unordered_set>
#include <shellapi.h>
#include <emmintrin.h>
#include <Xinput.h>
#include <ntstatus.h>
#include <winioctl.h>

#define FILE_DEVICE_UNKNOWN             0x00000022

#ifndef FILE_SPECIAL_ACCESS
#define FILE_SPECIAL_ACCESS ( FILE_ANY_ACCESS )
#endif

// Matches C:\Users\Zeeni\Downloads\driver\driver\handler\handler.cpp
#define IOCTL_ReadMemory        CTL_CODE( FILE_DEVICE_UNKNOWN, 0x12bac, METHOD_BUFFERED, FILE_SPECIAL_ACCESS )
#define IOCTL_WriteMemory       CTL_CODE( FILE_DEVICE_UNKNOWN, 0xb2aa4, METHOD_BUFFERED, FILE_SPECIAL_ACCESS )
#define IOCTL_GetBaseAddress    CTL_CODE( FILE_DEVICE_UNKNOWN, 0xcc051, METHOD_BUFFERED, FILE_SPECIAL_ACCESS )

struct DriverGetBaseRequest {
	uint32_t m_pid;
	uint64_t* m_address;
};

struct DriverReadRequest {
	uint32_t m_pid;
	uint64_t m_address;
	uint64_t m_buffer;
	uint64_t m_size;
};

struct DriverWriteRequest {
	uint32_t m_pid;
	uint64_t m_address;
	uint64_t m_buffer;
	uint64_t m_size;
};
