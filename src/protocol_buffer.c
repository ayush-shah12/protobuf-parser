#include <stdio.h>
#include <stdlib.h>

#include "protocol_buffer.h"
#include "zlib_inflate.h"


/* Build a linked list of protobuf messages from a stream of len bytes */
int PB_read_message(FILE *in, size_t len, PB_Message *msgp){
    
    PB_Field *head = malloc(sizeof(PB_Field));
    head->number = -1;
    head->type = 8;
    head->next = head;
    head->prev = head;

    PB_Field *current = head;

    size_t total_byte_count = 0;
    
    while (total_byte_count < len) {
        PB_Field *field = malloc(sizeof(PB_Field));
        int result = PB_read_field(in, field);

        // handle error in read
        if (result <= 0) {
            free(field);
            if (total_byte_count == 0){
                return 0;
            } 
            return -1;
        }

        total_byte_count += result;

        // linked
        field->prev = current;
        current->next = field;
        field->next = NULL;
        current = field;
    }

    // link back to sentiel and return
    current->next = head;
    *msgp = head;
    return total_byte_count;
}

/* Read the embedded message from a memory buffer */
int PB_read_embedded_message(char *buf, size_t len, PB_Message *msgp) {

    FILE *f = fmemopen(buf, len, "r");

    int result = PB_read_message(f, len, msgp);

    fclose(f);

    if (result == 0 || result == -1){
        return -1;
    }
    return 0;
}

/* Read zlib-compressed data from a memory buffer, inflating it and interpreting it as a protocol buffer message. */
int PB_inflate_embedded_message(char *buf, size_t len, PB_Message *msgp) {
    FILE *f = fmemopen(buf, len, "r");
    char *other_buffer = NULL;
    size_t decompressed_size = 0;

    FILE *other_stream = open_memstream(&other_buffer, &decompressed_size);
    int result = zlib_inflate(f, other_stream);    
    if(result != 0){
        return -1;
    }

    fclose(f);
    fclose(other_stream);

    
    int r = PB_read_embedded_message(other_buffer, decompressed_size, msgp);
    if(r == 0){
        return 0;
    }
    return -1;
}

/* This function reads data from the input stream in and interprets
 * it as a single field of a protocol buffers message.  The information read,
 * consisting of a tag that specifies a wire type and field number,
 * as well as content that depends on the wire type, is used to initialize
 * the caller-supplied PB_Field structure.
 */
int PB_read_field(FILE *in, PB_Field *fieldp) {
    int32_t return_fieldp;
    PB_WireType *typepb = malloc(sizeof(PB_WireType));
    int bytes_1 = PB_read_tag(in, typepb, &return_fieldp);

    if(bytes_1 == -1){
        return -1;
    }

    if(bytes_1 == 0){
        return 0;
    }

    union value *valuep = malloc(sizeof(union value));

    fieldp->type = *typepb;
    fieldp->number = return_fieldp;
    
    int bytes_2 = PB_read_value(in, *typepb, valuep);

    if(bytes_2 == -1){
        return -1;
    }
    if(bytes_2 == 0){
        return 0;
    }

    fieldp -> value = *valuep;


    return bytes_1 + bytes_2;
   
}

/**
 * Read the varint-encoded 32-bit tag portion of a protocol buffers field
 * and returns the wire type and field number. 
 */
int PB_read_tag(FILE *in, PB_WireType *typep, int32_t *fieldp) {
    
    uint64_t actual_value = 0; //this should be the actual value it reads, not the byte count

    unsigned char *buffer = malloc(sizeof(char) * 10); // max 10 bytes? idk if its the same here or not...
    int count = 0; 

    while(count < 10){
        if (fread(&buffer[count], 1, 1, in) != 1) { 
            free(buffer);
            if(count == 0){
                return 0;
            }
            else{
                return -1;
            }
        }

        if ((buffer[count] & 0x80) == 0) {
            count++;
            break;
        }

        count++;
    }

    for (int x = count - 1; x >= 0; x--) {
        unsigned char current = buffer[x] & 0b01111111; 
        actual_value = (actual_value << 7) | current;  
    }

    *fieldp = actual_value >> 3;
    *typep = (actual_value & 0b00000111);
    return count;
    
}

