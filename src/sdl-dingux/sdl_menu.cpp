/*
 * FinalBurn Alpha for Dingux/OpenDingux
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <SDL/SDL.h>

#include "version.h"
#include "burner.h"
#include "snd.h"
#include "sdl_run.h"
#include "sdl_video.h"

#define _s(A) #A
#define _a(A) _s(A)
#define VERSION _a(VER_MAJOR.VER_MINOR.VER_BETA.VER_ALPHA)

#define color16(red, green, blue) ((red << 11) | (green << 5) | blue)

#define COLOR_BG            color16(05, 03, 02)
#define COLOR_ROM_INFO      color16(22, 36, 26)
#define COLOR_ACTIVE_ITEM   color16(31, 63, 31)
#define COLOR_INACTIVE_ITEM color16(13, 40, 18)
#define COLOR_ACTIVE_CHANGED color16(31, 60, 00)
#define COLOR_INACTIVE_CHANGED color16(24, 40, 00)
#define COLOR_FRAMESKIP_BAR color16(15, 31, 31)
#define COLOR_HELP_TEXT     color16(16, 40, 24)

/* SDL declarations */
extern SDL_Surface *screen;
SDL_Surface *menuSurface = NULL; // menu rendering

/* type definitions */
typedef struct {
	char *itemName;       // item name
	int *itemPar;         // item parameter
	int itemParMaxValue;  // item max parameter value
	char **itemParName;   // item parameter name
	void (*itemOnA)();    // item action on A press
} MENUITEM;

typedef struct {
	int itemNum; // number of items
	int itemCur; // current item
	MENUITEM *m; // array of items
} MENU;

/* prototypes */
static void gui_Stub() { }
static void gui_LoadState() { extern int done; if(!StatedLoad(nSavestateSlot)) done = 1; }
static void gui_Savestate() { StatedSave(nSavestateSlot); }
static void call_exit() { extern int done; GameLooping = false; done = 1; }
static void call_continue() { extern int done; done = 1; }
static void gui_KeyMenuRun();
static void gui_DipMenuRun();
static void gui_reset();

/* data definitions */
char *gui_KeyNames[] = {"A", "B", "X", "Y", "L", "R"};
int gui_KeyData[] = {0, 1, 2, 3, 4, 5};
int gui_KeyValue[] = {SDLK_LALT, SDLK_LCTRL, SDLK_LSHIFT, SDLK_SPACE, SDLK_TAB, SDLK_BACKSPACE};
char *gui_SoundDrvNames[] = {"No sound", "LIBAO", "SDL mutex", "SDL"};
char *gui_SoundSampleRates[] = {"11025", "16000", "22050", "32000", "44100"};

MENUITEM gui_MainMenuItems[] = {
	{(char *)"Continue", NULL, 0, NULL, &call_continue},
	{(char *)"Key config", NULL, 0, NULL, &gui_KeyMenuRun},
	{(char *)"DIP switches", NULL, 0, NULL, &gui_DipMenuRun},
	{(char *)"Load state: ", &nSavestateSlot, 9, NULL, &gui_LoadState},
	{(char *)"Save state: ", &nSavestateSlot, 9, NULL, &gui_Savestate},
	{(char *)"Reset", NULL, 0, NULL, &gui_reset},
	{(char *)"Exit", NULL, 0, NULL, &call_exit},
	{NULL, NULL, 0, NULL, NULL}
};

MENU gui_MainMenu = { 7, 0, (MENUITEM *)&gui_MainMenuItems };

MENUITEM gui_KeyMenuItems[] = {
	{(char *)"Fire 1   - ", &gui_KeyData[0], 5, (char **)&gui_KeyNames, NULL},
	{(char *)"Fire 2   - ", &gui_KeyData[1], 5, (char **)&gui_KeyNames, NULL},
	{(char *)"Fire 3   - ", &gui_KeyData[2], 5, (char **)&gui_KeyNames, NULL},
	{(char *)"Fire 4   - ", &gui_KeyData[3], 5, (char **)&gui_KeyNames, NULL},
	{(char *)"Fire 5   - ", &gui_KeyData[4], 5, (char **)&gui_KeyNames, NULL},
	{(char *)"Fire 6   - ", &gui_KeyData[5], 5, (char **)&gui_KeyNames, NULL},
	{NULL, NULL, 0, NULL, NULL}
};

MENU gui_KeyMenu = { 6, 0, (MENUITEM *)&gui_KeyMenuItems };

int done = 0; // flag to indicate exit status
extern unsigned char gui_font[2048];

/* local functions */

void gui_Flip()
{
	SDL_Rect dstrect;

	dstrect.x = (screen->w - 320) / 2;
	dstrect.y = (screen->h - 240) / 2;

	SDL_BlitSurface(menuSurface, 0, screen, &dstrect);
	SDL_Flip(screen);
}

