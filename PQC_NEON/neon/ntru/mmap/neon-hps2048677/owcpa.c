#include "owcpa.h"

#include "memory_alloc.h"
#include "poly.h"
#include "sample.h"

static int owcpa_check_ciphertext(const unsigned char *ciphertext) {
    /* A ciphertext is log2(q)*(n-1) bits packed into bytes.  */
    /* Check that any unused bits of the final byte are zero. */

    uint16_t t = 0;

    t = ciphertext[NTRU_CIPHERTEXTBYTES - 1];
    t &= 0xff << (8 - (7 & (NTRU_LOGQ * NTRU_PACK_DEG)));

    /* We have 0 <= t < 256 */
    /* Return 0 on success (t=0), 1 on failure */
    return (int)(1 & ((~t + 1) >> 15));
}

static int owcpa_check_r(const poly *r) {
    /* A valid r has coefficients in {0,1,q-1} and has r[N-1] = 0 */
    /* Note: We may assume that 0 <= r[i] <= q-1 for all i        */

    int i;
    uint32_t t = 0;
    uint16_t c;
    for (i = 0; i < NTRU_N - 1; i++) {
        c = r->coeffs[i];
        t |= (c + 1) & (NTRU_Q - 4); /* 0 iff c is in {-1,0,1,2} */
        t |= (c + 2) & 4;            /* 1 if c = 2, 0 if c is in {-1,0,1} */
    }
    t |= r->coeffs[NTRU_N - 1]; /* Coefficient n-1 must be zero */

    /* We have 0 <= t < 2^16. */
    /* Return 0 on success (t=0), 1 on failure */
    return (int)(1 & ((~t + 1) >> 31));
}

#ifdef NTRU_HPS
static int owcpa_check_m(const poly *m) {
    /* Check that m is in message space, i.e.                  */
    /*  (1)  |{i : m[i] = 1}| = |{i : m[i] = 2}|, and          */
    /*  (2)  |{i : m[i] != 0}| = NTRU_WEIGHT.                  */
    /* Note: We may assume that m has coefficients in {0,1,2}. */

    int i;
    uint32_t t = 0;
    uint16_t ps = 0;
    uint16_t ms = 0;
    for (i = 0; i < NTRU_N; i++) {
        ps += m->coeffs[i] & 1;
        ms += m->coeffs[i] & 2;
    }
    t |= ps ^ (ms >> 1);   /* 0 if (1) holds */
    t |= ms ^ NTRU_WEIGHT; /* 0 if (1) and (2) hold */

    /* We have 0 <= t < 2^16. */
    /* Return 0 on success (t=0), 1 on failure */
    return (int)(1 & ((~t + 1) >> 31));
}
#endif

static poly *f_kp, *g_kp, *g_kp, *invf_mod3_kp, *gf_kp, *invgf_kp, *tmp1_kp, *tmp2_kp, *invh_kp, *h_kp;

__attribute__((constructor)) static void alloc_kp(void) {
    f_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    g_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    invf_mod3_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    gf_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    invgf_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    tmp1_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    tmp2_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    invh_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    h_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
}

void owcpa_keypair(unsigned char *pk, unsigned char *sk, const unsigned char seed[NTRU_SAMPLE_FG_BYTES]) {
    poly *f = f_kp, *g = g_kp, *invf_mod3 = invf_mod3_kp;
    poly *gf = gf_kp, *invgf = invgf_kp, *tmp1 = tmp1_kp, *tmp2 = tmp2_kp;
    poly *invh = invh_kp, *h = h_kp;

    sample_fg(f, g, seed);

    poly_S3_inv(invf_mod3, f);
    poly_S3_tobytes(sk, f);
    poly_S3_tobytes(sk + NTRU_PACK_TRINARY_BYTES, invf_mod3);

    /* Lift coeffs of f and g from Z_p to Z_q */
    poly_Z3_to_Zq(f);
    poly_Z3_to_Zq(g);

#ifdef NTRU_HRSS
    /* g = 3*(x-1)*g */
    polyhrss_mul3(g);
#endif

#ifdef NTRU_HPS
    /* g = 3*g */
    polyhps_mul3(g);
#endif

    poly_Rq_mul(gf, g, f);

    poly_Rq_inv(invgf, gf);

    poly_Rq_mul(tmp1, invgf, f);
    poly_Rq_mul(tmp2, invgf, g);
    poly_Sq_mul(invh, tmp1, f);
    poly_Rq_mul(h, tmp2, g);

    poly_Rq_sum_zero_tobytes(pk, h);                          // x4
    poly_Sq_tobytes(sk + 2 * NTRU_PACK_TRINARY_BYTES, invh);  // x3
}

