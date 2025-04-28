#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "common.hpp"


namespace {

namespace cron = std::chrono;
namespace fs   = std::filesystem;
using namespace std::string_literals;
using namespace std::string_view_literals;

} // namespace


namespace layered_icet {

using ByteBuffer = std::unique_ptr<std::byte[]>;

enum class ImageType : uint8_t {
	flat,
	layered,
	};

struct Args {
	std::string_view in_dir     {};
	std::string_view dataset    {};
	std::string_view renderer   {};
	IceTSizeType     width      {};
	IceTSizeType     height     {};
	int              num_reps   {};
	IceTLayerCount   num_layers {};
	ImageType        image_type {};

	Args(int argc, char const* argv[], bool print_errors) {
		auto print_usage = [&]() {
			std::clog << "Usage: " << (argc >= 1 ? argv[0] : "icet-benchmark")
			          << " <#repetitions> <input dir> <dataset> <renderer> <width> <height> "
			             "[<#layers>]\n";
			};

		if (argc < 7) {
			if (not print_errors) return;
			std::clog << log_sev_error << "Too few arguments.\n";
			print_usage();
			return;
			}

		renderer = argv[4];

		if (renderer == "convex") {
			image_type = ImageType::flat;
			num_layers = 1;
			}
		else {
			if (argc < 8) {
				if (not print_errors) return;
				std::clog << log_sev_error << "Missing number of layers.\n";
				print_usage();
				return;
				}

			num_layers = atoi(argv[7]);

			if (renderer == "flat") {
				image_type = ImageType::flat;
				}
			else if (renderer == "layered") {
				image_type = ImageType::layered;
				}
			else {
				if (not print_errors) return;
				std::clog << log_sev_error << "Unknown rendering interface '" << renderer
				          << "'. Must be either 'convex', 'flat', or 'layered'.\n";
				return;
				}}

		num_reps = atoi(argv[1]);
		in_dir   = argv[2];
		dataset  = argv[3];
		width    = atoi(argv[5]);
		height   = atoi(argv[6]);
		}

	constexpr auto is_valid() const noexcept -> bool {
		return not in_dir.empty();
		}

	constexpr auto num_fragments() const noexcept -> IceTSizeType {
		return width * height * num_layers;
		}

	};

auto read_binary(std::ifstream& file, std::size_t size) -> ByteBuffer {
	auto buffer {std::make_unique<std::byte[]>(size)};
	file.read(reinterpret_cast<char*>(buffer.get()), size);
	file.close();
	return buffer;
	}

using Duration = cron::milliseconds;

template<typename TFn>
auto time(TFn&& fn) -> std::tuple<Duration, std::invoke_result_t<TFn>> {
	using Clock = cron::steady_clock;

	auto const start_time {Clock::now()};
	auto const result     {fn()};
	return std::make_tuple(
		cron::duration_cast<Duration>(Clock::now() - start_time),
		std::move(result)
		);
	}

} // namespace icet_benchmarks

