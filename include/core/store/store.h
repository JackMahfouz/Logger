#ifndef LOGGER_CORE_STORE_STORE_H
#define LOGGER_CORE_STORE_STORE_H

/*
 * Main public API for the store: initialisation, shutdown and topic
 * operations. Also defines store_status_t, which the writer and reader APIs
 * share.
 */

#include <stddef.h>

#include "core/store/topic.h"

#define STORE_DEFAULT_MAX_TOPICS   1024
#define STORE_DEFAULT_BUFFER_BYTES (1u << 20)   /* 1 MiB per topic */

/* Opaque store handle. The layout lives in internal/store_priv.h. */
typedef struct store store_t;

typedef enum {
    STORE_OK = 0,

    /* Non-error outcomes from reader_next(). */
    STORE_END,               /* no more records in the requested range */
    STORE_TIMEOUT,           /* no record arrived within the timeout */

    STORE_ERR_INVALID_ARG,
    STORE_ERR_NOMEM,
    STORE_ERR_IO,
    STORE_ERR_EXISTS,        /* topic already exists */
    STORE_ERR_NOT_FOUND,     /* no such topic */
    STORE_ERR_FULL,          /* async buffer full; record not accepted */
    STORE_ERR_CLOSED         /* store has been shut down */
} store_status_t;

typedef struct {
    const char *root_dir;    /* directory that holds topic data; required */
    size_t      max_topics;  /* 0 = STORE_DEFAULT_MAX_TOPICS */
    size_t      buffer_bytes;/* async I/O buffer per topic; 0 = STORE_DEFAULT_BUFFER_BYTES */
} store_config_t;

/* Create a store rooted at config->root_dir and start its I/O thread. */
store_status_t store_init(const store_config_t *config, store_t **out);

/*
 * Flush pending writes, stop the I/O thread and free the store. All writers
 * and readers must be closed first. Returns the status of the final flush.
 * `store` must not be used afterwards.
 */
store_status_t store_shutdown(store_t *store);

/*
 * Topic operations. On success, `*out` is a handle already retained for the
 * caller, who must pass it to topic_release().
 */

/* Create a new topic. STORE_ERR_EXISTS if the name is taken. */
store_status_t store_topic_create(store_t *store, const topic_config_t *config,
                                  topic_t **out);

/* Look up an existing topic. STORE_ERR_NOT_FOUND if there is none. */
store_status_t store_topic_open(store_t *store, const char *name, topic_t **out);

/* Remove a topic and its data. Handles already held stay valid until released. */
store_status_t store_topic_delete(store_t *store, const char *name);

/* Number of topics currently in the store. */
size_t store_topic_count(const store_t *store);

/* Static, human-readable description of a status code. Never NULL. */
const char *store_strerror(store_status_t status);

#endif /* LOGGER_CORE_STORE_STORE_H */