/*
	Prints char on a menu surface
*/
void DrawChar(SDL_Surface *s, int x, int y, unsigned char a, int fg_color, int bg_color)
{
	Uint16 *dst;
	int w, h;

	if(SDL_MUSTLOCK(s)) SDL_LockSurface(s);
	for(h = 8; h; h--) {
		dst = (Uint16 *)s->pixels + (y+8-h)*s->w + x;
		for(w = 8; w; w--) {
			Uint16 color = bg_color; // background
			if((gui_font[a*8 + (8-h)] >> w) & 1) color = fg_color; // test bits 876543210
			*dst++ = color;
		}
	}
	if(SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);
}

/*
	Draws a string on a menu surface
*/
void DrawString(const char *s, unsigned short fg_color, unsigned short bg_color, int x, int y)
{
	int i, j = strlen(s);
	for(i = 0; i < j; i++, x += 8) DrawChar(menuSurface, x, y, s[i], fg_color, bg_color);
}

void ShowMenuItem(int x, int y, MENUITEM *m, int fg_color)
{
	static char i_str[24];

	// if no parameters, show simple menu item
	if(m->itemPar == NULL) DrawString(m->itemName, fg_color, COLOR_BG, x, y);
	else {
		if(m->itemParName == NULL) {
			// if parameter is a digit
			sprintf(i_str, "%s%i", m->itemName, *m->itemPar);
		} else {
			// if parameter is a name in array
			sprintf(i_str, "%s%s", m->itemName, *(m->itemParName + *m->itemPar));
		}
		DrawString(i_str, fg_color, COLOR_BG, x, y);
	}
}

/*
	Shows menu items and pointing arrow
*/
void ShowMenu(MENU *menu)
{
	int i;
	MENUITEM *mi = menu->m;

	// clear buffer
	SDL_FillRect(menuSurface, NULL, COLOR_BG);

	// show menu lines
	for(i = 0; i < menu->itemNum; i++, mi++) {
		int fg_color;

		if(menu->itemCur == i) fg_color = COLOR_ACTIVE_ITEM; else fg_color = COLOR_INACTIVE_ITEM;
		ShowMenuItem(80, (18 + i) * 8, mi, fg_color);
	}

	// show preview screen
	//ShowPreview(menu);

	// print info string
	//DrawString("Press B to return to game", COLOR_HELP_TEXT, COLOR_BG, 56, 220);
	DrawString("FinalBurn Alpha (" VERSION ")", COLOR_HELP_TEXT, COLOR_BG, 52, 2);
#ifdef USE_QSOUND_FBNEO
	DrawString("with QSound from FinalBurn Neo", COLOR_HELP_TEXT, COLOR_BG, 40, 12);
#endif
}

/*
	Main function that runs all the stuff
*/
void gui_MenuRun(MENU *menu)
{
	SDL_Event gui_event;
	MENUITEM *mi;

	done = 0;

	while(!done) {
		mi = menu->m + menu->itemCur; // pointer to highlit menu option

		while(SDL_PollEvent(&gui_event)) {
			if(gui_event.type == SDL_KEYDOWN) {
				// DINGOO A - apply parameter or enter submenu
				if(gui_event.key.keysym.sym == SDLK_LALT) if(mi->itemOnA != NULL) (*mi->itemOnA)();
				// DINGOO B - exit or back to previous menu
				if(gui_event.key.keysym.sym == SDLK_LCTRL) return;
				// DINGOO UP - arrow down
				if(gui_event.key.keysym.sym == SDLK_UP) if(--menu->itemCur < 0) menu->itemCur = menu->itemNum - 1;
				// DINGOO DOWN - arrow up
				if(gui_event.key.keysym.sym == SDLK_DOWN) if(++menu->itemCur == menu->itemNum) menu->itemCur = 0;
				// DINGOO LEFT - decrease parameter value
				if(gui_event.key.keysym.sym == SDLK_LEFT) {
					if(mi->itemPar != NULL && *mi->itemPar > 0) *mi->itemPar -= 1;
				}
				// DINGOO RIGHT - increase parameter value
				if(gui_event.key.keysym.sym == SDLK_RIGHT) {
					if(mi->itemPar != NULL && *mi->itemPar < mi->itemParMaxValue) *mi->itemPar += 1;
				}
			}
		}
		if(!done) ShowMenu(menu); // show menu items
		SDL_Delay(16);
		gui_Flip();
	}
}

static void gui_KeyMenuRun()
{
	// key decode
	int *key = &keymap.fire1;
	for(int i = 0; i < 6; key++, i++)
		for(int j = 0; j < 6; j++) if(gui_KeyValue[j] == *key) gui_KeyData[i] = j;

	gui_MenuRun(&gui_KeyMenu);

	// key encode
	key = &keymap.fire1;
	for(int i = 0; i < 6; key++, i++)
		*key = gui_KeyValue[gui_KeyData[i]];
}

