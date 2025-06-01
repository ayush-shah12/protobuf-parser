#include <stdio.h>
#include <stdint.h>

#include "config.h"
#include "osm.h"

/* OSM Data Structures */

struct OSM_Node
{
    OSM_Id id;
    OSM_Lat lat;
    OSM_Lon lon;
    struct OSM_Node *next;
    PB_Message string_table; // pointer for this node's specific string table
};

struct OSM_Way
{
    OSM_Id id;
    uint32_t *keys;
    int64_t keys_count;
    uint32_t *values;
    int64_t vals_count;
    int64_t *refs;
    int64_t refs_count; // number of refs
    struct OSM_Way *next;
    PB_Message string_table; // pointer for this ways specific string table
};

struct OSM_BBox
{
    // storing as nanodegrees that are already zig zag decoded
    OSM_Lon min_lon;
    OSM_Lon max_lon;
    OSM_Lat min_lat;
    OSM_Lat max_lat;
};

struct OSM_Map
{
    OSM_BBox *BBox;
    OSM_Node *nodes; // singly linked list
    OSM_Node *nodes_tail;
    OSM_Way *ways; // singly linked list
    OSM_Way *ways_tail;
    uint64_t num_nodes;
    uint64_t num_ways;
};

/* Helper Functions */

/* Count the number of fields given a field number and wire type*/
int32_t count(PB_Message msg, int field_number, PB_WireType type)
{
    PB_Field *current = msg;
    current = current->next; // skip sentinel

    int32_t count = 0;

    while (current != NULL && current->type != 8)
    {
        if (current->number == field_number)
        {
            if (current->type == type)
            {
                count++;
            }
        }
        current = current->next;
    }

    return count;
}

/* Decode zig-zag encoding */
int64_t zigzag(uint64_t val)
{
    if (val % 2 == 0)
    {
        return val / 2;
    }
    return (-1) * ((val + 1) / 2);
}

/* Get the first field of a type encountered */
PB_Field *PB_get_FIRST__field(PB_Message msg, int fnum, PB_WireType type)
{
    PB_Field *current = msg;

    do
    {
        if (current->number == fnum)
        {
            if (type == 9)
            {
                return current;
            }
            else
            {
                if (type == current->type)
                {
                    return current;
                }
            }
        }
        current = current->next;
    } while (current->type != 8 && current != NULL);

    return NULL;
}

/* Handlers for the incredibly nested PrimitiveGroup messages in Protobuf format.*/

int64_t handle_NODE(OSM_Map *map, PB_Message prim_group, int64_t lat_offset, int64_t lon_offset, int32_t granularity, PB_Message stringtable)
{
    PB_Field *current = prim_group;
    current = current->next; // skip Sentinel Node

    int64_t node_count = 0;

    if (current->number == 1)
    { // another check to ensure its NODE

        while (current != NULL && current->type != 8)
        {
            PB_Message curr_node = NULL;
            int embedded_read_result = PB_read_embedded_message(current->value.bytes.buf, current->value.bytes.size, &curr_node);
            if (embedded_read_result == -1)
            {
                return -1;
            }

            // sint64 id
            PB_Field *id = PB_get_field(curr_node, 1, VARINT_TYPE);
            if (!id)
            {
                return -1;
            }

            // sint64 lat
            PB_Field *lat = PB_get_field(curr_node, 8, VARINT_TYPE);
            if (!lat)
            {
                return -1;
            }

            // sint64 lon
            PB_Field *lon = PB_get_field(curr_node, 9, VARINT_TYPE);
            if (!lon)
            {
                return 1;
            }

            // create Node
            OSM_Node *node = malloc(sizeof(OSM_Node));
            node->id = id->value.i64; // don't zigzag decode?
            node->string_table = stringtable;

            // handle latitutde and longitude. the offsets are int64 values
            // the lat and lon gotten here from Node, are sint64, so decode first
            // then apply transformation ig.

            // dont do 0.0000...1 here since ill divide in the process since this is an int!!!!!!!!!
            int64_t new_lon = (lon_offset + (granularity * zigzag(lon->value.i64)));
            int64_t new_lat = (lat_offset + (granularity * zigzag(lat->value.i64)));

            node->lat = new_lat;
            node->lon = new_lon;

            // linked list of OSM_NODE
            if (!map->nodes)
            {
                map->nodes = node;
                map->nodes_tail = node;
            }
            else
            {
                map->nodes_tail->next = node;
                map->nodes_tail = node;
            }

            node_count += 1;
            current = current->next;
        }
        return node_count;
    }
    else
    {
        return -1;
    }
}

