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
#include <fstream>

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

template<typename T>
static size_t get_lut_size()
{
	// We only expect uint8_t and float.
	static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, float>);

	if (std::is_same_v<T, uint8_t>) {
		return std::min(g_config.lut_size, 256ull);
	}
	else {
		return g_config.lut_size;
	}
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

static void fill_lut_rgba8(std::vector<uint8_t>& lut_rgba8)
{
	const size_t lut_size = get_lut_size<uint8_t>();
	size_t i = 0;
	for (size_t b = 0; b < lut_size; ++b) {
		for (size_t g = 0; g < lut_size; ++g) {
			for (size_t r = 0; r < lut_size; ++r) {
				lut_rgba8[i++] = r * 255 / (lut_size - 1);
				lut_rgba8[i++] = g * 255 / (lut_size - 1);
				lut_rgba8[i++] = b * 255 / (lut_size - 1);
				i++; // Iterate over alpha.
			}
		}
	}
}

static void fill_lut_rgb32f(std::vector<float>& lut_rgb32f)
{
	const size_t lut_size = get_lut_size<float>();
	size_t i = 0;
	for (size_t b = 0; b < lut_size; ++b) {
		for (size_t g = 0; g < lut_size; ++g) {
			for (size_t r = 0; r < lut_size; ++r) {
				lut_rgb32f[i++] = static_cast<float>(r) / static_cast<float>(lut_size - 1);
				lut_rgb32f[i++] = static_cast<float>(g) / static_cast<float>(lut_size - 1);
				lut_rgb32f[i++] = static_cast<float>(b) / static_cast<float>(lut_size - 1);
			}
		}
	}
}

static bool transform_luts(std::vector<uint8_t>& lut_rgba8, std::vector<float>& lut_rgb32f)
{
	cmsUInt32Number flags = cmsFLAGS_NOCACHE | cmsFLAGS_HIGHRESPRECALC | cmsFLAGS_NOOPTIMIZE;
	if (g_config.bpc) {
		flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
	}

	// Open profiles.
	auto src_profile = g_config.src_profile();
	auto dst_profile = g_config.dst_profile();
	if (!src_profile || !dst_profile) {
		return false;
	}

	auto transform_rgba8 = cmsCreateTransform(src_profile, TYPE_RGBA_8, dst_profile, TYPE_BGRA_8, g_config.intent, flags);
	auto transform_rgb32f = cmsCreateTransform(src_profile, TYPE_RGB_FLT, dst_profile, TYPE_RGB_FLT, g_config.intent, flags);

	// At this point we don't need profiles anymore.
	cmsCloseProfile(src_profile);
	cmsCloseProfile(dst_profile);

	if (!transform_rgba8 || !transform_rgb32f) {
		return false;
	}

	// Create LUTs.
	fill_lut_rgba8(lut_rgba8);
	fill_lut_rgb32f(lut_rgb32f);
	cmsDoTransform(transform_rgba8, lut_rgba8.data(), lut_rgba8.data(), cube(get_lut_size<uint8_t>()));
	cmsDoTransform(transform_rgb32f, lut_rgb32f.data(), lut_rgb32f.data(), cube(get_lut_size<float>()));
	
	// Clean up.
	cmsDeleteTransform(transform_rgba8);
	cmsDeleteTransform(transform_rgb32f);

	return true;
}

static void write_lut_to_cube_file(const std::vector<float>& lut)
{
	std::ofstream file("CMSLUT.cube");
	if (!file.is_open()) {
		std::cerr << "ERROR: Faild to save CUBE LUT.\n";
		return;
	}
	const size_t lut_size = get_lut_size<float>();
	
	// Write the metadata header.
	file << "# Created by CMSLUTGenerator\n";
	file << "LUT_3D_SIZE " << lut_size << "\n";
	file << "DOMAIN_MIN 0.0 0.0 0.0\n";
	file << "DOMAIN_MAX 1.0 1.0 1.0\n";
	file << "\n";

	// Write the normalized RGB table.
	size_t i = 0;
	while (i < cube(lut_size) * 3) {
		file << std::fixed << std::setprecision(6) << lut[i++] << " ";
		file << std::fixed << std::setprecision(6) << lut[i++] << " ";
		file << std::fixed << std::setprecision(6) << lut[i++] << "\n";
	}

	file.close();
	std::cout << "Successfully saved CUBE LUT.\n";
}

