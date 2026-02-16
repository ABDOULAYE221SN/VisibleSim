# ═══════════════════════════════════════════════════════════════════════════
# Makefile pour AKC-PM - Modèle MULTI_KEY
# ═══════════════════════════════════════════════════════════════════════════

SRCS = akcpm_multi_key.cpp akcpmBlockCode_multi_key.cpp
OUT = ../../applicationsBin/akcpm_multi_key/akcpm_multi_key
MODULELIB = -lsimCatoms3D

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.depends)

OS = $(shell uname -s)
SIMULATORLIB = $(MODULELIB:-l%=../../simulatorCore/lib/lib%.a)

ifeq ($(GLOBAL_INCLUDES), )
INCLUDES = -I. -I../../simulatorCore/src
else
INCLUDES = -I. -I../../simulatorCore/src $(GLOBAL_INCLUDES)
endif

ifeq ($(GLOBAL_LIBS), )
    ifeq ($(OS),Darwin)
LIBS = -L../../simulatorCore/lib -lGLEW -lglut -framework GLUT -framework OpenGL $(MODULELIB)
    else
LIBS = -L../../simulatorCore/lib -lm -lglut -lGL -lGLU -lGLEW -lpthread $(MODULELIB)
    endif
else
LIBS = $(GLOBAL_LIBS) -L../../simulatorCore/lib
endif

ifeq ($(GLOBAL_CCFLAGS),)
CCFLAGS = -g -Wall -std=c++17 -DTINYXML_USE_STL
    ifeq ($(OS), Darwin)
    CCFLAGS += -DGL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
    endif
else
CCFLAGS = $(GLOBAL_CCFLAGS)
endif

CC = g++

.cpp.o:
	$(CC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

%.depends: %.cpp
	$(CC) -M $(CCFLAGS) $(INCLUDES) $< > $@

all: $(OUT)
	@:

$(OUT): $(SIMULATORLIB) $(OBJS)
	$(CC) -o $(OUT) $(OBJS) $(LIBS)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

clean:
	rm -f *~ $(OBJS) $(OUT) $(DEPS)
