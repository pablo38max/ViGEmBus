/*
* Virtual Gamepad Emulation Framework - Windows kernel-mode bus driver
*
* MIT License
*
* Copyright (c) 2016-2020 Nefarius Software Solutions e.U. and Contributors
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "EmulationTargetPDO.hpp"
#include "CRTCPP.hpp"
#include "trace.h"
#include "EmulationTargetPDO.tmh"
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <usbioctl.h>
#include <usbiodef.h>
#include <ViGEmBusDriver.h>


PCWSTR ViGEm::Bus::Core::EmulationTargetPDO::_deviceLocation = L"Virtual Gamepad Emulation Bus";

NTSTATUS ViGEm::Bus::Core::EmulationTargetPDO::PdoCreateDevice(WDFDEVICE ParentDevice, PWDFDEVICE_INIT DeviceInit)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	WDF_DEVICE_PNP_CAPABILITIES pnpCaps;
	WDF_DEVICE_POWER_CAPABILITIES powerCaps;
	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
	WDF_OBJECT_ATTRIBUTES pdoAttributes;
	WDF_IO_QUEUE_CONFIG defaultPdoQueueConfig;
	WDFQUEUE defaultPdoQueue;
	UNICODE_STRING deviceDescription;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_IO_QUEUE_CONFIG usbInQueueConfig;
	WDF_IO_QUEUE_CONFIG notificationsQueueConfig;
	PEMULATION_TARGET_PDO_CONTEXT pPdoContext;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BUSPDO, "%!FUNC! Entry");

	DECLARE_CONST_UNICODE_STRING(deviceLocation, L"Virtual Gamepad Emulation Bus");
	DECLARE_UNICODE_STRING_SIZE(buffer, MAX_INSTANCE_ID_LEN);
	// reserve space for device id
	DECLARE_UNICODE_STRING_SIZE(deviceId, MAX_INSTANCE_ID_LEN);

	UNREFERENCED_PARAMETER(ParentDevice);

	PAGED_CODE();

	// set device type
	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);
	// Bus is power policy owner
	WdfDeviceInitSetPowerPolicyOwnership(DeviceInit, FALSE);

	do
	{
#pragma region Enter RAW device mode

		status = WdfPdoInitAssignRawDevice(DeviceInit, &GUID_DEVCLASS_VIGEM_RAWPDO);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfPdoInitAssignRawDevice failed with status %!STATUS!",
				status);
			break;
		}

		WdfDeviceInitSetCharacteristics(DeviceInit, FILE_AUTOGENERATED_DEVICE_NAME, TRUE);

		status = WdfDeviceInitAssignSDDLString(DeviceInit, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfDeviceInitAssignSDDLString failed with status %!STATUS!",
				status);
			break;
		}

#pragma endregion

#pragma region Prepare PDO

		status = this->PdoPrepareDevice(DeviceInit, &deviceId, &deviceDescription);

		if (!NT_SUCCESS(status))
			break;

		// set device id
		status = WdfPdoInitAssignDeviceID(DeviceInit, &deviceId);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfPdoInitAssignDeviceID failed with status %!STATUS!",
				status);
			break;
		}

		// prepare instance id
		status = RtlUnicodeStringPrintf(&buffer, L"%02d", this->_SerialNo);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"RtlUnicodeStringPrintf failed with status %!STATUS!",
				status);
			break;
		}

		// set instance id
		status = WdfPdoInitAssignInstanceID(DeviceInit, &buffer);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfPdoInitAssignInstanceID failed with status %!STATUS!",
				status);
			break;
		}

		// set device description (for English operating systems)
		status = WdfPdoInitAddDeviceText(DeviceInit, &deviceDescription, &deviceLocation, 0x409);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfPdoInitAddDeviceText failed with status %!STATUS!",
				status);
			break;
		}

		// default locale is English
		// TODO: add more locales
		WdfPdoInitSetDefaultLocale(DeviceInit, 0x409);

#pragma region PNP/Power event callbacks

		WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

		pnpPowerCallbacks.EvtDevicePrepareHardware = EvtDevicePrepareHardware;

		WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

#pragma endregion

		// NOTE: not utilized at the moment
		WdfPdoInitAllowForwardingRequestToParent(DeviceInit);

#pragma endregion

#pragma region Create PDO

		// Add common device data context
		WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&pdoAttributes, EMULATION_TARGET_PDO_CONTEXT);

		pdoAttributes.EvtCleanupCallback = EvtDeviceContextCleanup;

		status = WdfDeviceCreate(&DeviceInit, &pdoAttributes, &this->_PdoDevice);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfDeviceCreate failed with status %!STATUS!",
				status);
			break;
		}

		TraceEvents(TRACE_LEVEL_VERBOSE,
			TRACE_BUSPDO,
			"Created PDO 0x%p",
			this->_PdoDevice);

#pragma endregion

#pragma region Expose USB Interface

		status = WdfDeviceCreateDeviceInterface(
			ParentDevice,
			const_cast<LPGUID>(&GUID_DEVINTERFACE_USB_DEVICE),
			nullptr
		);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfDeviceCreateDeviceInterface failed with status %!STATUS!",
				status);
			break;
		}

#pragma endregion

#pragma region Set PDO contexts

		//
		// Bind this object and device context together
		// 
		pPdoContext = EmulationTargetPdoGetContext(this->_PdoDevice);
		pPdoContext->Target = this;

		status = this->PdoInitContext();

		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"Couldn't initialize additional contexts: %!STATUS!",
				status);
			break;
		}

#pragma endregion

#pragma region Create Queues & Locks
		
		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = this->_PdoDevice;

		// Create and assign queue for incoming interrupt transfer
		WDF_IO_QUEUE_CONFIG_INIT(&usbInQueueConfig, WdfIoQueueDispatchManual);

		status = WdfIoQueueCreate(
			this->_PdoDevice,
			&usbInQueueConfig,
			WDF_NO_OBJECT_ATTRIBUTES,
			&this->_PendingUsbInRequests
		);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfIoQueueCreate (PendingUsbInRequests) failed with status %!STATUS!",
				status);
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = this->_PdoDevice;

		// Create and assign queue for user-land notification requests
		WDF_IO_QUEUE_CONFIG_INIT(&notificationsQueueConfig, WdfIoQueueDispatchManual);

		status = WdfIoQueueCreate(
			ParentDevice,
			&notificationsQueueConfig,
			WDF_NO_OBJECT_ATTRIBUTES,
			&this->_PendingNotificationRequests
		);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfIoQueueCreate (PendingNotificationRequests) failed with status %!STATUS!",
				status);
			break;
		}

#pragma endregion

#pragma region Default I/O queue setup

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = this->_PdoDevice;

		WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&defaultPdoQueueConfig, WdfIoQueueDispatchParallel);

		defaultPdoQueueConfig.EvtIoInternalDeviceControl = EvtIoInternalDeviceControl;

		status = WdfIoQueueCreate(
			this->_PdoDevice,
			&defaultPdoQueueConfig,
			WDF_NO_OBJECT_ATTRIBUTES,
			&defaultPdoQueue
		);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR,
				TRACE_BUSPDO,
				"WdfIoQueueCreate (Default) failed with status %!STATUS!",
				status);
			break;
		}

#pragma endregion

#pragma region PNP capabilities

		WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);

		pnpCaps.Removable = WdfTrue;
		pnpCaps.EjectSupported = WdfTrue;
		pnpCaps.SurpriseRemovalOK = WdfTrue;

		pnpCaps.Address = this->_SerialNo;
		pnpCaps.UINumber = this->_SerialNo;

		WdfDeviceSetPnpCapabilities(this->_PdoDevice, &pnpCaps);

#pragma endregion

#pragma region Power capabilities

		WDF_DEVICE_POWER_CAPABILITIES_INIT(&powerCaps);

		powerCaps.DeviceD1 = WdfTrue;
		powerCaps.WakeFromD1 = WdfTrue;
		powerCaps.DeviceWake = PowerDeviceD1;

		powerCaps.DeviceState[PowerSystemWorking] = PowerDeviceD0;
		powerCaps.DeviceState[PowerSystemSleeping1] = PowerDeviceD1;
		powerCaps.DeviceState[PowerSystemSleeping2] = PowerDeviceD3;
		powerCaps.DeviceState[PowerSystemSleeping3] = PowerDeviceD3;
		powerCaps.DeviceState[PowerSystemHibernate] = PowerDeviceD3;
		powerCaps.DeviceState[PowerSystemShutdown] = PowerDeviceD3;

		WdfDeviceSetPowerCapabilities(this->_PdoDevice, &powerCaps);

#pragma endregion
	} while (FALSE);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BUSPDO, "%!FUNC! Exit with status %!STATUS!", status);

	return status;
}

VOID ViGEm::Bus::Core::EmulationTargetPDO::EvtDeviceContextCleanup(
	IN WDFOBJECT Device
)
{
	TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BUSPDO, "%!FUNC! Entry");

	const auto ctx = EmulationTargetPdoGetContext(Device);

	//
	// This queues parent is the FDO so explicitly free memory
	//
	WdfIoQueuePurgeSynchronously(ctx->Target->_PendingPlugInRequests);
	WdfObjectDelete(ctx->Target->_PendingPlugInRequests);

	//
	// Wait for thread to finish, if active
	// 
	if (ctx->Target->_PluginRequestCompletionWorkerThreadHandle)
	{
		NTSTATUS status = KeWaitForSingleObject(
			&ctx->Target->_PluginRequestCompletionWorkerThreadHandle,
			Executive,
			KernelMode,
			FALSE,
			nullptr
		);

		if (NT_SUCCESS(status))
		{
			ZwClose(ctx->Target->_PluginRequestCompletionWorkerThreadHandle);
			ctx->Target->_PluginRequestCompletionWorkerThreadHandle = nullptr;
		}
		else
		{
			TraceEvents(TRACE_LEVEL_WARNING,
				TRACE_BUSPDO,
				"KeWaitForSingleObject failed with status %!STATUS!",
				status
			);
		}
	}
	
	//
	// PDO device object getting disposed, free context object 
	// 
	delete ctx->Target;

	TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_BUSPDO, "%!FUNC! Exit");
}

NTSTATUS ViGEm::Bus::Core::EmulationTargetPDO::SubmitReport(PVOID NewReport)
{
	return (this->IsOwnerProcess())
		? this->SubmitReportImpl(NewReport)
		: STATUS_ACCESS_DENIED;
}

NTSTATUS ViGEm::Bus::Core::EmulationTargetPDO::EnqueueNotification(WDFREQUEST Request) const
{
	return (this->IsOwnerProcess())
		? WdfRequestForwardToIoQueue(Request, this->_PendingNotificationRequests)
		: STATUS_ACCESS_DENIED;
}

bool ViGEm::Bus::Core::EmulationTargetPDO::IsOwnerProcess() const
{
	return this->_OwnerProcessId == current_process_id();
}

VIGEM_TARGET_TYPE ViGEm::Bus::Core::EmulationTargetPDO::GetType() const
{
	return this->_TargetType;
}

NTSTATUS ViGEm::Bus::Core::EmulationTargetPDO::EnqueuePlugin(WDFREQUEST Request)
{
	NTSTATUS status;

	if (!this->IsOwnerProcess())
		return STATUS_ACCESS_DENIED;

	if (!this->_PendingPlugInRequests)
		return STATUS_INVALID_DEVICE_STATE;

	if (this->_PluginRequestCompletionWorkerThreadHandle)
	{
		status = KeWaitForSingleObject(
			&this->_PluginRequestCompletionWorkerThreadHandle,
			Executive,
			KernelMode,
			FALSE,
			nullptr
		);

		if (NT_SUCCESS(status))
		{
			ZwClose(this->_PluginRequestCompletionWorkerThreadHandle);
			this->_PluginRequestCompletionWorkerThreadHandle = nullptr;
		}
		else
		{
			TraceEvents(TRACE_LEVEL_WARNING,
			            TRACE_BUSPDO,
			            "KeWaitForSingleObject failed with status %!STATUS!",
			            status
			);
		}
	}
	
	status = WdfRequestForwardToIoQueue(Request, this->_PendingPlugInRequests);

	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR,
		            TRACE_BUSPDO,
		            "WdfRequestForwardToIoQueue failed with status %!STATUS!",
		            status
		);

		return status;
	}

	OBJECT_ATTRIBUTES threadOb;

	InitializeObjectAttributes(&threadOb, NULL,
	                           OBJ_KERNEL_HANDLE, NULL, NULL);

	status = PsCreateSystemThread(&this->_PluginRequestCompletionWorkerThreadHandle,
	                              static_cast<ACCESS_MASK>(0L),
	                              &threadOb,
	                              nullptr,
	                              nullptr,
	                              PluginRequestCompletionWorkerRoutine,
	                              this
	);

	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR,
		            TRACE_BUSPDO,
		            "PsCreateSystemThread failed with status %!STATUS!",
		            status
		);
	}

	return status;
}

NTSTATUS ViGEm::Bus::Core::EmulationTargetPDO::PdoPrepare(WDFDEVICE ParentDevice)
{
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_IO_QUEUE_CONFIG plugInQueueConfig;
	
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = ParentDevice;

	// Create and assign queue for incoming interrupt transfer
	WDF_IO_QUEUE_CONFIG_INIT(&plugInQueueConfig, WdfIoQueueDispatchManual);

	status = WdfIoQueueCreate(
		ParentDevice,
		&plugInQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&this->_PendingPlugInRequests
	);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR,
			TRACE_BUSPDO,
			"WdfIoQueueCreate (PendingPlugInRequests) failed with status %!STATUS!",
			status);
	}

	return status;
}

unsigned long ViGEm::Bus::Core::EmulationTargetPDO::current_process_id()
{
	return static_cast<DWORD>(reinterpret_cast<DWORD_PTR>(PsGetCurrentProcessId()) & 0xFFFFFFFF);
}

#pragma region USB Interface Functions

BOOLEAN USB_BUSIFFN ViGEm::Bus::Core::EmulationTargetPDO::UsbInterfaceIsDeviceHighSpeed(IN PVOID BusContext)
{
	UNREFERENCED_PARAMETER(BusContext);

	return TRUE;
}

NTSTATUS USB_BUSIFFN ViGEm::Bus::Core::EmulationTargetPDO::UsbInterfaceQueryBusInformation(
	IN PVOID BusContext, IN ULONG Level,
	IN OUT PVOID BusInformationBuffer,
	IN OUT PULONG BusInformationBufferLength,
	OUT PULONG BusInformationActualLength)
{
	UNREFERENCED_PARAMETER(BusContext);
	UNREFERENCED_PARAMETER(Level);
	UNREFERENCED_PARAMETER(BusInformationBuffer);
	UNREFERENCED_PARAMETER(BusInformationBufferLength);
	UNREFERENCED_PARAMETER(BusInformationActualLength);

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS USB_BUSIFFN ViGEm::Bus::Core::EmulationTargetPDO::UsbInterfaceSubmitIsoOutUrb(IN PVOID BusContext, IN PURB Urb)
{
	UNREFERENCED_PARAMETER(BusContext);
	UNREFERENCED_PARAMETER(Urb);

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS USB_BUSIFFN ViGEm::Bus::Core::EmulationTargetPDO::UsbInterfaceQueryBusTime(
	IN PVOID BusContext, IN OUT PULONG CurrentUsbFrame)
{
	UNREFERENCED_PARAMETER(BusContext);
	UNREFERENCED_PARAMETER(CurrentUsbFrame);

	return STATUS_UNSUCCESSFUL;
}

VOID USB_BUSIFFN ViGEm::Bus::Core::EmulationTargetPDO::UsbInterfaceGetUSBDIVersion(IN PVOID BusContext,
	IN OUT PUSBD_VERSION_INFORMATION
	VersionInformation,
	IN OUT PULONG HcdCapabilities)
{
	UNREFERENCED_PARAMETER(BusContext);

	if (VersionInformation != nullptr)
	{
		VersionInformation->USBDI_Version = 0x500; /* Usbport */
		VersionInformation->Supported_USB_Version = 0x200; /* USB 2.0 */
	}

	if (HcdCapabilities != nullptr)
	{
		*HcdCapabilities = 0;
	}
}


