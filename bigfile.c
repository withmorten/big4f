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
	uint32_t BIGFile_Header_LastPos;
	uint32_t BIGFile_Header_AllocSize = 10000;

	// Initial malloc for header
	BIGFile_Header = malloc_d(sizeof(BIGHeader));

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
	BIGFile_Header->HeaderSize = sizeof(BIGHeader) - sizeof(BIGDirectoryEntry *);
	BIGFile_Header->ArchiveSize = 0;
	BIGFile_Header->NumFiles = 0;

	// Initial malloc for Directory
	BIGFile_Header->Directory = malloc_d(sizeof(BIGDirectoryEntry) * BIGFile_Header_AllocSize);

	// Get DirectoryEntries + some initial Header data
	BIGFile_Header = BIGFileHeader_Create(BIGFile_Header, InputDir, InputDir, BIGFile_Header_AllocSize);

	// Custom header info for 0.4
	BIG4F_Header = malloc_d(sizeof(BIG4FHeader));
	BIG4F_FileEntry = malloc_d(sizeof(BIG4FFileEntry));

	BIGFile_Header->HeaderSize += sizeof(BIG4FHeader);
	BIG4F_Header->BIG4FId = __builtin_bswap64(BIG4F_MAGIC);

	BIGFile_Header->ArchiveSize += BIGFile_Header->NumFiles * sizeof(BIG4FFileEntry);

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
	BIGFile_Header_LastPos = sizeof(BIGHeader) - sizeof(BIGDirectoryEntry *);

	file_time_info ti;

	// Loop through all DirEntries, read and write files into BIGFile, write Header
	for (uint32_t i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		DirectoryEntry = &BIGFile_Header->Directory[i];

		// Restore full InputFile Path after being trimmed in SetHeader()
		snprintf(InputFile_Path, PATH_MAX_WIN, "%s/%s", InputDir, DirectoryEntry->FilePath);

		// Finalise relative offset
		DirectoryEntry->FileOffset += BIGFile_Header->HeaderSize;

		// Write custom 0.4 header
		get_file_time_info(&ti, InputFile_Path);

		BIG4F_FileEntry->DateAccessed = ti.atime;
		BIG4F_FileEntry->DateModified = ti.mtime;

		fseek(BIGFile_Handle, DirectoryEntry->FileOffset - sizeof(BIG4FFileEntry), SEEK_SET);

		fwrite(&BIG4F_FileEntry->DateAccessed, sizeof(BIG4F_FileEntry->DateAccessed), 1, BIGFile_Handle);
		fwrite(&BIG4F_FileEntry->DateModified, sizeof(BIG4F_FileEntry->DateModified), 1, BIGFile_Handle);

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

	// Write custom header info for 0.4
	fseek(BIGFile_Handle, BIGFile_Header->HeaderSize - sizeof(BIG4FHeader), SEEK_SET);
	fwrite(&BIG4F_Header->BIG4FId, sizeof(BIG4F_Header->BIG4FId), 1, BIGFile_Handle);

	// byteswap because these values need to be BE, fix abused HeaderSize (actually FirstFileOffset) to be FirstFileOffset
	BIGFile_Header->NumFiles = __builtin_bswap32(BIGFile_Header->NumFiles);
	BIGFile_Header->HeaderSize = BIGFile_Header->Directory[0].FileOffset;

	// Write header at the end so we can work with unswapped values before
	fseek(BIGFile_Handle, 0, SEEK_SET);
	fwrite(&BIGFile_Header->MagicHeader, sizeof(BIGFile_Header->MagicHeader), 1, BIGFile_Handle);
	fwrite(&BIGFile_Header->ArchiveSize, sizeof(BIGFile_Header->ArchiveSize), 1, BIGFile_Handle);
	fwrite(&BIGFile_Header->NumFiles, sizeof(BIGFile_Header->NumFiles), 1, BIGFile_Handle);
	fwrite(&BIGFile_Header->HeaderSize, sizeof(BIGFile_Header->HeaderSize), 1, BIGFile_Handle);

	// Free stuff
	BIGFile_Header->NumFiles = __builtin_bswap32(BIGFile_Header->NumFiles);

	free(BIG4F_FileEntry);
	free(BIG4F_Header);

	for (uint32_t i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		free(BIGFile_Header->Directory[i].FilePath);
	}

	free(BIGFile_Header->Directory);
	free(BIGFile_Header);

	// Close BIGFile, done
	fclose(BIGFile_Handle);
}

BIGHeader *BIGFileHeader_Create(BIGHeader *BIGFile_Header, char *InputDir, char *SearchDir, uint32_t AllocSize)
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
			// Special exception for deskopt.ini - if hidden
			if (!strcmp(fd.cFileName, "desktop.ini") && fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			{
				continue;
			}

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
					BIGFile_Header->Directory = realloc_d(BIGFile_Header->Directory, AllocSize * sizeof(BIGDirectoryEntry));
				}

				// Found a file, let's add it to the Directory entries
				BIGFile_Header = BIGFileHeader_AddDirectoryEntry(BIGFile_Header, InputDir, SearchPath);
			}
		}
	}
	while (FindNextFile(h, &fd));

	FindClose(h);
