#include "common.hpp"


/// Use IceT to decompress an `IceTSparseImage` into a regular `IceTImage`.
auto main(int argc, char* argv[]) -> int {
	using namespace layered_icet;
	return try_main([&]() {

	// IceT setup.
	Context ctx {&argc, &argv};

	// This program is not distributed.
	if (ctx.proc_rank() != 0) {
		return EXIT_SUCCESS;
		}

	// Read input image.
	auto       in_buffer {read_all(freopen(nullptr, "rb", stdin))};
	auto const in_image  {icetSparseImageUnpackageFromReceive(in_buffer.data())};

	// Allocate output image.
	auto const out_image {icetGetStateBufferImage(
			ICET_RENDER_BUFFER,
			icetSparseImageGetWidth(in_image),
			icetSparseImageGetHeight(in_image)
			)};

	// Decompress image.
	icetDecompressImage(in_image, out_image);

	// Output result image.
	icetImageAdjustForOutput(out_image);
	write_image(out_image, fdopen(ctx.stdout(), "wb"));
	return EXIT_SUCCESS;
	});
	}
