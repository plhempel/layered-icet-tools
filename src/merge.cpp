#include "common.hpp"


/// Combine multiple raw layered fragments buffers into one by merging the fragments lists at each
/// pixel in order.
/// Arguments: <width> <height> [<color> <depth>]...
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
		             "Usage: " << argv[0] << " <width> <height> [<color> <depth>]...\n";
		return EXIT_FAILURE;
		}

	// Read input images.
	auto const            num_images {int_cast<std::size_t>(argc - 3) / 2};
	std::vector<RawImage> in_buffers;
	in_buffers.reserve(num_images);

	for (std::size_t i {0}; i < num_images; ++i) {
		std::span const files {&argv[3 + 2*i], 2};
		in_buffers.emplace_back(width, height, fopen(files[0], "rb"), fopen(files[1], "rb"));
		}

	// Merge images.
	RawImage out_buffer {width, height, in_buffers};

	// Output result image.
	out_buffer.write(freopen(nullptr, "wb", stdout));
	return EXIT_SUCCESS;
	});
	}
