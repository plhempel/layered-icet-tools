#include "common.hpp"


/// Use IceT to compress a layered fragment buffer into a layered `IceTSparseImage`.
auto main(int argc, char* argv[]) -> int {
	using namespace deep_icet;
	return try_main([&]() {

	// IceT setup.
	Context ctx {&argc, &argv};

	// This program is not distributed.
	if (ctx.proc_rank() != 0) {
		return EXIT_SUCCESS;
		}

	// Read input.
	FragmentBuffer const in_buffer {freopen(nullptr, "rb", stdin)};

	auto const in_image {icetGetStatePointerLayeredImage(
			ICET_RENDER_BUFFER,
			in_buffer.width(),
			in_buffer.height(),
			in_buffer.num_layers(),
			in_buffer.fragments().data()
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
