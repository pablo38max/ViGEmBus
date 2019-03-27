#pragma once

#include <guiddef.h>
#include <string>

namespace devcon
{
    bool create(std::wstring className, const GUID *classGuid, std::wstring hardwareId);

    bool remove(const GUID *classGuid, std::wstring instanceId);

    bool refresh();

    bool update(const std::wstring& hardwareId, const std::wstring& infPath, bool& rebootRequired);

    bool install(const std::wstring& infPath, bool& rebootRequired);
};