/**
 * This function reads bytes from the input stream in and interprets
 * them as a single protocol buffers value of the wire type specified by the type
 * parameter.
 */
int PB_read_value(FILE *in, PB_WireType type, union value *valuep) {

    int64_t _handle_varint(){
        int64_t actual_value = 0; //this should be the actual value it reads, not the byte count

        unsigned char *buffer = malloc(sizeof(char) * 10); // max 10 bytes
        int count = 0; 

        while(count < 10){
            if (fread(&buffer[count], 1, 1, in) != 1) { 
                free(buffer);
                if(count == 0){
                    return 0;
                }
                else{
                    return -1;
                }
            }

            if ((buffer[count] & 0x80) == 0) {
                count++;
                break;
            }

            count++;
        }

        for (int x = count - 1; x >= 0; x--) {
            unsigned char current = buffer[x] & 0b01111111; 
            actual_value = (actual_value << 7) | current;  
        }
    
        valuep->i64 = actual_value; // storing in i64 for consistency

        return count; // byte count
    }

    int64_t _handle_lentype(){
        int64_t actual_value = 0; //this should be the actual value it reads, not the byte count

        unsigned char *buffer = malloc(sizeof(char) * 10); // max 10 bytes
        int count = 0; 

        while(count < 10){
            if (fread(&buffer[count], 1, 1, in) != 1) { 
                free(buffer);               
                if(count == 0){
                    return 0;
                }
                else{
                    return -1;
                }
                
            }

            if ((buffer[count] & 0x80) == 0) {
                count++;
                break;
            }

            count++;
        }

        for (int x = count - 1; x >= 0; x--) {
            unsigned char current = buffer[x] & 0b01111111; 
            actual_value = (actual_value << 7) | current;  
        }

        valuep->bytes.size = actual_value;
        valuep->bytes.buf = malloc(actual_value * sizeof(char));
        int b_count = 0;
        while(b_count < actual_value){
            fread(&valuep->bytes.buf[b_count], 1, 1, in);
            b_count++;
        }
        
        return count + actual_value; // the number of bytes which represented the length + the number of bytes for the actual content
    }

    if (type == VARINT_TYPE){
        int64_t bytes_read = _handle_varint();
        return bytes_read;
    }

    else if(type == I64_TYPE){
        int bytes_read = fread(&valuep->i64, 1, 8, in);
        if(bytes_read == 8){
            return 8;
        }
        else if(bytes_read == 0){
            return 0;
        }
        else{
            return -1;
        }
    }

    else if(type == LEN_TYPE){
        int64_t bytes_read = _handle_lentype();
        return bytes_read;
    }

    else if(type == SGROUP_TYPE){
        return 0;
    }

    else if(type == EGROUP_TYPE){
        return 0;
    }

    else if(type == I32_TYPE){
        int bytes_read = fread(&valuep->i32, 1, 4, in);
        if(bytes_read == 4){
            return 4;
        }
        else if(bytes_read == 0){
            return 0;
        }
        else{
            return -1;
        }
    }

    else if(type == SENTINEL_TYPE){
        return -1;
    }

    else if(type == ANY_TYPE){
        return -1;
    }

    else{
        return 0;
    }
}

/**
 * Get the next field with a specified number from a PB_Message object,
 * scanning the fields in a specified direction starting from a specified previous field.
 */
PB_Field *PB_next_field(PB_Field *prev, int fnum, PB_WireType type, PB_Direction dir) {

    PB_Field *current = prev;

    // forward
    if (dir == 0){
        current = current->next; // start at the next field

        while(current != NULL && current -> type != 8){
            if (current -> number == fnum){
                if(type == 9){
                    return current;
                }
                else{
                    if(type == current->type){
                        return current;
                    }
                    else{
                        // return NULL;
                    }
                }
            }
            current = current -> next;
        }
        
        return NULL;
    }

    else{
        current = current->prev; // start at the prev field

        while(current != NULL && current -> type != 8){
            if (current -> number == fnum){
                if(type == 9){
                    return current;
                }
                else{
                    if(type == current->type){
                        return current;
                    }
                    else{
                        return NULL;
                    }
                }
            }
            current = current -> prev;
        }
    }
    return NULL;
}

