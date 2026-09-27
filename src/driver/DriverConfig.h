#pragma once

// External kernel driver tree (WDK build output).
#define RETRAC_DRIVER_SYS_PATH "C:\\Users\\Zeeni\\Downloads\\driver\\driver\\build\\bin\\kernel\\driver.sys"
// NDIS IRP hook in external driver.sys — IOCTLs go through this device.
#define RETRAC_DRIVER_DEVICE_PATH L"\\\\.\\Ndis"
