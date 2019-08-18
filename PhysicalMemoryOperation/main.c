#include "Head.h"

PHYSICAL_OP_CR3	g_PhysicalOpCR3 = { 0 };

NTSTATUS DispatchCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS DispatchClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS DispatchIoctl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG uIoControlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
	PVOID pIoBuffer = pIrp->AssociatedIrp.SystemBuffer;
	ULONG uInSize = pIrpStack->Parameters.DeviceIoControl.InputBufferLength;
	ULONG uOutSize = pIrpStack->Parameters.DeviceIoControl.OutputBufferLength;
	switch (uIoControlCode)
	{
	case IOCTL_TEST:
	{
		DWORD dw;
		memcpy(&dw, pIoBuffer, sizeof(dw));
		dw++;
		memcpy(pIoBuffer, &dw, sizeof(dw));
		status = STATUS_SUCCESS;
		break;
	}
	}
	if (status == STATUS_SUCCESS)
		pIrp->IoStatus.Information = uOutSize;
	else
		pIrp->IoStatus.Information = 0;
	pIrp->IoStatus.Status = status;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return status;
}

VOID DriverUnload(PDRIVER_OBJECT pDriverObj)
{
	UNICODE_STRING strLink;
	//do sth...
	MyPrint(_TitleAndFunc"Exit\n");
	FreePhysicalOpCR3(&g_PhysicalOpCR3);

	//delete device and symbolic link
	RtlInitUnicodeString(&strLink, LINK_NAME);
	IoDeleteSymbolicLink(&strLink);
	IoDeleteDevice(pDriverObj->DeviceObject);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObj, PUNICODE_STRING pRegistryString)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT pDevObj = NULL;
	UNICODE_STRING ustrDeviceName;
	UNICODE_STRING ustrLinkName;
	//set dispatch functions
	pDriverObj->DriverUnload = DriverUnload;
	pDriverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoctl;
	pDriverObj->MajorFunction[IRP_MJ_CREATE] = DispatchCreate;
	pDriverObj->MajorFunction[IRP_MJ_CLOSE] = DispatchClose;
	//create device
	RtlInitUnicodeString(&ustrDeviceName, DEVICE_NAME);
	status = IoCreateDevice(pDriverObj, 0, &ustrDeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDevObj);
	if (!NT_SUCCESS(status))
	{
		return status;
	}
	//create symbolic link
	RtlInitUnicodeString(&ustrLinkName, LINK_NAME);
	status = IoCreateSymbolicLink(&ustrLinkName, &ustrDeviceName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(pDevObj);
		return status;
	}
	//do sth...

	/*
	PVOID pTemp = MmAllocateNonCachedMemory(0x1000);
	PVOID pPhysical = (PVOID)(MmGetPhysicalAddress(pTemp).QuadPart);

	MyPrint(_TitleAndFunc "Entry\n");
	CreatePhysicalOpCR3BySystemCR3(GetCR3ByPID(4), &g_PhysicalOpCR3);

	ContextVirtualToPhysical(&g_PhysicalOpCR3);
	*(PULONG64)(pPhysical) = 0xFFFFFFFFFFFFFFFF;
	ContextPhysicalToVirtual(&g_PhysicalOpCR3);


	MyPrint(_TitleAndFunc "*pPhysical:%16IX\n", *(PULONG64)(pPhysical));
	MmFreeNonCachedMemory(pTemp, 0x1000);
	*/
	
	ContextVirtualToPhysical(&g_PhysicalOpCR3);
	ULONG64 Read1 = *(PULONG64)0x0;
	MyPrint(_TitleAndFunc "ReadTest1:%16IX\n", Read1);
	ContextPhysicalToVirtual(&g_PhysicalOpCR3);

	ContextVirtualToPhysical(&g_PhysicalOpCR3);
	ULONG64 Read2 = *(PULONG64)0x1000;
	MyPrint(_TitleAndFunc "ReadTest2:%16IX\n", Read2);
	ContextPhysicalToVirtual(&g_PhysicalOpCR3);
	

	return status;
}