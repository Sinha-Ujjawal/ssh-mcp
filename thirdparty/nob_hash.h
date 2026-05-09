#ifndef NOB_HASH_H_
#define NOB_HASH_H_

#define NOB_HASH_DEFAULT_MIX 69420

size_t nob_hash_uint(unsigned int x);
size_t nob_hash_int(int x);
size_t nob_hash_char(char c);
size_t nob_hash_long(long x);
size_t nob_hash_float(float x);
size_t nob_hash_bytes(const void *data, size_t size);

#define nob_hash_struct(x) nob_hash_bytes(&(x), sizeof(x))
#define nob_hash_cstr(x)   nob_hash_bytes(x, strlen(x))
#define nob_hash_sv(x)     nob_hash_bytes((x).data, ((x).count))

size_t nob_mix_hash(size_t h, size_t seed);
size_t nob_mix_hash_many(const size_t *hs, size_t hs_count, size_t seed);

#define nob_combine_hash(seed, n, ...) \
    nob_mix_hash_many((const size_t[]){ __VA_ARGS__ }, (n), (seed))

#define nob_defn_seeded_hash_fn(FN_NAME, T, hash_fn, seed) \
    size_t FN_NAME(T x) {                                  \
        return nob_mix_hash(hash_fn(x), seed);             \
    }

#endif // NOB_HASH_H_

#ifdef NOB_HASH_IMPLEMENTATION
#ifndef NOB_HASH_IMPLEMENTATION_GAURD_
#define NOB_HASH_IMPLEMENTATION_GAURD_

#include <stdint.h>
#include <string.h>

// https://stackoverflow.com/a/12996028
size_t nob_hash_uint(unsigned int x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return nob_mix_hash(x, NOB_HASH_DEFAULT_MIX);
}

size_t nob_hash_int(int x) {
    if (x < 0) {
        x *= -1;
        x <<= 1;
        x++;
        return nob_hash_uint(x);
    }
    return nob_hash_uint(x <<= 1);
}

size_t nob_hash_char(char c) {
    return nob_hash_int((int) c);
}

size_t nob_hash_long(long x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9L;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebL;
    x = x ^ (x >> 31);
    return nob_mix_hash(x, NOB_HASH_DEFAULT_MIX);
}

size_t nob_hash_float(float x) {
    unsigned int bits;
    memcpy(&bits, &x, sizeof(float)); // Interpret the float as an integer
    return nob_hash_uint(bits); // Reuse the unsigned int hash function
}

// djb2 hash
size_t nob_hash_bytes(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t hash = 5381;
    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + bytes[i]; // hash * 33 + current byte
    }
    return nob_mix_hash(hash, NOB_HASH_DEFAULT_MIX);
}

size_t nob_mix_hash(size_t h, size_t seed) {
    h ^= seed;
    h ^= h >> 33;
    h *= (size_t) 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= (size_t) 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

size_t nob_mix_hash_many(const size_t *hs, size_t hs_count, size_t seed) {
    size_t h = seed;
    for (size_t i = 0; i < hs_count; i++) {
        h = nob_mix_hash(hs[i], h);
    }
    return h;
}

#endif // NOB_HASH_IMPLEMENTATION_GAURD_
#endif // NOB_HASH_IMPLEMENTATION

#ifndef NOB_HASH_STRIP_PREFIX_GUARD_
#define NOB_HASH_STRIP_PREFIX_GUARD_
    #ifndef NOB_UNSTRIP_PREFIX
        #define hash_uint           nob_hash_uint
        #define hash_int            nob_hash_int
        #define hash_char           nob_hash_char
        #define hash_long           nob_hash_long
        #define hash_float          nob_hash_float
        #define hash_bytes          nob_hash_bytes
        #define hash_struct         nob_hash_struct
        #define hash_cstr           nob_hash_cstr
        #define hash_sv             nob_hash_sv
        #define mix_hash            nob_mix_hash
        #define mix_hash_many       nob_mix_hash_many
        #define combine_hash        nob_combine_hash
        #define defn_seeded_hash_fn nob_defn_seeded_hash_fn
        #define HASH_DEFAULT_MIX    NOB_HASH_DEFAULT_MIX
    #endif // NOB_UNSTRIP_PREFIX
#endif // NOB_HASH_STRIP_PREFIX_GUARD_