typedef struct {
	int nGroup;
} DIPMENUITEM;

static int gui_DipGetOffset()
{
	struct BurnDIPInfo bdi;
	for (int i = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++) {
		if (bdi.nFlags == 0xF0) return bdi.nInput;
	}

	return 0;
}

static bool gui_DipCheckSetting(int nSetting)
{
	struct BurnDIPInfo bdi;
	if (BurnDrvGetDIPInfo(&bdi, nSetting)) return false;

	unsigned char nValue;
	int nOffset = gui_DipGetOffset();
	if (InpDIPConfigGetByInput(bdi.nInput + nOffset, &nValue)) return false;
	if ((nValue & bdi.nMask) != bdi.nSetting) return false;

	unsigned char nFlags = bdi.nFlags;
	for (int i = 1; i < (nFlags & 0x0F); i++) {
		if (BurnDrvGetDIPInfo(&bdi, nSetting + i)) return false;
		if (InpDIPConfigGetByInput(bdi.nInput + nOffset, &nValue)) return false;
		bool bMatch = (nValue & bdi.nMask) == bdi.nSetting;
		if (nFlags & 0x80) {
			if (bMatch) return false;
		} else {
			if (!bMatch) return false;
		}
	}

	return true;
}

static int gui_DipBuildItems(DIPMENUITEM **ppItems)
{
	DIPMENUITEM *pItems = NULL;
	int nItemCount = 0;
	int nGroup = -1;
	struct BurnDIPInfo bdi;

	for (int i = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; ) {
		if ((bdi.nFlags & 0xF0) == 0xF0) {
			if (bdi.nFlags == 0xFE || bdi.nFlags == 0xFD) nGroup = i;
			i++;
			continue;
		}

		int nStep = bdi.nFlags & 0x0F;
		if (nStep < 1) nStep = 1;
		if (nGroup >= 0 && gui_DipCheckSetting(i) &&
			(nItemCount == 0 || pItems[nItemCount - 1].nGroup != nGroup)) {
			DIPMENUITEM *pNew = (DIPMENUITEM *)realloc(
				pItems, (nItemCount + 1) * sizeof(DIPMENUITEM));
			if (pNew == NULL) break;
			pItems = pNew;
			pItems[nItemCount++].nGroup = nGroup;
		}
		i += nStep;
	}

	*ppItems = pItems;
	return nItemCount;
}

static int gui_DipGetOption(int nGroup, int nOption, struct BurnDIPInfo *pOption)
{
	int nEntry = nGroup + 1;
	for (int i = 0; i <= nOption; i++) {
		do {
			if (BurnDrvGetDIPInfo(pOption, nEntry++)) return -1;
		} while (pOption->nFlags == 0);
	}

	return nEntry - 1;
}

static int gui_DipGetCurrentOption(int nGroup, struct BurnDIPInfo *pOption)
{
	struct BurnDIPInfo group;
	if (BurnDrvGetDIPInfo(&group, nGroup)) return -1;

	for (int i = 0; i < group.nSetting; i++) {
		int nEntry = gui_DipGetOption(nGroup, i, pOption);
		if (nEntry >= 0 && gui_DipCheckSetting(nEntry)) return i;
	}

	memset(pOption, 0, sizeof(*pOption));
	return -1;
}

static void gui_DipSetOption(int nGroup, int nOption)
{
	struct BurnDIPInfo bdi;
	int nEntry = gui_DipGetOption(nGroup, nOption, &bdi);
	if (nEntry < 0) return;

	int nOffset = gui_DipGetOffset();
	InpDIPConfigSetByInput(bdi.nInput + nOffset, bdi.nMask, bdi.nSetting);
	if (bdi.nFlags & 0x40) {
		while (BurnDrvGetDIPInfo(&bdi, ++nEntry) == 0 && bdi.nFlags == 0) {
			InpDIPConfigSetByInput(bdi.nInput + nOffset, bdi.nMask, bdi.nSetting);
		}
	}
}

static void gui_DipCycleOption(int nGroup, int nDirection)
{
	struct BurnDIPInfo group, option;
	if (BurnDrvGetDIPInfo(&group, nGroup) || group.nSetting == 0) return;

	int nCurrent = gui_DipGetCurrentOption(nGroup, &option);
	if (nCurrent < 0) {
		if (nDirection < 0) return;
		nCurrent = -1;
	}
	nCurrent += nDirection;
	if (nCurrent < 0 || nCurrent >= group.nSetting) return;
	gui_DipSetOption(nGroup, nCurrent);
}

