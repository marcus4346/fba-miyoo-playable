// Driver Save State module
#include "burner.h"
#include <errno.h>

FILE *bfp = NULL;

// If bAll=0 save/load all non-volatile ram to .fs
// If bAll=1 save/load all ram to .fs

// ------------ State len --------------------
static int nTotalLen = 0;

static int __cdecl StateLenAcb(struct BurnArea* pba)
{
	nTotalLen += pba->nLen;

	return 0;
}

static int StateInfo(int* pnLen, int* pnMinVer, int bAll)
{
	int nMin = 0;
	nTotalLen = 0;
	BurnAcb = StateLenAcb;

	BurnAreaScan(ACB_NVRAM, &nMin);						// Scan nvram
	if (bAll) {
		int m;
		BurnAreaScan(ACB_MEMCARD, &m);					// Scan memory card
		if (m > nMin) {									// Up the minimum, if needed
			nMin = m;
		}
		BurnAreaScan(ACB_VOLATILE, &m);					// Scan volatile ram
		if (m > nMin) {									// Up the minimum, if needed
			nMin = m;
		}
	}
	*pnLen = nTotalLen;
	*pnMinVer = nMin;

	return 0;
}

static int __cdecl ReadAcb(struct BurnArea* pba)
{
	fread(pba->Data, 1, pba->nLen, bfp);
	return 0;
}

// State load
int BurnStateLoadEmbed(FILE* fp, int nOffset, int bAll, int (*pLoadGame)())
{
	const char* szHeader = "FS1 ";						// Chunk identifier

	int nLen = 0;
	int nMin = 0, nFileVer = 0, nFileMin = 0;
	int t1 = 0, t2 = 0;
	char ReadHeader[4];
	char szForName[33];
	int nChunkSize = 0;
	unsigned char *Def = NULL;
	int nDefLen = 0;									// Deflated version
	int nRet = 0;

	if (nOffset >= 0) {
		fseek(fp, nOffset, SEEK_SET);
	} else {
		if (nOffset == -2) {
			fseek(fp, 0, SEEK_END);
		} else {
			fseek(fp, 0, SEEK_CUR);
		}
	}

	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fp);						// Read identifier
	if (memcmp(ReadHeader, szHeader, 4)) {				// Not the right file type
		return -2;
	}

	fread(&nChunkSize, 1, 4, fp);
	if (nChunkSize <= 0x40) {							// Not big enough
		return -1;
	}

	int nChunkData = ftell(fp);

	fread(&nFileVer, 1, 4, fp);							// Version of FB that this file was saved from

	fread(&t1, 1, 4, fp);								// Min version of FB that NV  data will work with
	fread(&t2, 1, 4, fp);								// Min version of FB that All data will work with

	if (bAll) {											// Get the min version number which applies to us
		nFileMin = t2;
	} else {
		nFileMin = t1;
	}

	fread(&nDefLen, 1, 4, fp);							// Get the size of the compressed data block

	memset(szForName, 0, sizeof(szForName));
	fread(szForName, 1, 32, fp);

	if (nBurnVer < nFileMin) {							// Error - emulator is too old to load this state
		return -5;
	}

	// Check the game the savestate is for, and load it if needed.
	{
		bool bLoadGame = false;

		if (nBurnDrvSelect < nBurnDrvCount) {
			if (strcmp(szForName, BurnDrvGetTextA(DRV_NAME))) {	// The save state is for the wrong game
				bLoadGame = true;
			}
		} else {										// No game loaded
			bLoadGame = true;
		}

		if (bLoadGame) {
			unsigned int nCurrentGame = nBurnDrvSelect;
			unsigned int i;
			for (i = 0; i < nBurnDrvCount; i++) {
				nBurnDrvSelect = i;
				if (strcmp(szForName, BurnDrvGetTextA(DRV_NAME)) == 0) {
					break;
				}
			}
			if (i == nBurnDrvCount) {
				nBurnDrvSelect = nCurrentGame;
				return -3;
			} else {
				if (pLoadGame == NULL) {
					return -1;
				}
				if (pLoadGame()) {
					return -1;
				}
			}
		}
	}

	StateInfo(&nLen, &nMin, bAll);
	if (nLen <= 0) {									// No memory to load
		return -1;
	}

	// Check if the save state is okay
	if (nFileVer < nMin) {								// Error - this state is too old and cannot be loaded.
		return -4;
	}

	fseek(fp, nChunkData + 0x30, SEEK_SET);				// Read current frame
	fread(&nCurrentFrame, 1, 4, fp);					//

	fseek(fp, 0x0C, SEEK_CUR);							// Move file pointer to the start of the compressed block

	bfp = fp;
	BurnAcb = ReadAcb;

	if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_WRITE, NULL);		// scan all ram, write (to driver <- decompress)
	else      BurnAreaScan(ACB_NVRAM    | ACB_WRITE, NULL);		// scan nvram,   write (to driver <- decompress)

	fseek(fp, nChunkData + nChunkSize, SEEK_SET);

	if (nRet) {
		return -1;
	} else {
		return 0;
	}
}

