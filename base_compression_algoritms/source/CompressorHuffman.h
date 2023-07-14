#pragma once
#include "Compressor.h"

class CompressorHuffman : public Compressor {
	
public:
	const char *getTypeName() const override { return "CompressorHuffman"; };

protected:
	bool onCompress(const uint8_t *data, size_t data_size,
		uint8_t *out_data, size_t out_data_size, size_t &compressed_size) override;
	bool onDecompress(const uint8_t *compressed_data, size_t compressed_data_size,
		uint8_t *data, size_t data_size, size_t &decompressed_size) override;

};
