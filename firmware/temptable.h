#ifndef _TEMPERATURE_MAPPING_TABLE_
#define _TEMPERATURE_MAPPING_TABLE_

/****************************************************************************
* Temperature mapping table
****************************************************************************/
typedef struct 
{
  unsigned int adcVal;
  char         step;
  char         temp;
} TempTableStruct;

extern TempTableStruct tempTable[];
extern unsigned tempTableSize;

#endif