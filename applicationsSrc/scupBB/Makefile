LOCAL_INCLUDES = -I. -I../../simulatorCore/src

SRCS = scupBB.cpp scupBBCode.cpp
OBJS = scupBB.o scupBBCode.o ../../simulatorCore/src/spongent160.o

OUT = scupBB

# Module library (BlinkyBlocks)
MODULELIB = -lsimBlinkyBlocks

CXX = g++
CXXFLAGS = -g -Wall -std=c++17

.PHONY: clean all

all: $(OUT)
	@mkdir -p ../../applicationsBin/$(OUT)
	@mv $(OUT) ../../applicationsBin/$(OUT)

$(OUT): $(OBJS)
	$(CXX) -o $(OUT) $(OBJS) -L../../simulatorCore/lib $(MODULELIB) $(GLOBAL_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(GLOBAL_CCFLAGS) $(LOCAL_INCLUDES) $(GLOBAL_INCLUDES) -c $< -o $@

clean:
	rm -f *~ *.o $(OUT)
	rm -rf ../../applicationsBin/$(OUT)

test:
	@echo "Running S-CUP tests..."
	@cd ../../applicationsBin/$(OUT) && ./$(OUT) -c config.xml -t
