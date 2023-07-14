cmake_minimum_required(VERSION 3.14)

project(Compress LANGUAGES CXX)

add_executable(Compressor
	main.cpp
	Compressor.h
	Compressor.cpp
	CompressorHuffman.h
	CompressorHuffman.cpp
	CompressorLZ77.h
	CompressorLZ77.cpp
	CompressorLZ78.h
	CompressorLZ78.cpp
)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
#cmake --build build_release && ./build_release/Compressor obj.obj dickens mr nci
