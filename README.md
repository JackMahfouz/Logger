# Logger

A small C library for storing JSON logs, organised into named **topics**. Writers
append records to a topic without waiting on disk; readers query a topic or tail
it live.

The directory layout follows
[JackWetherell/c-project-structure](https://github.com/JackWetherell/c-project-structure).

## Status

**The public and internal APIs are designed and declared. Nothing behind them is
implemented yet.**

| Area | State |
|---|---|
| Public API headers (`include/core/store/`) | Done: declared, compile cleanly |
| Internal headers (`include/core/internal/`) | Done: declared, compile cleanly |
| Implementations (`src/core/`) | Not started: the directory is empty |
| I/O thread and on-disk format | Not started: only the fields that point at them exist |
| Tests (`tests/`) | Not started |
| `src/main.c` | Placeholder that prints a greeting |

The headers are a first draft. Treat the signatures as the proposed shape of the
API, open to change once the implementation starts.

## Architecture

Solid boxes exist today as header declarations. Dashed boxes are planned.

```
                      Application code
                             |
     +-----------------------+-----------------------+
     | lifecycle             | writes                | reads
     v                       v                       v
+---------------------- PUBLIC API   include/core/store/ ----------------------+
|                                                                              |
|  store.h                 writer.h                 reader.h                   |
|  store_init              writer_open / close      reader_open / close        |
|  store_shutdown          writer_append            reader_next                |
|  store_topic_create      writer_append_batch      reader_query_t             |
|  store_topic_open        writer_flush             log_record_t               |
|  store_topic_delete      writer_config_t                                     |
|  store_status_t                                                              |
|                                                                              |
|  topic.h   topic_t, topic_config_t, topic_retain / release, topic_get_stats  |
+------------------------------------------------------------------------------+
                             |
                             v
+--------------------- INTERNAL   include/core/internal/ ----------------------+
|                                                                              |
|  store_priv.h                                                                |
|    struct store    topics_lock (rwlock), topic_map_t, io_thread, shutdown    |
|    struct topic    refcount, seq + stats counters, lock, fd, queue ---+      |
|    topic_map_t     chained hash map: topic name -> topic              |      |
|                                                                       v      |
|  buffer.h                                                                    |
|    ring_buffer_t   lock-free SPSC byte ring, length-prefixed records         |
|                                                                              |
+------------------------------------------------------------------------------+
                             |
                             v
    +- - - - - - - - - - - - - - - - - - - - - - - - -+
    : PLANNED: I/O thread, one per store,             :
    : drains every topic's ring buffer to disk        :
    +- - - - - - - - - - - - - - - - - - - - - - - - -+
                             |
                             v
    +- - - - - - - - - - - - - - - - - - - - - - - - -+
    : PLANNED: topic data files under root_dir        :
    : (newline-delimited JSON, one file per topic)    :
    +- - - - - - - - - - - - - - - - - - - - - - - - -+
```

### Write path

```
 producer thread(s)                   I/O thread                         reader
 ------------------                   ----------                         ------
 writer_append(json)
   lock topic->lock
   ring_buffer_push(topic->queue) --> ring_buffer_pop(topic->queue)
   unlock                             write to topic->fd  - - - - - -->  reader_next()
   (STORE_ERR_FULL if no room;
    counted in records_dropped)
```

Appends return as soon as the record is in the ring buffer. `writer_flush` blocks
until everything queued so far is on disk.

### Design decisions already made in the headers

- **Opaque handles.** `store_t`, `topic_t`, `log_writer_t` and `log_reader_t` are
  opaque to callers. The full `struct store` and `struct topic` live in
  `internal/store_priv.h`.
- **Reference-counted topics.** The store hands out a handle already retained for
  the caller, who releases it with `topic_release`. Writers and readers take
  their own reference, so a topic outlives a caller that releases early.
- **One status enum.** `store_status_t` is shared by the store, writer and reader
  APIs. `reader_next` also uses it for the non-error outcomes `STORE_END` and
  `STORE_TIMEOUT`.
- **Writers take serialized JSON.** Each record is one complete JSON value, copied
  before the call returns. The store does not build JSON for you.
- **All-or-nothing batches.** `writer_append_batch` accepts a whole batch or none
  of it.
- **Single-producer, single-consumer ring buffer.** This is the practical
  lock-free case. Several writers on one topic serialise their pushes through
  `topic->lock`; the I/O thread is the only consumer.
- **Lock order.** `store->topics_lock` first, then `topic->lock`. Never the other
  way round.

## Directory structure

```
|-- CMakeLists.txt         <- CMake build (C11, include/ on the include path)
|-- README.md              <- This file
|-- bin                    <- Reserved for executables (unused so far)
|-- build                  <- Reserved for intermediates (unused so far)
|-- data                   <- Project data
|   |-- raw
|   |-- interim
|   |-- input
|   `-- output
|-- docs                   <- Documentation
|-- include
|   `-- core
|       |-- store          <- Public API
|       |   |-- store.h        init, shutdown, topic operations, status codes
|       |   |-- topic.h        topic definition and handle lifecycle
|       |   |-- writer.h       JSON log writer and batching
|       |   `-- reader.h       querying and stream reading
|       `-- internal       <- Not part of the public API
|           |-- store_priv.h   store and topic state, locks, topic hash map
|           `-- buffer.h       lock-free ring buffer for async I/O
|-- lib                    <- Libraries to link against
|-- src
|   |-- main.c             <- Placeholder entry point
|   `-- core
|       `-- store          <- Planned: implementations of the headers above
`-- tests                  <- Unit tests (none yet)
```

Include headers by their path under `include/`, for example
`#include "core/store/store.h"`.

## Requirements

- A C11 compiler (developed with GCC)
- CMake 4.3 or newer
- POSIX threads

`internal/store_priv.h` uses `pthread_rwlock_t`. That works under `-std=gnu11`,
which is what CMake uses by default. Under strict `-std=c11` you must define
`_POSIX_C_SOURCE=200112L` (or later) before including any header. The same note
is in the header.

## Build and run

```
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
./cmake-build-debug/Logger
```

At the moment this builds only the placeholder `src/main.c` and prints
`Hello C is working!`. The headers are not yet compiled into anything.

## Next steps

1. Implement `topic.c` and the `topic_map_*` functions.
2. Implement `buffer.c` (ring buffer) and unit-test it from `tests/`.
3. Implement `store.c`: `store_init`, `store_shutdown`, topic create/open/delete.
4. Define the on-disk format and write the I/O thread.
5. Implement `writer.c`, then `reader.c`.
6. Wire `src/core/**` into CMake as a library target and add a `tests` target.
