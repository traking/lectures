#pragma once
#include <cstdint>
#include <cstddef>

class Compressor {
public:
	Compressor();
	virtual ~Compressor();

	virtual const char *getTypeName() const = 0;

	bool compress(const uint8_t *data, size_t data_size,
		uint8_t *out_data, size_t out_data_size, size_t &compressed_size);
	bool decompress(const uint8_t *compressed_data, size_t compressed_data_size,
		uint8_t *data, size_t data_size, size_t &decompressed_size);

protected:
	virtual bool onCompress(const uint8_t *data, size_t data_size,
		uint8_t *out_data, size_t out_data_size, size_t &compressed_size) = 0;
	virtual bool onDecompress(const uint8_t *compressed_data, size_t compressed_data_size,
		uint8_t *data, size_t data_size, size_t &decompressed_size) = 0;

};
