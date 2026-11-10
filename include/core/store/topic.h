#ifndef LOGGER_CORE_STORE_TOPIC_H
#define LOGGER_CORE_STORE_TOPIC_H

/*
 * Topic definition and topic handle lifecycle.
 *
 * A topic is a named, append-only stream of JSON log records. Topics are
 * created and looked up through the store (see store.h); this header covers
 * what a caller can do with a topic once it holds a handle.
 */

#include <stdint.h>

/* Longest topic name, including the terminating NUL. */
#define TOPIC_NAME_MAX 64

/* Opaque topic handle. The layout lives in internal/store_priv.h. */
typedef struct topic topic_t;

/* Parameters for creating a topic. */
typedef struct {
    const char *name;           /* [A-Za-z0-9._-], shorter than TOPIC_NAME_MAX */
    uint64_t    retention_bytes;   /* drop oldest data past this size; 0 = unlimited */
    uint32_t    retention_seconds; /* drop data older than this; 0 = unlimited */
} topic_config_t;

/* Point-in-time counters for a topic. */
typedef struct {
    uint64_t records_written;
    uint64_t bytes_written;
    uint64_t records_dropped;   /* rejected because the async buffer was full */
} topic_stats_t;

/*
 * Handle lifecycle. Handles are reference counted: the store hands out a
 * handle already retained once for the caller, and every handle must be
 * released exactly once. Writers and readers take their own reference, so a
 * topic outlives a caller that releases it while a writer is still open.
 */

/* Take an additional reference. Returns `topic` for call chaining. */
topic_t *topic_retain(topic_t *topic);

/* Drop a reference. The handle must not be used afterwards. NULL is a no-op. */
void topic_release(topic_t *topic);

/* The topic's name. Valid for as long as the caller holds a reference. */
const char *topic_name(const topic_t *topic);

/* Copy the current counters into `out`. */
void topic_get_stats(const topic_t *topic, topic_stats_t *out);

#endif /* LOGGER_CORE_STORE_TOPIC_H */
