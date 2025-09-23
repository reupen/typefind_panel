#pragma once

#define NOMINMAX
#define OEMRESOURCE

#include <cwctype>
#include <execution>
#include <ranges>
#include <regex>
#include <sstream>
#include <string>

// Included before windows.h, because pfc.h includes winsock2.h
#include "../pfc/pfc.h"

#include <Windows.h>
#include <windowsx.h>

#include <wil/win32_helpers.h>

#include "../foobar2000/SDK/foobar2000.h"

#include "../columns_ui-sdk/ui_extension.h"
#include "../ui_helpers/stdafx.h"
#include "../fbh/stdafx.h"
