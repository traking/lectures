#include "Compressor.h"

Compressor::Compressor() {
}

Compressor::~Compressor() {
}

bool Compressor::compress(const uint8_t *data, size_t data_size,
	uint8_t *out_data, size_t out_data_size, size_t &compressed_size)
{
	compressed_size = 0;

	if (!data || !out_data) {
		return false;
	}

	return onCompress(data, data_size, out_data, out_data_size, compressed_size);
}

bool Compressor::decompress(const uint8_t *compressed_data, size_t compressed_data_size,
	uint8_t *data, size_t data_size, size_t &decompressed_size)
{
	decompressed_size = 0;
	if (!data || !compressed_data) {
		return false;
	}
	
	return onDecompress(compressed_data, compressed_data_size, data, data_size,
		decompressed_size);
}

