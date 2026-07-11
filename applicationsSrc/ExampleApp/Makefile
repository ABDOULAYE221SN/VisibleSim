mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))
APPDIR = ../../applicationsBin/$(current_dir)

SRCS = ExampleApp.cpp ExampleAppCode.cpp
OUT  = $(APPDIR)/ExampleApp
MODULELIB = -lsimCatoms3D

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.depends)

OS = $(shell uname -s)
SIMULATORLIB = $(MODULELIB:-l%=../../simulatorCore/lib/lib%.a)

ifeq ($(GLOBAL_INCLUDES), )
INCLUDES = -I. -I../../simulatorCore/src -I/usr/local/include -I/opt/local/include
else
INCLUDES = -I. -I../../simulatorCore/src $(GLOBAL_INCLUDES)
endif

ifeq ($(GLOBAL_LIBS), )
	ifeq ($(OS),Darwin)
LIBS = -L./ -L../../simulatorCore/lib -lGLEW -lglut -framework GLUT -framework OpenGL -L/usr/X11/lib /usr/local/lib/libglut.dylib /usr/local/lib/libmuparser.dylib $(MODULELIB)
	else
LIBS = -L../../simulatorCore/lib -lglut -lGL -lGLU -lGLEW -lpthread -lm -ldl -lmuparser $(MODULELIB)
	endif
else
LIBS = $(GLOBAL_LIBS) -L../../simulatorCore/lib
endif

ifeq ($(GLOBAL_CCFLAGS),)
CCFLAGS = -g -Wall -std=c++17 -fno-stack-protector
	ifeq ($(OS), Darwin)
	CCFLAGS += -DGL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED -Wno-deprecated-declarations -Wno-overloaded-virtual
	endif
else
CCFLAGS = $(GLOBAL_CCFLAGS)
endif

CC = g++

.PHONY: clean all

.cpp.o:
	$(CC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

%.depends: %.cpp
	$(CC) -M $(CCFLAGS) $(INCLUDES) $< > $@

all: $(OUT)
	@:

$(OUT): $(SIMULATORLIB) $(OBJS)
	$(CC) -o $(OUT) $(OBJS) \
	-L../../simulatorCore/lib \
	-lsimCatoms3D \
	-lmuparser -lpthread -lm -ldl \
	-lGLEW -lglut -lGLU -lGL

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

clean:
	rm -f *~ $(OBJS) $(OUT) $(DEPS)
