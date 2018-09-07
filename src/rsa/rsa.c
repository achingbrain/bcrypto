#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "openssl/opensslv.h"
#include "rsa.h"

#if OPENSSL_VERSION_NUMBER >= 0x1010008fL

#include "openssl/bn.h"
#include "openssl/rsa.h"
#include "openssl/objects.h"

void
bcrypto_rsa_key_init(bcrypto_rsa_key_t *key) {
  assert(key);
  memset((void *)key, 0x00, sizeof(bcrypto_rsa_key_t));
}

void
bcrypto_rsa_key_free(bcrypto_rsa_key_t *key) {
  assert(key);
  free((void *)key);
}

static size_t
bcrypto_count_bits(const uint8_t *in, size_t in_len) {
  if (in == NULL)
    return 0;

  size_t i = 0;

  for (; i < in_len; i++) {
    if (in[i] != 0)
      break;
  }

  size_t bits = (in_len - i) * 8;

  if (bits == 0)
    return 0;

  bits -= 8;

  uint32_t oct = in[i];

  while (oct) {
    bits += 1;
    oct >>= 1;
  }

  return bits;
}

static bool
bcrypto_rsa_sane_pubkey(const bcrypto_rsa_key_t *key) {
  if (key == NULL)
    return false;

  size_t nb = bcrypto_count_bits(key->nd, key->nl);

  if (nb < 512 || nb > 16384)
    return false;

  size_t eb = bcrypto_count_bits(key->ed, key->el);

  if (eb < 2 || eb > 33)
    return false;

  if ((key->ed[key->el - 1] & 1) == 0)
    return false;

  return true;
}

static bool
bcrypto_rsa_sane_privkey(const bcrypto_rsa_key_t *key) {
  if (!bcrypto_rsa_sane_pubkey(key))
    return false;

  size_t nb = bcrypto_count_bits(key->nd, key->nl);
  size_t db = bcrypto_count_bits(key->dd, key->dl);

  if (db == 0 || db > nb)
    return false;

  size_t pb = bcrypto_count_bits(key->pd, key->pl);
  size_t qb = bcrypto_count_bits(key->qd, key->ql);

  if (pb + qb != nb)
    return false;

  size_t dpb = bcrypto_count_bits(key->dpd, key->dpl);

  if (dpb == 0 || dpb > pb)
    return false;

  size_t dqb = bcrypto_count_bits(key->dqd, key->dql);

  if (dqb == 0 || dqb > qb)
    return false;

  size_t qib = bcrypto_count_bits(key->qid, key->qil);

  if (qib == 0 || qib > pb)
    return false;

  return true;
}

static bool
bcrypto_rsa_sane_compute(const bcrypto_rsa_key_t *key) {
  if (key == NULL)
    return false;

  size_t nb = bcrypto_count_bits(key->nd, key->nl);
  size_t eb = bcrypto_count_bits(key->ed, key->el);
  size_t db = bcrypto_count_bits(key->dd, key->dl);
  size_t pb = bcrypto_count_bits(key->pd, key->pl);
  size_t qb = bcrypto_count_bits(key->qd, key->ql);
  size_t dpb = bcrypto_count_bits(key->dpd, key->dpl);
  size_t dqb = bcrypto_count_bits(key->dqd, key->dql);
  size_t qib = bcrypto_count_bits(key->qid, key->qil);

  if (pb == 0 || qb == 0)
    return false;

  if (eb == 0 && db == 0)
    return false;

  if (nb != 0) {
    if (nb < 512 || nb > 16384)
      return false;

    if (pb + qb != nb)
      return false;
  }

  if (eb != 0) {
    if (eb < 2 || eb > 33)
      return false;

    if ((key->ed[key->el - 1] & 1) == 0)
      return false;
  }

  if (db != 0) {
    if (db > pb + qb)
      return false;
  }

  if (dpb != 0) {
    if (dpb > pb)
      return false;
  }

  if (dqb != 0) {
    if (dqb > qb)
      return false;
  }

  if (qib != 0) {
    if (qib > pb)
      return false;
  }

  return true;
}

