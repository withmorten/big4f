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

#define LODWORD(l) ((DWORD)((DWORDLONG)(l)))
#define HIDWORD(l) ((DWORD)(((DWORDLONG)(l)>>32)&0xFFFFFFFF))
#else
#include <limits.h>
#include <sys/stat.h>
#include <fts.h>
#endif

#define PATH_MAX_WIN 260

#define BIG4F_VERSION_MAJOR 0
#define BIG4F_VERSION_MINOR 4

#define BIGF_MAGIC 0x42494746 // BIGF
#define BIG4_MAGIC 0x42494734 // BIG4

#define BIG4F_MAGIC 0x4249473446302E34 // BIG4F0.4

typedef struct BIGHeader BIGHeader;
typedef struct BIGDirectoryEntry BIGDirectoryEntry;
typedef struct BIG4FFileEntry BIG4FFileEntry;
typedef struct BIG4FHeader BIG4FHeader;

// From http://wiki.xentax.com/index.php/EA_BIG_BIGF_Archive
// and http://wiki.xentax.com/index.php/EA_VIV_BIG4
// C&C accepts BIGF files regardless of whether they have padding
// BFME accepts BIGF and BIG4 files

struct BIGHeader
{
	uint32_t MagicHeader;
	uint32_t ArchiveSize;
	uint32_t NumFiles;
	uint32_t HeaderSize; // FirstFileOffset
	BIGDirectoryEntry *DirectoryEntry[];
};

struct BIGDirectoryEntry
{
	uint32_t FileOffset;
	uint32_t FileLength;
	char *FilePath;
};

// extensions for 0.4

struct BIG4FHeader
{
	uint64_t BIG4FId;
};

struct BIG4FFileEntry
{
	int64_t DateCreated;
	int64_t DateAccessed;
	int64_t DateModified;
};

// util.c
void printf_help_exit(void);
void printf_error_exit(char *message, char *filename);
FILE *fopen_d(char *path, const char *mode);
FILE *fopen_r(char *fullpath);
uint32_t fsize(FILE *stream);
#ifdef _WIN32
int64_t ft2int64(FILETIME *ft);
void int642ft(FILETIME *ft, int64_t i64);
#endif
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
BIGHeader *BIGFileHeader_Parse(char *BIGFile_Path, FILE *BIGFile_Handle);