#pragma endregion

VOID ViGEm::Bus::Core::EmulationTargetPDO::PluginRequestCompletionWorkerRoutine(IN PVOID StartContext)
{
	const auto ctx = static_cast<EmulationTargetPDO*>(StartContext);

	WDFREQUEST pluginRequest;
	LARGE_INTEGER timeout;
	timeout.QuadPart = WDF_REL_TIMEOUT_IN_SEC(1);

	TraceEvents(TRACE_LEVEL_INFORMATION,
	            TRACE_BUSPDO,
	            "Waiting for 1 second to complete PDO boot..."
	);

	NTSTATUS status = KeWaitForSingleObject(
		&ctx->_PdoBootNotificationEvent,
		Executive,
		KernelMode,
		FALSE,
		&timeout
	);

	do
	{
		//
		// Fetch pending request (there has to be one at this point)
		// 
		if (!NT_SUCCESS(WdfIoQueueRetrieveNextRequest(ctx->_PendingPlugInRequests, &pluginRequest)))
		{
			TraceEvents(TRACE_LEVEL_WARNING,
			            TRACE_BUSPDO,
			            "No pending plugin request available"
			);
			break;
		}

		if (status == STATUS_TIMEOUT)
		{
			TraceEvents(TRACE_LEVEL_WARNING,
			            TRACE_BUSPDO,
			            "Plugin request timed out, completing with error"
			);

			//
			// We haven't hit a path where the event gets signaled, report error
			// 
			WdfRequestComplete(pluginRequest, STATUS_DEVICE_HARDWARE_ERROR);
			break;
		}

		if (NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_INFORMATION,
			            TRACE_BUSPDO,
			            "Plugin request completed successfully"
			);

			//
			// Event triggered in time, complete with success
			// 
			WdfRequestComplete(pluginRequest, STATUS_SUCCESS);
			break;
		}
	}
	while (FALSE);

	ZwClose(ctx->_PluginRequestCompletionWorkerThreadHandle);
	ctx->_PluginRequestCompletionWorkerThreadHandle = nullptr;
	KeClearEvent(&ctx->_PdoBootNotificationEvent);
	(void)PsTerminateSystemThread(0);
}

