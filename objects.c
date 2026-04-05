#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

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

// ======= VIRTUAL MACHINE ARCHITECTURE =======
typedef enum {
    OP_PUSH_INT, //Push an integer unto vm stack
    OP_PUSH_STRING, //Push a string unto vm  stack
    OP_BUILD_COLLECTION, //Build a collection from items in the vm stack
    OP_BUILD_VECTOR, //Build a vector from items in the vm stack
    OP_ADD,      //Pop two objects, add them, push result to vm stack
    OP_SUB,      //Pop two objects, subtract them, push result to vm stack
    OP_MUL,      //Pop two objects, multiply them, push result to vm stack
    OP_DIV,      //Pop two objects, divide them, oush result to vm stack
    OP_PRINT,    //Pop an item and print it
    OP_HALT,     //Stop execution
    OP_PUSH_FLOAT
} OpCode;

typedef struct {
    size_t *bytecode;
    size_t ip;
    object_t *operand_stack;
} vm_t;



//Integer object constructor
object_t *new_object_integer(int value){
    //Allocate enough memory for an object
    object_t *new_obj = malloc(sizeof(object_t));
    //check if memory allocation fails
    if (new_obj == NULL){
        return NULL;
    }

    //assign kind and actual integer value
    new_obj -> kind = INTEGER;
    new_obj -> data.v_int = value;

    return new_obj;
}

//Float object constructor
object_t *new_object_float(float value){
    //check above function, we're essentialy doing the same thing
   object_t *new_obj = (object_t *)malloc(sizeof(object_t));
   if (new_obj == NULL){
        return NULL;
   }
   new_obj -> kind = FLOAT;
   new_obj -> data.v_float = value;

   return new_obj;
}


object_t *new_object_string(char *value){
    //Allocate enough memory for object
   object_t *new_obj = malloc(sizeof(object_t));
   //check if memory allocation fails
   if (new_obj == NULL){
        return NULL;
   }
   //assign data kind
   new_obj -> kind = STRING;
   //allocate enough memory for string and the null terminator
   new_obj -> data.v_string = malloc(strlen(value) + 1);
   //check if memory allocation fails
   if(new_obj -> data.v_string == NULL){
        free(new_obj);
        return NULL;
   }
   //copy passed string into object string field
   strcpy(new_obj -> data.v_string, value);

   return new_obj;
}

object_t *new_object_vector(size_t dimens, float *coords){
    object_t *new_object = malloc(sizeof(object_t));
    if (new_object == NULL){
        return NULL;
    }

    new_object -> kind = VECTOR;
    new_object -> data.v_vector.dimensions = dimens;
    new_object -> data.v_vector.coords = malloc(dimens * sizeof(float));

    if (new_object -> data.v_vector.coords == NULL){
        free(new_object);
        return NULL;
    }

    memcpy(new_object -> data.v_vector.coords, coords, sizeof(float) * dimens);

    return new_object;
}

object_t *new_object_collection(size_t capacity, bool is_stack) {
    //check if capacity is 0
    if (capacity == 0){
        fprintf(stderr, "Cannot initialize collection kind with 0 capacity\n");
        return NULL;
    }
    //allocate memory for object
    object_t *new_obj = malloc(sizeof(object_t));

    //check if memory allocation fails
    if(new_obj == NULL){
        return NULL;
    }

    //set metadata for the new object collection
    new_obj -> kind = COLLECTION;
    new_obj -> data.v_collection.length = 0;
    new_obj -> data.v_collection.stack = is_stack;
    new_obj -> data.v_collection.capacity = capacity;

    //allocate memory 
    new_obj -> data.v_collection.data = calloc(capacity, sizeof(object_t *));

    if (new_obj -> data.v_collection.data == NULL){
        free(new_obj);
        return NULL;
    }
    return new_obj;

}

int object_length(object_t *obj){
    if (obj == NULL){
        fprintf(stderr, "Cannot perform operation on null parameters\n");
        return -1;
    }
    switch (obj -> kind){
        case INTEGER:
            fprintf(stderr, "Cannot perform operation on Object of kind INTEGER\n");
            return -1;
        case FLOAT:
            fprintf(stderr, "Cannot perform operation on Object of kind FLOAT\n");
            return -1;
        case STRING:
            return strlen(obj -> data.v_string);
        case COLLECTION:
            return  obj -> data.v_collection.length;
        default:
            fprintf(stderr, "Error: Unknown object kind detected\n");
            return -1;
    }

}



