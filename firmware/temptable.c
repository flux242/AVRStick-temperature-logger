#include "temptable.h"

TempTableStruct tempTable[] = {
    /* 2.56V ref, 2.5V stab */
    {30,  2, -40},
    {41,  3, -35},
    {62,  4, -28},
    {82,  5, -23},
    {107, 6, -18},
    {137, 7, -13},
    {165, 8, -9},
    {205, 9, -4},
    {250, 10, 1},
    {310, 11, 7},
    {588, 10, 32},
    {658, 9,  39},
    {712, 8,  45},
    {752, 7,  50},
    {794, 6,  56},
    {831, 5,  62},
    {870, 4,  70},
    {902, 3,  78},
    {935, 2,  89},
};

unsigned tempTableSize = sizeof(tempTable)/sizeof(tempTable[0]);
