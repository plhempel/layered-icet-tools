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
	2:BackgroundCorrect \
	2:CompressionSize \
	2:FloatingViewport \
	2:ImageConvert \
	2:Interlace \
	2:MaxImageSplit \
	1:OddImageSizes \
	8:OddProcessCounts \
	1:PreRender \
	2:RadixkrUnitTests \
	2:RadixkUnitTests \
	2:RenderEmpty \
	2:SimpleTiming \
	2:SparseImageCopy

ICET_OPENGL_TESTS := \
	2:BlankTiles \
	2:BoundsBehindViewer \
	2:DisplayNoDraw \
	2:RandomTransform \
	2:SimpleExample

ICET_OPENGL3_TESTS := \
	2:SimpleExampleOGL3


# Convert each word in the first argument to an IceT library, mapping e.g. "Core" to the path of
# "libIceTCore.so" or "IceTCore.dll".
icet_libs = $(foreach name,$1,$(wildcard $(BUILD_DIR)/_deps/icet-build/lib/*IceT$(name)*))

# Given a list of test cases, each of the format `<num_procs>:<name>`, return only the names of
# the cases.
test_names = $(foreach case,$1,$(lastword $(subst :, ,$(case))))
# Given the name of a test case ($1), extract the number of processes configured for that case in a
# given list ($2), whose entries are of the format `<num_procs>:<name>`.
test_procs = $(firstword $(subst :, ,$(filter %:$1,$2)))


# Default target: Run all tests.
all: icet-core icet-opengl icet-opengl3

# Delete log files.
clean:
	@rm -r $(OUT)

# Run tests.
icet-core: $(patsubst %,$(OUT)/icet-core/%.log,$(call test_names,$(ICET_CORE_TESTS)))
$(OUT)/icet-core/%.log: \
		$(BUILD_DIR)/bin/icetTests_mpi \
		$(call icet_libs,Core MPI) \
		| $(OUT)/icet-core/
	mpirun -n $(call test_procs,$*,$(ICET_CORE_TESTS)) --oversubscribe $< $* > $@ 2>&1

icet-opengl: $(patsubst %,$(OUT)/icet-opengl/%.log,$(call test_names,$(ICET_OPENGL_TESTS)))
$(OUT)/icet-opengl/%.log: \
		$(BUILD_DIR)/bin/icetTests_mpi_opengl \
		$(call icet_libs,Core MPI GL) \
		| $(OUT)/icet-opengl/
	mpirun -n $(call test_procs,$*,$(ICET_OPENGL_TESTS)) --oversubscribe $< $* > $@ 2>&1

icet-opengl3: $(patsubst %,$(OUT)/icet-opengl3/%.log,$(call test_names,$(ICET_OPENGL3_TESTS)))
$(OUT)/icet-opengl3/%.log: \
		$(BUILD_DIR)/bin/icetTests_mpi_opengl3 \
		$(call icet_libs,Core MPI GL3) \
		| $(OUT)/icet-opengl3/
	mpirun -n $(call test_procs,$*,$(ICET_OPENGL3_TESTS)) --oversubscribe $< $* > $@ 2>&1

# Create output directory.
$(OUT)/%/:
	@mkdir -p $@
.PRECIOUS: $(OUT)/%/
