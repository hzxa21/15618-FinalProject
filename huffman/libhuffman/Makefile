CXX=g++ -m64
CXXFLAGS=-I../common -Iobjs/ -O3 -std=c++11 -fopenmp -Icommon/
# CXXFLAGS=-I../common -Iobjs/ -g -O0 -ggdb -fsanitize=address -fno-omit-frame-pointer -fuse-ld=gold -std=c++11 -fopenmp
ISPC=ispc
# note: change target to avx-x2 for AVX capable machines
ISPCFLAGS=-O2 --target=sse4 --arch=x86-64
#ISPCFLAGS=-O3 --target=avx-x2 --arch=x86-64

OBJDIR=objs
COMMONDIR=common


TASKSYS_CXX=$(COMMONDIR)/tasksys.cpp
TASKSYS_LIB=-lpthread 
TASKSYS_OBJ=$(addprefix $(OBJDIR)/, $(subst $(COMMONDIR)/,, $(TASKSYS_CXX:.cpp=.o)))

OBJS=$(OBJDIR)/huffcode.o $(OBJDIR)/util.o $(OBJDIR)/test_ispc.o \
  $(OBJDIR)/huffman_seq.o $(OBJDIR)/huffman_parallel.o $(TASKSYS_OBJ)

default: huffman

.PHONY: dirs clean

dirs:
		/bin/mkdir -p $(OBJDIR)/

clean:
		/bin/rm -rf $(OBJDIR) *~ huffman *_output *.o


huffman: dirs  $(OBJS)
		$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lm $(TASKSYS_LIB)

$(OBJDIR)/%.o: %.cpp
		$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/%.o: $(COMMONDIR)/%.cpp
	$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/huffcode.o: util.h huffman.h $(OBJDIR)/test_ispc.h $(COMMONDIR)/CycleTimer.h

$(OBJDIR)/%_ispc.h $(OBJDIR)//%_ispc.o: %.ispc
		$(ISPC) $(ISPCFLAGS) $< -o $(OBJDIR)/$*_ispc.o -h $(OBJDIR)/$*_ispc.h

