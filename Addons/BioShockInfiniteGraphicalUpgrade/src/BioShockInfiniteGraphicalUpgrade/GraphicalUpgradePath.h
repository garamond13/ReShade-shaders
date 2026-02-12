#pragma once

#include "Common.h"

inline std::filesystem::path get_graphical_upgrade_path()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(nullptr, filename, ARRAYSIZE(filename));
	std::filesystem::path path = filename;
	path = path.parent_path();
	path /= L".\\GraphicalUpgrade";
	return path;
}

// Path to the GraphicalUpgrade folder.
inline std::filesystem::path g_graphical_upgrade_path;