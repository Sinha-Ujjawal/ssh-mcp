#ifndef NOB_HT_H_
#define NOB_HT_H_

#ifndef NOB_ASSERT
#include <assert.h>
#define NOB_ASSERT assert
#endif // NOB_ASSERT

// References:
//  - https://en.wikipedia.org/wiki/Open_addressing

// Initial capacity of a dynamic hash table
#ifndef NOB_HT_INIT_CAP
#define NOB_HT_INIT_CAP 256 // Make sure this is power of 2
#endif
static_assert(((NOB_HT_INIT_CAP) & ((NOB_HT_INIT_CAP) - 1)) == 0, "NOB_HT_INIT_CAP must be power of 2!");

#ifndef NOB_HT_NOT_FOUND
#define NOB_HT_NOT_FOUND ((size_t)-1)
#endif

/* The hash table `ht` should be of the form:

struct {
    slot *items;
    size_t count;
    size_t capacity;
    ...
} ht;

where, slot is parameterized by KeyType and ValueType of the form:

struct {
    KeyType key;
    ValueType value;
    bool is_occupied;
    ...
} slot;

*/

#define nob_embed_ht_with_slot(T) \
    struct {                      \
        T *items;                 \
        size_t count;             \
        size_t capacity;          \
    }

#define nob_embed_ht_key_slot(K) \
    struct {                     \
        K key;                   \
        bool is_occupied;        \
    }

#define nob_embed_ht_kv_slot(K, V) \
    struct {                       \
        nob_embed_ht_key_slot(K);  \
        V value;                   \
    }

#define nob_embed_ht_with_key(K)   nob_embed_ht_with_slot(nob_embed_ht_key_slot(K))
#define nob_embed_ht_with_kv(K, V) nob_embed_ht_with_slot(nob_embed_ht_kv_slot(K, V))

#define nob__ht_find_slot(ht, hash_fn, is_eql_fn, key_expr, out)                                                           \
    do {                                                                                                                   \
        NOB_ASSERT(((ht).capacity > 0) && (((ht).capacity & ((ht).capacity - 1)) == 0) && "Capacity must be power of 2!"); \
        *(out) = hash_fn((key_expr)) & ((ht).capacity - 1);                                                                \
        while ((ht).items[*(out)].is_occupied && !is_eql_fn((ht).items[*(out)].key, (key_expr))) {                         \
            *(out) = (*(out) + 1) & ((ht).capacity - 1);                                                                   \
        }                                                                                                                  \
    } while(0)

#define nob_ht_get_key(ht, hash_fn, is_eql_fn, key_expr, out)                 \
    do {                                                                      \
        if ((ht)->count == 0) {                                               \
            *(out) = NOB_HT_NOT_FOUND;                                        \
            break;                                                            \
        }                                                                     \
        nob__ht_find_slot((*(ht)), hash_fn, is_eql_fn, (key_expr), (out));    \
        if (!(ht)->items[*(out)].is_occupied) { /* key is not in the table */ \
            *(out) = NOB_HT_NOT_FOUND;                                        \
        }                                                                     \
    } while(0)

#define nob__ht_resize(ht, hash_fn, is_eql_fn, new_capacity)                                                          \
    do {                                                                                                              \
        typeof((ht)->items) old_items = (ht)->items;                                                                  \
        size_t _nob__ht_resize_old_capacity = (ht)->capacity;                                                         \
        (ht)->items = malloc(sizeof(*(ht)->items) * (new_capacity));                                                  \
        memset((ht)->items, 0, sizeof(*(ht)->items) * (new_capacity));                                                \
        NOB_ASSERT((ht)->items != NULL && "Buy more RAM lol");                                                        \
        (ht)->capacity = (new_capacity);                                                                              \
        for (size_t _nob__ht_resize_i = 0; _nob__ht_resize_i < _nob__ht_resize_old_capacity; _nob__ht_resize_i++) {   \
            if (old_items[_nob__ht_resize_i].is_occupied) {                                                           \
                size_t _nob__ht_resize_j;                                                                             \
                nob__ht_find_slot((*(ht)), hash_fn, is_eql_fn, old_items[_nob__ht_resize_i].key, &_nob__ht_resize_j); \
                (ht)->items[_nob__ht_resize_j] = old_items[_nob__ht_resize_i];                                        \
            }                                                                                                         \
        }                                                                                                             \
        free(old_items);                                                                                              \
    } while(0)

#define nob_ht_reserve(ht, hash_fn, is_eql_fn, expected_capacity)                   \
    do {                                                                            \
        if ((expected_capacity) > (ht)->capacity) {                                 \
            size_t _nob_ht_reserve_new_capacity = (ht)->capacity;                   \
            if (_nob_ht_reserve_new_capacity == 0) {                                \
               _nob_ht_reserve_new_capacity  = NOB_HT_INIT_CAP;                     \
            }                                                                       \
            while ((expected_capacity) > _nob_ht_reserve_new_capacity) {            \
                _nob_ht_reserve_new_capacity *= 2;                                  \
            }                                                                       \
            nob__ht_resize((ht), hash_fn, is_eql_fn, _nob_ht_reserve_new_capacity); \
        }                                                                           \
    } while(0)

