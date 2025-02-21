PACKAGES       = stages/test stmlib/utils stages stmlib/dsp tides2/ramp

VPATH          = $(PACKAGES)

TARGET         = stages_test
PERF_TARGET    = stages_perf
CLI_TARGET     = stages_cli
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
COMMON_CC	   = \
		ramp_extractor.cc \
    segment_generator.cc \
		resources.cc \
		units.cc \
		random.cc \
		quantizer.cc \
		braids_quantizer.cc
CC_FILES       = stages_test.cc $(COMMON_CC)
OBJ_FILES      = $(CC_FILES:.cc=.o)
OBJS           = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES)) $(STARTUP_OBJ)
DEPS           = $(OBJS:.o=.d)
DEP_FILE       = $(BUILD_DIR)depends.mk
OPEN           = $(shell type -p open || echo xdg-open)

PERF_CC_FILES  = stages_perf.cc $(COMMON_CC)
PERF_OBJ_FILES = $(PERF_CC_FILES:.cc=.o)
PERF_OBJS      = $(patsubst %,$(BUILD_DIR)%,$(PERF_OBJ_FILES)) $(STARTUP_OBJ)
PERF_DEPS      = $(PERF_OBJS:.o=.d)

CLI_CC_FILES   = stages_cli.cc $(COMMON_CC)
CLI_OBJ_FILES  = $(CLI_CC_FILES:.cc=.o)
CLI_OBJS       = $(patsubst %,$(BUILD_DIR)%,$(CLI_OBJ_FILES)) $(STARTUP_OBJ)
CLI_DEPS       = $(CLI_OBJS:.o=.d)

all:  stages_test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)%.o: %.cc
	g++ -c -DTEST -g -Wall -Werror -msse2 -Wno-unused-variable -O2 -I. $< -o $@

$(BUILD_DIR)%.d: %.cc
	g++ -MM -DTEST -I. $< -MF $@ -MT $(@:.d=.o)

stages_test:  $(OBJS)
	g++ -g -o $(TARGET) $(OBJS) -Wl,-no-pie -lm -lprofiler -L/opt/local/lib

stages_perf: $(PERF_OBJS)
	g++ -g -o $(PERF_TARGET) $(PERF_OBJS) -lm -lprofiler -L/opt/local/lib

depends:  $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

$(DEP_FILE):  $(BUILD_DIR) $(DEPS)
	cat $(DEPS) > $(DEP_FILE)

profile:	stages_perf
	env CPUPROFILE_FREQUENCY=10000 CPUPROFILE=$(BUILD_DIR)/stages.prof ./$(PERF_TARGET) && pprof --pdf ./$(PERF_TARGET) $(BUILD_DIR)/stages.prof > profile.pdf && ${OPEN} profile.pdf

cli: $(CLI_OBJS)
	g++ -g -o $(CLI_TARGET) $(CLI_OBJS) -lm -lprofiler -lboost_program_options -L/opt/local/lib

valgrind:	stages_test
	valgrind --tool=callgrind ./stages_test

perf:		stages_perf
	./stages_perf

clean:
	rm $(BUILD_DIR)*.*

include $(DEP_FILE)
