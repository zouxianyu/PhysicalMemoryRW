# PhysicalMemoryRW

A Windows kernel‑mode driver that demonstrates how to read physical memory by switching CR3 (page‑directory‑base) contexts.  
It provides a simple IOCTL (`IOCTL_TEST`) that can be used to verify driver loading and basic interaction, and it contains the core infrastructure (`PhysicalOpCR3`) for mapping physical memory into the driver’s own virtual address space.

## Overview

- **Purpose**: Show how to perform physical‑memory reads (and, by extension, writes) from a driver without relying on the `\Device\PhysicalMemory` interface.  
- **Method**: Temporarily switch to a custom CR3 generated from a copy of the system’s PML4 table, allowing any physical address to be accessed via normal virtual‑memory reads/writes.  
- **Key Components**:
  * `CreatePhysicalOpCR3BySystemCR3` – builds a private PML4/PDPT page‑table pair and maps the system’s PML4.
  * `ContextVirtualToPhysical` / `ContextPhysicalToVirtual` – raise IRQL, switch CR3, and later restore.
  * `OpenPhysicalMemory` / `MapPhysicalMemory` / `UnmapPhysicalMemory` – helper functions that use the `\Device\PhysicalMemory` section to map the system’s PML4 into a writable virtual address.
  * `DispatchIoctl` – driver entry point for `IOCTL_TEST` (increment a DWORD passed from user‑mode).

## Features

* **Basic IOCTL** – `IOCTL_TEST` (0x222000) increments a DWORD supplied in the input buffer.
* **Physical‑memory read primitive** – The driver can read any physical address (e.g., `*(PULONG64)physAddr`) while CR3 is switched.
* **Self‑contained code** – All logic resides in a single driver source tree; no external dependencies other than the Windows Driver Kit (WDK).

## Building the Driver

The driver is a classic WDM (`ntifs.h`) sample and can be built with any compiler that targets x64 Windows kernel code (MSVC via the WDK or clang‑cl). Typical steps:

1. **Install the Windows Driver Kit (WDK)**  
   * Download the latest WDK from Microsoft and run the installer. It adds the `x64` cross‑compilation tools and Visual Studio templates.

2. **Open the source in Visual Studio**  
   * In the repository root, create (or locate) a `.vcxproj` file. If none exists, you can use the WDK’s `New Project → Kernel Mode Driver (WDM) Sample` wizard and copy the provided `.c`/`.h` files into the generated project.

3. **Build**  
   * From the Visual Studio Developer Command Prompt (or any command prompt where the WDK environment variables are set) run:

   ```cmd
   msbuild PhysicalMemoryOperation.sln /p:Configuration=Release
   ```

   or, using the classic `ntbuild.bat` supplied with the WDK:

   ```cmd
   call "%WBINPDIR%\SetupEnvironmentVariables.cmd"
   ntmake.bat
   ```

   The build produces `PhysicalMemoryOperation.sys` in the `Build` directory.

4. **Sign the driver (optional but recommended for production)**  
   * Use `signtool sign /fd sha256 PhysicalMemoryOperation.sys` after obtaining a certificate.

## Installing the Driver

### Using an INF (recommended)

1. **Create a minimal INF file** (`PhysicalMemoryOperation.inf`) – the repository does not ship one, but you can copy a template from the WDK samples:

```inf
[Version]
Signature   = "$CHICAGO$"
Layout      = Driver
DriverType  = 1

[DriverInstall]
AddSec = 0x00000000

[SourceDisksFiles]
PhysicalMemoryOperation.sys = 1,1,0x10

[DestinationDirs]
DefaultDestDir = 1,SYSTEM32_DRIVERS

[DefaultInstall]
AddService = PhysicalMemoryOperation_install, 0xA, PhysicalMemoryOperation_setup

[PhysicalMemoryOperation_setup]
AddDevice = PhysicalMemoryOperation_AddDevice, PhysicalMemoryOperation.sys

[PhysicalMemoryOperation_AddDevice]
CreateDevice = PhysicalMemoryOperation_CreateDevice, 0x00000000

[PhysicalMemoryOperation_CreateDevice]
DeviceName = "\Device\PhysicalMemoryOperation"
DeviceType = 0x00000022
Characteristics = 0x00000040
Reserved = 0x00000000

[PhysicalMemoryOperation_install]
AddService = PhysicalMemoryOperation_install, 0xA, PhysicalMemoryOperation_setup
```

