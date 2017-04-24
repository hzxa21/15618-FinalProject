
EXECUTABLE := lzss

CC_FILES   := main.cpp

###########################################################

OBJDIR=objs
CXX=g++
CXXFLAGS=-g -O0 -ggdb -std=c++1y -Wall -Wextra -Werror

ARCH=$(shell uname | sed -e 's/-.*//g')
ifeq ($(ARCH), Darwin)
# Building on mac
LDFLAGS=-L/usr/local/Cellar/boost/1.63.0/ -lboost_program_options
else
# Building on Linux
LDFLAGS=-L/usr/local/cuda/lib64/ -lcudart
endif

OBJS=$(OBJDIR)/main.o

.PHONY: dirs clean

default: $(EXECUTABLE)

dirs:
	mkdir -p $(OBJDIR)/

clean:
	rm -rf $(OBJDIR) $(EXECUTABLE)

$(EXECUTABLE): dirs $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(OBJDIR)/%.o: %.cpp
	$(CXX) $< $(CXXFLAGS) -c -o $@
