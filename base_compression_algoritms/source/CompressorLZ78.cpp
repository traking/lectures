#include "CompressorLZ78.h"
#include <cstring>
#include <cstdio>
#include <iostream>

using PosType =  uint16_t;

namespace {

const uint32_t DictCapacity = 8192;

enum HeaderFlags : unsigned char {
	None = 0,
	Dt = 0b0000001,
	DtFourBit = 0b0000010,
	Pos = 0b00000100,
	PosFourBit = 0b00001000,
};

struct Node {
	uint8_t header;
	PosType pos;
	uint8_t next;
};

//djb2
inline uint16_t hash_function(const uint8_t *data, uint16_t size) {
	if(size == 0) return 0;

	uint16_t hash = 5381;
	for(uint16_t i = 0; i < size; ++i) {
		hash = ((hash << 5) + hash) + data[i]; /* hash * 33 + c */
	}
	return hash;
}

struct RawData {
	const uint8_t *data;
	uint16_t size;
};

template <uint32_t Capacity = 8>
struct Dict {

	struct Item {
		RawData data;
		uint16_t hash;
	};

	Dict() {
		memset(items, 0, sizeof(items));
	}

	void append(const RawData &raw_data) {
		auto hash = hash_function(raw_data.data, raw_data.size);
		auto &item = items[hash & (Capacity - 1)];
		item.data = raw_data;
		item.hash = hash;
	}

	bool has(const uint8_t *data, uint16_t size) const {
		auto hash = hash_function(data, size);
		const auto &item = items[hash & (Capacity - 1)];
		return item.hash == hash && item.data.size == size
			? memcmp(item.data.data, data, size) == 0 : false;
	}
private:
	Item items[Capacity];
};

}

bool CompressorLZ78::onCompress(const uint8_t *data, size_t data_size,
	uint8_t *out_data, size_t out_data_size, size_t &compressed_size) {

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

	Dict<DictCapacity> dict;
	const uint8_t *buffer = data;
	uint16_t buffer_size{0};
	uint8_t last_next{0};
	PosType last_pos{0};
	Node node;

	for(size_t i = 0; i < data_size; ++i) {
		while(i < (data_size - 1) && dict.has(buffer, buffer_size + 1)) {
			++buffer_size;
			++i;
		}

		uint8_t next = i < data_size ? buffer[buffer_size] : 0;
		node.next = next - last_next;
		last_next = next;

		node.header = i < data_size ? HeaderFlags::Dt : HeaderFlags::None;

		if(node.next < 16) {
			node.header |= HeaderFlags::DtFourBit;
		}

		if(buffer_size != 0) {
			PosType pos = hash_function(buffer, buffer_size) & (DictCapacity - 1);
			node.pos = pos - last_pos;
			last_pos = pos;
			node.header |= HeaderFlags::Pos;
			if(node.pos < 16) {
				node.header |= HeaderFlags::PosFourBit;
			}
		}

		//write node
		write_4bit(node.header);
		if((node.header & HeaderFlags::Pos) != 0) {
			if((node.header & HeaderFlags::PosFourBit) != 0)
				write_4bit(node.pos);
			else if(sizeof(PosType) == sizeof(uint8_t))
				write_8bit(node.pos);
			else
				write_16bit(node.pos);
		}

		if((node.header & HeaderFlags::Dt) != 0) {
			((node.header & HeaderFlags::DtFourBit) != 0)
				? write_4bit(node.next) : write_8bit(node.next);
		}

		++buffer_size;

		dict.append(RawData{buffer, buffer_size});
		buffer += buffer_size;
		buffer_size = 0;
	}

	compressed_size = out - out_data;
	if(half_byte_switch) ++compressed_size;

	return true;
}

bool CompressorLZ78::onDecompress(const uint8_t *compressed_data, size_t compressed_data_size,
	uint8_t *data, size_t data_size, size_t &decompressed_size) {

	RawData map[DictCapacity];

	const uint8_t *p = compressed_data;
	const uint8_t *compressed_data_end = compressed_data + compressed_data_size;

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
	PosType last_pos{0};
	while(p < compressed_data_end) {
		node.header = read_4bit();
		node.next = 0;
		node.pos = 0;
		if((node.header & HeaderFlags::Pos) != 0) {
			if((node.header & HeaderFlags::PosFourBit) != 0)
				node.pos = read_4bit();
			else if(sizeof(PosType) == sizeof(uint8_t))
				node.pos = read_8bit();
			else
				node.pos = read_16bit();
			node.pos += last_pos;
			last_pos = node.pos;
		}

		if((node.header & HeaderFlags::Dt) != 0) {
			node.next = (((node.header & HeaderFlags::DtFourBit) != 0)
				? read_4bit() : read_8bit()) + last_next;
		}

		last_next = node.next;
		uint8_t *s = out;
		if((node.header & HeaderFlags::Pos) != 0) {
			const RawData &d = map[node.pos];
			for(uint16_t j = 0; j < d.size; ++j) {
				if((out - data) > data_size) return false;
				*out++ = d.data[j];
			}
		}

		if((out - data) > data_size) return false;

		if((node.header & HeaderFlags::Dt) != 0)
			*out++ = node.next;

		uint16_t size = static_cast<uint16_t>(out - s);
		map[hash_function(s, size) & (DictCapacity - 1)] = RawData{s, size};
	}

	decompressed_size = out - data;
	return true;
}
