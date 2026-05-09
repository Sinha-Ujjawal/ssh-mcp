#ifndef NOB_CHANNELS_H_
#define NOB_CHANNELS_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*
 * Required dependencies:
 * - nob_fixed_deque.h (for both operations and data buffer)
 *
 * References-
 * - Inside core.async Channels" by Rich Hickey (2014) - https://youtu.be/hMEX6lfBeRM?si=W8fw02_NV17a7k-w
 */


#ifndef NOB_CHANNELS_MAX_WRITE_QUEUE_SIZE
#define NOB_CHANNELS_MAX_WRITE_QUEUE_SIZE 1024
#endif // NOB_CHANNELS_MAX_WRITE_QUEUE_SIZE

#ifndef NOB_CHANNELS_MAX_READ_QUEUE_SIZE
#define NOB_CHANNELS_MAX_READ_QUEUE_SIZE 1024
#endif // NOB_CHANNELS_MAX_READ_QUEUE_SIZE

#define nob_embed_channel(T, size)                   \
    struct {                                         \
        pthread_mutex_t lock;                        \
        bool is_closed;                              \
                                                     \
        /* Write Queue (Pending Producers) */        \
        nob_embed_fixed_deque(struct {               \
            pthread_cond_t *cond;                    \
            T data;                                  \
        }, NOB_CHANNELS_MAX_WRITE_QUEUE_SIZE) sends; \
                                                     \
        /* Data Buffer (Asynchronous room) */        \
        nob_embed_fixed_deque(T, size) buffer;       \
                                                     \
        /* Read Queue (Pending Consumers) */         \
        nob_embed_fixed_deque(struct {               \
            pthread_cond_t *cond;                    \
        }, NOB_CHANNELS_MAX_READ_QUEUE_SIZE) recvs;  \
   }

#define nob_channel_init(chan) do {          \
    (chan)->sends.begin = 0;                 \
    (chan)->sends.count = 0;                 \
                                             \
    (chan)->buffer.begin = 0;                \
    (chan)->buffer.count = 0;                \
                                             \
    (chan)->recvs.begin = 0;                 \
    (chan)->recvs.count = 0;                 \
                                             \
    pthread_mutex_init(&(chan)->lock, NULL); \
    (chan)->is_closed = false;               \
} while (0)

/* -------------------------------------------------------------------------
 * Internal: try to send to a locked channel.
 * Sets fired=true when data was successfully send on the channel
 * ------------------------------------------------------------------------- */
#define nob_channel__try_send(chan, val, fired) do {                                     \
    (fired) = false;                                                                     \
    NOB_ASSERT(!(chan)->is_closed && "Send to closed channel");                          \
    bool _room_in_buffer = ((chan)->buffer.count < NOB_ARRAY_LEN((chan)->buffer.items)); \
    bool _recvs_present = (chan)->recvs.count > 0;                                       \
                                                                                         \
    if (_recvs_present && _room_in_buffer) {                                             \
        /* 1. Rendezvous: Pass data directly to waiting receiver */                      \
        /* Push to buffer and wake the receiver to grab it. */                           \
        nob_fixed_deque_append(&(chan)->buffer, (val));                                  \
        typeof((chan)->recvs.items[0]) _receiver;                                        \
        nob_fixed_deque_shift(&(chan)->recvs, &_receiver);                               \
        pthread_cond_signal(_receiver.cond);                                             \
        (fired) = true;                                                                  \
    } else if (_room_in_buffer) {                                                        \
        /* 2. Buffer: Room exists in the fixed deque */                                  \
        nob_fixed_deque_append(&(chan)->buffer, (val));                                  \
        (fired) = true;                                                                  \
    }                                                                                    \
} while(0)

// Send a value to the channel
#define nob_channel_send(chan, val) do {                                            \
    pthread_mutex_lock(&(chan)->lock);                                              \
    bool _fired = false;                                                            \
    nob_channel__try_send((chan), (val), (_fired));                                 \
    if (!_fired) {                                                                  \
        /* Block: Enqueue producer and wait */                                      \
        /* Cond lives on the stack for its entire wait; the pointer in the deque */ \
        /* item stays valid because we don't destroy it until after we wake and  */ \
        /* the receiver has already shifted our entry out.                       */ \
        pthread_cond_t _sender_cond;                                                \
        pthread_cond_init(&_sender_cond, NULL);                                     \
        typeof((chan)->sends.items[0]) _sender = {                                  \
            .cond = &_sender_cond,                                                  \
            .data = (val),                                                          \
        };                                                                          \
        nob_fixed_deque_append(&(chan)->sends, _sender);                            \
        /* pthread_cond_wait atomically unlocks the mutex while sleeping */         \
        pthread_cond_wait(&_sender_cond, &(chan)->lock);                            \
        pthread_cond_destroy(&_sender_cond);                                        \
        /* Re-check: we may have been woken because the channel was closed. */      \
        NOB_ASSERT(!(chan)->is_closed && "Woken by channel close during send");     \
    }                                                                               \
    pthread_mutex_unlock(&(chan)->lock);                                            \
} while (0)