int collection_append(object_t *collection, object_t *item){
    if(collection == NULL || item == NULL){
        fprintf(stderr, "Cannot perform operation on null object\n");
        return -1;
    }
    if (collection -> kind != COLLECTION){
        fprintf(stderr, "Error: Can't perform append operation on non_collection kind\n");
        return -1;
    }
    if (collection -> data.v_collection.capacity == collection -> data.v_collection.length){
        size_t new_cap = collection -> data.v_collection.capacity * 2;
        object_t **temp = realloc(collection -> data.v_collection.data, sizeof(object_t *) * new_cap);

        if (temp == NULL){
            return -1;
        }

        collection -> data.v_collection.data = temp;
        collection -> data.v_collection.capacity = new_cap;

    }
    collection -> data.v_collection.data[collection -> data.v_collection.length] = item;

    collection -> data.v_collection.length++;
    

    return 0;
}

int collection_set(object_t *collection, size_t index, object_t *value){
    if (collection == NULL || value == NULL){
        fprintf(stderr, "Unable to perform operation with null values\n");
        return -1;
    }

    if (collection -> kind != COLLECTION){
        fprintf(stderr, "Cannot perform operation on non_collection kind\n");
        return -1;
    }

    if (index >= collection -> data.v_collection.length){
        fprintf(stderr, "Index specified is out of bounds\n");
        return -1;
    }

    object_free(collection -> data.v_collection.data[index]);

    collection -> data.v_collection.data[index] = value;
    return 0;
}


object_t *collection_access(object_t *collection, size_t index){
    if (collection == NULL){
        fprintf(stderr, "Unable to perform operation with null values\n");
        return NULL;
    }

    if (collection -> kind != COLLECTION){
        fprintf(stderr, "Cannot perform operation on non_collection kind\n");
        return NULL;
    }

    if (collection -> data.v_collection.stack == true){
        fprintf(stderr, "Cannot perform operation on stack kind\n");
        return NULL;
    }

    if (index >= collection -> data.v_collection.length){
        fprintf(stderr, "Index specified is out of bounds\n");
        return NULL;
    }

    return collection -> data.v_collection.data[index];

}


int is_empty(object_t *collection_stack){
    if (collection_stack -> kind != COLLECTION){
        fprintf(stderr, "Cannot perform empty function on non_collection kind");
        return -1;
    }
    if (collection_stack == NULL){
        fprintf(stderr, "Cannot perform empty function on null object\n");
        return -1;
    }

    if (collection_stack -> data.v_collection.stack == false){
        fprintf(stderr, "Cannot perform empty function on non_stack kind");
        return -1;
    }

    if (collection_stack -> data.v_collection.length == 0){
        return 1;
    }
    return 0;
}


object_t *collection_pop(object_t *collection){
    if (collection == NULL){
        return NULL;
    }   
    if (collection->kind != COLLECTION) {
        fprintf(stderr, "Error: Cannot pop from non-collection\n");
        return NULL;
    }

    if (collection -> data.v_collection.length == 0){
        fprintf(stderr, "Error: Cannot pop from empty collection\n");
        return NULL;
    }

    size_t top_index = collection -> data.v_collection.length - 1;
    object_t *popped_item = collection -> data.v_collection.data[top_index];

    collection -> data.v_collection.data[top_index] = NULL;
    collection -> data.v_collection.length--;

    return popped_item;
}

object_t *stack_peek(object_t *collection){
    if (collection == NULL){
        return NULL;
    }

    if (collection -> kind != COLLECTION) {
        fprintf(stderr, "Error: Cannot peek from non-collection\n");
        return NULL;
    }

    if ( collection -> data.v_collection.stack == false){
        fprintf(stderr, "Cannot perform peek operation on non_stack kind");
        return NULL;
    }

    if (collection -> data.v_collection.length == 0){
        fprintf(stderr, "Error: Cannot peek from empty collection\n");
        return NULL;
    }

    return collection -> data.v_collection.data[collection -> data.v_collection.length - 1];



}


void object_free(object_t *obj){
    if (obj == NULL){
        return;
    }
    
    else if (obj -> kind == STRING){
        free(obj -> data.v_string);
    }
    else if(obj -> kind == COLLECTION){
        for (size_t i = 0; i < obj -> data.v_collection.length; i++){
            object_free(obj -> data.v_collection.data[i]);
        }
        free(obj -> data.v_collection.data);
    }
    else if (obj -> kind == VECTOR){
        free(obj -> data.v_vector.coords);
    }

    free(obj);

}




