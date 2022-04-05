#include "PriorityBooster.hpp"

// Driver Entry
extern "C"
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING) {
	// Set major functions and driver unload routine
	DriverObject->DriverUnload = PriorityBoosterUnload;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

	// Device Object Initialization
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");
	PDEVICE_OBJECT DeviceObject;
	NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to create device object (0x%08X)\n", status);
		return status;
	}

	// Symbolic Link Creation
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to create symbolic link (0x%08X)\n", status);
		IoDeleteDevice(DeviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}

_Use_decl_annotations_
void PriorityBoosterUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	// Get IO_STACK_LOCATION*
	auto stack = IoGetCurrentIrpStackLocation(Irp);

	auto status = STATUS_SUCCESS;
	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY: {

		if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ThreadData)) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		// Get pointer to our ThreadData structure coming from usermode. It is legal to access this pointer because we are running under the usermode's context. The usermode thread transitioned to kernel mode.
		auto data = (ThreadData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		if (data == nullptr) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// Check if the priority level is in range
		if (data->Priority < 1 || data->Priority > 31) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// PsLookupThreadByThreadId actually returns an ETHREAD (even tho we need a pointer to KTHREAD). ETHREAD's first member is the actual KTHREAD structure (under the name of Tcb). Thus we can safely cast PETHREAD to PKTHREAD or vice versa.
		PETHREAD Thread;
		status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &Thread);
		if (!NT_SUCCESS(status))
			break;

		// Finally we can change the target thread priority level
		KeSetPriorityThread((PKTHREAD)Thread, data->Priority);

		// This call is very important. When we call PsLookupThreadByThreadId, the reference count to that kernel thread object. That means that cannot actually die until that reference count reaches zero. This is important otherwise the thread could end between PsLookupThreadByThreadId and KeSetPriorityThread calls.
		// So all we have to do now is decrease the thread object's reference, otherwise this target thread will never be able to terminate.
		ObDereferenceObject(Thread);

		DbgPrintEx(0, 0, "Thread Priority change for thread %d to level %d succeeded\n", data->ThreadId, data->Priority);
		break;
	}
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

