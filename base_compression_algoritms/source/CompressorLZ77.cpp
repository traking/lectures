#include "CompressorLZ77.h"
#include <cstring>
#include <stdio.h>


namespace {
using PairType = uint8_t;
const size_t WindowSize = 255;

enum HeaderFlags : unsigned char {
	None = 0,
	Pair = 0b00000001,
	Dt = 0b00000010,
	PairFourBit = 0b00000100,
	DtFourBit = 0b00001000
};

struct Node {
	uint8_t header;
	PairType offset;
	PairType length;
	uint8_t next;
};

void find(const uint8_t *window, const uint8_t *data, const uint8_t *data_end, Node &out);

}

bool CompressorLZ77::onCompress(const uint8_t *data, size_t data_size,
	uint8_t *out_data, size_t out_data_size, size_t &compressed_size)
{
	uint8_t *out = out_data;
	bool half_byte_switch{false};
	auto write_4bit = [&](uint8_t  d) {
		(half_byte_switch ^= 1) ? *out = d << 4 : *out++ |= (d & 0b00001111);
	};

	auto write_8bit = [&](uint8_t d) {
		write_4bit(d >> 4);
		write_4bit(d & 0b00001111);
	};
	auto write_16bit = [&](uint16_t d) {
		write_8bit(d >> 8);
		write_8bit(d & 0b0000000011111111);
	};

	Node node;

	uint8_t last_next = 0;
	PairType last_offset = 0;
	PairType last_length = 0;

	const uint8_t *window_start;
	for(size_t i = 0; i < data_size; ++i) {
		window_start = i >= WindowSize ? data + (i -  WindowSize) : data;

		find(window_start, data + i, data + data_size, node);
		i += node.length;

		node.header = node.next ? HeaderFlags::Dt : HeaderFlags::None;

		if(node.offset != 0 || node.length != 0) {
			PairType dt = node.offset - last_offset;
			last_offset = node.offset;
			node.offset = dt;

			dt = node.length - last_length;
			last_length = node.length;
			node.length = dt;

			node.header |= HeaderFlags::Pair;
			if(node.offset < 16 && node.length < 16) node.header |= HeaderFlags::PairFourBit;
		}

		uint8_t dt = node.next - last_next;
		last_next = node.next;
		if(dt < 16) node.header |= HeaderFlags::DtFourBit;

		write_4bit(node.header);

		if((node.header & HeaderFlags::Pair) != 0) {
			if((node.header & HeaderFlags::PairFourBit) != 0) {
				write_4bit(node.offset);
				write_4bit(node.length);
			} else if(sizeof(PairType) == sizeof(uint8_t)) {
				write_8bit(node.offset);
				write_8bit(node.length);
			} else {
				write_16bit(node.offset);
				write_16bit(node.length);
			}
		}

		if((node.header & HeaderFlags::Dt) != 0) {
			((node.header & HeaderFlags::DtFourBit) != 0) ? write_4bit(dt) : write_8bit(dt);
		}
	}

	compressed_size = out - out_data;
	if(half_byte_switch) ++compressed_size;

	return true;
}

bool CompressorLZ77::onDecompress(const uint8_t *compressed_data, size_t compressed_data_size,
	uint8_t *data, size_t data_size, size_t &decompressed_size)
{
	const unsigned char *p = compressed_data;
	const unsigned char *compressed_data_end = compressed_data + compressed_data_size;

	uint8_t read_byte;
	bool half_byte_switch{false};
	auto read_4bit = [&]() {
		return (half_byte_switch ^= 1) ? ((read_byte = *p++) >> 4) : (read_byte & 0b00001111);
	};

	auto read_8bit = [&]() {
		uint8_t ret = read_4bit();
		return (ret << 4) | read_4bit();
	};

	auto read_16bit = [&]() {
		uint16_t ret = read_8bit();
		return (ret << 8) | read_8bit();
	};

	Node node;
	uint8_t *out = data;
	uint8_t last_next{0};
	PairType last_offset{0};
	PairType last_length{0};
	while(p < compressed_data_end) {
		node.header = read_4bit();
		node.offset = node.length = 0;
		node.next = 0;
		if((node.header & HeaderFlags::Pair) != 0) {
			if((node.header & HeaderFlags::PairFourBit) != 0) {
				node.offset = read_4bit();
				node.length = read_4bit();
			} else if(sizeof(PairType) == sizeof(uint8_t)){
				node.offset = read_8bit();
				node.length = read_8bit();
			} else {
				node.offset = read_16bit();
				node.length = read_16bit();
			}
			node.offset += last_offset;
			node.length += last_length;
			last_offset = node.offset;
			last_length = node.length;
		}

		if((node.header & HeaderFlags::Dt) != 0) {
			node.next = (((node.header & HeaderFlags::DtFourBit) != 0)
				? read_4bit() : read_8bit()) + last_next;
		}

		last_next = node.next;

		if(node.length > 0) {
			uint8_t *p = out - node.offset;
			PairType length = node.length;
			while(length-- > 0) *out++ = *p++;
		  }

		if((out - data) >= data_size) break;

		*out++ = node.next;
	}

	decompressed_size = out - data;
	return true;
}

namespace
{

void find(const uint8_t *window, const uint8_t *data, const uint8_t *data_end, Node &out) {
	out.offset = 0;
	out.length = 0;
	out.next = *data;

	uint8_t d = *data;

	PairType match_length = 0;
	PairType match_offset = 0;

	while((data - window) > out.length) {
		if(*window++ != d) continue;
		if(data[out.length] != window[out.length - 1]) { window++; continue; }

		match_offset = data - (window - 1);
		const uint8_t *pd = data + 1;
		const uint8_t *pw = window;
		while(pd < data_end && pw < data_end && *pw == *pd) {
			++pw; ++pd;
		}
		if((match_length = (pd - data)) > out.length) {
			out.offset = match_offset;
			out.length = match_length;
			out.next = (data + match_length) < data_end ? data[match_length] : '\0';
		}
	}
}

}
