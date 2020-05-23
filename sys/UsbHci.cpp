#include "UsbHci.hpp"

#include <usb.h>
#include <usbioctl.h>

#include "trace.h"
#include "usbhci.tmh"

EXTERN_C_START

NTSTATUS UsbHci_GetHcdDriverkeyName(WDFDEVICE Device, WDFREQUEST Request, PULONG_PTR Length)
{
	size_t buflen;
	ULONG len;
	PUSB_HCD_DRIVERKEY_NAME pPayload;

	NTSTATUS status = WdfRequestRetrieveOutputBuffer(
		Request,
		sizeof(USB_HCD_DRIVERKEY_NAME),
		reinterpret_cast<PVOID*>(&pPayload),
		&buflen
	);

	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR,
			TRACE_USBHCI,
			"WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
			status
		);

		return status;
	}

	status = WdfDeviceQueryProperty(
		Device,
		DevicePropertyDriverKeyName,
		(static_cast<ULONG>(buflen) - FIELD_OFFSET(USB_HCD_DRIVERKEY_NAME, DriverKeyName[0])),
		pPayload->DriverKeyName,
		&len
	);

	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR,
			TRACE_USBHCI,
			"WdfDeviceQueryProperty failed with status %!STATUS!",
			status
		);

		return status;
	}

	len += FIELD_OFFSET(USB_HCD_DRIVERKEY_NAME, DriverKeyName[0]);
	pPayload->ActualLength = len;
	if (len > buflen)
	{
		*Length = buflen;
		buflen = (len - FIELD_OFFSET(USB_HCD_DRIVERKEY_NAME, DriverKeyName[0])) / sizeof(WCHAR);
		ASSERT(buflen); // there is always space for at least one wide-char!
		// make sure the last character is 0
		static_cast<PWCHAR>(pPayload->DriverKeyName)[buflen - 1] = 0;
	}
	else
	{
		*Length = len;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION,
		TRACE_USBHCI,
		"!! DevicePropertyDriverKeyName: %ws",
		pPayload->DriverKeyName
	);

	return status;
}

NTSTATUS UsbHci_GetRootHubName(WDFDEVICE Device, WDFREQUEST Request, PULONG_PTR Length)
{
	size_t buflen;
	ULONG len;
	PUSB_ROOT_HUB_NAME pPayload;

	NTSTATUS status = WdfRequestRetrieveOutputBuffer(
		Request,
		sizeof(USB_ROOT_HUB_NAME),
		reinterpret_cast<PVOID*>(&pPayload),
		&buflen
	);

	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR,
			TRACE_USBHCI,
			"WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
			status
		);

		return status;
	}

	status = WdfDeviceQueryProperty(
		Device,
		DevicePropertyDriverKeyName,
		(static_cast<ULONG>(buflen) - FIELD_OFFSET(USB_ROOT_HUB_NAME, RootHubName[0])),
		pPayload->RootHubName,
		&len
	);

	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR,
			TRACE_USBHCI,
			"WdfDeviceQueryProperty failed with status %!STATUS!",
			status
		);

		return status;
	}

	len += FIELD_OFFSET(USB_ROOT_HUB_NAME, RootHubName[0]);
	pPayload->ActualLength = len;
	if (len > buflen)
	{
		*Length = buflen;
		buflen = (len - FIELD_OFFSET(USB_ROOT_HUB_NAME, RootHubName[0])) / sizeof(WCHAR);
		ASSERT(buflen); // there is always space for at least one wide-char!
		// make sure the last character is 0
		static_cast<PWCHAR>(pPayload->RootHubName)[buflen - 1] = 0;
	}
	else
	{
		*Length = len;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION,
		TRACE_USBHCI,
		"!! DevicePropertyDriverKeyName: %ws",
		pPayload->RootHubName
	);

	return status;
}

EXTERN_C_END
