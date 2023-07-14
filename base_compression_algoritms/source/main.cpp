#include "CompressorHuffman.h"
#include "CompressorLZ77.h"
#include "CompressorLZ78.h"

#include <cstring>
#include <iostream>

#include <sys/time.h>

long long int getTime() {
	static bool init = false;
	static long long int offset = 0;
	if(!init) {
		init = true;
		struct timeval tval;
		gettimeofday(&tval,0);
		offset = (long long int)tval.tv_sec * 1000000 + tval.tv_usec;
	}
	struct timeval tval;
	gettimeofday(&tval,0);
	return (long long int)tval.tv_sec * 1000000 + tval.tv_usec - offset;
}

double getDoubleTimeMiliseconds() {
	return (double)getTime() / 1000.0;
}

class ScopeTimer {
public:
	ScopeTimer(const std::string &n) : time(getDoubleTimeMiliseconds()), name(n) {
	}
	~ScopeTimer()
	{
		printf("%s %lf miliseconds\n", name.c_str(), getDoubleTimeMiliseconds() - time);
	}
private:
	double time;
	std::string name;

};

void test_compress_data(const uint8_t *data, size_t data_size, Compressor &compressor) {

	std::cout << "--------------------------------------------------------------------------------" << std::endl;
	std::cout << "Compressor: " << compressor.getTypeName() << std::endl;

	uint8_t *compressed_data = new uint8_t[data_size * 8];
	memset(compressed_data, 0, data_size * 4);

	if (data_size < 128)
		std::cout << "Source data: " << data << std::endl;;
	std::cout << "Source data size: " << data_size << std::endl;;

	size_t compressed_size;
	{
		ScopeTimer timer("Compress time");
		if (!compressor.compress(data, data_size, compressed_data,
			data_size * 4, compressed_size)) {
			std::cerr << "compress failed." << std::endl;
		}
	}

	std::cout << "Compressed data size: " << compressed_size << std::endl;
	std::cout << "Ratio: " << float(data_size) / compressed_size << std::endl;

	uint8_t *decompressed_data = new uint8_t[data_size];
	size_t decompressed_size = 0;
	memset(decompressed_data, 0, data_size);

	{
		ScopeTimer timer("Decompress time");
		if (!compressor.decompress(compressed_data, compressed_size,
			decompressed_data, data_size, decompressed_size)) {
			std::cerr << "decompress failed." << std::endl;
		}
	}

	if (data_size < 128) {
		std::cout << "Uncompressed data: ";
		std::cout.write((const char *)decompressed_data, decompressed_size);
		std::cout << std::endl;
	}
	std::cout << "Uncompressed data size: " << decompressed_size << std::endl;

	if (data_size != decompressed_size || memcmp(data, decompressed_data, decompressed_size) != 0) {
		std::cerr << "Data corruption." << std::endl;
	}

	delete[] compressed_data;
	delete[] decompressed_data;
}

void test_compress_file(const char *filepath, Compressor &compressor) {
	
	FILE *file = fopen(filepath, "rb");
	if(!file) {
		return;
	}

	fseek(file, 0, SEEK_END);
	auto size  = ftell(file);
	fseek(file, 0, SEEK_SET);

	uint8_t *data = new uint8_t[size];
	fread(data, 1, size, file);
	fclose(file);

	test_compress_data(data, size, compressor);
	std::cout << "File: " << filepath << std::endl;
	delete[] data;
}

int main(int argc, char **argv) {
	
	const char data0[] = "abcdefghqwertyfdjkbnbvsmk.bnsjk;jkfndgsjlkdbnjkdnv;aslkndfkjfl;akjsdkjfa;skdjf;klasdjf;lasjdfa;lsjdf";
	const char data1[] = "aaaabbcddd";
	const char data2[] = "abacabacabadaca";
	const char data3[] = "hellolololololo";
	const char data4[] = "aacaacabcabaaac";
	const char data5[] = "aacaacabcabaaacaacaacabcabaaacaacaacabcabaaac";
	const char data6[] = "abacababacabc";
	const char data7[] = "aaaaaaaaaaaaaa";

	const int NUM_COMPRESSORS = 3;
	Compressor *compressors[NUM_COMPRESSORS] = {new CompressorHuffman(), new CompressorLZ77(), new CompressorLZ78()};
	for (int i = 0; i < NUM_COMPRESSORS; ++i) {
		test_compress_data((const uint8_t *)data0, sizeof(data0) - 1, *compressors[i]);
	}
	for (int i = 0; i < NUM_COMPRESSORS; ++i) {
		test_compress_data((const uint8_t *)data1, sizeof(data1) - 1, *compressors[i]);
	}
	for (int i = 0; i < NUM_COMPRESSORS; ++i) {
		test_compress_data((const uint8_t *)data2, sizeof(data2) - 1, *compressors[i]);
	}
	for (int i = 0; i < NUM_COMPRESSORS; ++i) {
		test_compress_data((const uint8_t *)data3, sizeof(data3) - 1, *compressors[i]);
	}
	for (int i = 0; i < NUM_COMPRESSORS; ++i) {
		test_compress_data((const uint8_t *)data4, sizeof(data4) - 1, *compressors[i]);
	}
	for (int i = 0; i < NUM_COMPRESSORS; ++i) {
		test_compress_data((const uint8_t *)data5, sizeof(data5) - 1, *compressors[i]);
	}
	for (int i = 0; i < NUM_COMPRESSORS; ++i) {
		test_compress_data((const uint8_t *)data6, sizeof(data6) - 1, *compressors[i]);
	}

	for (int i = 0; i < NUM_COMPRESSORS; ++i) {
		test_compress_data((const uint8_t *)data7, sizeof(data7) - 1, *compressors[i]);
	}

	for (int j = 1; j < argc; ++j) {
		for (int i = 0; i < NUM_COMPRESSORS; ++i) {
			test_compress_file(argv[j], *compressors[i]);
		}
	}
	for (int i = 0; i < NUM_COMPRESSORS; ++i) {
		delete compressors[i];
	}
	return 0;
}