object_t *object_add(object_t *a, object_t *b){
        /**
     * @brief Performs a polymorphic addition or collection merge.
     *
     * @warning **DESTRUCTIVE OPERATION FOR COLLECTIONS (MOVE SEMANTICS)**
     * When adding two objects of kind COLLECTION, this function takes OWNERSHIP
     * of the internal data of both 'a' and 'b'.
     *
     * @details
     * - **Primitives (INT/FLOAT):** Returns a new object. 'a' and 'b' remain valid.
     * - **Strings:** Returns a new string. 'a' and 'b' remain valid.
     * - **Collections:** * 1. A new collection is created containing pointers to ALL items from 'a' and 'b'.
     * 2. The items are **moved**, not copied. 
     * 3. The input shells 'a' and 'b' are **FREED** and invalid after return.
     *
     * @param a The first operand. Consumed if COLLECTION.
     * @param b The second operand. Consumed if COLLECTION.
     * @return object_t* A new object containing the result, or NULL on failure.
     */
    if (a == NULL || b == NULL){
        fprintf(stderr, "Cannot perform operation on Null data\n");
        return NULL;
    }
    
    switch (a -> kind){
        case INTEGER:
            switch (b -> kind){
                case INTEGER:
                    return new_object_integer(a -> data.v_int + b -> data.v_int);
                case FLOAT:
                    return new_object_float((float)a -> data.v_int + b -> data.v_float);
                default:
                    fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                    return NULL;
            }
        case FLOAT:
            switch (b -> kind){
                case INTEGER:
                    return new_object_float(a -> data.v_float + (float)b -> data.v_int);
                case FLOAT:
                    return new_object_float(a -> data.v_float + b -> data.v_float);
                default:
                    fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                    return NULL;
            }

        case STRING:{
            if (b -> kind != STRING){
                fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                return NULL;  
            }
            size_t length = strlen(a -> data.v_string) + strlen(b -> data.v_string) + 1;

            char *new_string = calloc(length, sizeof(char));
            if(new_string == NULL){
                return NULL;
            }

            strcat(new_string, a -> data.v_string);
            strcat(new_string, b -> data.v_string);

            object_t *newstring = new_object_string(new_string);
            free(new_string);
            return newstring;
        }
        case COLLECTION:
            if (b -> kind != COLLECTION){
               fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
               return NULL;          
            }

            if (a->data.v_collection.stack || b->data.v_collection.stack){
                fprintf(stderr, "object_add: can only concatenate non-stack collections (lists)\n");
                return NULL;
            }

            size_t total_length = a -> data.v_collection.length + b -> data.v_collection.length;

            size_t capacity = (total_length > 0) ? total_length : 1;
            object_t *new_collection = new_object_collection(capacity, false);

            for (size_t i = 0; i < a -> data.v_collection.length; i++){
                collection_append(new_collection, a -> data.v_collection.data[i]);
            }
            for (size_t j = 0; j < b -> data.v_collection.length; j++){
                collection_append(new_collection, b -> data.v_collection.data[j]);
            }
            // [DO NOT TOUCH] PREVENT RECURSIVE DESTRUCTION
            // We set lengths to 0 so object_free() destroys the array wrapper
            // but does NOT touch the items (which now belong to new_collection).
            // Removing this line causes Double Free errors.
            a -> data.v_collection.length = 0;
            b -> data.v_collection.length = 0;
            return new_collection;
        case VECTOR:{
            switch(b -> kind){
                case INTEGER:{
                    float scalar = (float)b -> data.v_int;
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] + scalar;
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);

                    return new_vector;
                }

                case FLOAT:{
                    float scalar = b -> data.v_float;
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] + scalar;
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);
                    return new_vector;

                }
                case VECTOR:{
                    if (a -> data.v_vector.dimensions != b -> data.v_vector.dimensions){
                        fprintf(stderr, "Cannot perform element wise addition on vectors in different dimenstions");
                        object_free(a);
                        object_free(b);
                        return NULL;
                    }
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] + b -> data.v_vector.coords[i];
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);

                    return new_vector;

                }
                default:
                    fprintf(stderr,"Incompatible kinds");
                    object_free(a);
                    object_free(b);
                    return NULL;

            }
        }
            
        default: return NULL;
    }


}

