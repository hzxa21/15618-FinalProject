/*
 *  huffman_coder - Encode/Decode files using Huffman encoding.
 *  http://huffman.sourceforge.net
 *  Copyright (C) 2003  Douglas Ryan Richardson
 */

#ifndef HUFFMAN_HUFFMAN_H
#define HUFFMAN_HUFFMAN_H

#include <stdio.h>
#include <stdint.h>
#include "CycleTimer.h"


typedef struct huffman_node_tag {
  unsigned char isLeaf;
  unsigned long count;
  struct huffman_node_tag *parent;

  union {
	struct {
	  struct huffman_node_tag *zero, *one;
	};
	unsigned char symbol;
  };
} huffman_node;

typedef struct huffman_code_tag {
  /* The length of this code in bits. */
  unsigned long numbits;

  /* The bits that make up this code. The first
     bit is at position 0 in bits[0]. The second
     bit is at position 1 in bits[0]. The eighth
     bit is at position 7 in bits[0]. The ninth
     bit is at position 0 in bits[1]. */
  unsigned char *bits;
} huffman_code;

struct data_buf {
  // Public functions
  data_buf() : data(NULL), size(0), curr_offset(0) {}
  data_buf(void* i_data, size_t& i_size) :
    data((unsigned char*)i_data), size(i_size), curr_offset(0) {}
  
  void write_data(void* address, size_t data_size) {
    assert(curr_offset + data_size <= size);
    memcpy(data + curr_offset, address, data_size);
    curr_offset += data_size;
  }
  
  void read_data(void* address, size_t data_size) {
    assert(curr_offset + data_size <= size);
    memcpy(address, data + curr_offset, data_size);
    curr_offset += data_size;
  }
  
  void rewind() {
    curr_offset = 0;
  }
  
  // Data Variables
  unsigned char* data;
  size_t size;
  unsigned int curr_offset;
};


#define MAX_SYMBOLS 256
typedef huffman_node *SymbolFrequencies[MAX_SYMBOLS];
typedef huffman_code *SymbolEncoder[MAX_SYMBOLS];

int huffman_encode_seq(data_buf& in_buf, data_buf& out_buf);
int huffman_decode_seq(data_buf& in_buf, data_buf& out_buf);

#endif
