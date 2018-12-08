#include "SeededMt19937.hpp"
#include <misc/RandomSeeder.hpp>

SeededMt19937::SeededMt19937() {
	// RandomSeeder fills the internal state of the mersenne twister with random
	// numbers from std::random_device
	RandomSeeder rs{};
	rng.seed(rs);
}

SeededMt19937::result_type SeededMt19937::operator()() {
	return rng();
}