bool object_equals(object_t *a, object_t *b){
    if (a == NULL || b == NULL){
        return false;
    }
    
    if (a -> kind != b -> kind){
        return false;
    }
    switch (a -> kind){
        case INTEGER:
            if(a -> data.v_int == b -> data.v_int){
                return true;
            }
            else{
                return false;
            }
        case FLOAT:
            if(a -> data.v_float == b -> data.v_float){
                return true;
            }
            else{
                return false;
            }
        case STRING:
            if(strcmp(a -> data.v_string,  b -> data.v_string) == 0){
                return true;
            }
            else {
                return false;
            }
        case COLLECTION:
            if (b -> data.v_collection.length != a -> data.v_collection.length){
                return false;
            }
            else{
                for (size_t i = 0; i < b -> data.v_collection.length; i++){
                    object_t *a_check = a -> data.v_collection.data[i];
                    object_t *b_check = b -> data.v_collection.data[i];

                    if (object_equals(a_check, b_check) == false){
                        return false;
                    }
                    
                }
                return true;
            }
        default:
            return false;

    }
    return false;
}

object_t *object_clone(object_t *obj){
    if (obj == NULL){
        fprintf(stderr, "Cannot perform operation on Null data\n");
        return NULL;   
    }
    switch(obj -> kind){
        case INTEGER:
            return new_object_integer(obj -> data.v_int);
        case FLOAT:
            return new_object_float(obj -> data.v_float);
        case STRING:
            return new_object_string(obj -> data.v_string);
        case COLLECTION:{
            object_t *collection_clone = new_object_collection(obj -> data.v_collection.capacity, obj -> data.v_collection.stack);

            for (size_t i = 0; i < obj -> data.v_collection.length; i++){
                collection_append(collection_clone, object_clone(obj -> data.v_collection.data[i]));
            }
            return collection_clone;
        }
        default:
            return NULL;
            
    }
    return NULL;
}

object_t *object_subtract(object_t *a, object_t *b){
     if (a == NULL || b == NULL){
        fprintf(stderr, "Cannot perform operation on Null data\n");
        return NULL;
    }
    switch(a -> kind){
        case INTEGER:
            switch (b -> kind){
                case INTEGER:
                    return new_object_integer(a -> data.v_int - b -> data.v_int);
                case FLOAT:
                    return new_object_float((float)a -> data.v_int - b -> data.v_float);
                default:
                    fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                    return NULL;
            }
        case FLOAT:
            switch (b -> kind){
                case INTEGER:
                    return new_object_float(a -> data.v_float - (float)b -> data.v_int);
                case FLOAT:
                    return new_object_float(a -> data.v_float - b -> data.v_float);
                default:
                    fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                    return NULL;
            }
        case STRING:
            fprintf(stderr, "Cannot perform subtraction operation on String kind");
            return NULL;

        case COLLECTION:
             if (b -> kind != COLLECTION){
               fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
               return NULL;          
            }

            if (a -> data.v_collection.stack == false ||b -> data.v_collection.stack == false ){
               fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
               return NULL;  

            }

            if (b -> data.v_collection.length >  a -> data.v_collection.length){
                fprintf(stderr, "Subtraction Underflow error");
                return NULL;
            }
            for (size_t i = 0; i < b -> data.v_collection.length; i++){
                object_t *a_test = a -> data.v_collection.data[a -> data.v_collection.length - 1 - i];
                object_t *b_test = b -> data.v_collection.data[i];
                if (object_equals(a_test, b_test) == false){
                    return NULL;
                }
            }
            for (size_t i = 0; i < b -> data.v_collection.length; i++){
                object_t *popped_item = collection_pop(a);
                object_free(popped_item);
            }
            
            return a;
         case VECTOR:{
            switch(b -> kind){
                case INTEGER:{
                    float scalar = (float)b -> data.v_int;
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] - scalar;
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);


                    return new_vector;
                }

                case FLOAT:{
                    float scalar = b -> data.v_float;
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] - scalar;
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);


                    return new_vector;

                }
                case VECTOR:{
                    if (a -> data.v_vector.dimensions != b -> data.v_vector.dimensions){
                        fprintf(stderr, "Cannot perform element wise subtraction on vectors in different dimenstions");
                        object_free(a);
                        object_free(b);
                        return NULL;
                    }
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] - b -> data.v_vector.coords[i];
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);

                    return new_vector;

                }
                default:
                    fprintf(stderr,"Incompatible kinds");

                    return NULL;

            }
        }
            
        default:
            return NULL;
    }
    return NULL;
}


