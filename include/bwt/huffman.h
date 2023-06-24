#pragma once
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include "..\BitIO.h"

const char* compressionName = "Adaptive Huffman coding, with escape codes\n";
const char* usage = "infile outfile\n";

#define END_OF_STREAM 256
#define ESCAPE 257
#define SYMBOL_COUNT 258
#define NODE_TABLE_COUNT ((SYMBOL_COUNT * 2) - 1)
#define ROOT_NODE 0
#define MAX_WEIGHT 0x8000 //this happens sooner than 0xffff
#define TRUE 1
#define FALSE 0

struct Tree {
	int leaf[SYMBOL_COUNT]{ 0 };
	int next_free_node;
	struct Node {
		unsigned int weight = 0;
		int parent = SYMBOL_COUNT;
		int child_is_leaf = false;
		int child = SYMBOL_COUNT;
	} nodes[NODE_TABLE_COUNT];
};

/*
initializeTree()->
when performing adaptive compression, the Huffman tree starts out very nearly empty. The only two symbols present
initially are the ESCAPE symbol and the END_OF_STREAM symbol. The ESCAPE symbol has to be included so we can tell
the expansion program that we are tramsmitting a previously unseen symbol. The END_OF_STREAM symbol is here because
it is greater than 8-bits, and our ESCAPE sequence only allows for eight bit symbols following the ESCAPE code

In addition to setting up the root node and its two children, the routine also initializes the leaf array. The
ESCAPE and END_OF_STREAM leaves are the only ones initially defined. The rest of the leaf elements are set to -1 to
show that they aren't present in the Huffman tree yet.
*/

void initializeTree(Tree& tree) {
	tree.nodes[ROOT_NODE].child = ROOT_NODE + 1;
	tree.nodes[ROOT_NODE].child_is_leaf = false;
	tree.nodes[ROOT_NODE].weight = 2;
	tree.nodes[ROOT_NODE].parent = -1;

	tree.nodes[ROOT_NODE + 1].child = ESCAPE;
	tree.nodes[ROOT_NODE + 1].child_is_leaf = TRUE;
	tree.nodes[ROOT_NODE + 1].weight = 1;
	tree.nodes[ROOT_NODE + 1].parent = ROOT_NODE;

	tree.leaf[ESCAPE] = ROOT_NODE + 1;

	tree.nodes[ROOT_NODE + 2].child = END_OF_STREAM;
	tree.nodes[ROOT_NODE + 2].weight = 1;
	tree.nodes[ROOT_NODE + 2].child_is_leaf = TRUE;
	tree.nodes[ROOT_NODE + 2].parent = ROOT_NODE;

	tree.leaf[END_OF_STREAM] = ROOT_NODE + 2;

	tree.next_free_node = ROOT_NODE + 3;
	for (int i{ 0 }; i < END_OF_STREAM; i++)
		tree.leaf[i] = -1;
}


void add_new_node(Tree& tree, unsigned int c) {
	int old_escape_node = tree.leaf[ESCAPE];
	int new_escape_node = tree.next_free_node;
	int zero_weight_node = tree.next_free_node + 1;

	tree.nodes[new_escape_node] = tree.nodes[old_escape_node];
	tree.nodes[new_escape_node].parent = old_escape_node;
	tree.leaf[ESCAPE] = new_escape_node;//update the value of escape in the leaf array

	tree.nodes[old_escape_node].child_is_leaf = FALSE;
	tree.nodes[old_escape_node].child = tree.next_free_node;

	tree.nodes[zero_weight_node].child = c;
	tree.nodes[zero_weight_node].child_is_leaf = TRUE;
	tree.nodes[zero_weight_node].weight = 0;
	tree.nodes[zero_weight_node].parent = old_escape_node;

	tree.leaf[c] = zero_weight_node;
	tree.next_free_node += 2;
}