int64_t handle_WAY(OSM_Map *map, PB_Message prim_group, PB_Message stringtable)
{
    PB_Field *current = prim_group;
    current = current->next; // skip Sentinel Node

    int64_t way_count = 0;

    if (current->number == 3)
    { // another check to ensure its WAY

        while (current != NULL && current->type != 8)
        {
            PB_Message curr_node = NULL;
            int embedded_read_result = PB_read_embedded_message(current->value.bytes.buf, current->value.bytes.size, &curr_node);
            if (embedded_read_result == -1)
            {
                return -1;
            }

            // expand keys and vals
            int a = PB_expand_packed_fields(curr_node, 2, VARINT_TYPE);
            if (a == -1)
            {
                return -1;
            }
            int b = PB_expand_packed_fields(curr_node, 3, VARINT_TYPE);
            if (b == -1)
            {
                return -1;
            }

            int32_t key_count = count(curr_node, 2, VARINT_TYPE);
            int32_t val_count = count(curr_node, 3, VARINT_TYPE);

            if (key_count != val_count)
            {
                return -1;
            }

            OSM_Way *way = malloc(sizeof(OSM_Way));

            way->keys = malloc(sizeof(uint32_t) * key_count);
            way->values = malloc(sizeof(uint32_t) * val_count);

            way->keys_count = key_count;
            way->vals_count = val_count;

            int key_index = 0;
            int val_index = 0;

            PB_Field *current_key_field = PB_get_FIRST__field(curr_node, 2, VARINT_TYPE);
            while (current_key_field != NULL && key_index < key_count)
            {
                way->keys[key_index] = (uint32_t)current_key_field->value.i64;
                key_index += 1;
                current_key_field = PB_next_field(current_key_field, 2, VARINT_TYPE, FORWARD_DIR);
                // current_key_field = PB_next_field(current_key_field, 2, VARINT_TYPE, BACKWARD_DIR);
            }

            PB_Field *current_val_field = PB_get_FIRST__field(curr_node, 3, VARINT_TYPE);
            while (current_val_field != NULL && val_index < val_count)
            {
                way->values[val_index] = (uint32_t)current_val_field->value.i64;
                val_index += 1;
                current_val_field = PB_next_field(current_val_field, 3, VARINT_TYPE, FORWARD_DIR);
                // current_val_field = PB_next_field(current_val_field, 3, VARINT_TYPE, BACKWARD_DIR);
            }

            int c = PB_expand_packed_fields(curr_node, 8, VARINT_TYPE);
            if (c == -1)
            {
                return -1;
            }

            int32_t ref_count = count(curr_node, 8, VARINT_TYPE);
            int32_t ref_index = 0;
            way->refs = malloc(sizeof(int64_t) * ref_count);
            way->refs_count = ref_count;

            PB_Field *current_ref_field = PB_get_FIRST__field(curr_node, 8, VARINT_TYPE);
            int64_t total = 0;
            while (current_ref_field != NULL && ref_index < ref_count)
            {
                int64_t delta = zigzag(current_ref_field->value.i64);
                total += delta;
                way->refs[ref_index] = total;
                ref_index += 1;
                current_ref_field = PB_next_field(current_ref_field, 8, VARINT_TYPE, FORWARD_DIR);
                // current_ref_field = PB_next_field(current_ref_field, 8, VARINT_TYPE, BACKWARD_DIR);
            }

            // sint64 id
            PB_Field *id = PB_get_field(curr_node, 1, VARINT_TYPE);
            if (!id)
            {
                return -1;
            }

            way->id = (int64_t)id->value.i64;
            way->string_table = stringtable;
            way->next = NULL;
            way_count += 1;

            if (!map->ways)
            {
                map->ways = way;
                map->ways_tail = way;
            }
            else
            {
                map->ways_tail->next = way;
                map->ways_tail = way;
            }
            current = current->next;
        }
        return way_count;
    }
    else
    {
        return -1;
    }
}

