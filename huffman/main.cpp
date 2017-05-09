#include <iostream>
#include <queue>
#include <string>
#include <unordered_map>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <assert.h>
#include "CycleTimer.h"

#define MAX_BIN 256
//#define DEBUG
//#define READ_FILE
//#define WRITE_FILE


unsigned char one[8]
    = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};


class HuffmanNode {
 public:
  HuffmanNode(unsigned long f)
      : parent(nullptr), freq(f) {}
  HuffmanNode(unsigned char c)
      : parent(nullptr), symbol(c), freq(0) {}
  HuffmanNode* parent;
  unsigned long freq;
  unsigned char symbol;
  bool left = false;

};

class HuffmanNodeComp
{
 public:
  bool operator() (const HuffmanNode* n1,const HuffmanNode* n2) const {
    return n1->freq > n2->freq;
  }
};

class HuffmanCode {
 public:
  HuffmanCode(uint8_t b, unsigned char* c)
      : numBits(b), bits(c) {}
  uint8_t numBits;
  unsigned char* bits;
};

class HuffmanTree {
 private:
  HuffmanNode* nodeMap[MAX_BIN];
  HuffmanNode** symbolList;
  unsigned symbolCnt;
  void* data;
  void* outData;

  HuffmanCode* codeMap[MAX_BIN];

 public:
  size_t dataSize;

  HuffmanTree() {
    symbolCnt = 0;
    for (size_t i = 0; i<MAX_BIN; i++) {
      nodeMap[i] = new HuffmanNode((unsigned char)i);
    }
    memset(codeMap, 0, sizeof(codeMap));

  }

  void Reset() {
    munmap(data, dataSize);
    for (int i=0; i<MAX_BIN; i++)
      delete nodeMap[i];
    delete[] symbolList;
  }

  ~HuffmanTree() {
    Reset();
  }

  void ReadFileToBuffer(std::string fileName) {
    struct stat sbuf;
    stat(fileName.c_str(), &sbuf);
    dataSize = sbuf.st_size;

#ifdef READ_FILE
    data = mmap(NULL, dataSize, PROT_READ,  MAP_SHARED, fd, 0);
#else
    data = new char[dataSize];
    outData = new char[dataSize];
    FILE * filp = fopen(fileName.c_str(), "rb");
    fread(data, sizeof(char), dataSize, filp);
    fclose(filp);
#endif
  }

  void GenerateHistogram(std::string fileName) {
    // Update frequency for each node
    for (int i = 0; i<dataSize; i++) {
      unsigned char symbol = ((unsigned char*)data)[i];
      nodeMap[symbol]->freq++;
    }

    // Sort to put valid symbols first
    std::sort(nodeMap, nodeMap+MAX_BIN, HuffmanNodeComp());

    while (nodeMap[symbolCnt++]->freq!=0);
    symbolCnt--;

    std::cout<<"Symbol cnt="<<symbolCnt<<std::endl;
    // Store the original symbol list
    symbolList = new HuffmanNode*[symbolCnt];
    std::copy(nodeMap, nodeMap+symbolCnt, symbolList);

#ifdef DEBUG
    for (int i=0; i<symbolCnt; i++)
      std::cout<<nodeMap[i]->symbol<<": "<<nodeMap[i]->freq<<std::endl;
    for (int i=0; i<symbolCnt; i++)
      std::cout<<symbolList[i]->symbol<<": "<<symbolList[i]->freq<<std::endl;
#endif
  }

  void BuildHuffmanTree() {
    for (int i=0, j=symbolCnt-1; i<symbolCnt-1; i++,j--) {
      auto first = nodeMap[j];
      auto second = nodeMap[j-1];
      nodeMap[j-1] =
      first->parent =
      second->parent =
          new HuffmanNode(first->freq+second->freq);
      // Set the left flag to indicate it is the left child
      first->left = true;
      nodeMap[j] = nullptr;
      std::sort(nodeMap, nodeMap+symbolCnt-i-1, HuffmanNodeComp());
    }

#ifdef DEBUG
    std::cout<<nodeMap[0]->symbol<<": "<<nodeMap[0]->freq<<std::endl;
#endif
  }

  HuffmanCode* GenerateCodeFromLeaf(HuffmanNode* node) {
    unsigned char bits[MAX_BIN];
    memset(bits, 0, sizeof(bits));

    uint8_t numBits = 0;
    auto parent = node->parent;
    while (parent != nullptr) {
      if (!node->left)
        bits[numBits] = 1;
      node = parent;
      parent = node->parent;
      numBits++;
    }

    unsigned char* code = new unsigned char[numBits];
    std::copy(bits, bits+numBits, code);

    return new HuffmanCode(numBits, code);
  }