static bool
bcrypto_rsa_needs_compute(const bcrypto_rsa_key_t *key) {
  if (key == NULL)
    return false;

  return bcrypto_count_bits(key->nd, key->nl) == 0
      || bcrypto_count_bits(key->ed, key->el) == 0
      || bcrypto_count_bits(key->dd, key->dl) == 0
      || bcrypto_count_bits(key->dpd, key->dpl) == 0
      || bcrypto_count_bits(key->dqd, key->dql) == 0
      || bcrypto_count_bits(key->qid, key->qil) == 0;
}

static RSA *
bcrypto_rsa_key2priv(const bcrypto_rsa_key_t *priv) {
  if (priv == NULL)
    return NULL;

  RSA *priv_r = NULL;
  BIGNUM *n = NULL;
  BIGNUM *e = NULL;
  BIGNUM *d = NULL;
  BIGNUM *p = NULL;
  BIGNUM *q = NULL;
  BIGNUM *dp = NULL;
  BIGNUM *dq = NULL;
  BIGNUM *qi = NULL;

  priv_r = RSA_new();

  if (!priv_r)
    goto fail;

  n = BN_bin2bn(priv->nd, priv->nl, NULL);
  e = BN_bin2bn(priv->ed, priv->el, NULL);
  d = BN_bin2bn(priv->dd, priv->dl, NULL);
  p = BN_bin2bn(priv->pd, priv->pl, NULL);
  q = BN_bin2bn(priv->qd, priv->ql, NULL);
  dp = BN_bin2bn(priv->dpd, priv->dpl, NULL);
  dq = BN_bin2bn(priv->dqd, priv->dql, NULL);
  qi = BN_bin2bn(priv->qid, priv->qil, NULL);

  if (!n || !e || !d || !p || !q || !dp || !dq || !qi)
    goto fail;

  if (!RSA_set0_key(priv_r, n, e, d))
    goto fail;

  n = NULL;
  e = NULL;
  d = NULL;

  if (!RSA_set0_factors(priv_r, p, q))
    goto fail;

  p = NULL;
  q = NULL;

  if (!RSA_set0_crt_params(priv_r, dp, dq, qi))
    goto fail;

  return priv_r;

fail:
  if (priv_r)
    RSA_free(priv_r);

  if (n)
    BN_free(n);

  if (e)
    BN_free(e);

  if (d)
    BN_free(d);

  if (p)
    BN_free(p);

  if (q)
    BN_free(q);

  if (dp)
    BN_free(dp);

  if (dq)
    BN_free(dq);

  if (qi)
    BN_free(qi);

  return NULL;
}

static RSA *
bcrypto_rsa_key2pub(const bcrypto_rsa_key_t *pub) {
  if (pub == NULL)
    return NULL;

  RSA *pub_r = NULL;
  BIGNUM *n = NULL;
  BIGNUM *e = NULL;

  pub_r = RSA_new();

  if (!pub_r)
    goto fail;

  n = BN_bin2bn(pub->nd, pub->nl, NULL);
  e = BN_bin2bn(pub->ed, pub->el, NULL);

  if (!n || !e)
    goto fail;

  if (!RSA_set0_key(pub_r, n, e, NULL))
    goto fail;

  return pub_r;

fail:
  if (pub_r)
    RSA_free(pub_r);

  if (n)
    BN_free(n);

  if (e)
    BN_free(e);

  return NULL;
}