void RebuildTree(Tree& tree) {
	int i, j, k;
	unsigned int weight;
	//printf("Rebuilding tree\n");
	//to rebuild the huffman tree, we collect all the leaves of the huffman tree and put them in the end of
	//the tree, While we do that, the counts are also scaled down by a factor of 2
	j = tree.next_free_node - 1;
	//step 1
	//collect all the leaf nodes, throw away all the internal nodes, and divide the leaf-node weights
	//by two. none of the two leaf nodes are scaled down to zero, this is done by adding one to it before
	//dividing by 2 (although it may be beneficial to do so).
	//what we end up wih in the code is a list of leaf nodes that are at the start of the list, terminating
	//at the next_free_node index. The internal nodes which start at 0 and end at the current value of j will
	//now be rebuilt in step 2.
	for (i = j; i > ROOT_NODE; --i) {
		if (tree.nodes[i].child_is_leaf) {
			tree.nodes[j] = tree.nodes[i];
			tree.nodes[j].weight = (tree.nodes[j].weight + 1) / 2;
			--j;
		}
	}
	//step 2
	//The process of creating the new internal node is simple. The new node, located at index j, is
	//assigned a weight. The weight is simply the sum of the two nodes at location i. After the node j
	//is created, we use the sibling property to locate wher it belongs to on the list. Before the node
	//can be positioned, we need to make room by moving all the nodes that have higher weights up (<-) by 
	//one position. This is done by the memmove function.
	for (i = tree.next_free_node - 2; j >= ROOT_NODE; i -= 2, --j) {
		k = i + 1;
		tree.nodes[j].weight = tree.nodes[i].weight + tree.nodes[k].weight;
		weight = tree.nodes[j].weight;
		tree.nodes[j].child_is_leaf = FALSE;
		for (k = j + 1; weight < tree.nodes[k].weight; ++k);
		--k;
		memmove(&tree.nodes[j], &tree.nodes[j + 1], (k - j) * sizeof(Tree::Node));
		tree.nodes[k].weight = weight;
		tree.nodes[k].child = i;
		tree.nodes[k].child_is_leaf = FALSE;
	}
	//step 3
	//The final step is to go through and setup all the leaf and parent members
	for (i = tree.next_free_node - 1; i >= ROOT_NODE; i--) {
		if (tree.nodes[i].child_is_leaf) {
			k = tree.nodes[i].child;
			tree.leaf[k] = i;
		}
		else {
			k = tree.nodes[i].child;
			tree.nodes[k].parent = tree.nodes[k + 1].parent = i;
		}
	}
}

void swap_nodes(Tree& tree, int i, int j) {
	Tree::Node temp;
	if (tree.nodes[i].child_is_leaf)
		tree.leaf[tree.nodes[i].child] = j;
	else {
		tree.nodes[tree.nodes[i].child].parent = j;
		tree.nodes[tree.nodes[i].child + 1].parent = j;
	}
	if (tree.nodes[j].child_is_leaf)
		tree.leaf[tree.nodes[j].child] = i;
	else {
		tree.nodes[tree.nodes[j].child].parent = i;
		tree.nodes[tree.nodes[j].child + 1].parent = i;
	}
	temp = tree.nodes[i];
	tree.nodes[i] = tree.nodes[j];
	tree.nodes[i].parent = temp.parent;
	temp.parent = tree.nodes[j].parent;
	tree.nodes[j] = temp;

}

void UpdateModel(Tree& tree, int c) {
	int current_node, new_node;
	if (tree.nodes[ROOT_NODE].weight == MAX_WEIGHT)
		RebuildTree(tree);
	current_node = tree.leaf[c];
	while (current_node != -1) {//while not at root
		tree.nodes[current_node].weight++;
		for (new_node = current_node; new_node > ROOT_NODE; --new_node)
			if (tree.nodes[new_node - 1].weight >= tree.nodes[current_node].weight)
				break;
		if (current_node != new_node) {
			swap_nodes(tree, current_node, new_node);
			current_node = new_node;
		}
		current_node = tree.nodes[current_node].parent;
	}
}


void EncodeSymbol(Tree& tree, unsigned int c, std::unique_ptr<stl::BitFile>& output) {
	unsigned long code = 0;
	unsigned long current_bit = 1;
	int code_size = 0;
	int current_node = tree.leaf[c];
	if (current_node == -1) /*if symbol is not yet present*/
		current_node = tree.leaf[ESCAPE];

	while (current_node != ROOT_NODE)
	{
		if ((current_node & 1) == 1)//if current_node is an odd number i.e its at the right side of parent node, set the current bit in code
			code |= current_bit;
		current_bit <<= 1;
		++code_size;
		current_node = tree.nodes[current_node].parent;
	}
	stl::outputBits(output, code, code_size);
	if (tree.leaf[c] == -1) {
		stl::outputBits(output, (std::uint32_t)c, 8);
		add_new_node(tree, c);
	}
}

int DecodeSymbol(Tree& tree, std::unique_ptr<stl::BitFile>& input) {
	int current_node;
	int next_bit;
	int c;
	current_node = ROOT_NODE;
	while (!tree.nodes[current_node].child_is_leaf) {
		current_node = tree.nodes[current_node].child;
		next_bit = stl::inputBit(input);
		current_node += next_bit == 0 ? 1 : 0;
	}
	c = tree.nodes[current_node].child;
	if (c == ESCAPE) {
		c = (int)stl::inputBits(input, 8);
		add_new_node(tree, c);
	}
	return c;
}


void huffCompress(unsigned char* input, size_t length, std::unique_ptr<stl::BitFile>& output) {
	unsigned int c;
	Tree tree;
	initializeTree(tree);
	size_t i = 0;
	while (i < length) {
		c = input[i++];
		EncodeSymbol(tree, c, output);
		UpdateModel(tree, c);
	}
	EncodeSymbol(tree, END_OF_STREAM, output);
}

void huffExpand(std::unique_ptr<stl::BitFile>& input, unsigned char* output) {
	int c;
	int counter{ 0 };
	Tree tree;
	initializeTree(tree);
	while ((c = DecodeSymbol(tree, input)) != END_OF_STREAM) {
		output[counter++] = c;
		UpdateModel(tree, c);
	}
}