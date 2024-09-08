#include "common.hpp"


/// Assemble PNG files into a single layered fragment buffer.
/// Expected arguments: <width> <height> [<image>]...
auto main(int argc, char* argv[]) -> int {
	using namespace deep_icet;
	return try_main([&]() {

	// Parse output size.
	IceTSizeType width, height;

	if (argc < 3
			or (width  = atoi(argv[1])) == 0
			or (height = atoi(argv[2])) == 0
			) {
		std::cerr << log_sev_fatal << "Invalid or missing arguments.\n"
		             "Usage: " << argv[0] << " <width> <height> [<image>]...\n";
		return EXIT_FAILURE;
		}


	UniqueSpan<InputLayer> const in_layers {int_cast<std::size_t>(argc - 3)};

	for (int i {0}; i < argc - 3; ++i) {
		in_layers.span()[i] = {argv[i + 3], static_cast<float>(i) / (argc - 3)};
		}

	// Construct a raw layered image from input images.
	RawImage const out_buffer {width, height, in_layers.span()};

	// Output the image.
	out_buffer.write(freopen(nullptr, "wb", stdout));
	return EXIT_SUCCESS;
	});
	}
