#include "big4f.h"

void BIGFile_Pack(char *InputDir, char *BIGFile_Path, char BIGFormat)
{
	FILE *BIGFile_Handle, *InputFile_Handle;
	BIGHeader *BIGFile_Header = NULL;
	BIGDirectoryEntry *DirectoryEntry = NULL;
	BIG4FHeader *BIG4F_Header = NULL;
	BIG4FFileEntry *BIG4F_FileEntry = NULL;
	char InputFile_Path[PATH_MAX_WIN];
	void *InputFile_Buffer = NULL;
	int i;
	int BIGFile_Header_LastPos;
	int BIGFile_Header_AllocSize = 10000;

	// Initial malloc for header
	BIGFile_Header = malloc_d(sizeof(BIGHeader) + (BIGFile_Header_AllocSize * sizeof(BIGDirectoryEntry *)));

	// Get format and set MagicHeader, byteswap because these values need to be BE
	if (BIGFormat == 'f')
	{
		BIGFile_Header->MagicHeader = __builtin_bswap32(BIGF_MAGIC);
	}
	else
	{
		BIGFile_Header->MagicHeader = __builtin_bswap32(BIG4_MAGIC);
	}

	// Temporary values, will be filled in BIGFileHeader_Create() and finalised after
	BIGFile_Header->HeaderSize = sizeof(BIGHeader);
	BIGFile_Header->ArchiveSize = 0;
	BIGFile_Header->NumFiles = 0;

	// Get DirectoryEntries + some initial Header data
	BIGFile_Header = BIGFileHeader_Create(BIGFile_Header, InputDir, InputDir, BIGFile_Header_AllocSize);

#ifdef _WIN32
	// Custom header info for 0.4
	BIG4F_Header = malloc_d(sizeof(BIG4FHeader));
	BIG4F_FileEntry = malloc_d(sizeof(BIG4FFileEntry));

	BIGFile_Header->HeaderSize += sizeof(BIG4FHeader);
	BIG4F_Header->BIG4FId = __builtin_bswap64(BIG4F_MAGIC);

	BIGFile_Header->ArchiveSize += BIGFile_Header->NumFiles * sizeof(BIG4FFileEntry);
#endif

	if (BIGFile_Header->NumFiles == 0)
	{
		printf_error_exit("No files in directory ", InputDir);
	}

	// Final ArchiveSize
	// Apparently gets ignored by (most) games that use this format, so just keep it LE
	BIGFile_Header->ArchiveSize += BIGFile_Header->HeaderSize;

	// Open BigFile and set to ArchiveSize
	BIGFile_Handle = fopen_d(BIGFile_Path, "wb");
	ftruncate(fileno(BIGFile_Handle), BIGFile_Header->ArchiveSize);

	// Set initial LastPos for first DirEntry offset
	BIGFile_Header_LastPos = sizeof(BIGHeader);

	// Loop through all DirEntries, read and write files into BIGFile, write Header
	for (i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		DirectoryEntry = BIGFile_Header->DirectoryEntry[i];

		// Restore full InputFile Path after being trimmed in SetHeader()
		snprintf(InputFile_Path, PATH_MAX_WIN, "%s/%s", InputDir, DirectoryEntry->FilePath);

		// Finalise relative offset
		DirectoryEntry->FileOffset += BIGFile_Header->HeaderSize;

#ifdef _WIN32
		// Write custom 0.4 header
		WIN32_FILE_ATTRIBUTE_DATA FileInfo;
		GetFileAttributesEx(InputFile_Path, GetFileExInfoStandard, &FileInfo);

		BIG4F_FileEntry->DateCreated = ft2int64(&FileInfo.ftCreationTime);
		BIG4F_FileEntry->DateAccessed = ft2int64(&FileInfo.ftLastAccessTime);
		BIG4F_FileEntry->DateModified = ft2int64(&FileInfo.ftLastWriteTime);

		fseek(BIGFile_Handle, DirectoryEntry->FileOffset - sizeof(BIG4FFileEntry), SEEK_SET);
		fwrite(&BIG4F_FileEntry->DateCreated, sizeof(BIG4F_FileEntry->DateCreated), 1, BIGFile_Handle);
		fwrite(&BIG4F_FileEntry->DateAccessed, sizeof(BIG4F_FileEntry->DateAccessed), 1, BIGFile_Handle);
		fwrite(&BIG4F_FileEntry->DateModified, sizeof(BIG4F_FileEntry->DateModified), 1, BIGFile_Handle);
#endif

		InputFile_Handle = fopen_d(unixify_path(InputFile_Path), "rb");

		// Read full Input File
		InputFile_Buffer = malloc_d(DirectoryEntry->FileLength);
		fread(InputFile_Buffer, DirectoryEntry->FileLength, 1, InputFile_Handle);

		// Seek to FileOffset in BIGFile and write data
		fseek(BIGFile_Handle, DirectoryEntry->FileOffset, SEEK_SET);
		fwrite(InputFile_Buffer, DirectoryEntry->FileLength, 1, BIGFile_Handle);

		free(InputFile_Buffer);
		fclose(InputFile_Handle);

		// byteswap because these values need to be BE
		DirectoryEntry->FileOffset = __builtin_bswap32(DirectoryEntry->FileOffset);
		DirectoryEntry->FileLength = __builtin_bswap32(DirectoryEntry->FileLength);

		// write this DirEntry to BIGFileHeader, remember position for next DirEntry
		fseek(BIGFile_Handle, BIGFile_Header_LastPos, SEEK_SET);
		fwrite(&DirectoryEntry->FileOffset, sizeof(DirectoryEntry->FileOffset), 1, BIGFile_Handle);
		fwrite(&DirectoryEntry->FileLength, sizeof(DirectoryEntry->FileLength), 1, BIGFile_Handle);
		fwrite(DirectoryEntry->FilePath, strlen(DirectoryEntry->FilePath) + 1, 1, BIGFile_Handle);
		BIGFile_Header_LastPos = ftell(BIGFile_Handle);

		printf("%s\n", DirectoryEntry->FilePath);
	}

#ifdef _WIN32
	// Write custom header info for 0.4
	fseek(BIGFile_Handle, BIGFile_Header->HeaderSize - sizeof(BIG4FHeader), SEEK_SET);
	fwrite(&BIG4F_Header->BIG4FId, sizeof(BIG4F_Header->BIG4FId), 1, BIGFile_Handle);
#endif

	// byteswap because these values need to be BE, fix abused HeaderSize (actually FirstFileOffset) to be FirstFileOffset
	BIGFile_Header->NumFiles = __builtin_bswap32(BIGFile_Header->NumFiles);
	BIGFile_Header->HeaderSize = BIGFile_Header->DirectoryEntry[0]->FileOffset;

	// Write header at the end so we can work with unswapped values before
	fseek(BIGFile_Handle, 0, SEEK_SET);
	fwrite(&BIGFile_Header->MagicHeader, sizeof(BIGFile_Header->MagicHeader), 1, BIGFile_Handle);
	fwrite(&BIGFile_Header->ArchiveSize, sizeof(BIGFile_Header->ArchiveSize), 1, BIGFile_Handle);
	fwrite(&BIGFile_Header->NumFiles, sizeof(BIGFile_Header->NumFiles), 1, BIGFile_Handle);
	fwrite(&BIGFile_Header->HeaderSize, sizeof(BIGFile_Header->HeaderSize), 1, BIGFile_Handle);

	// Close BIGFile, done
	fclose(BIGFile_Handle);
}

