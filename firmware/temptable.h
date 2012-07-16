#ifndef _TEMPERATURE_MAPPING_TABLE_
#define _TEMPERATURE_MAPPING_TABLE_

/****************************************************************************
* Temperature mapping table
****************************************************************************/
typedef struct 
{
  int          adcVal;
  char         step;
  char         temp;
} TempTableStruct;

extern const TempTableStruct tempTable[];
extern const unsigned char tempTableSize;

#endif
