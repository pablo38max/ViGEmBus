#pragma once

#include <guiddef.h>
#include <string>

namespace devcon
{
    /**
     * \fn  bool create(std::wstring className, const GUID *classGuid, std::wstring hardwareId);
     *
     * \brief   Creates a new device node.
     *
     * \author  Benjamin "Nefarius" Höglinger-Stelzer
     * \date    29.03.2019
     *
     * \param   className   Name of the device class.
     * \param   classGuid   Unique identifier for the device setup class.
     * \param   hardwareId  Hardware identifier string for the new node.
     *
     * \return  True if it succeeds, false if it fails.
     */
    bool create(std::wstring className, const GUID *classGuid, std::wstring hardwareId);

    /**
     * \fn  bool remove(const GUID *classGuid, std::wstring instanceId);
     *
     * \brief   Removes a device node.
     *
     * \author  Benjamin "Nefarius" Höglinger-Stelzer
     * \date    29.03.2019
     *
     * \param   classGuid   Unique identifier for the device interface class.
     * \param   instanceId  Identifier for the instance.
     *
     * \return  True if it succeeds, false if it fails.
     */
    bool remove(const GUID *classGuid, std::wstring instanceId);

    /**
     * \fn  bool refresh();
     *
     * \brief   Refreshes the device tree on the local system.
     *
     * \author  Benjamin "Nefarius" Höglinger-Stelzer
     * \date    29.03.2019
     *
     * \return  True if it succeeds, false if it fails.
     */
    bool refresh();

    /**
     * \fn  bool update(const std::wstring& hardwareId, const std::wstring& infPath, bool& rebootRequired);
     *
     * \brief   Updates the device driver for all devices of a specified hardware identifier.
     *
     * \author  Benjamin "Nefarius" Höglinger-Stelzer
     * \date    29.03.2019
     *
     * \param           hardwareId      Identifier for the hardware.
     * \param           infPath         Full pathname of the inf file.
     * \param [in,out]  rebootRequired  True if reboot required.
     *
     * \return  True if it succeeds, false if it fails.
     */
    bool update(const std::wstring& hardwareId, const std::wstring& infPath, bool& rebootRequired);

    /**
     * \fn  bool install(const std::wstring& infPath, bool& rebootRequired);
     *
     * \brief   Installs a device driver in the local driver store.
     *
     * \author  Benjamin "Nefarius" Höglinger-Stelzer
     * \date    29.03.2019
     *
     * \param           infPath         Full pathname of the inf file.
     * \param [in,out]  rebootRequired  True if reboot required.
     *
     * \return  True if it succeeds, false if it fails.
     */
    bool install(const std::wstring& infPath, bool& rebootRequired);

    /**
     * \fn  bool find(const GUID *classGuid, std::wstring& devicePath, std::wstring& instanceId, int instance = 0);
     *
     * \brief   Searches for device instances of a given device interface class.
     *
     * \author  Benjamin "Nefarius" Höglinger-Stelzer
     * \date    29.03.2019
     *
     * \param           classGuid   Unique identifier for the device interface class.
     * \param [in,out]  devicePath  Full path of the device node.
     * \param [in,out]  instanceId  Identifier for the instance.
     * \param           instance    (Optional) The instance index.
     *
     * \return  True if it succeeds, false if it fails.
     */
    bool find(const GUID *classGuid, std::wstring& devicePath, std::wstring& instanceId, int instance = 0);
};
