#include "utils.hpp"

#ifndef __WIN32
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
#else
	#include <windows.h>
#endif

#include <chrono>

bool makedir(const std::string& dir) {
#ifndef __WIN32
	return mkdir(dir.c_str(), 0755) == 0;
#else
	/* conversion from windows bool to c++ bool necessary? */
	return CreateDirectory(dir.c_str(), nullptr) == TRUE;
#endif
}

bool fileExists(const std::string& path) {
#ifndef __WIN32
	return access(path.c_str(), F_OK) != -1;
#else
	DWORD dwAttrib = GetFileAttributes(path.c_str());

	return dwAttrib != INVALID_FILE_ATTRIBUTES;
#endif
}

sz_t getUTF8strlen(const std::string& str) {
	sz_t j = 0, i = 0, x = 1;
	while (i < str.size()) {
		if (x > 4) { /* Invalid unicode */
			return -1;
		}

		if ((str[i] & 0xC0) != 0x80) {
			j += x == 4 ? 2 : 1;
			x = 1;
		} else {
			x++;
		}
		i++;
	}
	if (x == 4) {
		j++;
	}
	return (j);
}

i64 js_date_now() {
	namespace c = std::chrono;

	auto time = c::system_clock::now().time_since_epoch();
	return c::duration_cast<c::milliseconds>(time).count();
}