/* -------------------------------------------------------------------------
 * Internal: try to receive from a locked channel.
 * Sets fired=true and populates res/ok if data was available or closed.
 * ------------------------------------------------------------------------- */
#define nob_channel__try_recv(chan, res_ptr, ok_ptr, fired) do { \
    (fired) = false;                                             \
    if ((chan)->buffer.count > 0) {                              \
        /* 1. Drain the buffer first */                          \
        nob_fixed_deque_shift(&(chan)->buffer, (res_ptr));       \
        *(ok_ptr) = true;                                        \
        (fired) = true;                                          \
        if ((chan)->sends.count > 0) {                           \
            typeof((chan)->sends.items[0]) _w;                   \
            nob_fixed_deque_shift(&(chan)->sends, &_w);          \
            nob_fixed_deque_append(&(chan)->buffer, _w.data);    \
            pthread_cond_signal(_w.cond);                        \
        }                                                        \
    } else if ((chan)->sends.count > 0) {                        \
        /* 2. Check for pending senders (Rendezvous) */          \
        typeof((chan)->sends.items[0]) _w;                       \
        nob_fixed_deque_shift(&(chan)->sends, &_w);              \
        *(res_ptr) = _w.data;                                    \
        *(ok_ptr) = true;                                        \
        pthread_cond_signal(_w.cond);                            \
        (fired) = true;                                          \
    } else if ((chan)->is_closed) {                              \
        /* 3. Handle Closure */                                  \
        *(ok_ptr) = false;                                       \
        (fired) = true;                                          \
    }                                                            \
} while (0)

// Receive a value from the channel
#define nob_channel_recv(chan, res_ptr, ok_ptr) do {                                \
    pthread_mutex_lock(&(chan)->lock);                                              \
    /* Hoist the cond outside the loop so its address stays stable across     */    \
    /* iterations. A spurious wakeup would otherwise destroy and recreate it, */    \
    /* leaving the sender holding a dangling pointer → segfault.              */    \
    pthread_cond_t _receiver_cond;                                                  \
    pthread_cond_init(&_receiver_cond, NULL);                                       \
    bool _enqueued = false;                                                         \
    while (true) {                                                                  \
        bool _fired = false;                                                        \
        nob_channel__try_recv((chan), (res_ptr), (ok_ptr), (_fired));               \
        if (_fired) break;                                                          \
        /* Block: enqueue once (reuse same slot & cond on spurious wakeup) */       \
        if (!_enqueued) {                                                           \
            typeof((chan)->recvs.items[0]) _receiver = { .cond = &_receiver_cond }; \
            nob_fixed_deque_append(&(chan)->recvs, _receiver);                      \
            _enqueued = true;                                                       \
        }                                                                           \
        pthread_cond_wait(&_receiver_cond, &(chan)->lock);                          \
        _enqueued = false;                                                          \
        /* Loop back to re-evaluate: buffer, senders, or closed. */                 \
    }                                                                               \
    pthread_cond_destroy(&_receiver_cond);                                          \
    pthread_mutex_unlock(&(chan)->lock);                                            \
} while (0)

// Close the channel and wake up all pending receivers and senders.
#define nob_channel_close(chan) do {                                              \
    pthread_mutex_lock(&(chan)->lock);                                            \
    (chan)->is_closed = true;                                                     \
    /* Signal all receivers so they can exit the while(true) loop */              \
    nob_fixed_deque_foreach(typeof((chan)->recvs.items[0]), _r, &(chan)->recvs) { \
        pthread_cond_signal(_r->cond);                                            \
    }                                                                             \
    /* Signal all blocked senders so they can observe is_closed */                \
    nob_fixed_deque_foreach(typeof((chan)->sends.items[0]), _w, &(chan)->sends) { \
        pthread_cond_signal(_w->cond);                                            \
    }                                                                             \
    pthread_mutex_unlock(&(chan)->lock);                                          \
} while (0)

