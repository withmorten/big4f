#include "big4f.h"

void printf_help_exit(void)
{
	printf("big4f v%i.%i by withmorten\n\n", BIG4F_VERSION_MAJOR, BIG4F_VERSION_MINOR);
	printf("big4f supports the following arguments:\n\n");
	printf("big4f l <big file> to list a .big file's contents\n");
	printf("big4f e/x <big file> <directory> to extract a .big file\n");
	printf("big4f f/4 <directory> <big file> to pack a .big file\n\n");
	printf("'f' will create a BIGF file, '4' a BIG4 file\n\n");
	printf("big4f's source and readme are available at https://github.com/withmorten/big4f\n");
	exit(1);
}

void printf_error_exit(char *message, char *filename)
{
	printf("Error: %s%s\n", message, filename);
	exit(1);
}

FILE *fopen_d(char *path, const char *mode)
{
	FILE *f = fopen(path, mode);

	if (f == NULL)
	{
		printf("Error: couldn't fopen() file %s, exiting\n", systemify_path(path));
		exit(1);
	}

	return f;
}

FILE *fopen_r(char *filepath)
{
	FILE *f;
	char *path;

	filepath = unixify_path(filepath);

	path = strndup(filepath, strlen(filepath) - strlen(strrchr(filepath, '/')));

	mkdir_d(path);

	f = fopen_d(filepath, "wb");

	return f;
}

uint32_t fsize(FILE *stream)
{
	int fpos, fsize;

	fpos = ftell(stream);
	fseek(stream, 0, SEEK_END);
	fsize = ftell(stream);
	fseek(stream, fpos, SEEK_SET);

	return fsize;
}

#ifdef _WIN32
uint64_t ft2int64(FILETIME *ft)
{
	ULARGE_INTEGER lv_large;

	lv_large.LowPart = ft->dwLowDateTime;
	lv_large.HighPart = ft->dwHighDateTime;

	return lv_large.QuadPart;
}

void int642ft(FILETIME *ft, uint64_t i64)
{
	ft->dwLowDateTime = LODWORD(i64);
	ft->dwHighDateTime = HIDWORD(i64);
}
#endif

void *malloc_d(size_t size)
{
	void *m = malloc(size);

	if (m == NULL && size != 0)
	{
		printf("Error: couldn't malloc() %i bytes of memory, exiting\n", (int)size);
		exit(1);
	}

	return m;
}

void *calloc_d(size_t nitems, size_t size)
{
	void *m = calloc(nitems, size);

	if (m == NULL && size != 0)
	{
		printf("Error: couldn't calloc() %i bytes of memory, exiting\n", (int)size);
		exit(1);
	}

	return m;
}

void *realloc_d(void *ptr, size_t size)
{
	void *m = realloc(ptr, size);

	if (m == NULL && size != 0)
	{
		printf("Error: couldn't realloc() %i bytes of memory, exiting\n", (int)size);
		exit(1);
	}

	return m;
}

char *strndup(const char *str, size_t size)
{
	char *dup = calloc_d(1, size + 1);

	return memcpy(dup, str, size);
}

char *strncpy_d(char *dest, const char *src, size_t n)
{
	if (strlen(src) + 1 > n)
	{
		printf("Error: String too long: %s\n", src);
		exit(1);
	}

	return strncpy(dest, src, n);
}

char *fgets_c(char *str, int n, FILE *stream)
{
	int c;

	for (n--; n > 0; n--)
	{
		c = fgetc(stream);

		if (c == EOF)
		{
			*(str++) = '\0';
			break;
		}

		*(str++) = (char)c;

		if (n == 1)
		{
			*str = '\0';
			printf("Warning: Following filename too long, truncating.\n");
		}

		if (c == '\0')
		{
			break;
		}
	}

	return str;
}

char *unixify_path(char *path)
{
	char *p;

	for (p = path; *p; p++)
	{
		if (*p == '\\')
		{
			*p = '/';
		}
	}

	if (path[strlen(path) - 1] == '/')
	{
		path[strlen(path) - 1] = '\0';
	}

	return path;
}

char *windowsify_path(char *path)
{
	char *p;

	for (p = path; *p; p++)
	{
		if (*p == '/')
		{
			*p = '\\';
		}
	}

	if (path[strlen(path) - 1] == '\\')
	{
		path[strlen(path) - 1] = '\0';
	}

	return path;
}

char *systemify_path(char *path)
{
#ifdef _WIN32
	return windowsify_path(path);
#else
	return unixify_path(path);
#endif
}

int mkdir_w(const char *path)
{
#ifdef _WIN32
	return _mkdir(path);
#else
	return mkdir(path, S_IRWXU);
#endif
}

int mkdir_p(const char *path)
{
	// Adapted from https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950
	// Adapted from https://stackoverflow.com/a/2336245/119527

	char _path[PATH_MAX_WIN];
	char *p;

	errno = 0;

	if (strlen(path) + 1 > PATH_MAX_WIN)
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	// Copy string so its mutable
	strcpy(_path, unixify_path((char *)path));

	// Iterate the string
	for (p = _path + 1; *p; p++)
	{
		if (*p == '/')
		{
			// Temporarily truncate
			*p = '\0';

			if (mkdir_w(_path) != 0)
			{
				// Don't throw error if it's a drive
				if (errno != EEXIST && (errno == EACCES && !strchr(_path, ':')))
				{
					return -1;
				}
			}

			*p = '/';
		}
	}

	if (mkdir_w(_path) != 0)
	{
		if (errno != EEXIST)
		{
			return -1;
		}
	}

	return 0;
}

void mkdir_d(char *path)
{
	if (mkdir_p(path))
	{
		printf("Error: Couldn't create directory %s, exiting\n", systemify_path(path));
		exit(1);
	}
}
