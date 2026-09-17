#pragma once

#include "Common.h"

// Path to the GraphicalUpgrade folder.
inline std::filesystem::path g_graphical_upgrade_path;

inline void init_graphical_upgrade_path()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(nullptr, filename, ARRAYSIZE(filename));
	std::filesystem::path path = filename;
	path = path.parent_path();
	path /= L".\\GraphicalUpgrade";
	g_graphical_upgrade_path = path;
}