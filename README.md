# Final Checkpoint & Preliminary Result
![](https://raw.githubusercontent.com/hzxa21/15618-FinalProject/master/result/Speeup.png?token=AFQ05kXhtPEQfoj6OwCFKcoVMxhRYBR1ks5ZHWhswA%3D%3D)
![](https://raw.githubusercontent.com/hzxa21/15618-FinalProject/master/result/CompressionTime.png?token=AFQ05hnTC-686TBjZuATNWBCPjG-k-pTks5ZHWiKwA%3D%3D)

---------------------------------------
# Project Middle Checkpoint (Apr 25th)
## Project Status Summary
Since the beginning of the project, here are some of the progress we made.
#### Huffman Compression
- Finish researching and reading literatures on the implementation and current optimizations on the Huffman compression algorithms. The workflow of Huffman encoding mainly contains two pass of the input data and four stages: Generate frequency histogram for all the input bytes (1st pass), Build huffman tree based on the histogram, Construct huffman code for all the bytes in the histogram bin, Encode the original data into huffman code (2st pass). We used libhuffman, an open source sequential version of the huffman algorithm as the reference implementation and benchmark [2].
- Finish the naive implementation of huffman encoding. Specifically, we refactor the libhuffman code to fix some performance bug, remove the useless codes and make it object oriented. We also add timer to measure the percentage of time spent in different stage of huffman encoding using Wikipedia dataset. The results show that a majority of time is spent on the second pass of the data to convert original bytes into huffman codes and generating frequency histogram.

![](https://docs.google.com/spreadsheets/d/148EUsqlUiJGkqb_YZeRRp0t6aaDMeHU65LFHYP7BZWs/pubchart?oid=130625852&format=image)

- Next Step. We are implementing the parallel huffman encoding algorithm focusing on the bottleneck of encoding data in the second pass and generating histogram in the first pass. We are trying different approaches. For example, dividing the data into blocks and creating threads to do the whole huffman encoding workflow independently, which may achieve good speed-up but may decrease the compression ratio. We are also trying to use data parallelism only for histogram generation and data encoding so different blocks of the data are using the same huffman tree to construct huffman code, which will not hurt the compression ratio. We will also try to parallelize the huffman decoding algorithm which need to reconstruct the huffman code mapping and decompress the encoded data.

#### LZSS Compression
- Understand the details of LZSS algorithm. We have looked at some literatures and understood some of the optimizations that can be done for LZSS algorithm such as packing the encoded bits into a single byte. We also found out the trade off between different string comparison algorithms. Specifically, the easiest algorithm is linear search, where we compare second string with each substr within first string sequentially. There are also other algorithms such as using linked list or hash table to speed up this process. 
- Finished the sequential version of the algorithm. We found an sequential implementation of LZSS online [1] and did some modifications to it. Specifically, we add timer to identify the bottleneck, fixed some existing bugs, and make it more object oriented. In the sequential version, there are three main steps. First step is to read bytes from file and put them in a lookahead buffer. Secondly, compare bytes in lookahead buffer with bytes in the sliding window and find the longest match. Thirdly, write the encoded offset + length to the output file and shift the sliding window to include matched characters. In theory, the first step and third step are mainly doing sequential file I/O, most of the time will be spent on the second step, which is doing string match. However, we found that most of the time is spent in the first step. It turns out that it’s a problem in the implementation where the program only read one byte from file at a time. 

## Deliverables on the Competition
- Speed-up graph on parallel Huffman and LZSS compression vs. the sequential implementation with different number of cores using different datasets.
- Speed-up graph on parallel Huffman and LZSS decompression vs. the sequential implementation with different number of cores using different datasets.
- Space and speed overhead graph of the compression algorithm when the input data is relative small.
- Comparison of different parameter settings for the compression algorithm (i.e. number of bits used for the compression unit in Huffman compression).

## Issues & Concerns
#### Huffman Compression
All huffman algorithms are using a byte (8-bit) as the basic unit of the histogram, in other words, the maximum number of different leaves in the huffman tree is 256. We will try to use different number of bit (i.e. 16-bit, 32-bit) as the compression unit. Huffman compression works well when the distribution of the compression is highly skew. There is a trade-off on how many bits are used as the compressed unit because the more bits we use, the more space we can reduce for the less frequent code, but the less opportunity we can have for the skew distribution and the more time is spent on constructing the huffman tree and the huffman code.

#### LZSS  Compression
The only issue now is to rewrite the file I/O part of the sequential algorithm so that it can read a block size of data at a time instead of one byte of time. After we fix this issue, we will start parallelizing the linear search string match and string matching using hash table. Then compared to the sequential code to see how much speedup we get.

## Reference
[1] LZSS algorithm details. http://michael.dipperstein.com/bitlibs/index.html

[2] Libhuffman: https://github.com/drichardson/huffman 

---------------------------------------
# Project Proposal
## Summary
We are going to implement parallel versions of two lossless data compression algorithm, Lempel-Ziv-Storer-Szymanski (LZSS)  compression and Huffman coding, on many-core CPU. We will make use of SIMD intrinsics, ISPC compiler and OpenMP library to perform parallel compression. We will also conduct detailed analysis for our implementation considering characteristics including memory bandwidth, CPU usage and cache behavior, and compare the performance with the sequential implementation.

## Background
As internet are getting popular in 1980s, many compression algorithms are invented to overcome the limitation of network and storage bandwidth. Lossy compression algorithms are mainly used to compress image and audio. However, lossless compression algorithms are more useful in many other situations like compression in storage device where loss of data is unacceptable. The most famous lossless compression algorithm LZ77 was invented by Abraham Lempel and Jacob Ziv in 1977. After that, there are many algorithms derived from LZ77. One of them is LZSS, which is used in RAR. Another very popular compression algorithm is called Huffman coding, which generate prefix coding for encoding targets. These two algorithms plays an important role in many compression libraries and they are currently implemented sequentially in those libraries.  

## The Challenge
- In LZSS algorithm, the compressor will keep track of a sliding window as the encoding context. For the following sequence of characters, the compressor will try to find if there is any repeated sequence in the sliding window. If found, replace the following sequence with (offset, length) pair. Since this sliding windows are always moving sequentially, there isn’t a clear way to parallelize it. We may need to modify the algorithm to make it more easily to parallelize.
- In Huffman coding, there are three main steps. First, calculate the frequency of all characters. Then build a min heap using frequency as the key and characters as the value. Finally, in each iteration, the first two element in min heap will be popped out and inserted into the Huffman tree. Since the whole process is using a shared min heap and a shared tree, it’s unclear how to parallelize it efficiently. The synchronization overhead may cause the bad parallel implementation to achieve very poor scalability.


## Goals and Deliverables
#### 75% goal
- Finish a correct implementation for both sequential and parallel versions of  lossless compression algorithms LZSS and Huffman Coding.
- Run the algorithms on top of regular multi-core machines and Xeon Phi Coprocessor.
#### 100% goal
- Evaluate the memory bandwidth, memory footprint, CPU efficiency of parallel LZSS and Huffman Coding to identify bottlenecks.
- Perform optimization for parallel LZSS and Huffman Coding based on the evaluation results. Improve the compression speed without consuming more system resources and sacrificing compression ratio.
- Conduct detailed analysis on our parallel implementation running on Xeon Phi Coprocessor. Compare the performance of the parallel and sequential version as well as the behavior running on different platform (Xeon Phi Coprocessor, regular multi-core machine, single core machine) by providing speedup graphs.
#### 125% goal
- Achieve comparable or even beat state of the art LZSS and Huffman Coding implementation.
- Modify the implementation to adapt CUDA. Run our algorithm on top of GPU.

## Resources and Platform Choice
- Language/Compiler/Library: C/C++, ISPC, SIMD intrinsics, OpenMP
- Platform: 
  - Intel Xeon Phi coprocessor that supports 512bit AVX instructions
  - General multi-core processor (i.e. GHC machines)
- References
  - Patel, R. A., Zhang, Y., Mak, J., Davidson, A., & Owens, J. D. (2012). Parallel lossless data compression on the GPU (pp. 1-9). IEEE.
  - Ozsoy, A., Swany, M., & Chauhan, A. (2012, December). Pipelined parallel lzss for streaming data compression on gpgpus. In Parallel and Distributed Systems (ICPADS), 2012 IEEE 18th International Conference on (pp. 37-44). IEEE.
  - Ozsoy, A., Swany, M., & Chauhan, A. (2014). Optimizing LZSS compression on GPGPUs. Future Generation Computer Systems, 30, 170-178.


## Schedule
- 4/10 - 4/14: Read papers and understand the basic algorithm. Setup development environment.
- 4/15 - 4/21: Implement the naive but correct parallel version of the two algorithms (75% goal).
- 4/22 - 4/25: Benchmark the naive implementation, identify the bottlenecks.
- 4/26 - 5/4: Optimize the algorithms to tackle with the bottlenecks and improve compression speed. (100% goal)
- 5/5 - 5/11: Conduct experiments to analysis the optimized versions, compare with the sequential implementation, explain the trade-offs and finish write-up.
