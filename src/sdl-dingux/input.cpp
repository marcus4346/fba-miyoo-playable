#include <SDL/SDL.h>

#include "burner.h"

extern SDL_Joystick *joys[4];
extern char joyCount;
extern unsigned char ServiceRequest;
extern unsigned char P1P2Start;

int nAnalogSpeed=0x0100;

#define MAX_INPUT_inp (19)

struct GameInput {
	//unsigned char *pVal;  // Destination for the Input Value
	union {
		UINT8* pVal;					// Most inputs use a char*
		UINT16* pShortVal;				// All analog inputs use a short*
	};
	//int *ipVal;			 //for x axis steering
	unsigned char nType;  // 0=binary (0,1) 1=analog (0x01-0xFF) 2=dip switch
	unsigned char nConst;
	int nBit;   // bit offset of Keypad data
};

struct GameDIPBank {
	unsigned int nInput;
	UINT8 *pVal;
	UINT8 nDefault;
	UINT8 nConst;
};

struct DIPInfo{
	unsigned int nDIP;
	struct GameDIPBank *DIPData;
} DIPInfo;

struct DIPConfigEntry {
	int nBank;
	UINT8 nValue;
};

static struct DIPConfigEntry *pDIPConfig = NULL;
static int nDIPConfigCount = 0;

// Mapping of PC inputs to game inputs
struct GameInput GameInput[4][MAX_INPUT_inp];
unsigned int nGameInpCount = 0;
static bool bInputOk = false;
unsigned char *ServiceDip = 0;
unsigned char *P1Start = 0;
unsigned char *P2Start = 0;

