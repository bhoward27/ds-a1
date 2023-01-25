# TODO: Change back to original make file.
ifdef USE_INT
MACRO = -DUSE_INT
endif

#compiler setup
CXX = g++
# TODO: Restore original flags.
# CXXFLAGS = -std=c++14 -O3 -pthread $(MACRO)
CXXFLAGS = -std=c++14 -O3 -pthread $(MACRO) -Wall -Werror -Wno-error=unknown-pragmas


COMMON= core/utils.h core/cxxopts.h core/get_time.h core/graph.h core/quick_sort.h
SERIAL= pi_calculation triangle_counting page_rank
PARALLEL= pi_calculation_parallel page_rank_parallel # page_rank_parallel_atomic
## PARALLEL= pi_calculation_parallel # TODO: Add these back in: triangle_counting_parallel page_rank_parallel page_rank_parallel_atomic
ALL= $(SERIAL) $(PARALLEL)


all : $(ALL)

% : %.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o $@ $<

.PHONY : clean

clean :
	rm -f *.o *.obj $(ALL)