#include <algorithm>
#include <functional>

template<typename RandomAccessIterator>
void RandomSeeder::generate(RandomAccessIterator first, RandomAccessIterator last) {
	std::generate(first, last, std::ref(rd));
}
