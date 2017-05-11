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
#include <iostream>

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
using std::min;

#define UPDIV(a,b) (((a)+(b)-1)/((b)))

//#define DEBUG
#ifndef DEBUG
#define printf(...)
#endif

size_t compressed_chunk_start_offset[NUM_CHUNKS];

static void
get_symbol_frequencies_parallel(SymbolFrequencies *pSF, data_buf& buf) {
  int c;
  uint64_t total_count = 0;

  uint64_t buf_chunk_size = UPDIV(buf.size, NUM_CHUNKS);
  int histo_chunk_size = UPDIV(MAX_SYMBOLS, NUM_CHUNKS);
  uint64_t* histo_per_thread = new uint64_t[NUM_CHUNKS*MAX_SYMBOLS];
  memset(histo_per_thread, 0L, NUM_CHUNKS*MAX_SYMBOLS*sizeof(uint64_t));

  /* Set all frequencies to 0. */
  init_frequencies(pSF);

  auto start_time = CycleTimer::currentSeconds();

#pragma omp parallel
  {
    int tid = omp_get_thread_num();

    // Which chunk of the buffer to read
    uint64_t start_offset = buf_chunk_size*tid;
    // Prevent branches in the loop
    uint64_t end_offset = std::min(start_offset+buf_chunk_size, buf.size);

    // Which memory location to write the private histogram
    int histo_id = MAX_SYMBOLS*tid;

    for (uint64_t i=start_offset; i<end_offset; i++) {
      histo_per_thread[histo_id+(buf.data[i])]++;
    }

#pragma omp barrier
    // Which chunk of the histogram to update
    start_offset = histo_chunk_size*tid;
    end_offset = std::min(start_offset+histo_chunk_size, (uint64_t)MAX_SYMBOLS);

    for (uint64_t i=start_offset; i<end_offset; i++) {
      uint64_t freq = 0;
      for (int j=0; j<NUM_CHUNKS; j++) {
        freq+=histo_per_thread[MAX_SYMBOLS*j+i];
      }
      if (freq) {
        (*pSF)[i] = new_leaf_node(i);
        (*pSF)[i]->count = freq;
      }
    }
  }

  delete[] histo_per_thread;

}

static void
get_symbol_frequencies(SymbolFrequencies *pSF, data_buf& buf) {
  int c;

  /* Set all frequencies to 0. */
  init_frequencies(pSF);

  /* Count the frequency of each symbol in the input file. */
  for (size_t i=0; i<buf.size; i++) {
    unsigned char uc = buf.data[i];
    if (!(*pSF)[uc])
      (*pSF)[uc] = new_leaf_node(uc);
    ++(*pSF)[uc]->count;
  }
}