/*
 * nob_channel_alt — Go-style select over N channels via X-macros.
 *
 * -------------------------------------------------------------------------
 * Usage:
 *
 *   #define MY_ARMS(Send, Recv)       \
 *       Recv(&chan_a, &val_a, &ok_a)  \
 *       Recv(&chan_b, &val_b, &ok_b)  \
 *       Recv(&chan_c, &val_c, &ok_c)  \
 *       Send(&chan_d, val_d)
 *
 *   int which;
 *   nob_channel_alt(which, MY_ARMS);
 *   #undef MY_ARMS
 *
 *   if (which == 0) { ... } // chan_a fired
 *   if (which == 1) { ... } // chan_b fired
 *
 * -------------------------------------------------------------------------
 * No goto labels are used — the macro is safe to call multiple times in
 * the same function. Control flow uses only do/while, for, break, and
 * a _alt_done flag to skip the slow path when the fast path succeeds.
 *
 * -------------------------------------------------------------------------
 * How it works:
 *
 *   1. Count arms, collect+sort channel pointers, lock all in address order.
 *   2. Fast path: check each channel; try sending/receiving, if succeeded then return
 *   3. Slow path (only if fast path missed): enqueue a receiver with a SHARED
 *      cond into every channel, release all locks except the lowest, wait.
 *   4. On wake, reacquire all locks, find which channel fired.
 *   5. Remove our cond from every non-winning channel's queue.
 *   6. Unlock all in reverse order.
 *
 * -------------------------------------------------------------------------
 * Mixed-type channels:
 *   Nob_Channel__Alt_Any_Chan_Ptr casts void* to reach ->lock. Safe only when all
 *   channels in one alt call share the same concrete type (same layout).
 *   See note near Nob_Channel__Alt_Any_Chan_Ptr for the mixed-type workaround.
 */

/* -------------------------------------------------------------------------
 * Internal: remove the recvs-queue entry whose .cond == target_cond.
 * Linearizes the ring buffer after removal (begin = 0).
 * ------------------------------------------------------------------------- */

#define nob_channel__remove_target_cond_from_queue(dq, target_cond) do {   \
    size_t _cap = NOB_ARRAY_LEN((dq)->items);                              \
    size_t _new_count = 0;                                                 \
    typeof((dq)->items[0]) _tmp[NOB_ARRAY_LEN((dq)->items)];               \
    for (size_t _i = 0; _i < (dq)->count; _i++) {                          \
        typeof((dq)->items[0]) *_slot =                                    \
            &(dq)->items[((dq)->begin + _i) % _cap];                       \
        if (_slot->cond != (target_cond)) _tmp[_new_count++] = *_slot;     \
    }                                                                      \
    for (size_t _i = 0; _i < _new_count; _i++) (dq)->items[_i] = _tmp[_i]; \
    (dq)->begin = 0;                                                       \
    (dq)->count = _new_count;                                              \
} while (0)

/* -------------------------------------------------------------------------
 * Nob_Channel__Alt_Any_Chan_Ptr — erased pointer type used only for lock/unlock
 * in the sorted-locks loop. Safe for channels having same layout only.
 * This can be gaurenteed if you use nob_embed_channel for creating channel types
 * ------------------------------------------------------------------------- */
typedef struct {
    pthread_mutex_t lock;
    bool is_closed;
} *Nob_Channel__Alt_Any_Chan_Ptr;

/* -------------------------------------------------------------------------
 * Per-arm X expanders
 * ------------------------------------------------------------------------- */

/* +1 per arm for the count expression */
#define nob_channel__alt_count_arm(chan, ...)  + 1

/* Store chan pointer into _alt_chans[_alt_i++] */
#define nob_channel__alt_collect_chan(chan, ...) \
    _alt_chans[_alt_i++] = (void*)(chan);

/* Try immediate send; set _which_out if it fires, skip further arms */
#define nob_channel__alt_try_send(chan, val)              \
    if (_which_out < 0) {                                 \
        nob_channel__try_send((chan), (val), _alt_fired); \
        if (_alt_fired) _which_out = _alt_i;              \
    }                                                     \
    _alt_i++;

/* Try immediate receive; set _which_out if it fires, skip further arms */
#define nob_channel__alt_try_recv(chan, res, ok)                \
    if (_which_out < 0) {                                       \
        nob_channel__try_recv((chan), (res), (ok), _alt_fired); \
        if (_alt_fired) _which_out = _alt_i;                    \
    }                                                           \
    _alt_i++;

/* Enqueue a sender entry with the shared cond into this arm's channel */
#define nob_channel__alt_enqueue_for_sends(chan, val) {                          \
    typeof((chan)->sends.items[0]) _alt_w = { .cond = &_alt_cond, .data = val }; \
    nob_fixed_deque_append(&(chan)->sends, _alt_w);                              \
}

