GENERATED_CODE_DIR	:=IzhSparse_CODE
CXXFLAGS 		+=-std=c++11 -Wall -Wpedantic -Wextra

.PHONY: all clean generated_code

all: IzhSparse

IzhSparse: IzhSparseSim.cc generated_code
	$(CXX) $(CXXFLAGS)  -I../../include IzhSparseSim.cc -o IzhSparse -L$(GENERATED_CODE_DIR) -lrunner -Wl,-rpath $(GENERATED_CODE_DIR)

generated_code:
	$(MAKE) -C $(GENERATED_CODE_DIR)