// State load
int BurnStateLoad(const char * szName, int bAll, int (*pLoadGame)())
{
	const char szHeader[] = "FBS ";						// File identifier
	char szReadHeader[4] = "";
	int nRet = 0;

	FILE* fp = fopen(szName, "rb");
	if (fp == NULL) {
		return 1;
	}

	fread(szReadHeader, 1, 4, fp);						// Read identifier
	if (memcmp(szReadHeader, szHeader, 4) == 0) {		// Check filetype
		nRet = BurnStateLoadEmbed(fp, -1, bAll, pLoadGame);
	}
    fclose(fp);

	if (nRet < 0) {
		return -nRet;
	} else {
		return 0;
	}
}

static int __cdecl WriteAcb(struct BurnArea *pba)
{
	//printf("WRITE ACB - len: %i, name: %s\n", pba->nLen, pba->szName);
	fwrite(pba->Data, 1, pba->nLen, bfp);
	nTotalLen += pba->nLen;
	return 0;
}

// Write a savestate as a chunk of an "FBS " file
// nOffset is the absolute offset from the beginning of the file
// -1: Append at current position
// -2: Append at EOF
int BurnStateSaveEmbed(FILE* fp, int nOffset, int bAll)
{
	const char* szHeader = "FS1 ";						// Chunk identifier

	int nLen = 0;
	int nNvMin = 0, nAMin = 0;
	int nZero = 0;
	char szGame[33];
	unsigned char *Def = NULL;
	int nDefLen = 0;									// Deflated version

	if (fp == NULL) {
		return -1;
	}

	StateInfo(&nLen, &nNvMin, 0);						// Get minimum version for NV part
	nAMin = nNvMin;
	if (bAll) {											// Get minimum version for All data
		StateInfo(&nLen, &nAMin, 1);
	}

	if (nLen <= 0) {									// No memory to save
		return -1;
	}

	if (nOffset >= 0) {
		fseek(fp, nOffset, SEEK_SET);
	} else {
		if (nOffset == -2) {
			fseek(fp, 0, SEEK_END);
		} else {
			fseek(fp, 0, SEEK_CUR);
		}
	}

	fwrite(szHeader, 1, 4, fp);							// Chunk identifier
	int nSizeOffset = ftell(fp);						// Reserve space to write the size of this chunk
	fwrite(&nZero, 1, 4, fp);							//

	fwrite(&nBurnVer, 1, 4, fp);						// Version of FB this was saved from
	fwrite(&nNvMin, 1, 4, fp);							// Min version of FB NV  data will work with
	fwrite(&nAMin, 1, 4, fp);							// Min version of FB All data will work with

	fwrite(&nZero, 1, 4, fp);							// Reserve space to write the compressed data size

	memset(szGame, 0, sizeof(szGame));					// Game name
	sprintf(szGame, "%.32s", BurnDrvGetTextA(DRV_NAME));			//
	fwrite(szGame, 1, 32, fp);							//

	fwrite(&nCurrentFrame, 1, 4, fp);					// Current frame

	fwrite(&nZero, 1, 4, fp);							// Reserved
	fwrite(&nZero, 1, 4, fp);							//
	fwrite(&nZero, 1, 4, fp);							//

	bfp = fp;
	nTotalLen = 0;
	BurnAcb = WriteAcb;

	if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_READ, NULL);		// scan all ram, read (from driver <- decompress)
	else      BurnAreaScan(ACB_NVRAM    | ACB_READ, NULL);		// scan nvram,   read (from driver <- decompress)
	nDefLen = nTotalLen;

	if (nDefLen & 3) {									// Chunk size must be a multiple of 4
		fwrite(&nZero, 1, 4 - (nDefLen & 3), fp);		// Pad chunk if needed
	}

	fseek(fp, nSizeOffset + 0x10, SEEK_SET);			// Write size of the compressed data
	fwrite(&nDefLen, 1, 4, fp);							//

	nDefLen = (nDefLen + 0x43) & ~3;					// Add for header size and align

	fseek(fp, nSizeOffset, SEEK_SET);					// Write size of the chunk
	fwrite(&nDefLen, 1, 4, fp);							//

	fseek (fp, 0, SEEK_END);							// Set file pointer to the end of the chunk

	return nDefLen;
}

