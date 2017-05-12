#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <iostream>
#include "util.h"


// Return a integer represent the percentage. Range [0, 100]
int get_percentage(double total, double partial) {
  return (int)((partial / total) * 100);
}

unsigned long
numbytes_from_numbits(unsigned long numbits) {
  return numbits / 8 + (numbits % 8 ? 1 : 0);
}

/*
 * get_bit returns the ith bit in the bits array
 * in the 0th position of the return value.
 */
unsigned char
get_bit(unsigned char *bits, unsigned long i) {
  return (bits[i / 8] >> i % 8) & 1;
}

void
reverse_bits(unsigned char *bits, unsigned long numbits) {
  unsigned long numbytes = numbytes_from_numbits(numbits);
  unsigned char *tmp =
      (unsigned char *) alloca(numbytes);
  unsigned long curbit;
  long curbyte = 0;

  memset(tmp, 0, numbytes);

  for (curbit = 0; curbit < numbits; ++curbit) {
    unsigned int bitpos = curbit % 8;

    if (curbit > 0 && curbit % 8 == 0)
      ++curbyte;

    tmp[curbyte] |= (get_bit(bits, numbits - curbit - 1) << bitpos);
  }

  memcpy(bits, tmp, numbytes);
}

/*
 * new_code builds a huffman_code from a leaf in
 * a Huffman tree.
 */
huffman_code *
new_code(const huffman_node *leaf) {
  /* Build the libhuffman code by walking up to
   * the root node and then reversing the bits,
   * since the Huffman code is calculated by
   * walking down the tree. */
  unsigned long numbits = 0;
  unsigned char *bits = NULL;
  huffman_code *p;

  while (leaf && leaf->parent) {
    huffman_node *parent = leaf->parent;
    unsigned char cur_bit = (unsigned char) (numbits % 8);
    unsigned long cur_byte = numbits / 8;

    /* If we need another byte to hold the code,
       then allocate it. */
    if (cur_bit == 0) {
      size_t newSize = cur_byte + 1;
      bits = (unsigned char *) realloc(bits, newSize);
      bits[newSize - 1] = 0; /* Initialize the new byte. */
    }

    /* If a one must be added then or it in. If a zero
     * must be added then do nothing, since the byte
     * was initialized to zero. */
    if (leaf == parent->one)
      bits[cur_byte] |= 1 << cur_bit;

    ++numbits;
    leaf = parent;
  }

  if (bits)
    reverse_bits(bits, numbits);

  p = (huffman_code *) malloc(sizeof(huffman_code));
  p->numbits = numbits;
  p->bits = bits;
  return p;
}

huffman_node *
new_leaf_node(unsigned char symbol) {
  huffman_node *p = (huffman_node *) malloc(sizeof(huffman_node));
  p->isLeaf = 1;
  p->symbol = symbol;
  p->count = 0;
  p->parent = 0;
  return p;
}

huffman_node *
new_nonleaf_node(unsigned long count, huffman_node *zero, huffman_node *one) {
  huffman_node *p = (huffman_node *) malloc(sizeof(huffman_node));
  p->isLeaf = 0;
  p->count = count;
  p->zero = zero;
  p->one = one;
  p->parent = 0;

  return p;
}

void
free_huffman_tree(huffman_node *subtree) {
  if (subtree == NULL)
    return;

  if (!subtree->isLeaf) {
    free_huffman_tree(subtree->zero);
    free_huffman_tree(subtree->one);
  }

  free(subtree);
}

void
free_code(huffman_code *p) {
  free(p->bits);
  free(p);
}

void
free_encoder(SymbolEncoder *pSE) {
  unsigned long i;
  for (i = 0; i < MAX_SYMBOLS; ++i) {
    huffman_code *p = (*pSE)[i];
    if (p)
      free_code(p);
  }

  free(pSE);
}

