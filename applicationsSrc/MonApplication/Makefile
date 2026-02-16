SRCS = MonApplication.cpp MonApplicationCode.cpp
OUT = ../../applicationsBin/MonApplication/MonApplication
MODULELIB = -lsimCatoms3D

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.depends)

OS = $(shell uname -s)
SIMULATORLIB = $(MODULELIB:-l%=../../simulatorCore/lib/lib%.a)

INCLUDES = -I. -I../../simulatorCore/src

ifeq ($(OS),Darwin)
LIBS = -L../../simulatorCore/lib $(MODULELIB) -lGLEW -lmuparser -framework GLUT -framework OpenGL
else
# Order matters: simulator lib first, then GLEW, then GLUT, then GLU, then GL
LIBS = -L../../simulatorCore/lib $(MODULELIB) -lGLEW -lglut -lGLU -lGL -lmuparser -lpthread -lm
endif

CCFLAGS = -g -Wall -std=c++17 -DTINYXML_USE_STL -fPIC
ifeq ($(OS), Darwin)
CCFLAGS += -DGL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
endif

CC = g++

.cpp.o:
	$(CC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

%.depends: %.cpp
	$(CC) -M $(CCFLAGS) $(INCLUDES) $< > $@

all: $(OUT)

$(OUT): $(SIMULATORLIB) $(OBJS)
	$(CC) -o $(OUT) $(OBJS) $(LIBS) -no-pie

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

clean:
	rm -f *~ $(OBJS) $(OUT) $(DEPS)