// State save
int BurnStateSave(const char * szName, int bAll)
{
	const char szHeader[] = "FBS ";						// File identifier
	int nLen = 0, nVer = 0;
	int nRet = 0;

	if (bAll) {											// Get amount of data
		StateInfo(&nLen, &nVer, 1);
	} else {
		StateInfo(&nLen, &nVer, 0);
	}
	if (nLen <= 0) {									// No data, so exit without creating a savestate
		return 0;										// Don't return an error code
	}

	FILE* fp = fopen(szName, "wb");
	if (fp == NULL) {
		return 1;
	}

	fwrite(&szHeader, 1, 4, fp);
	nRet = BurnStateSaveEmbed(fp, -1, bAll);
    fclose(fp);

	if (nRet < 0) {
		return 1;
	} else {
		return 0;
	}
}

int nSavestateSlot = 0;
 
static char szSavestateName[MAX_PATH];

int StatedLoad(int nSlot)
{
	sprintf(szSavestateName, "%s/%s%i.sav", szAppSavePath, BurnDrvGetText(DRV_NAME), nSlot);
	printf("StatedLoad: %s\n", szSavestateName);
	return BurnStateLoad(szSavestateName, 1, &DrvInitCallback);
}

int StatedSave(int nSlot)
{
	sprintf(szSavestateName, "%s/%s%i.sav", szAppSavePath, BurnDrvGetText(DRV_NAME), nSlot);
	printf("StatedSave: %s\n", szSavestateName);
	return BurnStateSave(szSavestateName, 1);
}

// ------------ Automatic non-volatile storage ------------------------------

typedef int (__cdecl *AutoBurnAcb)(struct BurnArea* pba);

static unsigned char* pAutoData = NULL;
static int nAutoDataLen = 0;
static int nAutoDataPos = 0;
static int nAutoScanError = 0;

static int __cdecl AutoLenAcb(struct BurnArea* pba)
{
	if (pba->nLen > 0x7FFFFFFF - nAutoDataLen) {
		nAutoScanError = 1;
		return 1;
	}

	nAutoDataLen += pba->nLen;
	return 0;
}

static int AutoGetLen(int nType)
{
	AutoBurnAcb pOldBurnAcb = BurnAcb;

	nAutoDataLen = 0;
	nAutoScanError = 0;
	BurnAcb = AutoLenAcb;
	BurnAreaScan(nType, NULL);
	BurnAcb = pOldBurnAcb;

	return nAutoScanError ? -1 : nAutoDataLen;
}

