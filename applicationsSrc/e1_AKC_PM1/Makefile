SRCS = e1_AKC_PM.cpp e1_AKC_PMCode.cpp
OUT  = ../../applicationsBin/e1_AKC_PM/e1_AKC_PM
MODULELIB = -lsimCatoms3D

INCLUDES = -I. -I../../simulatorCore/src
LIBS = -L../../simulatorCore/lib -lglut -lGL -lGLU -lGLEW -lpthread -lm -ldl -lmuparser $(MODULELIB)
CCFLAGS = -g -Wall -std=c++17

CC = g++

OBJS = $(SRCS:.cpp=.o)

all: $(OUT)

.cpp.o:
	$(CC) $(INCLUDES) $(CCFLAGS) -c $< -o $@

$(OUT): $(SIMULATORLIB) $(OBJS)
	$(CC) -o $(OUT) $(OBJS) \
	-L../../simulatorCore/lib \
	-lsimCatoms3D \
	-lmuparser -lpthread -lm -ldl \
	-lGLEW -lglut -lGLU -lGL
clean:
	rm -f *.o $(OUT)