static bcrypto_rsa_key_t *
bcrypto_rsa_priv2key(const RSA *priv_r) {
  if (priv_r == NULL)
    return NULL;

  uint8_t *arena = NULL;

  const BIGNUM *n = NULL;
  const BIGNUM *e = NULL;
  const BIGNUM *d = NULL;
  const BIGNUM *p = NULL;
  const BIGNUM *q = NULL;
  const BIGNUM *dp = NULL;
  const BIGNUM *dq = NULL;
  const BIGNUM *qi = NULL;

  RSA_get0_key(priv_r, &n, &e, &d);
  RSA_get0_factors(priv_r, &p, &q);
  RSA_get0_crt_params(priv_r, &dp, &dq, &qi);

  if (!n || !e || !d || !p || !q || !dp || !dq || !qi)
    goto fail;

  size_t nl = BN_num_bytes(n);
  size_t el = BN_num_bytes(e);
  size_t dl = BN_num_bytes(d);
  size_t pl = BN_num_bytes(p);
  size_t ql = BN_num_bytes(q);
  size_t dpl = BN_num_bytes(dp);
  size_t dql = BN_num_bytes(dq);
  size_t qil = BN_num_bytes(qi);

  size_t kl = sizeof(bcrypto_rsa_key_t);
  size_t size = kl + nl + el + dl + pl + ql + dpl + dql + qil;

  arena = malloc(size);

  if (!arena)
    goto fail;

  size_t pos = 0;

  bcrypto_rsa_key_t *priv;

  priv = (bcrypto_rsa_key_t *)&arena[pos];
  bcrypto_rsa_key_init(priv);
  pos += kl;

  priv->nd = (uint8_t *)&arena[pos];
  priv->nl = nl;
  pos += nl;

  priv->ed = (uint8_t *)&arena[pos];
  priv->el = el;
  pos += el;

  priv->dd = (uint8_t *)&arena[pos];
  priv->dl = dl;
  pos += dl;

  priv->pd = (uint8_t *)&arena[pos];
  priv->pl = pl;
  pos += pl;

  priv->qd = (uint8_t *)&arena[pos];
  priv->ql = ql;
  pos += ql;

  priv->dpd = (uint8_t *)&arena[pos];
  priv->dpl = dpl;
  pos += dpl;

  priv->dqd = (uint8_t *)&arena[pos];
  priv->dql = dql;
  pos += dql;

  priv->qid = (uint8_t *)&arena[pos];
  priv->qil = qil;
  pos += qil;

  assert(BN_bn2bin(n, priv->nd) != -1);
  assert(BN_bn2bin(e, priv->ed) != -1);
  assert(BN_bn2bin(d, priv->dd) != -1);
  assert(BN_bn2bin(p, priv->pd) != -1);
  assert(BN_bn2bin(q, priv->qd) != -1);
  assert(BN_bn2bin(dp, priv->dpd) != -1);
  assert(BN_bn2bin(dq, priv->dqd) != -1);
  assert(BN_bn2bin(qi, priv->qid) != -1);

  return priv;

fail:
  if (arena)
    free(arena);

  return NULL;
}

static bcrypto_rsa_key_t *
bcrypto_rsa_pub2key(const RSA *pub_r) {
  if (pub_r == NULL)
    return NULL;

  uint8_t *arena = NULL;

  const BIGNUM *n = NULL;
  const BIGNUM *e = NULL;

  RSA_get0_key(pub_r, &n, &e, NULL);

  if (!n || !e)
    goto fail;

  size_t nl = BN_num_bytes(n);
  size_t el = BN_num_bytes(e);

  size_t kl = sizeof(bcrypto_rsa_key_t);
  size_t size = kl + nl + el;

  arena = malloc(size);

  if (!arena)
    goto fail;

  size_t pos = 0;

  bcrypto_rsa_key_t *pub;

  pub = (bcrypto_rsa_key_t *)&arena[pos];
  bcrypto_rsa_key_init(pub);
  pos += kl;

  pub->nd = (uint8_t *)&arena[pos];
  pub->nl = nl;
  pos += nl;

  pub->ed = (uint8_t *)&arena[pos];
  pub->el = el;
  pos += el;

  assert(BN_bn2bin(n, pub->nd) != -1);
  assert(BN_bn2bin(e, pub->ed) != -1);

  return pub;

fail:
  if (arena)
    free(arena);

  return NULL;
}