static int gui_DipGetText(int nGroup, int nNumber, char *szGroup, int nGroupLen,
	char *szSetting, int nSettingLen)
{
	struct BurnDIPInfo group, option;
	BurnDrvGetDIPInfo(&group, nGroup);
	if (group.szText && group.szText[0]) {
		snprintf(szGroup, nGroupLen, "%s", group.szText);
	} else {
		snprintf(szGroup, nGroupLen, "DIP option %d", nNumber + 1);
	}

	int nCurrent = gui_DipGetCurrentOption(nGroup, &option);
	if (nCurrent >= 0 && option.szText) {
		snprintf(szSetting, nSettingLen, "%s", option.szText);
	} else {
		snprintf(szSetting, nSettingLen, "Unknown");
	}

	return nCurrent;
}

static bool gui_DipSettingChanged(struct BurnDIPInfo *pSetting, int nOffset)
{
	unsigned char nValue, nDefault;
	int nInput = pSetting->nInput + nOffset;
	if (InpDIPConfigGetByInput(nInput, &nValue) ||
		InpDIPConfigGetDefaultByInput(nInput, &nDefault)) return false;
	return (nValue & pSetting->nMask) != (nDefault & pSetting->nMask);
}

static bool gui_DipIsChanged(int nGroup)
{
	struct BurnDIPInfo setting;
	int nCurrent = gui_DipGetCurrentOption(nGroup, &setting);
	if (nCurrent < 0 && gui_DipGetOption(nGroup, 0, &setting) < 0) return false;

	int nOffset = gui_DipGetOffset();
	if (gui_DipSettingChanged(&setting, nOffset)) return true;
	if (setting.nFlags & 0x40) {
		int nEntry = gui_DipGetOption(nGroup, nCurrent < 0 ? 0 : nCurrent, &setting);
		while (BurnDrvGetDIPInfo(&setting, ++nEntry) == 0 && setting.nFlags == 0) {
			if (gui_DipSettingChanged(&setting, nOffset)) return true;
		}
	}

	return false;
}

static void gui_DipFormatRow(char *szLine, int nLineLen, const char *szGroup,
	const char *szSetting, int nCurrent, int nSettingCount, bool bSelected)
{
	const int nWidth = 37;
	bool bLeft = bSelected && nCurrent > 0;
	bool bRight = bSelected && nCurrent >= 0 && nCurrent + 1 < nSettingCount;
	const char *szLeft = bSelected ? (bLeft ? "< " : "  ") : "";
	const char *szRight = bRight ? " >" : "";
	int nGroupChars = strlen(szGroup);
	int nSettingChars = strlen(szSetting);
	int nAvailable = nWidth - 2 - strlen(szLeft) - strlen(szRight);
	if (nGroupChars + nSettingChars > nAvailable) {
		int nReservedSetting = nSettingChars < 12 ? nSettingChars : 12;
		nGroupChars = nAvailable - nReservedSetting;
		if (nGroupChars < 1) nGroupChars = 1;
		nSettingChars = nAvailable - nGroupChars;
	}

	snprintf(szLine, nLineLen, "%.*s: %s%.*s%s", nGroupChars, szGroup,
		szLeft, nSettingChars, szSetting, szRight);
}

static void gui_DipDrawClipped(const char *szText, int x, int y, int nChars, int nColor)
{
	char szLine[64];
	if (nChars >= (int)sizeof(szLine)) nChars = sizeof(szLine) - 1;
	strncpy(szLine, szText ? szText : "", nChars);
	szLine[nChars] = 0;
	DrawString(szLine, nColor, COLOR_BG, x, y);
}

static void gui_DipFormatBinary(unsigned char nValue, char *szBinary)
{
	for (int i = 0; i < 8; i++) {
		szBinary[i] = (nValue & (0x80 >> i)) ? '1' : '0';
	}
	szBinary[8] = 0;
}

static void gui_DipShowBankValues()
{
	int nBankCount = InpDIPConfigGetBankCount();
	for (int nBank = 0, nLine = 0; nBank < nBankCount && nLine < 4; nBank += 2, nLine++) {
		char szLine[48], szFirst[9], szSecond[9];
		unsigned char nValue = 0;
		InpDIPConfigGetByBank(nBank, &nValue);
		gui_DipFormatBinary(nValue, szFirst);
		if (nBank + 1 < nBankCount) {
			InpDIPConfigGetByBank(nBank + 1, &nValue);
			gui_DipFormatBinary(nValue, szSecond);
			snprintf(szLine, sizeof(szLine), "DIP %d: %s  DIP %d: %s",
				nBank + 1, szFirst, nBank + 2, szSecond);
		} else {
			snprintf(szLine, sizeof(szLine), "DIP %d: %s", nBank + 1, szFirst);
		}
		DrawString(szLine, COLOR_HELP_TEXT, COLOR_BG, 8, 184 + nLine * 8);
	}
}

