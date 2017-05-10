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
#include <stdexcept>
#include <exception>
#include <iostream>
#include "huffman.h"
#include "util.h"


using std::runtime_error;
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

static void run_huffman(string& infile_name, bool is_seq, parallel_type type = OPENMP_NAIVE) {
  // Read input file into memory
  struct stat sbuf;
  stat(infile_name.c_str(), &sbuf);
  size_t file_size = sbuf.st_size;
  FILE* in_file = fopen(infile_name.c_str(), "rb");
  if (in_file == NULL) {
    throw runtime_error("Input file does not exist.");
  }
  unsigned char* in_data = new unsigned char[file_size];
  size_t read_cnt = fread(in_data, 1, file_size, in_file);
  if (read_cnt != file_size) {
    throw runtime_error("Cannot read entire input file");
  }
  fclose(in_file);
  // Buffer that stores input file bytes
  data_buf in_buf(in_data, file_size);
  // Buffer that store compressed bytes
  data_buf tmp_buf;
  // Buffer that stores decompressed bytes. It should be the same as input bytes
  data_buf out_buf;
  
  // Encode input buffer to intermediate buffer
  string tmpfile_name;
  if (is_seq) {
    tmpfile_name = "compressed_seq";
    huffman_encode_seq(in_buf, tmp_buf);
  } else {
    tmpfile_name = "compressed_parallel";
    huffman_encode_parallel(in_buf, tmp_buf, type);
  }
  
  // Write the intermediate result to the file
  FILE* tmp_file = fopen(tmpfile_name.c_str(), "wb");
  fwrite(tmp_buf.data, 1, tmp_buf.size, tmp_file);
  fclose(tmp_file);
  
  // At this point input buffer can be deleted
  delete[] in_buf.data;
  
  // Rewind the offset pointer in tmp_buf back to the beginning
  tmp_buf.rewind();
  
  // Decompressed the intermediate bytes back to the original bytes
  string outfile_name;
  if (is_seq) {
    outfile_name = "decompressed_seq";
    huffman_decode_seq(tmp_buf, out_buf);
  } else {
    outfile_name = "decompressed_parallel";
    huffman_decode_parallel(tmp_buf, out_buf, type);
  }
  
  // Write the decompressed bytes to the file
  FILE* out_file = fopen(outfile_name.c_str(), "wb");
  fwrite(out_buf.data, 1, out_buf.size, out_file);
  fclose(out_file);
  
  // Correctness Check.
  int ret_code = system(("diff " + infile_name + " " + outfile_name).c_str());
  if (ret_code == 0) {
    cout << "Compression result is correct!!" << endl;
    cout << "Compression Ratio = " << tmp_buf.size * 1.0 / file_size << endl;
  } else {
    cout << "Error: Compression result is incorrect" << endl;
  }
  
  // Clean up
  delete[] tmp_buf.data;
  delete[] out_buf.data;
}

// Time statistics
double c_time[5];
double d_time[3];

// Given compress times and decompress times, print statistics
static void print_stats(double c_time[5], double d_time[3]) {
  // Print Compression Stats
  auto total_time = c_time[4] - c_time[0];
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
  cout << "Decompression Statistics:" << endl;
  cout << "\tTime to generate Huffman Tree = " << d_time[1] - d_time[0] << "s, "
  << get_percentage(total_time, d_time[1]- d_time[0]) << "%" << endl;
  cout << "\tTime to decompress file = " << d_time[2] - d_time[1] << "s, "
  << get_percentage(total_time, d_time[2]- d_time[1]) << "%" << endl;
  cout << "\tDecompression Elapse time = " << total_time << "s" << endl << endl;
}

static void print_summary(double c_time[5], double d_time[3], double pre_c_time[5], double pre_d_time[3]) {
  // Print environment setup and speedup
  cout << "************************* Summary *************************" << endl;
  cout << "Number of threads: " << NUM_CHUNKS << endl;
  double total_c_time = c_time[4] - c_time[0];
  double pre_total_c_time = pre_c_time[4] - pre_c_time[0];
  cout << "Compression speedup: " << pre_total_c_time / total_c_time << endl;
  double total_d_time = d_time[2] - d_time[0];
  double pre_total_d_time = pre_d_time[2] - pre_d_time[0];
  cout << "Decompression speedup: " << pre_total_d_time / total_d_time << endl;
  cout << "Total speedup: " << (pre_total_c_time + pre_total_d_time) /
      (total_c_time + total_d_time) << endl;
  cout << endl;
}

int main(int argc, char **argv) {
  /* Get the command line arguments. */
  int opt;
  string infile_name;
  while ((opt = getopt(argc, argv, "i:bhvmnt")) != -1) {
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


  double pre_c_time[5];
  double pre_d_time[3];
  
  /************ Start Benchmarking **************/
  // Run Sequential Version
  cout << "******************** Sequential Version *******************" << endl;
  run_huffman(infile_name, true);
  print_stats(c_time, d_time);


  memcpy(pre_c_time, c_time, sizeof(double)*5);
  memcpy(pre_d_time, d_time, sizeof(double)*3);

  // Run Parallel Version Next
  cout << "******************** Parallel Version (OPENMP_NAIVE)*********************" << endl;
  run_huffman(infile_name, false, OPENMP_NAIVE);
  print_stats(c_time, d_time);

  print_summary(c_time, d_time, pre_c_time, pre_d_time);

  memcpy(pre_c_time, c_time, sizeof(double)*5);
  memcpy(pre_d_time, d_time, sizeof(double)*3);

  // Run Parallel Version Next
  cout << "******************** Parallel Version (OPENMP_ParallelHistogram)*********************" << endl;
  run_huffman(infile_name, false, OPENMP_ParallelHistogram);
  print_stats(c_time, d_time);

  print_summary(c_time, d_time, pre_c_time, pre_d_time);

  return 0;
}