BIGHeader *BIGFileHeader_Create(BIGHeader *BIGFile_Header, char *InputDir, char *SearchDir, int AllocSize)
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

	if (h == INVALID_HANDLE_VALUE)
	{
		printf_error_exit("Path not found: ", SearchDir);
	}

	do
	{
		// Find first file will always return "." and ".." as the first two directories.
		if (strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, ".."))
		{
			// Build up our file path using the passed in SearchDir and the file/foldername we just found:
			snprintf(SearchPath, PATH_MAX_WIN, "%s/%s", SearchDir, fd.cFileName);

			// Is the entity a File or Folder?
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// Recursion
				BIGFile_Header = BIGFileHeader_Create(BIGFile_Header, InputDir, SearchPath, AllocSize);
			}
			else
			{
				// Check if the alloc'd memory is still big enough, if not allocate twice as much
				if (BIGFile_Header->NumFiles == AllocSize)
				{
					AllocSize *= 2;
					BIGFile_Header = realloc_d(BIGFile_Header, sizeof(BIGHeader) + (AllocSize * sizeof(BIGDirectoryEntry *)));
				}

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

	if (errno)
	{
		printf_error_exit("Input directory could not be opened: ", InputDir);
	}

	while ((ent = fts_read(fts)) != NULL)
	{
		// No errno checking because fts_read adds random backslashes into pathnames, resulting in "No such dir"
		// fopen() will fail later on if the file cannot actually be opened and the program will exit
		if (ent->fts_info & FTS_F)
		{
			// Check if the alloc'd memory is still big enough, if not allocate twice as much
			if (BIGFile_Header->NumFiles == AllocSize)
			{
				AllocSize *= 2;
				BIGFile_Header = realloc_d(BIGFile_Header, sizeof(BIGHeader) + (AllocSize * sizeof(BIGDirectoryEntry *)));
			}

			// Found a file, let's add it to the Directory entries
			BIGFile_Header = BIGFileHeader_AddDirectoryEntry(BIGFile_Header, unixify_path(InputDir), unixify_path(ent->fts_path));
		}
	}

	fts_close(fts);
#endif

	return BIGFile_Header;
}

