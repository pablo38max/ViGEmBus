#pragma once

#include <ntddk.h>
#include <wdf.h>

EXTERN_C_START

NTSTATUS UsbHci_GetHcdDriverkeyName(WDFDEVICE Device, WDFREQUEST Request, PULONG_PTR Length);

NTSTATUS UsbHci_GetRootHubName(WDFDEVICE Device, WDFREQUEST Request, PULONG_PTR Length);

EXTERN_C_END
