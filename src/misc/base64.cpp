#include "base64.hpp"

#include <memory>
#include <openssl/bio.h>
#include <openssl/evp.h>

constexpr auto bioDeleter = [] (BIO * b) {
	BIO_free_all(b);
};

int base64Decode(const char * encoded, sz_t encLength, u8 * out, sz_t outMaxLen) {
	std::unique_ptr<BIO, bioDeleter> b64(BIO_new(BIO_f_base64()));
	BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);

	BIO * source = BIO_new_mem_buf(encoded, encLength);
	BIO_push(b64.get(), source);

	return BIO_read(b64.get(), out, outMaxLen);
}