static void write_lut_to_dds_file(std::vector<uint8_t>& lut)
{
	// We need to manualy create an array of 2D slices from the LUT data.
	const size_t lut_size = get_lut_size<uint8_t>();
	std::vector<DirectX::Image> images(lut_size);
	const size_t row_pitch = lut_size * 4; // width * n channels * bytes per channel
	const size_t slice_pitch = row_pitch * lut_size; // row pitch * height
	for (size_t i = 0; i < lut_size; ++i) {
		images[i].width = lut_size;
		images[i].height = lut_size;
		images[i].format = DXGI_FORMAT_R8G8B8A8_UNORM;
		images[i].rowPitch = row_pitch;
		images[i].slicePitch = slice_pitch;
		images[i].pixels = lut.data() + i * slice_pitch;
	}

	// Create the final 3D volume image.
	DirectX::ScratchImage scratch_image;
	auto hr = scratch_image.Initialize3DFromImages(images.data(), images.size());
	if (FAILED(hr)) {
		std::cerr << "ERROR: Faild to save DDS LUT.\n";
		return;
	}

	hr = DirectX::SaveToDDSFile(scratch_image.GetImages(), scratch_image.GetImageCount(), scratch_image.GetMetadata(), DirectX::DDS_FLAGS_NONE, L"CMSLUT.dds");
	if (FAILED(hr)) {
		std::cerr << "ERROR: Faild to save DDS LUT.\n";
		return;
	}
	std::cout << "Successfully saved DDS LUT.\n";
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
		("bpc", "Black Point Compensation. 0 - disable, 1 - enable", cxxopts::value<int>()->default_value("0"))
		;

	auto result = options.parse(argc, argv);

	// Handle help.
	if (result.count("help")) {
		std::cout << options.help() << std::endl;
		return 0;
	}

	// Setup g_config.
	switch (result["src-profile"].as<int>()) {
		case 0:
			g_config.src_profile = cmsCreate_sRGBProfile;
			break;
		case 1:
			g_config.src_profile = cms_create_profile_srgb;
			break;
		case 2:
			g_config.src_profile = get_custom_src_profile;
			break;
		default:
			return 1;
	}
	g_config.gamma = result["gamma"].as<float>();
	g_config.custom_src_profile = result["src-icc-path"].as<std::string>();
	switch (result["dst-profile"].as<int>()) {
		case 0:
			g_config.dst_profile = get_display_profile;
			break;
		case 1:
			g_config.dst_profile = get_custom_dst_profile;
			break;
		default:
			return 1;
	}
	g_config.custom_dst_profile = result["dst-icc-path"].as<std::string>();
	g_config.lut_size = result["lut-size"].as<size_t>();
	g_config.intent = result["intent"].as<int>();
	g_config.bpc = result["bpc"].as<int>();

	//

	// Create and tranform LUTs.
	std::vector<uint8_t> lut_rgba8(cube(get_lut_size<uint8_t>()) * 4);
	std::vector<float> lut_rgb32f(cube(get_lut_size<float>()) * 3);
	if (!transform_luts(lut_rgba8, lut_rgb32f)) {
		std::cerr << "ERROR: Faild to create LUTs.\n";
		return 1;
	}

	// Save LUTs.
	write_lut_to_cube_file(lut_rgb32f);
	write_lut_to_dds_file(lut_rgba8);

	return 0;
}