static int
bcrypto_rsa_type(const char *alg) {
  if (alg == NULL)
    return -1;

  int type = -1;

  if (strcmp(alg, "md5") == 0)
    type = NID_md5;
  else if (strcmp(alg, "ripemd160") == 0)
    type = NID_ripemd160;
  else if (strcmp(alg, "sha1") == 0)
    type = NID_sha1;
  else if (strcmp(alg, "sha224") == 0)
    type = NID_sha224;
  else if (strcmp(alg, "sha256") == 0)
    type = NID_sha256;
  else if (strcmp(alg, "sha384") == 0)
    type = NID_sha384;
  else if (strcmp(alg, "sha512") == 0)
    type = NID_sha512;

  return type;
}

bcrypto_rsa_key_t *
bcrypto_rsa_privkey_generate(int bits, unsigned long long exp) {
  RSA *priv_r = NULL;
  BIGNUM *exp_bn = NULL;

  if (bits < 512 || bits > 16384)
    goto fail;

  if (exp < 3ull || exp > 0x1ffffffffull)
    goto fail;

  if ((exp & 1ull) == 0)
    goto fail;

  priv_r = RSA_new();

  if (!priv_r)
    goto fail;

  exp_bn = BN_new();

  if (!exp_bn)
    goto fail;

  if (!BN_set_word(exp_bn, (BN_ULONG)exp))
    goto fail;

  if (!RSA_generate_key_ex(priv_r, bits, exp_bn, NULL))
    goto fail;

  bcrypto_rsa_key_t *priv = bcrypto_rsa_priv2key(priv_r);

  if (!priv)
    goto fail;

  RSA_free(priv_r);
  BN_free(exp_bn);

  return priv;

fail:
  if (priv_r)
    RSA_free(priv_r);

  if (exp_bn)
    BN_free(exp_bn);

  return NULL;
}

