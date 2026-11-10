#ifndef LOGGER_CORE_STORE_READER_H
#define LOGGER_CORE_STORE_READER_H

/*
 * Log querying and stream reading interfaces.
 *
 * A reader walks the records of one topic in sequence order, either over a
 * fixed range (a query) or as a live tail (`follow`). Records are returned one
 * at a time through reader_next().
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/store/store.h"
#include "core/store/topic.h"

/* Opaque reader handle. */
typedef struct log_reader log_reader_t;

/* What to read. A zeroed struct reads every record currently in the topic. */
typedef struct {
    uint64_t from_seq;     /* first sequence number, inclusive */
    uint64_t to_seq;       /* last sequence number, inclusive; 0 = no upper bound */
    int64_t  since_ms;     /* oldest timestamp (Unix ms), inclusive; 0 = no lower bound */
    int64_t  until_ms;     /* newest timestamp (Unix ms), inclusive; 0 = no upper bound */
    size_t   limit;        /* stop after this many records; 0 = no limit */
    bool     follow;       /* keep waiting for new records instead of ending */
} reader_query_t;

/* One record as returned by reader_next(). */
typedef struct {
    uint64_t    seq;           /* position within the topic, increasing */
    int64_t     timestamp_ms;  /* Unix ms when the record was accepted */
    const char *json;          /* the record; not NUL-terminated */
    size_t      len;
} log_record_t;

/*
 * Open a reader on `topic`. The reader takes its own reference on the topic.
 * `query` may be NULL to read everything.
 */
store_status_t reader_open(topic_t *topic, const reader_query_t *query,
                           log_reader_t **out);

/* Release the topic and free the reader. NULL is a no-op. */
void reader_close(log_reader_t *reader);

/*
 * Fetch the next record into `record`.
 *
 *   STORE_OK       a record was returned
 *   STORE_END      the query range is exhausted (never returned when following)
 *   STORE_TIMEOUT  `timeout_ms` elapsed with no new record (following only)
 *
 * `timeout_ms` < 0 waits indefinitely, 0 never waits. `record->json` stays
 * valid until the next reader_next() or reader_close() on this reader.
 */
store_status_t reader_next(log_reader_t *reader, log_record_t *record,
                           int timeout_ms);

#endif /* LOGGER_CORE_STORE_READER_H */
