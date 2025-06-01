# Protobuf Deserializer

A lightweight command-line tool written in C for parsing and querying OpenStreetMap PBF files. This project implements a custom PBF deserializer without relying on external parsing libraries, providing direct access to OSM map data.

OpenStreetMap (OSM) uses Protocol Buffers (protobuf) as its primary data format for handling massive geographic datasets. Protocol Buffers were developed by Google to efficiently serialize structured data in a language-agnostic, platform-neutral format. They use a binary encoding that's both compact and fast to parse, making them ideal for OSM's massive datasets.

To illustrate the scale: the map of Andorra, a country of just 180 square miles, contains nearly 500 million nodes. In human-readable XML format, this data would be ~77,000 KB, but using Protocol Buffers' binary serialization, it's compressed to just ~3,000 KB - a 96% reduction while maintaining all data. This efficiency is crucial when dealing with larger regions: continents and global datasets would be impractical to work with in XML format. See my program deserialize and run multiple queries against Andorra's 500 million nodes & 25K ways in under a second below! 

The binary format enables:
- Rapid parsing and serialization of massive datasets
- Minimal memory footprint during processing
- Efficient storage and network transfer
- Direct binary access to map elements

## Features

- PBF file deserialization from scratch
- Command-line interface for querying map data
- Support for various query types:
  - Map summary statistics
  - Bounding box information
  - Node lookup by ID
  - Way lookup by ID
  - Way key-value pair queries


## Usage

The tool provides several command-line options for querying OSM map data:

```bash
bin/osm_parser [-h] [-f filename] [-s] [-b] [-n id] [-w id] [-w id key ...]

Options:
  -h              Help: displays this help menu
  -f filename     File: read map data from the specified file
  -s              Summary: displays map summary information
  -b              Bounding box: displays map bounding box
  -n id           Node: displays information about the specified node
  -w id           Way refs: displays node references for the specified way
  -w id key ...   Way values: displays values associated with the specified way and keys
```

## Query

Here's a comprehensive example that demonstrates multiple query types in a single command:

```bash
bin/osm_parser -f files/andorra.osm.pbf \
        -b \                    # Get bounding box of the map
        -s \                    # Show summary statistics
        -n 625033 \            # Look up node with ID 625033
        -n 45161231 \          # Look up node with ID 45161231
        -w 5203906 \           # Get way with ID 5203906 and its tags:
        highway \              #   - highway type
        maxspeed \             #   - speed limit
        oneway \               #   - oneway status
        ref \                  #   - reference number
        surface \              #   - surface type
        -w 6165721 \           # Get way with ID 6165721 and its tags:
        highway \              #   - highway type
        junction \             #   - junction type
        ref \                  #   - reference number
        surface                #   - surface type
```

### Query Output

```
=== OSM Map Query Results ===
Processing file: files/andorra.osm.pbf

=== Map Bounding Box ===
Bounding Box Coordinates:
  Minimum Longitude: 1.412360000
  Maximum Longitude: 1.787480000
  Minimum Latitude:  42.427600000
  Maximum Latitude:  42.657170000

=== Map Summary ===
Total Nodes: 484821
Total Ways: 25305

=== Node Information ===
Searching for Node ID: 625033
Node Found:
  ID: 625033
  Latitude:  42.521940000
  Longitude: 1.559610000

=== Node Information ===
Searching for Node ID: 45161231
Node Found:
  ID: 45161231
  Latitude:  42.625760000
  Longitude: 1.603870000

=== Way Key-Value Pairs ===
Way ID: 5203906
Requested Key-Value Pairs:
  highway: primary
  maxspeed: 30
  oneway: yes
  ref: N 22
  surface: asphalt

=== Way Key-Value Pairs ===
Way ID: 6165721
Requested Key-Value Pairs:
  highway: primary
  junction: roundabout
  ref: CG-2
  surface: asphalt
```

Try it out for yourself by obtaining a protobuf serialized version of an OSM Map here: https://download.geofabrik.de/

## Technical Details

This project implements a custom PBF (Protocol Buffer Format) deserializer for OpenStreetMap data. It provides:

- Direct binary parsing of PBF files
- Memory-efficient data structures
- Command-line interface for data access
- No external parsing library dependencies

### Protocol Buffer Implementation

The implementation required careful handling of OSM's Protocol Buffer schema:

1. **Schema Definition**:
   - Implemented the OSM PBF schema in C
   - Handled multi-nested message types for nodes, ways, and relations
   - Managed tag key-value pairs using string tables
   - Implemented delta encoding for coordinates

2. **Binary Parsing**:
   - Direct binary reading of varint-encoded fields
   - Handling of wire types (0: varint, 1: 64-bit, 2: length-delimited)
   - Proper field number tracking for nested messages
   - Efficient string table management

3. **Data Structures**:
   - Custom implementations for OSM primitives (Node, Way, Relation)
   - Efficient memory management for large datasets
   - Optimized coordinate storage using delta encoding
   - Fast lookup tables for node and way references

The implementation follows the OSM PBF specification exactly, allowing for reliable parsing of any valid OSM PBF file while maintaining high performance and low memory usage.

