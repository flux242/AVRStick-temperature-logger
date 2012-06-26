#include "temptable.h"

TempTableStruct tempTable[] = {
    /* 2.56V ref * 0.909, 2.33V stab */
    {62,	4,	-28},
    {82,	5,	-23},
    {107,	6,	-18},
    {137,	7,	-13},
    {165,	8,	-9},
    {205,	9,	-4},
    {250,	10,	1},
    {311,	11,	7},
    {589,	10,	32},
    {659,	9,	39},
    {713,	8,	45},
    {753,	7,	50},
    {795,	6,	56},
    {837,	5,	63},
    {872,	4,	70},
    {903,	3,	78},
};

unsigned tempTableSize = sizeof(tempTable)/sizeof(tempTable[0]);

