#pragma once

// ReShade
#include <imgui.h> // Has to be included before reshade.hpp
#include <reshade.hpp>
#include <crc32_hash.hpp>

// DirectX
//#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>

#include "Ensure.h"
#include "ComPtr.h"

// std
#include <filesystem>
#include <array>
#include <string>
#include <format>
#include <numbers>
#include <fstream>