int DoInputBlank(int /*bDipSwitch*/)
{
  int iJoyNum = 0;
  unsigned int i=0;
	unsigned int nDIPIndex = 0;
  // Reset all inputs to undefined (even dip switches, if bDipSwitch==1)
  char controlName[MAX_INPUT_inp];

	if (DIPInfo.DIPData) {
		free(DIPInfo.DIPData);
		DIPInfo.DIPData = NULL;
	}
	DIPInfo.nDIP = 0;
	for (i = 0; i < nGameInpCount; i++) {
		struct BurnInputInfo bii;
		memset(&bii, 0, sizeof(bii));
		BurnDrvGetInputInfo(&bii, i);
		if (bii.nType == BIT_DIPSWITCH) DIPInfo.nDIP++;
	}
	if (DIPInfo.nDIP) {
		DIPInfo.DIPData = (struct GameDIPBank *)malloc(DIPInfo.nDIP * sizeof(struct GameDIPBank));
		if (DIPInfo.DIPData == NULL) {
			DIPInfo.nDIP = 0;
			return 1;
		}
		memset(DIPInfo.DIPData, 0, DIPInfo.nDIP * sizeof(struct GameDIPBank));
	}

  // Get the targets in the library for the Input Values
  for (i=0; i<nGameInpCount; i++)
  {
	struct BurnInputInfo bii;
	memset(&bii,0,sizeof(bii));
	BurnDrvGetInputInfo(&bii,i);

	printf("c %s\n",bii.szInfo);

	//if (bDipSwitch==0 && bii.nType==2) continue; // Don't blank the dip switches

	if (bii.nType==BIT_DIPSWITCH)
	{
		DIPInfo.DIPData[nDIPIndex].nInput = i;
		DIPInfo.DIPData[nDIPIndex].pVal = bii.pVal;
		nDIPIndex++;
	}

	if ((bii.szInfo[0]=='p') || (bii.szInfo[0]=='m'))
		if (bii.szInfo[0]=='m') iJoyNum=0; else iJoyNum = bii.szInfo[1] - '1';
	else
	{
		if (strcmp(bii.szInfo, "diag") == 0 || strcmp(bii.szInfo, "test") == 0)
		{
			ServiceDip = bii.pVal;
		}
		continue;
	}

	sprintf(controlName,"p%i coin",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][0].nBit = 4;
		GameInput[iJoyNum][0].pVal = bii.pVal;
		GameInput[iJoyNum][0].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i start",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][1].nBit = 5;
		GameInput[iJoyNum][1].pVal = bii.pVal;
		GameInput[iJoyNum][1].nType = bii.nType;
		switch (iJoyNum)
		{
			case 0:
				P1Start = bii.pVal;
			break;
			case 1:
				P2Start = bii.pVal;
			break;
		}
	}
	else {
	sprintf(controlName,"p%i up",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][2].nBit = 0;
		GameInput[iJoyNum][2].pVal = bii.pVal;
		GameInput[iJoyNum][2].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i down",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][3].nBit = 1;
		GameInput[iJoyNum][3].pVal = bii.pVal;
		GameInput[iJoyNum][3].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i left",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][4].nBit = 2;
		GameInput[iJoyNum][4].pVal = bii.pVal;
		GameInput[iJoyNum][4].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i right",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][5].nBit = 3;
		GameInput[iJoyNum][5].pVal = bii.pVal;
		GameInput[iJoyNum][5].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i x-axis",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][12].nBit = 2;
		GameInput[iJoyNum][12].pShortVal = bii.pShortVal;
		GameInput[iJoyNum][12].nType = bii.nType;
		GameInput[iJoyNum][13].nBit = 3;
		GameInput[iJoyNum][13].pShortVal = bii.pShortVal;
		GameInput[iJoyNum][13].nType = bii.nType;
	}
	else {
	sprintf(controlName,"mouse x-axis");
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][12].nBit = 2;
		GameInput[iJoyNum][12].pShortVal = bii.pShortVal;
		GameInput[iJoyNum][12].nType = bii.nType;
		GameInput[iJoyNum][13].nBit = 3;
		GameInput[iJoyNum][13].pShortVal = bii.pShortVal;
		GameInput[iJoyNum][13].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i y-axis",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][14].nBit = 0;
		GameInput[iJoyNum][14].pShortVal = bii.pShortVal;
		GameInput[iJoyNum][14].nType = bii.nType;
		GameInput[iJoyNum][15].nBit = 1;
		GameInput[iJoyNum][15].pShortVal = bii.pShortVal;
		GameInput[iJoyNum][15].nType = bii.nType;
	}
	else {
	sprintf(controlName,"mouse y-axis");
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][14].nBit = 0;
		GameInput[iJoyNum][14].pShortVal = bii.pShortVal;
		GameInput[iJoyNum][14].nType = bii.nType;
		GameInput[iJoyNum][15].nBit = 1;
		GameInput[iJoyNum][15].pShortVal = bii.pShortVal;
		GameInput[iJoyNum][15].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i z-axis",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][16].nBit = 11;
		GameInput[iJoyNum][16].pShortVal = bii.pShortVal;
		GameInput[iJoyNum][16].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i fire 1",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][6].nBit = 6;
		GameInput[iJoyNum][6].pVal = bii.pVal;
		GameInput[iJoyNum][6].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i fire 2",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][7].nBit = 7;
		GameInput[iJoyNum][7].pVal = bii.pVal;
		GameInput[iJoyNum][7].nType = bii.nType;
	}
	else {
	sprintf(controlName,"mouse button 1");
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][6].nBit = 6;
		GameInput[iJoyNum][6].pVal = bii.pVal;
		GameInput[iJoyNum][6].nType = bii.nType;
	}
	else {
	sprintf(controlName,"mouse button 2");
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][7].nBit = 7;
		GameInput[iJoyNum][7].pVal = bii.pVal;
		GameInput[iJoyNum][7].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i fire 3",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][8].nBit = 8;
		GameInput[iJoyNum][8].pVal = bii.pVal;
		GameInput[iJoyNum][8].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i fire 4",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][9].nBit = 9;
		GameInput[iJoyNum][9].pVal = bii.pVal;
		GameInput[iJoyNum][9].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i fire 5",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][10].nBit = 10;
		GameInput[iJoyNum][10].pVal = bii.pVal;
		GameInput[iJoyNum][10].nType = bii.nType;
	}
	else {
	sprintf(controlName,"p%i fire 6",iJoyNum+1);
	if (strcmp(bii.szInfo, controlName) == 0)
	{
		GameInput[iJoyNum][11].nBit = 11;
		GameInput[iJoyNum][11].pVal = bii.pVal;
		GameInput[iJoyNum][11].nType = bii.nType;
	}}}}}}}}}}}}}}}}}}}

