#pragma once
#include <misc/explints.hpp>

#include <memory>

namespace rle {

template<typename T>
std::pair<std::unique_ptr<u8[]>, sz_t> compress(T* arr, u16 numItems);


template<typename T>
sz_t getItems(u8 * in, sz_t size);

template<typename T>
void decompress(u8* in, sz_t inSize, T* output, sz_t outMaxItems);

}

#include "rle.tpp"