#else
	FTS *fts = NULL;
	FTSENT *ent = NULL;
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
				BIGFile_Header->Directory = realloc_d(BIGFile_Header->Directory, AllocSize * sizeof(BIGDirectoryEntry));
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
	uint32_t FilePath_Start, FilePath_Length;

	BIGDirectoryEntry *DirectoryEntry = &BIGFile_Header->Directory[BIGFile_Header->NumFiles];

	DirectoryEntry->FileLength = fsize(FullFilePath);

	// Temporary relative offset, will have to be adjusted after all files are known
	if (BIGFile_Header->NumFiles == 0)
	{
		DirectoryEntry->FileOffset = 0;
	}
	else
	{
		// Last FileLength + last FileOffset
		DirectoryEntry->FileOffset = BIGFile_Header->Directory[BIGFile_Header->NumFiles - 1].FileLength
										+ BIGFile_Header->Directory[BIGFile_Header->NumFiles - 1].FileOffset;
	}

	DirectoryEntry->FileOffset += sizeof(BIG4FFileEntry);

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

	BIGFile_Header->NumFiles++;

	return BIGFile_Header;
}

void BIGFile_List(char *BIGFile_Path)
{
	FILE *BIGFile_Handle;
	BIGHeader *BIGFile_Header = NULL;

	// Open BIGFile
	BIGFile_Handle = fopen_d(BIGFile_Path, "rb");

	// Get full Header from BIG file
	BIGFile_Header = BIGFileHeader_Parse(BIGFile_Path, BIGFile_Handle);

	// Loop through DirEntry list and list files
	for (uint32_t i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		printf("%s\n", BIGFile_Header->Directory[i].FilePath);
	}

	// Free stuff
	for (uint32_t i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		free(BIGFile_Header->Directory[i].FilePath);
	}

	free(BIGFile_Header->Directory);
	free(BIGFile_Header);

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

	// Open BIGFile
	BIGFile_Handle = fopen_d(BIGFile_Path, "rb");

	// Get full Header from BIG file
	BIGFile_Header = BIGFileHeader_Parse(BIGFile_Path, BIGFile_Handle);

	file_time_info ti;

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

		// If custom header doesn't exist, use the times from the big file
		get_file_time_info(&ti, BIGFile_Path);
	}

	mkdir_d(ExtractPath);

	// Loop through DirEntry list and extract files
	for (uint32_t i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		DirectoryEntry = &BIGFile_Header->Directory[i];

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

		if (BIG4F_Header->BIG4FId)
		{
			fseek(BIGFile_Handle, DirectoryEntry->FileOffset - sizeof(BIG4FFileEntry), SEEK_SET);

			fread(&BIG4F_FileEntry->DateAccessed, sizeof(BIG4F_FileEntry->DateAccessed), 1, BIGFile_Handle);
			fread(&BIG4F_FileEntry->DateModified, sizeof(BIG4F_FileEntry->DateModified), 1, BIGFile_Handle);

			ti.atime = BIG4F_FileEntry->DateAccessed;
			ti.mtime = BIG4F_FileEntry->DateModified;
		}

		set_file_time_info(&ti, ExtractPath_Actual);

		printf("%s\n", DirectoryEntry->FilePath);
	}

	// Free stuff
	free(BIG4F_FileEntry);
	free(BIG4F_Header);

	for (uint32_t i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		free(BIGFile_Header->Directory[i].FilePath);
	}

	free(BIGFile_Header->Directory);
	free(BIGFile_Header);

	// Close BIGFile, done
	fclose(BIGFile_Handle);
}

BIGHeader *BIGFileHeader_Parse(char *BIGFile_Path, FILE *BIGFile_Handle)
{
	BIGHeader *BIGFile_Header = NULL;
	BIGDirectoryEntry *DirectoryEntry = NULL;
	char FilePath_Buffer[PATH_MAX_WIN];
	uint32_t FilePath_Start, FilePath_Length;
	uint32_t BIGFile_Size;

	// Initial Header malloc
	BIGFile_Header = malloc_d(sizeof(BIGHeader));

	// Get Size
	BIGFile_Size = fsize(BIGFile_Path);

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
	BIGFile_Header->Directory = malloc_d(BIGFile_Header->NumFiles * sizeof(BIGDirectoryEntry));

	// loop through DirEntry list in header and get data
	for (uint32_t i = 0; i < BIGFile_Header->NumFiles; i++)
	{
		DirectoryEntry = &BIGFile_Header->Directory[i];

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
	}

	return BIGFile_Header;
}
