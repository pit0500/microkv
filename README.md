# MicroKV

A minimalist, ultra-fast, in-memory Key-Value store featuring synchronous durability via an Append-Only File (AOF). Inspired by the core architecture of early Redis instances and built entirely in pure ISO C99/C17 with a philosophy of radical simplicity.

To provide a modern, production-grade command-line experience without the massive overhead of GNU Readline, MicroKV integrates Salvatore Sanfilippo's (`antirez`) legendary `linenoise` library for line editing and persistent interactive history.

---

## Architectural Overview

MicroKV operates on a predictable, deterministic lifecycle split into four phases:

1. **Boot / Replay**: Upon invocation, the engine scans the local directory for an existing `database.aof` transaction log. If present, it executes a sequential replay of all recorded mutations (`SET` and `DEL`) to perfectly reconstruct the last known state in RAM before rendering the prompt.
2. **Read (Linenoise)**: The runtime enters a synchronous REPL loop powered by `linenoise`, allowing fluent line editing, navigation, and command tracking.
3. **Eval / Mutate**: Incoming text strings are tokenized into command-key-value tuples. Read transactions (`get`) execute in deterministic $O(1)$ time. Write transactions (`set`, `del`) instantly update the transient state and append the raw mutation block to disk.
4. **Shutdown**: Graceful exit procedures intercept terminators to sequentially evict all allocated string references and nodes, ensuring an absolute zero memory-leak footprint.

---

## Technical Specifications

* **Core Storage**: Hash Table resolving structural bucket collisions via closed-addressing singly-linked lists (Chaining).
* **Hashing Function**: Dan Bernstein's **DJB2** bit-shifting string hashing algorithm ($H_{k+1} = (H_k \times 33) + c$), minimizing clustering and enforcing uniform distribution across the fixed bucket space.
* **Memory Lifecycle**: Strict, single-owner heap allocation invariants. No hidden overhead or reference counting; data structures are surgically allocated via `malloc`/`strdup` and instantly claimed via `free`.
* **Persistence**: Synchronous `Append-Only File` file streams ensuring durability against unexpected process interruption.

---

## Supported Commands

MicroKV implements a subset of the standard Redis protocol vocabulary:

* `set <key> <value>`: Inserts or replaces a key-value mapping. Returns `OK`.
* `get <key>`: Extracts the value string matching the specified key. Returns `"<value>"` or `(nil)` if not found.
* `del <key>`: Safely unlinks, deallocates, and purges a key mapping from memory and logs the removal. Returns `(integer) 1` if the key existed, or `(integer) 0` otherwise.
* `exit` / `quit`: Breaks the execution loop and triggers global resource reclamation.

---

## Getting Started

### Prerequisites

A standard POSIX environment (Linux, macOS, BSD) equipped with a modern `gcc` or `clang` compiler toolchain and `make`.

### Compilation

Build the architecture by triggering the optimized compilation pipeline defined within the `Makefile`:

```bash
make

To automatically sweep temporary artifacts, clear the active log, and spin up a fresh REPL session in a single transaction:
make run

Usage Example
$ ./microkv
[MicroKV] Engine initialized with Linenoise. Type 'exit' to quit.
microkv> set session_id 42077
OK
microkv> get session_id
"42077"
microkv> del session_id
(integer) 1
microkv> get session_id
(nil)
microkv> quit
Graceful shutdown initiated. Freeing allocated nodes...
$

## Project Structure
• microkv.c — Core transactional logic, memory allocation loops, AOF parser, and the REPL pipeline.
• linenoise.c / linenoise.h — Minimalist terminal raw mode controller and inline input processor.
• Makefile — Build automation engine utilizing production-level diagnostic flags (-Wall -Wextra -O2 -g).

