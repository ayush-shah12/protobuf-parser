#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "osm.h"

// flags
int help_requested = 0;

// input file if specified
char *osm_input_file = NULL;

/* helper to run queries on the map */
int run_queries(char **argv, OSM_Map *mp)
{
  printf("\n=== OSM Map Query Results ===\n");
  printf("Processing file: %s", osm_input_file);

  // internal function to truncate (used for the nanodegrees of Nodes or Ways)
  double truncate(double value) { return ((int)(value * 100000)) / 100000.0; }

  for (char **p = argv + 1; *p != NULL; p++)
  {
    if (strcmp(*p, "-s") == 0)
    {
      printf("=== Map Summary ===\n");
      int num_nodes = OSM_Map_get_num_nodes(mp);
      int num_ways = OSM_Map_get_num_ways(mp);
      printf("Total Nodes: %d\n", num_nodes);
      printf("Total Ways: %d\n", num_ways);
    }
    else if (strcmp(*p, "-b") == 0)
    {
      printf("=== Map Bounding Box ===\n");
      OSM_BBox *bbox = OSM_Map_get_BBox(mp);

      if (bbox)
      {
        double factor = 1000000000;

        double min_lon_value = ((OSM_BBox_get_min_lon(bbox))) / (factor);
        double max_lon_value = ((OSM_BBox_get_max_lon(bbox))) / (factor);
        double min_lat_value = ((OSM_BBox_get_min_lat(bbox))) / (factor);
        double max_lat_value = ((OSM_BBox_get_max_lat(bbox))) / (factor);

        double rounded_min_lon = truncate(min_lon_value);
        double rounded_max_lon = truncate(max_lon_value);
        double rounded_min_lat = truncate(min_lat_value);
        double rounded_max_lat = truncate(max_lat_value);

        printf("Bounding Box Coordinates:\n");
        printf("  Minimum Longitude: %.9f\n", rounded_min_lon);
        printf("  Maximum Longitude: %.9f\n", rounded_max_lon);
        printf("  Minimum Latitude:  %.9f\n", rounded_min_lat);
        printf("  Maximum Latitude:  %.9f\n", rounded_max_lat);
      }
    }
    else if (strcmp(*p, "-n") == 0)
    {
      p++;
      char *id = *p;

      char *endptr;
      int64_t id_as_int = strtol(id, &endptr, 10);

      int num_of_nodes = OSM_Map_get_num_nodes(mp);
      int found = 0;

      printf("=== Node Information ===\n");
      printf("Searching for Node ID: %ld\n", id_as_int);

      double factor = 1000000000;

      for (int x = 0; x < num_of_nodes; x++)
      {
        OSM_Node *curr_node = OSM_Map_get_Node(mp, x);
        if (OSM_Node_get_id(curr_node) == id_as_int)
        {
          found = 1;
          double lon = ((float)(OSM_Node_get_lon(curr_node))) / factor;
          double lat = ((float)(OSM_Node_get_lat(curr_node))) / factor;
          double rounded_lon = truncate(lon);
          double rounded_lat = truncate(lat);
          printf("Node Found:\n");
          printf("  ID: %ld\n", id_as_int);
          printf("  Latitude:  %.9f\n", rounded_lat);
          printf("  Longitude: %.9f\n", rounded_lon);
          break;
        }
      }
      if (!found) {
        printf("Node not found in the map.\n");
      }
    }
    else if (strcmp(*p, "-w") == 0)
    {
      // if the value after -w [arg] is NULL or contains a -, then its just one arg
      if (*(p + 2) == NULL || strchr(*(p + 2), '-') != NULL)
      {
        p++;

        char *id = *p;

        char *endptr;
        int64_t id_as_int = strtol(id, &endptr, 10);

        int num_of_ways = OSM_Map_get_num_ways(mp);
        int found = 0;

        printf("=== Way Node References ===\n");
        printf("Searching for Way ID: %ld\n", id_as_int);

        for (int x = 0; x < num_of_ways; x++)
        {
          OSM_Way *curr_way = OSM_Map_get_Way(mp, x);

          if (OSM_Way_get_id(curr_way) == id_as_int)
          {
            found = 1;
            int ref_count = OSM_Way_get_num_refs(curr_way);
            printf("Way Found:\n");
            printf("  ID: %ld\n", id_as_int);
            printf("  Number of Node References: %d\n", ref_count);
            printf("  Node Reference Sequence: ");
            
            int curr_index = 0;
            while (curr_index < ref_count - 1)
            {
              int64_t id = OSM_Way_get_ref(curr_way, curr_index);
              printf("%ld ", id);
              curr_index += 1;
            }
            int64_t id = OSM_Way_get_ref(curr_way, ref_count - 1);
            printf("%ld\n", id);
            break;
          }
        }
        if (!found) {
          printf("Way not found in the map.\n");
        }
      }
      else
      {
        //-------------Get the Way Object----------------
        p++;
        char *id = *p;

        char *endptr;
        int64_t id_as_int = strtol(id, &endptr, 10);

        int num_of_ways = OSM_Map_get_num_ways(mp);

        OSM_Way *curr_way = NULL;
        int found = 0;
        for (int x = 0; x < num_of_ways; x++)
        {
          curr_way = OSM_Map_get_Way(mp, x);
          if (OSM_Way_get_id(curr_way) == id_as_int)
          {
            found = 1;
            break;
          }
        }

        if (found == 1)
        {
          printf("=== Way Key-Value Pairs ===\n");
          printf("Way ID: %ld\n", id_as_int);
          printf("Requested Key-Value Pairs:\n");
          
          int num_keys = OSM_Way_get_num_keys(curr_way);
          while (*(p + 1) != NULL && strchr(*(p + 1), '-') == NULL)
          {
            p++;
            char *key = *p;

            int k_index = 0;
            int actual_key_index = -1;

            while (k_index < num_keys)
            {
              // find index of key
              char *curr_key = OSM_Way_get_key(curr_way, k_index);
              if (strcmp(curr_key, key) == 0)
              {
                actual_key_index = k_index;
                break;
              }
              k_index += 1;
            }

            if (actual_key_index != -1)
            {
              char *value = OSM_Way_get_value(curr_way, actual_key_index);
              if (value)
              {
                printf("  %s: %s\n", key, value);
              }
              else
              {
                printf("  %s: <no value>\n", key);
              }
            }
            else
            {
              printf("  %s: <key not found>\n", key);
            }
          }
        }
        else
        {
          printf("Way ID %ld not found in the map.\n", id_as_int);
        }
      }
    }
    printf("\n");
  }
  return 0;
}