static poly *h_enc, *ct_enc;

__attribute__((constructor)) static void alloc_enc(void) {
    h_enc = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    ct_enc = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
}

void owcpa_enc(unsigned char *c, poly *r, const poly *m, const unsigned char *pk) {
    poly *h = h_enc;
    poly *ct = ct_enc;

    poly_Rq_sum_zero_frombytes(h, pk);

    poly_Rq_mul(ct, r, h);

    // c += Lift(m);
    poly_lift_add(ct, m);

    poly_Rq_sum_zero_tobytes(c, ct);
}

static poly *c_dec, *f_dec, *cf_dec, *mf_dec, *finv3_dec, *m_dec, *invh_dec, *r_dec, *b_dec;

__attribute__((constructor)) static void alloc_dec(void) {
    c_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    f_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    cf_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    mf_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    finv3_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    m_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    invh_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    r_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
    b_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
}

int owcpa_dec(unsigned char *rm, const unsigned char *ciphertext, const unsigned char *secretkey) {
    int fail;

    poly *c = c_dec, *f = f_dec, *finv3 = finv3_dec, *cf = cf_dec;
    poly *mf = mf_dec, *m = m_dec;
    poly *invh = invh_dec, *r = r_dec;
    poly *b = b_dec;

    poly_Rq_sum_zero_frombytes(c, ciphertext);
    poly_S3_frombytes(f, secretkey);
    poly_S3_frombytes(finv3, secretkey + NTRU_PACK_TRINARY_BYTES);

    poly_Z3_to_Zq(f);
    poly_Rq_mul(cf, c, f);
    poly_Rq_to_S3(mf, cf);

    poly_S3_mul(m, mf, finv3);
    poly_S3_tobytes(rm + NTRU_PACK_TRINARY_BYTES, m);

    fail = 0;

    /* Check that the unused bits of the last byte of the ciphertext are zero */
    fail |= owcpa_check_ciphertext(ciphertext);

    /* For the IND-CCA2 KEM we must ensure that c = Enc(h, (r,m)).             */
    /* We can avoid re-computing r*h + Lift(m) as long as we check that        */
    /* r (defined as b/h mod (q, Phi_n)) and m are in the message space.       */
    /* (m can take any value in S3 in NTRU_HRSS) */
#ifdef NTRU_HPS
    fail |= owcpa_check_m(m);
#endif
    poly_Sq_frombytes(invh, secretkey + 2 * NTRU_PACK_TRINARY_BYTES);

    /* b = c - Lift(m) mod (q, x^n - 1) */
    poly_lift_sub(b, c, m);

    /* r = b / h mod (q, Phi_n) */
    poly_Sq_mul(r, b, invh);

    /* NOTE: Our definition of r as b/h mod (q, Phi_n) follows Figure 4 of     */
    /*   [Sch18] https://eprint.iacr.org/2018/1174/20181203:032458.            */
    /* This differs from Figure 10 of Saito--Xagawa--Yamakawa                  */
    /*   [SXY17] https://eprint.iacr.org/2017/1005/20180516:055500             */
    /* where r gets a final reduction modulo p.                                */
    /* We need this change to use Proposition 1 of [Sch18].                    */

    /* Proposition 1 of [Sch18] shows that re-encryption with (r,m) yields c.  */
    /* if and only if fail==0 after the following call to owcpa_check_r        */
    /* The procedure given in Fig. 8 of [Sch18] can be skipped because we have */
    /* c(1) = 0 due to the use of poly_Rq_sum_zero_{to,from}bytes.             */
    fail |= owcpa_check_r(r);

    poly_trinary_Zq_to_Z3(r);
    poly_S3_tobytes(rm, r);

    return fail;
}
