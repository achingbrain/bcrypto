/*!
 * mac.h - macs for libtorsion
 * Copyright (c) 2020, Christopher Jeffrey (MIT License).
 * https://github.com/bcoin-org/libtorsion
 */

#ifndef _TORSION_MAC_H
#define _TORSION_MAC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "common.h"

/*
 * Symbol Aliases
 */

#define poly1305_init torsion_poly1305_init
#define poly1305_update torsion_poly1305_update
#define poly1305_final torsion_poly1305_final
#define siphash_sum torsion_siphash_sum
#define siphash_mod torsion_siphash_mod
#define siphash128_sum torsion_siphash128_sum
#define siphash256_sum torsion_siphash256_sum

/*
 * Structs
 */

typedef struct poly1305_s {
  size_t aligner;
  unsigned char opaque[136];
} poly1305_t;

/*
 * Poly1305
 */

TORSION_EXTERN void
poly1305_init(poly1305_t *ctx, const unsigned char *key);

TORSION_EXTERN void
poly1305_update(poly1305_t *ctx, const unsigned char *data, size_t len);

TORSION_EXTERN void
poly1305_final(poly1305_t *ctx, unsigned char *mac);

/*
 * Siphash
 */

TORSION_EXTERN uint64_t
siphash_sum(const unsigned char *data, size_t len, const unsigned char *key);

TORSION_EXTERN uint64_t
siphash_mod(const unsigned char *data,
            size_t len,
            const unsigned char *key,
            uint64_t mod);

TORSION_EXTERN uint64_t
siphash128_sum(uint64_t num, const unsigned char *key);

TORSION_EXTERN uint64_t
siphash256_sum(uint64_t num, const unsigned char *key);

#ifdef __cplusplus
}
#endif

#endif /* _TORSION_MAC_H */
