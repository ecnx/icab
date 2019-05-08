/*
 --------------------------------------------------------------------------
                        iCAB - Project Shared Header
 --------------------------------------------------------------------------
 */

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#include <sys/time.h>

#ifndef ICAB_H
#define ICAB_H

#define ACTION_LISTONLY 1
#define ACTION_UNPACK   2

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define _BV(n) (1<<n)

#define ICAB_VERSION "2.0.01"

#define PTR_ASSERT(p,n,b,s) \
    if ((unsigned char*) p + n >= (unsigned char*) b + s) { \
        return ERANGE; \
    } \

/* Cabinet header structure */
struct CFHEADER
{
    unsigned char signature[4];
    unsigned char reserved1[4];
    unsigned int cbCabinet;
    unsigned char reserved2[4];

    unsigned int coffFiles;
    unsigned char reserved3[4];

    unsigned char versionMinor;
    unsigned char versionMajor;

    unsigned short cFolders;
    unsigned short cFiles;

    unsigned short flags;
    unsigned short setID;

    unsigned short iCabinet;
};

/* Cabinet folder structure */
struct CFFOLDER
{
    unsigned int coffCabStart;
    unsigned short cCFData;
    unsigned short typeCompress;
};

/* Cabinet file structure */
struct CFFILE
{
    unsigned int cbFile;
    unsigned int uoffFolderStart;
    unsigned short iFolder;
    unsigned short date;
    unsigned short time;
    unsigned short attribs;
};

/* Cabinet file structure with filename */
struct CFFILE_FN
{
    unsigned int cbFile;
    unsigned int uoffFolderStart;
    unsigned short iFolder;
    unsigned short date;
    unsigned short time;
    unsigned short attribs;
    const char *filename;
};

/* Cabinet data strcuture */
struct CFDATA
{
    unsigned int csum;
    unsigned short cbData;
    unsigned short cbUncomp;
};

/* Cabinet data managment context */
struct cfdata_ctx
{
    unsigned char *uncompressed;
    size_t uncompressed_size;
};

/* Cabinet folder managment context */
struct cffolder_ctx
{
    struct cfdata_ctx *sectors;
    size_t n_sectors;
};

/* Unpack progress info */
struct progress_t
{
    unsigned short file;
    unsigned short n_files;
};

/* Cabinet folder data context */
struct folder_mem_ctx
{
    unsigned short n_cfdata;
    unsigned char *compressed;
    size_t compressed_size;
};

#endif