object_t *object_multiply(object_t *a, object_t *b){
    if (a == NULL || b == NULL){
        fprintf(stderr, "Cannot perform operation on Null data\n");
        return NULL;
    }
    switch(a -> kind){
        case INTEGER:
            switch (b -> kind){
                case INTEGER:
                    return new_object_integer(a -> data.v_int * b -> data.v_int);
                case FLOAT:
                    return new_object_float((float)a -> data.v_int * b -> data.v_float);
                default:
                    fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                    return NULL;
            }
        case FLOAT:
            switch (b -> kind){
                case INTEGER:
                    return new_object_float(a -> data.v_float * (float)b -> data.v_int);
                case FLOAT:
                    return new_object_float(a -> data.v_float * b -> data.v_float);
                default:
                    fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                    return NULL;
            }
        case STRING:{
            if (b -> kind !=  INTEGER || b -> data.v_int <= 0){
                fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                return NULL;  
            }
            size_t chunk_size = strlen(a -> data.v_string);
            size_t offset = 0;
            size_t length =chunk_size * (b -> data.v_int) + 1;


            char *new_string = calloc(length, sizeof(char));
            if(new_string == NULL){
                return NULL;
            }
            for (size_t i = 0; i < b -> data.v_int; i++){
                 strcpy(new_string + offset, a -> data.v_string);
                 offset = offset + chunk_size;
            }

            object_t *newstring = new_object_string(new_string);
            free(new_string);
            return newstring;
        }
        case COLLECTION:{
            if (b -> kind !=  INTEGER || b -> data.v_int <= 0){
                fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                return NULL;  
            }

            size_t cap = a -> data.v_collection.length * b -> data.v_int;
            object_t *new_collection = new_object_collection(cap, a -> data.v_collection.stack);

            for (size_t i = 0; i < b -> data.v_int; i++){
                for (size_t j =0; j < a -> data.v_collection.length; j++){
                    collection_append(new_collection, object_clone(a -> data.v_collection.data[j]));
                }
            }
            return new_collection;
        }

         case VECTOR:{
            switch(b -> kind){
                case INTEGER:{
                    float scalar = (float)b -> data.v_int;
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] * scalar;
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);

                    return new_vector;
                }

                case FLOAT:{
                    float scalar = b -> data.v_float;
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] * scalar;
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);


                    return new_vector;

                }
                case VECTOR:{
                    if (a -> data.v_vector.dimensions != b -> data.v_vector.dimensions){
                        fprintf(stderr, "Cannot perform element wise multiplication on vectors in different dimenstions");

                        return NULL;
                    }
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] * b -> data.v_vector.coords[i];
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);

                    return new_vector;

                }
                default:
                    fprintf(stderr,"Incompatible kinds");

                    return NULL;

            }
        }
            
        default:
            return NULL;
    }
}

