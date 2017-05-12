export void prefix_sum(
    uniform unsigned int64 data[],
    uniform unsigned int64 size,
    uniform unsigned int64 output[]) {
  print ("%\n", programIndex);
  output[programIndex] = exclusive_scan_add(data[programIndex]);
  uniform unsigned int64 last_index = 15;
  for (unsigned int64 i = programIndex+16; i<size; i+=16) {
    output[i] = exclusive_scan_add(data[i]) + output[last_index]+data[last_index];
    last_index += 16;
    print ("%\n", last_index);

  }
}

export void divide_mod_8(
    uniform unsigned int64 data[],
    uniform unsigned int64 size,
    uniform unsigned int64 divide[],
    uniform unsigned int64 mod[]) {
  uniform unsigned int64 eight = 8;
  for (unsigned int64 i = programIndex; i<size; i+=16) {
    mod[i] = (data[i])%eight;
    divide[i] = (data[i])/eight;
  }
}

export void mark_one(uniform unsigned int64 input[], uniform unsigned int64 size, uniform unsigned int64 output[]) {
  for (unsigned int64 i = programIndex; i<size-1; i+=16) {
    if (input[i] != input[i+1])
      output[i] = 1;
  }
}

export void write_end_offset(
    uniform unsigned int64 input[],
    uniform unsigned int64 size,
    uniform unsigned int64 output[]) {

  for (unsigned int64 i = programIndex; i<size-1; i+=16) {
    if (input[i] != input[i+1])
      output[input[i]] = i+1;
  }
}

export void write_bytes(
    uniform unsigned int64 codes[],
    uniform unsigned int8 data[],
    uniform unsigned int64 index[],
    uniform unsigned int64 size,
    uniform unsigned int64 output[]) {
  for (unsigned int64 i = programIndex; i<size-1; i+=16) {
    int8 byte = data[i];
    int64* huffman_code_ptr = (int64*)codes[byte];
    int64 numbits =

  }
}


  export void test_struct(uniform int64 s[]) {
  if (programIndex == 0) {

    print("%\n", ((int8*)(s[1]))[2]);
  }

}