#include "common.hpp"


/// Blend a layered fragment buffer, back to front, into a regular `IceTImage`.
/// Arguments: <width> <height>
auto main(int argc, char* argv[]) -> int {
	using namespace layered_icet;
	return try_main([&]() {

	// IceT setup.
	Context ctx {&argc, &argv};

	// This program is not distributed.
	if (ctx.proc_rank() != 0) {
		return EXIT_SUCCESS;
		}

	// Parse output size.
	IceTSizeType width, height;

	if (argc < 3
			or (width  = atoi(argv[1])) == 0
			or (height = atoi(argv[2])) == 0
			) {
		std::cerr << log_sev_fatal << "Invalid or missing arguments.\n"
		             "Usage: " << argv[0] << " <width> <height>\n";
		return EXIT_FAILURE;
		}

	// Read input.
	RawImage in_buffer {width, height, freopen(nullptr, "rb", stdin)};

	// Allocate result image.
	auto const out_image {icetGetStateBufferImage(
			ICET_RENDER_BUFFER,
			in_buffer.width(),
			in_buffer.height()
			)};

	// Blend fragments.
	auto in_pixel {in_buffer.color().begin()};

	// For each pixel:
	for (auto& out_color : std::span{
			reinterpret_cast<Color*>(icetImageGetColorVoid(out_image, nullptr)),
			int_cast<std::size_t>(icetImageGetNumPixels(out_image))
			}) {
		// Initialize the output color to a black background.
		out_color = {0, 0, 0, 0};

		// Iterate fragments back to front.
		for (IceTSizeType layer_idx {in_buffer.num_layers()}; layer_idx-- > 0;) {
			auto const in_color     {in_pixel[layer_idx]};
			auto const transparency {color::channel_max - in_color[color::alpha_channel]};

			// Blend color using the over-operator.
			for (std::size_t i {0}; i < in_color.size(); ++i) {
				out_color[i] = out_color[i] * transparency / color::channel_max + in_color[i];
				}}

		// Advance input iterator.
		in_pixel += in_buffer.num_layers();
		}

	// Output result image.
	icetImageAdjustForOutput(out_image);
	write_image(out_image, fdopen(ctx.stdout(), "wb"));
	return EXIT_SUCCESS;
	});
	}
