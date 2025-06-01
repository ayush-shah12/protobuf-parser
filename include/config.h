#include <stdio.h>
#include <stdlib.h>

#include "osm.h"

/*
 * Command Line Output Message And Exit Code
 */

#define USAGE(program_name, retcode)                                                                             \
    do                                                                                                           \
    {                                                                                                            \
        fprintf(stderr, "USAGE: %s %s\n", program_name,                                                          \
                "[-h] [-f filename] [-s] [-b] [-n id] [-w id] [-w id key ...]\n"                                 \
                "   -h              Help: displays this help menu.\n"                                            \
                "   -f filename     File: read map data from the specified file\n"                               \
                "   -s              Summary: displays map summary information.\n"                                \
                "   -b              Bounding box: displays map bounding box.\n"                                  \
                "   -n id           Node: displays information about the specified node.\n"                      \
                "   -w id           Way refs: displays node references for the specified way.\n"                 \
                "   -w id key ...   Way values: displays values associated with the specified way and keys.\n"); \
        exit(retcode);                                                                                           \
    } while (0)

/* set flag if -h is passed in CLI */
extern int help_requested;

/* store path if a file is specified to be read */
extern char *osm_input_file;

/*
    process CLI args and queries
*/

int process_args(int argc, char **argv, OSM_Map *mp);