#if 0
if (pgi->pVal != NULL)
	printf("GI(%02d): %-12s 0x%02x 0x%02x %-12s, [%d]\n", i, bii.szName, bii.nType, *(pgi->pVal), bii.szInfo, pgi->nBit );
else
	printf("GI(%02d): %-12s 0x%02x N/A  %-12s, [%d]\n", i, bii.szName, bii.nType, bii.szInfo, pgi->nBit );
#endif

  }
  return 0;
}

int InpInit()
{
	unsigned int i=0;
	int nRet=0;

	bInputOk = false;
	// Count the number of inputs
	nGameInpCount=0;
	for (i=0;i<0x1000;i++) {
		nRet = BurnDrvGetInputInfo(NULL,i);
		if (nRet!=0) {   // end of input list
			nGameInpCount=i;
			break;
		}
	}

	memset(GameInput,0,MAX_INPUT_inp*4*sizeof(struct GameInput));
	ServiceDip = NULL;
	P1Start = NULL;
	P2Start = NULL;
	if (DoInputBlank(1)) return 1;

	bInputOk = true;

	return 0;
}

int InpExit()
{
	bInputOk = false;
	nGameInpCount = 0;
	if (DIPInfo.DIPData) free(DIPInfo.DIPData);
	DIPInfo.DIPData = NULL;
	DIPInfo.nDIP = 0;
	return 0;
}

int InpMake(unsigned int key[])
{
	if (!bInputOk) return 1;

	static int skip = 0;
	skip ++;
	if (skip > 1) skip = 0;
	if (skip != 1) return 1;

	unsigned int i=0;
	unsigned int down = 0;
	short numJoy = joyCount?joyCount:1;
	if (ServiceDip)
	{
		*(ServiceDip)=ServiceRequest;
	}
	int nJoy;
	for (short joyNum=0;joyNum<numJoy;joyNum++)
	{
		for (i=0; i<MAX_INPUT_inp; i++)
		{
			nJoy=0;
			if (GameInput[joyNum][i].pVal == NULL) continue;

			if ( GameInput[joyNum][i].nBit >= 0 )
			{
				down = key[joyNum] & (1U << GameInput[joyNum][i].nBit);

				if (GameInput[joyNum][i].nType!=1) {
					// Set analog controls to full

					if (i<12)
					{
						if (down) *(GameInput[joyNum][i].pVal)=0xff; else *(GameInput[joyNum][i].pVal)=0x01;
					}

					if (i==12) //analogue x
					{
						nJoy=SDL_JoystickGetAxis(joys[joyNum],0) << 1;
						if (down) nJoy=-32768 << 1;
						nJoy *= nAnalogSpeed;
						nJoy >>= 13;

						// Clip axis to 8 bits
						if (nJoy < -32768) {
							nJoy = -32768;
						}
						if (nJoy >  32767) {
							nJoy =  32767;
						}

						*(GameInput[joyNum][i].pShortVal)=nJoy;
					}
					if (i==13) //analogue right
					{
						if (down) {nJoy=32768 << 1;
						nJoy *= nAnalogSpeed;
						nJoy >>= 13;

						// Clip axis to 8 bits
						if (nJoy < -32768) {
							nJoy = -32768;
						}
						if (nJoy >  32767) {
							nJoy =  32767;
						}
						*(GameInput[joyNum][i].pShortVal)=nJoy;}
					}
					if (i==14) //analogue y
					{
						nJoy=SDL_JoystickGetAxis(joys[joyNum],1) << 1;
						if (down) nJoy=-32768 << 1;
						nJoy *= nAnalogSpeed;
						nJoy >>= 13;

						// Clip axis to 8 bits
						if (nJoy < -32768) {
							nJoy = -32768;
						}
						if (nJoy >  32767) {
							nJoy =  32767;
						}
						*(GameInput[joyNum][i].pShortVal)=nJoy;
					}
					if (i==15) //analogue down
					{
						if (down) {nJoy=32768 << 1;
						nJoy *= nAnalogSpeed;
						nJoy >>= 13;

						// Clip axis to 8 bits
						if (nJoy < -32768) {
							nJoy = -32768;
						}
						if (nJoy >  32767) {
							nJoy =  32767;
						}
						*(GameInput[joyNum][i].pShortVal)=nJoy;}
					}
					if (i==16) //analogue z
					{
						nJoy=(-SDL_JoystickGetAxis(joys[joyNum+1],1)) << 1;
						//printf("%d\n",nJoy);
						if (down) nJoy=32768 << 1;
						nJoy *= nAnalogSpeed;
						nJoy >>= 13;

						// Clip axis to 8 bits
						if (nJoy < -32768) {
							nJoy = -32768;
						}
						if (nJoy >  32767) {
							nJoy =  32767;
						}
						*(GameInput[joyNum][i].pShortVal)=nJoy;
					}
				}
				else
				{
					// Binary controls
					if (down) *(GameInput[joyNum][i].pVal)=1;	else *(GameInput[joyNum][i].pVal)=0;
					//(GameInput[joyNum][i].pVal)=0;
				}
			}
		}
	}
	for (i=0; i<(int)DIPInfo.nDIP; i++) {
		if (DIPInfo.DIPData[i].pVal == NULL)
			continue;
		*(DIPInfo.DIPData[i].pVal) = DIPInfo.DIPData[i].nConst;
	}
	if (P1P2Start)
	{
		*(P1Start) = *(P2Start) = 1;
	}
	return 0;
}

