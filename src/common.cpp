#include "common.hpp"

#include <algorithm>
#include <numeric>


namespace deep_icet {

namespace mpi {

auto error_message(int const error_code) noexcept -> std::string {
	// Allocate memory to hold the message.
	std::string msg    (MPI_MAX_ERROR_STRING, '\0');
	int         length {0};

	// Retrieve the message.
	if (MPI_Error_string(error_code, msg.data(), &length)) {
		// If no message could be found, return the error code directly.
		return std::to_string(error_code);
		}

	// Trim excess characters.
	msg.resize(length);

	return msg;
	}

} // namespace mpi


Context::Context(int* argc, char*** argv)
	: _mpi {argc, argv}
	{
	// Redirect stdout to stderr so IceT's diagnostics do not interfere with result output.
	stdout_to_stderr();

	// Basic IceT configuration.
	icetDiagnostics(ICET_DIAG_FULL);

	icetCompositeMode(ICET_COMPOSITE_MODE_BLEND);

	icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
	icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);
	}

auto Context::stdout_to_stderr() noexcept -> void {
	if (_stdout == STDOUT_FILENO) {
		fflush(::stdout);
		_stdout = dup(STDOUT_FILENO);
		dup2(STDERR_FILENO, STDOUT_FILENO);
		}}

auto Context::restore_stdout() noexcept -> void {
	if (_stdout != STDOUT_FILENO) {
		fsync(_stdout);
		dup2(_stdout, STDOUT_FILENO);
		_stdout = STDOUT_FILENO;
		}}


/// Read a given number of bytes from a binary file.
template <typename T>
auto read_binary(FILE* in, std::span<T> buffer) -> void {
	while (not buffer.empty()) {
		buffer = buffer.subspan(fread(buffer.data(), sizeof(T), buffer.size(), in));

		if (feof(in) or ferror(in)) {
			throw std::runtime_error{"Could not read requested amount of data"};
			}}}

[[nodiscard]] auto read_all(FILE* in, std::size_t size_hint) -> std::vector<std::byte> {
	std::vector<std::byte> buffer (size_hint);
	std::size_t            size   {0};

	while (not feof(in)) {
		if (size == buffer.size()) {
			buffer.resize(size * 2);
			}

		size += fread(&buffer[size], sizeof(std::byte), buffer.size() - size, in);

		if (ferror(in)) {
			throw std::runtime_error("Error reading file");
			}}

	buffer.resize(size);
	return buffer;
	}

namespace {

// Helper for implementation of `write_image`.
template<typename TImage, typename TPackage>
auto write_image_impl(TImage const image, TPackage&& package, FILE* const out) -> void {
	IceTVoid*    data {nullptr};
	IceTSizeType size {0};

	package(image, &data, &size);

	write_binary(std::span(reinterpret_cast<std::byte const*>(data), size), out);
	}

} // namespace

auto write_image(IceTImage const image, FILE* const out) -> void {
	write_image_impl(image, icetImagePackageForSend, out);
	}

auto write_image(IceTSparseImage const image, FILE* const out) -> void {
	write_image_impl(image, icetSparseImagePackageForSend, out);
	}


RawImage::RawImage(
		IceTSizeType const          width,
		IceTSizeType const          height,
		std::span<InputLayer const> layers
		)
	: _width        {width}
	, _height       {height}
	, _num_layers   {int_cast<IceTSizeType>(layers.size())}
	, _buffer       {num_fragments() * (sizeof(Color) + sizeof(Depth)), std::byte{0}}
	, _depth_buffer {reinterpret_cast<Depth*>(_buffer.data() + num_fragments() * sizeof(Color))}
	{
	// Store the number of fragments at each pixel of the output image.
	auto layers_at {std::vector<IceTLayerCount>(num_pixels(), 0)};

	// For each input image:
	for (auto const& layer : layers) {
		Png const  png        {layer.path};
		auto const png_width  {int_cast<IceTSizeType>(png.get_width())};
		auto const png_height {int_cast<IceTSizeType>(png.get_height())};

		// For each row of the input image:
		for (IceTSizeType y {0}; y < std::min(height, png_height); ++y) {
			IceTSizeType x {0};

			// Copy pixels from the input image.
			for (; x < std::min(width, png_width); ++x) {
				// Skip empty pixels.
				auto const& color {reinterpret_cast<Color const&>(png[y][x])};
				auto const  alpha {color[color::alpha_channel]};

				if (alpha == 0) {
					continue;
					}

				// Copy color, scaled by alpha,
				auto const pixel_idx {y * width + x};
				auto const out_idx   {pixel_idx * _num_layers + layers_at[pixel_idx]};

				for (std::size_t i {0}; i < color.size(); ++i) {
					color_buffer(out_idx)[i] = color[i] * alpha / color::channel_max;
					}

				color_buffer(out_idx)[color::alpha_channel] = alpha;

				// Set depth.
				_depth_buffer[out_idx] = layer.depth;

				// Count active fragments.
				++layers_at[pixel_idx];
				}}}}

