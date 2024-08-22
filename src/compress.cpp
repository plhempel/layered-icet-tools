#include <cstdlib>
#include <iostream>

#include "IceT.h"
#include "IceTDevImage.h"

#include "common.hpp"


namespace {

/// Helper for writing objects' binary representations in sequence.
struct BinaryWriter {
	/// Current position.
	std::byte* ptr;

	/// Write a value at the current position, advance past it, then return a reference to the
	/// output.
	template<typename T>
	constexpr T& push(T const& value) {
		auto& out {reinterpret_cast<T&>(*ptr)};
		out  = value;
		ptr += sizeof(T);
		return out;
		}

	};


/// A set of run lengths in a layered `IceTSparseImage`.
struct RunLengths {
	IceTSizeType inactive  {0};
	IceTSizeType active    {0};
	IceTSizeType fragments {0};
	};

} // namespace


/// Compress PNG files into a layered `IceTSparseImage`.
/// Expected arguments: <width> <height> [<image>]...
auto main(int argc, char* argv[]) -> int try {
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

	// Load input images.
	std::vector<Image> layers;
	layers.reserve(argc - 3);

	for (auto argi {3}; argi < argc ; ++argi) {
		layers.emplace_back(argv[argi]);
		}

	// Set up IceT just enough to use basic image functions.
	mpi::Runtime mpi     {&argc, &argv};
	auto const   mpi_com {MPI_COMM_WORLD};

	icet::Communicator com {mpi_com};
	icet::Context      ctx {com};

	icetDiagnostics(ICET_DIAG_OFF);

	icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
	icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);

	// Allocate result image and set header.
	std::vector<std::byte> out_buffer (
			icetSparseLayeredImageBufferSize(width, height, layers.size()),
			{}
			);
	icetSparseLayeredImageAssignBuffer(out_buffer.data(), width, height);

	// Compress input layers into a single sparse image.
	BinaryWriter out               {out_buffer.data() + 7 * sizeof(IceTInt32)};
	auto*        runlengths        {&out.push<RunLengths>({})};
	auto         prev_pixel_active {false};

	// For each pixel:
	for (ImageSize y {0}; y < height; ++y) {
		for (ImageSize x {0}; x < width; ++x) {
			// Leave room to count the active fragments in this pixel.
			auto& num_frags {out.push<IceTLayerCount>(0)};

			// For each input image:
			for (std::size_t layer_idx {0}; layer_idx < layers.size(); ++layer_idx) {
				auto const& layer {layers[layer_idx]};

				// Nothing to do for inactive pixels.
				if (x >= layer.get_width() or y >= layer.get_height()) {
					continue;
					}

				auto const& color {reinterpret_cast<PixelData const&>(layer[y][x])};
				auto const  alpha {color[alpha_channel]};

				if (alpha == 0) {
					continue;
					}

				// Count the fragment.
				++num_frags;

				// Write depth and color scaled by alpha.
				for (auto channel {0}; channel < alpha_channel; ++channel) {
					out.push(static_cast<Channel>((color[channel] * alpha) / channel_max));
					}

				out.push(alpha);
				out.push(static_cast<float>(layer_idx) / layers.size());
				}

			if (num_frags == 0) {
				// No need to store fragment count for inactive pixels.
				out.ptr -= sizeof(IceTLayerCount);

				// Run lengths are stores before every inactive run.
				if (prev_pixel_active) {
					runlengths     = &out.push(RunLengths{});
					prev_pixel_active = false;
					}

				// Count inactive pixels.
				runlengths->inactive += 1;
				}
			else {
				// Count active pixels and fragments per run.
				runlengths->active    += 1;
				runlengths->fragments += num_frags;
				prev_pixel_active         = true;
				}}}

	// Store the final image size.
	auto const size {out.ptr - out_buffer.data()};
	reinterpret_cast<IceTInt32*>(out_buffer.data())[6] = size;

	// Print the compressed image in binary format.
	std::cout.write(reinterpret_cast<char const*>(out_buffer.data()), size);
	}
	catch (std::exception const& error) {
		std::cerr << deep_icet::log_sev_fatal << error.what() << "\n";
		return EXIT_FAILURE;
		}
	catch (...) {
		std::cerr << deep_icet::log_sev_fatal << "Unknown error\n";
		return EXIT_FAILURE;
		}
