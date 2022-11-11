#pragma once
#include "platform.h"

struct BitFile_Sections {
	char* sDescription;
	char* sPart;
	char* sTimestampDate;
	char* sTimestampTime;

	uchar* dBitStream;
	size_t iBitStreamLength;
};

int BitFile_Read(FILE* hFile, struct BitFile_Sections** pSections);
void BitFile_Free(struct BitFile_Sections* this);
