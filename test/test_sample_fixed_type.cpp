#include "gtest/gtest.h"
#include "test.h"

extern "C" {
#include "poly.h"
#include "rng.h"
}

// Required to avoid linker errors, not used in tests
extern "C" void ntru_ref_shuffling_sample_iid(poly *r, const unsigned char uniformbytes[NTRU_SAMPLE_IID_BYTES]) {
    (void)r;
    (void)uniformbytes;
}

extern "C" void ntru_opt_shuffling_sample_iid(poly *r, const unsigned char uniformbytes[NTRU_SAMPLE_IID_BYTES]) {
    (void)r;
    (void)uniformbytes;
}

extern "C" void ntru_ref_shuffling_sample_fixed_type(poly *r, const unsigned char uniformbytes[NTRU_SAMPLE_FT_BYTES]);
extern "C" void ntru_opt_shuffling_sample_fixed_type(poly *r, const unsigned char uniformbytes[NTRU_SAMPLE_FT_BYTES]);

#define TEST_ITERATIONS 10000

TEST(TEST_NAME, ref_weights_match_expected) {
    poly r;
    unsigned char uniformbytes[NTRU_SAMPLE_FT_BYTES], entropy_input[48] = {0};
    int actual[3], expected[3] = {NTRU_N - 1 - 2 * (NTRU_Q / 16 - 1), NTRU_Q / 16 - 1, NTRU_Q / 16 - 1};

    for (int i = 0; i < 48; i++) {
        entropy_input[i] = i;
    }

    randombytes_init(entropy_input, NULL, 256);

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        randombytes(uniformbytes, sizeof(uniformbytes));

        ntru_ref_shuffling_sample_fixed_type(&r, uniformbytes);

        memset(actual, 0, sizeof(actual));

        for (int j = 0; j < NTRU_N - 1; j++) {
            actual[r.coeffs[j]]++;
        }

        for (int j = 0; j < 3; j++) {
            ASSERT_EQ(expected[j], actual[j])
                << "Iteration " << i << ": expected (" << expected[j] << ") differs from actual (" << actual[j] << ")";
        }
    }
}

TEST(TEST_NAME, ref_matches_opt) {
    poly r_ref, r_opt;
    unsigned char uniformbytes[NTRU_SAMPLE_FT_BYTES], entropy_input[48] = {0};

    for (int i = 0; i < 48; i++) {
        entropy_input[i] = i;
    }

    randombytes_init(entropy_input, NULL, 256);

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        randombytes(uniformbytes, sizeof(uniformbytes));

        ntru_ref_shuffling_sample_fixed_type(&r_ref, uniformbytes);
        ntru_opt_shuffling_sample_fixed_type(&r_opt, uniformbytes);

        ASSERT_TRUE(ArraysMatch(r_ref.coeffs, r_opt.coeffs));
    }
}
