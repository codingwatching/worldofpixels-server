#pragma once

#include <random>

class RandomSeeder {
	std::random_device rd;

public:
	RandomSeeder();

	template<typename RandomAccessIterator>
	void generate(RandomAccessIterator, RandomAccessIterator);
};

#include "RandomSeeder.tpp"
