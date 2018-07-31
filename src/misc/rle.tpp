#include <misc/BufferHelper.hpp>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace rle {

template<typename T>
std::pair<std::unique_ptr<u8[]>, sz_t> compress(T* arr, u16 numItems) {
	struct compressedPoint { u16 pos; u16 length; };
	std::vector<compressedPoint> compressedPos;

	sz_t compBytes = sizeof(T) * numItems;
	for (u16 i = 1, t = 0; i <= numItems; i++) {
		// true if we're at the end of the array
		if (i == numItems || arr[i] != arr[i - 1]) {
			sz_t saved = (sz_t(i - t) - 1) * sizeof(T);
			if (saved > sizeof(compressedPoint)) {
				compBytes -= saved;
				compressedPos.push_back({t, u16(i - t)});
			}
			t = i;
		}
	}

	// original length, elem size, num repeats
	const sz_t points(compressedPos.size() * sizeof(compressedPoint));
	const sz_t header(sizeof(u16) * 3 + points);
	auto out(std::make_unique<u8[]>(header + compBytes));

	u8* curr = out.get();
	curr += buf::writeLE(curr, numItems);
	curr += buf::writeLE(curr, u16(sizeof(T)));
	curr += buf::writeLE(curr, u16(compressedPos.size()));

	curr = std::copy_n(
		reinterpret_cast<u8*>(compressedPos.data()),
		points,
		curr
	);

	T* currT = reinterpret_cast<T*>(curr);
	sz_t arrIdx = 0;
	for (auto point : compressedPos) {
		currT = std::copy(&arr[arrIdx], &arr[point.pos + 1], currT);
		arrIdx = point.pos + point.length;
	}
	currT = std::copy_n(&arr[arrIdx], numItems - arrIdx, currT);
	sz_t size = reinterpret_cast<u8*>(currT) - out.get();
	return {std::move(out), size};
}

template<typename T>
sz_t getItems(u8 * in, sz_t size) {
	if (size < sizeof(u16) * 3) {
		throw std::length_error("Input too small");
	}

	u8* curr = in;
	u16 numItems = buf::readLE<u16>(curr); curr += sizeof(u16);
	u16 itemSize = buf::readLE<u16>(curr); curr += sizeof(u16);
	if (itemSize != sizeof(T)) {
		throw std::length_error("Item length does not match");
	}
	return numItems;
}

template<typename T>
void decompress(u8* in, sz_t inSize, T* output, sz_t outMaxItems) {
	struct cPoint { u16 pos; u16 length; };
	u8* curr = in;
	if (inSize < sizeof(u16) * 3) {
		throw std::length_error("Input too small");
	}

	u16 numItems = buf::readLE<u16>(curr); curr += sizeof(u16);
	u16 itemSize = buf::readLE<u16>(curr); curr += sizeof(u16);
	u16 numRptPoints = buf::readLE<u16>(curr); curr += sizeof(u16);
	if (itemSize != sizeof(T)) {
		throw std::length_error("Item length does not match");
	}

	if (numItems > outMaxItems) {
		throw std::length_error("Output buffer is too small");
	}

	if (numRptPoints * sizeof(cPoint) >= inSize - (curr - in)) {
		throw std::length_error("Input buffer corrupted");
	}

	cPoint* pt = reinterpret_cast<cPoint*>(curr);
	curr += numRptPoints * sizeof(cPoint);
	T* data = reinterpret_cast<T*>(curr);
	sz_t j = 0;
	sz_t k = 0;
	for (u16 i = 0; i < numRptPoints; i++, pt++) { // XXX: no bound checking!
		//std::cout << pt->pos << ":" << pt->length << std::endl;
		while (j < pt->pos) {
			output[j++] = data[k++];
		}
		std::fill_n(&output[j], pt->length, data[k++]);
		j += pt->length;
	}
	while (j < numItems) {
		output[j++] = data[k++];
	}
}

}
