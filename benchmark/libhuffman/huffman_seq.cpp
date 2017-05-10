/*
 *  huffman - Encode/Decode files using Huffman encoding.
 *  http://huffman.sourceforge.net
 *  Copyright (C) 2003  Douglas Ryan Richardson
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util.h"
#include "huffman.h"
#include "util.h"

#ifdef WIN32
#include <winsock2.h>
#include <malloc.h>
#define alloca _alloca
#else
#include <netinet/in.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#endif

using std::cout;
using std::endl;

/****************** Helper functions ***********************/

static unsigned int get_symbol_frequencies(SymbolFrequencies *pSF, data_buf& buf) {
  int c;
  unsigned int total_count = 0;

  /* Set all frequencies to 0. */
  init_frequencies(pSF);

  /* Count the frequency of each symbol in the input file. */
  for (size_t i=0; i<buf.size; i++) {
    unsigned char uc = buf.data[i];
    if (!(*pSF)[uc])
      (*pSF)[uc] = new_leaf_node(uc);
    ++(*pSF)[uc]->count;
    ++total_count;
  }
  return total_count;
}

static void write_code_table_memory(data_buf& out_data_buf,
                             SymbolEncoder *se,
                             uint32_t symbol_count) {
  uint32_t i, count = 0;
  
  size_t curr_offset = 0;
  
  /* Determine the number of entries in se. */
  for (i = 0; i < MAX_SYMBOLS; ++i) {
    if ((*se)[i])
      ++count;
  }
  
  /* Write the number of entries in network byte order. */
  i = htonl(count);
  out_data_buf.write_data(&i, sizeof(i));
  
  /* Write the number of bytes that will be encoded. */
  symbol_count = htonl(symbol_count);
  out_data_buf.write_data(&symbol_count, sizeof(symbol_count));
  
  /* Write the entries. */
  for (i = 0; i < MAX_SYMBOLS; ++i) {
    huffman_code *p = (*se)[i];
    if (p) {
      unsigned int numbytes;
      /* The value of i is < MAX_SYMBOLS (256), so it can
       be stored in an unsigned char. */
      unsigned char uc = (unsigned char) i;
      
      /* Write the 1 byte symbol. */
      out_data_buf.write_data(&uc, sizeof(uc));
      
      /* Write the 1 byte code bit length. */
      uc = (unsigned char) p->numbits;
      
      out_data_buf.write_data(&uc, sizeof(uc));
      
      /* Write the code bytes. */
      numbytes = numbytes_from_numbits(p->numbits);
      
      out_data_buf.write_data(p->bits, numbytes);
    }
  }
}


static size_t get_out_size(data_buf& in_buf, SymbolEncoder *se) {
  size_t res = 0;
  
  // Calculate the size of symbol metadata
  // uint32_t for number of unique symbols
  res += 4;
  // uint32_t for number of bytes in the input file
  res += 4;
  for (int i = 0; i < MAX_SYMBOLS; ++i) {
    if ((*se)[i]) {
      // 1 byte for symbol, 1 byte for code bit length
      res += 2;
      // Code bytes;
      res += numbytes_from_numbits((*se)[i]->numbits);
    }
  }
  
  // Calculate the size of compressed file
  int cnt = 0;
  for (int i=0; i<in_buf.size; i++) {
    unsigned char uc = in_buf.data[i];
    huffman_code* code = (*se)[uc];
    cnt += code->numbits;
    res += cnt / 8;
    cnt = cnt % 8;
  }
  if (cnt)
    res++;
  return res;
}


static int do_encode(data_buf& in_buf, data_buf& out_buf, SymbolEncoder *se) {
  unsigned char curbyte = 0;
  unsigned char curbit = 0;

  for (int i_offset=0; i_offset<in_buf.size; i_offset++) {
    unsigned char uc = in_buf.data[i_offset];
    huffman_code *code = (*se)[uc];
    unsigned long i;
//    printf("%c", uc);


    for (i = 0; i < code->numbits; ++i) {
      /* Add the current bit to curbyte. */

      curbyte |= get_bit(code->bits, i) << curbit;

      /* If this byte is filled up then write it
       * out and reset the curbit and curbyte. */
      if (++curbit == 8) {
        out_buf.write_data(&curbyte, 1);
        curbyte = 0;
        curbit = 0;
      }
    }
  }

  /*
   * If there is data in curbyte that has not been
   * output yet, which means that the last encoded
   * character did not fall on a byte boundary,
   * then output it.
   */
  if (curbit > 0)
    out_buf.write_data(&curbyte, 1);

  return 0;
}

