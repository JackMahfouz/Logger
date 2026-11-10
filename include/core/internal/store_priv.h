#ifndef LOGGER_CORE_INTERNAL_STORE_PRIV_H
#define LOGGER_CORE_INTERNAL_STORE_PRIV_H

/*
 * INTERNAL: not part of the public API.
 *
 * Internal store state: the full definitions of the opaque store_t and topic_t
 * types, their locks, and the hash map that indexes topics by name.
 *
 * Lock order, outermost first: store->topics_lock, then topic->lock. Never
 * take them in the other order.
 *
 * Needs POSIX threads (pthread_rwlock_t). That works under -std=gnu11, which
 * is what CMake uses by default; under strict -std=c11, define
 * _POSIX_C_SOURCE=200112L (or later) before including any header.
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/internal/buffer.h"
#include "core/store/store.h"
#include "core/store/topic.h"

struct topic {
    char     name[TOPIC_NAME_MAX];
    uint64_t retention_bytes;
    uint32_t retention_seconds;

    atomic_uint        refcount;       /* the store's own reference counts as one */
    atomic_uint_fast64_t next_seq;     /* sequence number given to the next record */
    atomic_uint_fast64_t records_written;
    atomic_uint_fast64_t bytes_written;
    atomic_uint_fast64_t records_dropped;

    ring_buffer_t   *queue;            /* writers -> I/O thread */
    pthread_mutex_t  lock;             /* serialises producers on `queue`; guards `fd` */
    int              fd;               /* open data file */

    uint32_t       hash;               /* cached hash of `name` */
    struct topic  *hash_next;          /* next topic in the same bucket */
};

/* Chained hash map from topic name to topic. Not internally synchronised. */
typedef struct {
    struct topic **buckets;
    size_t         nbuckets;           /* power of two */
    size_t         count;
} topic_map_t;

struct store {
    char  *root_dir;                   /* owned copy of config->root_dir */
    size_t max_topics;
    size_t buffer_bytes;

    pthread_rwlock_t topics_lock;      /* guards `topics` */
    topic_map_t      topics;

    pthread_t   io_thread;             /* drains every topic's queue to disk */
    atomic_bool shutting_down;
};

/* Hash a topic name (FNV-1a). */
uint32_t topic_name_hash(const char *name);

/* Set up an empty map with at least `nbuckets` buckets. Returns false on allocation failure. */
bool topic_map_init(topic_map_t *map, size_t nbuckets);

/* Free the buckets. Does not free the topics themselves. */
void topic_map_destroy(topic_map_t *map);

/* Find a topic by name, or NULL. Does not take a reference. */
struct topic *topic_map_get(const topic_map_t *map, const char *name);

/* Insert `topic`. Returns false if the name is taken or on allocation failure. */
bool topic_map_put(topic_map_t *map, struct topic *topic);

/* Unlink and return the topic named `name`, or NULL. The caller owns the store's reference. */
struct topic *topic_map_remove(topic_map_t *map, const char *name);

#endif /* LOGGER_CORE_INTERNAL_STORE_PRIV_H */
