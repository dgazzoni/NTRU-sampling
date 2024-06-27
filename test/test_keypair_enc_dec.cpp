#include "gtest/gtest.h"
#include "test.h"

extern "C" {
#include "api.h"
#include "rng.h"
}

extern "C" int CRYPTO_NAMESPACE_SORTING(keypair)(unsigned char *pk, unsigned char *sk);
extern "C" int CRYPTO_NAMESPACE_SHUFFLING(keypair)(unsigned char *pk, unsigned char *sk);

extern "C" int CRYPTO_NAMESPACE_SORTING(enc)(unsigned char *c, unsigned char *k, const unsigned char *pk);
extern "C" int CRYPTO_NAMESPACE_SHUFFLING(enc)(unsigned char *c, unsigned char *k, const unsigned char *pk);

extern "C" int CRYPTO_NAMESPACE_SORTING(dec)(unsigned char *k, const unsigned char *c, const unsigned char *sk);
extern "C" int CRYPTO_NAMESPACE_SHUFFLING(dec)(unsigned char *k, const unsigned char *c, const unsigned char *sk);

#define TEST_ITERATIONS 10
#define ENC_DEC_REPETITIONS 10

TEST(TEST_NAME, shuffling_keypair_enc_dec) {
    unsigned char pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES], c[CRYPTO_CIPHERTEXTBYTES];
    unsigned char k_enc[CRYPTO_BYTES], k_dec[CRYPTO_BYTES];
    unsigned char entropy_input[48] = {0};

    for (int i = 0; i < 48; i++) {
        entropy_input[i] = i;
    }

    randombytes_init(entropy_input, NULL, 256);

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        CRYPTO_NAMESPACE_SHUFFLING(keypair)(pk, sk);

        for (int j = 0; j < ENC_DEC_REPETITIONS; j++) {
            CRYPTO_NAMESPACE_SHUFFLING(enc)(c, k_enc, pk);

            CRYPTO_NAMESPACE_SHUFFLING(dec)(k_dec, c, sk);

            ASSERT_TRUE(ArraysMatch(k_enc, k_dec));
        }
    }
}

TEST(TEST_NAME, sorting_keypair__shuffling_enc_dec) {
    unsigned char pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES], c[CRYPTO_CIPHERTEXTBYTES];
    unsigned char k_enc[CRYPTO_BYTES], k_dec[CRYPTO_BYTES];
    unsigned char entropy_input[48] = {0};

    for (int i = 0; i < 48; i++) {
        entropy_input[i] = i;
    }

    randombytes_init(entropy_input, NULL, 256);

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        CRYPTO_NAMESPACE_SORTING(keypair)(pk, sk);

        for (int j = 0; j < ENC_DEC_REPETITIONS; j++) {
            CRYPTO_NAMESPACE_SHUFFLING(enc)(c, k_enc, pk);

            CRYPTO_NAMESPACE_SHUFFLING(dec)(k_dec, c, sk);

            ASSERT_TRUE(ArraysMatch(k_enc, k_dec));
        }
    }
}

TEST(TEST_NAME, sorting_enc_dec__shuffling_keypair) {
    unsigned char pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES], c[CRYPTO_CIPHERTEXTBYTES];
    unsigned char k_enc[CRYPTO_BYTES], k_dec[CRYPTO_BYTES];
    unsigned char entropy_input[48] = {0};

    for (int i = 0; i < 48; i++) {
        entropy_input[i] = i;
    }

    randombytes_init(entropy_input, NULL, 256);

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        CRYPTO_NAMESPACE_SHUFFLING(keypair)(pk, sk);

        for (int j = 0; j < ENC_DEC_REPETITIONS; j++) {
            CRYPTO_NAMESPACE_SORTING(enc)(c, k_enc, pk);

            CRYPTO_NAMESPACE_SORTING(dec)(k_dec, c, sk);

            ASSERT_TRUE(ArraysMatch(k_enc, k_dec));
        }
    }
}