bool
bcrypto_rsa_privkey_compute(
  const bcrypto_rsa_key_t *priv,
  bcrypto_rsa_key_t **key
) {
  assert(key);

  RSA *priv_r = NULL;
  BIGNUM *rsa_n = NULL;
  BIGNUM *rsa_e = NULL;
  BIGNUM *rsa_d = NULL;
  BIGNUM *rsa_p = NULL;
  BIGNUM *rsa_q = NULL;
  BIGNUM *rsa_dmp1 = NULL;
  BIGNUM *rsa_dmq1 = NULL;
  BIGNUM *rsa_iqmp = NULL;
  BN_CTX *ctx = NULL;
  BIGNUM *r0 = NULL;
  BIGNUM *r1 = NULL;
  BIGNUM *r2 = NULL;
  RSA *out_r = NULL;
  bcrypto_rsa_key_t *out = NULL;

  if (!bcrypto_rsa_sane_compute(priv))
    goto fail;

  if (!bcrypto_rsa_needs_compute(priv)) {
    *key = NULL;
    return true;
  }

  priv_r = bcrypto_rsa_key2priv(priv);

  if (!priv_r)
    goto fail;

  const BIGNUM *n = NULL;
  const BIGNUM *e = NULL;
  const BIGNUM *d = NULL;
  const BIGNUM *p = NULL;
  const BIGNUM *q = NULL;
  const BIGNUM *dp = NULL;
  const BIGNUM *dq = NULL;
  const BIGNUM *qi = NULL;

  RSA_get0_key(priv_r, &n, &e, &d);
  RSA_get0_factors(priv_r, &p, &q);
  RSA_get0_crt_params(priv_r, &dp, &dq, &qi);
  assert(n && e && d && p && q && dp && dq && qi);

  rsa_n = BN_new();
  rsa_e = BN_new();
  rsa_d = BN_new();
  rsa_p = BN_new();
  rsa_q = BN_new();
  rsa_dmp1 = BN_new();
  rsa_dmq1 = BN_new();
  rsa_iqmp = BN_new();

  if (!rsa_n
      || !rsa_e
      || !rsa_d
      || !rsa_p
      || !rsa_q
      || !rsa_dmp1
      || !rsa_dmq1
      || !rsa_iqmp) {
    goto fail;
  }

  if (!BN_copy(rsa_n, n)
      || !BN_copy(rsa_e, e)
      || !BN_copy(rsa_d, d)
      || !BN_copy(rsa_p, p)
      || !BN_copy(rsa_q, q)
      || !BN_copy(rsa_dmp1, dp)
      || !BN_copy(rsa_dmq1, dq)
      || !BN_copy(rsa_iqmp, qi)) {
    goto fail;
  }

  ctx = BN_CTX_new();
  r0 = BN_new();
  r1 = BN_new();
  r2 = BN_new();

  if (!ctx || !r0 || !r1 || !r2)
    goto fail;

  // See: https://github.com/openssl/openssl/blob/master/crypto/rsa/rsa_gen.c

  if (BN_is_zero(rsa_n)) {
    // modulus n = p * q * r_3 * r_4
    if (!BN_mul(rsa_n, rsa_p, rsa_q, ctx))
      goto fail;
  }

  // p - 1
  if (!BN_sub(r1, rsa_p, BN_value_one()))
    goto fail;

  // q - 1
  if (!BN_sub(r2, rsa_q, BN_value_one()))
    goto fail;

  // (p - 1)(q - 1)
  if (!BN_mul(r0, r1, r2, ctx))
    goto fail;

  if (BN_is_zero(rsa_e)) {
    BIGNUM *pr0 = BN_new();

    if (pr0 == NULL)
      goto fail;

    BN_with_flags(pr0, r0, BN_FLG_CONSTTIME);

    if (!BN_mod_inverse(rsa_e, rsa_d, pr0, ctx)) {
      BN_free(pr0);
      goto fail;
    }

    BN_free(pr0);
  }

  if (BN_is_zero(rsa_d)) {
    BIGNUM *pr0 = BN_new();

    if (pr0 == NULL)
      goto fail;

    BN_with_flags(pr0, r0, BN_FLG_CONSTTIME);

    if (!BN_mod_inverse(rsa_d, rsa_e, pr0, ctx)) {
      BN_free(pr0);
      goto fail;
    }

    BN_free(pr0);
  }

  if (BN_is_zero(rsa_dmp1) || BN_is_zero(rsa_dmq1)) {
    BIGNUM *d = BN_new();

    if (d == NULL)
      goto fail;

    BN_with_flags(d, rsa_d, BN_FLG_CONSTTIME);

    // calculate d mod (p-1) and d mod (q - 1)
    if (!BN_mod(rsa_dmp1, d, r1, ctx)
        || !BN_mod(rsa_dmq1, d, r2, ctx)) {
      BN_free(d);
      goto fail;
    }

    BN_free(d);
  }

  if (BN_is_zero(rsa_iqmp)) {
    BIGNUM *p = BN_new();

    if (p == NULL)
      goto fail;

    BN_with_flags(p, rsa_p, BN_FLG_CONSTTIME);

    // calculate inverse of q mod p
    if (!BN_mod_inverse(rsa_iqmp, rsa_q, p, ctx)) {
      BN_free(p);
      goto fail;
    }

    BN_free(p);
  }

  out_r = RSA_new();

  if (!out_r)
    goto fail;

  assert(RSA_set0_key(out_r, rsa_n, rsa_e, rsa_d));

  rsa_n = NULL;
  rsa_e = NULL;
  rsa_d = NULL;

  assert(RSA_set0_factors(out_r, rsa_p, rsa_q));

  rsa_p = NULL;
  rsa_q = NULL;

  assert(RSA_set0_crt_params(out_r, rsa_dmp1, rsa_dmq1, rsa_iqmp));

  rsa_dmp1 = NULL;
  rsa_dmq1 = NULL;
  rsa_iqmp = NULL;

  out = bcrypto_rsa_priv2key(out_r);

  if (!out)
    goto fail;

  RSA_free(priv_r);
  BN_CTX_free(ctx);
  BN_free(r0);
  BN_free(r1);
  BN_free(r2);
  RSA_free(out_r);

  *key = out;

  return true;

fail:
  if (priv_r)
    RSA_free(priv_r);

  if (rsa_n)
    BN_free(rsa_n);

  if (rsa_e)
    BN_free(rsa_e);

  if (rsa_d)
    BN_free(rsa_d);

  if (rsa_p)
    BN_free(rsa_p);

  if (rsa_q)
    BN_free(rsa_q);

  if (rsa_dmp1)
    BN_free(rsa_dmp1);

  if (rsa_dmq1)
    BN_free(rsa_dmq1);

  if (rsa_iqmp)
    BN_free(rsa_iqmp);

  if (ctx)
    BN_CTX_free(ctx);

  if (r0)
    BN_free(r0);

  if (r1)
    BN_free(r1);

  if (r2)
    BN_free(r2);

  if (out_r)
    RSA_free(out_r);

  return false;
}

