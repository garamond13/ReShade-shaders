// cxxopts
#include "cxxopts.hpp"

// Windows
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// LCMS
#include <lcms2.h>

// DirectXTex
#include <DirectXTex.h>

// STD
#include <memory>
#include <array>
#include <vector>
#include <functional>
#include <iostream>

struct Config
{
	std::function<cmsHPROFILE()> src_profile;
	float gamma;
	std::string custom_src_profile;
	std::function<cmsHPROFILE()> dst_profile;
	std::string custom_dst_profile;
	size_t lut_size;
	int intent;
	bool bpc;
};

static Config g_config;

static auto cube(auto a)
{
	return a * a * a;
}

static cmsHPROFILE cms_create_profile_srgb()
{
	const cmsCIExyY whitepoint = { 0.3127, 0.3290, 1.0 };
	const cmsCIExyYTRIPLE primaries = {
		{ 0.6400, 0.3300, 1.0 },
		{ 0.3000, 0.6000, 1.0 },
		{ 0.1500, 0.0600, 1.0 }
	};
	std::array<cmsToneCurve*, 3> tone_curve;
	tone_curve[2] = tone_curve[1] = tone_curve[0] = cmsBuildGamma(nullptr, g_config.gamma);
	cmsHPROFILE profile = cmsCreateRGBProfile(&whitepoint, &primaries, tone_curve.data());
	cmsFreeToneCurve(tone_curve[0]);
	return profile;
}

static cmsHPROFILE get_display_profile()
{
	// Get system default ICC profile.
	auto dc = GetDC(nullptr);
	DWORD buffer_size = 0;
	GetICMProfileA(dc, &buffer_size, nullptr);
	auto path = std::make_unique_for_overwrite<char[]>(buffer_size);
	GetICMProfileA(dc, &buffer_size, path.get());
	ReleaseDC(nullptr, dc);

	return cmsOpenProfileFromFile(path.get(), "r");
}

static cmsHPROFILE get_custom_src_profile()
{
	return cmsOpenProfileFromFile(g_config.custom_src_profile.c_str(), "r");
}

static cmsHPROFILE get_custom_dst_profile()
{
	return cmsOpenProfileFromFile(g_config.custom_dst_profile.c_str(), "r");
}

static void fill_lut(std::vector<uint8_t>& lut)
{
	size_t i = 0;
	for (size_t b = 0; b < g_config.lut_size; ++b) {
		for (size_t g = 0; g < g_config.lut_size; ++g) {
			for (size_t r = 0; r < g_config.lut_size; ++r) {
				lut[i++] = r * 255 / (g_config.lut_size - 1);
				lut[i++] = g * 255 / (g_config.lut_size - 1);
				lut[i++] = b * 255 / (g_config.lut_size - 1);
				i++; // Iterate over alpha.
			}
		}
	}
}

static bool transform_lut(std::vector<uint8_t>& lut)
{
	// Create the transform from src and dst profiles.
	cmsUInt32Number flags = cmsFLAGS_NOCACHE | cmsFLAGS_HIGHRESPRECALC | cmsFLAGS_NOOPTIMIZE;
	if (g_config.bpc) {
		flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
	}
	auto src_profile = g_config.src_profile();
	auto dst_profile = g_config.dst_profile();
	if (!src_profile || !dst_profile) {
		return false;
	}
	auto transform = cmsCreateTransform(src_profile, TYPE_RGBA_8, dst_profile, TYPE_BGRA_8, g_config.intent, flags);

	// At this point we don't need profiles anymore.
	cmsCloseProfile(src_profile);
	cmsCloseProfile(dst_profile);

	// Create the LUT.
	if (!transform) {
		return false;
	}
	fill_lut(lut);
	cmsDoTransform(transform, lut.data(), lut.data(), cube(g_config.lut_size));
	cmsDeleteTransform(transform);

	return true;
}

