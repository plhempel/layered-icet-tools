#!/usr/bin/make -f

# Run IceT tests.


# Check for required arguments.
ifndef BUILD_DIR
$(error Missing variable "BUILD_DIR", which must contain the path to CMake's build directory.)
endif


# Log output folder.
OUT := out/test

# Test cases per executable.
ICET_CORE_TESTS := \
	BackgroundCorrect \
	CompressionSize \
	FloatingViewport \
	ImageConvert \
	Interlace \
	MaxImageSplit \
	OddImageSizes \
	OddProcessCounts \
	PreRender \
	RadixkrUnitTests \
	RadixkUnitTests \
	RenderEmpty \
	SimpleTiming \
	SparseImageCopy

ICET_OPENGL_TESTS := \
	BlankTiles \
	BoundsBehindViewer \
	DisplayNoDraw \
	RandomTransform \
	SimpleExample \

ICET_OPENGL3_TESTS := \
	SimpleExampleOGL3


# Helper function to list IceT libraries.
icet_libs = $(foreach name,$1,$(wildcard $(BUILD_DIR)/_deps/icet-build/lib/*IceT$(name)*))


# Default target: Run all tests.
all: icet-core icet-opengl icet-opengl3

# Delete log files.
clean:
	@rm -r $(OUT)

# Run tests.
icet-core: $(ICET_CORE_TESTS:%=$(OUT)/icet-core/%.log)
$(OUT)/icet-core/%.log: \
		$(BUILD_DIR)/bin/icetTests_mpi \
		$(call icet_libs,Core MPI) \
		| $(OUT)/icet-core/
	mpirun -n 2 $< $* > $@ 2>&1

icet-opengl: $(ICET_OPENGL_TESTS:%=$(OUT)/icet-opengl/%.log)
$(OUT)/icet-opengl/%.log: \
		$(BUILD_DIR)/bin/icetTests_mpi_opengl \
		$(call icet_libs,Core MPI GL) \
		| $(OUT)/icet-opengl/
	mpirun -n 2 $< $* > $@ 2>&1

icet-opengl3: $(ICET_OPENGL3_TESTS:%=$(OUT)/icet-opengl3/%.log)
$(OUT)/icet-opengl3/%.log: \
		$(BUILD_DIR)/bin/icetTests_mpi_opengl3 \
		$(call icet_libs,Core MPI GL3) \
		| $(OUT)/icet-opengl3/
	mpirun -n 2 $< $* > $@ 2>&1

# Create output directory.
$(OUT)/%/:
	@mkdir -p $@
.PRECIOUS: $(OUT)/%/