2. **Install** (requires administrative privileges):

```cmd
pnputil /add-driver PhysicalMemoryOperation.inf /install
```

3. **Verify** – the symbolic link `\\?\GLOBALROOT\Device\PhysicalMemoryOperation` (or `\\.\Global\PhysicalMemoryOperation` depending on the link name in `Head.h`) should now appear in the system.

### Manual deployment (for testing)

If you prefer not to use an INF, you can manually copy the built `PhysicalMemoryOperation.sys` to `%SystemRoot%\System32\Drivers\PhysicalMemoryOperation.sys`, create a service entry with SC, and start it:

```cmd
sc create PhysicalMemoryOperation binPath= "%SystemRoot%\System32\drivers\PhysicalMemoryOperation.sys" type= kernel start= auto
sc start PhysicalMemoryOperation
```

## Using the Driver

The driver exposes a single IOCTL (`IOCTL_TEST`) that can be used to confirm that the driver is responding. A minimal user‑mode test program (e.g., in C++/C#) would:

1. Open `\\.\PhysicalMemoryOperation` (the symbolic link defined in `Head.h` as `\\DosDevices\\Global\\PhysicalMemoryOperation`).  
2. Call `DeviceIoControl` with `IOCTL_TEST` (0x222000), passing a DWORD buffer.
3. The driver increments the value and returns it.

### Example (C) – Simple Test

```c
#include <windows.h>
#include <winioctl.h>

#define IOCTL_TEST_CTL_CODE 0x222000

int main()
{
    HANDLE hDevice = CreateFile(L"\\\\.\\Global\\PhysicalMemoryOperation",
                                 GENERIC_READ | GENERIC_WRITE,
                                 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Failed to open device: %lu\n", GetLastError());
        return 1;
    }

    DWORD value = 42;
    DWORD bytes;

    if (DeviceIoControl(hDevice, IOCTL_TEST_CTL_CODE,
                        &value, sizeof(value), &value, sizeof(value), &bytes, NULL)) {
        printf("Driver incremented value: %lu\n", value);
    } else {
        printf("DeviceIoControl failed: %lu\n", GetLastError());
    }

    CloseHandle(hDevice);
    return 0;
}
```

### Extending for Full Physical‑Memory Access

The driver already contains the `ContextVirtualToPhysical` / `ContextPhysicalToVirtual` pair that swaps the thread’s CR3 with a generated one. To expose arbitrary physical reads/writes you could add new IOCTLs, e.g.:

```c
#define IOCTL_READ_PHYSICAL  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_PHYSICAL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
```

The handler would copy a `PHYSICAL_READ_REQUEST` struct (containing `ULONG64 PhysicalAddress`, `PVOID Buffer`, `SIZE_T Size`) into the driver, call `ContextVirtualToPhysical`, perform the copy (`memcpy`), then call `ContextPhysicalToVirtual`.

## Notes & Limitations

* **IRQL & Preemption** – The driver raises the IRQL to `DISPATCH_LEVEL` when switching CR3. This is safe but limits concurrency with other drivers that also raise IRQL.  
* **Process‑specific CR3** – Currently the driver switches to the CR3 of PID 4 (system process). You may want to expose a PID parameter in a future IOCTL.  
* **Security** – This driver grants direct physical‑memory access to any user‑mode process that can open the device. Treat it as a privileged tool; consider adding security checks in a production environment.  
* **Compatibility** – Built for x64 Windows 10/11. The code uses `__readcr3`/`__writecr3` intrinsics which are available in recent MSVC/Clang toolchains.

## License

This repository is provided as an educational sample. Use it at your own risk. No warranty is expressed or implied.
