#include <array>
#include <concepts>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <IceT.h>
#include <IceTMPI.h>
#include <mpi.h>

#include <png.hpp>

#include "common.hpp"


namespace deep_icet {

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
auto error_message(int const error_code) noexcept -> std::string {
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

/// Compose PNG images front to back using IceT.
/// Expected arguments: <width> <height> [<rank>:<image>]...
auto main(int argc, char* argv[]) -> int try {
	using namespace std::string_literals;
	using namespace deep_icet;

	// Parse output size.
	ImageSize width, height;

	if (argc < 3
			or (width  = atoi(argv[1])) == 0
			or (height = atoi(argv[2])) == 0
			) {
		std::cerr << log_sev_fatal << "Invalid or missing arguments.\n"
		             "Usage: " << argv[0] << " <width> <height> [<rank>:<image>]...\n";
		return EXIT_FAILURE;
		}

	// Initialize MPI and retrieve configuration.
	mpi::Runtime mpi     {&argc, &argv};
	auto const   mpi_com {MPI_COMM_WORLD};

	int proc_rank;
	MPI_Comm_rank(mpi_com, &proc_rank);

	int num_procs;
	MPI_Comm_size(mpi_com, &num_procs);

	if (proc_rank == 0) {
		std::clog << log_sev_info << "Using " << num_procs << " processes.\n";
		}

	// Initialize and configure IceT.
	icet::Communicator com {mpi_com};
	icet::Context      ctx {com};

	icetDiagnostics(ICET_DIAG_FULL);

	icetStrategy(ICET_STRATEGY_SEQUENTIAL);
	icetSingleImageStrategy(ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC);
	icetCompositeMode(ICET_COMPOSITE_MODE_BLEND);

	icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
	icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);

	icetResetTiles();
	icetAddTile(0, 0, width, height, 0);

	// Determine local input layers (layers assigned to this rank).
	struct LayerInfo {
		char const* path;
		int         depth;
		};

	std::vector<LayerInfo> local_layers;
	local_layers.reserve(argc - 3);

	for (auto argi {3}; argi < argc; ++argi) {
		char* parse_ptr;

		if (std::strtol(argv[argi], &parse_ptr, 10) == proc_rank) {
			if (parse_ptr == argv[argi] or *parse_ptr != ':') {
				std::cerr << log_sev_error << "Argument " << argv[argi]
				          << " does not match the expected pattern <rank>:<image>.\n";
				continue;
				}

			++parse_ptr;
			local_layers.emplace_back(parse_ptr, argi - 3);
			}}

	auto  const num_local_layers {std::max<std::size_t>(local_layers.size(), 1)};
	float const max_layers       {static_cast<float>(argc - 3)};

	// Combine local layers into a Layered Depth Image (LDI).
	struct LdiFragment {
		PixelData color;
		float     depth;
		};

	std::vector<LdiFragment> local_ldi (width * height * num_local_layers);

	for (std::size_t layer_idx {0}; layer_idx < local_layers.size(); ++layer_idx) {
		Image const layer {local_layers[layer_idx].path};

		for (ImageSize y {0}; y < std::min(height, layer.get_height()); ++y) {
			for (ImageSize x {0}; x < std::min(width, layer.get_width()); ++x) {
				auto& pixel      {reinterpret_cast<PixelData const&>(layer[y][x])};
				auto& fragment   {local_ldi[(y * width + x) * local_layers.size() + layer_idx]};
				auto const alpha {pixel[alpha_channel]};

				for (auto i {0}; i < alpha_channel; ++i) {
					fragment.color[i] = pixel[i] * alpha / channel_max;
					}

				fragment.color[alpha_channel] = alpha;
				fragment.depth                = alpha == 0 ?
					1 : local_layers[layer_idx].depth / max_layers;
				}}}

	// Store input data.
	std::ofstream out_stream {concat("out/input-", proc_rank, ".raw"),  std::ios_base::binary};
	out_stream.write(reinterpret_cast<char const*>(
		local_ldi.data()),
		local_ldi.size() * sizeof(LdiFragment)
		);
	out_stream.close();

	// Composite images from all ranks.
	std::array<IceTFloat, 4> const background {0, 0, 0, 0};
	auto const result_image {icetCompositeImageLayered(
			local_ldi.data(),
			local_layers.size(),
			nullptr,
			nullptr,
			nullptr,
			background.data()
			)};

	// Output result image as PNG on rank zero.
	struct ImageRows : png::generator<png::rgba_pixel, ImageRows> {
		IceTUByte* data;

		[[nodiscard]] ImageRows(IceTImage image, png::image_info& info)
			: generator(info)
			, data {icetImageGetColorub(image)}
			{}

		auto get_next_row(ImageSize row) -> png::byte* {
			return data + row * get_info().get_width() * 4;
			}

		};

	if (proc_rank == 0) {
		png::image_info info;
		info.set_width(width);
		info.set_height(height);
		info.set_bit_depth(8);
		info.set_color_type(png::color_type_rgba);

		ImageRows{result_image, info}.write(std::cout);
		}

	}
	catch (std::exception const& error) {
		std::cerr << deep_icet::log_sev_fatal << error.what() << "\n";
		return EXIT_FAILURE;
		}
	catch (...) {
		std::cerr << deep_icet::log_sev_fatal << "Unknown error\n";
		return EXIT_FAILURE;
		}
