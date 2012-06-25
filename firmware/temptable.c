#include "temptable.h"

TempTableStruct tempTable[] = {
    /* 2.56V ref, 2.45V stab */
    {63,	4,	-27},
    {83,	5,	-22},
    {107,	6,	-17},
    {137,	7,	-12},
    {172,	8,	-7},
    {213,	9,	-2},
    {258,	10,	3},
    {338,	11,	11},
    {521,	10,	28},
    {602,	9,	36},
    {665,	8,	43},
    {712,	7,	49},
    {754,	6,	55},
    {789,	5,	61},
    {824,	4,	68},
    {860,	3,	77},
    {892,	2,	88},
};

unsigned tempTableSize = sizeof(tempTable)/sizeof(tempTable[0]);

