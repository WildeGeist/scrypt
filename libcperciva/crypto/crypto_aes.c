#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/aes.h>

#include "cpusupport.h"
#include "crypto_aes_aesni.h"
#include "insecure_memzero.h"
#include "warnp.h"

#include "crypto_aes.h"

#if defined(CPUSUPPORT_X86_AESNI)
#define HWACCEL
#endif

/**
 * This represents either an AES_KEY or a struct crypto_aes_key_aesni; we
 * know which it is based on whether we're using AESNI code or not.  As such,
 * it's just an opaque pointer; but declaring it as a named structure type
 * prevents type-mismatch bugs in upstream code.
 */
struct crypto_aes_key;

#ifdef HWACCEL
/*
 * Test whether OpenSSL and hardware extensions code produce the same AES
 * ciphertext.
 */
static int
hwtest(uint8_t ptext[16], uint8_t * key, size_t len)
{
	AES_KEY kexp_openssl;
	uint8_t ctext_openssl[16];
	void * kexp_hw;
	uint8_t ctext_hw[16];

	/* Sanity-check. */
	assert((len == 16) || (len == 32));

	/* Expand the key and encrypt with OpenSSL. */
	AES_set_encrypt_key(key, (int)(len * 8), &kexp_openssl);
	AES_encrypt(ptext, ctext_openssl, &kexp_openssl);

	/* Expand the key and encrypt with hardware intrinstics. */
#if defined(CPUSUPPORT_X86_AESNI)
	if ((kexp_hw = crypto_aes_key_expand_aesni(key, len)) == NULL)
		goto err0;
	crypto_aes_encrypt_block_aesni(ptext, ctext_hw, kexp_hw);
	crypto_aes_key_free_aesni(kexp_hw);
#endif

	/* Do the outputs match? */
	return (memcmp(ctext_openssl, ctext_hw, 16));

err0:
	/* Failure! */
	return (-1);
}
#endif /* HWACCEL */

/**
 * crypto_aes_use_x86_aesni(void):
 * Return non-zero if AESNI operations are available.
 */
int
crypto_aes_use_x86_aesni(void)
{
	static int aesnigood = -1;
#ifdef HWACCEL
	uint8_t key[32];
	uint8_t ptext[16];
	size_t i;

	/* If we haven't decided which code to use yet, decide now. */
	while (aesnigood == -1) {
		/* Default to OpenSSL. */
		aesnigood = 0;

#if defined(CPUSUPPORT_X86_AESNI)
		/* If the CPU doesn't claim to support AESNI, stop here. */
		if (!cpusupport_x86_aesni())
			break;
#endif

		/* Test cases: key is 0x00010203..., ptext is 0x00112233... */
		for (i = 0; i < 16; i++)
			ptext[i] = (0x11 * i) & 0xff;
		for (i = 0; i < 32; i++)
			key[i] = i & 0xff;

		/*
		 * Test that hardware intrinsics and OpenSSL produce the same
		 * results.
		 */
		if (hwtest(ptext, key, 16) || hwtest(ptext, key, 32)) {
			warn0("Disabling AESNI due to failed self-test");
			break;
		}

		/* AESNI works; use it. */
		aesnigood = 1;
	}
#else /* !HWACCEL */
	aesnigood = 0;
#endif /* HWACCEL */

	return (aesnigood);
}

/**
 * crypto_aes_key_expand(key, len):
 * Expand the ${len}-byte AES key ${key} into a structure which can be passed
 * to crypto_aes_encrypt_block().  The length must be 16 or 32.
 */
struct crypto_aes_key *
crypto_aes_key_expand(const uint8_t * key, size_t len)
{
	AES_KEY * kexp;

	/* Sanity-check. */
	assert((len == 16) || (len == 32));

#ifdef CPUSUPPORT_X86_AESNI
	/* Use AESNI if we can. */
	if (crypto_aes_use_x86_aesni())
		return (crypto_aes_key_expand_aesni(key, len));
#endif

	/* Allocate structure. */
	if ((kexp = malloc(sizeof(AES_KEY))) == NULL)
		goto err0;

	/* Expand the key. */
	AES_set_encrypt_key(key, (int)(len * 8), kexp);

	/* Success! */
	return ((void *)kexp);

err0:
	/* Failure! */
	return (NULL);
}

/**
 * crypto_aes_encrypt_block(in, out, key):
 * Using the expanded AES key ${key}, encrypt the block ${in} and write the
 * resulting ciphertext to ${out}.  ${in} and ${out} can overlap.
 */
void
crypto_aes_encrypt_block(const uint8_t in[16], uint8_t out[16],
    const struct crypto_aes_key * key)
{

#ifdef CPUSUPPORT_X86_AESNI
	if (crypto_aes_use_x86_aesni()) {
		crypto_aes_encrypt_block_aesni(in, out, (const void *)key);
		return;
	}
#endif

	/* Get AES to do the work. */
	AES_encrypt(in, out, (const void *)key);
}

/**
 * crypto_aes_key_free(key):
 * Free the expanded AES key ${key}.
 */
void
crypto_aes_key_free(struct crypto_aes_key * key)
{

#ifdef CPUSUPPORT_X86_AESNI
	if (crypto_aes_use_x86_aesni()) {
		crypto_aes_key_free_aesni((void *)key);
		return;
	}
#endif

	/* Behave consistently with free(NULL). */
	if (key == NULL)
		return;

	/* Attempt to zero the expanded key. */
	insecure_memzero(key, sizeof(AES_KEY));

	/* Free the key. */
	free(key);
}
