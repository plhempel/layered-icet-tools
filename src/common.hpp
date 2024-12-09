#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>

#include <IceT.h>
#include <IceTDevCommunication.h>
#include <IceTDevImage.h>
#include <IceTMPI.h>

#include <png.hpp>


#if defined(__GNUC__) and not defined(__clang__) and __GNUC__ < 14
	#pragma GCC diagnostic ignored "-Wsubobject-linkage"
	#endif


/// Functionality used by multiple deep-icet executables.
namespace deep_icet {

// Logging constants.
constexpr std::string_view log_sev_info  {      "\x1b[1m[info]\x1b[m  "};
constexpr std::string_view log_sev_warn  {   "\x1b[1;33m[warn]\x1b[m  "};
constexpr std::string_view log_sev_error {   "\x1b[1;31m[error]\x1b[m "};
constexpr std::string_view log_sev_fatal {"\x1b[1;30;41m[fatal]\x1b[m "};


// Image format.
namespace color {
using Channel = uint8_t;

constexpr uint8_t alpha_channel {3};
constexpr Channel channel_max   {std::numeric_limits<Channel>::max()};
};

using Color    = std::array<color::Channel, 4>;
using PngPixel = png::basic_rgba_pixel<color::Channel>;
using Png      = png::image<PngPixel, png::solid_pixel_buffer<PngPixel>>;
using PngSize  = decltype(std::declval<Png>().get_width());
using Depth    = float;

/// A fragment of a layered image.
struct Fragment {
	Color color {0, 0, 0, 0};
	Depth depth {1};
	};


/// Cast between integer types, asserting that the given value can be represented in both.
template<std::integral TTo, std::integral TFrom>
constexpr auto int_cast(TFrom const& value) -> TTo {
	using BitUnion = std::make_unsigned_t<std::common_type_t<TTo, TFrom>>;

	// Assert that only bits shared between both types are set.
	assert(static_cast<BitUnion>(value) < BitUnion{1} << std::min(
			std::numeric_limits<TTo>::digits,
			std::numeric_limits<TFrom>::digits
			));

	return static_cast<TTo>(value);
	}


/// Concatenate arguments into a string using a string stream for formatting.
template<typename... TArgs>
auto concat(TArgs const&... args) -> std::string {
	std::ostringstream result;
	(result << ... << args);
	return std::move(result).str();
	}


/// Base class for an RAII wrapper that uniquely owns a handle.
template<std::regular THandle, std::regular_invocable<THandle&&> TDeleter>
	requires std::default_initializable<TDeleter>
class Handle {
public:
	using RawHandle = THandle;


	[[nodiscard]] constexpr Handle() noexcept = default;

	Handle(Handle const&) = delete;
	constexpr Handle(Handle&& src) noexcept
		: _handle {std::move(src._handle)}
		{
		src._handle = {};
		}

	auto operator=(Handle const&) = delete;
	constexpr auto operator=(Handle&& src) noexcept -> Handle&
		{
		// Prevent deletion on self-assignment.
		if (_handle == src._handle) {
			return *this;
			}

		// Delete our current resource, then take ownership of the given handle.
		this->~Handle();
		_handle     = src._handle;
		src._handle = {};
		return *this;
		}

	~Handle() noexcept {
		TDeleter{}(std::move(_handle));
		}


	[[nodiscard]] auto constexpr operator==(Handle const& rhs) const noexcept -> bool {
		return _handle = rhs._handle;
		}

	/// Return the raw handle.
	[[nodiscard]] constexpr auto handle() const noexcept -> RawHandle const& {
		return _handle;
		}

protected:
	RawHandle _handle {};

	[[nodiscard]] constexpr explicit Handle(RawHandle&& handle) noexcept
		: _handle {std::move(handle)}
		{}

	};


/// Convenience wrappers for MPI.
namespace mpi {

/// Return the message associated with an MPI error code.
auto error_message(int const error_code) noexcept -> std::string;

/// An RAII handle to the MPI execution environment.
struct Environment : Handle<
		std::tuple<>,
		decltype([](auto){MPI_Finalize();})
		> {
	/// MPI initialization.
	[[nodiscard]] Environment(int* argc = nullptr, char*** argv = nullptr) {
		if (auto const error = MPI_Init(argc, argv)) {
			throw std::runtime_error(concat("Could not initialize MPI: ", error_message(error)));
			}}

	};

} // namespace mpi


/// Convenience wrappers for IceT.
namespace icet {

/// RAII handle for an `IceTCommunicator`.
class Communicator : public Handle<
		IceTCommunicator,
		decltype([](IceTCommunicator&& com) {icetDestroyMPICommunicator(std::move(com));})
		> {
public:
	[[nodiscard]] explicit Communicator(MPI_Comm const& mpi_com) noexcept
		: Handle{icetCreateMPICommunicator(mpi_com)}
		{}

	};

/// RAII handle for an `IceTContext`.
class Context : public Handle<
		IceTContext,
		decltype([](IceTContext&& ctx) {icetDestroyContext(std::move(ctx));})
		> {
public:
	[[nodiscard]] explicit Context(Communicator const& com) noexcept
		: Handle{icetCreateContext(com.handle())}
		{}

	};

} // namespace icet


/// Wraps a main function with pretty printing for exceptions.
template<typename Fn>
	requires std::is_invocable_r_v<int, Fn>
auto try_main(Fn&& fn) -> int {
	try {
		return fn();
		}
	catch (std::exception const& error) {
		std::cerr << log_sev_fatal << error.what() << "\n";
		return EXIT_FAILURE;
		}
	catch (...) {
		std::cerr << log_sev_fatal << "Unknown error\n";
		return EXIT_FAILURE;
		}}


/// Provides a basic environment setup for programs using IceT.
class Context {
public:
	[[nodiscard]] Context(int* argc, char*** argv);

