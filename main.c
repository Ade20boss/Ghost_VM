#include "network.h"
#include "vm.h"
#include "opcode.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int main()
{
    opcodebatch *staging_array[3];
    staging_array[0] = malloc(sizeof(opcodebatch) + 256);
    staging_array[1] = malloc(sizeof(opcodebatch) + 256);
    staging_array[2] = malloc(sizeof(opcodebatch) + 256);
    if (staging_array[0] == NULL || staging_array[1] == NULL || staging_array[2] == NULL)
    {
        fprintf(stderr, "LMAOO NOTHING FOR YOU BRO");
        return 1; 
    }
    staging_array[0] -> batch_id = 100;
    staging_array[0] -> write_head = 0;

    
    staging_array[1] -> batch_id = 101;
    staging_array[1] -> write_head = 0;

    
    staging_array[2] -> batch_id = 102;
    staging_array[2] -> write_head = 0;


    
    // Print label
    emit_string(staging_array[0], (int)strlen("2x^2 - 3x + 7, x=6:"), "2x^2 - 3x + 7, x=6:");
    emit_instructions(staging_array[0], OP_PRINT);

    // x² term: push x twice, multiply
    emit_int(staging_array[0], 6);
    emit_int(staging_array[0], 6);
    emit_instructions(staging_array[0], OP_MUL);       // stack: [36]

    // 2x²: push coefficient, multiply
    emit_int(staging_array[0], 2);
    emit_instructions(staging_array[0], OP_MUL);       // stack: [72]

    // 3x: push x and coefficient, multiply
    emit_int(staging_array[0], 6);
    emit_int(staging_array[0], 3);
    emit_instructions(staging_array[0], OP_MUL);       // stack: [72, 18]

    // 2x² - 3x: pop1=18 (top), pop2=72, result = pop2 - pop1 = 54
    emit_instructions(staging_array[0], OP_SUB);       // stack: [54]

    // + 7
    emit_int(staging_array[0], 7);
    emit_instructions(staging_array[0], OP_ADD);       // stack: [61]
    emit_instructions(staging_array[0], OP_PRINT);     // prints: 61
    emit_instructions(staging_array[0], OP_HALT);



    
    // Build vector A = [3.0, 1.0, 4.0]
    emit_float(staging_array[1], 3.0f);
    emit_float(staging_array[1], 1.0f);
    emit_float(staging_array[1], 4.0f);
    emit_instructions(staging_array[1], OP_BUILD_VECTOR);
    staging_array[1]->opcodes[staging_array[1]->write_head] = 3;
    staging_array[1]->write_head++;                    // stack: [A]

    // Build vector B = [1.0, 5.0, 9.0]
    emit_float(staging_array[1], 1.0f);
    emit_float(staging_array[1], 5.0f);
    emit_float(staging_array[1], 9.0f);
    emit_instructions(staging_array[1], OP_BUILD_VECTOR);
    staging_array[1]->opcodes[staging_array[1]->write_head] = 3;
    staging_array[1]->write_head++;                    // stack: [A, B]

    // A + B element-wise → [4.0, 6.0, 13.0]
    // pop1=B (top), pop2=A → object_add(A, B) → VECTOR+VECTOR path
    emit_instructions(staging_array[1], OP_ADD);       // stack: [[4.0, 6.0, 13.0]]
    emit_instructions(staging_array[1], OP_PRINT);     // stack: []

    // Build vector C = [2.0, 2.0, 2.0]
    emit_float(staging_array[1], 2.0f);
    emit_float(staging_array[1], 2.0f);
    emit_float(staging_array[1], 2.0f);
    emit_instructions(staging_array[1], OP_BUILD_VECTOR);
    staging_array[1]->opcodes[staging_array[1]->write_head] = 3;
    staging_array[1]->write_head++;                    // stack: [C]

    // C + scalar 10 → [12.0, 12.0, 12.0]
    // pop1=10 (INT, top), pop2=C (VECTOR) → object_add(C, 10) → VECTOR+INTEGER path
    emit_int(staging_array[1], 10);
    emit_instructions(staging_array[1], OP_ADD);       // stack: [[12.0, 12.0, 12.0]]
    emit_instructions(staging_array[1], OP_PRINT);     // stack: []
    emit_instructions(staging_array[1], OP_HALT);


    // String concatenation: "Ghost" + " VM" → "Ghost VM"
    emit_string(staging_array[2], (int)strlen("Ghost"), "Ghost");
    emit_string(staging_array[2], (int)strlen(" VM"), " VM");
    emit_instructions(staging_array[2], OP_ADD);       // STRING+STRING path in object_add
    emit_instructions(staging_array[2], OP_PRINT);     // prints: Ghost VM

    // Build collection [100, 42, 3.14, 2.71] — mixed int and float
    emit_int(staging_array[2], 100);
    emit_int(staging_array[2], 42);
    emit_float(staging_array[2], 3.14f);
    emit_float(staging_array[2], 2.71f);
    emit_instructions(staging_array[2], OP_BUILD_COLLECTION);
    staging_array[2]->opcodes[staging_array[2]->write_head] = 4;
    staging_array[2]->write_head++;                    // stack: [{100, 42, 3.14, 2.71}]
    emit_instructions(staging_array[2], OP_PRINT); // prints the collection

    // Circle area: π × r², r = 5
    // push 5, push 5, MUL → 25 (INT×INT)
    // then push π, MUL → π × 25 (FLOAT×INT)
    emit_int(staging_array[2], 5);
    emit_int(staging_array[2], 5);
    emit_instructions(staging_array[2], OP_MUL);       // stack: [25]
    emit_float(staging_array[2], 3.14159f);
    emit_instructions(staging_array[2], OP_MUL);       // stack: [~78.54]
    emit_instructions(staging_array[2], OP_PRINT);     // prints: ~78.539749

    emit_instructions(staging_array[2], OP_HALT);

    packagerResult packet = packager(staging_array, 3);
    if (packet.error == 1)
    {
        fprintf(stderr, "LMAOO NOTHING FOR YOU BRO");
        free(staging_array[0]); free(staging_array[1]); free(staging_array[2]);
        return 1; 
    }
    ParserResult parsed = parser(packet.buff, packet.size);
    if (parsed.error == 1)
    {
        fprintf(stderr, "LMAOO NOTHING FOR YOU BRO");
        free(staging_array[0]); free(staging_array[1]); free(staging_array[2]);
        return 1; 
    }

    printf("=== EXECUTING KERNEL STRESS TEST ===\n");
    vm_t *vm = new_virtual_machine(parsed.batches[0]->opcodes, parsed.batches[0] -> write_head);
    run_vm(vm);
    printf("BATCH 0 COMPLETE\n");
    printf("\n");

    for (uint32_t i = 1; i < parsed.batch_count; i++)
    {

        vm -> bytecode = parsed.batches[i] -> opcodes;
        vm -> bytecode_len = parsed.batches[i] -> write_head;
        vm -> ip = 0;
        run_vm(vm);
        printf("BATCH %d COMPLETE\n", i);
        printf("\n");
        
    }

    free(parsed.batches);
    free(packet.buff);
    free(staging_array[0]);
    free(staging_array[1]);
    free(staging_array[2]);

    
    
    


    


    vm_free(vm);
    printf("=== VIRTUAL MACHINE TERMINATED ===\n");

    return 0;


}