BIGHeader *BIGFileHeader_AddDirectoryEntry(BIGHeader *BIGFile_Header, char *InputDir, char *FullFilePath)
{
	BIGDirectoryEntry *DirectoryEntry = NULL;
	int FilePath_Start, FilePath_Length;

	DirectoryEntry = malloc_d(sizeof(BIGDirectoryEntry));

#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA FileInfo;
	GetFileAttributesEx(FullFilePath, GetFileExInfoStandard, &FileInfo);

	DirectoryEntry->FileLength = FileInfo.nFileSizeLow;
#else
	// Open found file to get filesize
	FILE *f;
	f = fopen_d(FullFilePath, "rb");
	DirectoryEntry->FileLength = fsize(f);
	fclose(f);
#endif

	// Temporary relative offset, will have to be adjusted after all files are known
	if (BIGFile_Header->NumFiles == 0)
	{
#ifdef _WIN32
		// DirectoryEntry->FileOffset = sizeof(BIG4FHeader);
		DirectoryEntry->FileOffset = 0;
#else
		DirectoryEntry->FileOffset = 0;
#endif
	}
	else
	{
		// Last FileLength + last FileOffset
		DirectoryEntry->FileOffset = BIGFile_Header->DirectoryEntry[BIGFile_Header->NumFiles - 1]->FileLength
										+ BIGFile_Header->DirectoryEntry[BIGFile_Header->NumFiles - 1]->FileOffset;
	}

#ifdef _WIN32
	DirectoryEntry->FileOffset += sizeof(BIG4FFileEntry);
#endif

	// Trim the InputDir from the FilePath
	FilePath_Start = strlen(InputDir) + 1;
	FilePath_Length = (strlen(FullFilePath) - FilePath_Start) + 1;
	DirectoryEntry->FilePath = malloc_d(FilePath_Length);
	strncpy_d(DirectoryEntry->FilePath, windowsify_path(FullFilePath + FilePath_Start), FilePath_Length);

	// Build up HeaderSize: Each DirEntry is 4 + 4 + strlen(FilePath) + 1, or rather FilePath_Length
	// Is final after last run, and will then be set to FileOffset of first File
	BIGFile_Header->HeaderSize += (sizeof(BIGDirectoryEntry) - sizeof(DirectoryEntry->FilePath)) + FilePath_Length;

	// Build up ArchiveSize from FileSizes, FirstFileOffset will be added at the end
	BIGFile_Header->ArchiveSize += DirectoryEntry->FileLength;

	// Add current DirectoryEntry to the Header DirEntry list
	BIGFile_Header->DirectoryEntry[BIGFile_Header->NumFiles] = DirectoryEntry;
	BIGFile_Header->NumFiles++;

	return BIGFile_Header;
}

void BIGFile_List(char *BIGFile_Path)
{
	FILE *BIGFile_Handle;
	BIGHeader *BIGFile_Header = NULL;
	int i;

	// Open BIGFile
	BIGFile_Handle = fopen_d(BIGFile_Path, "rb");

	// Get full Header from BIG file
	BIGFile_Header = BIGFileHeader_Parse(BIGFile_Path, BIGFile_Handle);

	// Loop through DirEntry list and list files
	for (i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		printf("%s\n", BIGFile_Header->DirectoryEntry[i]->FilePath);
	}

	// Close BIGFile, done
	fclose(BIGFile_Handle);
}

