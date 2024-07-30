#include <cstdlib>
#include <iostream>

#include "common.hpp"


/// Blend PNG files front to back.
/// Expected arguments: <width> <height> [<image>]...
auto main(int const argc, char const* const argv[]) -> int try {
	using namespace deep_icet;

	// Parse output size.
	ImageSize width, height;

	if (argc < 3
			or (width  = atoi(argv[1])) == 0
			or (height = atoi(argv[2])) == 0
			) {
		std::cerr << log_sev_fatal << "Invalid or missing arguments.\n"
		             "Usage: " << argv[0] << " <width> <height> [<image>]...\n";
		return EXIT_FAILURE;
		}

	// Allocate empty result image.
	Image blend {width, height};

	// Add layers.
	for (auto argi {3}; argi < argc; ++argi) {
		// Read image.
		Image const layer {argv[argi]};

		// Blend pixels using the over operator.
		for (ImageSize y {0}; y < std::min(height, layer.get_height()); ++y) {
			for (ImageSize x {0}; x < std::min(width, layer.get_width()); ++x) {
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
