#include "common.hpp"

#include <strategy-hash.hpp>
#include <single-image-strategy-hash.hpp>


/// Use IceT to blend PNG images front to back.
/// Expected arguments: <strategy>[/<single-image-strategy>] <width> <height> [<rank>:<color>:<depth>]...
auto main(int argc, char* argv[]) -> int {
	using namespace deep_icet;
	return try_main([&]() {

	// Parse output size.
	IceTSizeType width, height;

	if (argc < 4
			or (width  = atoi(argv[2])) == 0
			or (height = atoi(argv[3])) == 0
			) {
		std::cerr << log_sev_fatal << "Invalid or missing arguments.\n"
		             "Usage: " << argv[0] << " <strategy>[/<single-image-strategy>] <width> "
		                                     "<height> [<rank>:<color>:<depth>]...\n";
		return EXIT_FAILURE;
		}

	// Parse strategy.
	std::string_view const strategy_name {argv[1], std::strcspn(argv[1], "/")};
	auto const             strategy      {
			StrategyTable::find(strategy_name.data(), strategy_name.size())};

	if (not strategy) {
		std::cerr << log_sev_fatal << "Unknown compositing strategy `" << strategy_name << "`.\n";
		return EXIT_FAILURE;
		}

	// Parse single image strategy if used.
	auto single_image_strategy {ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC};

	if (strategy->uses_single_image_strategy) {
		if (*strategy_name.end() != '/') {
			std::cerr << "The selected compositing strategy requires a single image compositing "
			             "strategy to be specified.\n";
			return EXIT_FAILURE;
			}

		std::string_view const si_strategy_name {strategy_name.end() + 1};
		auto const             si_strategy      {
				SingleImageStrategyTable::find(si_strategy_name.data(), si_strategy_name.size())};

		if (not si_strategy) {
			std::cerr << log_sev_fatal << "Unknown single image compositing strategy `"
			          << si_strategy_name << "`.\n";
			return EXIT_FAILURE;
			}

		single_image_strategy = si_strategy->key;
		}

	// IceT setup.
	Context ctx {&argc, &argv};

	icetStrategy(strategy->key);
	icetSingleImageStrategy(single_image_strategy);

	icetResetTiles();
	icetAddTile(0, 0, width, height, 0);

	// Parse layers.
	struct RawLayer {
		std::string      color_file;
		std::string_view depth_file;
		};

	UniqueSpan<RawLayer> const in_layers  {int_cast<IceTLayerCount>(argc - 4)};
	IceTLayerCount             num_layers {0};

	for (auto argi {4}; argi < argc; ++argi) {
		char* parse_ptr;

		// Parse rank.
		if (std::strtol(argv[argi], &parse_ptr, 10) == ctx.proc_rank()) {
			if (parse_ptr == argv[argi] or *parse_ptr != ':') {
				std::cerr << log_sev_error << "Argument " << argv[argi]
				          << " does not match the expected pattern <rank>:<color>:<depth>.\n";
				continue;
				}

			// Skip colon.
			++parse_ptr;

			// Color buffer path.
			std::string color_file {parse_ptr, std::strcspn(parse_ptr, ":")};
			parse_ptr += color_file.size();

			// Colon.
			if (*parse_ptr != ':') {
				std::cerr << log_sev_error << "Argument " << argv[argi]
				          << " does not match the expected pattern <rank>:<color>:<depth>.\n";
				continue;
				}

			++parse_ptr;

			// Depth buffer path.
			std::string_view depth_file {parse_ptr};

			// Create layer.
			in_layers.span()[num_layers] = {color_file, depth_file};
			++num_layers;
			}}

	// Assemble layers into a single color and depth buffer.
	auto const         image_size   {num_layers * width * height};
	std::vector<Color> color_buffer (image_size);
	std::vector<Depth> depth_buffer (image_size);

	for (IceTLayerCount i {0}; i < num_layers; ++i) {
		auto const& layer {in_layers.span()[i]};

		// Read color data.
		auto const layer_color {read_all(
				fopen(layer.color_file.c_str(), "rb"),
				height * width * sizeof(Color)
				)};
		assert(layer_color.size() == width * height * sizeof(Color));

		// Read depth data.
		auto const layer_depth {read_all(
				fopen(layer.depth_file.data(), "rb"),
				height * width * sizeof(Depth)
				)};
		assert(layer_depth.size() == width * height * sizeof(Depth));

		// For each pixel:
		for (IceTSizeType y {0}; y < height; ++y) {
			for (IceTSizeType x {0}; x < width; ++x) {
				auto const layer_idx {y * width + x};
				auto const out_idx   {(y * width + x) * num_layers + i};

				color_buffer[out_idx] = reinterpret_cast<Color const*>(layer_color.data())[layer_idx];
				depth_buffer[out_idx] = reinterpret_cast<Depth const*>(layer_depth.data())[layer_idx];
				}}}

	// Composite fragments from all ranks.
	std::array<IceTFloat, 4> const background {0, 0, 0, 1};
	auto const out_image {icetCompositeImageLayered(
			color_buffer.data(),
			depth_buffer.data(),
			num_layers,
			nullptr,
			nullptr,
			nullptr,
			background.data()
			)};

	// Output result image.
	if (ctx.proc_rank() == 0) {
		write_image(out_image, fdopen(ctx.stdout(), "wb"));
		}

	return EXIT_SUCCESS;
	});
	}