/* helper to validate the args */
int validate_args(int argc, char **argv)
{
  // handle no args passed
  if (argc == 1)
  {
    return -1;
  }

  // handle help as first arg. Can return right away
  if (strcmp(argv[1], "-h") == 0)
  {
    help_requested = 1;
    return 0;
  }

  for (char **p = argv + 1; *p != NULL; p++)
  {
    if (strcmp(*p, "-h") == 0)
    {
      return -1; // if we reached this loop and found -h, it's invalid.
    }
    else if (strcmp(*p, "-f") == 0)
    {
      // if the input file is set, we've alr seen -f
      if (osm_input_file != NULL)
      {
        return -1;
      }
      else
      {
        if (*(p + 1) == NULL)
        {
          return -1;
        }

        // this triggers on file names with dashes (like the germany file so
        // skip ig) else if(strchr(*(p+1), '-') != NULL){
        //     // fprintf(stderr, "the arg after -f contained a -, it
        //     shouldn't have"); return -1;
        // }

        p++;
        osm_input_file = *p;
      }
    }
    else if (strcmp(*p, "-s") == 0)
    {
      if (*(p + 1) == NULL)
      {
        // valid
      }
      else if (strchr(*(p + 1), '-') == NULL)
      {
        // -s must be followed with nothing OR a dashed arg
        return -1;
      }
    }
    else if (strcmp(*p, "-b") == 0)
    {
      if (*(p + 1) == NULL)
      {
        // valid
      }
      else if (strchr(*(p + 1), '-') == NULL)
      {
        // -b must be followed with nothing OR a dashed arg
        return -1;
      }
    }
    else if (strcmp(*p, "-n") == 0)
    {
      if (*(p + 1) == NULL)
      {
        return -1;
      }
      else if (strchr(*(p + 1), '-') != NULL)
      {
        return -1;
      }

      p++;
    }
    else if (strcmp(*p, "-w") == 0)
    {
      if (*(p + 1) == NULL)
      {
        return -1;
      }

      else if (strchr(*(p + 1), '-') != NULL)
      {
        return -1;
      }

      // theres nothing OR theres an -p or whatever after the first val after
      // -w [val] then this isn't key value
      else if (*(p + 2) == NULL || strchr(*(p + 2), '-') != NULL)
      {
        p++;
      }

      // validate keys
      else
      {
        // while not null and not an -arg
        while (*(p + 1) != NULL && strchr(*(p + 1), '-') == NULL)
        {
          p++;
        }
      }
    }
    else
    {
      return -1;
    }
  }
  return 0;
}

/* main function to process the args */
int process_args(int argc, char **argv, OSM_Map *mp)
{

  if (mp)
  {
    int result = run_queries(argv, mp);
    return result;
  }

  else
  {
    int result = validate_args(argc, argv);
    return result;
  }
}