bool
bcrypto_rsa_privkey_verify(const bcrypto_rsa_key_t *priv) {
  RSA *priv_r = NULL;

  if (!bcrypto_rsa_sane_privkey(priv))
    goto fail;

  priv_r = bcrypto_rsa_key2priv(priv);

  if (!priv_r)
    goto fail;

  if (RSA_check_key(priv_r) <= 0)
    goto fail;

  RSA_free(priv_r);

  return true;

fail:
  if (priv_r)
    RSA_free(priv_r);

  return false;
}

bool
bcrypto_rsa_privkey_export(
  const bcrypto_rsa_key_t *priv,
  uint8_t **out,
  size_t *out_len
) {
  assert(out && out_len);

  if (!bcrypto_rsa_sane_privkey(priv))
    return false;

  RSA *priv_r = bcrypto_rsa_key2priv(priv);

  if (!priv_r)
    return false;

  uint8_t *buf = NULL;
  int len = i2d_RSAPrivateKey(priv_r, &buf);

  RSA_free(priv_r);

  if (len <= 0)
    return false;

  *out = buf;
  *out_len = (size_t)len;

  return true;
}

bcrypto_rsa_key_t *
bcrypto_rsa_privkey_import(
  const uint8_t *raw,
  size_t raw_len
) {
  RSA *priv_r = NULL;
  const uint8_t *p = raw;

  if (!d2i_RSAPrivateKey(&priv_r, &p, raw_len))
    return false;

  bcrypto_rsa_key_t *k = bcrypto_rsa_priv2key(priv_r);

  RSA_free(priv_r);

  return k;
}

bool
bcrypto_rsa_pubkey_verify(const bcrypto_rsa_key_t *pub) {
  return bcrypto_rsa_sane_pubkey(pub);
}

bool
bcrypto_rsa_pubkey_export(
  const bcrypto_rsa_key_t *pub,
  uint8_t **out,
  size_t *out_len
) {
  assert(out && out_len);

  if (!bcrypto_rsa_sane_pubkey(pub))
    return false;

  RSA *pub_r = bcrypto_rsa_key2pub(pub);

  if (!pub_r)
    return false;

  uint8_t *buf = NULL;
  int len = i2d_RSAPublicKey(pub_r, &buf);

  RSA_free(pub_r);

  if (len <= 0)
    return false;

  *out = buf;
  *out_len = (size_t)len;

  return true;
}

bcrypto_rsa_key_t *
bcrypto_rsa_pubkey_import(
  const uint8_t *raw,
  size_t raw_len
) {
  RSA *pub_r = NULL;
  const uint8_t *p = raw;

  if (!d2i_RSAPublicKey(&pub_r, &p, raw_len))
    return false;

  bcrypto_rsa_key_t *k = bcrypto_rsa_pub2key(pub_r);

  RSA_free(pub_r);

  return k;
}

