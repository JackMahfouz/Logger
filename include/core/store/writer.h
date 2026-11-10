#ifndef LOGGER_CORE_STORE_WRITER_H
#define LOGGER_CORE_STORE_WRITER_H

/*
 * JSON log writer interface and batching APIs.
 *
 * A writer appends JSON records to one topic. Records are queued into the
 * topic's async buffer and written by the store's I/O thread, so append calls
 * return without waiting on disk. Each record is one complete JSON value;
 * the store keeps them newline-delimited on disk.
 */

#include <stddef.h>
#include <stdint.h>

#include "core/store/store.h"
#include "core/store/topic.h"

/* Opaque writer handle. */
typedef struct log_writer log_writer_t;

typedef struct {
    size_t   batch_max_records;    /* flush after this many queued records; 0 = default */
    size_t   batch_max_bytes;      /* flush after this many queued bytes; 0 = default */
    uint32_t flush_interval_ms;    /* flush at least this often; 0 = only on the limits above */
} writer_config_t;

/*
 * Open a writer on `topic`. The writer takes its own reference on the topic.
 * `config` may be NULL for defaults.
 */
store_status_t writer_open(topic_t *topic, const writer_config_t *config,
                           log_writer_t **out);

/* Flush anything still queued, release the topic and free the writer. NULL is a no-op. */
void writer_close(log_writer_t *writer);

/*
 * Queue one record. `json` must be a single complete JSON value and is copied
 * before the call returns. STORE_ERR_FULL means the buffer had no room and
 * the record was dropped (and counted in topic_stats_t.records_dropped).
 */
store_status_t writer_append(log_writer_t *writer, const char *json, size_t len);

/*
 * Queue `count` records as one batch: they are accepted together or not at
 * all, and are written contiguously. `records[i]` is `lengths[i]` bytes long.
 */
store_status_t writer_append_batch(log_writer_t *writer,
                                   const char *const *records,
                                   const size_t *lengths, size_t count);

/* Block until every record queued so far is written to disk. */
store_status_t writer_flush(log_writer_t *writer);

#endif /* LOGGER_CORE_STORE_WRITER_H */
