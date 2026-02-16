###############################################################################
# Makefile for AKC-PM Protocol on VisibleSim
# With SPONGENT-160 Lightweight Hash Function
# Based on VisibleSim standard Makefile structure
###############################################################################

# Application name
APPNAME = akcProtocol

# Source files
SOURCES = akcProtocol.cpp akcProtocolCode.cpp

# VisibleSim paths (adjust if needed)
SIMULATORDIR = ../..
SIMUEXPORT = $(SIMULATORDIR)/simulatorCore

# Compiler and flags
CC = g++
CXXFLAGS = -g -Wall -std=c++11 -DTINYXML_USE_STL -DTIXML_USE_STL -O2
INCLUDES = -I$(SIMUEXPORT)/src -I.

# Libraries (no OpenSSL needed - using SPONGENT-160)
LIBS = -L$(SIMUEXPORT)/lib -lsimCatoms3D -lsimBase -lmuparser -lGL -lGLU -lglut -lpthread

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Executable
EXECUTABLE = $(APPNAME)

# Default target
all: $(EXECUTABLE)
	@echo "╔═══════════════════════════════════════════════════════════╗"
	@echo "║  AKC-PM Protocol with SPONGENT-160 compiled successfully ║"
	@echo "╚═══════════════════════════════════════════════════════════╝"

# Linking
$(EXECUTABLE): $(OBJECTS)
	@echo "Linking $(EXECUTABLE)..."
	$(CC) $(OBJECTS) -o $@ $(LIBS)
	@echo "Build complete!"

# Compilation
%.o: %.cpp
	@echo "Compiling $<..."
	$(CC) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Automatic dependencies
%.depends: %.cpp
	$(CC) -M $(CXXFLAGS) $(INCLUDES) $< > $@

-include $(SOURCES:.cpp=.depends)

# Clean
clean:
	@echo "Cleaning..."
	rm -f $(OBJECTS) $(EXECUTABLE) *.depends *~
	@echo "Clean complete!"

# Clean all
distclean: clean
	rm -f *.o *.depends

# Help
help:
	@echo "═══════════════════════════════════════════════════════════"
	@echo "  AKC-PM Protocol Makefile (SPONGENT-160)"
	@echo "═══════════════════════════════════════════════════════════"
	@echo "Targets:"
	@echo "  all       - Build the project (default)"
	@echo "  clean     - Remove build files"
	@echo "  distclean - Remove all generated files"
	@echo "  help      - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  make          # Build"
	@echo "  make clean    # Clean"
	@echo ""
	@echo "Features:"
	@echo "  - SPONGENT-160 lightweight hash (20 bytes output)"
	@echo "  - No OpenSSL dependency"
	@echo "  - Optimized for ATtiny45 constraints simulation"
	@echo "  - 3 key management models (SINGLE, FOUR, MULTI)"
	@echo "═══════════════════════════════════════════════════════════"

# Test compilation (verbose)
test: clean
	@echo "Testing compilation with verbose output..."
	$(CC) $(CXXFLAGS) $(INCLUDES) -v -c akcProtocol.cpp -o akcProtocol.o
	$(CC) $(CXXFLAGS) $(INCLUDES) -v -c akcProtocolCode.cpp -o akcProtocolCode.o
	$(CC) $(OBJECTS) -o $(EXECUTABLE) $(LIBS) -v
	@echo "Test compilation successful!"

# Check dependencies
check-deps:
	@echo "Checking required files..."
	@test -f akcProtocol.cpp && echo "✓ akcProtocol.cpp" || echo "✗ akcProtocol.cpp MISSING"
	@test -f akcProtocolCode.hpp && echo "✓ akcProtocolCode.hpp" || echo "✗ akcProtocolCode.hpp MISSING"
	@test -f akcProtocolCode.cpp && echo "✓ akcProtocolCode.cpp" || echo "✗ akcProtocolCode.cpp MISSING"
	@test -f spongent160.hpp && echo "✓ spongent160.hpp" || echo "✗ spongent160.hpp MISSING"
	@test -f Makefile && echo "✓ Makefile" || echo "✗ Makefile MISSING"

.PHONY: all clean distclean test help check-deps

###############################################################################
# Build instructions:
#
# 1. Ensure you're in: VisibleSim/applicationsSrc/akcProtocol/
#
# 2. Required files:
#    - akcProtocol.cpp
#    - akcProtocolCode.hpp
#    - akcProtocolCode.cpp
#    - spongent160.hpp  (NEW - lightweight hash function)
#    - Makefile
#
# 3. Run: make
#
# 4. The executable will be created: ./akcProtocol
#
# 5. Copy to applicationsBin:
#    mkdir -p ../../applicationsBin/akcProtocol
#    cp akcProtocol ../../applicationsBin/akcProtocol/
#    cp *.xml ../../applicationsBin/akcProtocol/
#
# 6. Run simulation:
#    cd ../../applicationsBin/akcProtocol
#    ../../build/simulators/catoms3DSimulator -f test_simple.xml -c akcProtocol
#
# NOTES:
# - No OpenSSL dependency (SPONGENT-160 is header-only)
# - Lighter and more realistic for embedded systems
# - Output: 20 bytes (160 bits) vs SHA-256's 32 bytes
###############################################################################
