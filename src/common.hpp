#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <IceT.h>
#include <IceTMPI.h>

#include <png.hpp>


namespace deep_icet {

// Logging constants.
std::string_view constexpr log_sev_info  {      "\x1b[1m[info]\x1b[m  "};
std::string_view constexpr log_sev_warn  {   "\x1b[1;33m[warn]\x1b[m  "};
std::string_view constexpr log_sev_error {   "\x1b[1;31m[error]\x1b[m "};
std::string_view constexpr log_sev_fatal {"\x1b[1;30;41m[fatal]\x1b[m "};

std::string_view constexpr log_tag_egl    {"\x1b[1m[egl]\x1b[m "};
std::string_view constexpr log_tag_glfw   {"\x1b[1m[glfw]\x1b[m "};
std::string_view constexpr log_tag_opengl {"\x1b[1m[opengl]\x1b[m "};

// Image format.
using Channel   = uint8_t;
using Pixel     = png::basic_rgba_pixel<Channel>;
using PixelData = std::array<Channel, Pixel::traits::channels>;
using Image     = png::image<Pixel, png::solid_pixel_buffer<Pixel>>;
using ImageSize = decltype(std::declval<Image>().get_width());

auto constexpr alpha_channel {Pixel::traits::channels - 1};
auto constexpr channel_max   {std::numeric_limits<Channel>::max()};


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
		if (_handle == src._handle) {
			return *this;
			}

		this->~Handle();
		_handle     = src._handle;
		src._handle = {};
		return *this;
		}

	~Handle() noexcept {
		if (_handle != THandle{}) {
			TDeleter{}(std::move(_handle));
			_handle = {};
			}}


	[[nodiscard]] auto constexpr operator==(Handle const& rhs) const noexcept -> bool {
		return _handle = rhs._handle;
		}

	/// Return the raw handle.
	[[nodiscard]] constexpr auto handle() const noexcept -> THandle const& {
		return _handle;
		}

protected:
	THandle _handle {};

	[[nodiscard]] constexpr explicit Handle(THandle&& handle) noexcept
		: _handle {std::move(handle)}
		{}

	};


/// Convenience wrappers for MPI.
namespace mpi {

/// Return the message associated with an MPI error code.
inline auto error_message(int const error_code) noexcept -> std::string {
	// Allocate memory to hold the message.
	std::string msg (MPI_MAX_ERROR_STRING, '\0');
	int length;

	// Retrieve the message.
	if (MPI_Error_string(error_code, msg.data(), &length)) {
		// If no message could be found, return the error code directly.
		return std::to_string(error_code);
		}

	// Trim excess characters.
	msg.resize(length);

	return msg;
	}


/// An RAII handle to the MPI execution environment.
struct Runtime {
	/// MPI initialization.
	[[nodiscard]] Runtime(int* argc = nullptr, char*** argv = nullptr) {
		if (auto const error = MPI_Init(argc, argv)) {
			throw std::runtime_error(concat("Could not initialize MPI: ", error_message(error)));
			}}

	Runtime(Runtime const&) = delete;
	Runtime(Runtime&&) = delete;

	auto operator=(Runtime const&) = delete;
	auto operator=(Runtime&&) = delete;

	/// MPI cleanup,
	~Runtime() noexcept {
		MPI_Finalize();
		}

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
} // namespace deep_icet
