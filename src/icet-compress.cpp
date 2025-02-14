#include "common.hpp"


/// Use IceT to compress a layered fragment buffer into a layered `IceTSparseImage`.
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
	RawImage const in_buffer {width, height, freopen(nullptr, "rb", stdin)};

	auto const in_image {icetGetStatePointerLayeredImage(
			ICET_RENDER_BUFFER,
			in_buffer.width(),
			in_buffer.height(),
			in_buffer.num_layers(),
			in_buffer.color().data(),
			in_buffer.depth().data()
			)};

	// Allocate output image.
	auto out_image {icetGetStateBufferSparseLayeredImage(
			ICET_SPARSE_TILE_BUFFER,
			in_buffer.width(),
			in_buffer.height(),
			in_buffer.num_layers()
			)};

	// Compress image.
	icetCompressImage(in_image, out_image);

	// Output result image.
	write_image(out_image, fdopen(ctx.stdout(), "wb"));

	return EXIT_SUCCESS;
	});
	}
