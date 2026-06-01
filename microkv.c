/**
 * This project represents a minimal but useful REPL implementation of a Key-
 * Value storage. The design centers on an in-memory hash table that handles
 * collisions via chaining. Inspired by the architectural simplicity of Redis,
 * it guarantees durability by logging mutations sequentially to an Append-Only
 * File (AOF).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "linenoise.h"

#define HASH_SIZE 4096

typedef struct KVNode {
  char *key;
  char *val;
  struct KVNode *next;
} KVNode;

KVNode *hash_table[HASH_SIZE];

/**
 * [Algorithm: String Duplication Invariant]
 * Given a null-terminated string $s$, this function allocates a distinct
 * block of dynamic memory exactly of size $\text{strlen}(s) + 1$ bytes.
 *
 * Let $p$ be the newly allocated pointer. If $p \neq \text{NULL}$, the contents
 * of $s$ are systematically copied to $p$, establishing a new, independent
 * ownership context in the heap.
 */
char *mystrdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) {
        strcpy(p, s);
    }
    return p;
}

/* Table initialization and cleanup operations. */

void init_table() { memset(hash_table, 0, sizeof(hash_table)); }

/**
 * [Algorithm: Hash Table Destruction]
 * To completely release the dynamic memory occupied by the hash table, we must
 * traverse each bucket independently.
 *
 * Let $h$ be the current bucket index such that $0 \le h < \text{HASH\_SIZE}$.
 * For each bucket, we maintain a traversal pointer $c$ initialized to the head
 * of the linked list, $\text{hash\_table}[h]$. To safely deallocate node $c$
 * without losing the reference to the subsequent nodes, we must cache the link
 * to the next node, $n = c\rightarrow\text{next}$, prior to invoking the
 * memory deallocator on the string fields and the node structure itself.
 */
void free_table() {
  for (int i = 0; i < HASH_SIZE; ++i) {
    KVNode *current = hash_table[i];
    while (current != NULL) {
      KVNode *next_node = current->next;

      free(current->key);
      free(current->val);
      free(current);

      current = next_node;
    }
    hash_table[i] = NULL;
  }
}

/* Cryptographic and Hashing Utilities */

/**
 * [Algorithm: DJB2 String Hashing]
 * This function computes a 32-bit unsigned hash value for a null-terminated
 * character string $s$. It implements Dan Bernstein's legendary 'djb2'
 * algorithm, which is highly effective for short identifiers and strings.
 *
 * Let $H_k$ represent the hash state after processing $k$ characters.
 * The initial state is defined as:
 * $$H_0 = 5381$$
 * For each subsequent character $c = s[k]$, the state transition invariant is:
 * $$H_{k+1} = (H_k \times 33) + c$$
 *
 * The multiplication by 33 is implemented via bit-shifting as $(H_k \ll 5) +
 * H_k$, which allows the compiler to optimize the operation down to a single
 * clock cycle instruction on most modern processor architectures.
 */
unsigned int hash(const char *s) {
  unsigned int hashval = 5381;
  while (*s != '\0') {
    hashval = ((hashval << 5) + hashval) + (unsigned char)(*s);
    s++;
  }
  return hashval;
}

/* Core Data Manipulation Functions */

// Add a new node to the hash_table. If a node with a specific key already
// exists, it will be replaced using the new value to prevent memory leaks.
// Only one value for a key is allowed.
void add_key_value(const char *key, const char *value) {
  unsigned int idx = hash(key) % HASH_SIZE;
  KVNode *current = hash_table[idx];

  while (current) {
    if (strcmp(current->key, key) == 0) {
      free(current->val);
      current->val = mystrdup(value);
      return;
    }
    current = current->next;
  }

  KVNode *new_node = malloc(sizeof(*new_node));
  if (!new_node) {
    fprintf(stderr, "Error during memory allocation!\n");
    exit(EXIT_FAILURE);
  }
  new_node->key = mystrdup(key);
  new_node->val = mystrdup(value);

  new_node->next = hash_table[idx];
  hash_table[idx] = new_node;
}

// Returns the value stored at the searched key.
// If the key is not found, returns NULL.
const char *get_value(const char *key) {
  unsigned int idx = hash(key) % HASH_SIZE;
  KVNode *current = hash_table[idx];

  while (current) {
    if (strcmp(current->key, key) == 0) {
      return current->val;
    }
    current = current->next;
  }
  return NULL;
}

// This function deletes the node containing the key and all the content.
void delete_key_value(const char *key) {
  unsigned int idx = hash(key) % HASH_SIZE;
  KVNode *current = hash_table[idx];
  KVNode *prev = NULL;

  while (current) {
    if (strcmp(current->key, key) == 0) {
      if (prev == NULL) {
        hash_table[idx] = current->next;
      } else {
        prev->next = current->next;
      }
      free(current->key);
      free(current->val);
      free(current);
      return;
    }
    prev = current;
    current = current->next;
  }
}