int64_t handle_DENSE(OSM_Map *map, PB_Message prim_group, int64_t lat_offset, int64_t lon_offset, int32_t granularity, PB_Message stringtable)
{
    PB_Field *current = prim_group;
    current = current->next; // skip Sentinel Node

    int64_t node_count = 0;

    if (current->number == 2)
    { // another check to ensure its DENSE

        while (current != NULL && current->type != 8)
        {
            PB_Message curr_node = NULL;
            int embedded_read_result = PB_read_embedded_message(current->value.bytes.buf, current->value.bytes.size, &curr_node);
            if (embedded_read_result == -1)
            {
                return -1;
            }

            // expand INT IDS
            int a = PB_expand_packed_fields(curr_node, 1, VARINT_TYPE);
            if (a == -1)
            {
                return -1;
            }

            int b = PB_expand_packed_fields(curr_node, 8, VARINT_TYPE);
            if (b == -1)
            {
                return -1;
            }
            int c = PB_expand_packed_fields(curr_node, 9, VARINT_TYPE);
            if (c == -1)
            {
                return -1;
            }
            int32_t id_count = count(curr_node, 1, VARINT_TYPE);
            int32_t lat_count = count(curr_node, 8, VARINT_TYPE);
            int32_t lon_count = count(curr_node, 9, VARINT_TYPE);

            if (id_count != lat_count || id_count != lon_count || lat_count != lon_count)
            {
                return -1;
            }

            int64_t id_total = 0;
            int64_t lat_total = 0;
            int64_t lon_total = 0;

            PB_Field *current_id_field = PB_get_FIRST__field(curr_node, 1, VARINT_TYPE);

            PB_Field *current_lon_field = PB_get_FIRST__field(curr_node, 9, VARINT_TYPE);

            PB_Field *current_lat_field = PB_get_FIRST__field(curr_node, 8, VARINT_TYPE);

            for (int64_t x = 0; x < id_count; x++)
            {
                int64_t id_delta = zigzag(current_id_field->value.i64);
                int64_t lon_delta = zigzag(current_lon_field->value.i64);
                int64_t lat_delta = zigzag(current_lat_field->value.i64);

                id_total += id_delta;
                lat_total += lat_delta;
                lon_total += lon_delta;

                int64_t new_lon = (lon_offset + (granularity * lon_total));
                int64_t new_lat = (lat_offset + (granularity * lat_total));

                OSM_Node *node = malloc(sizeof(OSM_Node));
                node->id = id_total;
                node->lat = new_lat;
                node->lon = new_lon;
                node->string_table = stringtable;

                if (!map->nodes)
                {
                    map->nodes = node;
                    map->nodes_tail = node;
                }
                else
                {
                    map->nodes_tail->next = node;
                    map->nodes_tail = node;
                }
                node_count += 1;

                current_id_field = PB_next_field(current_id_field, 1, VARINT_TYPE, FORWARD_DIR);

                current_lon_field = PB_next_field(current_lon_field, 9, VARINT_TYPE, FORWARD_DIR);

                current_lat_field = PB_next_field(current_lat_field, 8, VARINT_TYPE, FORWARD_DIR);
            }

            current = current->next;
        }
        return node_count;
    }
    else
    {
        return -1;
    }
}

