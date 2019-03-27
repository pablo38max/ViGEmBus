#include "Devcon.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <newdev.h>

#include <cwctype>
#include <string>
#include <algorithm>

bool devcon::create(std::wstring className, const GUID *classGuid, std::wstring hardwareId)
{
    auto deviceInfoSet = SetupDiCreateDeviceInfoList(classGuid, nullptr);

    if (INVALID_HANDLE_VALUE == deviceInfoSet)
        return false;

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    auto cdiRet = SetupDiCreateDeviceInfo(
        deviceInfoSet,
        className.c_str(),
        classGuid,
        nullptr,
        nullptr,
        DICD_GENERATE_ID,
        &deviceInfoData
    );

    if (!cdiRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    auto sdrpRet = SetupDiSetDeviceRegistryProperty(
        deviceInfoSet,
        &deviceInfoData,
        SPDRP_HARDWAREID,
        (const PBYTE)hardwareId.c_str(),
        hardwareId.size() * sizeof(WCHAR)
    );

    if (!sdrpRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    auto cciRet = SetupDiCallClassInstaller(
        DIF_REGISTERDEVICE,
        deviceInfoSet,
        &deviceInfoData
    );

    if (!cciRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return true;
}

bool devcon::remove(const GUID *classGuid, std::wstring instanceId)
{
    auto retval = false;

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    auto deviceInfoSet = SetupDiGetClassDevs(classGuid, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (SetupDiOpenDeviceInfo(deviceInfoSet, instanceId.c_str(), nullptr, 0, &deviceInfoData))
    {
        SP_CLASSINSTALL_HEADER spCih;
        spCih.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        spCih.InstallFunction = DIF_REMOVE;

        SP_REMOVEDEVICE_PARAMS spRdp;
        spRdp.ClassInstallHeader = spCih;
        spRdp.HwProfile = 0x00;
        spRdp.Scope = DI_REMOVEDEVICE_GLOBAL;

        if (SetupDiSetClassInstallParams(
            deviceInfoSet,
            &deviceInfoData,
            &spRdp.ClassInstallHeader,
            sizeof(SP_REMOVEDEVICE_PARAMS)))
        {
            retval = SetupDiCallClassInstaller(DIF_REMOVE, deviceInfoSet, &deviceInfoData);
        }
    }

    if (deviceInfoSet)
        SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return retval;
}

bool devcon::refresh()
{
    DEVINST dinst;

    if (CM_Locate_DevNode_Ex(
        &dinst,
        nullptr,
        0,
        nullptr
    ) != CR_SUCCESS
        ) return false;

    return CM_Reenumerate_DevNode_Ex(dinst, 0, nullptr) == CR_SUCCESS;
}

bool devcon::update(const std::wstring& hardwareId, const std::wstring& infPath, bool& rebootRequired)
{
    BOOL reboot = FALSE;

    const auto ret = UpdateDriverForPlugAndPlayDevices(
        nullptr,
        hardwareId.c_str(),
        infPath.c_str(),
        INSTALLFLAG_FORCE | INSTALLFLAG_NONINTERACTIVE,
        &reboot
    );

    rebootRequired = (reboot == TRUE);

    return ret;
}

bool devcon::install(const std::wstring& infPath, bool & rebootRequired)
{
    BOOL reboot = FALSE;

    const auto ret = DiInstallDriver(
        nullptr,
        infPath.c_str(),
        DIIRFLAG_FORCE_INF,
        &reboot
    );

    rebootRequired = (reboot == TRUE);

    return ret;
}

bool devcon::find(const GUID *classGuid, std::wstring & devicePath, std::wstring & instanceId, int instance)
{
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    SP_DEVINFO_DATA da;
    da.cbSize = sizeof(SP_DEVINFO_DATA);
    DWORD bufferSize = 0, memberIndex = 0;
    bool retval = false;

    const auto deviceInfoSet = SetupDiGetClassDevs(
        classGuid,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    while (SetupDiEnumDeviceInterfaces(
        deviceInfoSet,
        nullptr,
        classGuid,
        memberIndex,
        &deviceInterfaceData))
    {
        // Fetch required buffer size
        SetupDiGetDeviceInterfaceDetail(
            deviceInfoSet,
            &deviceInterfaceData,
            nullptr,
            0,
            &bufferSize,
            &da
        );

        const auto detailDataBuffer = PSP_DEVICE_INTERFACE_DETAIL_DATA(malloc(bufferSize));
        detailDataBuffer->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        // Make request with correct buffer size
        if (SetupDiGetDeviceInterfaceDetail(
            deviceInfoSet,
            &deviceInterfaceData,
            detailDataBuffer,
            bufferSize,
            &bufferSize,
            &da)
            )
        {
            devicePath = std::wstring(detailDataBuffer->DevicePath);
            std::transform(
                devicePath.begin(),
                devicePath.end(),
                devicePath.begin(),
                std::towupper
            );

            if (memberIndex == instance)
            {
                const auto instanceBuffer = PWSTR(malloc(MAX_PATH));

                CM_Get_Device_ID(deviceInterfaceData.Flags, instanceBuffer, MAX_PATH, 0);

                instanceId = std::wstring(instanceBuffer);
                std::transform(
                    instanceId.begin(),
                    instanceId.end(),
                    instanceId.begin(),
                    std::towupper
                );

                free(instanceBuffer);
                retval = true;
            }
        }

        free(detailDataBuffer);

        memberIndex++;
    }

    if (deviceInfoSet)
        SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return retval;
}
