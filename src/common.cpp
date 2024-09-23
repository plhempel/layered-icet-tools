#include "common.hpp"


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
		, _color_buffer {std::make_unique<Color[]>(num_fragments())}
		, _depth_buffer {std::make_unique<Depth[]>(num_fragments())}
		{
	// For each layer:
	for (std::size_t layer_idx {0}; layer_idx < layers.size(); ++layer_idx) {
		Png const  png        {layers[layer_idx].path};
		auto const png_width  {int_cast<IceTSizeType>(png.get_width())};
		auto const png_height {int_cast<IceTSizeType>(png.get_height())};

		// For each pixel:
		for (IceTSizeType y {0}; y < std::min(height, png_height); ++y) {
			for (IceTSizeType x {0}; x < std::min(width, png_width); ++x) {
				// Copy color, scaled by alpha,
				auto const& color   {reinterpret_cast<Color const&>(png[y][x])};
				auto const  alpha   {color[color::alpha_channel]};
				auto const  out_idx {(y * width + x) * _num_layers + layer_idx};

				for (std::size_t i {0}; i < color.size(); ++i) {
					_color_buffer[out_idx][i] = color[i] * alpha / color::channel_max;
					}

				_color_buffer[out_idx][color::alpha_channel] = alpha;

				// Set depth, with empty fragments marked as background.
				_depth_buffer[out_idx] = alpha == 0 ? 1 : layers[layer_idx].depth;
				}}}}

RawImage::RawImage(FILE* const in) {
	// Read size.
	read_binary(in, std::span{&_width, 3});

	// Allocate buffers.
	_color_buffer = std::make_unique<Color[]>(num_fragments());
	_depth_buffer = std::make_unique<Depth[]>(num_fragments());

	// Read fragment data.
	read_binary(in, std::span{_color_buffer.get(), static_cast<std::size_t>(num_fragments())});
	read_binary(in, std::span{_depth_buffer.get(), static_cast<std::size_t>(num_fragments())});
	}

auto RawImage::write(FILE* out) const -> void {
	write_binary(std::span{&_width, 3}, out);
	write_binary(color(), out);
	write_binary(depth(), out);
	}

} // namespace deep_icet
