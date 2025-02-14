#include "common.hpp"


namespace {

using namespace layered_icet;

/// Adapts a color buffer
struct IcetToPng : png::generator<PngPixel, IcetToPng> {
	IceTUByte* data;

	[[nodiscard]] IcetToPng(IceTImage image, png::image_info& info)
		: generator(info)
		, data {icetImageGetColorub(image)}
		{}

	auto get_next_row(PngSize row) -> png::byte* {
		return data + row * get_info().get_width() * 4;
		}

	};

} // namespace


/// Convert a non-layered `IceTImage` to PNG.
auto main(int argc, char* argv[]) -> int {
	return try_main([&]() {

	// IceT setup.
	Context ctx {&argc, &argv};

	// This program is not distributed.
	if (ctx.proc_rank() != 0) {
		return EXIT_SUCCESS;
		}

	// Read input image.
	auto       in_buffer {read_all(freopen(nullptr, "rb", stdin))};
	auto const in_image  {icetImageUnpackageFromReceive(in_buffer.data())};

	// Create PNG metadata.
	png::image_info info;
	info.set_width(icetImageGetWidth(in_image));
	info.set_height(icetImageGetHeight(in_image));
	info.set_bit_depth(Png::traits::get_bit_depth());
	info.set_color_type(Png::traits::get_color_type());

	// Write output image to the real stdout, then redirect it to stderr again for IceT's shutdown
	// message.
	ctx.restore_stdout();
	IcetToPng{in_image, info}.write(std::cout);
	ctx.stdout_to_stderr();

	return EXIT_SUCCESS;
	});
	}