object_t *object_divide(object_t *a, object_t *b){
    if (a == NULL || b == NULL){
        fprintf(stderr, "Cannot perform operation on Null data\n");
        return NULL;
    }
    switch(a -> kind){
        case INTEGER:
            switch (b -> kind){
                case INTEGER:
                    if (b -> data.v_int == 0){
                        fprintf(stderr, "VM ERROR: Division by zero");
                        return NULL;
                    }
                    return new_object_integer(a -> data.v_int / b -> data.v_int);
                case FLOAT:
                    if (b -> data.v_float == 0){
                        fprintf(stderr, "VM ERROR: Division by zero");
                        return NULL;
                    }
                    return new_object_float((float)a -> data.v_int / b -> data.v_float);
                default:
                    fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                    return NULL;
            }
        case FLOAT:
            switch (b -> kind){
                case INTEGER:
                    if (b -> data.v_int == 0){
                        fprintf(stderr, "VM ERROR: Division by zero");
                        return NULL;
                    }
                    return new_object_float(a -> data.v_float / (float)b -> data.v_int);
                case FLOAT:
                    if (b -> data.v_float == 0){
                        fprintf(stderr, "VM ERROR: Division by zero");
                        return NULL;
                    }
                    return new_object_float(a -> data.v_float / b -> data.v_float);
                default:
                    fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
                    return NULL;
            }
         case VECTOR:{
            switch(b -> kind){
                case INTEGER:{
                    if (b -> data.v_int == 0){
                        fprintf(stderr, "Division by zero error");

                        return NULL;
                    }
                    float scalar = (float)b -> data.v_int;
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] / scalar;
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);

                    return new_vector;
                }

                case FLOAT:{
                    if (b -> data.v_float == 0.0){
                        fprintf(stderr, "Division by zero error");

                        return NULL;
                    }
                    float scalar = b -> data.v_float;
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] / scalar;
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);


                    return new_vector;

                }
                case VECTOR:{
                    if (a -> data.v_vector.dimensions != b -> data.v_vector.dimensions){
                        fprintf(stderr, "Cannot perform element wise division on vectors in different dimenstions");
                        return NULL;
                    }
                    for (size_t j = 0; j < a -> data.v_vector.dimensions; j++){
                        if (b -> data.v_vector.coords[j] == 0.0){
                            fprintf(stderr, "Division by zero error");
                            return NULL;
                        }
                    }
                    float buffer[a -> data.v_vector.dimensions];

                    for (size_t i = 0; i < a -> data.v_vector.dimensions; i++){
                        buffer[i] = a -> data.v_vector.coords[i] / b -> data.v_vector.coords[i];
                    }

                    object_t *new_vector = new_object_vector(a -> data.v_vector.dimensions, buffer);
                    return new_vector;

                }
                default:
                    fprintf(stderr,"Incompatible kinds");
                    return NULL;

            }
        }
            
        case STRING:
            fprintf(stderr, "Cannot perform division operation on string kind");
            return NULL;
        case COLLECTION:
            fprintf(stderr, "Cannot perform division operation on Collection kind");
            return NULL;

        default:
            fprintf(stderr, "Cannot perform operation on incompatible kinds\n");
            return NULL;    

    }
}




void print_object(object_t *obj1){
    if (obj1 == NULL){
        printf("NULL");
        return;
    }

    switch(obj1 -> kind){
        case INTEGER:
            printf("%d", obj1 -> data.v_int);
            break;
        case FLOAT:
            printf("%f", obj1 -> data.v_float);
            break;
        case STRING:
            printf("%s", obj1 -> data.v_string);
            break;
        case COLLECTION:
            printf("[");
            for (size_t i = 0; i < obj1 -> data.v_collection.length; i++){
                print_object(obj1 -> data.v_collection.data[i]);
                if (i < obj1 -> data.v_collection.length - 1){
                      printf(", ");
                }

            }
            printf("]\n");
            break;
        case VECTOR:
            printf("<");
            for (size_t j = 0; j < obj1 -> data.v_vector.dimensions; j++){
                printf("%f", obj1 -> data.v_vector.coords[j]);
                if (j < obj1 -> data.v_vector.dimensions - 1){
                    printf(",");
                }
            }
            printf(">\n");
            break;
    }

}

void print_collection_data(object_t *obj){
    if (obj -> kind != COLLECTION){
        fprintf(stderr, "Cannot print data of non_collection kind");
        return;
    }

    printf("Collection capacity: %zu\n", obj -> data.v_collection.capacity);
    printf("Number of items in collection: %zu\n", obj -> data.v_collection.length);

}


int is_full(object_t *obj){
    if (obj -> kind != COLLECTION){
        fprintf(stderr, "Object of non_collection kind cannot be empty");
        return -1;
    }

    if(obj -> data.v_collection.length == obj -> data.v_collection.capacity){
        return 1;
    }
    return 0;

}

vm_t *new_virtual_machine(size_t *code){
    vm_t *vm = malloc(sizeof(vm_t));
    if (vm == NULL){
        fprintf(stderr, "Memory allocation failed");
        return NULL;
    }

    vm -> ip = 0;
    vm -> bytecode = code;

    vm -> operand_stack = new_object_collection(256, true);

    if (vm -> operand_stack == NULL){
        free(vm);
        return NULL;
    }

    return vm;
}

