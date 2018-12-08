#include "base64.hpp"

#include <memory>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/md5.h>

constexpr auto bioDeleter = [] (BIO * b) {
	BIO_free_all(b);
};

int base64Decode(const char * encoded, sz_t encLength, u8 * out, sz_t outMaxLen) {
	std::unique_ptr<BIO, decltype(bioDeleter)> b64(BIO_new(BIO_f_base64()), bioDeleter);
	BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);

	BIO * source = BIO_new_mem_buf(encoded, encLength);
	BIO_push(b64.get(), source);

	return BIO_read(b64.get(), out, outMaxLen);
}

std::string base64Encode(const u8 * buf, sz_t len) {
	std::unique_ptr<BIO, decltype(bioDeleter)> b64(BIO_new(BIO_f_base64()), bioDeleter);
	BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);

	BIO * mem = BIO_new(BIO_s_mem());
	BIO_push(b64.get(), mem);

	BIO_write(b64.get(), buf, len);
	BIO_flush(b64.get());

	BUF_MEM * outBuf;
	BIO_get_mem_ptr(b64.get(), &outBuf);

	return std::string(outBuf->data, outBuf->length);
}

std::array<u8, MD5_DIGEST_LENGTH> md5sum(const u8 * buf, sz_t len) {
	std::array<u8, MD5_DIGEST_LENGTH> result;
	MD5(buf, len, &result[0]);

	return result;
}