RawImage::RawImage(
		IceTSizeType const        width,
		IceTSizeType const        height,
		std::span<RawImage const> sources
		)
	: _width        {width}
	, _height       {height}
	, _num_layers   {std::accumulate(
		sources.begin(),
		sources.end(),
		0,
		[](auto const accum, RawImage const& img) {
			return accum + img.num_layers();
			}
		)}
	, _buffer       {num_fragments() * (sizeof(Color) + sizeof(Depth)), std::byte{0}}
	, _depth_buffer {reinterpret_cast<Depth*>(_buffer.data() + num_fragments() * sizeof(Color))}
	{
	// Stores all fragments at the current pixel.
	std::vector<Fragment> frags;
	frags.reserve(_num_layers);

	// For each pixel:
	for (IceTSizeType y {0}; y < height; ++y) {
		for (IceTSizeType x {0}; x < width; ++x) {
			frags.clear();

			// Gather the fragments from all input images.
			for (auto const& img : sources) {
				if (x < img.width() and y < img.height()) {
					for (auto layer {0}; layer < img.num_layers(); ++layer) {
						auto const idx {(y * img.width() + x) * img.num_layers() + layer};

						// Active fragments must come before inactive ones.
						if (img.color()[idx][color::alpha_channel] == 0) {
							break;
							}

						frags.push_back({img.color()[idx], img.depth()[idx]});
						}}}

			// Sort fragments by depth.
			std::sort(frags.begin(), frags.end(), [](Fragment const& lhs, Fragment const& rhs) {
					return std::less{}(lhs.depth , rhs.depth);
					});

			// Copy fragments to the image buffer in order.
			auto const pixel {(y * _width + x) * _num_layers};

			for (IceTLayerCount layer {0}; layer < frags.size(); ++layer) {
				color_buffer(pixel + layer)  = frags[layer].color;
				_depth_buffer[pixel + layer] = frags[layer].depth;
				}}}}

RawImage::RawImage(IceTSizeType width, IceTSizeType height, FILE* const in)
	: _width  {width}
	, _height {height}
	{
	auto const layer_size {width * height * (sizeof(Color) + sizeof(Depth))};

	// Read data.
	_buffer = read_all(in, layer_size);

	// Calculate number of layers and verify size.
	_num_layers = _buffer.size() / layer_size;

	if (_buffer.size() % layer_size != 0) {
		throw std::runtime_error{"Buffer size does not match the expected number of pixels"};
		}

	// Calculate depth buffer offset.
	_depth_buffer = reinterpret_cast<Depth*>(_buffer.data() + num_fragments() * sizeof(Color));
	}

RawImage::RawImage(
		IceTSizeType width,
		IceTSizeType height,
		FILE* const  color_file,
		FILE* const  depth_file
		)
	: _width  {width}
	, _height {height}
	{
	auto const layer_size {width * height * sizeof(Color)};

	// Read color data.
	_buffer = read_all(color_file, layer_size);

	// Calculate number of layers and verify size.
	_num_layers = _buffer.size() / layer_size;

	if (_buffer.size() % layer_size != 0) {
		throw std::runtime_error{"Buffer size does not match the expected number of pixels"};
		}

	// Read depth data.
	_buffer.resize(_buffer.size() + num_pixels() * sizeof(Depth));
	_depth_buffer = reinterpret_cast<Depth*>(_buffer.data() + num_fragments() * sizeof(Color));
	read_binary(
			depth_file,
			std::span<Depth>{_depth_buffer, static_cast<std::size_t>(num_fragments())}
			);
	}

auto RawImage::write(FILE* out) const -> void {
	write_binary(color(), out);
	write_binary(depth(), out);
	}

} // namespace deep_icet