void run_vm(vm_t *vm){
    if (vm == NULL || vm -> bytecode == NULL || vm -> operand_stack == NULL){
        fprintf(stderr, "[NULL ERROR] VM cannot run on null parameters\n");
        return;
    }
    printf("--- VM BOOT SEQUENCE INITIATED ---\n");
    while(true){
        size_t instruction = vm -> bytecode[vm -> ip];
        vm -> ip++;

        switch(instruction){
            case OP_HALT:
                printf("--- VM HALTED ----\n");
                return;
            case OP_PUSH_INT:{
                int push_int = vm -> bytecode[vm -> ip];
                vm -> ip++;
                object_t *int_obj = new_object_integer(push_int);    
                collection_append(vm -> operand_stack, int_obj);
                break;
            }
            case OP_PUSH_FLOAT:{
                size_t raw_bits = vm -> bytecode[vm -> ip];
                vm -> ip++;
                object_t *float_obj = new_object_float(*(float*)&raw_bits);
                collection_append(vm -> operand_stack, float_obj);
                break;
            }
            case OP_PUSH_STRING:{
                size_t raw_bits_string = vm -> bytecode[vm -> ip];
                vm -> ip++;
                char * text = (char *)raw_bits_string;
                object_t *string_obj = new_object_string(text);
                collection_append(vm -> operand_stack, string_obj);
                break;
            }

            case OP_BUILD_COLLECTION:{
                size_t pop_depth = vm -> bytecode[vm -> ip];
                vm -> ip++;

                if (pop_depth > vm -> operand_stack -> data.v_collection.length){
                    fprintf(stderr, "STACK UNDERFLOW ERROR DURING BUILD PROCESS");
                    return;
                }
                object_t *new_collection = new_object_collection(pop_depth, false);
                object_t *buffer[pop_depth];

                for (size_t i = 0; i < pop_depth; i++){
                    buffer[i] = collection_pop(vm -> operand_stack);
                }

                for (size_t j = 0; j < pop_depth ; j++){
                    collection_append(new_collection, buffer[(pop_depth - 1) - j]);
                }

                collection_append(vm -> operand_stack, new_collection);
                break;
            }
            case OP_BUILD_VECTOR:{
                size_t d = vm -> bytecode[vm -> ip];
                vm -> ip++;

                float buffer[d];

                for (size_t i = 0; i < d; i++){
                    object_t *popped_item = collection_pop(vm -> operand_stack);

                    if (popped_item == NULL){
                        fprintf(stderr, "STACK UNDERFLOW ERROR");
                        return;
                    }

                    else if (popped_item -> kind == INTEGER){
                        buffer[d - 1 - i] = (float)popped_item -> data.v_int;
                    }

                    else if(popped_item -> kind == FLOAT){
                        buffer[d - 1 - i] = (float)popped_item -> data.v_float;

                    }

                    else {
                        fprintf(stderr, "Cannot vectorize non-int or non-float kind");
                        return;
                    }

                    object_free(popped_item);

                    
                }
                collection_append(vm -> operand_stack, new_object_vector(d, buffer));

                break;

            }



            case OP_ADD:{
                object_t *pop1 = collection_pop(vm -> operand_stack);
                object_t *pop2 = collection_pop(vm -> operand_stack);
                
                if(pop1 == NULL || pop2 == NULL){
                    fprintf(stderr, "VM Error: Stack underflow during ADD.\n");
                    object_free(pop1);
                    object_free(pop2);
                    return;                   
                }

                object_t *result = object_add(pop2, pop1);

                if (result == NULL){
                    fprintf(stderr, "VM Error: ADD Operation failed.\n");
                    object_free(pop1);
                    object_free(pop2);
                    return;
                }

                collection_append(vm -> operand_stack, result);

                object_free(pop1);
                object_free(pop2);

                break;
            }
            case OP_SUB:{
                object_t *pop1 = collection_pop(vm -> operand_stack);
                object_t *pop2 = collection_pop(vm -> operand_stack);
                
                if(pop1 == NULL || pop2 == NULL){
                    fprintf(stderr, "VM Error: Stack underflow during SUB.\n");
                    object_free(pop1);
                    object_free(pop2);
                    return;                   
                }

                object_t *result = object_subtract(pop2, pop1);

                if (result == NULL){
                    fprintf(stderr, "VM Error: SUB Operation failed.\n");
                    object_free(pop1);
                    object_free(pop2);
                    return;
                }

                collection_append(vm -> operand_stack, result);
                object_free(pop1);
                object_free(pop2);

                break;

            }
            case OP_MUL:{
                object_t *pop1 = collection_pop(vm -> operand_stack);
                object_t *pop2 = collection_pop(vm -> operand_stack);
                
                if(pop1 == NULL || pop2 == NULL){
                    fprintf(stderr, "VM Error: Stack underflow during MUL.\n");
                    object_free(pop1);
                    object_free(pop2);
                    return;                   
                }

                object_t *result = object_multiply(pop2, pop1);

                if (result == NULL){
                    fprintf(stderr, "VM Error: MUL Operation failed.\n");
                    object_free(pop1);
                    object_free(pop2);
                    return;
                }

                collection_append(vm -> operand_stack, result);

                object_free(pop1);
                object_free(pop2);

                break;

            }
            case OP_DIV:{
                object_t *pop1 = collection_pop(vm -> operand_stack);
                object_t *pop2 = collection_pop(vm -> operand_stack);
                
                if(pop1 == NULL || pop2 == NULL){
                    fprintf(stderr, "VM Error: Stack underflow during DIV.\n");
                    object_free(pop1);
                    object_free(pop2);
                    return;                   
                }

                object_t *result = object_divide(pop2, pop1);

                if (result == NULL){
                    fprintf(stderr, "VM Error: DIV Operation failed.\n");
                    object_free(pop1);
                    object_free(pop2);
                    return;
                }

                collection_append(vm -> operand_stack, result);
                object_free(pop1);
                object_free(pop2);

                break;

            }

            case OP_PRINT:{
                object_t *stack_top = collection_pop(vm -> operand_stack);
                if(stack_top == NULL){
                    fprintf(stderr, "VM Error: Stack underflow during PRINT\n");
                    return;
                }
                print_object(stack_top);
                printf("\n");
                object_free(stack_top);
                break;
            }
            default:
                fprintf(stderr, "Unknown Virtual Machine OP_CODE");
                return;

        }

    }
}
int main() {
    // --- TEST 1 ASSETS: Polymorphic Multiplication ---
    char *str_base = "ha";
    int mult_val = 3;

    // --- TEST 2 ASSETS: Vector/Scalar Arithmetic ---
    float v1 = 1.5f;
    float v2 = 2.5f;
    float v3 = 3.5f;
    int scalar = 2; // Intentionally using an INT to test implicit float casting

    // --- TEST 3 ASSETS: Destructive Collection Merging ---
    int c1 = 10, c2 = 20;
    int c3 = 30, c4 = 40;

    // --- TEST 4 ASSETS: Heterogeneous String Concat ---
    char *str_hello = "VM Architecture: ";
    char *str_status = "STABLE.";

    size_t opcodes[] = {
        // --------------------------------------------------------
        // TEST 1: String repetition (STRING * INTEGER)
        // Expected Output: hahahaha
        // --------------------------------------------------------
        OP_PUSH_STRING, (size_t)str_base,
        OP_PUSH_INT, mult_val,
        OP_MUL,                 
        OP_PRINT,               

        // --------------------------------------------------------
        // TEST 2: Vector Math with Implicit Type Promotion
        // Pushes 3 floats, builds a vector, multiplies by an INTEGER
        // Expected Output: <3.000000, 5.000000, 7.000000>
        // --------------------------------------------------------
        OP_PUSH_FLOAT, *(size_t*)&v1,
        OP_PUSH_FLOAT, *(size_t*)&v2,
        OP_PUSH_FLOAT, *(size_t*)&v3,
        OP_BUILD_VECTOR, 3,     
        OP_PUSH_INT, scalar,    
        OP_MUL,                 
        OP_PRINT,               

        // --------------------------------------------------------
        // TEST 3: Destructive Move Semantics (The Memory Killer)
        // Builds [10, 20] and [30, 40], merges them into [10, 20, 30, 40]
        // This will immediately segfault if your length=0 memory fix fails.
        // Expected Output: [10, 20, 30, 40]
        // --------------------------------------------------------
        OP_PUSH_INT, c1,
        OP_PUSH_INT, c2,
        OP_BUILD_COLLECTION, 2, 

        OP_PUSH_INT, c3,
        OP_PUSH_INT, c4,
        OP_BUILD_COLLECTION, 2, 

        OP_ADD,                 
        OP_PRINT,               

        // --------------------------------------------------------
        // TEST 4: String Concatenation 
        // Expected Output: VM Architecture: STABLE.
        // --------------------------------------------------------
        OP_PUSH_STRING, (size_t)str_hello,
        OP_PUSH_STRING, (size_t)str_status,
        OP_ADD,                 
        OP_PRINT,

        OP_HALT
    };

    printf("=== EXECUTING KERNEL STRESS TEST ===\n");
    vm_t *stress_vm = new_virtual_machine(opcodes);
    run_vm(stress_vm);
    
    // --- CRITICAL MEMORY SHUTDOWN ---
    object_free(stress_vm->operand_stack);
    free(stress_vm);
    printf("=== VIRTUAL MACHINE TERMINATED ===\n");

    return 0;
}
