# 👻 Ghost VM — A Tiny Dynamically-Typed Virtual Machine in C

> *"Why use Python when you can spend 3 weeks writing C to do the same thing, but cooler?"*
> — probably you, at some point during this project

---

## 🧠 What Is This?

This is a **hand-rolled, stack-based virtual machine** written in pure C, complete with a polymorphic object system that supports integers, floats, strings, collections, and vectors. It's the kind of thing you build when you want to understand how languages actually work under the hood — or when you just really, really don't want to use Python's `list`.

Under the hood, there are two main systems working together:

1. **The Object System** — A tagged union that can hold any of the supported data types, with a full suite of operations (add, subtract, multiply, divide, clone, compare, free).
2. **The Virtual Machine** — A bytecode interpreter with its own operand stack, instruction pointer, and opcode dispatch loop.

Together, they form a surprisingly capable little runtime. It's not going to replace the JVM. But it *will* impress your systems professor.

---

## 🗂️ Project Structure

Everything lives in one C file because we are brave (or slightly chaotic). Here's a mental map:

```
ghost_vm.c
│
├── Types & Structs
│   ├── object_kind_t      → enum: INTEGER | FLOAT | STRING | COLLECTION | VECTOR
│   ├── object_data_t      → union: holds whichever kind of data the object is
│   ├── object_t           → the actual object (kind + data)
│   ├── collection         → dynamic array of object_t pointers (doubles as a stack)
│   ├── vector             → N-dimensional float array
│   └── vm_t               → the virtual machine itself (bytecode + ip + operand stack)
│
├── Object Constructors
│   ├── new_object_integer(int)
│   ├── new_object_float(float)
│   ├── new_object_string(char*)
│   ├── new_object_vector(size_t, float*)
│   └── new_object_collection(size_t capacity, bool is_stack)
│
├── Collection Operations
│   ├── collection_append()    → push to back
│   ├── collection_pop()       → remove from back
│   ├── collection_access()    → index-based read (non-stack only)
│   ├── collection_set()       → index-based write
│   ├── stack_peek()           → look at top without removing
│   ├── is_empty()             → 1 if empty, 0 if not, -1 if you passed nonsense
│   └── is_full()              → checks capacity
│
├── Object Operations (Polymorphic)
│   ├── object_add()           → +  (integers, floats, strings, collections, vectors)
│   ├── object_subtract()      → -  (integers, floats, stack-collections, vectors)
│   ├── object_multiply()      → *  (integers, floats, strings, collections, vectors)
│   ├── object_divide()        → /  (integers, floats, vectors)
│   ├── object_equals()        → deep equality check
│   ├── object_clone()         → deep copy
│   └── object_free()          → recursive destructor
│
├── Virtual Machine
│   ├── new_virtual_machine()  → allocates VM, sets up operand stack
│   └── run_vm()               → the main fetch-decode-execute loop
│
└── main()                     → demo program (builds a 3D vector, adds a scalar, prints it)
```

---

## 🧩 The Object System

### The Tagged Union Pattern

The whole object system is built on C's `union` — a memory region that can be interpreted as different types depending on context. We use an `enum` tag (the `kind` field) to track what's actually stored.

```c
typedef struct Object {
    object_kind_t kind;   // what type is this?
    object_data_t data;   // the actual data (interpreted based on kind)
} object_t;
```

This is essentially what dynamically-typed languages do internally. Python objects, Ruby objects, JavaScript values — they're all tagged unions in disguise. We just wrote ours in C where there's no safety net and the floor is made of segfaults.

### Supported Types

| Kind | C Type | Notes |
|------|--------|-------|
| `INTEGER` | `int` | Whole numbers. Classic. |
| `FLOAT` | `float` | Decimal numbers. Slightly treacherous. |
| `STRING` | `char *` | Heap-allocated, null-terminated. |
| `COLLECTION` | `collection` | Dynamic array. Can behave as a list or a stack. |
| `VECTOR` | `vector` | N-dimensional float coordinates. |

### Collections — The Swiss Army Knife

The `collection` type is doing double duty:

- As a **list**: supports indexed access, append, set, clone
- As a **stack**: supports push (append), pop, peek

The `bool stack` field on the struct is the flag that determines which mode it's in. Try calling `collection_access()` on a stack and you'll get a very stern `fprintf(stderr, ...)`.

The VM's **operand stack** is itself a `collection` with `stack = true`. So yes — the object system eats its own cooking.

### Memory Management — You Are The GC

There is no garbage collector here. You `malloc`, you `free`. The `object_free()` function is your best friend:

```c
void object_free(object_t *obj);
```

It's recursive — freeing a `COLLECTION` will free all its children. Freeing a `STRING` will free its heap-allocated string. Freeing a `VECTOR` will free its coords array. And then it frees the object shell itself.

> ⚠️ **Double-free danger zone:** The `object_add()` function for collections uses **move semantics**. It transfers ownership of items from `a` and `b` into a new collection, then sets their lengths to 0 before freeing the shells. This is intentional and documented in the source. Do not remove those lines. Do not look at those lines funny. Just trust them.

---

## ⚙️ Polymorphic Operations

Every arithmetic operation is overloaded by `kind`. Here's a quick reference for what works with what:

### `object_add(a, b)`

| a \ b | INTEGER | FLOAT | STRING | COLLECTION | VECTOR |
|-------|---------|-------|--------|------------|--------|
| **INTEGER** | ✅ int | ✅ float | ❌ | ❌ | ❌ |
| **FLOAT** | ✅ float | ✅ float | ❌ | ❌ | ❌ |
| **STRING** | ❌ | ❌ | ✅ concat | ❌ | ❌ |
| **COLLECTION** | ❌ | ❌ | ❌ | ✅ merge* | ❌ |
| **VECTOR** | ✅ broadcast | ✅ broadcast | ❌ | ❌ | ✅ elementwise |

