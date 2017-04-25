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

#define MAX_BIN 256
#define DEBUG


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
    if (n1 == nullptr)
      return false;
    if (n2 == nullptr)
      return true;
    return n1->freq < n2->freq;
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
  unsigned char* data;
  size_t dataSize;

  HuffmanCode* codeMap[MAX_BIN];

 public:
  HuffmanTree() {
    symbolCnt = 0;
    memset(nodeMap, 0, sizeof(nodeMap));
    memset(codeMap, 0, sizeof(codeMap));

  }

  ~HuffmanTree() {
    munmap(data, dataSize);
    for (int i=0; i<symbolCnt; i++)
      delete nodeMap[i];
    delete[] symbolList;
  }

  void GenerateHistogram(std::string fileName) {
    int fd = open(fileName.c_str(), O_RDONLY);

    // mmap input file
    struct stat sbuf;
    stat(fileName.c_str(), &sbuf);
    dataSize = sbuf.st_size;
    data = (unsigned char*)mmap(NULL, dataSize, PROT_READ,  MAP_SHARED, fd, 0);

    // Update frequency for each node
    for (int i = 0; i<dataSize; i++) {
      unsigned char symbol = data[i];
      if (!nodeMap[symbol]) {
        nodeMap[symbol] = new HuffmanNode(symbol);
        symbolCnt++;
      }
      nodeMap[symbol]->freq++;
    }

    // Sort to put valid symbols first
    std::sort(nodeMap, nodeMap+MAX_BIN, HuffmanNodeComp());

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
    for (int i=0; i<symbolCnt-1; i++) {
      auto first = nodeMap[0];
      auto second = nodeMap[1];
      nodeMap[0] =
      first->parent =
      second->parent =
          new HuffmanNode(first->freq+second->freq);
      // Set the left flag to indicate it is the left child
      first->left = true;
      nodeMap[1] = nullptr;
      std::sort(nodeMap, nodeMap+MAX_BIN-i, HuffmanNodeComp());
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
      if (node->left)
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

  void OutputCompressFile(std::string fileName) {
    std::ofstream out;
    out.open(fileName.c_str(), std::ios_base::binary | std::ios_base::out);
    assert(out.is_open());


    // Output metadata
    out.write((char*)&dataSize, sizeof(dataSize)); // File Length
    out.write((char*)&symbolCnt, sizeof(symbolCnt)); // Symbol Cnt
    for (int i=0; i<symbolCnt; i++) {  // Symbol and number of bits
      auto symbol = symbolList[i]->symbol;
      auto code = codeMap[symbol];
      out.write((char*)&symbol, 1); // Symbol
      out.write((char*)&code->numBits, 1); // Bit cnt
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
          out.write((char*)&curByte, 1); // Symbol
          curByte = 0;
          bitCnt = 0;
        }
      }
    }

    // Compress file
    for (int i=0; i<dataSize; i++) {
      auto code = codeMap[data[i]];
      for (int j = 0; j < code->numBits; j++) {
        if (code->bits[j])
          curByte += one[bitCnt % 8];

        // Write out the byte when it is full
        if (++bitCnt == 8) {
          out.write((char *) &curByte, 1); // Symbol
          curByte = 0;
          bitCnt = 0;
        }
      }
    }

    if (bitCnt)
      out.write((char *) &curByte, 1); // Symbol

    out.close();
  }

};

int main() {
  char filename[100] = "../raw_input";
  HuffmanTree tree;
  tree.GenerateHistogram(std::string(filename));
  tree.BuildHuffmanTree();
  tree.ConstructHuffmanCode();
  tree.OutputCompressFile("../test_output");

  return 0;
}