static int InpDIPFindBankByInput(int nInput)
{
	for (unsigned int i = 0; i < DIPInfo.nDIP; i++) {
		if ((int)DIPInfo.DIPData[i].nInput == nInput) return i;
	}

	return -1;
}

static int InpDIPConfigFind(int nBank)
{
	for (int i = 0; i < nDIPConfigCount; i++) {
		if (pDIPConfig[i].nBank == nBank) return i;
	}

	return -1;
}

static void InpDIPConfigRemove(int nEntry)
{
	if (nEntry < 0 || nEntry >= nDIPConfigCount) return;

	if (nEntry + 1 < nDIPConfigCount) {
		memmove(pDIPConfig + nEntry, pDIPConfig + nEntry + 1,
			(nDIPConfigCount - nEntry - 1) * sizeof(struct DIPConfigEntry));
	}
	nDIPConfigCount--;
	if (nDIPConfigCount == 0) {
		free(pDIPConfig);
		pDIPConfig = NULL;
	}
}

void InpDIPConfigClear()
{
	if (pDIPConfig) free(pDIPConfig);
	pDIPConfig = NULL;
	nDIPConfigCount = 0;
}

int InpDIPConfigSet(int nBank, int nValue)
{
	if (nBank < 0 || nValue < 0 || nValue > 0xFF) return 1;

	int nEntry = InpDIPConfigFind(nBank);
	if (nEntry >= 0) {
		pDIPConfig[nEntry].nValue = nValue;
		return 0;
	}

	struct DIPConfigEntry *pNew = (struct DIPConfigEntry *)realloc(
		pDIPConfig, (nDIPConfigCount + 1) * sizeof(struct DIPConfigEntry));
	if (pNew == NULL) return 1;
	pDIPConfig = pNew;

	int nInsert = nDIPConfigCount;
	while (nInsert > 0 && pDIPConfig[nInsert - 1].nBank > nBank) {
		pDIPConfig[nInsert] = pDIPConfig[nInsert - 1];
		nInsert--;
	}
	pDIPConfig[nInsert].nBank = nBank;
	pDIPConfig[nInsert].nValue = nValue;
	nDIPConfigCount++;

	return 0;
}

int InpDIPConfigGetEntry(int nEntry, int *pnBank, unsigned char *pnValue)
{
	if (nEntry < 0 || nEntry >= nDIPConfigCount) return 1;
	if (pnBank) *pnBank = pDIPConfig[nEntry].nBank;
	if (pnValue) *pnValue = pDIPConfig[nEntry].nValue;
	return 0;
}

