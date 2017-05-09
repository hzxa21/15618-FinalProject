export void test_ispc_huffman(uniform int width, uniform int output[]) {
  foreach (i = 0 ... width) {
    output[i] = i;
  }

}