auto main(int argc, char const* argv[]) -> int {
	using namespace layered_icet;
	return try_main([=]() {

	// Create MPI and IceT context.
	Context ctx {nullptr, nullptr};

	// Parse command line arguments.
	Args const args {argc, argv, ctx.proc_rank() == 0};

	if (not args.is_valid()) {
		return EXIT_FAILURE;
		}

	// Configure IceT.
	icetStrategy(ICET_STRATEGY_SEQUENTIAL);
	icetSingleImageStrategy(ICET_SINGLE_IMAGE_STRATEGY_RADIXK);

	icetDiagnostics(ICET_DIAG_OFF);

	icetAddTile(0, 0, args.width, args.height, 0);

	switch (args.image_type) {
		case ImageType::flat: {
			std::vector<IceTInt> ranks (ctx.num_procs());
			std::iota(ranks.begin(), ranks.end(), 0);
			icetCompositeOrder(ranks.data());
			icetSetDepthFormat(ICET_IMAGE_DEPTH_NONE);
			break;
			}
		case ImageType::layered:
			icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);
			break;
			}

	// Construct path used for both input and output.
	auto subdirs {fs::path{args.dataset} / args.renderer / std::to_string(ctx.num_procs())};

	if (args.renderer != "convex") {
		subdirs += 'x' + std::to_string(args.num_layers);
		}

	auto in_path {fs::path(args.in_dir) / subdirs / ""};

	// Ensure input directory exists.
	if (not fs::is_directory(in_path)) {
		if (ctx.proc_rank() == 0) {
			std::clog << log_sev_error << "Missing directory " << in_path << ".\n";
			}

		return EXIT_FAILURE;
		}

	// Load input data.
	if (ctx.proc_rank() == 0) {
		std::clog << "Loading frame data...\n";
		}

	std::vector<RawImage> frames;
	std::ifstream         in_file;
	auto const            com_rank_str {std::to_string(ctx.proc_rank())};
	auto const            color_suffix {"-"s + com_rank_str + ".color"};

	in_file.exceptions(std::ios_base::goodbit | std::ios_base::badbit);

	for (unsigned fnum = 1;; ++fnum) { // Skip the first frame, since it is empty.
		in_path.replace_filename(std::to_string(fnum) + color_suffix);
		FILE* const color_file {fopen(in_path.c_str(), "rb")};

		// Last frame has been reached.
		if (not color_file) {
			break;
			}

		FILE* depth_file {nullptr};

		if (args.image_type != ImageType::flat) {
			in_path.replace_extension(".depth");
			depth_file = fopen(in_path.c_str(), "rb");
			}

		auto const& frame {frames.emplace_back(args.width, args.height, color_file, depth_file)};

		if (frame.num_layers() != args.num_layers) {
			std::clog << log_sev_error << "Frame #" << fnum << " has " << frame.num_layers()
			                           << " layers, not " << args.num_layers << "\n";
			return EXIT_FAILURE;
			}}

	// When LiV is interrupted, some processes may not have stored the last frame yet.
	// Ensure we only use frames for which all ranks have data, otherwise the program will run
	// indefinitely.
	unsigned num_frames {static_cast<unsigned>(frames.size())};
	MPI_Allreduce(MPI_IN_PLACE, &num_frames, 1, MPI_UNSIGNED, MPI_MIN, MPI_COMM_WORLD);
	frames.resize(num_frames);

	if (ctx.proc_rank() == 0) {
		std::clog << "Found " << frames.size() << " complete frames.\n";
		}

	// Create output files.
	auto out_path {fs::path{"out/bench"} / subdirs};
	fs::create_directories(out_path);
	out_path /= "rank-" + com_rank_str + ".csv";

	std::ofstream out_file {out_path};
	out_file << "frame,duration\n";

	out_path.replace_extension(".prof.csv");
	std::ofstream prof_file {out_path};
	prof_file << "image_type,num_procs,num_layers,rank,frame,split_t,interlace_t,merge_t,collect_t,"
	             "total_t,bytes_sent\n";

	// Columns of the profiling file that do not change.
	auto const prof_consts {
		std::string{args.renderer} + ","
		+ std::to_string(ctx.num_procs()) + ","
		+ std::to_string(args.num_layers) + ","
		+ com_rank_str + ","
		};

	// Repeatedly composite each frame.
	for (int rep {1}; rep <= args.num_reps; ++rep) {
		if (ctx.proc_rank() == 0) {
			std::clog << "Repetition " << rep << '/' << args.num_reps << "\n";
			}

		for (unsigned fnum {1}; fnum <= frames.size(); ++fnum) {
			std::array<float, 4>            background {0, 0, 0, 0};
			std::tuple<Duration, IceTImage> result;

			switch (args.image_type) {
				case ImageType::flat:
					result = time([&]() {
						return icetCompositeImage(
							frames[fnum - 1].color().data(),
							nullptr,
							nullptr,
							nullptr,
							nullptr,
							background.data()
							);
						});
					break;
				case ImageType::layered:
					result = time([&]() {
						return icetCompositeImageLayered(
							frames[fnum - 1].color().data(),
							frames[fnum - 1].depth().data(),
							args.num_layers,
							nullptr,
							nullptr,
							nullptr,
							background.data()
							);
						});
					break;
				}

			auto const [duration, result_image] = std::move(result);

			out_file << fnum << "," << duration.count() << "\n";

			// Save the output image on the first repetition only.
			if (ctx.proc_rank() == 0 and rep == 1) {
				out_path.replace_filename("frame-"s + std::to_string(fnum) + ".out");
				std::ofstream{out_path, std::ios::binary}.write(
					reinterpret_cast<char const*>(icetImageGetColorcub(result_image)),
					icetImageGetNumPixels(result_image) * 4
					);
				}

			// Save IceT's built-in metrics for profiling.
			prof_file << prof_consts << fnum << ",";

			std::array constexpr timing_enums {
				ICET_COMPRESS_TIME,
				ICET_INTERLACE_TIME,
				ICET_BLEND_TIME,
				ICET_COLLECT_TIME,
				ICET_TOTAL_DRAW_TIME,
				};

			for (std::size_t i {0}; i < timing_enums.size(); ++i) {
				IceTDouble time;
				icetGetDoublev(timing_enums[i], &time);
				prof_file << (time * 1000.0) << ",";
				}

			IceTInt bytes_sent;
			icetGetIntegerv(ICET_BYTES_SENT, &bytes_sent);
			prof_file << bytes_sent << "\n";
			}}

	return EXIT_SUCCESS;
	});
	}
