CXX=g++ -m64
CXXFLAGS=-I../common -Iobjs/ -O3 -std=c++11 -fopenmp
ISPC=ispc
# note: change target to avx-x2 for AVX capable machines
ISPCFLAGS=-O2 --target=sse4-x2 --arch=x86-64

APP_NAME=huffman
OBJDIR=objs
COMMONDIR=common


TASKSYS_CXX=$(COMMONDIR)/tasksys.cpp
TASKSYS_LIB=-lpthread
TASKSYS_OBJ=$(addprefix $(OBJDIR)/, $(subst $(COMMONDIR)/,, $(TASKSYS_CXX:.cpp=.o)))

default: $(APP_NAME)

.PHONY: dirs clean

dirs:
		/bin/mkdir -p $(OBJDIR)/

clean:
		/bin/rm -rf $(OBJDIR) *~ $(APP_NAME) *_output *.o

OBJS=$(OBJDIR)/huffcode.o $(OBJDIR)/huffman.o $(OBJDIR)/test_ispc.o $(TASKSYS_OBJ)

$(APP_NAME): dirs $(OBJS)
		$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lm $(TASKSYS_LIB)

$(OBJDIR)/%.o: %.cpp
		$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/%.o: $(COMMONDIR)/%.cpp
	$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/huffcode.o: huffman.h $(OBJDIR)/test_ispc.h $(COMMONDIR)/CycleTimer.h

$(OBJDIR)/%_ispc.h $(OBJDIR)//%_ispc.o: %.ispc
		$(ISPC) $(ISPCFLAGS) $< -o $(OBJDIR)/$*_ispc.o -h $(OBJDIR)/$*_ispc.h

