/*
 *  huffman - Encode/Decode files using Huffman encoding.
 *  http://huffman.sourceforge.net
 *  Copyright (C) 2003  Douglas Ryan Richardson
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <omp.h>
#include "util.h"
#include "huffman.h"


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

#define NUM_CHUNKS 2

int compressed_chunk_start_offset[NUM_CHUNKS];


#define huffman_encode huffman_encode_seq
#define huffman_decode huffman_decode_seq

/*************************** New Added (Sequential) *******************************/
static unsigned int
get_symbol_frequencies(SymbolFrequencies *pSF, data_buf& buf) {
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

void write_code_table_memory(data_buf& out_data_buf,
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


size_t get_out_size(data_buf in_buf, SymbolEncoder *se) {
  size_t res = 0;
  int cnt = 0;
  omp_set_num_threads(NUM_CHUNKS);
  int bytes_in_chunks[NUM_CHUNKS];
#pragma omp parallel
  {
    int tid = omp_get_thread_num();
    bytes_in_chunks[tid] = 0;
    compressed_chunk_start_offset[tid] = 0;
    int cnt = 0;
    #pragma omp for schedule(static) nowait
    for (int i = 0; i < in_buf.size; i++) {
      unsigned char uc = in_buf.data[i];
      huffman_code *code = (*se)[uc];
      cnt += code->numbits;
    }
    bytes_in_chunks[tid] = (cnt+7)/8;
  }

  // TODO: Compute prefix sum and store in chunk_start_offsest (byte-level)
  int sum = 0;
  compressed_chunk_start_offset[0] = 0;
  for (int i = 0; i<NUM_CHUNKS-1; i++) {
    sum+=bytes_in_chunks[i];
    compressed_chunk_start_offset[i+1] = sum;
    printf("[%d]: %d\n",i+1,sum);
  }
  res = NUM_CHUNKS*sizeof(int) + sum + bytes_in_chunks[NUM_CHUNKS-1];

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

  return res;
}


static int
do_encode(data_buf& in_buf, data_buf& out_buf, SymbolEncoder *se) {


  printf("Start encoding, cur offset = %ld\n", out_buf.curr_offset);
  omp_set_num_threads(NUM_CHUNKS);
  int chunk_size = (in_buf.size+NUM_CHUNKS-1)/NUM_CHUNKS;
  #pragma omp parallel
  {
    unsigned char curbyte = 0;
    unsigned char curbit = 0;
    int tid = omp_get_thread_num();

    int start_offset = compressed_chunk_start_offset[tid];
    ((int*)(out_buf.data+out_buf.curr_offset))[tid] = start_offset;
    start_offset+=NUM_CHUNKS*sizeof(int);
    printf("[%d] Start offset = %d\n", tid, start_offset);

    int i_offset = chunk_size*tid;
    int e_offset = std::min(i_offset+chunk_size, (int)in_buf.size);
    for (; i_offset < e_offset; i_offset++) {
      unsigned char uc = in_buf.data[i_offset];
      huffman_code *code = (*se)[uc];
      unsigned long i;


      for (i = 0; i < code->numbits; ++i) {
        /* Add the current bit to curbyte. */

        curbyte |= get_bit(code->bits, i) << curbit;

        /* If this byte is filled up then write it
         * out and reset the curbit and curbyte. */
        if (++curbit == 8) {
          (out_buf.data+out_buf.curr_offset)[start_offset++] = curbyte;
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
      (out_buf.data+out_buf.curr_offset)[start_offset] = curbyte;
  }

  return 0;
}

static huffman_node *
read_code_table(data_buf& buf, unsigned int *pDataBytes, size_t& cur_offset) {
  huffman_node *root = new_nonleaf_node(0, NULL, NULL);
  uint32_t count = *((uint32_t*)(buf.data+cur_offset));
  cur_offset+=sizeof(uint32_t);

  count = ntohl(count);

  *pDataBytes = ntohl(  *pDataBytes = *((unsigned int*)(buf.data+cur_offset)));
  cur_offset+=sizeof(unsigned int);

  /* Read the entries. */
  while (count-- > 0) {
    int c;
    unsigned int curbit;
    unsigned char symbol;
    unsigned char numbits;
    unsigned char numbytes;
    unsigned char *bytes;
    huffman_node *p = root;

    symbol = buf.data[cur_offset++];
    numbits = buf.data[cur_offset++];
    numbytes = (unsigned char) numbytes_from_numbits(numbits);
    bytes = (unsigned char *) malloc(numbytes);
    memcpy(bytes, buf.data+cur_offset, numbytes);
    cur_offset+=numbytes;

    /*
     * Add the entry to the Huffman tree. The value
     * of the current bit is used switch between
     * zero and one child nodes in the tree. New nodes
     * are added as needed in the tree.
     */
    for (curbit = 0; curbit < numbits; ++curbit) {
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

    free(bytes);
  }

  return root;
}


huffman_node * read_code_table_memory(data_buf& buf, unsigned int& num_bytes) {
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

//int huffman_encode(const char *file_in, const char* file_out) {
int huffman_encode(data_buf& in_data_buf, data_buf& out_data_buf) {
  auto startTime = CycleTimer::currentSeconds();
  SymbolFrequencies sf;
  huffman_node *root = NULL;
  unsigned int symbol_count;

  printf("In size = %ld\n", in_data_buf.size);

  /* Get the frequency of each symbol in the input file. */
  symbol_count = get_symbol_frequencies(&sf, in_data_buf);
  auto endTime1 = CycleTimer::currentSeconds();

  /* Build an optimal table from the symbolCount. */
  SymbolEncoder *se = calculate_huffman_codes(&sf);
  auto endTime2 = CycleTimer::currentSeconds();

  /* Get output buffer size */
  size_t out_size = get_out_size(in_data_buf, se);
  auto endTime3 = CycleTimer::currentSeconds();
  std::cout << "Out size = " << out_size << std::endl;

  /* Allocate output buffer */
  out_data_buf.data = new unsigned char[out_size];
  out_data_buf.size = out_size;
  out_data_buf.curr_offset = 0;
  auto endTime4 = CycleTimer::currentSeconds();

  /* Write code table */
  write_code_table_memory(out_data_buf, se, symbol_count);
  auto endTime5 = CycleTimer::currentSeconds();

  /* Real encoding */
  do_encode(in_data_buf, out_data_buf, se);
  auto endTime6 = CycleTimer::currentSeconds();

  std::cout << "Gen Histogram Elapse time = " << endTime1 - startTime << std::endl;
  std::cout << "Get Output Size Time = " << endTime3 - endTime2 << std::endl;
  std::cout << "Allocate Output Buffer Time = " << endTime4 - endTime3 << std::endl;
  std::cout << "Compress File time = " << endTime6 - endTime5 << std::endl;
  std::cout << "Total Elapse time = " << endTime6 - startTime << std::endl;

  printf("Compress Ratio = %f\n", out_size * 1.0 / in_data_buf.size);


  /* Free the Huffman tree. */
  free_huffman_tree(sf[0]);
  free_encoder(se);
  return 0;
}


int
huffman_decode(data_buf& in_data_buf, data_buf& out_data_buf) {
  printf("Start decompress\n");

  // Read the symbol list from input buffer and build Huffman Tree
  unsigned int data_count;
  huffman_node *root = read_code_table_memory(in_data_buf, data_count);

  // Initialize output buffer
  out_data_buf.data = new unsigned char[data_count];
  out_data_buf.size = data_count;
  out_data_buf.curr_offset = 0;

  // Decode the file using Huffman Tree
  huffman_node *p = root;
  printf("In size = %ld\n", in_data_buf.size);

  /* Decode the file. */
  in_data_buf.read_data(compressed_chunk_start_offset, NUM_CHUNKS*sizeof(int));
  printf("Out size = %ld\n", data_count);
  int o_chunk_size = (data_count+NUM_CHUNKS-1)/NUM_CHUNKS;
  omp_set_num_threads(NUM_CHUNKS);
#pragma omp parallel
  {
    int tid = omp_get_thread_num();
    printf("[%d] compressed_chunk_start_offset = %d\n", tid, compressed_chunk_start_offset[tid]);
    int i_offset = compressed_chunk_start_offset[tid]+in_data_buf.curr_offset;
    printf("[%d] Current offset = %d\n", tid, i_offset);

    size_t o_start_offset = o_chunk_size * tid;
    size_t o_end_offset = std::min(o_start_offset+o_chunk_size, (size_t)data_count);
//    printf("[%d] o_start_offset = %d\n", tid, o_start_offset);
//    printf("[%d] o_end_offset = %d\n", tid, o_end_offset);

    while (o_start_offset < o_end_offset) {
      if (i_offset >= in_data_buf.size)
        printf("[%d] Overflow in\n", tid);
      unsigned char byte = in_data_buf.data[i_offset++];
      unsigned char mask = 1;
      while (o_start_offset < o_end_offset && mask) {
        if (o_start_offset >= data_count)
          printf("[%d] Overflow out\n", tid);
        if (byte & mask) {
          if (!p->one)
            printf("NULL at 1\n");
        }
        else if (!p->zero)
          printf("NULL at 0\n");

        p = byte & mask ? p->one : p->zero;
        mask <<= 1;

        if (p->isLeaf) {
          out_data_buf.data[o_start_offset++] = p->symbol;
          p = root;
        }
      }
    }
  }
  printf("Finish decompress\n");



  free_huffman_tree(root);
  return 0;
}

