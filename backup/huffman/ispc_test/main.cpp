#include <stdio.h>
#include <cstring>
#include <iostream>
#include "test_ispc.h"

int main(int argc, char** argv) {
  int* output = new int[1024];
  memset(output, 0, 1024);
  int n = 1024;
  ispc::test_ispc_huffman(n, output);
  for (int i=0; i<1024; i++)
    std::cout << output[i] << " ";

}