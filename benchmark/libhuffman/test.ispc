export void prefix_sum(
    uniform unsigned int64 data[],
    uniform unsigned int64 size,
    uniform unsigned int64 output[]) {
  print ("%d", programIndex);
  for (unsigned int64 i = programIndex; i<size; i+=4) {
    output[i] = exclusive_scan_add(data[programIndex]);
  }
}