/* Enqueue a receiver entry with the shared cond into this arm's channel */
#define nob_channel__alt_enqueue_for_recvs(chan, res, ok) {         \
    typeof((chan)->recvs.items[0]) _alt_r = { .cond = &_alt_cond }; \
    nob_fixed_deque_append(&(chan)->recvs, _alt_r);                 \
}

/*
 * Re-check a send arm after waking from cond_wait.
 *
 * We cannot simply call nob_channel__alt_try_send here because our entry
 * is still in (chan)->sends from the enqueue step. If try_send succeeded
 * it would push the value into the buffer AND our queued entry would remain
 * as a ghost — causing a double-send or clogging sends for future callers.
 *
 * Fix: remove our entry first, then try cleanly. If the send still can't
 * fire (no room, no waiting receiver), re-enqueue so we remain registered
 * and will be woken again when space opens up.
 */
#define nob_channel__alt_recheck_send(chan, val)                                \
    if (_which_out < 0) {                                                       \
        nob_channel__remove_target_cond_from_queue(&(chan)->sends, &_alt_cond); \
        nob_channel__try_send((chan), (val), _alt_fired);                       \
        if (_alt_fired) {                                                       \
            _which_out = _alt_i;                                                \
        } else {                                                                \
            /* Still no room — re-enqueue so we stay registered */              \
            typeof((chan)->sends.items[0]) _alt_s = {                           \
                .cond = &_alt_cond, .data = (val)                               \
            };                                                                  \
            nob_fixed_deque_append(&(chan)->sends, _alt_s);                     \
        }                                                                       \
    }                                                                           \
    _alt_i++;

/* Remove our shared cond from channels that did not fire (For Sends) */
#define nob_channel__alt_cleanup_for_sends(chan, ...)                           \
    if (_alt_i != _which_out) {                                                 \
        nob_channel__remove_target_cond_from_queue(&(chan)->sends, &_alt_cond); \
    }                                                                           \
    _alt_i++;

/* Remove our shared cond from channels that did not fire (For Sends) */
#define nob_channel__alt_cleanup_for_recvs(chan, ...)                           \
    if (_alt_i != _which_out) {                                                 \
        nob_channel__remove_target_cond_from_queue(&(chan)->recvs, &_alt_cond); \
    }                                                                           \
    _alt_i++;

/* -------------------------------------------------------------------------
 * Lock/unlock helpers (deduplicated, sorted order)
 * ------------------------------------------------------------------------- */
#define nob_channel__alt_lock_all(_alt_chans, _alt_n) do {                  \
    void *_prev = NULL;                                                     \
    for (int _li = 0; _li < (_alt_n); _li++) {                              \
        if ((_alt_chans)[_li] != _prev) {                                   \
            pthread_mutex_lock(                                             \
                &((Nob_Channel__Alt_Any_Chan_Ptr)(_alt_chans)[_li])->lock); \
            _prev = (_alt_chans)[_li];                                      \
        }                                                                   \
    }                                                                       \
} while(0)

#define nob_channel__alt_unlock_all(_alt_chans, _alt_n) do {                \
    void *_prev = NULL;                                                     \
    for (int _li = (_alt_n) - 1; _li >= 0; _li--) {                         \
        if ((_alt_chans)[_li] != _prev) {                                   \
            pthread_mutex_unlock(                                           \
                &((Nob_Channel__Alt_Any_Chan_Ptr)(_alt_chans)[_li])->lock); \
            _prev = (_alt_chans)[_li];                                      \
        }                                                                   \
    }                                                                       \
} while(0)

/* Release all locks except [0] (kept for cond_wait) */
#define nob_channel__alt_unlock_except_first(_alt_chans, _alt_n) do {       \
    void *_prev = (_alt_chans)[0];                                          \
    for (int _li = (_alt_n) - 1; _li >= 1; _li--) {                         \
        if ((_alt_chans)[_li] != _prev) {                                   \
            pthread_mutex_unlock(                                           \
                &((Nob_Channel__Alt_Any_Chan_Ptr)(_alt_chans)[_li])->lock); \
            _prev = (_alt_chans)[_li];                                      \
        }                                                                   \
    }                                                                       \
} while(0)

/* Reacquire all locks except [0] (already held after cond_wait) */
#define nob_channel__alt_lock_except_first(_alt_chans, _alt_n) do {         \
    void *_prev = (_alt_chans)[0];                                          \
    for (int _li = 1; _li < (_alt_n); _li++) {                              \
        if ((_alt_chans)[_li] != _prev) {                                   \
            pthread_mutex_lock(                                             \
                &((Nob_Channel__Alt_Any_Chan_Ptr)(_alt_chans)[_li])->lock); \
            _prev = (_alt_chans)[_li];                                      \
        }                                                                   \
    }                                                                       \
} while(0)