VOID ViGEm::Bus::Core::EmulationTargetPDO::DumpAsHex(PCSTR Prefix, PVOID Buffer, ULONG BufferLength)
{
#ifdef DBG

	size_t dumpBufferLength = ((BufferLength * sizeof(CHAR)) * 2) + 1;
	PSTR dumpBuffer = static_cast<PSTR>(ExAllocatePoolWithTag(
		NonPagedPoolNx,
		dumpBufferLength,
		'1234'
	));
	if (dumpBuffer)
	{

		RtlZeroMemory(dumpBuffer, dumpBufferLength);

		for (ULONG i = 0; i < BufferLength; i++)
		{
			sprintf(&dumpBuffer[i * 2], "%02X", static_cast<PUCHAR>(Buffer)[i]);
		}

		TraceDbg(TRACE_BUSPDO,
			"%s - Buffer length: %04d, buffer content: %s\n",
			Prefix,
			BufferLength,
			dumpBuffer
		);

		ExFreePoolWithTag(dumpBuffer, '1234');
	}
#else
	UNREFERENCED_PARAMETER(Prefix);
	UNREFERENCED_PARAMETER(Buffer);
	UNREFERENCED_PARAMETER(BufferLength);
#endif
}

void ViGEm::Bus::Core::EmulationTargetPDO::UsbAbortPipe()
{
	this->AbortPipe();

	// Higher driver shutting down, emptying PDOs queues
	WdfIoQueuePurge(this->_PendingUsbInRequests, nullptr, nullptr);
	WdfIoQueuePurge(this->_PendingNotificationRequests, nullptr, nullptr);
}

