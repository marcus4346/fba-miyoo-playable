#ifndef _FBA_CACHE_H_
#define _FBA_CACHE_H_

extern int bBurnUseRomCache;

int BurnCacheInit(const char* path, char* romName);
void BurnCacheExit();
unsigned int BurnCacheBlockSize(int blockId);
int BurnCacheRead(unsigned char* dst, int blockId);
void* BurnCacheMap(int blockId);
int BurnCacheIsFile(const char* path);

#endif