bool
bcrypto_rsa_sign(
  const char *alg,
  const uint8_t *msg,
  size_t msg_len,
  const bcrypto_rsa_key_t *priv,
  uint8_t **sig,
  size_t *sig_len
) {
  assert(sig && sig_len);

  RSA *priv_r = NULL;
  uint8_t *sig_buf = NULL;
  size_t sig_buf_len = 0;

  int type = bcrypto_rsa_type(alg);

  if (type == -1)
    goto fail;

  if (msg == NULL || msg_len < 1 || msg_len > 64)
    goto fail;

  if (!bcrypto_rsa_sane_privkey(priv))
    goto fail;

  priv_r = bcrypto_rsa_key2priv(priv);

  if (!priv_r)
    goto fail;

  sig_buf_len = RSA_size(priv_r);
  sig_buf = (uint8_t *)malloc(sig_buf_len * sizeof(uint8_t));

  if (!sig_buf)
    goto fail;

  // Protect against side-channel attacks.
  if (!RSA_blinding_on(priv_r, NULL))
    goto fail;

  int result = RSA_sign(
    type,
    msg,
    msg_len,
    sig_buf,
    (unsigned int *)&sig_buf_len,
    priv_r
  );

  RSA_blinding_off(priv_r);

  if (!result)
    goto fail;

  RSA_free(priv_r);

  *sig = sig_buf;
  *sig_len = sig_buf_len;

  return true;

fail:
  if (priv_r)
    RSA_free(priv_r);

  if (sig_buf)
    free(sig_buf);

  return false;
}

bool
bcrypto_rsa_verify(
  const char *alg,
  const uint8_t *msg,
  size_t msg_len,
  const uint8_t *sig,
  size_t sig_len,
  const bcrypto_rsa_key_t *pub
) {
  RSA *pub_r = NULL;

  int type = bcrypto_rsa_type(alg);

  if (type == -1)
    goto fail;

  if (msg == NULL || msg_len < 1 || msg_len > 64)
    goto fail;

  if (sig == NULL || sig_len < 1 || sig_len > 3072)
    goto fail;

  if (!bcrypto_rsa_sane_pubkey(pub))
    goto fail;

  pub_r = bcrypto_rsa_key2pub(pub);

  if (!pub_r)
    goto fail;

  if (!RSA_verify(type, msg, msg_len, sig, sig_len, pub_r))
    goto fail;

  RSA_free(pub_r);

  return true;
fail:
  if (pub_r)
    RSA_free(pub_r);

  return false;
}

bool
bcrypto_rsa_encrypt(
  int type,
  const uint8_t *msg,
  size_t msg_len,
  const bcrypto_rsa_key_t *pub,
  uint8_t **ct,
  size_t *ct_len
) {
  assert(ct && ct_len);

  RSA *pub_r = NULL;
  uint8_t *out = NULL;

  if (type < 0 || type > 1)
    goto fail;

  if (msg == NULL || msg_len < 1)
    goto fail;

  if (!bcrypto_rsa_sane_pubkey(pub))
    goto fail;

  pub_r = bcrypto_rsa_key2pub(pub);

  if (!pub_r)
    goto fail;

  int padding = type ? RSA_PKCS1_OAEP_PADDING : RSA_PKCS1_PADDING;
  int max = type ? RSA_size(pub_r) - 41 : RSA_size(pub_r) - 11;

  if (max < 0 || msg_len > (size_t)max)
    goto fail;

  out = malloc(RSA_size(pub_r) * 2);

  if (!out)
    goto fail;

  int out_len = RSA_public_encrypt((int)msg_len, msg, out, pub_r, padding);

  if (out_len < 0)
    goto fail;

  RSA_free(pub_r);

  *ct = out;
  *ct_len = (size_t)out_len;

  return true;

fail:
  if (pub_r)
    RSA_free(pub_r);

  if (out)
    free(out);

  return false;
}

