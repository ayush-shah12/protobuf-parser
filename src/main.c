#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "osm.h"

int main(int argc, char **argv)
{
    // initial pass to validate CLI args
    int initial_result = process_args(argc, argv, NULL);

    if (initial_result == -1){
        USAGE(*argv, EXIT_FAILURE);
    }

    if(help_requested){
        USAGE(*argv, EXIT_SUCCESS);
    }

    // handle case of input file path provided 
    if(osm_input_file){
        FILE *f = fopen(osm_input_file, "r");
        if(!f){
            fprintf(stderr, "Could Not Open File Named: %s", osm_input_file); 
            fflush(stderr);
            exit(EXIT_FAILURE);
        }

        OSM_Map *map = OSM_read_Map(f);
        
        if(!map){
            fprintf(stderr, "Error Processing File Contents To OSM_Map struc\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
        }

        // run query on in memory deserialized protobuf file 
        int result = process_args(argc, argv, map);

        if(result == - 1){
            fprintf(stderr, "Error running queries (path provided via CLI).\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
            
        }
        exit(EXIT_SUCCESS);
    }

    // if no file explicitly passed, read from STDIN
    else{
        FILE *f = stdin;  
        OSM_Map *map = OSM_read_Map(f);

        if(!f){
            fprintf(stderr, "No File Specified in STDIN\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
        }

        if(!map){
            fprintf(stderr, "Error Processing File Input from stdin to OSM_Map struct\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
        }
        int result = process_args(argc, argv, map);
        if (result == -1) {
            fprintf(stderr, "Error running queries(File originated from stdin)\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
        }
    
        exit(EXIT_SUCCESS);
    }
}