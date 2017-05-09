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

int huffman_encode_memory(const unsigned char *bufin,
                          uint32_t bufinlen,
                          unsigned char **pbufout,
                          uint32_t *pbufoutlen) {}
int huffman_decode_memory(const unsigned char *bufin,
                          uint32_t bufinlen,
                          unsigned char **bufout,
                          uint32_t *pbufoutlen) {}

int huffman_encode_file(FILE *in, FILE *out){}
int huffman_decode_file(FILE *in, FILE *out){}
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

// Return offset
static long
write_code_table(int out_fd, SymbolEncoder *se, uint32_t symbol_count) {
  size_t cur_offset = 0;

  uint32_t i, count = 0;

  /* Determine the number of entries in se. */
  for (i = 0; i < MAX_SYMBOLS; ++i) {
    if ((*se)[i])
      ++count;
  }

  /* Write the number of entries in network byte order. */
  i = htonl(count);

  if (write(out_fd, &i, sizeof(i)) < 0)
    return -1;

  /* Write the number of bytes that will be encoded. */
  symbol_count = htonl(symbol_count);
  if (write(out_fd, &symbol_count, sizeof(symbol_count)) < 0)
    return -1;

  cur_offset += sizeof(symbol_count) + sizeof(i);

  /* Write the entries. */
  for (i = 0; i < MAX_SYMBOLS; ++i) {
    huffman_code *p = (*se)[i];
    if (p) {
      unsigned int numbytes;
      /* Write the 1 byte symbol. */
      write(out_fd, &i, 1);
      /* Write the 1 byte code bit length. */
      write(out_fd, &(p->numbits), 1);
      /* Write the code bytes. */
      numbytes = numbytes_from_numbits(p->numbits);
      if (write(out_fd, p->bits, numbytes) < 0)
        return -1;
      cur_offset+= numbytes+2;
    }
  }

  return cur_offset;
}

size_t get_out_size(data_buf in_buf, SymbolEncoder *se) {
  size_t res = 0;
  int cnt = 0;
  omp_set_num_threads(8);
  int output[8];
  memset(output, 0, sizeof(output));
#pragma omp parallel
  {
    int cnt = 0;
    #pragma omp for schedule(static) nowait
    for (int i = 0; i < in_buf.size; i++) {
      unsigned char uc = in_buf.data[i];
      huffman_code *code = (*se)[uc];
      cnt += code->numbits;
    }
    output[omp_get_thread_num()] = cnt;
  }
  for (int i=0; i<8; i++) {
    printf("%d\n", output[i]);
    res += output[i];
  }
  res = (res+7)/8;

  return res;
}


static int
do_encode(data_buf& in_buf, data_buf& out_buf, SymbolEncoder *se) {
  unsigned char curbyte = 0;
  unsigned char curbit = 0;
  int o_offset = 0;

  printf("Start encoding\n");

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
        out_buf.data[o_offset++] = curbyte;
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
    out_buf.data[o_offset] = curbyte;

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

int
huffman_encode(const char *file_in, const char* file_out) {
  auto startTime = CycleTimer::currentSeconds();
  SymbolFrequencies sf;
  SymbolEncoder *se;
  huffman_node *root = NULL;
  unsigned int symbol_count;

  struct stat sbuf;
  stat(file_in, &sbuf);
  size_t in_size = sbuf.st_size;
  printf("In size = %ld\n", in_size);
  int in_fd = open(file_in, O_RDONLY);
  void* in_data = mmap(NULL, in_size, PROT_READ, MAP_SHARED, in_fd, 0);
  auto in_data_buf = data_buf(in_data, in_size);

  /* Get the frequency of each symbol in the input file. */
  symbol_count = get_symbol_frequencies(&sf, in_data_buf);
  auto endTime1 = CycleTimer::currentSeconds();
  std::cout << "Gen Histogram Elapse time = " << endTime1 - startTime << std::endl;

  /* Build an optimal table from the symbolCount. */
  se = calculate_huffman_codes(&sf);
  root = sf[0];

  auto endTime3 = CycleTimer::currentSeconds();

  int out_fd = open(file_out, O_WRONLY);

  /* Scan the file again and, using the table
     previously built, encode it into the output file. */
  int rc;
  size_t offset = write_code_table(out_fd, se, symbol_count);
  size_t out_size;
  if (offset >= 0) {
    out_size = get_out_size(in_data_buf, se);
    std::cout << "Out size = " << out_size << std::endl;
//    void* out_data = mmap(NULL, out_size, PROT_READ | PROT_WRITE, MAP_SHARED, out_fd, offset);
//    if (out_data == MAP_FAILED) {
//      printf("Map fail\n");
//      return -1;
//    }
    void* out_data = malloc(out_size);
    auto out_data_buf = data_buf(out_data, out_size);
    rc = do_encode(in_data_buf, out_data_buf, se);
    auto endTime5 = CycleTimer::currentSeconds();
    write(out_fd, out_data, out_size);
    std::cout << "Write Elapse time = " << CycleTimer::currentSeconds() - endTime5 << std::endl;

  }

  close(out_fd);
  close(in_fd);

  auto endTime4 = CycleTimer::currentSeconds();
  std::cout << "Compress File Elapse time = " << endTime4 - endTime3 << std::endl;

  std::cout << "Total Elapse time = " << endTime4 - startTime << std::endl;

  printf("Compress Ratio = %f\n", out_size*1.0/in_size);


  /* Free the Huffman tree. */
  free_huffman_tree(root);
  free_encoder(se);
  return rc;
}


int
huffman_decode(const char* file_in, const char* file_out) {
  huffman_node *root, *p;
  int c;
  unsigned int data_count;

  /* Read the Huffman code table. */
  struct stat sbuf;
  stat(file_in, &sbuf);
  size_t in_size = sbuf.st_size;
  printf("In size = %ld\n", in_size);
  int in_fd = open(file_in, O_RDONLY);
  void* in_data = mmap(NULL, in_size, PROT_READ, MAP_SHARED, in_fd, 0);
  auto in_data_buf = data_buf(in_data, in_size);
  size_t cur_offset = 0;
  root = read_code_table(in_data_buf, &data_count, cur_offset);
  if (!root)
    return 1;

  /* Decode the file. */
  p = root;
  size_t out_size = data_count;
  void* out_data = malloc(out_size);
  auto out_data_buf = data_buf(out_data, out_size);
  size_t o_offset = 0;
  while (data_count > 0) {
    unsigned char byte = in_data_buf.data[cur_offset++];
    unsigned char mask = 1;
    while (data_count > 0 && mask) {
      p = byte & mask ? p->one : p->zero;
      mask <<= 1;

      if (p->isLeaf) {
        out_data_buf.data[o_offset++] = p->symbol;
        p = root;
        --data_count;
      }
    }
  }

  int out_fd = open(file_out, O_WRONLY);
  write(out_fd, out_data, out_size);
  close(out_fd);

  free_huffman_tree(root);
  return 0;
}

