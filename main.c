#include "big4f.h"

int
main(int argc, char *argv[])
{
	// Check for first arg
	if(argc < 2)
	{
		printf_help_exit();
	}

	// Needs to be just one char
	if(strlen(argv[1]) != 1)
	{
		printf_help_exit();
	}

	printf("\n");

	switch(argv[1][0])
	{
	case 'l':
		// Not enough args
		if(argc < 3)
		{
			printf_help_exit();
		}

		// Exits on failure
		BIGFile_List(argv[2]);

		printf("\nSuccessfully listed %s\n", argv[2]);
		break;
	case 'e':
	case 'x':
		// Not enough args
		if(argc < 4)
		{
			printf_help_exit();
		}

		// Exits on failure
		BIGFile_Extract(argv[2], argv[3]);

		printf("\nSuccessfully extracted %s\n", argv[2]);
		break;
	case 'f':
	case '4':
		// Not enough args
		if(argc < 4)
		{
			printf_help_exit();
		}

		// Exits on failure
		BIGFile_Pack(argv[2], argv[3], (char)argv[1][0]);

		printf("\nSuccessfully packed %s\n", argv[3]);
		break;
	default:
		printf_help_exit();
		break;
	}

	return 0;
}
