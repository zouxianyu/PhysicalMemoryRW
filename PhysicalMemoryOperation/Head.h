#pragma once

#include <ntifs.h>
#include <ntddk.h>
#include <stdlib.h>
#include <windef.h>
#include <ntimage.h>
#include <intrin.h>

#include "MyDebugPrint.h"
#include "PhysicalMemoryOperation.h"

#define	DEVICE_NAME	L"\\Device\\PhysicalMemoryOperation"
#define LINK_NAME	L"\\DosDevices\\Global\\PhysicalMemoryOperation"

#define IOCTL_TEST	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS) 