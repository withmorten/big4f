#include "big4f.h"

void
BIGFile_Pack(char *InputDir, char *BIGFile_Path, char BIGFormat)
{
    FILE *BIGFile_Handle, *InputFile_Handle;
    BIGHeader *BIGFile_Header = NULL;
    BIGDirectoryEntry *DirectoryEntry = NULL;
    char InputFile_Path[PATH_MAX_WIN];
    void *InputFile_Buffer = NULL;
    int i;
    int BIGFile_Header_LastPos;

    // Initial malloc for header
    BIGFile_Header = malloc_d(sizeof(BIGHeader));

    // Get format and set MagicHeader, byteswap because these values need to be BE
    if(BIGFormat == 'f')
    {
        BIGFile_Header->MagicHeader = __builtin_bswap32(BIGF_MAGIC);
    }
    else
    {
        BIGFile_Header->MagicHeader = __builtin_bswap32(BIG4_MAGIC);
    }

    // Temporary values, will be filled in BIGFileHeader_Create() and finalised after
    BIGFile_Header->HeaderEnd = sizeof(BIGHeader);
    BIGFile_Header->ArchiveSize = 0;
    BIGFile_Header->NumFiles = 0;

    // Get DirectoryEntries + some initial Header data
    BIGFile_Header = BIGFileHeader_Create(BIGFile_Header, InputDir, InputDir);

    if(BIGFile_Header->NumFiles == 0)
    {
        printf_error_exit("No files in directory ", InputDir);
    }

    // Final ArchiveSize
    // Apparently gets ignored by (most) games that use this format, so just keep it LE
    BIGFile_Header->ArchiveSize += BIGFile_Header->HeaderEnd;

    // Open BigFile and set to ArchiveSize
    BIGFile_Handle = fopen_d(BIGFile_Path, "wb");
    ftruncate(fileno(BIGFile_Handle), BIGFile_Header->ArchiveSize);

    // Set initial LastPos for first DirEntry offset
    BIGFile_Header_LastPos = BIGFile_Header->HeaderEnd;

    // Loop through all DirEntries, read and write files into BIGFile, write Header
    for(i = 0; i < BIGFile_Header->NumFiles; i++)
    {
        DirectoryEntry = BIGFile_Header->DirectoryEntry[i];

        // Restore full InputFile Path after being trimmed in SetHeader()
        snprintf(InputFile_Path, PATH_MAX_WIN, "%s/%s", InputDir, DirectoryEntry->FilePath);

        // Finalise relative offset
        DirectoryEntry->FileOffset += BIGFile_Header->HeaderEnd;

        InputFile_Handle = fopen_d(unixify_path(InputFile_Path), "rb");

        // Read full Input File
        InputFile_Buffer = realloc_d(InputFile_Buffer, DirectoryEntry->FileLength);
        fread(InputFile_Buffer, DirectoryEntry->FileLength, 1, InputFile_Handle);

        // Seek to FileOffset in BIGFile and write data
        fseek(BIGFile_Handle, DirectoryEntry->FileOffset, SEEK_SET);
        fwrite(InputFile_Buffer, DirectoryEntry->FileLength, 1, BIGFile_Handle);

        fclose(InputFile_Handle);

        // byteswap because these values need to be BE
        DirectoryEntry->FileOffset = __builtin_bswap32(DirectoryEntry->FileOffset);
        DirectoryEntry->FileLength = __builtin_bswap32(DirectoryEntry->FileLength);

        // write this DirEntry to BIGFile, remember position for next DirEntry
        fseek(BIGFile_Handle, BIGFile_Header_LastPos, SEEK_SET);
        fwrite(DirectoryEntry, sizeof(BIGDirectoryEntry) - sizeof(DirectoryEntry->FilePath), 1, BIGFile_Handle);
        fwrite(DirectoryEntry->FilePath, strlen(DirectoryEntry->FilePath) + 1, 1, BIGFile_Handle);
        BIGFile_Header_LastPos = ftell(BIGFile_Handle);

        printf("%s\n", DirectoryEntry->FilePath);
    }

    // byteswap because these values need to be BE
    BIGFile_Header->NumFiles = __builtin_bswap32(BIGFile_Header->NumFiles);
    BIGFile_Header->HeaderEnd = __builtin_bswap32(BIGFile_Header->HeaderEnd);

    // Write header at the end so we can work with unswapped values before
    fseek(BIGFile_Handle, 0, SEEK_SET);
    fwrite(BIGFile_Header, sizeof(BIGHeader), 1, BIGFile_Handle);

    fclose(BIGFile_Handle);
}

