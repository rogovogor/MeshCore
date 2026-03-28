# Number Allocations

This document lists unique numbers/identifiers used in various MeshCore protcol payloads.

# Group Data Types

The `PAYLOAD_TYPE_GRP_DATA` payloads have a 16-bit data-type field, which identifies which application the packet is for.

To make sure multiple applications can function without interfering with each other, the table below is for reserving various ranges of data-type values. Just modify this table, adding a row, then submit a PR to have it authorised/merged.

The 16-bit types are allocated in blocks of 16, ie. the lower 4-bits is the range.

| Data-Type range | App name                 | Contact                                              |
|-----------------|--------------------------|------------------------------------------------------|
| 000x            | -reserved-               |  |
| FFFx            | -reserved-               |  |
