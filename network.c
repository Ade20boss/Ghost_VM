#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "opcode.h"
#include "network.h"

#define GHST_MAGIC 0x47485354
#define FNV_PRIME 0x01000193



typedef struct __attribute__ ((packed))
{
    uint32_t magic_no;
    uint32_t checksum;
    uint32_t sequence_id;
    uint32_t batch_count;
} PacketHeader;





uint32_t hash_algorithm(uint8_t *buffer, size_t size)
{
    uint32_t hash = 0x811c9dc5;
    for(size_t i = 0; i < size; i++)
    {
        hash = (hash ^ buffer[i]) * FNV_PRIME;
    }

    return hash;
}


int emit_int(opcodebatch *batch, int value)
{
    if (batch->write_head + 5 > BATCH_SIZE_MAX)
    {
        fprintf(stderr, "LMAOO NOTHING FOR YOU BRO");
        return 1;
    }
    batch -> opcodes[batch -> write_head] = OP_PUSH_INT;
    batch -> write_head += 1;
    memcpy(&batch -> opcodes[batch -> write_head], &value, sizeof(int));
    batch -> write_head += 4;

     return 0;
}

int emit_instructions(opcodebatch *batch, uint8_t instruction)
{
    if (batch->write_head + 1 > BATCH_SIZE_MAX)
    {
        fprintf(stderr, "LMAOO NOTHING FOR YOU BRO");
        return 1;
    }

    batch -> opcodes[batch -> write_head] = instruction;
    batch -> write_head += 1;

    return 0;
}





int emit_float(opcodebatch *batch, float value)
{
    if (batch -> write_head + 5 > BATCH_SIZE_MAX)
    {
        fprintf(stderr, "LMAOO NOTHING FOR YOU BRO");
        return 1;        
    }
    batch -> opcodes[batch -> write_head] = OP_PUSH_FLOAT;
    batch -> write_head += 1;
    memcpy(&batch -> opcodes[batch -> write_head], &value, sizeof(float));
    batch -> write_head += 4;

    return 0;
}

int emit_string(opcodebatch *batch, int length, char *text)
{
    if ((batch -> write_head + 5 + length) > BATCH_SIZE_MAX)
    {
        fprintf(stderr, "LMAOO NOTHING FOR YOU BRO");
        return 1;        
    }
    batch -> opcodes[batch -> write_head] = OP_PUSH_STRING;
    batch -> write_head += 1;
    memcpy(&batch -> opcodes[batch -> write_head], &length, sizeof(int));
    batch -> write_head += 4;
    memcpy(&batch -> opcodes[batch -> write_head], text, length);
    batch -> write_head += length;

    return 0;
}

packagerResult packager(opcodebatch **array, size_t length)
{
    packagerResult result = {0, 0, 0};
    static uint32_t seq_id = 1;
    size_t required_size = sizeof(PacketHeader);
    for(size_t i = 0; i < length; i++)
    {
        required_size += (sizeof(opcodebatch) + array[i] -> write_head);
    }

    if (required_size > PACKET_SIZE)
    {
        result.error = 1;
        return result;
    }

    uint8_t *buffer = malloc(required_size);

    if(buffer == NULL)
    {
        result.error = 1;
        return result;
    }

    PacketHeader *header = (PacketHeader*)buffer;

    header -> magic_no = GHST_MAGIC;
    header -> checksum = 0x00000000;
    header -> sequence_id = seq_id;
    header -> batch_count = length;

    opcodebatch *payload = (opcodebatch*)(buffer+sizeof(PacketHeader));

    for(size_t i = 0; i < length; i++)
    {
        payload -> batch_id = array[i] -> batch_id;
        payload -> write_head = array[i] -> write_head;

        memcpy(payload -> opcodes, array[i] -> opcodes, payload -> write_head);

        payload = (opcodebatch*)((uint8_t*)payload + sizeof(opcodebatch) + array[i] -> write_head);
    }

    header -> checksum = hash_algorithm(buffer, required_size);

    seq_id++;
    result.buff = buffer;
    result.size = required_size;

    return result;

}


ParserResult parser(uint8_t *buffer, size_t size)
{

    
    ParserResult result = {0, 0, 0};
    printf("--- Packet Received ---\n\n");

    PacketHeader *header = (PacketHeader*)buffer;


    if (header -> magic_no != GHST_MAGIC)
    {
        result.error = 1;
        return result;
    }

    if (size > PACKET_SIZE)
    {
        result.error = 1;
        return result;
    }

    uint32_t old_hash = header -> checksum;
    header -> checksum = 0x00000000;
    uint32_t new_hash = hash_algorithm(buffer, size);

    header -> checksum = old_hash;

    if(new_hash != old_hash)
    {
        result.error = 1;
        return result;
    }


    opcodebatch *payload = (opcodebatch*)(buffer + sizeof(PacketHeader));


    result.batches = malloc(header -> batch_count * sizeof(opcodebatch*));
    if(result.batches == NULL)
    {
        result.error = 1;
        return result;
    }
    for (size_t i = 0; i < header -> batch_count; i++)
    {
        result.batches[i] = payload;

        payload = (opcodebatch*)((uint8_t*)payload + sizeof(opcodebatch) + payload -> write_head);

    }


    result.batch_count = header -> batch_count;
    

    return result;

}