NTSTATUS ViGEm::Bus::Core::EmulationTargetPDO::UsbGetConfigurationDescriptorType(PURB Urb)
{
	const PUCHAR Buffer = static_cast<PUCHAR>(Urb->UrbControlDescriptorRequest.TransferBuffer);

	// First request just gets required buffer size back
	if (Urb->UrbControlDescriptorRequest.TransferBufferLength == sizeof(USB_CONFIGURATION_DESCRIPTOR))
	{
		const ULONG length = sizeof(USB_CONFIGURATION_DESCRIPTOR);

		this->GetConfigurationDescriptorType(Buffer, length);
		return STATUS_SUCCESS;
	}

	const ULONG length = Urb->UrbControlDescriptorRequest.TransferBufferLength;

	// Second request can store the whole descriptor
	if (length >= _UsbConfigurationDescriptionSize)
	{
		this->GetConfigurationDescriptorType(Buffer, _UsbConfigurationDescriptionSize);
		return STATUS_SUCCESS;
	}

	return STATUS_UNSUCCESSFUL;
}

NTSTATUS ViGEm::Bus::Core::EmulationTargetPDO::UsbSelectConfiguration(PURB Urb)
{
	TraceDbg(
		TRACE_USBPDO,
		">> >> >> URB_FUNCTION_SELECT_CONFIGURATION: TotalLength %d",
		Urb->UrbHeader.Length);

	if (Urb->UrbHeader.Length == sizeof(struct _URB_SELECT_CONFIGURATION))
	{
		TraceDbg(
			TRACE_USBPDO,
			">> >> >> URB_FUNCTION_SELECT_CONFIGURATION: NULL ConfigurationDescriptor");
		return STATUS_SUCCESS;
	}

	return this->SelectConfiguration(Urb);
}

