#ifndef OPCODES_H
#define OPCODES_H

#include <stdint.h>

// --- Memory & Network Protocol Limits ---
#define BATCH_SIZE_MAX 4096

// --- Virtual Machine Instruction Set ---
// Explicit hex values ensure the network bytes map perfectly across all machines
typedef enum {
    OP_HALT             = 0x00,
    OP_PUSH_INT         = 0x01,
    OP_PUSH_FLOAT       = 0x02,
    OP_PUSH_STRING      = 0x03,
    OP_BUILD_COLLECTION = 0x04,
    OP_BUILD_VECTOR     = 0x05,
    OP_ADD              = 0x06,
    OP_SUB              = 0x07,
    OP_MUL              = 0x08,
    OP_DIV              = 0x09,
    OP_PRINT            = 0x0A
} Opcode;

// --- The Core Tape Container ---
// Placed here because both the Emitters (to write) and the main loop (to route) 
// need to agree on the exact layout of this physical structure.
typedef struct __attribute__ ((packed)) {
    uint32_t batch_id;
    uint32_t write_head; // Secured as uint32_t for cross-architecture alignment
    uint8_t opcodes[];   // The Flexible Array Member
} opcodebatch;

#endif // OPCODES_H