void write_code_table_memory(data_buf& out_data_buf,
                             SymbolEncoder *se,
                             uint64_t symbol_count) {
  uint32_t i, count = 0;

  size_t curr_offset = 0;

  /* Determine the number of entries in se. */
  for (i = 0; i < MAX_SYMBOLS; ++i) {
    if ((*se)[i])
      ++count;
  }

  /* Write the number of entries in network byte order. */
  out_data_buf.write_data(&count, sizeof(count));
  printf("[DEBUG] Symbol Count = %d\n", count);


  /* Write the number of bytes that will be encoded. */
  out_data_buf.write_data(&symbol_count, sizeof(symbol_count));
  printf("[DEBUG] Offset after writing data_size = %ld\n", out_data_buf.curr_offset);


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


size_t get_out_size(data_buf& in_buf, SymbolEncoder *se) {
  size_t res = 0;
  size_t cnt = 0;
  size_t bytes_in_chunks[NUM_CHUNKS];
  #pragma omp parallel
  {
    int tid = omp_get_thread_num();
    bytes_in_chunks[tid] = 0;
    compressed_chunk_start_offset[tid] = 0;
    int cnt = 0;
    #pragma omp for schedule(static) nowait
    for (size_t i = 0; i < in_buf.size; i++) {
      unsigned char uc = in_buf.data[i];
      huffman_code *code = (*se)[uc];
      cnt += code->numbits;
    }
    bytes_in_chunks[tid] = (cnt+7)/8;
  }

  size_t sum = 0;
  compressed_chunk_start_offset[0] = 0;
  for (int i = 0; i<NUM_CHUNKS-1; i++) {
    sum+=bytes_in_chunks[i];
    compressed_chunk_start_offset[i+1] = sum;
  }
  res = NUM_CHUNKS*sizeof(size_t) + sum + bytes_in_chunks[NUM_CHUNKS-1];

  // Calculate the size of symbol metadata
  // uint32_t for number of unique symbols
  res += 4;
  // uint64_t for number of bytes in the input file
  res += 8;
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


static int do_encode(data_buf& in_buf, data_buf& out_buf, SymbolEncoder *se) {
  size_t chunk_size = (in_buf.size+NUM_CHUNKS-1)/NUM_CHUNKS;
#pragma omp parallel
  {
    unsigned char curbyte = 0;
    unsigned char curbit = 0;
    int tid = omp_get_thread_num();

    size_t start_offset = compressed_chunk_start_offset[tid];
    ((size_t*)(out_buf.data+out_buf.curr_offset))[tid] = start_offset;
    start_offset+=NUM_CHUNKS*sizeof(size_t);

    size_t i_offset = chunk_size*tid;
    size_t e_offset = std::min(i_offset+chunk_size, in_buf.size);

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

huffman_node * read_code_table_memory(data_buf& buf, uint64_t& num_bytes) {
  // Read number of symbol count
  uint32_t count;
  buf.read_data(&count, sizeof(count));

  // Read number of bytes in the original file
  buf.read_data(&num_bytes, sizeof(num_bytes));
  printf("[DEBUG] Offset after reading data_size = %ld\n", buf.curr_offset);


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

int huffman_encode_parallel(data_buf& in_data_buf, data_buf& out_data_buf, parallel_type type) {
  omp_set_num_threads(NUM_CHUNKS);
  printf("[DEBUG] Start Compression\n");
  c_time[0] = CycleTimer::currentSeconds();

  // Get the frequency of each symbol in the input file.
  SymbolFrequencies sf;
  uint64_t symbol_count = in_data_buf.size;
  printf("[DEBUG] Generate Histogram\n");
  if (type == parallel_type::OPENMP_NAIVE)
    get_symbol_frequencies(&sf, in_data_buf);
  else if (type == parallel_type::OPENMP_ParallelHistogram)
    get_symbol_frequencies_parallel(&sf, in_data_buf);
  printf("[DEBUG] Input Size = %ld\n", symbol_count);

  c_time[1] = CycleTimer::currentSeconds();
  printf("[DEBUG] Construct Huffman Codes\n");
  // Build an optimal table from the symbolCount.
  SymbolEncoder *se = calculate_huffman_codes(&sf);
  printf("[DEBUG] Get Output Size\n");
  size_t out_size = get_out_size(in_data_buf, se);
  printf("[DEBUG] Output Size = %ld, new output buffer\n", out_size);
  out_data_buf.data = new unsigned char[out_size];
  out_data_buf.size = out_size;
  out_data_buf.curr_offset = 0;
  
  c_time[2] = CycleTimer::currentSeconds();

  printf("[DEBUG] Write code table\n");
  // Write symbol table
  write_code_table_memory(out_data_buf, se, symbol_count);
  
  c_time[3] = CycleTimer::currentSeconds();

  printf("[DEBUG] Compress File\n");
  // Encode file
  do_encode(in_data_buf, out_data_buf, se);
  
  c_time[4] = CycleTimer::currentSeconds();
  printf("[DEBUG] Finish Compression\n");
  /* Free the Huffman tree. */
  free_huffman_tree(sf[0]);
  free_encoder(se);
  return 0;
}


int
huffman_decode_parallel(data_buf& in_data_buf, data_buf& out_data_buf, parallel_type type) {
  printf("[DEBUG] Start Decompression\n");

  d_time[0] = CycleTimer::currentSeconds();

  printf("[DEBUG] Read Code Table\n");

  // Read the symbol list from input buffer and build Huffman Tree
  size_t data_count;
  huffman_node *root = read_code_table_memory(in_data_buf, data_count);
  printf("[DEBUG] Output Size = %ld, new output buffer\n", data_count);

  d_time[1] = CycleTimer::currentSeconds();

  // Initialize output buffer
  out_data_buf.data = new unsigned char[data_count];
  out_data_buf.size = data_count;
  out_data_buf.curr_offset = 0;

  printf("[DEBUG] Decompres File\n");
  // Decode the file using Huffman Tree
  in_data_buf.read_data(compressed_chunk_start_offset, NUM_CHUNKS*sizeof(size_t));
  size_t o_chunk_size = (data_count+NUM_CHUNKS-1)/NUM_CHUNKS;
  #pragma omp parallel
  {
    huffman_node *p = root;
    int tid = omp_get_thread_num();
    size_t i_offset = compressed_chunk_start_offset[tid] + in_data_buf.curr_offset;

    size_t o_start_offset = o_chunk_size * tid;
    size_t o_end_offset = min(o_start_offset+o_chunk_size, (size_t)data_count);
    
    while (o_start_offset < o_end_offset) {
      unsigned char byte = in_data_buf.data[i_offset++];
      unsigned char mask = 1;
      while (o_start_offset < o_end_offset && mask) {
        p = byte & mask ? p->one : p->zero;
        mask <<= 1;

        if (p->isLeaf) {
          out_data_buf.data[o_start_offset++] = p->symbol;
          p = root;
        }
      }
    }
  }
  
  d_time[2] = CycleTimer::currentSeconds();
  printf("[DEBUG] Finish Decompression\n");

  free_huffman_tree(root);
  return 0;
}

