#include "common.hpp"


namespace {

/// Helper for writing objects' binary representations in sequence.
struct BinaryWriter {
	/// Current position.
	std::byte* ptr;

	/// Write a value at the current position, advance past it, then return a reference to the
	/// output.
	template<typename T>
	constexpr auto push(T const& value) noexcept -> T& {
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


/// Compress a layered fragment buffer into a layered `IceTSparseImage`.
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

	// Allocate result image.
	UniqueSpan<std::byte> const out_buffer {int_cast<std::size_t>(icetSparseLayeredImageBufferSize(
			in_buffer.width(),
			in_buffer.height(),
			in_buffer.num_layers()
			))};
	icetSparseLayeredImageAssignBuffer(
			out_buffer.data(),
			in_buffer.width(),
			in_buffer.height()
			);

	// Compress input layers into a single sparse image.
	BinaryWriter out               {out_buffer.data() + 7 * sizeof(IceTInt32)};
	auto*        runlengths        {&out.push<RunLengths>({})};
	auto         prev_pixel_active {false};

	// For each pixel:
	for (IceTSizeType pixel_start {0};
	     pixel_start < in_buffer.num_fragments();
	     pixel_start += in_buffer.num_layers()
	     ) {
		auto const& in_color {in_buffer.color()[pixel_start]};

		// Handle inactive pixels.
		if (in_color[color::alpha_channel] == 0) {
			// Run lengths are stored before every inactive run.
			if (prev_pixel_active) {
				runlengths        = &out.push(RunLengths{});
				prev_pixel_active = false;
				}

			// Count inactive pixels.
			runlengths->inactive += 1;
			continue;
			}

		// Leave room to count the active fragments in this pixel.
		auto& num_frags {out.push<IceTLayerCount>(1)};

		// Copy the first fragment.
		out.push(in_color);
		out.push(in_buffer.depth()[pixel_start]);

		// Process the remaining layers.
		for (IceTLayerCount layer {1}; layer < in_buffer.num_layers(); ++layer) {
			auto const& in_color {in_buffer.color()[pixel_start + layer]};

			// Active fragments must come before inactive ones.
			if (in_color[color::alpha_channel] == 0) {
				break;
				}

			// Copy and count active fragments.
			out.push(in_color);
			out.push(in_buffer.depth()[pixel_start + layer]);
			++num_frags;
			}

		// Count active pixels and fragments per run.
		runlengths->active    += 1;
		runlengths->fragments += num_frags;
		prev_pixel_active      = true;
		}

	// Store final image size.
	auto const size {out.ptr - out_buffer.data()};
	reinterpret_cast<IceTInt32*>(out_buffer.data())[6] = size;

	// Output result image.
	write_binary(std::as_bytes(out_buffer.span()).subspan(0, size), fdopen(ctx.stdout(), "wb"));
	return EXIT_SUCCESS;
	});
	}
