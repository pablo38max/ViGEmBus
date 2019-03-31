
#include "ViGEmBusInst.h"
#include <ViGEm/km/BusShared.h>

using namespace colorwin;

int main(int, char* argv[])
{
    argh::parser cmdl;
    cmdl.add_params({ "--inf-path", "--bin-path" });
    cmdl.parse(argv);

    std::string infPath;
    std::wstring infPathW, devicePath, instanceId;

    if ((cmdl({ "--inf-path" }) >> infPath)) {

        infPathW = strconv::multi2wide(infPath);
    }


    auto r = devcon::find(
        &GUID_DEVINTERFACE_BUSENUM_VIGEM,
        devicePath,
        instanceId
    );

    //auto t = devcon::remove(&GUID_DEVINTERFACE_BUSENUM_VIGEM, L"ROOT\\SYSTEM\\0001");
}

std::wstring strconv::multi2wide(const std::string & str, UINT codePage)
{
    if (str.empty())
    {
        return std::wstring();
    }

    int required = ::MultiByteToWideChar(codePage, 0, str.data(), (int)str.size(), NULL, 0);
    if (0 == required)
    {
        return std::wstring();
    }

    std::wstring str2;
    str2.resize(required);

    int converted = ::MultiByteToWideChar(codePage, 0, str.data(), (int)str.size(), &str2[0], str2.capacity());
    if (0 == converted)
    {
        return std::wstring();
    }

    return str2;
}
