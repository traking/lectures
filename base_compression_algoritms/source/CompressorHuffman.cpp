#include "CompressorHuffman.h"

#include <memory.h>
#include <vector>
#include <algorithm>
#include <iostream>

using CodeType = uint8_t;
using IndexType = int16_t;
using std::vector;
using std::sort;

namespace
{


struct Node
{
	CodeType code;
	uint8_t dir;
	IndexType left;
	IndexType right;
	IndexType parent;
};

struct Index
{
	IndexType node;
	size_t count;
};

}

bool CompressorHuffman::onCompress(const uint8_t *data, size_t data_size,
	uint8_t *out_data, size_t out_data_size, size_t &compressed_size)
{
	size_t counters[1 << (sizeof(CodeType) * 8)];
	memset(counters, 0, sizeof(counters));

	for(size_t i = 0; i < data_size; ++i) {
		counters[data[i]]++;
	}

	vector<Node> nodes;
	vector<Index> indices;
	uint8_t char_to_index[1 << (sizeof(CodeType) * 8)];

	// init tree
	for(size_t i = 0; i < sizeof(counters) / sizeof(size_t); ++i) {
		size_t count = counters[i];
		if(count == 0) continue;
		char_to_index[i] = nodes.size();
		indices.push_back({static_cast<int16_t>(nodes.size()), count});
		nodes.push_back({static_cast<uint8_t>(i), 0, -1, -1, -1});
	}

	// sort by count
	sort(indices.begin(), indices.end(),
		[](const auto &i0, const auto &i1){ return i0.count > i1.count; });

	// build tree
	while(indices.size() > 1) {
		auto i0 = indices.back();
		indices.pop_back();

		auto i1 = indices.back();
		indices.pop_back();

		Node &n0 = nodes[i0.node];
		n0.dir = 1;
		n0.parent = nodes.size();

		Node &n1 = nodes[i1.node];
		n1.dir = 0;
		n1.parent = nodes.size();

		indices.push_back({static_cast<int16_t>(nodes.size()), i0.count + i1.count});

		nodes.push_back({'\0', 0, i1.node, i0.node, -1});

		sort(indices.begin(), indices.end(),
			[](const auto &i0, const auto &i1){ return i0.count > i1.count; });
	}

	uint8_t *out_header = out_data;
	uint8_t tail = 0;
	out_header += sizeof(tail); //reserve space for tail

	// write num nodes
	*reinterpret_cast<uint16_t *>(out_header) = nodes.size();
	out_header += sizeof(uint16_t);

	// write tree
	for(const Node &n : nodes) {
		*out_header++ = n.code;
		*reinterpret_cast<int16_t *>(out_header) = n.left;
		out_header += sizeof(int16_t);
		*reinterpret_cast<int16_t *>(out_header) = n.right;
		out_header += sizeof(int16_t);
	}

	uint8_t *out = out_header;
	*out = 0;

	uint8_t index_bit = 0;
	uint8_t path[sizeof(uint32_t) * 8];

	for(size_t i = 0; i < data_size; ++i) {
		uint8_t c = data[i];
		IndexType node_index = char_to_index[c];
		uint8_t *p = path - 1;
		while(node_index != -1) {
			const Node &node = nodes[node_index];
			*++p = node.dir;
			node_index = node.parent;
		}

		if(p == path) p++;

		while(p > path) {
			*out |= (*--p << index_bit);
			index_bit = (index_bit + 1) & (sizeof(uint8_t) * 8 - 1);
			if(index_bit == 0) {
				if((out - out_data) >= out_data_size) return false;
				*++out = 0;
			}
		}
	}

	compressed_size = out - out_data;
	tail = index_bit;
	if(tail != 0) ++compressed_size;

	*out_data = tail;

	std::cout << "Header size: " << out_header - out_data  <<std::endl;
	std::cout << "Tail: " << int(tail)  <<std::endl;
	return true;
}

bool CompressorHuffman::onDecompress(const uint8_t *compressed_data, size_t compressed_data_size,
	uint8_t *data, size_t data_size, size_t &decompressed_size)
{
	const uint8_t *header = compressed_data;
	uint8_t tail = *header++;

	uint16_t num_nodes = *reinterpret_cast<const uint16_t *>(header);
	header += sizeof(uint16_t);

	vector<Node> nodes;
	nodes.reserve(num_nodes);
	for(uint16_t i = 0; i < num_nodes; ++i) {
		Node n;
		n.code = *header++;

		n.left = *reinterpret_cast<const int16_t *>(header);
		header += sizeof(int16_t);
		n.right = *reinterpret_cast<const int16_t *>(header);
		header += sizeof(int16_t);

		nodes.push_back(n);
	}

	const uint8_t *p = header;
	uint8_t *out = data;
	const Node &root = nodes.back();
	const Node *current = &root;
	for(size_t i = header - compressed_data; i < compressed_data_size; ++i) {
		uint8_t byte = compressed_data[i];
		for(uint8_t b = 0; b < sizeof(byte) * 8; ++b) {
			int32_t node_index = (byte & (1 << b)) ? current->right : current->left;
			current = node_index == -1 ? &root : &nodes[node_index];
			if(current->left == -1 && current->right == -1) {
				*out++ = current->code;
				if((out - data) > data_size) return false;
				if(tail > 0 && i == (compressed_data_size - 1) && b == (tail - 1)) break;
				current = &root;
			}
		}
	}

	decompressed_size = (out - data);
	return true;
}
