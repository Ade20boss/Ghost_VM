#ifndef NETWORK_H
#define NETWORK_H
#include "opcode.h"
#include <stddef.h>
#include <stdint.h>

#define PACKET_SIZE 4120


typedef struct {
    uint8_t *buff;
    size_t size;
    int error;
} packagerResult;

typedef struct {
    uint32_t batch_count;
    opcodebatch **batches;
    int error;
} ParserResult; 


// OWNERSHIP CONTRACT:
// - result.batches (the pointer array) is heap-allocated by parser.
//   The caller must free it with free(result.batches).
// - result.batches[i] (each batch pointer) points directly into the
//   'buffer' argument passed to parser. The caller must NOT free these.
//   The caller must NOT free 'buffer' until all batches have been read.
ParserResult parser(uint8_t *buffer, size_t size);


packagerResult packager(opcodebatch **array, size_t length);
uint32_t hash_algorithm(uint8_t *buffer, size_t size);
int emit_int(opcodebatch *batch, int value);
int emit_float(opcodebatch *batch, float value);
int emit_string(opcodebatch *batch, int length, char *text);
int emit_instructions(opcodebatch *batch, uint8_t instruction);



#endif