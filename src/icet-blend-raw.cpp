#include "common.hpp"

#include <strategy-hash.hpp>
#include <single-image-strategy-hash.hpp>


/// Use IceT to blend PNG images front to back.
/// Arguments: <strategy>[/<single-image-strategy>] <width> <height> (<color> <depth>)...
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
		                                     "<height> (<color> <depth>)...\n";
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

	if (argc < 4 + ctx.num_procs() * 2) {
		std::cerr << log_sev_fatal << "Too few arguments, must specify one image per process\n";
		return EXIT_FAILURE;
		}

	// Read image.
	std::span const files {&argv[4 + ctx.proc_rank() * 2], 2};
	RawImage const in_image {width, height, fopen(files[0], "rb"), fopen(files[1], "rb")};

	// Composite fragments from all ranks.
	std::array<IceTFloat, 4> const background {0, 0, 0, 0};
	auto const out_image {icetCompositeImageLayered(
			in_image.color().data(),
			in_image.depth().data(),
			in_image.num_layers(),
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