static int __cdecl AutoSaveAcb(struct BurnArea* pba)
{
	if (pba->nLen > (unsigned int)(nAutoDataLen - nAutoDataPos)) {
		nAutoScanError = 1;
		return 1;
	}

	memcpy(pAutoData + nAutoDataPos, pba->Data, pba->nLen);
	nAutoDataPos += pba->nLen;
	return 0;
}

static int __cdecl AutoLoadAcb(struct BurnArea* pba)
{
	if (pba->nLen > (unsigned int)(nAutoDataLen - nAutoDataPos)) {
		nAutoScanError = 1;
		return 1;
	}

	memcpy(pba->Data, pAutoData + nAutoDataPos, pba->nLen);
	nAutoDataPos += pba->nLen;
	return 0;
}

// 0 = loaded, 1 = file does not exist, 2 = invalid or unreadable
static int AutoReadFile(const char* szName, unsigned char* pData, int nLen)
{
	FILE* fp = fopen(szName, "rb");
	if (fp == NULL) {
		return errno == ENOENT ? 1 : 2;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return 2;
	}

	long nFileLen = ftell(fp);
	if (nFileLen != nLen || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return 2;
	}

	int nRet = fread(pData, 1, nLen, fp) == (unsigned int)nLen ? 0 : 2;
	if (fclose(fp) != 0) nRet = 2;
	return nRet;
}

static int AutoWriteFile(const char* szName, const unsigned char* pData, int nLen)
{
	char szTempName[MAX_PATH];
	if (snprintf(szTempName, sizeof(szTempName), "%s.tmp", szName) >= (int)sizeof(szTempName)) {
		return 1;
	}

	FILE* fp = fopen(szTempName, "wb");
	if (fp == NULL) return 1;

	int nRet = 0;
	if (fwrite(pData, 1, nLen, fp) != (unsigned int)nLen) nRet = 1;
	if (fflush(fp) != 0) nRet = 1;
	if (fclose(fp) != 0) nRet = 1;

	if (nRet == 0 && rename(szTempName, szName) != 0) nRet = 1;
	if (nRet) remove(szTempName);
	return nRet;
}

static int AutoMakeName(char* szName, int nNameLen, const char* szPath, const char* szExtension)
{
	int nWritten = snprintf(szName, nNameLen, "%s/%s.%s", szPath,
		BurnDrvGetTextA(DRV_NAME), szExtension);
	return nWritten < 0 || nWritten >= nNameLen ? 1 : 0;
}

static int AutoLoadNvram()
{
	int nLen = AutoGetLen(ACB_NVRAM);
	if (nLen <= 0) return nLen < 0 ? 1 : 0;

	char szName[MAX_PATH];
	if (AutoMakeName(szName, sizeof(szName), szAppNvramPath, "nv")) return 1;

	unsigned char* pData = (unsigned char*)malloc(nLen);
	if (pData == NULL) return 1;

	int nRead = AutoReadFile(szName, pData, nLen);
	if (nRead == 0) {
		AutoBurnAcb pOldBurnAcb = BurnAcb;
		pAutoData = pData;
		nAutoDataLen = nLen;
		nAutoDataPos = 0;
		nAutoScanError = 0;
		BurnAcb = AutoLoadAcb;
		BurnAreaScan(ACB_NVRAM | ACB_WRITE, NULL);
		BurnAcb = pOldBurnAcb;

		if (nAutoScanError || nAutoDataPos != nLen) {
			printf("NVRAM load failed: scan size changed for %s\n", szName);
			nRead = 2;
		} else {
			printf("NVRAM loaded: %s\n", szName);
		}
	} else if (nRead == 2) {
		printf("NVRAM load ignored: invalid or unreadable file %s\n", szName);
	}

	free(pData);
	pAutoData = NULL;
	return nRead == 2 ? 1 : 0;
}