#define nob_channel_alt(which_out, ARMS) do {                                            \
    /* Count arms: 0 + 1 + 1 + ... */                                                    \
    const int _alt_n = 0 ARMS(nob_channel__alt_count_arm, nob_channel__alt_count_arm);   \
                                                                                         \
    /* Collect channel pointers */                                                       \
    void *_alt_chans[_alt_n];                                                            \
    {                                                                                    \
        int _alt_i = 0;                                                                  \
        ARMS(nob_channel__alt_collect_chan, nob_channel__alt_collect_chan)               \
    }                                                                                    \
                                                                                         \
    /* Sort ascending by address (insertion sort — N is tiny) */                         \
    for (int _si = 1; _si < _alt_n; _si++) {                                             \
        void *_sk = _alt_chans[_si];                                                     \
        int _sj = _si - 1;                                                               \
        while (_sj >= 0 && _alt_chans[_sj] > _sk) {                                      \
            _alt_chans[_sj + 1] = _alt_chans[_sj];                                       \
            _sj--;                                                                       \
        }                                                                                \
        _alt_chans[_sj + 1] = _sk;                                                       \
    }                                                                                    \
                                                                                         \
    nob_channel__alt_lock_all(_alt_chans, _alt_n);                                       \
                                                                                         \
    /* Fast path */                                                                      \
    bool _alt_fired = false;                                                             \
    bool _alt_done  = false;                                                             \
    (which_out) = -1;                                                                    \
    {                                                                                    \
        int _alt_i = 0;                                                                  \
        int _which_out = -1;                                                             \
        ARMS(nob_channel__alt_try_send, nob_channel__alt_try_recv)                       \
        (which_out) = _which_out;                                                        \
    }                                                                                    \
    if ((which_out) >= 0) _alt_done = true;                                              \
                                                                                         \
    /* Slow path — only entered when fast path found nothing */                          \
    pthread_cond_t _alt_cond;                                                            \
    if (!_alt_done) {                                                                    \
        pthread_cond_init(&_alt_cond, NULL);                                             \
        { ARMS(nob_channel__alt_enqueue_for_sends, nob_channel__alt_enqueue_for_recvs) } \
                                                                                         \
        nob_channel__alt_unlock_except_first(_alt_chans, _alt_n);                        \
                                                                                         \
        /* Wait loop — handles spurious wakeups */                                       \
        while ((which_out) < 0) {                                                        \
            pthread_cond_wait(&_alt_cond,                                                \
                &((Nob_Channel__Alt_Any_Chan_Ptr)_alt_chans[0])->lock);                  \
            nob_channel__alt_lock_except_first(_alt_chans, _alt_n);                      \
            {                                                                            \
                int _alt_i = 0;                                                          \
                int _which_out = -1;                                                     \
                ARMS(nob_channel__alt_recheck_send, nob_channel__alt_try_recv)           \
                (which_out) = _which_out;                                                \
            }                                                                            \
            /* Spurious wakeup: release extras and go back to wait */                    \
            if ((which_out) < 0) {                                                       \
                nob_channel__alt_unlock_except_first(_alt_chans, _alt_n);                \
            }                                                                            \
        }                                                                                \
                                                                                         \
        /* Cleanup: remove our cond from non-winning channels */                         \
        {                                                                                \
            int _alt_i = 0;                                                              \
            int _which_out = (which_out);                                                \
            ARMS(nob_channel__alt_cleanup_for_sends, nob_channel__alt_cleanup_for_recvs) \
        }                                                                                \
        pthread_cond_destroy(&_alt_cond);                                                \
    }                                                                                    \
                                                                                         \
    nob_channel__alt_unlock_all(_alt_chans, _alt_n);                                     \
} while (0)

#endif // NOB_CHANNELS_H_

#ifndef NOB_CHANNELS_STRIP_PREFIX_GUARD_
#define NOB_CHANNELS_STRIP_PREFIX_GUARD_
    #ifndef NOB_UNSTRIP_PREFIX
        #define embed_channel   nob_embed_channel
        #define channel_init    nob_channel_init
        #define channel_send    nob_channel_send
        #define channel_recv    nob_channel_recv
        #define channel_close   nob_channel_close
        #define channel_alt     nob_channel_alt
    #endif // NOB_UNSTRIP_PREFIX
#endif // NOB_CHANNELS_STRIP_PREFIX_GUARD_
