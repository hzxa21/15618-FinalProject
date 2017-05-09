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
  data_buf(void* i_data, size_t& i_size) : data((unsigned char*)i_data), size(i_size) {}
  unsigned char* data;
  size_t size;
};

int huffman_encode_file(FILE *in, FILE *out);
int huffman_decode_file(FILE *in, FILE *out);
int huffman_encode_memory(const unsigned char *bufin,
						  uint32_t bufinlen,
						  unsigned char **pbufout,
						  uint32_t *pbufoutlen);
int huffman_decode_memory(const unsigned char *bufin,
						  uint32_t bufinlen,
						  unsigned char **bufout,
						  uint32_t *pbufoutlen);

int huffman_encode(const char*, const char*);
int huffman_decode(const char*, const char*);

#endif
