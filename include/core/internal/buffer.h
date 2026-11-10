#ifndef LOGGER_CORE_INTERNAL_BUFFER_H
#define LOGGER_CORE_INTERNAL_BUFFER_H

/*
 * INTERNAL: not part of the public API.
 *
 * Lock-free ring buffer carrying records from writer threads to the store's
 * I/O thread for asynchronous writes.
 *
 * It is single-producer / single-consumer: exactly one thread may push and
 * exactly one may pop. Callers with several producers must serialise pushes
 * themselves (store_priv.h gives each topic a lock for this).
 *
 * Records are variable length. Each is stored with a small length header, so
 * the capacity is in bytes, not records.
 */

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

/* Keep producer and consumer indices on separate cache lines. */
#define BUFFER_CACHE_LINE 64

typedef enum {
    RB_OK = 0,
    RB_EMPTY,        /* pop: nothing queued */
    RB_FULL,         /* push: not enough free space */
    RB_TOO_SMALL     /* pop: output buffer smaller than the record; *out_len is the size needed */
} rb_status_t;

typedef struct ring_buffer {
    uint8_t *data;
    size_t   capacity;                                  /* power of two, in bytes */
    size_t   mask;                                      /* capacity - 1 */

    _Alignas(BUFFER_CACHE_LINE) atomic_size_t head;     /* write index; owned by the producer */
    _Alignas(BUFFER_CACHE_LINE) atomic_size_t tail;     /* read index; owned by the consumer */
} ring_buffer_t;

/* Allocate a buffer of at least `capacity_hint` bytes (rounded up to a power of two). NULL on failure. */
ring_buffer_t *ring_buffer_create(size_t capacity_hint);

/* Free the buffer. Not thread-safe: stop the producer and consumer first. NULL is a no-op. */
void ring_buffer_destroy(ring_buffer_t *rb);

/* Producer: copy `len` bytes (len > 0) in as one record. RB_FULL if it does not fit. */
rb_status_t ring_buffer_push(ring_buffer_t *rb, const void *data, size_t len);

/*
 * Consumer: remove the oldest record into `out` (at most `out_cap` bytes) and
 * store its length in `*out_len`. On RB_TOO_SMALL the record stays queued.
 */
rb_status_t ring_buffer_pop(ring_buffer_t *rb, void *out, size_t out_cap,
                            size_t *out_len);

/* Bytes currently in use, including record headers. A snapshot; may be stale immediately. */
size_t ring_buffer_used(const ring_buffer_t *rb);

#endif /* LOGGER_CORE_INTERNAL_BUFFER_H */
