/*
 *  huffcode - Encode/Decode files using Huffman encoding.
 *  http://huffman.sourceforge.net
 *  Copyright (C) 2003  Douglas Ryan Richardson
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "huffman.h"

using std::string;

#ifdef WIN32
#include <malloc.h>
extern int getopt(int, char **, char *);
extern char *optarg;
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

//#define READ_FILE
#define WRITE_FILE

static int memory_encode_file(FILE *in, FILE *out);
static int memory_decode_file(FILE *in, FILE *out);

static void version(FILE *out) {
  fputs(
      "huffcode 0.3\n"
      "Copyright (C) 2003 Douglas Ryan Richardson"
      "; Gauss Interprise, Inc\n"
      "Modified by: Chen Luo and Patrick Huang",
      out);
}

static void usage(FILE *out) {
  // Sample usage to run benchmarking. ./huffmancode -b -i input_file
  fputs(
      "Usage: huffcode [-i<input file>] [-o<output file>] [-d|-c]\n"
      "-i - input file (default is standard input)\n"
      "-o - output file (default is standard output)\n"
      "-d - decompress\n"
      "-c - compress (default)\n"
      "-b - Benchmarking. Run parallel version and compared against seq version\n",
      out);
}

int main(int argc, char **argv) {
  char memory = 0;
  char compress = 1;
  int opt;
  bool is_benchmark = false;
  string infile_name;
  string outfile_name;

  /* Get the command line arguments. */
  while ((opt = getopt(argc, argv, "i:o:cbdhvmn")) != -1) {
    switch (opt) {
      case 'i':
        infile_name = string(optarg);
        break;
      case 'o':
        outfile_name = string(optarg);
        break;
      case 'b':
        is_benchmark = true;
        break;
      case 'c':
        compress = 1;
        break;
      case 'd':
        compress = 0;
        break;
      case 'h':
        usage(stdout);
        return 0;
      case 'v':
        version(stdout);
        return 0;
      default:
        usage(stderr);
        return 1;
    }
  }

  void *inbuf, *outbuf;
  size_t dataSize = 0;

  // Input file name cannot be empty
  if (infile_name.empty()) {
    usage(stderr);
    return 1;
  }
  
  // Read input file into memory
  struct stat sbuf;
  stat(infile_name.c_str(), &sbuf);
  size_t file_size = sbuf.st_size;
  FILE* in_file = fopen(infile_name.c_str(), "rb");
  unsigned char* in_data = new unsigned char[file_size];
  fread(in_data, 1, file_size, in_file);
  data_buf in_buf(in_data, file_size);
  data_buf out_buf;
  
  // Doing benchmarking
  if (is_benchmark) {
    // Run sequential version first
    huffman_encode(in_buf, out_buf);
    
    // TODO: delete this
    FILE* out_file = fopen(outfile_name.c_str(), "wb");
    fwrite(out_buf.data, 1, out_buf.size, out_file);
    fclose(out_file);
    fclose(in_file);
  }
  
  return 0;
    
//  /* If an input file is given then open it. */
//  if (file_in) {
//    struct stat sbuf;
//    stat(file_in, &sbuf);
//    dataSize = sbuf.st_size;
//#ifdef READ_FILE
//    in = fopen(file_in, "rb");
//#else
//    inbuf = new unsigned char[dataSize];
//    outbuf = new unsigned char[dataSize];
//    FILE *filp = fopen(file_in, "rb");
//    fread(inbuf, sizeof(unsigned char), dataSize, filp);
//    fclose(filp);
//    in = fmemopen(inbuf, dataSize, "rb");
//#endif
//    if (!in) {
//      fprintf(stderr, "Can't open input file '%s': %s\n", file_in,
//              strerror(errno));
//      return 1;
//    }
//  }
//
//  /* If an output file is given then create it. */
//  if (file_out) {
//#ifdef WRITE_FILE
//    out = fopen(file_out, "wb");
//#else
//    out = fmemopen(outbuf, dataSize, "wb");
//
//#endif
//    if (!out) {
//      fprintf(stderr, "Can't open output file '%s': %s\n", file_out,
//              strerror(errno));
//      return 1;
//    }
//  }
//
//  if (memory) {
//    return compress ? memory_encode_file(in, out) : memory_decode_file(in, out);
//  }
//
//  if (new_flag)
//    return compress ? huffman_encode(file_in, file_out)
//                    : huffman_decode(file_in, file_out);
//
//  return compress ? huffman_encode_file(in, out) : huffman_decode_file(in, out);
}

static int memory_encode_file(FILE *in, FILE *out) {
  unsigned char *buf = NULL, *bufout = NULL;
  unsigned int len = 0, cur = 0, inc = 1024, bufoutlen = 0;

  assert(in && out);

  /* Read the file into memory. */
  while (!feof(in)) {
    unsigned char *tmp;
    len += inc;
    tmp = (unsigned char *)realloc(buf, len);
    if (!tmp) {
      if (buf) free(buf);
      return 1;
    }

    buf = tmp;
    cur += fread(buf + cur, 1, inc, in);
  }

  if (!buf) return 1;

  /* Encode the memory. */
  if (huffman_encode_memory(buf, cur, &bufout, &bufoutlen)) {
    free(buf);
    return 1;
  }

  free(buf);

  /* Write the memory to the file. */
  if (fwrite(bufout, 1, bufoutlen, out) != bufoutlen) {
    free(bufout);
    return 1;
  }

  free(bufout);

  return 0;
}

static int memory_decode_file(FILE *in, FILE *out) {
  unsigned char *buf = NULL, *bufout = NULL;
  unsigned int len = 0, cur = 0, inc = 1024, bufoutlen = 0;
  assert(in && out);

  /* Read the file into memory. */
  while (!feof(in)) {
    unsigned char *tmp;
    len += inc;
    tmp = (unsigned char *)realloc(buf, len);
    if (!tmp) {
      if (buf) free(buf);
      return 1;
    }

    buf = tmp;
    cur += fread(buf + cur, 1, inc, in);
  }

  if (!buf) return 1;

  /* Decode the memory. */
  if (huffman_decode_memory(buf, cur, &bufout, &bufoutlen)) {
    free(buf);
    return 1;
  }

  free(buf);

  /* Write the memory to the file. */
  if (fwrite(bufout, 1, bufoutlen, out) != bufoutlen) {
    free(bufout);
    return 1;
  }

  free(bufout);

  return 0;
}
