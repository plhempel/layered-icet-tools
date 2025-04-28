#include "common.hpp"

#include <strategy-hash.hpp>
#include <single-image-strategy-hash.hpp>


/// Use IceT to blend PNG images front to back.
/// Arguments: <strategy>[/<single-image-strategy>] <width> <height> [<rank>:<image>]...
auto main(int argc, char* argv[]) -> int {
	using namespace layered_icet;
	return try_main([&]() {

	// Parse output size.
	IceTSizeType width, height;

	if (argc < 4
			or (width  = atoi(argv[2])) == 0
			or (height = atoi(argv[3])) == 0
			) {
		std::cerr << log_sev_fatal << "Invalid or missing arguments.\n"
		             "Usage: " << argv[0] << " <strategy>[/<single-image-strategy>] <width> "
		                                     "<height> [<rank>:<image>]...\n";
		return EXIT_FAILURE;
		}

	// Parse strategy.
	std::string_view const strategy_name {argv[1], std::strcspn(argv[1], "/")};
	auto const             strategy      {
			StrategyLut::find(strategy_name.data(), strategy_name.size())};

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
				SingleImageStrategyLut::find(si_strategy_name.data(), si_strategy_name.size())};

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
	UniqueSpan<InputLayer> const in_layers  {int_cast<std::size_t>(argc - 4)};
	std::size_t                  num_layers {0};

	for (auto argi {4}; argi < argc; ++argi) {
		char* parse_ptr;

		// Parse rank.
		if (std::strtol(argv[argi], &parse_ptr, 10) == ctx.proc_rank()) {
			if (parse_ptr == argv[argi] or *parse_ptr != ':') {
				std::cerr << log_sev_error << "Argument " << argv[argi]
				          << " does not match the expected pattern <rank>:<image>.\n";
				continue;
				}

			// Skip colon.
			++parse_ptr;
			in_layers.span()[num_layers] = {parse_ptr, static_cast<float>(argi - 3) / (argc - 3)};
			++num_layers;
			}}

	// Assemble layers assigned to this rank into a fragment buffer.
	RawImage in_buffer {width, height, in_layers.span().first(num_layers)};

	// Composite fragments from all ranks.
	std::array<IceTFloat, 4> const background {0, 0, 0, 0};
	auto const out_image {icetCompositeImageLayered(
			in_buffer.color().data(),
			in_buffer.depth().data(),
			in_buffer.num_layers(),
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