ViGEm::Bus::Core::EmulationTargetPDO::
EmulationTargetPDO(ULONG Serial, LONG SessionId, USHORT VendorId, USHORT ProductId) : _SerialNo(Serial),
_SessionId(SessionId),
_VendorId(VendorId),
_ProductId(ProductId)
{
	this->_OwnerProcessId = current_process_id();
	KeInitializeEvent(&this->_PdoBootNotificationEvent, NotificationEvent, FALSE);
}

bool ViGEm::Bus::Core::EmulationTargetPDO::GetPdoBySerial(
	IN WDFDEVICE ParentDevice, IN ULONG SerialNo, OUT EmulationTargetPDO** Object)
{
	WDF_CHILD_RETRIEVE_INFO info;

	const WDFCHILDLIST list = WdfFdoGetDefaultChildList(ParentDevice);

	PDO_IDENTIFICATION_DESCRIPTION description;

	WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(&description.Header, sizeof(description));

	//
	// Identify by serial number
	// 
	description.SerialNo = SerialNo;

	WDF_CHILD_RETRIEVE_INFO_INIT(&info, &description.Header);

	const WDFDEVICE pdoDevice = WdfChildListRetrievePdo(list, &info);

	if (pdoDevice == nullptr)
		return false;

	*Object = EmulationTargetPdoGetContext(pdoDevice)->Target;

	return true;
}

