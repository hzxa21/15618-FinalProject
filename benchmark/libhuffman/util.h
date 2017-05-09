#pragma once
#include "huffman.h"

unsigned long
numbytes_from_numbits(unsigned long numbits);

unsigned char
get_bit(unsigned char *bits, unsigned long i);

void
reverse_bits(unsigned char *bits, unsigned long numbits);

huffman_code *
new_code(const huffman_node *leaf);

huffman_node *
new_leaf_node(unsigned char symbol);

huffman_node *
new_nonleaf_node(unsigned long count, huffman_node *zero, huffman_node *one);

void
free_huffman_tree(huffman_node *subtree);

void
free_code(huffman_code *p);

void
free_encoder(SymbolEncoder *pSE);

void
init_frequencies(SymbolFrequencies *pSF);

int
SFComp(const void *p1, const void *p2);

void
build_symbol_encoder(huffman_node *subtree, SymbolEncoder *pSF);

SymbolEncoder *
calculate_huffman_codes(SymbolFrequencies *pSF);