int main(int argc, char** argv)
{
	// Options
	//

	cxxopts::Options options("CMS LUT Generator", "Generates CMS LUT");

	options.add_options()
		("h,help", "Print help")
		("src-profile", "Source profile (game profile). 0 - sRGB/sRGB, 1 - sRGB/gamma, 2 - custom ICC profile", cxxopts::value<int>()->default_value("0"))
		("gamma", "Set gamma value if src-profile=1", cxxopts::value<float>()->default_value("2.2"))
		("src-icc-path", "Set path to custom source ICC profile if src-profile=2", cxxopts::value<std::string>()->default_value(""))
		("dst-profile", "Destination profile (display profile). 0 - default system profile, 1 - custom ICC profile", cxxopts::value<int>()->default_value("0"))
		("dst-icc-path", "Set path to custom destination ICC profile if dst-profile=1", cxxopts::value<std::string>()->default_value(""))
		("lut-size", "LUT size. The final size will be lut-size^3", cxxopts::value<size_t>()->default_value("65"))
		("intent", "Rendering intent. 0 - perceptual, 1 - relative colorimetric, 2 - saturation, 3 - absolute colorimetric", cxxopts::value<int>()->default_value("0"))
		("bpc", "Enable Black Point Compensation. 0 - disable, 1 - enable", cxxopts::value<bool>()->default_value("1"))
		;

	auto result = options.parse(argc, argv);

	// Handle help.
	if (result.count("help")) {
		std::cout << options.help() << std::endl;
		return 0;
	}

	// Setup g_config.
	switch (result["src-profile"].as<int>()) {
	case 0: {
		g_config.src_profile = cmsCreate_sRGBProfile;
		break;
	}
	case 1: {
		g_config.src_profile = cms_create_profile_srgb;
		break;
	}
	case 2: {
		g_config.src_profile = get_custom_src_profile;
		break;
	}
	default: {
		return 1;
	}
	}
	g_config.gamma = result["gamma"].as<float>();
	g_config.custom_src_profile = result["src-icc-path"].as<std::string>();
	switch (result["dst-profile"].as<int>()) {
	case 0: {
		g_config.dst_profile = get_display_profile;
		break;
	}
	case 1: {
		g_config.dst_profile = get_custom_dst_profile;
		break;
	}
	default: {
		return 1;
	}
	}
	g_config.custom_dst_profile = result["dst-icc-path"].as<std::string>();
	g_config.lut_size = result["lut-size"].as<size_t>();
	g_config.intent = result["intent"].as<int>();
	g_config.bpc = result["bpc"].as<bool>();

	//

	// Tranform LUT.
	std::vector<uint8_t> lut(cube(g_config.lut_size) * 4);
	if (!transform_lut(lut)) {
		std::cerr << "ERROR: Faild to create the LUT.\n";
		return 1;
	}

	// Save the LUT as DDS 3D volume texture.
	//

	// We need to manualy create an array of 2D slices from the LUT data.
	std::vector<DirectX::Image> images(g_config.lut_size);
	const size_t row_pitch = g_config.lut_size * 4; // width * n channels * bytes per channel
	const size_t slice_pitch = row_pitch * g_config.lut_size; // row pitch * height
	uint8_t* lut_data = reinterpret_cast<uint8_t*>(lut.data());
	for (size_t i = 0; i < g_config.lut_size; ++i) {
		images[i].width = g_config.lut_size;
		images[i].height = g_config.lut_size;
		images[i].format = DXGI_FORMAT_R8G8B8A8_UNORM;
		images[i].rowPitch = row_pitch;
		images[i].slicePitch = slice_pitch;
		images[i].pixels = lut_data + i * slice_pitch;
	}

	// Create the final 3D volume image.
	DirectX::ScratchImage scratchImage;
	auto hr = scratchImage.Initialize3DFromImages(images.data(), images.size());
	if (FAILED(hr)) {
		std::cerr << "ERROR: Faild to save the LUT.\n";
		return 1;
	}

	hr = DirectX::SaveToDDSFile(scratchImage.GetImages(), scratchImage.GetImageCount(), scratchImage.GetMetadata(), DirectX::DDS_FLAGS_NONE, L"CMSLUT.dds");
	if (FAILED(hr)) {
		std::cerr << "ERROR: Faild to save the LUT.\n";
		return 1;
	}

	//

	return 0;
}