/**
 * [Algorithm: Append-Only File Replay]
 * Before initializing the interactive REPL, the database must reconstruct its
 * prior state from disk. This function opens the log file in read-only mode
 * ("r").
 *
 * Let each line in the file represent a historical mutation transaction. We
 * read sequentially using a local buffer. For each line:
 * 1. We tokenize the operation, the key, and the value.
 * 2. If the operation matches "SET", we invoke add_key_value().
 * 3. If the operation matches "DEL", we invoke delete_key_value().
 *
 * This sequential replay guarantees that the final state of the in-memory hash
 * table is mathematically identical to the state when the process last
 * terminated.
 */
void load_aof(const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f)
    return; /* File does not exist yet; start with an empty database */

  char line[1024];
  char cmd[32], key[256], val[512];

  while (fgets(line, sizeof(line), f) != NULL) {
    line[strcspn(line, "\n")] = '\0';
    if (strlen(line) == 0)
      continue;

    int parsed = sscanf(line, "%31s %255s %511s", cmd, key, val);
    if (parsed >= 2) {
      if (strcmp(cmd, "SET") == 0 && parsed == 3) {
        add_key_value(key, val);
      } else if (strcmp(cmd, "DEL") == 0) {
        delete_key_value(key);
      }
    }
  }
  fclose(f);
}

/**
 * [Protocol: Linenoise-Powered REPL Loop]
 * The interactive cycle leverages antirez's linenoise library to provide
 * advanced terminal line editing, command history, and raw mode handling
 * without standard GNU Readline overhead.
 *
 * Let $line$ be the dynamically allocated string returned by linenoise().
 * - If $line == \text{NULL}$, the user triggered an EOF (Ctrl+D); the loop terminates.
 * - If $\text{strlen}(line) > 0$, the token is appended to the transient and
 * persistent history files before execution.
 *
 * CRITICAL MEMORY INVARIANT:
 * Because linenoise() internalizes a dynamic buffer allocation via malloc(), 
 * the resulting pointer $line$ MUST be explicitly deallocated via free() at 
 * the end of each evaluation cycle to prevent accumulation of heap leaks.
 */
int main(void) {
  const char *aof_file = "database.aof";
  const char *history_file = ".microkv_history";

  /* 1. Initialization and Boot Replay */
  init_table();
  load_aof(aof_file);

  /* 2. Setup Linenoise History Configuration */
  linenoiseHistoryLoad(history_file); /* Load previous session's history */
  linenoiseHistorySetMaxLen(100);     /* Cap history capacity to 100 entries */

  printf("[MicroKV] Engine initialized with Linenoise. Type 'exit' to quit.\n");

  /* 3. Interactive REPL Loop */
  while (1) {
    /* Linenoise handles both the prompt display and the raw line reading */
    char *line = linenoise("microkv> ");

    /* Check for EOF (Ctrl+D) */
    if (line == NULL) {
      printf("\n");
      break;
    }

    /* Skip blank submissions but maintain memory lifecycle safety */
    if (strlen(line) == 0) {
      free(line);
      continue;
    }

    /* Record valid inputs to the history tracking systems */
    linenoiseHistoryAdd(line);           /* In-memory history */
    linenoiseHistorySave(history_file);  /* Sync history to disk */

    char cmd[32] = {0};
    char key[256] = {0};
    char val[512] = {0};

    /* Parsing Phase */
    int parsed = sscanf(line, "%31s %255s %511s", cmd, key, val);

    if (parsed > 0) {
      /* Execution and Routing Phase */
      if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        free(line); /* Clean up before breaking */
        break;
      } 
      else if (strcmp(cmd, "set") == 0) {
        if (parsed < 3) {
          printf("(error) ERR 'set' command requires both a key and a value\n");
        } else {
          add_key_value(key, val);
          FILE *f = fopen(aof_file, "a");
          if (f) {
            fprintf(f, "SET %s %s\n", key, val);
            fclose(f);
          }
          printf("OK\n");
        }
      } 
      else if (strcmp(cmd, "get") == 0) {
        if (parsed < 2) {
          printf("(error) ERR 'get' command requires a target key\n");
        } else {
          const char *retrieved_val = get_value(key);
          if (retrieved_val) {
            printf("\"%s\"\n", retrieved_val);
          } else {
            printf("(nil)\n");
          }
        }
      } 
      else if (strcmp(cmd, "del") == 0) {
        if (parsed < 2) {
          printf("(error) ERR 'del' command requires a target key\n");
        } else {
          if (get_value(key) != NULL) {
            delete_key_value(key);
            FILE *f = fopen(aof_file, "a");
            if (f) {
              fprintf(f, "DEL %s\n", key);
              fclose(f);
            }
            printf("(integer) 1\n");
          } else {
            printf("(integer) 0\n");
          }
        }
      } 
      else {
        printf("(error) ERR Unknown or unsupported command '%s'\n", cmd);
      }
    }

    /* Enforce the memory invariant: release linenoise buffer */
    free(line);
  }

  /* 4. Shutdown and Global Resource Eviction */
  printf("Graceful shutdown initiated. Freeing allocated nodes...\n");
  free_table();
  
  return EXIT_SUCCESS;
}