  void ConstructHuffmanCode() {
    for (int i=0; i<symbolCnt; i++) {
      auto node = symbolList[i];
      codeMap[node->symbol] = GenerateCodeFromLeaf(node);

#ifdef DEBUG
      std::cout<<node->symbol<<": "<<(int)codeMap[node->symbol]->numBits<<std::endl;
      std::cout<<"\t";
      for (int j=0; j<codeMap[node->symbol]->numBits; j++)
        std::cout<<(int)(codeMap[node->symbol]->bits[j]);
      std::cout<<std::endl;
#endif
    }
  }

  size_t OutputCompressFile(std::string fileName) {
    size_t outputSize = 0;

#ifdef WRITE_FILE
    auto out = fopen(fileName.c_str(), "wb");
#else
    auto out = fmemopen(outData, dataSize, "wb");
#endif

    // Output metadata
    fwrite(&dataSize, sizeof(dataSize), 1, out); // File Length
    fwrite(&symbolCnt, sizeof(symbolCnt), 1, out); // Symbol Cnt
    outputSize += sizeof(dataSize)+sizeof(symbolCnt);
    for (int i=0; i<symbolCnt; i++) {  // Symbol and number of bits
      auto symbol = symbolList[i]->symbol;
      auto code = codeMap[symbol];
      fwrite(&symbol, 1, 1, out); // Symbol
      fwrite(&code->numBits, 1, 1, out); // Bit cnt
      outputSize+=2;
    }

    // Output code
    unsigned char curByte = 0;
    uint8_t bitCnt = 0;
    for (int i=0; i<symbolCnt; i++) {
      auto code = codeMap[symbolList[i]->symbol];
      for (int j=0; j<code->numBits; j++) {
        if (code->bits[j])
          curByte += one[bitCnt%8];

        // Write out the byte when it is full
        if (++bitCnt == 8) {
          fwrite(&curByte, 1, 1, out); // Symbol
          outputSize++;
          curByte = 0;
          bitCnt = 0;
        }
      }
    }

    // Compress file
    for (int i=0; i<dataSize; i++) {
      auto code = codeMap[((unsigned char*)data)[i]];
      for (int j = 0; j < code->numBits; j++) {
        if (code->bits[j])
          curByte += one[bitCnt % 8];

        // Write out the byte when it is full
        if (++bitCnt == 8) {
          fwrite(&curByte, 1, 1, out); // Symbol
          outputSize++;
          curByte = 0;
          bitCnt = 0;
        }
      }
    }

    if (bitCnt) {
      fwrite(&curByte, 1, 1, out); // Symbol
      outputSize++;
    }

    fclose(out);
    return outputSize;
  }

  void DecompressFile(std::string inFileName, std::string outFileName) {
    int fd = open(inFileName.c_str(), O_RDONLY);

    // mmap input file
    struct stat sbuf;
    stat(inFileName.c_str(), &sbuf);
    dataSize = sbuf.st_size;
    data = mmap(NULL, dataSize, PROT_READ,  MAP_SHARED, fd, 0);

    size_t rawDataSize = ((size_t *)data)[0];
    symbolCnt = ((unsigned*)data)[1];

  }

};

int main() {
//  char filename[100] = "/home/patrick/pagecounts-20160501-000000";

  char filename[100] = "./raw_input";
  HuffmanTree tree;

  std::cout << "Read file start" << std::endl;
  tree.ReadFileToBuffer(filename);
  std::cout << "Gen Histogram Elapse start" << std::endl;
  auto startTime = CycleTimer::currentSeconds();
  tree.GenerateHistogram(std::string(filename));
  auto endTime1 = CycleTimer::currentSeconds();
  std::cout << "Gen Histogram Elapse time = " << endTime1 - startTime << std::endl;


  tree.BuildHuffmanTree();
  auto endTime2 = CycleTimer::currentSeconds();
  std::cout << "Build Tree Elapse time = " << endTime2 - endTime1 << std::endl;


  tree.ConstructHuffmanCode();
  auto endTime3 = CycleTimer::currentSeconds();
  std::cout << "Construct Code Elapse time = " << endTime3 - endTime2 << std::endl;


  auto outputSize = tree.OutputCompressFile("./test_output");
  auto endTime4 = CycleTimer::currentSeconds();
  std::cout << "Compress File Elapse time = " << endTime4 - endTime3 << std::endl;


  std::cout << "Total Elapse time = " << endTime4 - startTime << std::endl;
  std::cout << "Compression Ratio = " << outputSize*1.0/tree.dataSize << std::endl;
//  tree.DecompressFile("../test_output", "../decompress_output");

  return 0;
}