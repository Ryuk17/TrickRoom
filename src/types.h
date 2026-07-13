#ifndef TYPES_H
#define TYPES_H

typedef enum
{
	CONNECTED,
	DISCONNECTED
} client_state_t;

typedef int socklen_t;

#pragma pack(push, 1)
typedef struct
{
	int size;
	int width;
	int height;
	short planes;
	short bpp;
	int compression;
	int image_size;
	int xres;
	int yres;
	int clr_used;
	int clr_important;
} dib_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	char type[2];
	int file_size;
	int reserved;
	int offset;
} bmpheader_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	bmpheader_t	header;
	dib_t		dib;
} bitmap_t;
#pragma pack(pop)

typedef struct
{
	unsigned int magic;
	unsigned int size;
} header_t;
#endif


