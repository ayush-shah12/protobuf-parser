#ifndef PROTOCOL_BUFFER_H
#define PROTOCOL_BUFFER_H

#include <stdio.h>
#include <stdint.h>

/* Protobuf wire values */

typedef enum {
    VARINT_TYPE = 0,
    I64_TYPE = 1,
    LEN_TYPE = 2,
    SGROUP_TYPE = 3,
    EGROUP_TYPE = 4,
    I32_TYPE = 5,
    SENTINEL_TYPE = 8, // custom type to denote the 'beginning' of a *PB_Message 
    ANY_TYPE = 9
} PB_WireType;

typedef enum {
    FORWARD_DIR = 0,
    BACKWARD_DIR = 1
} PB_Direction;

#define ANY_FIELD (-1)

/* represents a field in a message */

typedef struct PB_Field {
    PB_WireType type;
    int number;
    union value {
	uint32_t i32;
	uint64_t i64;
	struct bytes {
	    size_t size;
	    char *buf;
	} bytes;
    } value;
    struct PB_Field *next, *prev;
} PB_Field;


typedef PB_Field *PB_Message;


/* For reading messages from an input stream. */
int PB_read_message(FILE *in, size_t len, PB_Message *msgp);
int PB_read_field(FILE *in, PB_Field *fieldp);
int PB_read_tag(FILE *in, PB_WireType *typep, int32_t *fieldp);
int PB_read_value(FILE *in, PB_WireType type, union value *valuep);

/* For reading embedded messages from memory buffers. */
int PB_read_embedded_message(char *buf, size_t len, PB_Message *msgp);
int PB_inflate_embedded_message(char *buf, size_t len, PB_Message *msgp);

/* For traversing and manipulating PB_Message objects. */
PB_Field *PB_next_field(PB_Field *prev, int fnum, PB_WireType type, PB_Direction dir);
PB_Field *PB_get_field(PB_Message msg, int fnum, PB_WireType type);
int PB_expand_packed_fields(PB_Message msg, int fnum, PB_WireType type);

/* Debugging/Testing */
void PB_show_message(PB_Message msg, FILE *out);
void PB_show_field(PB_Field *fp, FILE *out);

#endif