void
init_frequencies(SymbolFrequencies *pSF) {
  memset(*pSF, 0, sizeof(SymbolFrequencies));
#if 0
  unsigned int i;
  for(i = 0; i < MAX_SYMBOLS; ++i)
  {
      unsigned char uc = (unsigned char)i;
      (*pSF)[i] = new_leaf_node(uc);
  }
#endif
}

/*
 * When used by qsort, SFComp sorts the array so that
 * the symbol with the lowest frequency is first. Any
 * NULL entries will be sorted to the end of the list.
 */
int
SFComp(const void *p1, const void *p2) {
  const huffman_node *hn1 = *(const huffman_node **) p1;
  const huffman_node *hn2 = *(const huffman_node **) p2;

  /* Sort all NULLs to the end. */
  if (hn1 == NULL && hn2 == NULL)
    return 0;
  if (hn1 == NULL)
    return 1;
  if (hn2 == NULL)
    return -1;

  if (hn1->count > hn2->count)
    return 1;
  else if (hn1->count < hn2->count)
    return -1;

  return 0;
}

/*
 * build_symbol_encoder builds a SymbolEncoder by walking
 * down to the leaves of the Huffman tree and then,
 * for each leaf, determines its code.
 */
void
build_symbol_encoder(huffman_node *subtree, SymbolEncoder *pSF) {
  if (subtree == NULL)
    return;

  if (subtree->isLeaf)
    (*pSF)[subtree->symbol] = new_code(subtree);
  else {
    build_symbol_encoder(subtree->zero, pSF);
    build_symbol_encoder(subtree->one, pSF);
  }
}

/*
 * calculate_huffman_codes turns pSF into an array
 * with a single entry that is the root of the
 * libhuffman tree. The return value is a SymbolEncoder,
 * which is an array of libhuffman codes index by symbol value.
 */
SymbolEncoder *
calculate_huffman_codes(SymbolFrequencies *pSF) {
  auto endTime1 = CycleTimer::currentSeconds();

  unsigned int i = 0;
  unsigned int n = 0;
  huffman_node *m1 = NULL, *m2 = NULL;
  SymbolEncoder *pSE = NULL;

#if 0
  printf("BEFORE SORT\n");
  print_freqs(pSF);
#endif

  /* Sort the symbol frequency array by ascending frequency. */
  qsort((*pSF), MAX_SYMBOLS, sizeof((*pSF)[0]), SFComp);

#if 0
  printf("AFTER SORT\n");
  print_freqs(pSF);
#endif

  /* Get the number of symbols. */
  for (n = 0; n < MAX_SYMBOLS && (*pSF)[n]; ++n);

  /*
   * Construct a Huffman tree. This code is based
   * on the algorithm given in Managing Gigabytes
   * by Ian Witten et al, 2nd edition, page 34.
   * Note that this implementation uses a simple
   * count instead of probability.
   */
  for (i = 0; i < n - 1; ++i) {
    /* Set m1 and m2 to the two subsets of least probability. */
    m1 = (*pSF)[0];
    m2 = (*pSF)[1];

    /* Replace m1 and m2 with a set {m1, m2} whose probability
     * is the sum of that of m1 and m2. */
    (*pSF)[0] = m1->parent = m2->parent =
        new_nonleaf_node(m1->count + m2->count, m1, m2);
    (*pSF)[1] = NULL;

    /* Put newSet into the correct count position in pSF. */
    qsort((*pSF), n, sizeof((*pSF)[0]), SFComp);
  }

  auto endTime2 = CycleTimer::currentSeconds();
//  std::cout << "Build Tree Elapse time = " << endTime2 - endTime1 << std::endl;

  /* Build the SymbolEncoder array from the tree. */
  pSE = (SymbolEncoder *) malloc(sizeof(SymbolEncoder));
  memset(pSE, 0, sizeof(SymbolEncoder));
  build_symbol_encoder((*pSF)[0], pSE);

  auto endTime3 = CycleTimer::currentSeconds();
//  std::cout << "Construct Code Elapse time = " << endTime3 - endTime2 << std::endl;
  return pSE;
}