/* Parse an entire OSM Map from a file stream */
OSM_Map *OSM_read_Map(FILE *in)
{

    OSM_Map *map = malloc(sizeof(OSM_Map));
    int header_done = 0;
    map->num_nodes = 0;
    map->num_ways = 0;

    while (1)
    {
        // ------------HEADER---------------------
        uint32_t header_length = 0;
        size_t bytes_read = fread(&header_length, 1, sizeof(header_length), in); // network byte order
        if (bytes_read != sizeof(header_length))
        {
            if (feof(in))
            {
                return map;
            }
            return NULL;
        }
        // convert to little endian
        //  0x12345678 =>
        int first = header_length & 0x000000FF;
        int second = (header_length & 0x0000FF00) >> 8;
        int third = (header_length & 0x00FF0000) >> 16;
        int forth = (header_length & 0xFF000000) >> 24;

        // concatenate back
        uint32_t final_length = (first << 24) | (second << 16) | (third << 8) | forth;
        // ------------HEADER---------------------

        PB_Message blob_header = malloc(sizeof(PB_Message));

        int result = PB_read_message(in, final_length, &blob_header); // get the 'BlobHeader'

        if (result == -1 || result == 0)
        {
            return NULL;
        }

        PB_Field *header_with_datasize = PB_get_field(blob_header, 3, 0); // get datasize field of 'BlobHeader'

        if (!header_with_datasize)
        {
            return NULL;
        }

        uint64_t blob_msg_size = header_with_datasize->value.i64; // get value from datasize field

        PB_Message blob_proper = malloc(sizeof(PB_Message));

        result = PB_read_message(in, blob_msg_size, &blob_proper); // get the 'BlobProper'
        if (result == -1 || result == 0)
        {
            return NULL;
        }

        // handle OSM_HEADER
        if (header_done == 0)
        {
            header_done = 1;

            // field #3 of Blob Proper of LEN_TYPE
            PB_Field *field = PB_get_field(blob_proper, 3, LEN_TYPE); // is field 1 possible(idts but confirm on piazza later)
            if (!field)
            {
                return NULL;
            }

            PB_Message header_block = NULL;
            int inflated_result = PB_inflate_embedded_message(field->value.bytes.buf, field->value.bytes.size, &header_block);

            if (inflated_result == -1 || !header_block)
            {
                return NULL;
            }

            // get the bbox (field 1)
            PB_Field *bbox_field = PB_get_field(header_block, 1, LEN_TYPE);

            // if not bbox, continue without it?
            if (!bbox_field)
            {
                map->BBox = NULL;
            }
            else
            {
                PB_Message HeaderBBox = NULL;

                int embedded_read_result = PB_read_embedded_message(bbox_field->value.bytes.buf, bbox_field->value.bytes.size, &HeaderBBox);

                if (!HeaderBBox || embedded_read_result == -1)
                {
                    return NULL;
                }

                PB_Field *min_lon = PB_get_field(HeaderBBox, 1, VARINT_TYPE);
                PB_Field *max_lon = PB_get_field(HeaderBBox, 2, VARINT_TYPE);
                PB_Field *max_lat = PB_get_field(HeaderBBox, 3, VARINT_TYPE);
                PB_Field *min_lat = PB_get_field(HeaderBBox, 4, VARINT_TYPE);

                if (!min_lon || !max_lon || !min_lat || !max_lat)
                {
                    return NULL;
                }

                OSM_BBox *BBox_pointer = malloc(sizeof(OSM_BBox));

                BBox_pointer->min_lon = zigzag(min_lon->value.i64);
                BBox_pointer->max_lon = zigzag(max_lon->value.i64);
                BBox_pointer->min_lat = zigzag(min_lat->value.i64);
                BBox_pointer->max_lat = zigzag(max_lat->value.i64);

                map->BBox = BBox_pointer;
            }
        }

        // handle OSM_Data
        else
        {
            // field #3 of Blob Proper of LEN_TYPE
            PB_Field *field = PB_get_field(blob_proper, 3, LEN_TYPE); // is field 1 possible(idts but confirm on piazza later)
            if (!field)
            {
                return NULL;
            }

            PB_Message primitive_block = NULL;
            int inflated_result = PB_inflate_embedded_message(field->value.bytes.buf, field->value.bytes.size, &primitive_block);

            if (inflated_result == -1 || !primitive_block)
            {
                // print error here?
                return NULL;
            }

            // save the lat/lon offsets and granularity
            PB_Field *lat_offset = PB_get_field(primitive_block, 19, I64_TYPE);
            PB_Field *lon_offset = PB_get_field(primitive_block, 20, I32_TYPE);
            PB_Field *granularity = PB_get_field(primitive_block, 17, I32_TYPE);

            int64_t lat_offset_value;
            int64_t lon_offset_value;
            int32_t granularity_value;

            if (!lat_offset)
            {
                lat_offset_value = 0;
            }
            else
            {
                lat_offset_value = (int64_t)lat_offset->value.i64;
            }

            if (!lon_offset)
            {
                lon_offset_value = 0;
            }
            else
            {
                lon_offset_value = (int64_t)lon_offset->value.i64;
            }

            if (!granularity)
            {
                granularity_value = 100;
            }
            else
            {
                granularity_value = (int32_t)granularity->value.i32;
            }

            // String Table (Field Number 1, Wire Type = LEN_TYPE)
            PB_Field *string_table = PB_get_field(primitive_block, 1, LEN_TYPE);

            PB_Message string_table_message = NULL;
            int embedded_read_result = PB_read_embedded_message(string_table->value.bytes.buf, string_table->value.bytes.size, &string_table_message);

            if (embedded_read_result == -1 || !string_table_message)
            {
                return NULL;
            }

            // PrimitiveGroup

            // Get The First PrimitiveGroup (Field #2, LEN_TYPE)
            PB_Field *current_primitive_group_field = PB_get_field(primitive_block, 2, LEN_TYPE);

            while (current_primitive_group_field != NULL)
            {
                PB_Message current_prim_group_message = NULL;
                int embedded_read_result = PB_read_embedded_message(current_primitive_group_field->value.bytes.buf, current_primitive_group_field->value.bytes.size, &current_prim_group_message);

                if (!current_prim_group_message || embedded_read_result == -1)
                {
                    return NULL;
                }
                else
                {
                    // ATP We'd Have A message of a list of MULTIPLE Field 1(Node) or Field 3(Way).
                    // IF this has a Field 2(DenseNodes), then its ONLY 1 FIELD

                    // Find What Type is it (NODE, WAY, DENSENODES)
                    PB_Field *current = current_prim_group_message;
                    current = current->next; // skip Sentinel Node

                    if (current->number == 1)
                    { // NODE
                        int64_t result = handle_NODE(map, current_prim_group_message, lat_offset_value, lon_offset_value, granularity_value, string_table_message);
                        if (result == -1)
                        {
                            return NULL;
                        }
                        map->num_nodes = result + map->num_nodes;
                    }
                    else if (current->number == 2)
                    { // DENSE NODES
                        int64_t result = handle_DENSE(map, current_prim_group_message, lat_offset_value, lon_offset_value, granularity_value, string_table_message);
                        if (result == -1)
                        {
                            return NULL;
                        }
                        map->num_nodes = result + map->num_nodes;
                    }
                    else if (current->number == 3)
                    { // WAYS
                        int64_t result = handle_WAY(map, current_prim_group_message, string_table_message);
                        if (result == -1)
                        {
                            return NULL;
                        }
                        map->num_ways = result + map->num_ways;
                    }
                    else if (current->number == 4)
                    { // RELATION
                    }
                    else if (current->number == 5)
                    { // ChangeSet
                    }
                    else
                    {
                        return NULL;
                    }
                }
                current_primitive_group_field = PB_next_field(current_primitive_group_field, 2, LEN_TYPE, FORWARD_DIR);
            }
        }
    }
    return NULL;
}