BIGHeader *
BIGFileHeader_Create(BIGHeader *BIGFile_Header, char *InputDir, char *SearchDir)
{
#ifdef _WIN32

    // Adapted from https://stackoverflow.com/a/2315808
    WIN32_FIND_DATA fd;
    HANDLE h = NULL;
    char SearchPath[PATH_MAX_WIN];

    // Just to be sure we don't have any backslashes and trailing slashes
    InputDir = unixify_path(InputDir);
    SearchDir = unixify_path(SearchDir);

    // Specify a file mask. *.* = We want everything!
    snprintf(SearchPath, PATH_MAX_WIN, "%s/*.*", SearchDir);

    h = FindFirstFile(SearchPath, &fd);

    if(h == INVALID_HANDLE_VALUE)
    {
        printf_error_exit("Path not found: ", SearchDir);
    }

    do
    {
        // Find first file will always return "." and ".." as the first two directories.
        if(strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, ".."))
        {
            // Build up our file path using the passed in SearchDir and the file/foldername we just found:
            snprintf(SearchPath, PATH_MAX_WIN, "%s/%s", SearchDir, fd.cFileName);

            // Is the entity a File or Folder?
            if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                // Recursion
                BIGFile_Header = BIGFileHeader_Create(BIGFile_Header, InputDir, SearchPath);
            }
            else
            {
                // Found a file, let's add it to the Directory entries
                BIGFile_Header = BIGFileHeader_AddDirectoryEntry(BIGFile_Header, InputDir, SearchPath);
            }
        }
    }
    while(FindNextFile(h, &fd));

    FindClose(h);
#else

    FTS *fts;
    FTSENT *ent;
    char *fts_argv[] = { InputDir, NULL };

    fts = fts_open((char * const *)fts_argv, FTS_PHYSICAL | FTS_NOCHDIR, NULL);

    if(errno)
    {
        printf_error_exit("Input directory could not be opened: ", InputDir);
    }

    while((ent = fts_read(fts)) != NULL)
    {
        // No errno checking because fts_read adds random backslashes into pathnames, resulting in "No such dir"
        // fopen() will fail later on if the file cannot actually be opened and the program will exit

        if(ent->fts_info & FTS_F)
        {
            // Found a file, let's add it to the Directory entries
            BIGFile_Header = BIGFileHeader_AddDirectoryEntry(BIGFile_Header, unixify_path(InputDir), unixify_path(ent->fts_path));
        }
    }

    fts_close(fts);
#endif

    return BIGFile_Header;
}

