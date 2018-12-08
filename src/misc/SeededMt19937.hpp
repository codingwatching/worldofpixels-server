#pragma once

#include <random>

class SeededMt19937 {
	std::mt19937 rng;

public:
	SeededMt19937();

	using result_type = std::mt19937::result_type;
	
	result_type operator()();

	static constexpr result_type max();
	static constexpr result_type min();
};

#include "SeededMt19937.tpp"
