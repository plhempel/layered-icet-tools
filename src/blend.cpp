#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

#include <png.hpp>

#include "common.hpp"


/// Blend PNG files passed as command line arguments from front to back.
auto main(int const argc, char const* const argv[]) -> int try {
	using namespace deep_icet;

	// Verify that there is at least one layer.
	if (argc <= 1) {
		std::cerr << log_sev_warn << "No input given, so no output will be produced.\n";
		return EXIT_SUCCESS;
		}

	// Define pixel format.
	using Channel   = uint8_t;
	using Pixel     = png::basic_rgba_pixel<Channel>;
	using PixelData = std::array<Channel, Pixel::traits::channels>;
	using Image     = png::image<Pixel>;

	auto constexpr alpha_channel {Pixel::traits::channels - 1};
	auto constexpr channel_max   {std::numeric_limits<Channel>::max()};

	// Initialize the output image to the first layer.
	Image      blend  {argv[1]};
	auto const width  {blend.get_width()};
	auto const height {blend.get_height()};

	using ImageSize = std::remove_cv_t<decltype(width)>;

	// Scale colors by opacity.
	for (ImageSize y {0}; y < height; ++y) {
		for (ImageSize x {0}; x < width; ++x) {
			auto& pixel {reinterpret_cast<PixelData&>(blend[y][x])};

			for (auto i {0}; i < alpha_channel; ++i) {
				pixel[i] = pixel[i] * pixel[alpha_channel] / channel_max;
				}}}

	// Add remaining layers behind.
	for (auto const* arg_it {&argv[2]}; arg_it < &argv[argc]; ++arg_it) {
		// Read image.
		Image const layer {*arg_it};

		// Ensure all layers have the same size.
		if (layer.get_width() != width or layer.get_height() != height) {
			std::cerr << log_sev_error << "Skipping " << *arg_it
			          << ", which has a different size than the first layer.";
			continue;
			}

		// Blend pixels using the over operator.
		for (ImageSize y {0}; y < height; ++y) {
			for (ImageSize x {0}; x < width; ++x) {
				auto& blend_pixel {reinterpret_cast<PixelData&      >(blend[y][x])};
				auto& layer_pixel {reinterpret_cast<PixelData const&>(layer[y][x])};

				auto const transparency {channel_max - blend_pixel[alpha_channel]};

				// Blend color channels scaled by opacity.
				for (auto i {0}; i < alpha_channel; ++i) {
					blend_pixel[i] += layer_pixel[i]
					                  * layer_pixel[alpha_channel] / channel_max
					                  * transparency               / channel_max;
					}

				// Blend alpha channel.
				blend_pixel[alpha_channel] += layer_pixel[alpha_channel]
				                              * transparency / channel_max;
				}}}

	// Write final image to stdout.
	blend.write_stream(std::cout);
	}
	catch (std::exception const& error) {
		std::cerr << deep_icet::log_sev_fatal << error.what() << "\n";
		return EXIT_FAILURE;
		}
	catch (...) {
		std::cerr << deep_icet::log_sev_fatal << "Unknown error\n";
		return EXIT_FAILURE;
		}
