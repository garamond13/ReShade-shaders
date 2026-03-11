#pragma once

// ReShade
#include <imgui.h> // Has to be included before reshade.hpp
#include <reshade.hpp>
#include <crc32_hash.hpp>

// DirectX
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>

#include "Ensure.h"
#include "ComPtr.h"

// std
#include <vector>
#include <filesystem>
#include <array>
#include <string>
#include <format>
#include <chrono>
#include <thread>
#include <fstream>
#include <numbers>
#include <unordered_map>