BIGHeader *
BIGFileHeader_AddDirectoryEntry(BIGHeader *BIGFile_Header, char *InputDir, char *FullFilePath)
{
    BIGDirectoryEntry *DirectoryEntry = NULL;
    int FilePath_Start, FilePath_Length;
    FILE *f;

    // Realloc more memory for current Header + one more DirEntry and memory for new DirEntry struct
    BIGFile_Header = realloc_d(BIGFile_Header, sizeof(BIGHeader) + ((sizeof(BIGDirectoryEntry)) * (BIGFile_Header->NumFiles + 1)));
    DirectoryEntry = malloc_d(sizeof(BIGDirectoryEntry));

    // Open found file to get filesize
    f = fopen_d(FullFilePath, "rb");
    DirectoryEntry->FileLength = fsize(f);
    fclose(f);

    // Temporary relative offset, will have to be adjusted after all files are known
    if(BIGFile_Header->NumFiles == 0)
    {
        DirectoryEntry->FileOffset = 0;
    }
    else
    {
        // Last FileLength + last FileOffset
        DirectoryEntry->FileOffset = BIGFile_Header->DirectoryEntry[BIGFile_Header->NumFiles - 1]->FileLength
                                        + BIGFile_Header->DirectoryEntry[BIGFile_Header->NumFiles - 1]->FileOffset;
    }

    // Trim the InputDir from the FilePath
    FilePath_Start = strlen(InputDir) + 1;
    FilePath_Length = (strlen(FullFilePath) - FilePath_Start) + 1;
    DirectoryEntry->FilePath = malloc_d(FilePath_Length);
    strncpy_d(DirectoryEntry->FilePath, windowsify_path(FullFilePath + FilePath_Start), FilePath_Length);

    // Build up HeaderEnd offset: Each DirEntry is 4 + 4 + strlen(FilePath) + 1, or rather FilePath_Length
    // Is final after last run
    BIGFile_Header->HeaderEnd += (sizeof(BIGDirectoryEntry) - sizeof(DirectoryEntry->FilePath)) + FilePath_Length;

    // Build up ArchiveSize from FileSizes, HeaderEnd will be added at the end
    BIGFile_Header->ArchiveSize += DirectoryEntry->FileLength;

    // Add current DirectoryEntry to the Header DirEntry list
    BIGFile_Header->DirectoryEntry[BIGFile_Header->NumFiles] = DirectoryEntry;
    BIGFile_Header->NumFiles++;

    return BIGFile_Header;
}

void
BIGFile_Extract(char *BIGFile_Path, char *ExtractPath)
{
    FILE *BIGFile_Handle, *ExtractFile_Handle;
    BIGHeader *BIGFile_Header = NULL;
    BIGDirectoryEntry *DirectoryEntry = NULL;
    void *BIGFile_Buffer = NULL;
    char ExtractPath_Actual[PATH_MAX_WIN];
    int i;

    // Open BIGFile and get Size
    BIGFile_Handle = fopen_d(BIGFile_Path, "rb");

    // Get full Header from BIG file
    BIGFile_Header = BIGFileHeader_Parse(BIGFile_Handle);

    mkdir_d(ExtractPath);

    // Loop through DirEntry list and extract files
    for(i = 0; i < BIGFile_Header->NumFiles; i++)
    {
        DirectoryEntry = BIGFile_Header->DirectoryEntry[i];

        // Get full extraction path
        snprintf(ExtractPath_Actual, PATH_MAX_WIN, "%s/%s", ExtractPath, DirectoryEntry->FilePath);

        ExtractFile_Handle = fopen_r(ExtractPath_Actual);

        // Seek to file position in BIGFile
        fseek(BIGFile_Handle, DirectoryEntry->FileOffset, SEEK_SET);

        // Read output file from BIGFile into Buffer, write from Buffer into output file
        BIGFile_Buffer = realloc_d(BIGFile_Buffer, DirectoryEntry->FileLength);
        fread(BIGFile_Buffer, DirectoryEntry->FileLength, 1, BIGFile_Handle);

        fwrite(BIGFile_Buffer, DirectoryEntry->FileLength, 1, ExtractFile_Handle);
        fclose(ExtractFile_Handle);

        printf("%s\n", DirectoryEntry->FilePath);
    }

    fclose(BIGFile_Handle);
}

