#include "bitfile.h"

int BitFile_Read(FILE* hFile, struct BitFile_Sections** pSections) {
	uchar cSectionID = 0;
	uchar* pSectionData;
	uint32_t iSectionSize;

	uchar dFileHeader[] = { 0x00, 0x09, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x00, 0x00, 0x01 };
	uchar dData[MAX(4, sizeof(dFileHeader))];

	struct BitFile_Sections* this = calloc(1, sizeof(struct BitFile_Sections));
	if(!this) return -1;

	if(fread(dData, 1, sizeof(dFileHeader), hFile) != sizeof(dFileHeader)) goto error_nofree;
	if(memcmp(dData, dFileHeader, sizeof(dFileHeader)) != 0) goto error_nofree;

	for(;;) {
		if(fread(&cSectionID, 1, 1, hFile) != 1) {
			if(feof(hFile)) break;
			goto error_nofree;
		}

		uchar iLengthBytes = cSectionID == 'e' ? 4 : 2;
		char dData[4];
		if(fread(dData, 1, iLengthBytes, hFile) != iLengthBytes) goto error_nofree;

		if(iLengthBytes == 4) iSectionSize = dData[0] << 24 | dData[1] << 16 | dData[2] << 8 | dData[3];
		else iSectionSize = dData[0] << 8 | dData[1];

		pSectionData = malloc(iSectionSize + 1);
		if(!pSectionData) goto error_nofree;
		if(fread(pSectionData, 1, iSectionSize, hFile) != iSectionSize) goto error;

		pSectionData[iSectionSize] = '\0';

		switch(cSectionID) {
			case 'a':
				if(this->sDescription) goto error;
				this->sDescription = (char*)pSectionData;
				break;

			case 'b':
				if(this->sPart) goto error;
				this->sPart = (char*)pSectionData;
				break;

			case 'c':
				if(this->sTimestampDate) goto error;
				this->sTimestampDate = (char*)pSectionData;
				break;

			case 'd':
				if(this->sTimestampTime) goto error;
				this->sTimestampTime = (char*)pSectionData;
				break;

			case 'e':
				if(this->dBitStream) goto error;
				this->dBitStream = pSectionData;
				this->iBitStreamLength = iSectionSize;
				break;

			default:
				fprintf(stderr, "WARNING: Unsupported bit file section: 0x%02X\n", cSectionID);
				free(pSectionData);
				break;
		}
	}

	*pSections = this;
	return 0;

	error:
	free(pSectionData);
	error_nofree:
	BitFile_Free(this);
	return -1;
}

void BitFile_Free(struct BitFile_Sections* this) {
	free(this->sDescription);
	free(this->sPart);
	free(this->sTimestampDate);
	free(this->sTimestampTime);
	free(this->dBitStream);
	free(this);
}