bool
bcrypto_rsa_decrypt(
  int type,
  const uint8_t *msg,
  size_t msg_len,
  const bcrypto_rsa_key_t *priv,
  uint8_t **pt,
  size_t *pt_len
) {
  assert(pt && pt_len);

  RSA *priv_r = NULL;
  uint8_t *out = NULL;

  if (type < 0 || type > 1)
    goto fail;

  if (msg == NULL || msg_len < 1)
    goto fail;

  if (!bcrypto_rsa_sane_privkey(priv))
    goto fail;

  priv_r = bcrypto_rsa_key2priv(priv);

  if (!priv_r)
    goto fail;

  int padding = type ? RSA_PKCS1_OAEP_PADDING : RSA_PKCS1_PADDING;

  out = malloc(RSA_size(priv_r));

  if (!out)
    goto fail;

  // Protect against side-channel attacks.
  if (!RSA_blinding_on(priv_r, NULL))
    goto fail;

  int out_len = RSA_private_decrypt((int)msg_len, msg, out, priv_r, padding);

  RSA_blinding_off(priv_r);

  if (out_len < 0)
    goto fail;

  RSA_free(priv_r);

  *pt = out;
  *pt_len = (size_t)out_len;

  return true;

fail:
  if (priv_r)
    RSA_free(priv_r);

  if (out)
    free(out);

  return false;
}

#else

void
bcrypto_rsa_key_init(bcrypto_rsa_key_t *key) {}

void
bcrypto_rsa_key_free(bcrypto_rsa_key_t *key) {}

bcrypto_rsa_key_t *
bcrypto_rsa_privkey_generate(int bits, int exp) {
  return NULL;
}

bool
bcrypto_rsa_privkey_compute(
  const bcrypto_rsa_key_t *priv,
  bcrypto_rsa_key_t **key
) {
  return NULL;
}

bool
bcrypto_rsa_privkey_verify(const bcrypto_rsa_key_t *priv) {
  return false;
}

bool
bcrypto_rsa_privkey_export(
  const bcrypto_rsa_key_t *priv,
  uint8_t **out,
  size_t *out_len
) {
  return false;
}

bcrypto_rsa_key_t *
bcrypto_rsa_privkey_import(
  const uint8_t *raw,
  size_t raw_len
) {
  return NULL;
}

bool
bcrypto_rsa_pubkey_verify(const bcrypto_rsa_key_t *pub) {
  return false;
}

bool
bcrypto_rsa_pubkey_export(
  const bcrypto_rsa_key_t *pub,
  uint8_t **out,
  size_t *out_len
) {
  return false;
}

bcrypto_rsa_key_t *
bcrypto_rsa_pubkey_import(
  const uint8_t *raw,
  size_t raw_len
) {
  return NULL;
}

bool
bcrypto_rsa_sign(
  const char *alg,
  const uint8_t *msg,
  size_t msg_len,
  const bcrypto_rsa_key_t *priv,
  uint8_t **sig,
  size_t *sig_len
) {
  return false;
}

bool
bcrypto_rsa_verify(
  const char *alg,
  const uint8_t *msg,
  size_t msg_len,
  const uint8_t *sig,
  size_t sig_len,
  const bcrypto_rsa_key_t *pub
) {
  return false;
}

bool
bcrypto_rsa_encrypt(
  int type,
  const uint8_t *msg,
  size_t msg_len,
  const bcrypto_rsa_key_t *pub,
  uint8_t **ct,
  size_t *ct_len
) {
  return false;
}

bool
bcrypto_rsa_decrypt(
  int type,
  const uint8_t *msg,
  size_t msg_len,
  const bcrypto_rsa_key_t *priv,
  uint8_t **pt,
  size_t *pt_len
) {
  return false;
}
#endif