*Collection merge uses move semantics. `a` and `b` are consumed.

### `object_multiply(a, b)`

Strings and collections support **repetition** — `"ha" * 3` gives you `"hahaha"`, and `[1, 2] * 3` gives you `[1, 2, 1, 2, 1, 2]`. Just like Python. Except you wrote it yourself in C. Feel good about that.

### `object_subtract(a, b)` on stacks

Subtracting two stack-typed collections pops matching items off the top of `a`. Both must be stacks, and the top items of `a` must equal the items of `b`. It's niche, but it's there.

---

## 🖥️ The Virtual Machine

### Architecture

The VM is a classic **stack machine**:

- **Bytecode array** (`size_t *`) — the program to execute
- **Instruction Pointer** (`ip`) — points to the current instruction
- **Operand Stack** — an `object_t` collection where instructions push and pop their operands

No registers. No frames (yet). Just a stack and a loop.

### Opcodes

| Opcode | Operand(s) | Description |
|--------|-----------|-------------|
| `OP_PUSH_INT` | `int value` | Pushes an integer object onto the stack |
| `OP_PUSH_FLOAT` | `size_t raw_bits` | Pushes a float (encoded as raw bits) onto the stack |
| `OP_PUSH_STRING` | `size_t ptr` | Pushes a string object (from a raw char* cast) |
| `OP_BUILD_COLLECTION` | `size_t n` | Pops `n` items, builds a collection, pushes it |
| `OP_BUILD_VECTOR` | `size_t d` | Pops `d` numeric items, builds a `d`-dimensional vector |
| `OP_ADD` | — | Pops 2, adds, pushes result |
| `OP_SUB` | — | Pops 2, subtracts, pushes result |
| `OP_MUL` | — | Pops 2, multiplies, pushes result |
| `OP_DIV` | — | Pops 2, divides, pushes result |
| `OP_PRINT` | — | Pops 1, prints it, frees it |
| `OP_HALT` | — | Stops the VM |

### Float Encoding (The Spooky Part)

Floats are passed into the bytecode array as raw bit patterns reinterpreted as `size_t`. This is done via the `*(size_t*)&f` pattern:

```c
float f = 10.5f;
size_t encoded = *(size_t*)&f;  // reinterpret the bits, don't convert the value
```

And decoded inside the VM like so:

```c
size_t raw_bits = vm->bytecode[vm->ip];
float value = *(float*)&raw_bits;
```

This is technically a **type-punning** trick. It is valid in C via `memcpy` semantics, and works correctly here. Is it elegant? No. Does it work? Yes. Will it confuse anyone reading it for the first time? Absolutely.

---

## 🚀 Running the Demo

Compile and run:

```bash
gcc -o ghost ghost_vm.c -Wall -Wextra
./ghost
```

Expected output (from the `main()` demo):

```
--- VM BOOT SEQUENCE INITIATED ---
<12.000000,22.000000,32.000000>
--- VM HALTED ----
```

What it did:
1. Pushed floats `10.0`, `20.0`, `30.0` onto the stack
2. Built a 3D vector `<10, 20, 30>`
3. Pushed float `2.0`
4. Added `2.0` to the vector (broadcast scalar addition) → `<12, 22, 32>`
5. Printed and halted

---

## 🐛 Known Behaviors Worth Knowing

- **Stack underflow** is checked at the VM level before each arithmetic op. You'll get a `fprintf(stderr, ...)` and an early return. The VM doesn't crash silently.
- **`is_empty()` checks `NULL` after checking `kind`** — the null check should come first. This is a minor ordering quirk in the current implementation.
- **`object_clone()` doesn't handle `VECTOR` yet** — if you try to clone a vector kind, you'll get `NULL`. Something to add next.
- **`object_equals()` doesn't handle `VECTOR` or `FLOAT` precision** — float equality is compared with `==`, which is fine for exact matches but will bite you with computed values.

---

## 🔭 What Could Come Next

If you want to extend this into something more serious, here's a natural roadmap:

- [ ] **Variable store** — a hash map from names to `object_t *`
- [ ] **`OP_LOAD` / `OP_STORE`** — load/store variables from/to the store
- [ ] **`OP_JUMP` / `OP_JUMP_IF`** — control flow
- [ ] **Call frames** — support for function calls with local scopes
- [ ] **A bytecode compiler** — take source text, emit opcodes
- [ ] **Fix `object_clone()` for vectors**
- [ ] **A proper REPL**

---

## 🏗️ Design Philosophy

This codebase makes one big bet: **the object is the unit of everything**. Every value the VM touches is an `object_t`. The VM's stack is an `object_t`. Collections hold `object_t` pointers. Operations take `object_t` pointers and return `object_t` pointers.

This makes the architecture extremely uniform. Adding a new type means:
1. Add it to the `enum`
2. Add it to the `union`
3. Write a constructor
4. Handle it in each operation's `switch`
5. Handle it in `object_free()` and `object_clone()`

It's verbose. It's explicit. It's C. Welcome.

---

## 📜 License

Do whatever you want with this. Learn from it, break it, extend it, ship it. Just don't blame me when you get a segfault at 2am. That's between you and Valgrind.

---

*Built with stubbornness, `malloc`, and a deep suspicion of garbage collectors.*License: MIT (Go wild).