static UINT8 InpDIPConfigGetValue(int nBank)
{
	int nEntry = InpDIPConfigFind(nBank);
	if (nEntry >= 0) return pDIPConfig[nEntry].nValue;
	return DIPInfo.DIPData[nBank].nDefault;
}

int InpDIPConfigGetBankCount()
{
	return DIPInfo.nDIP;
}

int InpDIPConfigGetByBank(int nBank, unsigned char *pnValue)
{
	if (nBank < 0 || nBank >= (int)DIPInfo.nDIP || pnValue == NULL) return 1;
	*pnValue = InpDIPConfigGetValue(nBank);
	return 0;
}

int InpDIPConfigGetDefaultByInput(int nInput, unsigned char *pnValue)
{
	int nBank = InpDIPFindBankByInput(nInput);
	if (nBank < 0 || pnValue == NULL) return 1;
	*pnValue = DIPInfo.DIPData[nBank].nDefault;
	return 0;
}

int InpDIPConfigGetByInput(int nInput, unsigned char *pnValue)
{
	int nBank = InpDIPFindBankByInput(nInput);
	if (nBank < 0 || pnValue == NULL) return 1;
	*pnValue = InpDIPConfigGetValue(nBank);
	return 0;
}

int InpDIPConfigSetByInput(int nInput, unsigned char nMask, unsigned char nSetting)
{
	int nBank = InpDIPFindBankByInput(nInput);
	if (nBank < 0) return 1;

	UINT8 nValue = InpDIPConfigGetValue(nBank);
	nValue = (nValue & ~nMask) | (nSetting & nMask);
	int nEntry = InpDIPConfigFind(nBank);
	if (nValue == DIPInfo.DIPData[nBank].nDefault) {
		if (nEntry >= 0) InpDIPConfigRemove(nEntry);
		return 0;
	}

	return InpDIPConfigSet(nBank, nValue);
}

void InpDIP()
{
	struct BurnDIPInfo bdi;
	int i;
	int nDIPOffset = 0;

	// get dip switch offset
	for (i = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++)
		if (bdi.nFlags == 0xF0) {
			nDIPOffset = bdi.nInput;
			break;
		}

	for (i = 0; i < (int)DIPInfo.nDIP; i++) {
		DIPInfo.DIPData[i].nDefault = 0;
		DIPInfo.DIPData[i].nConst = 0;
	}

	// set DIP to driver defaults
	i = 0;
	while (BurnDrvGetDIPInfo(&bdi, i) == 0) {
		if (bdi.nFlags == 0xFF) {
			int nBank = InpDIPFindBankByInput(bdi.nInput + nDIPOffset);
			if (nBank >= 0) {
				struct GameDIPBank *pDip = DIPInfo.DIPData + nBank;
				pDip->nDefault = (pDip->nDefault & ~bdi.nMask) | (bdi.nSetting & bdi.nMask);
			}
		}
		i++;
	}

	for (i = 0; i < (int)DIPInfo.nDIP; i++) {
		DIPInfo.DIPData[i].nConst = DIPInfo.DIPData[i].nDefault;
		if (DIPInfo.DIPData[i].pVal) {
			*(DIPInfo.DIPData[i].pVal) = DIPInfo.DIPData[i].nConst;
		}
	}
}

void InpDIPApplyConfig()
{
	for (int i = nDIPConfigCount - 1; i >= 0; i--) {
		if (pDIPConfig[i].nBank >= (int)DIPInfo.nDIP ||
			pDIPConfig[i].nValue == DIPInfo.DIPData[pDIPConfig[i].nBank].nDefault) {
			InpDIPConfigRemove(i);
		}
	}

	for (unsigned int i = 0; i < DIPInfo.nDIP; i++) {
		DIPInfo.DIPData[i].nConst = InpDIPConfigGetValue(i);
		if (DIPInfo.DIPData[i].pVal) {
			*(DIPInfo.DIPData[i].pVal) = DIPInfo.DIPData[i].nConst;
		}
	}
}
