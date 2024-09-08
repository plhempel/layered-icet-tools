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
	RawImage const in_buffer {freopen(nullptr, "rb", stdin)};

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
	for (IceTSizeType frag_idx {0}; frag_idx < in_buffer.num_fragments();) {
		// Leave room to count the active fragments in this pixel.
		auto& num_frags {out.push<IceTLayerCount>(0)};

		// For each layer:
		auto const frag_end {frag_idx + in_buffer.num_layers()};

		for (; frag_idx != frag_end; ++frag_idx) {
			auto const& in_color {in_buffer.color()[frag_idx]};
			auto const  alpha    {in_color[color::alpha_channel]};

			// Copy and count active fragments.
			if (alpha != 0) {
				out.push(in_color);
				out.push(in_buffer.depth()[frag_idx]);
				++num_frags;
				}}

		if (num_frags == 0) {
			// No need to store fragment count for inactive pixels.
			out.ptr -= sizeof(IceTLayerCount);

			// Run lengths are stores before every inactive run.
			if (prev_pixel_active) {
				runlengths        = &out.push(RunLengths{});
				prev_pixel_active = false;
				}

			// Count inactive pixels.
			runlengths->inactive += 1;
			}
		else {
			// Count active pixels and fragments per run.
			runlengths->active    += 1;
			runlengths->fragments += num_frags;
			prev_pixel_active      = true;
			}}

	// Store final image size.
	auto const size {out.ptr - out_buffer.data()};
	reinterpret_cast<IceTInt32*>(out_buffer.data())[6] = size;

	// Output result image.
	write_binary(std::as_bytes(out_buffer.span()).subspan(0, size), fdopen(ctx.stdout(), "wb"));
	return EXIT_SUCCESS;
	});
	}
