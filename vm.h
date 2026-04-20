#ifndef VM_H
#define VM_H
#include "opcode.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct Object object_t;
void object_free(object_t *obj);

//Enum for kind
typedef enum {
    INTEGER,
    FLOAT,
    STRING,
    COLLECTION,
    VECTOR,
} object_kind_t;


//Struct definition for collection kind
typedef struct{
    size_t  length; //count of objects in collection
    object_t **data; //array of object_t pointers to hold objects in the collection
    size_t capacity; //Capacity(in bytes) of the collection
    bool stack;  //Identifier if collection is a stack or a normal collection
} collection;

//Struct definition for vector kind
typedef struct {
    size_t dimensions; //
    float *coords; //Array of floats to hold dimensions information
} vector;




//Union to hold different data types(primitives, strings, collections and vectors)
typedef union {
    int v_int;
    float v_float;
    char * v_string;
    collection v_collection;
    vector v_vector;
} object_data_t;


//Struct definition for the actual object
typedef struct Object{
    object_kind_t kind;
    object_data_t data;
} object_t;

typedef struct {
    uint8_t *bytecode;
    size_t ip;
    size_t   bytecode_len;
    object_t *operand_stack;
} vm_t;


vm_t *new_virtual_machine(uint8_t *code, size_t length);
void run_vm(vm_t *vm);
void vm_free(vm_t *vm);




#endif