/* OSM Map Accessor Functions */

int OSM_Map_get_num_nodes(OSM_Map *mp)
{
    return mp->num_nodes;
}

int OSM_Map_get_num_ways(OSM_Map *mp)
{
    return mp->num_ways;
}

OSM_Node *OSM_Map_get_Node(OSM_Map *mp, int index)
{
    if (mp == NULL || mp->nodes == NULL || index < 0 || index >= mp->num_nodes)
    {
        return NULL;
    }

    OSM_Node *current = mp->nodes;
    int count = 0;

    while (current != NULL)
    {
        if (count == index)
        {
            return current;
        }
        current = current->next;
        count++;
    }

    return NULL;
}

OSM_Way *OSM_Map_get_Way(OSM_Map *mp, int index)
{
    if (mp == NULL || mp->ways == NULL || index < 0 || index >= mp->num_ways)
    {

        return NULL;
    }

    OSM_Way *current = mp->ways;
    int count = 0;

    while (current != NULL)
    {
        if (count == index)
        {
            return current;
        }
        current = current->next;
        count++;
    }

    return NULL;
}

OSM_BBox *OSM_Map_get_BBox(OSM_Map *mp)
{
    if (mp && mp->BBox)
    {
        return mp->BBox;
    }
    return NULL;
}