#define nob_ht_insert_key(ht, hash_fn, is_eql_fn, key_expr, out)          \
    do {                                                                  \
        if ((ht)->capacity == 0) {                                        \
            nob__ht_resize((ht), hash_fn, is_eql_fn, NOB_HT_INIT_CAP);    \
        }                                                                 \
        if ((ht)->count >= (ht)->capacity * 0.7) { /* resize at 70% */    \
            nob__ht_resize((ht), hash_fn, is_eql_fn, (ht)->capacity * 2); \
        }                                                                 \
        nob__ht_find_slot((*ht), hash_fn, is_eql_fn, (key_expr), (out));  \
        if ((ht)->items[*(out)].is_occupied) { /* key is in the table */  \
            break;                                                        \
        }                                                                 \
        (ht)->count++;                                                    \
        (ht)->items[*(out)].is_occupied = true;                           \
        (ht)->items[*(out)].key = (key_expr);                             \
    } while(0)

#define nob_ht_delete_key(ht, hash_fn, is_eql_fn, key_expr, out)                                                                                                       \
    do {                                                                                                                                                               \
        if ((ht)->count == 0) break;                                                                                                                                   \
        size_t _nob_ht_delete_key_i;                                                                                                                                   \
        nob__ht_find_slot((*(ht)), hash_fn, is_eql_fn, (key_expr), &_nob_ht_delete_key_i);                                                                             \
        if (!(ht)->items[_nob_ht_delete_key_i].is_occupied) break;                                                                                                     \
        (ht)->items[_nob_ht_delete_key_i].is_occupied = false;                                                                                                         \
        if ((out) != NULL) {                                                                                                                                           \
            *(out) = (ht)->items[_nob_ht_delete_key_i];                                                                                                                \
        }                                                                                                                                                              \
        size_t _nob_ht_delete_key_j = _nob_ht_delete_key_i;                                                                                                            \
        for (;;) {                                                                                                                                                     \
            _nob_ht_delete_key_j = (_nob_ht_delete_key_j + 1) & ((ht)->capacity - 1);                                                                                  \
            if (!(ht)->items[_nob_ht_delete_key_j].is_occupied) break;                                                                                                 \
            size_t _nob_ht_delete_key_k = hash_fn((ht)->items[_nob_ht_delete_key_i].key) & ((ht)->capacity - 1);                                                       \
            /* determine if k lies cyclically in (i,j] */                                                                                                              \
            /* i ≤ j: |    i..k..j    |                */                                                                                                              \
            /* i > j:    |.k..j     i....|             */                                                                                                              \
            /*        or |....j     i..k.|             */                                                                                                              \
            if (_nob_ht_delete_key_i <= _nob_ht_delete_key_j && _nob_ht_delete_key_i < _nob_ht_delete_key_k && _nob_ht_delete_key_k <= _nob_ht_delete_key_j) continue; \
            if (_nob_ht_delete_key_k <= _nob_ht_delete_key_j || _nob_ht_delete_key_i < _nob_ht_delete_key_k) continue;                                                 \
            (ht)->items[_nob_ht_delete_key_i].is_occupied = true;                                                                                                      \
            (ht)->items[_nob_ht_delete_key_i].key = (ht)->items[_nob_ht_delete_key_j].key;                                                                             \
            (ht)->items[_nob_ht_delete_key_i].value = (ht)->items[_nob_ht_delete_key_j].value;                                                                         \
            (ht)->items[_nob_ht_delete_key_j].is_occupied = false;                                                                                                     \
            _nob_ht_delete_key_i = _nob_ht_delete_key_j;                                                                                                               \
        }                                                                                                                                                              \
        (ht)->count -= 1;                                                                                                                                              \
        if ((ht)->capacity > NOB_HT_INIT_CAP && (ht)->count <= (ht)->capacity * 0.25) {                                                                                \
            nob__ht_resize((ht), hash_fn, is_eql_fn, (ht)->capacity * 0.5);                                                                                            \
        }                                                                                                                                                              \
    } while(0)

#define nob_ht_foreach(type, it, ht)                                      \
    for (type *it = (ht)->items; it < (ht)->items + (ht)->capacity; ++it) \
        if (it->is_occupied)

#endif // NOB_HT_H_

#ifndef NOB_HT_STRIP_PREFIX_GUARD_
#define NOB_HT_STRIP_PREFIX_GUARD_
    #ifndef NOB_UNSTRIP_PREFIX
        #define HT_INIT_CAP        NOB_HT_INIT_CAP
        #define HT_NOT_FOUND       NOB_HT_NOT_FOUND
        #define ht_reserve         nob_ht_reserve
        #define ht_get_key         nob_ht_get_key
        #define ht_insert_key      nob_ht_insert_key
        #define ht_delete_key      nob_ht_delete_key
        #define ht_foreach         nob_ht_foreach
        #define embed_ht_with_slot nob_embed_ht_with_slot
        #define embed_ht_key_slot  nob_embed_ht_key_slot
        #define embed_ht_kv_slot   nob_embed_ht_kv_slot
        #define embed_ht_with_key  nob_embed_ht_with_key
        #define embed_ht_with_kv   nob_embed_ht_with_kv
    #endif // NOB_UNSTRIP_PREFIX
#endif // NOB_HT_STRIP_PREFIX_GUARD_
