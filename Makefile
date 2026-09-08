
CXX = g++
CXXFLAGS  = -std=c++17
CXXFLAGS  = -O2
CXXFLAGS += -Wall -Werror -Wextra
CXXFLAGS += -I./include

CXXLDFLAGS = -lpthread

SRCS = src/*.cpp
INCS = include/*.h

TARGET = main

${TARGET}: ${INCS} ${SRCS} main.cpp
	${CXX} -o $@ ${CXXFLAGS} $^ ${CXXLDFLAGS}

.PHONY: clean

clean:
	rm ${TARGET}