	/// Return the number of processes in the global MPI communicator.
	[[nodiscard]] auto num_procs() const noexcept -> int {
		return _com_size;
		}

	/// Return the rank of this process within the global MPI communicator.
	[[nodiscard]] auto proc_rank() const noexcept -> int {
		return _com_rank;
		}

	/// Return a file descriptor referring to `stdout` at the time of construction.
	[[nodiscard]] auto stdout() const noexcept -> int {
		return _stdout;
		}

	/// Redirect `stdout` to `stderr` so IceT's debug messages do not interfere with output data.
	/// Called on construction.
	auto stdout_to_stderr() noexcept -> void;

	/// Replace `stdout` with its original file.
	auto restore_stdout() noexcept -> void;

private:
	mpi::Environment   _mpi;
	icet::Communicator _com      {MPI_COMM_WORLD};
	icet::Context      _icet     {_com};
	int                _com_size {icetCommSize()};
	int                _com_rank {icetCommRank()};
	int                _stdout   {STDOUT_FILENO};
	};


/// Uniquely ownes a contiguous sequence of elements.
template<typename TElem>
class UniqueSpan {
public:
	[[nodiscard]] constexpr UniqueSpan(std::size_t length)
		: _data   {std::make_unique<TElem[]>(length)}
		, _length {length}
		{}

	[[nodiscard]] constexpr auto data() const noexcept -> TElem* {
		return _data.get();
		}

	[[nodiscard]] constexpr auto span() const noexcept -> std::span<TElem> {
		return std::span{_data.get(), _length};
		}

private:
	std::unique_ptr<TElem[]> _data;
	std::size_t              _length;
	};


/// Read the entire contents of a binary file into a buffer.
[[nodiscard]] auto read_all(FILE* in, std::size_t size_hint = 256) -> std::vector<std::byte>;

/// Write a contiguous buffer to a binary file.
template<typename T>
auto write_binary(std::span<T const> buffer, FILE* out) -> void {
	while (not buffer.empty()) {
		buffer = buffer.subspan(fwrite(buffer.data(), sizeof(T), buffer.size(), out));

		if (ferror(out)) {
			throw std::runtime_error("Error writing to file");
			}}}

/// Write a contiguous `IceTImage` or `IceTSparseImage` to a binary file.
auto write_image(IceTImage, FILE* out) -> void;
auto write_image(IceTSparseImage, FILE* out) -> void;


/// Defines input required to construct a layer.
struct InputLayer {
	char const* path;
	Depth       depth;
	};

/// A raw layered image.
/// Can be written to and read from a file.
class RawImage {
public:
	/// Build an image by layering PNGs.
	/// Scales each fragment's color by its alpha value.
	[[nodiscard]] RawImage(
			IceTSizeType                width,
			IceTSizeType                height,
			std::span<InputLayer const> layers
			);
	/// Merge multiple deep images into a single one.
	/// The fragment lists of each pixel are merged in order of depth.
	[[nodiscard]] RawImage(
			IceTSizeType              width,
			IceTSizeType              height,
			std::span<RawImage const> sources
			);
	/// Read an image from a file containing the color buffer followed by the depth buffer.
	[[nodiscard]] RawImage(IceTSizeType width, IceTSizeType height, FILE* in);
	/// Read an image from separate color and depth buffer files.
	[[nodiscard]] RawImage(
			IceTSizeType width,
			IceTSizeType height,
			FILE*        color_file,
			FILE*        depth_file
			);

	[[nodiscard]] constexpr auto width() const noexcept -> IceTSizeType {
		return _width;
		}

	[[nodiscard]] constexpr auto height() const noexcept -> IceTSizeType {
		return _height;
		}

	[[nodiscard]] constexpr auto num_layers() const noexcept -> IceTSizeType {
		return _num_layers;
		}

	[[nodiscard]] auto color() const noexcept -> std::span<Color const> {
		return {
				reinterpret_cast<Color const*>(_buffer.data()),
				static_cast<std::size_t>(num_fragments())
				};
		}

	[[nodiscard]] constexpr auto depth() const noexcept -> std::span<Depth const> {
		return {_depth_buffer, static_cast<std::size_t>(num_fragments())};
		}

	[[nodiscard]] constexpr auto num_pixels() const noexcept -> IceTSizeType {
		return _width * _height;
		}

	[[nodiscard]] constexpr auto num_fragments() const noexcept -> IceTSizeType {
		return num_pixels() * _num_layers;
		}

	/// Write dimensions and fragment data to a binary file.
	auto write(FILE* out) const -> void;

private:
	IceTSizeType           _width        {0};
	IceTSizeType           _height       {0};
	IceTSizeType           _num_layers   {0};
	std::vector<std::byte> _buffer       {};
	Depth*                 _depth_buffer {nullptr};

	[[nodiscard]] auto color_buffer(std::size_t idx = 0) noexcept -> Color& {
		return reinterpret_cast<Color*>(_buffer.data())[idx];
		}

	};

} // namespace deep_icet