/* Get a single field with a specified number from a PB_Message object. */
PB_Field *PB_get_field(PB_Message msg, int fnum, PB_WireType type) {
    PB_Field *last_field = NULL;
    PB_Field *current = msg;

    do{
        if(current -> number == fnum){
            if(type == 9){
                last_field = current;
            }
            else{
                if(type == current->type){
                    last_field = current;
                }
            }
        }
        current = current->next;
    }
    while(current -> type != 8 && current != NULL);

    return last_field;
}

/* Expand packed fields of a PB_Message */
int PB_expand_packed_fields(PB_Message msg, int fnum, PB_WireType type) {
    PB_Field *current = msg;
    
    int _handle_packed_field(PB_Field *packed_field, PB_Field **expanded_fields_head) {
        if (packed_field->type != 2) { 
            return -1;
        }

        int64_t size = packed_field->value.bytes.size;
        char *buf = packed_field->value.bytes.buf;

        if(type == VARINT_TYPE){
    
            PB_Field *head = NULL;
            PB_Field *tail = NULL;
            int64_t bytes_read = 0;
    
            FILE *f = fmemopen(buf, size, "r");
    
            while (bytes_read < size) {
            // -------------------------------HANDLE VARINT--------------------------------
                int64_t actual_value = 0;  
                unsigned char *buffer = malloc(sizeof(char) * 10);  
                int count = 0;
    
                while (count < 10) {
                    if (fread(&buffer[count], 1, 1, f) != 1) {
                        free(buffer);
                        fclose(f);
                        return -1;
                    }
                    if ((buffer[count] & 0x80) == 0) {
                        count++;
                        break;
                    }
                    count++;
                }
    
                for (int i = count - 1; i >= 0; i--) {
                    unsigned char byte = buffer[i] & 0x7F;  
                    actual_value = (actual_value << 7) | byte;
                }
    
                // -------------------------------HANDLE VARINT--------------------------------
    
                PB_Field *new_field = malloc(sizeof(PB_Field));
                new_field->number = fnum;  
                new_field->type = type;  
                new_field->value.i64 = actual_value;  
    
                if (head == NULL) {
                    head = new_field;
                    head->prev = head;
                    head->next = head;
                } else {
                    tail->next = new_field;
                    new_field->prev = tail;
                    new_field->next = head; 
                    head->prev = new_field;  
                }
    
                tail = new_field;
    
                bytes_read += count;
            }
            fclose(f);
    
            *expanded_fields_head = head;
            return 0;
        }
        
        else if(type == I32_TYPE){
            PB_Field *head = NULL;
            PB_Field *tail = NULL;

            if(size % 4 != 0){
                return -1;
            }

            int64_t bytes_read = 0;

            FILE *f = fmemopen(buf, size, "r");
            while(bytes_read < size){

                PB_Field *new_field = malloc(sizeof(PB_Field));
                new_field->number = fnum;
                new_field->type = 5;

                if(fread(&new_field->value.i32, 4, 1, f) != 1){
                    return -1;
                }
                bytes_read += 4;
    
                if (head == NULL) {
                    head = new_field;
                    head->prev = head;
                    head->next = head;
                } else {
                    tail->next = new_field;
                    new_field->prev = tail;
                    new_field->next = head; 
                    head->prev = new_field;  
                }
    
                tail = new_field;

            }   
            fclose(f);
            *expanded_fields_head = head;
            return 0;
        }

        else if(type == I64_TYPE){
            PB_Field *head = NULL;
            PB_Field *tail = NULL;

            if(size % 8 != 0){
                return -1;
            }

            int64_t bytes_read = 0;

            FILE *f = fmemopen(buf, size, "r");
            while(bytes_read < size){

                PB_Field *new_field = malloc(sizeof(PB_Field));
                new_field->number = fnum;
                new_field->type = 1;

                if(fread(&new_field->value.i64, 8, 1, f) != 1){
                    return -1;
                }
                bytes_read += 8;
    
                if (head == NULL) {
                    head = new_field;
                    head->prev = head;
                    head->next = head;
                } else {
                    tail->next = new_field;
                    new_field->prev = tail;
                    new_field->next = head; 
                    head->prev = new_field;  
                }
    
                tail = new_field;

            }   
            fclose(f);
            *expanded_fields_head = head;
            return 0;
        }
        
        return -1;
        
    }

    do {
        if (current->number == fnum && current->type == 2) {  
            PB_Field *expanded_fields = NULL;

            int result = _handle_packed_field(current, &expanded_fields);
            if (result == -1 || expanded_fields == NULL) {
                return -1; 
            }

            PB_Field *tail = expanded_fields->prev;  

            current->prev->next = expanded_fields;
            expanded_fields->prev = current->prev;

            tail->next = current->next;
            if (current->next != NULL) {
                current->next->prev = tail;
            }

            current = tail;  
        }
        current = current->next;
    } while (current != NULL && current->type != 8); 

    return 0;
}