static void gui_DipShow(DIPMENUITEM *pItems, int nItemCount, int nCurrent, int nFirst)
{
	const int nVisible = 18;
	SDL_FillRect(menuSurface, NULL, COLOR_BG);
	DrawString("DIP switches", COLOR_HELP_TEXT, COLOR_BG, 8, 2);

	if (nItemCount == 0) {
		DrawString("No DIP switches", COLOR_INACTIVE_ITEM, COLOR_BG, 96, 104);
		DrawString("B: back", COLOR_HELP_TEXT, COLOR_BG, 8, 224);
		return;
	}

	int nTotal = nItemCount + 1;
	for (int row = 0; row < nVisible && nFirst + row < nTotal; row++) {
		int nItem = nFirst + row;
		bool bSelected = nItem == nCurrent;
		int nColor = bSelected ? COLOR_ACTIVE_ITEM : COLOR_INACTIVE_ITEM;
		char szLine[96];
		if (nItem == nItemCount) {
			snprintf(szLine, sizeof(szLine), "Restore defaults");
		} else {
			char szGroup[64], szSetting[64];
			struct BurnDIPInfo group;
			BurnDrvGetDIPInfo(&group, pItems[nItem].nGroup);
			int nOption = gui_DipGetText(pItems[nItem].nGroup, nItem, szGroup, sizeof(szGroup),
				szSetting, sizeof(szSetting));
			gui_DipFormatRow(szLine, sizeof(szLine), szGroup, szSetting,
				nOption, group.nSetting, bSelected);
			if (gui_DipIsChanged(pItems[nItem].nGroup)) {
				nColor = bSelected ? COLOR_ACTIVE_CHANGED : COLOR_INACTIVE_CHANGED;
			}
		}
		if (bSelected) DrawString(">", nColor, COLOR_BG, 0, 20 + row * 8);
		gui_DipDrawClipped(szLine, 8, 20 + row * 8, 37, nColor);
	}

	if (nFirst > 0) DrawString("^", COLOR_HELP_TEXT, COLOR_BG, 312, 20);
	if (nFirst + nVisible < nTotal) DrawString("v", COLOR_HELP_TEXT, COLOR_BG, 312, 156);

	gui_DipShowBankValues();
	if (nCurrent == nItemCount) {
		DrawString("A: restore defaults  B: save/back", COLOR_HELP_TEXT, COLOR_BG, 8, 224);
	} else {
		DrawString("Left/Right: change  B: save/back", COLOR_HELP_TEXT, COLOR_BG, 8, 224);
	}
}

static void gui_DipKeepVisible(int nCurrent, int nTotal, int *pnFirst)
{
	const int nVisible = 18;
	if (nCurrent < *pnFirst) *pnFirst = nCurrent;
	if (nCurrent >= *pnFirst + nVisible) *pnFirst = nCurrent - nVisible + 1;
	int nMax = nTotal > nVisible ? nTotal - nVisible : 0;
	if (*pnFirst > nMax) *pnFirst = nMax;
	if (*pnFirst < 0) *pnFirst = 0;
}

static void gui_DipMenuRun()
{
	DIPMENUITEM *pItems = NULL;
	int nItemCount = gui_DipBuildItems(&pItems);
	int nCurrent = 0;
	int nFirst = 0;
	SDL_Event gui_event;

	while (1) {
		while (SDL_PollEvent(&gui_event)) {
			if (gui_event.type != SDL_KEYDOWN) continue;
			if (gui_event.key.keysym.sym == SDLK_LCTRL) {
				InpDIPApplyConfig();
				ConfigGameSave();
				if (pItems) free(pItems);
				return;
			}

			if (nItemCount == 0) continue;
			int nTotal = nItemCount + 1;
			if (gui_event.key.keysym.sym == SDLK_UP) {
				if (--nCurrent < 0) nCurrent = nTotal - 1;
			} else if (gui_event.key.keysym.sym == SDLK_DOWN) {
				if (++nCurrent >= nTotal) nCurrent = 0;
			} else if ((gui_event.key.keysym.sym == SDLK_LEFT ||
				gui_event.key.keysym.sym == SDLK_RIGHT) && nCurrent < nItemCount) {
				int nGroup = pItems[nCurrent].nGroup;
				gui_DipCycleOption(nGroup, gui_event.key.keysym.sym == SDLK_LEFT ? -1 : 1);
				free(pItems);
				pItems = NULL;
				nItemCount = gui_DipBuildItems(&pItems);
				nCurrent = 0;
				for (int i = 0; i < nItemCount; i++) {
					if (pItems[i].nGroup == nGroup) {
						nCurrent = i;
						break;
					}
				}
			} else if (gui_event.key.keysym.sym == SDLK_LALT && nCurrent == nItemCount) {
				InpDIPConfigClear();
				free(pItems);
				pItems = NULL;
				nItemCount = gui_DipBuildItems(&pItems);
				nCurrent = nItemCount;
			}
			gui_DipKeepVisible(nCurrent, nItemCount + 1, &nFirst);
		}

		gui_DipShow(pItems, nItemCount, nCurrent, nFirst);
		SDL_Delay(16);
		gui_Flip();
	}
}