static int AutoSaveNvram()
{
	int nLen = AutoGetLen(ACB_NVRAM);
	if (nLen <= 0) return nLen < 0 ? 1 : 0;

	char szName[MAX_PATH];
	if (AutoMakeName(szName, sizeof(szName), szAppNvramPath, "nv")) return 1;

	unsigned char* pData = (unsigned char*)malloc(nLen);
	if (pData == NULL) return 1;

	AutoBurnAcb pOldBurnAcb = BurnAcb;
	pAutoData = pData;
	nAutoDataLen = nLen;
	nAutoDataPos = 0;
	nAutoScanError = 0;
	BurnAcb = AutoSaveAcb;
	BurnAreaScan(ACB_NVRAM | ACB_READ, NULL);
	BurnAcb = pOldBurnAcb;

	int nRet = nAutoScanError || nAutoDataPos != nLen || AutoWriteFile(szName, pData, nLen);
	if (nRet) {
		printf("NVRAM save failed: %s\n", szName);
	} else {
		printf("NVRAM saved: %s\n", szName);
	}

	free(pData);
	pAutoData = NULL;
	return nRet;
}

static int AutoLoadMemcard()
{
	int nLen = AutoGetLen(ACB_MEMCARD);
	if (nLen <= 0) return nLen < 0 ? 1 : 0;

	char szName[MAX_PATH];
	if (AutoMakeName(szName, sizeof(szName), szAppMemcardPath, "mem")) return 1;

	unsigned char* pData = (unsigned char*)malloc(nLen);
	if (pData == NULL) return 1;

	int nRead = AutoReadFile(szName, pData, nLen);
	if (nRead != 0) {
		memset(pData, 0, nLen);
		if (nRead == 1) {
			printf("Memory card created: %s\n", szName);
		} else {
			printf("Memory card load ignored: invalid or unreadable file %s\n", szName);
		}
	}

	AutoBurnAcb pOldBurnAcb = BurnAcb;
	pAutoData = pData;
	nAutoDataLen = nLen;
	nAutoDataPos = 0;
	nAutoScanError = 0;
	BurnAcb = AutoLoadAcb;
	BurnAreaScan(ACB_MEMCARD | ACB_WRITE, NULL);
	BurnAcb = pOldBurnAcb;

	int nRet = nAutoScanError || nAutoDataPos != nLen;
	if (nRet) {
		printf("Memory card insert failed: %s\n", szName);
	} else if (nRead == 0) {
		printf("Memory card loaded: %s\n", szName);
	}

	free(pData);
	pAutoData = NULL;
	return nRet;
}

static int AutoSaveMemcard()
{
	int nLen = AutoGetLen(ACB_MEMCARD);
	if (nLen <= 0) return nLen < 0 ? 1 : 0;

	char szName[MAX_PATH];
	if (AutoMakeName(szName, sizeof(szName), szAppMemcardPath, "mem")) return 1;

	unsigned char* pData = (unsigned char*)malloc(nLen);
	if (pData == NULL) return 1;
	memset(pData, 0, nLen);

	AutoBurnAcb pOldBurnAcb = BurnAcb;
	pAutoData = pData;
	nAutoDataLen = nLen;
	nAutoDataPos = 0;
	nAutoScanError = 0;
	BurnAcb = AutoSaveAcb;
	BurnAreaScan(ACB_MEMCARD | ACB_READ, NULL);
	BurnAcb = pOldBurnAcb;

	// Some cards report a smaller active size when ejected. The remainder of
	// the fixed-capacity raw image stays zero-filled.
	int nRet = nAutoScanError || AutoWriteFile(szName, pData, nLen);
	if (nRet) {
		printf("Memory card save failed: %s\n", szName);
	} else {
		printf("Memory card saved: %s\n", szName);
	}

	free(pData);
	pAutoData = NULL;
	return nRet;
}

int StatedAutoLoad()
{
	int nRet = AutoLoadNvram();
	if (AutoLoadMemcard()) nRet = 1;
	return nRet;
}

int StatedAutoSave()
{
	int nRet = AutoSaveNvram();
	if (AutoSaveMemcard()) nRet = 1;
	return nRet;
}
