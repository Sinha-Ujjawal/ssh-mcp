#ifndef NOB_FIXED_DEQUE_H_
#define NOB_FIXED_DEQUE_H_

#ifndef NOB_ASSERT
#include <assert.h>
#define NOB_ASSERT assert
#endif // NOB_ASSERT

#include <string.h>

/* The fixed_deque `dq` is parameterized by T, and SIZE and should be of the form:

struct {
    T items[SIZE];
    size_t begin;
    size_t count;
    ...
} fixed_deque;

*/

#define nob_embed_fixed_deque(T, capacity) \
    struct {                               \
        T items[capacity];                 \
        size_t begin;                      \
        size_t count;                      \
    }

// Append an item to a fixed queue
#define nob_fixed_deque_append(dq, item)                                                               \
    do {                                                                                               \
        NOB_ASSERT(((dq)->count < NOB_ARRAY_LEN((dq)->items)) && "Cannot append to full fixed_deque"); \
        (dq)->items[((dq)->begin + (dq)->count) % NOB_ARRAY_LEN((dq)->items)] = (item);                \
        (dq)->count++;                                                                                 \
    } while (0)

// Pop an item from the end in a fixed queue
#define nob_fixed_deque_pop(dq, result)                                                    \
    do {                                                                                   \
        NOB_ASSERT(((dq)->count > 0) && "Cannot pop from empty fixed_deque!");             \
        (dq)->count -= 1;                                                                  \
        *(result) = (dq)->items[((dq)->begin + (dq)->count) % NOB_ARRAY_LEN((dq)->items)]; \
    } while(0)

// Peek an item at the end in a fixed queue
#define nob_fixed_deque_last(dq) \
    (dq)->items[NOB_ASSERT(((dq)->count > 0) && "Cannot peek into empty fixed_deque!"), ((dq)->begin + (dq)->count - 1) % NOB_ARRAY_LEN((dq)->items)];

// Prepends an item to a fixed queue
#define nob_fixed_deque_prepend(dq, item)                                                               \
    do {                                                                                                \
        NOB_ASSERT(((dq)->count < NOB_ARRAY_LEN((dq)->items)) && "Cannot prepend to full fixed_deque"); \
        if ((dq)->begin == 0) {                                                                         \
            (dq)->begin = NOB_ARRAY_LEN((dq)->items);                                                   \
        }                                                                                               \
        (dq)->begin -= 1;                                                                               \
        (dq)->items[(dq)->begin] = (item);                                                              \
        (dq)->count++;                                                                                  \
    } while (0)

// Pop an item from the beginning in a fixed queue
#define nob_fixed_deque_shift(dq, result)                                      \
    do {                                                                       \
        NOB_ASSERT(((dq)->count > 0) && "Cannot pop from empty fixed_deque!"); \
        (dq)->count -= 1;                                                      \
        *(result) = (dq)->items[(dq)->begin];                                  \
        (dq)->begin = ((dq)->begin + 1) % NOB_ARRAY_LEN((dq)->items);          \
    } while(0)

// Peek an item at the beginning in a fixed queue
#define nob_fixed_deque_first(dq) \
    (dq)->items[NOB_ASSERT(((dq)->count > 0) && "Cannot peek into empty fixed_deque!"), ((dq)->begin) % NOB_ARRAY_LEN((dq)->items)];

#define nob_fixed_deque_foreach(type, it, dq) \
    for (size_t i = 0 ; i < (dq)->count; i++) \
        for (type *it = &(dq)->items[((dq)->begin + i) % NOB_ARRAY_LEN((dq)->items)]; it != NULL; it = NULL)

#endif // NOB_FIXED_DEQUE_H_

#ifndef NOB_FIXED_DEQUE_STRIP_PREFIX_GUARD_
#define NOB_FIXED_DEQUE_STRIP_PREFIX_GUARD_
    #ifndef NOB_UNSTRIP_PREFIX
        #define fixed_deque_reserve  nob_fixed_deque_reserve
        #define fixed_deque_append   nob_fixed_deque_append
        #define fixed_deque_pop      nob_fixed_deque_pop
        #define fixed_deque_last     nob_fixed_deque_last
        #define fixed_deque_prepend  nob_fixed_deque_prepend
        #define fixed_deque_shift    nob_fixed_deque_shift
        #define fixed_deque_first    nob_fixed_deque_first
        #define fixed_deque_foreach  nob_fixed_deque_foreach
        #define embed_fixed_deque    nob_embed_fixed_deque
    #endif // NOB_UNSTRIP_PREFIX
#endif // NOB_FIXED_DEQUE_STRIP_PREFIX_GUARD_
