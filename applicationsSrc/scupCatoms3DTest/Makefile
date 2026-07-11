SRCS = scupCatoms3DTest.cpp SCupCatoms3DTestCode.cpp
SPONGENT_OBJ = ../../simulatorCore/src/spongent160.o
OUT = ../../applicationsBin/scupCatoms3DTest/scupCatoms3DTest
MODULELIB = -lsimCatoms3D

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.depends)

OS = $(shell uname -s)
SIMULATORLIB = $(MODULELIB:-l%=../../simulatorCore/lib/lib%.a)

INCLUDES = -I. -I../../simulatorCore/src -I/usr/local/include -I/opt/local/include -I/usr/X11/include

ifeq ($(OS),Darwin)
LIBS = -L../../simulatorCore/lib $(MODULELIB) -lmuparser -lpthread -lm -ldl \
       -lGLEW -framework GLUT -framework OpenGL
else
LIBS = -L../../simulatorCore/lib $(MODULELIB) -lmuparser -lpthread -lm -ldl \
       -lGLEW -lglut -lGLU -lGL
endif

CCFLAGS = -g -Wall -std=c++17 -fno-stack-protector

CC = g++

.cpp.o:
	$(CC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

%.depends: %.cpp
	$(CC) -M $(CCFLAGS) $(INCLUDES) $< > $@

all: $(OUT)

$(OUT): $(SIMULATORLIB) $(OBJS) $(SPONGENT_OBJ)
	$(CC) -o $(OUT) $(OBJS) $(SPONGENT_OBJ) $(LIBS)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

clean:
	rm -f *~ $(OBJS) $(OUT) $(DEPS)
