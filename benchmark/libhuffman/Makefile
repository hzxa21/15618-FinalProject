CXX=g++ -m64
CXXFLAGS=-I../common -Iobjs/ -O3 -std=c++11 -fopenmp
ISPC=ispc
# note: change target to avx-x2 for AVX capable machines
ISPCFLAGS=-O2 --target=sse4-x2 --arch=x86-64

OBJDIR=objs
COMMONDIR=common


TASKSYS_CXX=$(COMMONDIR)/tasksys.cpp
TASKSYS_LIB=-lpthread
TASKSYS_OBJ=$(addprefix $(OBJDIR)/, $(subst $(COMMONDIR)/,, $(TASKSYS_CXX:.cpp=.o)))

OBJS=$(OBJDIR)/huffcode.o $(OBJDIR)/util.o $(OBJDIR)/test_ispc.o $(TASKSYS_OBJ)

default: huffman_seq huffman_v1

seq: huffman_seq

v1: huffman_v1

.PHONY: dirs clean

dirs:
		/bin/mkdir -p $(OBJDIR)/

clean:
		/bin/rm -rf $(OBJDIR) *~ huffman huffman_seq huffman_v1 *_output *.o


huffman_seq: dirs  $(OBJS) $(OBJDIR)/huffman_seq.o
		$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(OBJDIR)/huffman_seq.o -lm $(TASKSYS_LIB)

huffman_v1: dirs $(OBJS) $(OBJDIR)/huffman_v1.o
		$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(OBJDIR)/huffman_v1.o -lm $(TASKSYS_LIB)

$(OBJDIR)/%.o: %.cpp
		$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/%.o: $(COMMONDIR)/%.cpp
	$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/huffcode.o: util.h huffman.h $(OBJDIR)/test_ispc.h $(COMMONDIR)/CycleTimer.h

$(OBJDIR)/%_ispc.h $(OBJDIR)//%_ispc.o: %.ispc
		$(ISPC) $(ISPCFLAGS) $< -o $(OBJDIR)/$*_ispc.o -h $(OBJDIR)/$*_ispc.h