BIGHeader *
BIGFileHeader_Parse(FILE *BIGFile_Handle)
{
    BIGHeader *BIGFile_Header = NULL;
    BIGDirectoryEntry *DirectoryEntry = NULL;
    char FilePath_Buffer[PATH_MAX_WIN];
    int FilePath_Start, FilePath_Length;
    int BIGFile_Size;
    int i;

    // Initial Header malloc
    BIGFile_Header = malloc_d(sizeof(BIGHeader));

    // Read Header struct from BIGFile
    BIGFile_Size = fsize(BIGFile_Handle);
    fread(BIGFile_Header, sizeof(BIGHeader), 1, BIGFile_Handle);

    // byteswap because this value is always BE
    BIGFile_Header->MagicHeader = __builtin_bswap32(BIGFile_Header->MagicHeader);

    // Some sanity checks for header values
    if(BIGFile_Header->MagicHeader != BIGF_MAGIC && BIGFile_Header->MagicHeader != BIG4_MAGIC)
    {
        printf_error_exit("Wrong header in BIG file, only BIGF and BIG4 are supported", "");
    }

    // If filesize is wrong, try as BE for some older BIGF versions
    if(BIGFile_Header->MagicHeader == BIGF_MAGIC && BIGFile_Size != BIGFile_Header->ArchiveSize)
    {
        BIGFile_Header->ArchiveSize = __builtin_bswap32(BIGFile_Header->ArchiveSize);

        // Older games seem to have wrong ArchiveSizes in the header relatively often
        // By checking some files from FIFA 2002 they only seem to have *smaller* sizes, so only fail if the reported size is bigger
        // Additionally, FIFA 2002 ignores the ArchiveSize value (as does BFME)
        if(BIGFile_Header->ArchiveSize > BIGFile_Size)
        {
            printf_error_exit("Wrong archive size in header, ArchiveSize bigger than FileSize", "");
        }

        // But at least display a warning
        if(BIGFile_Header->ArchiveSize != BIGFile_Size)
        {
            printf("Warning: Wrong archive size in header, ArchiveSize smaller than FileSize\n");
            printf("Check the last file for validity\n\n");
        }
    }
    // In any other case, fail if ArchiveSize is wrong
    else if(BIGFile_Header->ArchiveSize != BIGFile_Size)
    {
        printf_error_exit("Wrong archive size in header", "");
    }

    BIGFile_Header->NumFiles = __builtin_bswap32(BIGFile_Header->NumFiles);
    BIGFile_Header->HeaderEnd = __builtin_bswap32(BIGFile_Header->HeaderEnd);

    // realloc enough memory for all Files
    BIGFile_Header = realloc_d(BIGFile_Header, sizeof(BIGHeader) + (BIGFile_Header->NumFiles * sizeof(BIGDirectoryEntry)));

    // loop through DirEntry list in header and get data
    for(i = 0; i < BIGFile_Header->NumFiles; i++)
    {
        DirectoryEntry = malloc_d(sizeof(BIGDirectoryEntry));

        // Read DirEntry struct minus FilePath
        fread(DirectoryEntry, sizeof(BIGDirectoryEntry) - sizeof(DirectoryEntry->FilePath), 1, BIGFile_Handle);

        // byteswap because these values are always BE
        DirectoryEntry->FileOffset = __builtin_bswap32(DirectoryEntry->FileOffset);
        DirectoryEntry->FileLength = __builtin_bswap32(DirectoryEntry->FileLength);

        // Get FilePath and Length (fgets_c reads until null termination)
        FilePath_Start = ftell(BIGFile_Handle);
        fgets_c(FilePath_Buffer, PATH_MAX_WIN, BIGFile_Handle);
        FilePath_Length = ftell(BIGFile_Handle) - FilePath_Start;

        DirectoryEntry->FilePath = malloc_d(FilePath_Length);
        strncpy_d(DirectoryEntry->FilePath, FilePath_Buffer, FilePath_Length);

        BIGFile_Header->DirectoryEntry[i] = DirectoryEntry;
    }

    return BIGFile_Header;
}