bool ViGEm::Bus::Core::EmulationTargetPDO::GetPdoByTypeAndSerial(IN WDFDEVICE ParentDevice, IN VIGEM_TARGET_TYPE Type,
	IN ULONG SerialNo, OUT EmulationTargetPDO** Object)
{
	return (GetPdoBySerial(ParentDevice, SerialNo, Object) && (*Object)->GetType() == Type);
}

NTSTATUS ViGEm::Bus::Core::EmulationTargetPDO::EvtDevicePrepareHardware(
	_In_ WDFDEVICE Device,
	_In_ WDFCMRESLIST ResourcesRaw,
	_In_ WDFCMRESLIST ResourcesTranslated
)
{
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BUSPDO, "%!FUNC! Entry");

	UNREFERENCED_PARAMETER(ResourcesRaw);
	UNREFERENCED_PARAMETER(ResourcesTranslated);

	const auto ctx = EmulationTargetPdoGetContext(Device);

	NTSTATUS status = ctx->Target->PdoPrepareHardware();

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BUSPDO, "%!FUNC! Exit with status %!STATUS!", status);

	return status;
}

VOID ViGEm::Bus::Core::EmulationTargetPDO::EvtIoInternalDeviceControl(
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ size_t OutputBufferLength,
	_In_ size_t InputBufferLength,
	_In_ ULONG IoControlCode
)
{
	const auto ctx = EmulationTargetPdoGetContext(WdfIoQueueGetDevice(Queue));

	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	NTSTATUS status = STATUS_INVALID_PARAMETER;
	PIRP irp;
	PURB urb;
	PIO_STACK_LOCATION irpStack;

	TraceDbg(TRACE_BUSPDO, "%!FUNC! Entry");

	// No help from the framework available from here on
	irp = WdfRequestWdmGetIrp(Request);
	irpStack = IoGetCurrentIrpStackLocation(irp);

	switch (IoControlCode)
	{
	case IOCTL_INTERNAL_USB_SUBMIT_URB:

		TraceDbg(
			TRACE_BUSPDO,
			">> IOCTL_INTERNAL_USB_SUBMIT_URB");

		urb = static_cast<PURB>(URB_FROM_IRP(irp));

		switch (urb->UrbHeader.Function)
		{
		case URB_FUNCTION_CONTROL_TRANSFER:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_CONTROL_TRANSFER");

			status = ctx->Target->UsbControlTransfer(urb);

			break;

		case URB_FUNCTION_CONTROL_TRANSFER_EX:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_CONTROL_TRANSFER_EX");

			status = STATUS_UNSUCCESSFUL;

			break;

		case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:

			TraceDbg(
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER");

			status = ctx->Target->UsbBulkOrInterruptTransfer(&urb->UrbBulkOrInterruptTransfer, Request);

			break;

		case URB_FUNCTION_SELECT_CONFIGURATION:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_SELECT_CONFIGURATION");

			status = ctx->Target->UsbSelectConfiguration(urb);

			break;

		case URB_FUNCTION_SELECT_INTERFACE:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_SELECT_INTERFACE");

			status = ctx->Target->UsbSelectInterface(urb);

			break;

		case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE");

			switch (urb->UrbControlDescriptorRequest.DescriptorType)
			{
			case USB_DEVICE_DESCRIPTOR_TYPE:

				TraceEvents(TRACE_LEVEL_VERBOSE,
					TRACE_BUSPDO,
					">> >> >> USB_DEVICE_DESCRIPTOR_TYPE");

				status = ctx->Target->UsbGetDeviceDescriptorType(
					static_cast<PUSB_DEVICE_DESCRIPTOR>(urb->UrbControlDescriptorRequest.TransferBuffer));

				break;

			case USB_CONFIGURATION_DESCRIPTOR_TYPE:

				TraceEvents(TRACE_LEVEL_VERBOSE,
					TRACE_BUSPDO,
					">> >> >> USB_CONFIGURATION_DESCRIPTOR_TYPE");

				status = ctx->Target->UsbGetConfigurationDescriptorType(urb);

				break;

			case USB_STRING_DESCRIPTOR_TYPE:

				TraceEvents(TRACE_LEVEL_VERBOSE,
					TRACE_BUSPDO,
					">> >> >> USB_STRING_DESCRIPTOR_TYPE");

				status = ctx->Target->UsbGetStringDescriptorType(urb);

				break;

			default:

				TraceEvents(TRACE_LEVEL_VERBOSE,
					TRACE_BUSPDO,
					">> >> >> Unknown descriptor type");

				break;
			}

			TraceDbg(
				TRACE_BUSPDO,
				"<< <<");

			break;

		case URB_FUNCTION_GET_STATUS_FROM_DEVICE:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_GET_STATUS_FROM_DEVICE");

			// Defaults always succeed
			status = STATUS_SUCCESS;

			break;

		case URB_FUNCTION_ABORT_PIPE:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_ABORT_PIPE");

			ctx->Target->UsbAbortPipe();

			break;

		case URB_FUNCTION_CLASS_INTERFACE:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_CLASS_INTERFACE");

			status = ctx->Target->UsbClassInterface(urb);

			break;

		case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >> URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE");

			status = ctx->Target->UsbGetDescriptorFromInterface(urb);

			break;

		default:

			TraceEvents(TRACE_LEVEL_VERBOSE,
				TRACE_BUSPDO,
				">> >>  Unknown function: 0x%X",
				urb->UrbHeader.Function);

			break;
		}

		TraceDbg(
			TRACE_BUSPDO,
			"<<");

		break;

	case IOCTL_INTERNAL_USB_GET_PORT_STATUS:

		TraceEvents(TRACE_LEVEL_VERBOSE,
			TRACE_BUSPDO,
			">> IOCTL_INTERNAL_USB_GET_PORT_STATUS");

		// We report the (virtual) port as always active
		*static_cast<unsigned long*>(irpStack->Parameters.Others.Argument1) = USBD_PORT_ENABLED | USBD_PORT_CONNECTED;

		status = STATUS_SUCCESS;

		break;

	case IOCTL_INTERNAL_USB_RESET_PORT:

		TraceEvents(TRACE_LEVEL_VERBOSE,
			TRACE_BUSPDO,
			">> IOCTL_INTERNAL_USB_RESET_PORT");

		// Sure, why not ;)
		status = STATUS_SUCCESS;

		break;

	case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION:

		TraceEvents(TRACE_LEVEL_VERBOSE,
			TRACE_BUSPDO,
			">> IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION");

		// TODO: implement?
		// This happens if the I/O latency is too high so HIDUSB aborts communication.
		status = STATUS_SUCCESS;

		break;

	default:

		TraceEvents(TRACE_LEVEL_VERBOSE,
			TRACE_BUSPDO,
			">> Unknown I/O control code 0x%X",
			IoControlCode);

		break;
	}

	if (status != STATUS_PENDING)
	{
		WdfRequestComplete(Request, status);
	}

	TraceDbg(TRACE_BUSPDO, "%!FUNC! Exit with status %!STATUS!", status);
}