/* Debugging function to print a single field of a PB_Message */
void PB_show_field(PB_Field *current, FILE *out) {

    // terminal output
        printf("Field Number: %d, Type: %d\n", current->number, current->type);
        switch (current->type) {
            case VARINT_TYPE:
                printf("Value (VARINT): %lu", current->value.i64);
                printf("\n");
                break;
            case I32_TYPE:
                printf("Value (I32): %u\n", current->value.i32);
                break;
            case I64_TYPE:
                printf("Value (I64): %lu\n", current->value.i64);
                break;
            case LEN_TYPE:
                printf("Value (LEN_TYPE): Size = %zu\n", current->value.bytes.size);
                printf("Buffer: ");
                for (size_t i = 0; i < current->value.bytes.size && i < 200; i++) {
                    printf("%02x ", (unsigned char)current->value.bytes.buf[i]);
                }
                printf("\n");
                break;
            case SENTINEL_TYPE:
                printf("Sentinel Node\n");
                break;
            default:
                printf("Unknown field type\n");
                break;
            }
            printf("\n");


        //file
        // fprintf(out, "Field Number: %d, Type: %d\n", current->number, current->type);
        // fflush(out);
        
        // switch (current->type) {
        //     case VARINT_TYPE:
        //         fprintf(out, "Value (VARINT): %lu\n", current->value.i64);
        //         fflush(out);
        //         break;
        //     case I32_TYPE:
        //         fprintf(out, "Value (I32): %u\n", current->value.i32);
        //         fflush(out);
        //         break;
        //     case I64_TYPE:
        //         fprintf(out, "Value (I64): %lu\n", current->value.i64);
        //         fflush(out);
        //         break;
        //     case LEN_TYPE:
        //         fprintf(out, "Value (LEN_TYPE): Size = %zu\n", current->value.bytes.size);
        //         fflush(out);
        //         fprintf(out, "Buffer: ");
        //         fflush(out);
        //         for (size_t i = 0; i < current->value.bytes.size; i++) {
        //             fprintf(out, "%02x ", (unsigned char)current->value.bytes.buf[i]);
        //         }
        //         fprintf(out, "\n");
        //         fflush(out);
        //         break;
        //     case SENTINEL_TYPE:
        //         fprintf(out, "Sentinel Node\n");
        //         fflush(out);
        //         break;
        //     default:
        //         fprintf(out, "Unknown field type\n");
        //         fflush(out);
        //         break;
        // }
        
        // fprintf(out, "\n");
        // fflush(out);
}

/* Debugging function to print a complete PB_Message */
void PB_show_message(PB_Message msg, FILE *out) {
    PB_Field *start = msg; 
    PB_Field *current = start;

    do {
        PB_show_field(current, NULL);
        fflush(out);
        current = current->next;  
    } while (current -> type != 8); // stop when we reach sentinel again
}