int64_t OSM_Node_get_id(OSM_Node *np)
{
    return np->id;
}

int64_t OSM_Node_get_lat(OSM_Node *np)
{
    return np->lat;
}

int64_t OSM_Node_get_lon(OSM_Node *np)
{
    return np->lon;
}

int64_t OSM_Way_get_id(OSM_Way *wp)
{
    return wp->id;
}

int OSM_Way_get_num_refs(OSM_Way *wp)
{
    return wp->refs_count;
}

OSM_Id OSM_Way_get_ref(OSM_Way *wp, int index)
{
    if (wp == NULL || wp->refs == NULL || index < 0 || index >= wp->refs_count)
    {
        return -1;
    }

    return wp->refs[index];
}

int OSM_Way_get_num_keys(OSM_Way *wp)
{
    return wp->keys_count;
}

char *OSM_Way_get_key(OSM_Way *wp, int index)
{
    if (wp == NULL || wp->keys == NULL || index < 0 || index >= wp->keys_count)
    {
        return NULL;
    }

    uint32_t key_index = wp->keys[index]; // index of key in the keys object, use this to query string table

    PB_Field *curr = wp->string_table;

    if (!curr)
    {
        return NULL;
    }

    curr = curr->next; // skip sentinel node

    for (int i = 0; i < key_index; i++)
    {
        if (!curr->next)
        {
            return NULL;
        }
        curr = curr->next;
    }

    char *result = malloc(curr->value.bytes.size + 1);

    for (size_t i = 0; i < curr->value.bytes.size; i++)
    {
        result[i] = curr->value.bytes.buf[i];
    }
    result[curr->value.bytes.size] = '\0';

    return result;
}

char *OSM_Way_get_value(OSM_Way *wp, int index)
{
    if (wp == NULL || wp->values == NULL || index < 0 || index >= wp->vals_count)
    {
        return NULL;
    }

    uint32_t value_index = wp->values[index]; // index of key in the keys object, use this to query string table

    PB_Field *curr = wp->string_table;

    if (!curr)
    {
        return NULL;
    }

    curr = curr->next; // skip sentinel node

    for (int i = 0; i < value_index; i++)
    {
        if (!curr->next)
        {
            return NULL;
        }
        curr = curr->next;
    }

    char *result = malloc(curr->value.bytes.size + 1);

    for (size_t i = 0; i < curr->value.bytes.size; i++)
    {
        result[i] = curr->value.bytes.buf[i];
    }
    result[curr->value.bytes.size] = '\0';

    return result;
}

int64_t OSM_BBox_get_min_lon(OSM_BBox *bbp)
{
    return bbp->min_lon;
}

int64_t OSM_BBox_get_max_lon(OSM_BBox *bbp)
{
    return bbp->max_lon;
}

int64_t OSM_BBox_get_max_lat(OSM_BBox *bbp)
{
    return bbp->max_lat;
}

int64_t OSM_BBox_get_min_lat(OSM_BBox *bbp)
{
    return bbp->min_lat;
}
