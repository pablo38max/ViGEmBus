#pragma once

//
// Windows
// 
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SetupAPI.h>
#include <newdev.h>

//
// Device class interfaces
// 
#include <initguid.h>
#include <devguid.h>

//
// STL
// 
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

//
// CLI argument parser
// 
#include "argh.h"

#include "colorwin.hpp"

#include "Devcon.h"

namespace strconv
{
    std::wstring multi2wide(const std::string& str, UINT codePage = CP_THREAD_ACP);
};
