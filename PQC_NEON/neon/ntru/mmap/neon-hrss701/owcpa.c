#include "owcpa.h"
#include "sample.h"
#include "poly.h"
#include "memory_alloc.h"

static int owcpa_check_r(const poly *r)
{
  /* Check that r is in message space. */
  /* Note: Assumes that r has coefficients in {0, 1, ..., q-1} */
  int i;
  uint64_t t = 0;
  uint16_t c;
  for(i=0; i<NTRU_N; i++)
  {
    c = MODQ(r->coeffs[i]+1);
    t |= c & (NTRU_Q-4);  /* 0 if c is in {0,1,2,3} */
    t |= (c + 1) & 0x4;   /* 0 if c is in {0,1,2} */
  }
  t |= MODQ(r->coeffs[NTRU_N-1]); /* Coefficient n-1 must be zero */
  t = (-t) >> 63;
  return t;
}

#ifdef NTRU_HPS
static int owcpa_check_m(const poly *m)
{
  /* Check that m is in message space. */
  /* Note: Assumes that m has coefficients in {0,1,2}. */
  int i;
  uint64_t t = 0;
  uint16_t p1 = 0;
  uint16_t m1 = 0;
  for(i=0; i<NTRU_N; i++)
  {
    p1 += m->coeffs[i] & 0x01;
    m1 += (m->coeffs[i] & 0x02) >> 1;
  }
  /* Need p1 = m1 and p1 + m1 = NTRU_WEIGHT */
  t |= p1 ^ m1;
  t |= (p1 + m1) ^ NTRU_WEIGHT;
  t = (-t) >> 63;
  return t;
}
#endif


static void alloc_kp(void);

static poly *f_kp, *g_kp, *invf_mod3_kp, *Gf_kp, *invGf_kp, *tmp_kp, *invh_kp, *h_kp;

__attribute__((constructor)) static void alloc_kp(void) {
  f_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  g_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  invf_mod3_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  Gf_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  invGf_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  tmp_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  invh_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  h_kp = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
}

void owcpa_keypair(unsigned char *pk,
                   unsigned char *sk,
                   const unsigned char seed[NTRU_SAMPLE_FG_BYTES])
{
  int i;

  poly *f = f_kp, *g = g_kp, *invf_mod3 = invf_mod3_kp;
  poly *Gf = Gf_kp, *invGf = invGf_kp, *tmp = tmp_kp;
  poly *invh = invh_kp, *h = h_kp;

  sample_fg(f,g,seed);

  poly_S3_inv(invf_mod3, f);
  poly_S3_tobytes(sk, f);
  poly_S3_tobytes(sk+NTRU_PACK_TRINARY_BYTES, invf_mod3);

  /* Lift coeffs of f and g from Z_p to Z_q */
  poly_Z3_to_Zq(f);
  poly_Z3_to_Zq(g);

#ifdef NTRU_HRSS
  /* g = 3*(x-1)*g */
  for(i=NTRU_N-1; i>0; i--)
    g->coeffs[i] = 3*(g->coeffs[i-1] - g->coeffs[i]);
  g->coeffs[0] = -(3*g->coeffs[0]);
#endif

#ifdef NTRU_HPS
  /* g = 3*g */
  for(i=0; i<NTRU_N; i++)
    g->coeffs[i] = 3 * g->coeffs[i];
#endif

  poly_Rq_mul(Gf, g, f);

  poly_Rq_inv(invGf, Gf);

  poly_Rq_mul(tmp, invGf, f);
  poly_Sq_mul(invh, tmp, f);
  poly_Sq_tobytes(sk+2*NTRU_PACK_TRINARY_BYTES, invh);

  poly_Rq_mul(tmp, invGf, g);
  poly_Rq_mul(h, tmp, g);
  poly_Rq_sum_zero_tobytes(pk, h);
}

static void alloc_enc(void);

static poly *h_enc, *liftm_enc, *ct_enc;

__attribute__((constructor)) static void alloc_enc(void) {
  h_enc = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  liftm_enc = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  ct_enc = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
}

void owcpa_enc(unsigned char *c,
               poly *r,
               const poly *m,
               const unsigned char *pk)
{
  int i;

  poly *h = h_enc, *liftm = liftm_enc;
  poly *ct = ct_enc;

  poly_Rq_sum_zero_frombytes(h, pk);

  poly_Rq_mul(ct, r, h);

  poly_lift(liftm, m);
  for(i=0; i<NTRU_N; i++)
    ct->coeffs[i] = ct->coeffs[i] + liftm->coeffs[i];

  poly_Rq_sum_zero_tobytes(c, ct);
}

static void alloc_dec(void);

static poly *c_dec, *f_dec, *cf_dec, *mf_dec, *finv3_dec, *m_dec, *liftm_dec,
    *invh_dec, *r_dec, *b_dec;

__attribute__((constructor)) static void alloc_dec(void) {
  c_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  f_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  cf_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  mf_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  finv3_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  m_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  liftm_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  invh_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  r_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
  b_dec = MEMORY_ALLOC(32 * ((NTRU_N + 31) / 32) * sizeof(uint16_t));
}

int owcpa_dec(unsigned char *rm,
              const unsigned char *ciphertext,
              const unsigned char *secretkey)
{
  int i;
  int fail;

  poly *c = c_dec, *f = f_dec, *cf = cf_dec;
  poly *mf = mf_dec, *finv3 = finv3_dec, *m = m_dec;
  poly *liftm = liftm_dec, *invh = invh_dec, *r = r_dec;
  poly *b = b_dec;

  poly_Rq_sum_zero_frombytes(c, ciphertext);
  poly_S3_frombytes(f, secretkey);
  poly_Z3_to_Zq(f);

  poly_Rq_mul(cf, c, f);
  poly_Rq_to_S3(mf, cf);

  poly_S3_frombytes(finv3, secretkey+NTRU_PACK_TRINARY_BYTES);
  poly_S3_mul(m, mf, finv3);
  poly_S3_tobytes(rm+NTRU_PACK_TRINARY_BYTES, m);

  /* NOTE: For the IND-CCA2 KEM we must ensure that c = Enc(h, (r,m)).       */
  /* We can avoid re-computing r*h + Lift(m) as long as we check that        */
  /* r (defined as b/h mod (q, Phi_n)) and m are in the message space.       */
  /* (m can take any value in S3 in NTRU_HRSS) */
  fail = 0;
#ifdef NTRU_HPS
  fail |= owcpa_check_m(m);
#endif

  /* b = c - Lift(m) mod (q, x^n - 1) */
  poly_lift(liftm, m);
  for(i=0; i<NTRU_N; i++)
    b->coeffs[i] = c->coeffs[i] - liftm->coeffs[i];

  /* r = b / h mod (q, Phi_n) */
  poly_Sq_frombytes(invh, secretkey+2*NTRU_PACK_TRINARY_BYTES);
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
