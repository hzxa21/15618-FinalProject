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
#include <iostream>
#include "huffman.h"
#include "util.h"

using std::string;
using std::cout;
using std::endl;

#ifdef WIN32
#include <malloc.h>
extern int getopt(int, char **, char *);
extern char *optarg;
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

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
      "Usage: huffcode -i <input file>\n"
      "-i - input file. Will compress and decompress it\n"
      "-h - print usage information\n",
      out);
}

static void run_huffman(data_buf& in_buf, bool is_seq) {
  // Buffer that store compressed bytes
  data_buf tmp_buf;
  // Buffer that stores decompressed bytes. It should be the same as input bytes
  data_buf out_buf;
  
  // Encode input buffer to intermediate buffer
  if (is_seq)
    huffman_encode_seq(in_buf, tmp_buf);
  else
    huffman_encode_parallel(in_buf, tmp_buf);
  
  // Write the intermediate result to a file called "compressed"
  FILE* tmp_file = fopen(is_seq ? "compressed_seq" : "compressed_parallel", "wb");
  fwrite(tmp_buf.data, 1, tmp_buf.size, tmp_file);
  fclose(tmp_file);
  
  // Rewind the offset pointer in out_buf back to the beginning
  tmp_buf.rewind();
  
  // Decompressed the intermediate bytes back to the original bytes
  if (is_seq)
    huffman_decode_seq(tmp_buf, out_buf);
  else
    huffman_decode_parallel(tmp_buf, out_buf);
  
  // Correctness Check.
  assert(in_buf.size == out_buf.size);
  int res = memcmp(in_buf.data, out_buf.data, in_buf.size);
  if (res == 0) {
    cout << "Compression result is correct!!" << endl;
    cout << "Compression Ratio = " << tmp_buf.size * 1.0 / in_buf.size << endl;
  } else {
    cout << "Compression result is incorrect!!" << endl;
  }
}

// Time statistics
double c_time_seq[5];
double d_time_seq[3];
double c_time_p[5];
double d_time_p[3];

// Given compress times and decompress times, print statistics
static void print_stats(double c_time[5], double d_time[3]) {
  // Print Compression Stats
  auto total_time = c_time[4] - c_time[0];
//  cout << "*********** Compression Statistics ************" << endl;
  cout << "Compression Statistics:" << endl;
  cout << "\tTime to generate Histogram = " << c_time[1] - c_time[0] << "s, "
    << get_percentage(total_time, c_time[1]- c_time[0]) << "%"<< endl;
  cout << "\tTime to generate Huffman Tree = " << c_time[2] - c_time[1] << "s, "
    << get_percentage(total_time, c_time[2] - c_time[1]) << "%" << endl;
  cout << "\tTime to write symbol list = " << c_time[3] - c_time[2] << "s, "
    << get_percentage(total_time, c_time[3] - c_time[2]) << "%" << endl;
  cout << "\tTime to compress file = " << c_time[4] - c_time[3] << "s, "
    << get_percentage(total_time, c_time[4] - c_time[3]) << "%" << endl;
  cout << "\tCompression Elapse time = " << total_time << "s" <<endl << endl;
  
  // Print Decompression Stats
  total_time = d_time[2] - d_time[0];
//  cout << "*********** Decompression Statistics ************" << endl;
  cout << "Decompression Statistics:" << endl;
  cout << "\tTime to generate Huffman Tree = " << d_time[1] - d_time[0] << "s, "
  << get_percentage(total_time, d_time[1]- d_time[0]) << "%" << endl;
  cout << "\tTime to decompress file = " << d_time[2] - d_time[1] << "s, "
  << get_percentage(total_time, d_time[2]- d_time[1]) << "%" << endl;
  cout << "\tDecompression Elapse time = " << total_time << "s" << endl << endl;
}

int main(int argc, char **argv) {
  /* Get the command line arguments. */
  int opt;
  string infile_name;
  while ((opt = getopt(argc, argv, "i:bhvmn")) != -1) {
    switch (opt) {
      case 'i':
        infile_name = string(optarg);
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
  if (in_file == NULL) {
    cout << "Input file " << infile_name << " does not exist." << endl;
    return 1;
  }
  unsigned char* in_data = new unsigned char[file_size];
  size_t read_cnt = fread(in_data, 1, file_size, in_file);
  if (read_cnt != file_size) {
    cout << "Cannot read entire input file" << endl;
    return 1;
  }
  fclose(in_file);
  // Buffer that stores input file bytes
  data_buf in_buf(in_data, file_size);
  
  
  
  /************ Start Benchmarking **************/
  // Run Sequential Version
  cout << "******************** Sequential Version *******************" << endl;
  run_huffman(in_buf, true);
  print_stats(c_time_seq, d_time_seq);
  
  // Run Parallel Version Next
  cout << "******************** Parallel Version *********************" << endl;
  run_huffman(in_buf, false);
  print_stats(c_time_p, d_time_p);
  
  
  // Print environment setup and speedup
  cout << "************************* Summary *************************" << endl;
  cout << "Number of threads: " << NUM_CHUNKS << endl;
  double total_c_time_seq = c_time_seq[4] - c_time_seq[0];
  double total_c_time_p = c_time_p[4] - c_time_p[0];
  cout << "Compression speedup: " << total_c_time_seq / total_c_time_p << endl;
  double total_d_time_seq = d_time_seq[2] - d_time_seq[0];
  double total_d_time_p = d_time_p[2] - d_time_p[0];
  cout << "Decompression speedup: " << total_d_time_seq / total_d_time_p << endl;
  cout << "Total speedup: " << (total_c_time_seq + total_d_time_seq) /
      (total_c_time_p + total_d_time_p) << endl;
  
  return 0;
}
