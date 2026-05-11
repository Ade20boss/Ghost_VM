# Ghost VM

A stack-based bytecode virtual machine with a binary network delivery protocol, built from first principles in C. Ghost VM separates two concerns that production systems often conflate: **what to execute** (the VM's typed object system and bytecode interpreter) and **how to deliver it** (a binary wire protocol with integrity checking and zero-copy parsing).

This document is the full technical design record of the system — architecture, wire format, object model, memory ownership, design decisions, and the tradeoffs behind each one.

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Building and Running](#building-and-running)
3. [Verification](#verification)
4. [The Wire Protocol](#the-wire-protocol)
5. [The Object System](#the-object-system)
6. [The Instruction Set](#the-instruction-set)
7. [The Execution Model](#the-execution-model)
8. [Memory Ownership Model](#memory-ownership-model)
9. [Design Decisions and Tradeoffs](#design-decisions-and-tradeoffs)
10. [File Structure](#file-structure)

---

## System Architecture

Ghost VM is organized into two independent layers connected by a shared wire format contract.

```
┌─────────────────────────────────────────────────────────────┐
│                         main.c                              │
│              (orchestration — emit, pack, execute)          │
└────────────────────┬──────────────────┬─────────────────────┘
                     │                  │
        ┌────────────▼──────┐  ┌────────▼────────────┐
        │   TRANSPORT LAYER │  │  EXECUTION LAYER     │
        │   network.c       │  │  vm.c                │
        │   network.h       │  │  vm.h                │
        └────────────┬──────┘  └────────┬─────────────┘
                     │                  │
              ┌──────▼──────────────────▼──────┐
              │           opcode.h              │
              │   (shared wire format contract) │
              └─────────────────────────────────┘
```

**Transport layer** (`network.c`, `network.h`): Serializes `opcodebatch` arrays into a contiguous wire buffer with a `PacketHeader`, computes an FNV-1a checksum over the full packet, and on the receive side validates integrity and builds a zero-copy index of batch pointers into the buffer.

**Execution layer** (`vm.c`, `vm.h`): A stack-based interpreter with a five-type polymorphic object system. The operand stack is itself a heap-managed COLLECTION object. The VM reads one flat `uint8_t*` bytecode buffer, advancing an instruction pointer until it hits `OP_HALT`.

**Shared contract** (`opcode.h`): Defines the `Opcode` enum with explicit hex values, the `opcodebatch` struct with its flexible array member, and `BATCH_SIZE_MAX`. Both layers include this header. Neither layer knows what the other does — they communicate only through the types defined here.

The two layers have no direct dependency on each other. `main.c` is the only translation unit that calls into both.

---

## Building and Running

```bash
gcc -o ghost main.c vm.c network.c -lm -Wall -Wextra
./ghost
```

`-lm` links the math library for `fabsf`, used in float equality comparisons.

**On macOS with Apple Silicon:** `gcc` is aliased to Apple's Clang by default. To use real GCC:

```bash
brew install gcc
gcc-14 -o ghost main.c vm.c network.c -lm -Wall -Wextra
```

**Memory check with AddressSanitizer:**

```bash
gcc -fsanitize=address -o ghost main.c vm.c network.c -lm
./ghost
```

---

## Verification

Compiled with full warnings enabled, zero diagnostics:

```bash
gcc -Wall -Wextra -g main.c network.c vm.c -o ghost_vm
./ghost_vm
```

Valgrind clean — zero leaks, zero errors, perfectly balanced allocations:

```
==19981== HEAP SUMMARY:
==19981==     in use at exit: 0 bytes in 0 blocks
==19981==   total heap usage: 63 allocs, 63 frees, 5,955 bytes allocated
==19981==
==19981== All heap blocks were freed -- no leaks are possible
==19981==
==19981== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

Expected output:

```
--- Packet Received ---

=== EXECUTING KERNEL STRESS TEST ===
--- VM BOOT SEQUENCE INITIATED ---
2x^2 - 3x + 7, x=6:
61
--- VM HALTED ----
BATCH 0 COMPLETE

--- VM BOOT SEQUENCE INITIATED ---
<4.000000,6.000000,13.000000>
<12.000000,12.000000,12.000000>
--- VM HALTED ----
BATCH 1 COMPLETE

--- VM BOOT SEQUENCE INITIATED ---
Ghost VM
[100, 42, 3.140000, 2.710000]
78.539749
--- VM HALTED ----
BATCH 2 COMPLETE

=== VIRTUAL MACHINE TERMINATED ===
```

The three batches demonstrate the full instruction set in sequence: polynomial evaluation via integer arithmetic (batch 0), element-wise vector addition and scalar broadcasting (batch 1), string concatenation, heterogeneous collection construction, and floating-point computation (batch 2).

---

## The Wire Protocol

### Packet Layout

Every packet sent from the transport layer has this exact byte layout:

```
┌─────────────────────────────────────────────┐
│              PacketHeader (16 bytes)         │
│  magic_no     uint32_t  — 0x47485354 "GHST" │
│  checksum     uint32_t  — FNV-1a over packet │
│  sequence_id  uint32_t  — monotonic counter  │
│  batch_count  uint32_t  — number of batches  │
├─────────────────────────────────────────────┤
│           opcodebatch[0]                     │
│  batch_id    uint32_t                        │
│  write_head  uint32_t  — bytes used          │
│  opcodes[]   uint8_t[] — write_head bytes    │
├─────────────────────────────────────────────┤
│           opcodebatch[1]                     │
│  ...                                         │
├─────────────────────────────────────────────┤
│           opcodebatch[N-1]                   │
│  ...                                         │
└─────────────────────────────────────────────┘
```

The entire struct is packed (`__attribute__((packed))`). There is no padding anywhere in the wire format. This means the in-memory representation is identical to the on-wire representation on any architecture, which is a prerequisite for a correct binary protocol.

### The Magic Number

`0x47485354` is the ASCII encoding of `GHST`. The parser checks this as the first validation step — any packet that doesn't begin with these four bytes is rejected immediately without reading further. This is standard practice in binary protocols (PNG uses `\x89PNG`, zip uses `PK`, ELF uses `\x7fELF`).

### Integrity: FNV-1a Checksum

Ghost VM uses FNV-1a (Fowler–Noll–Vo variant 1a) as its checksum algorithm:

```c
uint32_t hash_algorithm(uint8_t *buffer, size_t size) {
    uint32_t hash = 0x811c9dc5;   // FNV offset basis
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ buffer[i]) * 0x01000193;  // FNV prime
    }
    return hash;
}
```

FNV-1a operates XOR-then-multiply (as opposed to FNV-1 which is multiply-then-XOR). The XOR-first ordering produces better avalanche characteristics — a single bit flip in the input propagates to more output bits, making accidental collisions less likely.

**Checksum computation protocol:**

The checksum is computed over the entire packet with the `checksum` field zeroed. This is the standard approach:

1. `packager`: writes `checksum = 0x00000000`, serializes all batches, then computes FNV-1a over the full buffer and writes the result into `header->checksum`.
2. `parser`: saves `header->checksum`, zeros the field, recomputes FNV-1a over the full buffer, restores the field, compares. If they differ, the packet is rejected.

This approach avoids having to split the hash computation around the checksum field — the packet is always hashed as a single contiguous region.

### The opcodebatch Flexible Array Member

```c
typedef struct __attribute__ ((packed)) {
    uint32_t batch_id;
    uint32_t write_head;
    uint8_t  opcodes[];    // flexible array member
} opcodebatch;
```

`opcodes[]` is a C99 flexible array member. `sizeof(opcodebatch) == 8` — only the two fixed fields are counted. The actual opcode bytes live immediately after the struct in memory. Allocation always requires:

```c
malloc(sizeof(opcodebatch) + N)   // N bytes of opcode space
```

This design has an important consequence for parsing: you cannot index into an array of `opcodebatch` structs using normal array subscript notation. `batches[1]` would jump exactly `sizeof(opcodebatch) = 8` bytes forward, landing in the middle of batch 0's opcodes. The size of each batch in the buffer varies by `write_head`. The parser must walk the buffer manually:

```c
payload = (opcodebatch*)((uint8_t*)payload + sizeof(opcodebatch) + payload->write_head);
```

The cast to `uint8_t*` before the pointer arithmetic is critical — pointer arithmetic in C scales by the pointed-to type. Without it, the compiler would scale the addition by `sizeof(opcodebatch)` instead of 1 byte, producing completely wrong addresses.

The parser records each batch's address in a separate `opcodebatch**` index array — the manual indexing result that the compiler cannot compute automatically.

### Zero-Copy Parsing

The parser does not copy batch data out of the packet buffer. `result.batches[i]` points directly into `packet.buff`. This is an intentional zero-copy design: the packet buffer is allocated once by `packager`, ownership transfers to the caller, and the parser builds an index into it. The consequence is a strict lifetime constraint documented in `network.h`:

```c
// OWNERSHIP CONTRACT:
// - result.batches (the pointer array) is heap-allocated by parser.
//   The caller must free it with free(result.batches).
// - result.batches[i] (each batch pointer) points directly into the
//   'buffer' argument passed to parser. The caller must NOT free these.
//   The caller must NOT free 'buffer' until all batches have been read.
```

This is the same contract used by `recv()` buffer management in real network code.

---

## The Object System

Ghost VM implements a five-type dynamic object system using a tagged union — the standard C pattern for discriminated types used in interpreters like CPython and SQLite.

### Type Definitions

```c
typedef enum {
    INTEGER,
    FLOAT,
    STRING,
    COLLECTION,
    VECTOR,
} object_kind_t;

typedef union {
    int         v_int;
    float       v_float;
    char       *v_string;
    collection  v_collection;
    vector      v_vector;
} object_data_t;

typedef struct Object {
    object_kind_t kind;
    object_data_t data;
} object_t;
```

Every value in the VM is a heap-allocated `object_t*`. The `kind` field is the tag — all dispatch on it before touching `data`. Reading `data.v_int` on an object whose `kind` is FLOAT is undefined behavior.

### The Five Types

**INTEGER** — 32-bit signed integer, stored by value in `data.v_int`. No heap allocation beyond the `object_t` itself.

**FLOAT** — 32-bit IEEE 754 single-precision float, stored by value in `data.v_float`. Float equality uses epsilon comparison (`fabsf(a - b) <= 0.00001`) rather than `==` to account for floating-point representation errors.

**STRING** — Null-terminated heap-allocated string. `data.v_string` is a separate `malloc`'d buffer. `object_free` frees both the string buffer and the object struct.

**COLLECTION** — A dynamically-resizing array of `object_t*` pointers, implemented as a struct with `length`, `capacity`, `data` (pointer array), and a `stack` boolean flag. The `stack` flag gates certain operations — `collection_access` (random index) is blocked on stack-mode collections; `collection_pop` works on both. The operand stack itself is a COLLECTION with `stack = true`, initial capacity 256. When full, `collection_append` doubles capacity via `realloc`.

**VECTOR** — A fixed-dimension array of `float` coordinates. `data.v_vector.dimensions` is the count; `data.v_vector.coords` is a heap-allocated `float*` array. Vector operations (add, subtract, multiply, divide) support both element-wise vector×vector and scalar broadcasting (vector × integer, vector × float).

### The Recursive Destructor

```c
void object_free(object_t *obj) {
    if (obj == NULL) return;
    
    if (obj->kind == STRING)
        free(obj->data.v_string);
    
    else if (obj->kind == COLLECTION) {
        for (size_t i = 0; i < obj->data.v_collection.length; i++)
            object_free(obj->data.v_collection.data[i]);
        free(obj->data.v_collection.data);
    }
    
    else if (obj->kind == VECTOR)
        free(obj->data.v_vector.coords);
    
    free(obj);
}
```

`object_free` is the single cleanup path for all object kinds. It recurses into COLLECTIONs, frees the coords array for VECTORs, and frees the string buffer for STRINGs. INTEGER and FLOAT have no heap allocations beyond the struct itself. The NULL guard at entry makes defensive `object_free(NULL)` calls safe.

### Move Semantics for Collection Merging

When two COLLECTIONs are added via `object_add`, the result is a new collection containing pointers to all elements from both inputs. The elements are **moved**, not copied:

```c
// Transfer element pointers to new_collection
for (size_t i = 0; i < a->data.v_collection.length; i++)
    collection_append(new_collection, a->data.v_collection.data[i]);
for (size_t j = 0; j < b->data.v_collection.length; j++)
    collection_append(new_collection, b->data.v_collection.data[j]);

// [DO NOT TOUCH] PREVENT RECURSIVE DESTRUCTION
// Setting length=0 means object_free() will not recurse into elements.
// The elements now belong to new_collection.
a->data.v_collection.length = 0;
b->data.v_collection.length = 0;
```

Setting `length = 0` before the caller frees `a` and `b` is the mechanism that prevents double-free. When `object_free` iterates `for i < length`, it iterates zero times and does not touch the elements, which now belong exclusively to `new_collection`. This implements ownership transfer without a type-system-enforced ownership model — a deliberate design choice to avoid the complexity of reference counting or a borrow checker while still correctly managing memory.

### Polymorphic Arithmetic

Every arithmetic operation dispatches on both operands' types using nested switch statements. The full type dispatch table for `object_add`:

| `a` type | `b` type | Result |
|---|---|---|
| INTEGER | INTEGER | INTEGER |
| INTEGER | FLOAT | FLOAT |
| FLOAT | INTEGER | FLOAT |
| FLOAT | FLOAT | FLOAT |
| STRING | STRING | STRING (concatenation) |
| COLLECTION | COLLECTION | COLLECTION (merge, move semantics) |
| VECTOR | INTEGER | VECTOR (scalar broadcast) |
| VECTOR | FLOAT | VECTOR (scalar broadcast) |
| VECTOR | VECTOR | VECTOR (element-wise, requires equal dimensions) |

`object_subtract`, `object_multiply`, and `object_divide` follow equivalent dispatch tables with type-appropriate semantics (string repetition for `STRING * INTEGER`, collection replication for `COLLECTION * INTEGER`, element-wise vector arithmetic, division-by-zero guards for all numeric paths).

---

## The Instruction Set

Opcodes are defined with explicit hex values in `opcode.h`. Explicit assignment is mandatory for a binary wire protocol — compiler-assigned enum values would silently renumber when new opcodes are inserted, corrupting all serialized bytecode.

```
Opcode              Value   Operands        Stack effect
──────────────────────────────────────────────────────────
OP_HALT             0x00    none            halts execution
OP_PUSH_INT         0x01    int32 (4 bytes) pushes INTEGER
OP_PUSH_FLOAT       0x02    float (4 bytes) pushes FLOAT
OP_PUSH_STRING      0x03    int32 + N bytes pushes STRING
OP_BUILD_COLLECTION 0x04    uint8 count     pops N, pushes COLLECTION
OP_BUILD_VECTOR     0x05    uint8 dims      pops N floats, pushes VECTOR
OP_ADD              0x06    none            pops 2, pushes result
OP_SUB              0x07    none            pops 2, pushes result
OP_MUL              0x08    none            pops 2, pushes result
OP_DIV              0x09    none            pops 2, pushes result
OP_PRINT            0x0A    none            pops 1, prints, frees
```

**Variable-length encoding:** `OP_PUSH_INT` and `OP_PUSH_FLOAT` consume 5 bytes total (1 opcode + 4 operand). `OP_PUSH_STRING` consumes `1 + 4 + length` bytes — the 4-byte length field is read first, then exactly that many bytes of string data. `OP_BUILD_COLLECTION` and `OP_BUILD_VECTOR` read a single byte count/dimension immediately after the opcode byte.

**Alignment-safe reads:** All multi-byte operand reads use `memcpy` rather than pointer casting:

```c
int push_int;
memcpy(&push_int, &vm->bytecode[vm->ip], 4);
```

Casting `&vm->bytecode[vm->ip]` directly to `int*` and dereferencing would be undefined behavior on architectures that require aligned memory access (ARM requires 4-byte alignment for 4-byte reads). `memcpy` has no alignment requirement — it copies bytes without assuming anything about the source address.

**Bounds checking:** Every instruction fetch and every operand read is bounds-checked against `vm->bytecode_len` before executing. A bytecode buffer that ends without `OP_HALT` terminates with a diagnostic rather than reading into adjacent memory.

---

## The Execution Model

### The Batch Execution Model

Ghost VM uses a per-batch execution model rather than a single flat bytecode buffer. Each `opcodebatch` is an independent execution unit. The VM maintains a single operand stack across batch boundaries — values pushed in batch N are available to batch N+1.

```
batch 0 opcodes → run_vm(vm) → OP_HALT → return
                                              │
batch 1 opcodes → run_vm(vm) → OP_HALT → return   ← same vm, same stack
                                              │
batch 2 opcodes → run_vm(vm) → OP_HALT → return
```

Each `run_vm` call requires the batch to end with `OP_HALT`. The VM has no awareness of batch boundaries — it reads bytes until it halts. The batch model is a transport concept; the VM is a tape machine.

This model has a real precedent: the JVM loads and executes class files (analogous to batches) as discrete units, sharing state through the heap. The alternative — flattening all batches into one contiguous buffer with a single HALT — treats batches as a transport-only concern with no runtime significance.

### The Instruction Pointer and Bounds Safety

The VM struct tracks both a bytecode pointer and its length:

```c
typedef struct {
    uint8_t *bytecode;
    size_t   bytecode_len;
    size_t   ip;
    object_t *operand_stack;
} vm_t;
```

`new_virtual_machine(uint8_t *code, size_t length)` stores both. Between batch executions in `main.c`, both `vm->bytecode` and `vm->bytecode_len` are updated together before each `run_vm` call. The `ip` is reset to 0.

At the top of every loop iteration in `run_vm`:

```c
if (vm->ip >= vm->bytecode_len) {
    fprintf(stderr, "VM Error: ip overran bytecode without HALT\n");
    return;
}
```

Before every multi-byte operand read, the remaining buffer is checked:

```c
if (vm->ip + 4 > vm->bytecode_len) {
    fprintf(stderr, "VM Error: truncated operand\n");
    return;
}
```

A VM that reads past its bytecode buffer executes arbitrary memory as opcodes. In a networked context where the bytecode arrives over a socket, this is an arbitrary code execution vulnerability. The bounds checks close this.

### Stack Operations

All arithmetic operations follow this pattern in `run_vm`:

```c
object_t *pop1 = collection_pop(vm->operand_stack);  // top of stack
object_t *pop2 = collection_pop(vm->operand_stack);  // second from top
object_t *result = object_add(pop2, pop1);           // pop2 op pop1
```

`pop1` is the top of the stack (pushed last), `pop2` is below it. For commutative operations (ADD, MUL) the order doesn't matter. For non-commutative operations (SUB, DIV), the convention is `pop2 op pop1` — to compute `a - b`, push `a` first, then `b`. This matches the conventional stack machine convention: operands are consumed in push order.

For `OP_BUILD_VECTOR`, the pop loop reverses push order:

```c
for (size_t i = 0; i < d; i++) {
    popped = collection_pop(vm->operand_stack);
    buffer[d - 1 - i] = popped->data.v_float;
}
```

`buffer[d-1-i]` fills the buffer in reverse pop order, restoring push order in the final vector. If you push `[3.0, 1.0, 4.0]`, the vector is `[3.0, 1.0, 4.0]`.

---

## Memory Ownership Model

Ghost VM uses a strict manual ownership model without reference counting. Every heap allocation has exactly one owner responsible for freeing it.

**`opcodebatch` structs** (staging arrays): allocated by the caller in `main.c`, freed by `main.c` after `packager` has copied their contents into `packet.buff`.

**`packet.buff`** (the wire packet): allocated by `packager`, owned by the caller. Must remain alive until all `run_vm` calls on the parsed batches complete. Freed by the caller after VM execution.

**`parsed.batches`** (the index array): allocated by `parser`, owned by the caller. Contains pointers into `packet.buff` — freeing `parsed.batches` does not free the batch data, only the pointer array. Freed by the caller before or after `packet.buff` (both are independent allocations).

**`vm->bytecode`**: a non-owning pointer into `packet.buff`. The VM does not free it. The VM does not own it. The caller must not free `packet.buff` while the VM holds this pointer.

**`vm->operand_stack`** and all objects on it: owned by the VM. `vm_free` calls `object_free(vm->operand_stack)`, which recursively frees all objects currently on the stack, then frees the stack collection itself, then frees the `vm_t` struct.

**Object operands during arithmetic:** `run_vm` pops operands, passes them to an `object_*` function, receives a new result object. The caller (`run_vm`) owns all three. The `object_*` functions do not free their operands — operands are freed by `run_vm` after the result is pushed. This is the consistent ownership contract across all arithmetic operations.

---

## Design Decisions and Tradeoffs

### Why a Binary Protocol Instead of Text

A text protocol (JSON, s-expressions, assembly source) would be easier to debug by eye. The binary protocol was chosen because it mirrors production systems (PostgreSQL wire protocol, Redis RESP, Bloomberg's binary market data formats) and forces correct reasoning about byte layout, endianness, and alignment. The packed struct layout, the explicit hex opcodes, and the `memcpy` operand reads are all direct consequences of this choice. Each one teaches something that a text protocol would have made invisible.

### Why FNV-1a and Not CRC32

CRC32 provides better error detection properties than FNV-1a for this use case — it's designed specifically for burst error detection in data streams. FNV-1a is designed as a general-purpose hash and is used here as a checksum.

The choice of FNV-1a was made because it is trivially implementable without lookup tables (CRC32 typically uses a 256-entry precomputed table), the implementation is six lines and immediately auditable, and the constants are well-documented and verifiable. For a VM whose integrity model is accidental corruption detection rather than adversarial protection, FNV-1a is adequate.

### Why Tagged Union Instead of Vtable Dispatch

The alternative to a tagged union + switch dispatch is a vtable — a struct of function pointers that each type provides for each operation. Vtable dispatch eliminates the switch statements and makes adding new types easier (add a new vtable struct, implement the methods). Tagged union dispatch is faster for a small fixed type set (the switch compiles to a jump table) and makes the full type dispatch matrix visible in one place. With five types and four arithmetic operations, the switch approach has an advantage in auditability: every type combination and its behavior is readable in one function.

### Why `length = 0` Move Semantics Instead of Reference Counting

Reference counting (as used in CPython) solves the same problem — knowing when an object's memory can be freed — with more generality but more overhead. Every assignment increments a counter; every free decrements it; zero triggers deallocation. For a VM that never shares objects between the stack and other data structures, reference counting buys nothing over manual ownership. The `length = 0` trick is a targeted solution: it exploits the specific structure of `object_free`'s collection recursion to implement move semantics in exactly one place, without adding per-object metadata or a decrement path.

### Why Explicit Hex Opcode Values

```c
typedef enum {
    OP_HALT             = 0x00,
    OP_PUSH_INT         = 0x01,
    ...
} Opcode;
```

Without explicit assignment, the C compiler assigns sequential integer values starting from 0. Inserting a new opcode between existing ones renumbers everything after it. Any bytecode serialized before the change would decode incorrectly after it — silently, with no compile error. Explicit hex values freeze the wire format: opcode values are stable regardless of what gets added or reordered in the enum.

### The Batch Execution Model vs. Flat Bytecode

The flat bytecode model is simpler — one buffer, one `run_vm` call, one HALT at the end. The batch execution model adds complexity: each batch requires its own HALT, the VM struct needs `bytecode_len` tracking, and `main.c` must update `vm->bytecode` between calls.

The batch model was chosen because it maps directly to how networked execution would work in practice. When bytecode arrives over a socket, it arrives in packets. Each packet corresponds to a batch. Executing each batch as it arrives — rather than buffering all packets into one flat stream first — reduces latency and allows the receiver to begin execution before the full program has been transmitted. The shared operand stack is the mechanism for passing values between execution units without a separate inter-batch communication channel.

---

## File Structure

```
ghost-vm/
├── opcode.h      — wire format contract: Opcode enum, opcodebatch struct, BATCH_SIZE_MAX
├── network.h     — transport layer public API, PACKET_SIZE, ownership documentation
├── network.c     — packager, parser, emit functions, FNV-1a implementation
├── vm.h          — execution layer public API: object types, vm_t struct
├── vm.c          — object system, arithmetic engine, bytecode interpreter
└── main.c        — orchestration: emit programs, pack, parse, execute
```

`opcode.h` has no dependencies. `network.h` depends on `opcode.h`. `vm.h` depends on `opcode.h`. `network.c` depends on both `opcode.h` and `network.h`. `vm.c` depends on `opcode.h` and `vm.h`. `main.c` depends on all headers. The dependency graph is a DAG with no cycles.
