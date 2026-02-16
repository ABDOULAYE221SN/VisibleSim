# Makefile pour MonApplication

SRCS = MonApplication.cpp MonApplicationCode.cpp
OUT = ../../applicationsBin/MonApplication/MonApplication
MODULELIB = -lsimCatoms3D

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.depends)

OS = $(shell uname -s)
SIMULATORLIB = $(MODULELIB:-l%=../../simulatorCore/lib/lib%.a)

INCLUDES = -I. -I../../simulatorCore/src -I/usr/local/include -I/opt/local/include -I/usr/X11/include

ifeq ($(OS),Darwin)
LIBS = -L../../simulatorCore/lib -lGLEW -lglut -framework GLUT -framework OpenGL $(MODULELIB)
else
LIBS = -L./ -L/usr/local/lib -L/usr/X11/lib -lsimCatoms3D -lsimSlidingCubes -lsimBlinkyBlocks -lsimSmartBlocks -lsimDatoms -lsimHexanodes -lmuparser -lglut -lGL -lGLEW -lGLU -lpthread -ldl -lstdc++ -lm -L../../simulatorCore/lib
endif

CCFLAGS = -g -Wall -std=c++17 -DTINYXML_USE_STL
ifeq ($(OS), Darwin)
CCFLAGS += -DGL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
endif

CC = g++

.PHONY: all clean

all: $(OUT)

$(OUT): $(SIMULATORLIB) $(OBJS)
	@mkdir -p $(dir $(OUT))
	$(CC) -o $(OUT) $(OBJS) $(LIBS)

%.o: %.cpp
	$(CC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

%.depends: %.cpp
	$(CC) -M $(CCFLAGS) $(INCLUDES) $< > $@

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

clean:
	rm -f *~ $(OBJS) $(OUT) $(DEPS)