static huffman_node * read_code_table_memory(data_buf& buf, unsigned int& num_bytes) {
  // Read number of symbol count
  uint32_t count;
  buf.read_data(&count, sizeof(count));
  count = ntohl(count);

  // Read number of bytes in the original file
  buf.read_data(&num_bytes, sizeof(num_bytes));
  num_bytes = ntohl(num_bytes);

  // Read the symbols and build huffman tree
  huffman_node *root = new_nonleaf_node(0, NULL, NULL);
  while (count-- > 0) {
    huffman_node *p = root;
    
    // Read the symbol
    unsigned char symbol;
    buf.read_data(&symbol, sizeof(symbol));

    // Read number of symbol bits
    unsigned char numbits;
    buf.read_data(&numbits, sizeof(numbits));
    
    // Read the actual symbol bits
    unsigned char numbytes = (unsigned char) numbytes_from_numbits(numbits);
    unsigned char *bytes = new unsigned char[numbytes];
    buf.read_data(bytes, numbytes);
    
    // Traverse the huffman tree based on symbol bits
    // Create intermediate nodes and leaf nodes if missing
    for (unsigned int curbit = 0; curbit < numbits; ++curbit) {
      if (get_bit(bytes, curbit)) {
        if (p->one == NULL) {
          p->one = curbit == (unsigned char) (numbits - 1)
                   ? new_leaf_node(symbol)
                   : new_nonleaf_node(0, NULL, NULL);
          p->one->parent = p;
        }
        p = p->one;
      } else {
        if (p->zero == NULL) {
          p->zero = curbit == (unsigned char) (numbits - 1)
                    ? new_leaf_node(symbol)
                    : new_nonleaf_node(0, NULL, NULL);
          p->zero->parent = p;
        }
        p = p->zero;
      }
    }

    delete[] bytes;
  }

  return root;
}
                     
int huffman_encode_seq(data_buf& in_data_buf, data_buf& out_data_buf) {
  c_time[0] = CycleTimer::currentSeconds();

  // Get the frequency of each symbol in the input file.
  SymbolFrequencies sf;
  unsigned int symbol_count = get_symbol_frequencies(&sf, in_data_buf);
  
  c_time[1] = CycleTimer::currentSeconds();
  
  // Build an optimal table from the symbolCount.
  SymbolEncoder *se = calculate_huffman_codes(&sf);
  size_t out_size = get_out_size(in_data_buf, se);
  out_data_buf.data = new unsigned char[out_size];
  out_data_buf.size = out_size;
  out_data_buf.curr_offset = 0;
  
  c_time[2] = CycleTimer::currentSeconds();

  // Write symbol information into out_data_buf
  write_code_table_memory(out_data_buf, se, symbol_count);
  
  c_time[3] = CycleTimer::currentSeconds();
  
  // Encode file and write to out_data_buf
  do_encode(in_data_buf, out_data_buf, se);
  
  // By now, data_buf should all be used
  assert(out_data_buf.curr_offset == out_data_buf.size);
  
  c_time[4] = CycleTimer::currentSeconds();
  
  // Free the Huffman tree.
  free_huffman_tree(sf[0]);
  free_encoder(se);
  
  return 0;
}


int huffman_decode_seq(data_buf& in_data_buf, data_buf& out_data_buf) {
  d_time[0] = CycleTimer::currentSeconds();
  
  // Read the symbol list from input buffer and build Huffman Tree
  unsigned int data_count;
  huffman_node *root = read_code_table_memory(in_data_buf, data_count);
  
  d_time[1] = CycleTimer::currentSeconds();

  // Initialize output buffer
  out_data_buf.data = new unsigned char[data_count];
  out_data_buf.size = data_count;
  out_data_buf.curr_offset = 0;
  
  // Decode the file using Huffman Tree
  huffman_node *p = root;
  while (data_count > 0) {
    unsigned char byte;
    in_data_buf.read_data(&byte, sizeof(byte));
    
    unsigned char mask = 1;
    while (data_count > 0 && mask) {
      p = byte & mask ? p->one : p->zero;
      mask <<= 1;

      if (p->isLeaf) {
        out_data_buf.write_data(&p->symbol, sizeof(p->symbol));
        p = root;
        --data_count;
      }
    }
  }
  
  d_time[2] = CycleTimer::currentSeconds();
  
  // Free the Huffman Tree
  free_huffman_tree(root);
  return 0;
}

