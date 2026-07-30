#include "burner.h"
#include "cache.h"

#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>

int bBurnUseRomCache = 0;

static const int kCacheBlockCount = 15;

static int nBurnCacheFile = -1;
static void* pBurnCacheBase = NULL;
static unsigned int nBurnCacheSize = 0;

static struct BurnCacheHeader {
	unsigned int ver;
	char name[12];
	struct BurnCacheBlock {
		unsigned int offset;
		char desc[12];
	} blocks[kCacheBlockCount];
} BurnCacheHeader;

void show_rom_loading_text(char* szText, int nSize, int nTotalSize);

static const char* GetExtension(const char* path)
{
	const char* file = strrchr(path, '/');
	const char* backslash = strrchr(path, '\\');
	if (backslash && (!file || backslash > file)) {
		file = backslash;
	}
	file = file ? file + 1 : path;

	return strrchr(file, '.');
}

static void SplitPath(const char* path, char* dir, char* file)
{
	const char* slash = strrchr(path, '/');
	const char* backslash = strrchr(path, '\\');
	const char* sep = slash;
	if (backslash && (!sep || backslash > sep)) {
		sep = backslash;
	}

	if (sep) {
		int len = sep - path + 1;
		if (len >= MAX_PATH) {
			len = MAX_PATH - 1;
		}
		strncpy(dir, path, len);
		dir[len] = 0;
		strncpy(file, sep + 1, MAX_PATH - 1);
		file[MAX_PATH - 1] = 0;
	} else {
		strcpy(dir, "./");
		strncpy(file, path, MAX_PATH - 1);
		file[MAX_PATH - 1] = 0;
	}
}

static int ValidateCacheHeader(unsigned int fileSize)
{
	unsigned int lastOffset = 0;
	int validOffsets = 0;

	for (int i = 0; i < kCacheBlockCount; i++) {
		unsigned int offset = BurnCacheHeader.blocks[i].offset;
		if (offset == 0) {
			break;
		}
		if (offset < sizeof(BurnCacheHeader) || offset < lastOffset || offset > fileSize) {
			return 1;
		}
		lastOffset = offset;
		validOffsets++;
	}

	if (validOffsets < 2 || lastOffset == 0) {
		return 1;
	}

	nBurnCacheSize = lastOffset;
	return 0;
}

int BurnCacheIsFile(const char* path)
{
	const char* ext = GetExtension(path);
	return ext && strcasecmp(ext, ".fba") == 0;
}

int BurnCacheInit(const char* path, char* romName)
{
	char fileName[MAX_PATH];
	struct stat st;

	BurnCacheExit();

	if (!path || !romName) {
		return -1;
	}

	SplitPath(path, szAppRomPaths[0], fileName);

	char* ext = strrchr(fileName, '.');
	if (!ext) {
		return -1;
	}

	if (strcasecmp(ext, ".zip") == 0) {
		*ext = 0;
		strcpy(romName, fileName);
		return 0;
	}

	if (strcasecmp(ext, ".fba") != 0) {
		return -1;
	}

	nBurnCacheFile = open(path, O_RDONLY);
	if (nBurnCacheFile < 0) {
		return -2;
	}

	if (fstat(nBurnCacheFile, &st) != 0 || st.st_size < (off_t)sizeof(BurnCacheHeader)) {
		BurnCacheExit();
		return -3;
	}

	if (read(nBurnCacheFile, &BurnCacheHeader, sizeof(BurnCacheHeader)) != (ssize_t)sizeof(BurnCacheHeader)) {
		BurnCacheExit();
		return -4;
	}

	if (ValidateCacheHeader((unsigned int)st.st_size)) {
		BurnCacheExit();
		return -5;
	}

	pBurnCacheBase = mmap(0, nBurnCacheSize, PROT_READ, MAP_PRIVATE, nBurnCacheFile, 0);
	if (pBurnCacheBase == MAP_FAILED) {
		pBurnCacheBase = NULL;
		BurnCacheExit();
		return -6;
	}

	strncpy(romName, BurnCacheHeader.name, 12);
	romName[12] = 0;
	bBurnUseRomCache = 1;

	return 0;
}

unsigned int BurnCacheBlockSize(int blockId)
{
	if (blockId < 0 || blockId + 1 >= kCacheBlockCount) {
		return 0;
	}
	if (!BurnCacheHeader.blocks[blockId].offset || !BurnCacheHeader.blocks[blockId + 1].offset) {
		return 0;
	}
	if (BurnCacheHeader.blocks[blockId + 1].offset < BurnCacheHeader.blocks[blockId].offset) {
		return 0;
	}

	return BurnCacheHeader.blocks[blockId + 1].offset - BurnCacheHeader.blocks[blockId].offset;
}

int BurnCacheRead(unsigned char* dst, int blockId)
{
	unsigned int size = BurnCacheBlockSize(blockId);

	if (nBurnCacheFile < 0 || !dst || size == 0) {
		return 1;
	}

	show_rom_loading_text(BurnCacheHeader.blocks[blockId].desc, size, nBurnCacheSize);

	if (lseek(nBurnCacheFile, BurnCacheHeader.blocks[blockId].offset, SEEK_SET) < 0) {
		return 1;
	}
	if (read(nBurnCacheFile, dst, size) != (ssize_t)size) {
		return 1;
	}

	return 0;
}

void* BurnCacheMap(int blockId)
{
	unsigned int size = BurnCacheBlockSize(blockId);

	if (!pBurnCacheBase || size == 0) {
		return NULL;
	}

	show_rom_loading_text(BurnCacheHeader.blocks[blockId].desc, size, nBurnCacheSize);
	return (unsigned char*)pBurnCacheBase + BurnCacheHeader.blocks[blockId].offset;
}

void BurnCacheExit()
{
	if (pBurnCacheBase) {
		munmap(pBurnCacheBase, nBurnCacheSize);
		pBurnCacheBase = NULL;
	}

	if (nBurnCacheFile >= 0) {
		close(nBurnCacheFile);
		nBurnCacheFile = -1;
	}

	nBurnCacheSize = 0;
	bBurnUseRomCache = 0;
	memset(&BurnCacheHeader, 0, sizeof(BurnCacheHeader));
}