static void gui_reset()
{
	DrvInitCallback();
	done = 1;
}

/* exported functions */ 

void gui_Init()
{
	menuSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, 16, 0, 0, 0, 0);
}

void gui_Run()
{
	struct timeval s, e;
	extern struct timeval start;

	gettimeofday(&s, NULL);

	VideoClear();
	SDL_EnableKeyRepeat(/*SDL_DEFAULT_REPEAT_DELAY*/ 150, /*SDL_DEFAULT_REPEAT_INTERVAL*/30);
	gui_MainMenu.itemCur = 0;
	gui_MenuRun(&gui_MainMenu);
	SDL_EnableKeyRepeat(0, 0);
	ConfigGameSave();
	VideoClear();

	gettimeofday(&e, NULL);
	start.tv_sec += e.tv_sec - s.tv_sec;
	start.tv_usec += e.tv_usec - s.tv_usec;
}

void gui_Exit()
{
	if(menuSurface) SDL_FreeSurface(menuSurface);
}

//
// Font: THIN8X8.pf
// Exported from PixelFontEdit 2.7.0

unsigned char gui_font[2048] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 000 (.)
	0x7E, 0x81, 0xA5, 0x81, 0xBD, 0x99, 0x81, 0x7E,	// Char 001 (.)
	0x7E, 0xFF, 0xDB, 0xFF, 0xC3, 0xE7, 0xFF, 0x7E,	// Char 002 (.)
	0x6C, 0xFE, 0xFE, 0xFE, 0x7C, 0x38, 0x10, 0x00,	// Char 003 (.)
	0x10, 0x38, 0x7C, 0xFE, 0x7C, 0x38, 0x10, 0x00,	// Char 004 (.)
	0x38, 0x7C, 0x38, 0xFE, 0xFE, 0x7C, 0x38, 0x7C,	// Char 005 (.)
	0x10, 0x10, 0x38, 0x7C, 0xFE, 0x7C, 0x38, 0x7C,	// Char 006 (.)
	0x00, 0x00, 0x18, 0x3C, 0x3C, 0x18, 0x00, 0x00,	// Char 007 (.)
	0xFF, 0xFF, 0xE7, 0xC3, 0xC3, 0xE7, 0xFF, 0xFF,	// Char 008 (.)
	0x00, 0x3C, 0x66, 0x42, 0x42, 0x66, 0x3C, 0x00,	// Char 009 (.)
	0xFF, 0xC3, 0x99, 0xBD, 0xBD, 0x99, 0xC3, 0xFF,	// Char 010 (.)
	0x0F, 0x07, 0x0F, 0x7D, 0xCC, 0xCC, 0xCC, 0x78,	// Char 011 (.)
	0x3C, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x7E, 0x18,	// Char 012 (.)
	0x3F, 0x33, 0x3F, 0x30, 0x30, 0x70, 0xF0, 0xE0,	// Char 013 (.)
	0x7F, 0x63, 0x7F, 0x63, 0x63, 0x67, 0xE6, 0xC0,	// Char 014 (.)
	0x99, 0x5A, 0x3C, 0xE7, 0xE7, 0x3C, 0x5A, 0x99,	// Char 015 (.)
	0x80, 0xE0, 0xF8, 0xFE, 0xF8, 0xE0, 0x80, 0x00,	// Char 016 (.)
	0x02, 0x0E, 0x3E, 0xFE, 0x3E, 0x0E, 0x02, 0x00,	// Char 017 (.)
	0x18, 0x3C, 0x7E, 0x18, 0x18, 0x7E, 0x3C, 0x18,	// Char 018 (.)
	0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00,	// Char 019 (.)
	0x7F, 0xDB, 0xDB, 0x7B, 0x1B, 0x1B, 0x1B, 0x00,	// Char 020 (.)
	0x3E, 0x63, 0x38, 0x6C, 0x6C, 0x38, 0xCC, 0x78,	// Char 021 (.)
	0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x7E, 0x00,	// Char 022 (.)
	0x18, 0x3C, 0x7E, 0x18, 0x7E, 0x3C, 0x18, 0xFF,	// Char 023 (.)
	0x18, 0x3C, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x00,	// Char 024 (.)
	0x18, 0x18, 0x18, 0x18, 0x7E, 0x3C, 0x18, 0x00,	// Char 025 (.)
	0x00, 0x18, 0x0C, 0xFE, 0x0C, 0x18, 0x00, 0x00,	// Char 026 (.) right arrow
	0x00, 0x30, 0x60, 0xFE, 0x60, 0x30, 0x00, 0x00,	// Char 027 (.)
	0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xFE, 0x00, 0x00,	// Char 028 (.)
	0x00, 0x24, 0x66, 0xFF, 0x66, 0x24, 0x00, 0x00,	// Char 029 (.)
	0x00, 0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x00, 0x00,	// Char 030 (.)
	0x00, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00, 0x00,	// Char 031 (.)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 032 ( )
	0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x40, 0x00,	// Char 033 (!)
	0x90, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 034 (")
	0x50, 0x50, 0xF8, 0x50, 0xF8, 0x50, 0x50, 0x00,	// Char 035 (#)
	0x20, 0x78, 0xA0, 0x70, 0x28, 0xF0, 0x20, 0x00,	// Char 036 ($)
	0xC8, 0xC8, 0x10, 0x20, 0x40, 0x98, 0x98, 0x00,	// Char 037 (%)
	0x70, 0x88, 0x50, 0x20, 0x54, 0x88, 0x74, 0x00,	// Char 038 (&)
	0x60, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 039 (')
	0x20, 0x40, 0x80, 0x80, 0x80, 0x40, 0x20, 0x00,	// Char 040 (()
	0x20, 0x10, 0x08, 0x08, 0x08, 0x10, 0x20, 0x00,	// Char 041 ())
	0x00, 0x20, 0xA8, 0x70, 0x70, 0xA8, 0x20, 0x00,	// Char 042 (*)
	0x00, 0x00, 0x20, 0x20, 0xF8, 0x20, 0x20, 0x00,	// Char 043 (+)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x20, 0x40,	// Char 044 (,)
	0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00,	// Char 045 (-)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00,	// Char 046 (.)
	0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00,	// Char 047 (/)
	0x70, 0x88, 0x98, 0xA8, 0xC8, 0x88, 0x70, 0x00,	// Char 048 (0)
	0x40, 0xC0, 0x40, 0x40, 0x40, 0x40, 0xE0, 0x00,	// Char 049 (1)
	0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xF8, 0x00,	// Char 050 (2)
	0x70, 0x88, 0x08, 0x10, 0x08, 0x88, 0x70, 0x00,	// Char 051 (3)
	0x08, 0x18, 0x28, 0x48, 0xFC, 0x08, 0x08, 0x00,	// Char 052 (4)
	0xF8, 0x80, 0x80, 0xF0, 0x08, 0x88, 0x70, 0x00,	// Char 053 (5)
	0x20, 0x40, 0x80, 0xF0, 0x88, 0x88, 0x70, 0x00,	// Char 054 (6)
	0xF8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40, 0x00,	// Char 055 (7)
	0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70, 0x00,	// Char 056 (8)
	0x70, 0x88, 0x88, 0x78, 0x08, 0x08, 0x70, 0x00,	// Char 057 (9)
	0x00, 0x00, 0x60, 0x60, 0x00, 0x60, 0x60, 0x00,	// Char 058 (:)
	0x00, 0x00, 0x60, 0x60, 0x00, 0x60, 0x60, 0x20,	// Char 059 (;)
	0x10, 0x20, 0x40, 0x80, 0x40, 0x20, 0x10, 0x00,	// Char 060 (<)
	0x00, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00, 0x00,	// Char 061 (=)
	0x80, 0x40, 0x20, 0x10, 0x20, 0x40, 0x80, 0x00,	// Char 062 (>)
	0x78, 0x84, 0x04, 0x08, 0x10, 0x00, 0x10, 0x00,	// Char 063 (?)
	0x70, 0x88, 0x88, 0xA8, 0xB8, 0x80, 0x78, 0x00,	// Char 064 (@)
	0x20, 0x50, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x00,	// Char 065 (A)
	0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0, 0x00,	// Char 066 (B)
	0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70, 0x00,	// Char 067 (C)
	0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0, 0x00,	// Char 068 (D)
	0xF8, 0x80, 0x80, 0xE0, 0x80, 0x80, 0xF8, 0x00,	// Char 069 (E)
	0xF8, 0x80, 0x80, 0xE0, 0x80, 0x80, 0x80, 0x00,	// Char 070 (F)
	0x70, 0x88, 0x80, 0x80, 0x98, 0x88, 0x78, 0x00,	// Char 071 (G)
	0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0x00,	// Char 072 (H)
	0xE0, 0x40, 0x40, 0x40, 0x40, 0x40, 0xE0, 0x00,	// Char 073 (I)
	0x38, 0x10, 0x10, 0x10, 0x10, 0x90, 0x60, 0x00,	// Char 074 (J)
	0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88, 0x00,	// Char 075 (K)
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8, 0x00,	// Char 076 (L)
	0x82, 0xC6, 0xAA, 0x92, 0x82, 0x82, 0x82, 0x00,	// Char 077 (M)
	0x84, 0xC4, 0xA4, 0x94, 0x8C, 0x84, 0x84, 0x00,	// Char 078 (N)
	0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00,	// Char 079 (O)
	0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80, 0x00,	// Char 080 (P)
	0x70, 0x88, 0x88, 0x88, 0xA8, 0x90, 0x68, 0x00,	// Char 081 (Q)
	0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88, 0x00,	// Char 082 (R)
	0x70, 0x88, 0x80, 0x70, 0x08, 0x88, 0x70, 0x00,	// Char 083 (S)
	0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00,	// Char 084 (T)
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00,	// Char 085 (U)
	0x88, 0x88, 0x88, 0x50, 0x50, 0x20, 0x20, 0x00,	// Char 086 (V)
	0x82, 0x82, 0x82, 0x82, 0x92, 0x92, 0x6C, 0x00,	// Char 087 (W)
	0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88, 0x00,	// Char 088 (X)
	0x88, 0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x00,	// Char 089 (Y)
	0xF8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xF8, 0x00,	// Char 090 (Z)
	0xE0, 0x80, 0x80, 0x80, 0x80, 0x80, 0xE0, 0x00,	// Char 091 ([)
	0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00,	// Char 092 (\)
	0xE0, 0x20, 0x20, 0x20, 0x20, 0x20, 0xE0, 0x00,	// Char 093 (])
	0x20, 0x50, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 094 (^)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00,	// Char 095 (_)
	0x40, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 096 (`)
	0x00, 0x00, 0x70, 0x08, 0x78, 0x88, 0x74, 0x00,	// Char 097 (a)
	0x80, 0x80, 0xB0, 0xC8, 0x88, 0xC8, 0xB0, 0x00,	// Char 098 (b)
	0x00, 0x00, 0x70, 0x88, 0x80, 0x88, 0x70, 0x00,	// Char 099 (c)
	0x08, 0x08, 0x68, 0x98, 0x88, 0x98, 0x68, 0x00,	// Char 100 (d)
	0x00, 0x00, 0x70, 0x88, 0xF8, 0x80, 0x70, 0x00,	// Char 101 (e)
	0x30, 0x48, 0x40, 0xE0, 0x40, 0x40, 0x40, 0x00,	// Char 102 (f)
	0x00, 0x00, 0x34, 0x48, 0x48, 0x38, 0x08, 0x30,	// Char 103 (g)
	0x80, 0x80, 0xB0, 0xC8, 0x88, 0x88, 0x88, 0x00,	// Char 104 (h)
	0x20, 0x00, 0x60, 0x20, 0x20, 0x20, 0x70, 0x00,	// Char 105 (i)
	0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x90, 0x60,	// Char 106 (j)
	0x80, 0x80, 0x88, 0x90, 0xA0, 0xD0, 0x88, 0x00,	// Char 107 (k)
	0xC0, 0x40, 0x40, 0x40, 0x40, 0x40, 0xE0, 0x00,	// Char 108 (l)
	0x00, 0x00, 0xEC, 0x92, 0x92, 0x92, 0x92, 0x00,	// Char 109 (m)
	0x00, 0x00, 0xB0, 0xC8, 0x88, 0x88, 0x88, 0x00,	// Char 110 (n)
	0x00, 0x00, 0x70, 0x88, 0x88, 0x88, 0x70, 0x00,	// Char 111 (o)
	0x00, 0x00, 0xB0, 0xC8, 0xC8, 0xB0, 0x80, 0x80,	// Char 112 (p)
	0x00, 0x00, 0x68, 0x98, 0x98, 0x68, 0x08, 0x08,	// Char 113 (q)
	0x00, 0x00, 0xB0, 0xC8, 0x80, 0x80, 0x80, 0x00,	// Char 114 (r)
	0x00, 0x00, 0x78, 0x80, 0x70, 0x08, 0xF0, 0x00,	// Char 115 (s)
	0x40, 0x40, 0xE0, 0x40, 0x40, 0x50, 0x20, 0x00,	// Char 116 (t)
	0x00, 0x00, 0x88, 0x88, 0x88, 0x98, 0x68, 0x00,	// Char 117 (u)
	0x00, 0x00, 0x88, 0x88, 0x88, 0x50, 0x20, 0x00,	// Char 118 (v)
	0x00, 0x00, 0x82, 0x82, 0x92, 0x92, 0x6C, 0x00,	// Char 119 (w)
	0x00, 0x00, 0x88, 0x50, 0x20, 0x50, 0x88, 0x00,	// Char 120 (x)
	0x00, 0x00, 0x88, 0x88, 0x98, 0x68, 0x08, 0x70,	// Char 121 (y)
	0x00, 0x00, 0xF8, 0x10, 0x20, 0x40, 0xF8, 0x00,	// Char 122 (z)
	0x10, 0x20, 0x20, 0x40, 0x20, 0x20, 0x10, 0x00,	// Char 123 ({)
	0x40, 0x40, 0x40, 0x00, 0x40, 0x40, 0x40, 0x00,	// Char 124 (|)
	0x40, 0x20, 0x20, 0x10, 0x20, 0x20, 0x40, 0x00,	// Char 125 (})
	0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Char 126 (~)
	0x00, 0x10, 0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0x00	// Char 127 (.)
};