void BIGFile_Extract(char *BIGFile_Path, char *ExtractPath)
{
	FILE *BIGFile_Handle, *ExtractFile_Handle;
	BIGHeader *BIGFile_Header = NULL;
	BIGDirectoryEntry *DirectoryEntry = NULL;
	BIG4FHeader *BIG4F_Header = NULL;
	BIG4FFileEntry *BIG4F_FileEntry = NULL;
	void *BIGFile_Buffer = NULL;
	char ExtractPath_Actual[PATH_MAX_WIN];
	int i;

	// Open BIGFile
	BIGFile_Handle = fopen_d(BIGFile_Path, "rb");

	// Get full Header from BIG file
	BIGFile_Header = BIGFileHeader_Parse(BIGFile_Path, BIGFile_Handle);

#ifdef _WIN32
	FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
	WIN32_FILE_ATTRIBUTE_DATA FileInfo;

	BIG4F_FileEntry = malloc_d(sizeof(BIG4FFileEntry));

	// Get custom header info for 0.4 if it exists
	BIG4F_Header = malloc_d(sizeof(BIG4FHeader));
	fseek(BIGFile_Handle, BIGFile_Header->HeaderSize - sizeof(BIG4FHeader) - sizeof(BIG4FFileEntry), SEEK_SET);
	fread(&BIG4F_Header->BIG4FId, sizeof(BIG4F_Header->BIG4FId), 1, BIGFile_Handle);
	BIG4F_Header->BIG4FId = __builtin_bswap64(BIG4F_Header->BIG4FId);

	if (BIG4F_Header->BIG4FId == BIG4F_MAGIC)
	{
		BIGFile_Header->HeaderSize -= sizeof(BIG4FFileEntry);
	}
	else
	{
		BIG4F_Header->BIG4FId = 0;

		GetFileAttributesEx(BIGFile_Path, GetFileExInfoStandard, &FileInfo);

		ftCreationTime = FileInfo.ftCreationTime;
		ftLastAccessTime = FileInfo.ftLastAccessTime;
		ftLastWriteTime = FileInfo.ftLastWriteTime;
	}
#endif

	mkdir_d(ExtractPath);

	// Loop through DirEntry list and extract files
	for (i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		DirectoryEntry = BIGFile_Header->DirectoryEntry[i];

		// Get full extraction path
		snprintf(ExtractPath_Actual, PATH_MAX_WIN, "%s/%s", ExtractPath, DirectoryEntry->FilePath);

		ExtractFile_Handle = fopen_r(ExtractPath_Actual);

		// Seek to file position in BIGFile
		fseek(BIGFile_Handle, DirectoryEntry->FileOffset, SEEK_SET);

		// Read output file from BIGFile into Buffer, write from Buffer into output file
		BIGFile_Buffer = malloc_d(DirectoryEntry->FileLength);

		fread(BIGFile_Buffer, DirectoryEntry->FileLength, 1, BIGFile_Handle);
		fwrite(BIGFile_Buffer, DirectoryEntry->FileLength, 1, ExtractFile_Handle);

		free(BIGFile_Buffer);
		fclose(ExtractFile_Handle);

#ifdef _WIN32
		HANDLE ExtractFile_WIN32Handle;

		ExtractFile_WIN32Handle = CreateFile(ExtractPath_Actual, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (!ExtractFile_WIN32Handle)
		{
			printf_error_exit("Could not open previously created file for setting time attributes: ", ExtractPath_Actual);
		}

		if (BIG4F_Header->BIG4FId)
		{
			fseek(BIGFile_Handle, DirectoryEntry->FileOffset - sizeof(BIG4FFileEntry), SEEK_SET);

			fread(&BIG4F_FileEntry->DateCreated, sizeof(BIG4F_FileEntry->DateCreated), 1, BIGFile_Handle);
			fread(&BIG4F_FileEntry->DateAccessed, sizeof(BIG4F_FileEntry->DateAccessed), 1, BIGFile_Handle);
			fread(&BIG4F_FileEntry->DateModified, sizeof(BIG4F_FileEntry->DateModified), 1, BIGFile_Handle);

			int642ft(&ftCreationTime, BIG4F_FileEntry->DateCreated);
			int642ft(&ftLastAccessTime, BIG4F_FileEntry->DateAccessed);
			int642ft(&ftLastWriteTime, BIG4F_FileEntry->DateModified);
		}

		SetFileTime(ExtractFile_WIN32Handle, &ftCreationTime, &ftLastAccessTime, &ftLastWriteTime);

		CloseHandle(ExtractFile_WIN32Handle);
#endif

		printf("%s\n", DirectoryEntry->FilePath);
	}

	// Close BIGFile, done
	fclose(BIGFile_Handle);
}

BIGHeader *BIGFileHeader_Parse(char *BIGFile_Path, FILE *BIGFile_Handle)
{
	BIGHeader *BIGFile_Header = NULL;
	BIGDirectoryEntry *DirectoryEntry = NULL;
	char FilePath_Buffer[PATH_MAX_WIN];
	int FilePath_Start, FilePath_Length;
	uint32_t BIGFile_Size;
	int i;

	// Initial Header malloc
	BIGFile_Header = malloc_d(sizeof(BIGHeader));

	// Get Size
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA FileInfo;
	GetFileAttributesEx(BIGFile_Path, GetFileExInfoStandard, &FileInfo);

	BIGFile_Size = FileInfo.nFileSizeLow;
#else
	BIGFile_Size = fsize(BIGFile_Handle);
#endif

	// Read Header struct from BIGFile
	fread(&BIGFile_Header->MagicHeader, sizeof(BIGFile_Header->MagicHeader), 1, BIGFile_Handle);
	fread(&BIGFile_Header->ArchiveSize, sizeof(BIGFile_Header->ArchiveSize), 1, BIGFile_Handle);
	fread(&BIGFile_Header->NumFiles, sizeof(BIGFile_Header->NumFiles), 1, BIGFile_Handle);
	fread(&BIGFile_Header->HeaderSize, sizeof(BIGFile_Header->HeaderSize), 1, BIGFile_Handle);

	// byteswap because this value is always BE
	BIGFile_Header->MagicHeader = __builtin_bswap32(BIGFile_Header->MagicHeader);

	// Some sanity checks for header values
	if (BIGFile_Header->MagicHeader != BIGF_MAGIC && BIGFile_Header->MagicHeader != BIG4_MAGIC)
	{
		printf_error_exit("Wrong header in BIG file, only BIGF and BIG4 are supported", "");
	}

	// Older games seem to have wrong ArchiveSizes in the header relatively often
	// Seems to get ignored by most games that use this file format
	// If filesize is wrong, try as BE for some older BIGF versions
	if (BIGFile_Header->MagicHeader == BIGF_MAGIC && BIGFile_Size != BIGFile_Header->ArchiveSize)
	{
		BIGFile_Header->ArchiveSize = __builtin_bswap32(BIGFile_Header->ArchiveSize);

		if (BIGFile_Header->ArchiveSize != BIGFile_Size)
		{
			printf("Warning: Wrong archive size in header\n\n");
		}
	}
	else if (BIGFile_Header->ArchiveSize != BIGFile_Size)
	{
		printf("Warning: Wrong archive size in header\n\n");
	}

	BIGFile_Header->NumFiles = __builtin_bswap32(BIGFile_Header->NumFiles);
	BIGFile_Header->HeaderSize = __builtin_bswap32(BIGFile_Header->HeaderSize);

	// realloc enough memory for all DirectoryEntries
	BIGFile_Header = realloc_d(BIGFile_Header, sizeof(BIGHeader) + (BIGFile_Header->NumFiles * sizeof(BIGDirectoryEntry *)));

	// loop through DirEntry list in header and get data
	for (i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		DirectoryEntry = malloc_d(sizeof(BIGDirectoryEntry));

		// Read DirEntry struct minus FilePath
		fread(&DirectoryEntry->FileOffset, sizeof(DirectoryEntry->FileOffset), 1, BIGFile_Handle);
		fread(&DirectoryEntry->FileLength, sizeof(DirectoryEntry->FileLength), 1, BIGFile_Handle);

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
