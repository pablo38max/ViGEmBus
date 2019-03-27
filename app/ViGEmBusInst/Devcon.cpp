#include "Devcon.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>

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
