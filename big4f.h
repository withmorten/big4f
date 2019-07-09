#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#include <fileapi.h>
#include <handleapi.h>
#else
#include <limits.h>
#include <sys/stat.h>
#include <fts.h>
#endif

#define PATH_MAX_WIN 260

#define BIG4F_VERSION_MAJOR 0
#define BIG4F_VERSION_MINOR 3

#define BIGF_MAGIC 0x42494746
#define BIG4_MAGIC 0x42494734

typedef struct BIGHeader BIGHeader;
typedef struct BIGDirectoryEntry BIGDirectoryEntry;

// From http://wiki.xentax.com/index.php/EA_BIG_BIGF_Archive
// and http://wiki.xentax.com/index.php/EA_VIV_BIG4
// C&C accepts BIGF files regardless of whether they have padding
// BFME accepts BIGF and BIG4 files

struct BIGHeader
{
	uint32_t MagicHeader;
	uint32_t ArchiveSize;
	uint32_t NumFiles;
	uint32_t HeaderEnd;
	BIGDirectoryEntry *DirectoryEntry[];
};

struct BIGDirectoryEntry
{
	uint32_t FileOffset;
	uint32_t FileLength;
	char *FilePath;
};

// util.c
void printf_help_exit(void);
void printf_error_exit(char *message, char *filename);
FILE *fopen_d(char *path, const char *mode);
FILE *fopen_r(char *fullpath);
int fsize(FILE *stream);
void *malloc_d(size_t size);
void *calloc_d(size_t nitems, size_t size);
void *realloc_d(void *ptr, size_t size);
char *strndup(const char *str, size_t size);
char *strncpy_d(char *dest, const char *src, size_t n);
char *fgets_c(char *str, int n, FILE *stream);
char *unixify_path(char *path);
char *windowsify_path(char *path);
char *systemify_path(char *path);
int mkdir_w(const char *path);
int mkdir_p(const char *path);
void mkdir_d(char *path);

// bigfile.c
void BIGFile_Pack(char *InputDir, char *BIGFile_Path, char BIGFormat);
BIGHeader *BIGFileHeader_Create(BIGHeader *BIGFile_Header, char *InputDir, char *SearchDir, int AllocSize);
BIGHeader *BIGFileHeader_AddDirectoryEntry(BIGHeader *BIGFile_Header, char *InputDir, char *FullFilePath);

void BIGFile_List(char *BIGFile_Path);
void BIGFile_Extract(char *BIGFile_Path, char *ExtractPath);
BIGHeader *BIGFileHeader_Parse(FILE *BIGFile_Handle);
