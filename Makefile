# ==============================================================================
# [Specification: MicroKV Compilation Engine]
# This Makefile controls the build lifecycle of the MicroKV utility.
#
# Let $CC$ define the core compiler backend (defaulting to gcc).
# Let $CFLAGS$ define the strict compilation invariants:
#   -std=c99: Enforce the ISO C99 standard specification.
#   -Wall -Wextra: Enable comprehensive compiler warning diagnostics.
#   -O2: Apply standard level-2 code optimizations for production execution.
#   -g: Maintain debugging symbols for GDB/LLDB analysis.
# ==============================================================================

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -D_DEFAULT_SOURCE -O2 -g
TARGET  = microkv
SRC     = microkv.c linenoise.c
OBJ     = $(SRC:.c=.o)

.PHONY: all clean run

# [Target: Default Build Rule]
# Resolution invariant: The primary entry point 'all' depends strictly on the 
# macro expansion of $(TARGET). This ensures that executing `make` without 
# parameters defaults to generating the binary executable.
all: $(TARGET)

# [Target: Binary Linking Execution]
# The binary target file relies on the compiled object state $(OBJ).
# Execution maps the linker phase to join the static object components into a 
# single standalone executable machine image.
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# [Target: Object Translation Invariant]
# Each source code element `.c` translates into an intermediate object unit `.o`.
# The flag `-c` instructs the compiler to stop before the linking phase, 
# outputting raw object code directly from the parsing loop.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# [Target: System Purge / Clean]
# To prevent state contamination during subsequent compilation loops, the clean 
# rule executes a destructive sweep using the system file eraser. It forcefully 
# removes the target binary executable, any transient `.o` object artifacts, 
# and the local database storage log.
clean:
	rm -f $(TARGET) $(OBJ) database.aof

# [Target: Local Test Execution]
# Convenience shortcut rule to rebuild the architecture from scratch and 
# immediately initiate the interactive REPL shell environment.
run: clean all
	./$(TARGET)
