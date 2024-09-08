#include "common.hpp"


/// Use IceT to blend PNG images front to back.
/// Expected arguments: <width> <height> [<rank>:<image>]...
auto main(int argc, char* argv[]) -> int {
	using namespace deep_icet;
	return try_main([&]() {

	// Parse output size.
	IceTSizeType width, height;

	if (argc < 3
			or (width  = atoi(argv[1])) == 0
			or (height = atoi(argv[2])) == 0
			) {
		std::cerr << log_sev_fatal << "Invalid or missing arguments.\n"
		             "Usage: " << argv[0] << " <width> <height> [<fragments>]...\n";
		return EXIT_FAILURE;
		}

	// IceT setup.
	Context ctx {&argc, &argv};

	icetStrategy(ICET_STRATEGY_SEQUENTIAL);
	icetSingleImageStrategy(ICET_SINGLE_IMAGE_STRATEGY_AUTOMATIC);

	icetResetTiles();
	icetAddTile(0, 0, width, height, 0);

	// Parse layers.
	UniqueSpan<InputLayer> const in_layers  {int_cast<std::size_t>(argc - 3)};
	std::size_t                  num_layers {0};

	for (auto argi {3}; argi < argc; ++argi) {
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
