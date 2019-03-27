
#include "ViGEmBusInst.h"
#include <ViGEm/km/BusShared.h>

int main()
{
    std::wstring devicePath, instanceId;

    auto r = devcon::find(
        &GUID_DEVINTERFACE_BUSENUM_VIGEM,
        devicePath,
        instanceId
    );

    //auto t = devcon::remove(&GUID_DEVINTERFACE_BUSENUM_VIGEM, L"ROOT\\SYSTEM\\0001");
}
