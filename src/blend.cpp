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
	using Pixel   = png::rgba_pixel;
	using Channel = png::pixel_traits<Pixel>::component_type;

	// Initialize the output image to the first layer.
	png::image<Pixel> blend  {argv[1]};
	auto const        width  {blend.get_width()};
	auto const        height {blend.get_height()};

	// Add remaining layers behind.
	for (auto const* arg_it {&argv[2]}; arg_it < &argv[argc]; ++arg_it) {
		// Read image.
		png::image<Pixel> layer {*arg_it};

		// Ensure all layers have the same size.
		if (layer.get_width() != width or layer.get_height() != height) {
			std::cerr << log_sev_warn << "Skipping " << *arg_it
			          << ", which has a different size than the first layer.";
			continue;
			}

		// Blend pixels using the over operator.
		for (auto y {0}; y < height; ++y) {
			for (auto x {0}; x < blend.get_width(); ++x) {
				auto&       blend_pixel {blend[y][x]};
				auto const& layer_pixel {layer[y][x]};

				auto*       blend_channel_it {reinterpret_cast<Channel*      >(&blend_pixel)};
				auto const* layer_channel_it {reinterpret_cast<Channel const*>(&blend_pixel)};

				auto constexpr channel_max  {std::numeric_limits<Channel>::max()};
				auto const     transparency {channel_max - blend_pixel.alpha};

				// Blend color channels scaled by opacity.
				while (blend_channel_it != &blend_pixel.alpha) {
					*blend_channel_it += (transparency * *layer_channel_it * blend_pixel.alpha)
					                     / channel_max;

					++blend_channel_it;
					++layer_channel_it;
					}

				// Blend alpha channels.
				*blend_channel_it += (transparency * *layer_channel_it) / channel_max;
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
