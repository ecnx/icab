/*
 --------------------------------------------------------------------------------------
                        iCAB - List / Unpack CAB Archive Utility
 --------------------------------------------------------------------------------------
 */

#include "icab.h"

/* Dump cabinet header details */
static void dump_header ( const struct CFHEADER *header )
{
    printf ( "cabinet header dump\n" );
    printf ( " |-signature: %.2x %.2x %.2x %.2x\n",
        header->signature[0], header->signature[1], header->signature[2], header->signature[3] );
    printf ( " |-total size: %u\n", header->cbCabinet );
    printf ( " |-version major: %u\n", header->versionMajor );
    printf ( " |-version minor: %u\n", header->versionMinor );
    printf ( " |-folders count: %u\n", header->cFolders );
    printf ( " |-files count: %u\n", header->cFiles );
    printf ( " |-flags: 0x%.4x\n", header->flags );
    printf ( " |-set id: 0x%.4x\n", header->setID );
    printf ( " \\-seq number: 0x%.4x\n\n", header->iCabinet );
}

/* Dump cabinet folder details */
static void dump_folder ( const struct CFFOLDER *folder, unsigned short n, int lastone )
{
    const char *s_compression;
    printf ( " %c-folder: %5u", ( lastone ? '\\' : '|' ), n );
    printf ( " offset: %11u", folder->coffCabStart );
    printf ( " sectors: %5u", folder->cCFData );
    switch ( folder->typeCompress & 0x000F )
    {
    case 0:
        s_compression = "none";
        break;
    case 1:
        s_compression = "ms-zip";
        break;
    case 2:
        s_compression = "quantum";
        break;
    case 3:
        s_compression = "lzx";
        break;
    default:
        printf ( " compression: unknown (0x%.4x)\n", folder->typeCompress );
        return;
    }
    printf ( " compression: %s\n", s_compression );
}

/* Dump cabinet file details */
static void dump_file ( const struct CFFILE *file, int lastone )
{
    char s_attribs[7] = { 'r', 'h', 's', 'a', 'e', 'u', '\0' };

    printf ( " %c-size: %11u", ( lastone ? '\\' : '|' ), file->cbFile );
    printf ( " offset: %11u", file->uoffFolderStart );
    printf ( " folder: %5u", file->iFolder );

    printf ( " %.4u-%.2u-%.2u",
        ( file->date >> 9 ) + 1980, ( file->date >> 5 ) & 0xF, file->date & 0x1F );
    printf ( " %.2u:%.2u:%.2u",
        file->time >> 11, ( file->time >> 5 ) & 0x3F, ( file->time & 0x1F ) << 1 );

    if ( ~file->attribs & _BV ( 6 ) )
    {
        s_attribs[0] = '-';
    }

    if ( ~file->attribs & _BV ( 5 ) )
    {
        s_attribs[1] = '-';
    }

    if ( ~file->attribs & _BV ( 4 ) )
    {
        s_attribs[2] = '-';
    }

    if ( ~file->attribs & _BV ( 1 ) )
    {
        s_attribs[3] = '-';
    }

    if ( ~file->attribs & _BV ( 0 ) )
    {
        s_attribs[4] = '-';
    }

    if ( ~file->attribs & _BV ( 15 ) )
    {
        s_attribs[5] = '-';
    }

    printf ( " (%s)", s_attribs );
}

/* Dump file name */
static size_t dump_file_name ( const unsigned char *offset, const unsigned char *limit )
{
    const unsigned char *finish = offset;

    while ( finish < limit && *finish != '\0' )
    {
        finish++;
    }

    printf ( " %s\n", ( const char * ) offset );

    return finish - offset + 1;
}

/* Dump data structure */
static size_t dump_data ( const struct CFDATA *data, int sector, int lastone )
{
    printf ( " | %c-sector: %5u", ( lastone ? '\\' : '|' ), sector );
    printf ( " csum: 0x%.8x", data->csum );
    printf ( " compressed size: %11u", data->cbData );
    printf ( " uncompressed size: %11u\n", data->cbUncomp );

    if ( lastone )
    {
        printf ( " |\n" );
    }

    return data->cbData;
}

/* List cabinet file entries */
static int list_files ( const unsigned char *base, size_t size )
{
    unsigned short i;
    unsigned short j;
    const struct CFHEADER *header;
    const struct CFFOLDER *folder;
    const unsigned char *offset;
    const unsigned char *sector;

    /* Assign header structure pointer */
    header = ( const struct CFHEADER * ) base;
    PTR_ASSERT ( header, sizeof ( struct CFHEADER ), base, size );
    offset = base + sizeof ( struct CFHEADER );

    /* Dump header structure */
    dump_header ( header );

    /* Show folder dump header */
    printf ( "cabinet folder dump\n" );

    /* Dump folders structures */
    for ( i = 0; i < header->cFolders; i++ )
    {
        /* Assign folder structure pointer */
        PTR_ASSERT ( offset, sizeof ( struct CFFOLDER ), base, size );
        folder = ( const struct CFFOLDER * ) offset;

        /* Dump folder structure */
        dump_folder ( folder, i, i + 1 == header->cFolders );
        offset += sizeof ( struct CFFOLDER );

        /* Assign sector strcuture pointer */
        sector = base + folder->coffCabStart;

        /* Dump data structures */
        for ( j = 0; j < folder->cCFData; j++ )
        {
            PTR_ASSERT ( sector, sizeof ( struct CFDATA ), base, size );
            sector += dump_data ( ( const struct CFDATA * ) sector, j, j + 1 == folder->cCFData );
            sector += sizeof ( struct CFDATA );
        }
    }

    /* Show file dump header */
    printf ( "\ncabinet file dump\n" );

    /* Dump all file structures */
    for ( i = 0; i < header->cFiles; i++ )
    {
        /* Assign file structure pointer */
        PTR_ASSERT ( header, sizeof ( struct CFFILE ), base, size );

        /* Dump file structure */
        dump_file ( ( const struct CFFILE * ) offset, i + 1 == header->cFiles );
        offset += sizeof ( struct CFFILE );

        /* Dump file name */
        offset += dump_file_name ( offset, base + size );
    }

    /* Print separator */
    putchar ( '\n' );

    return 0;
}

/* Uncompress data */
static int uncompress_data ( int type, const unsigned char *compressed, size_t compressed_size,
    struct cfdata_ctx *sector, z_stream * stream )
{
    int error_status;

    /* Supported compression types are none and ms-zip */
    if ( !( type & 0x000F ) )
    {
        /* On none compression type copy data */
        if ( compressed_size > sector->uncompressed_size )
        {
            return ENOBUFS;
        }
        memcpy ( sector->uncompressed, compressed, compressed_size );

    } else if ( ( type & 0x000F ) != 1 )
    {
        return ENOTSUP;
    }

    /* Validate compressed data length */
    if ( compressed_size < 2 )
    {
        return ENODATA;
    }

    /* Validate ms-zip header */
    if ( compressed[0] != 0x43 || compressed[1] != 0x4b )
    {
        return EINVAL;
    }

    /* Prepare decompression parameters */
    stream->avail_in = compressed_size - 2;
    stream->next_in = ( unsigned char * ) compressed + 2;

    stream->avail_out = sector->uncompressed_size;
    stream->next_out = sector->uncompressed;

    /* Decompress data with RFC 1951 inflate */
    error_status = inflate ( stream, Z_FINISH );
    if ( error_status != Z_OK && error_status != Z_STREAM_END )
    {
        fprintf ( stderr, "Failed to inflate data: %s\n", stream->msg );
        return error_status;
    }

    if ( stream->total_out != sector->uncompressed_size )
    {
        return ENODATA;
    }

    return 0;
}

/* Calculate file name length */
static size_t file_name_len ( const unsigned char *offset, const unsigned char *limit )
{
    const unsigned char *finish = offset;

    while ( finish < limit && *finish != '\0' )
    {
        finish++;
    }

    return finish - offset + 1;
}

/* Unpack single file */
static int unpack_file ( const struct cffolder_ctx *folder_ctx, const struct CFFILE *file,
    const unsigned char *limit, size_t * suboffset, const char *prefix,
    const struct progress_t *progress )
{
    int fd;
    int stop = FALSE;
    size_t i;
    size_t isum = 0;
    size_t osum = 0;
    size_t offset;
    size_t ilen;
    size_t olen;
    const char *filename;
    char path[2048];

    /* Calculate suboffset value */
    *suboffset =
        sizeof ( struct CFFILE ) + file_name_len ( ( const unsigned char * ) file +
        sizeof ( struct CFFILE ), limit );

    /* Validate data range */
    if ( ( const unsigned char * ) file + *suboffset >= limit )
    {
        return ERANGE;
    }

    /* Assign filename pointer */
    filename = ( const char * ) file + sizeof ( struct CFFILE );

    /* Prepare unpack path */
    snprintf ( path, sizeof ( path ), "%s/%s", prefix, filename );

    /* Open file for writing */
    if ( ( fd = open ( path, O_CREAT | O_WRONLY | O_TRUNC, 0644 ) ) < 0 )
    {
        return errno;
    }

    /* Extract data from sectors matching file range */
    for ( i = 0; i < folder_ctx->n_sectors; i++ )
    {
        printf ( "\rDone %.2u%% file %u of %u\r",
            ( 100 * ( progress->file + 1 ) / progress->n_files ), ( progress->file + 1 ),
            progress->n_files );

        if ( isum + folder_ctx->sectors[i].uncompressed_size < file->uoffFolderStart )
        {
            isum += folder_ctx->sectors[i].uncompressed_size;
            continue;
        }

        if ( file->uoffFolderStart > isum )
        {
            offset = file->uoffFolderStart - isum;
        } else
        {
            offset = 0;
        }

        if ( offset + file->cbFile - osum > folder_ctx->sectors[i].uncompressed_size )
        {
            ilen = folder_ctx->sectors[i].uncompressed_size - offset;
            isum += folder_ctx->sectors[i].uncompressed_size;
        } else
        {
            ilen = file->cbFile - osum;
            stop = TRUE;
        }

        if ( ( ssize_t ) ( olen =
                write ( fd, folder_ctx->sectors[i].uncompressed + offset, ilen ) ) < 0 )
        {
            close ( fd );
            return errno;
        }

        if ( stop )
        {
            break;
        }

        osum += olen;
    }

    /* Free output file fd */
    close ( fd );

    return 0;
}

#define GetUi32(p) ( \
             ((const Byte *)(p))[0]        | \
    ((unsigned int)((const Byte *)(p))[1] <<  8) | \
    ((unsigned int)((const Byte *)(p))[2] << 16) | \
((unsigned int)((const Byte *)(p))[3] << 24))


/* Calculate cfdata checksum */
static unsigned int checksum ( const unsigned char *p, unsigned int size )
{
    unsigned int sum = 0;

    for ( ; size >= 8; size -= 8 )
    {
        sum ^= GetUi32 ( p ) ^ GetUi32 ( p + 4 );
        p += 8;
    }

    if ( size >= 4 )
    {
        sum ^= GetUi32 ( p );
        p += 4;
    }

    size &= 3;
    if ( size > 2 )
        sum ^= ( unsigned int ) ( *p++ ) << 16;
    if ( size > 1 )
        sum ^= ( unsigned int ) ( *p++ ) << 8;
    if ( size > 0 )
        sum ^= ( unsigned int ) ( *p++ );

    return sum;
}

/* Load and uncompress folder sectors */
static int uncompress_folder ( size_t nfolder, const struct CFFOLDER *folder,
    struct cffolder_ctx *folder_ctx, const unsigned char *base, size_t size, const char *prefix )
{
    int error_status = 0;
    unsigned short i;
    const struct CFHEADER *header;
    const struct CFFILE *file;
    const struct CFDATA *sector;
    const unsigned char *offset;
    size_t suboffset;
    z_stream stream;
    int stream_freed = TRUE;
    struct progress_t progress;

    /* Assign header structure pointer */
    header = ( const struct CFHEADER * ) base;
    PTR_ASSERT ( header, sizeof ( struct CFHEADER ), base, size );

    /* Allocate sectors table */
    if ( ( folder_ctx->sectors =
            ( struct cfdata_ctx * ) malloc ( folder->cCFData * sizeof ( struct cfdata_ctx ) ) ) ==
        NULL )
    {
        return ENOMEM;
    }

    /* Reset each sector content pointer */
    for ( i = 0; i < folder->cCFData; i++ )
    {
        folder_ctx->sectors[i].uncompressed = NULL;
    }

    /* Assign sector strcuture pointer */
    sector = ( const struct CFDATA * ) ( base + folder->coffCabStart );

    /* Prepare zlib inflate stream */
    memset ( &stream, '\0', sizeof ( stream ) );
    stream.zalloc = ( alloc_func ) NULL;
    stream.zfree = ( free_func ) NULL;
    stream.opaque = ( voidpf ) NULL;

    /* Initialize inflate stream for raw data */
    if ( ( error_status = inflateInit2 ( &stream, -15 ) ) != Z_OK )
    {
        return error_status;
    }

    stream_freed = FALSE;

    /* Load content each sector */
    for ( i = 0; i < folder->cCFData; i++ )
    {
        /* Assert sector structure pointer */
        PTR_ASSERT ( sector, sizeof ( struct CFDATA ), base, size );

        /* Allocate sector buffer */
        if ( ( folder_ctx->sectors[i].uncompressed =
                ( unsigned char * ) malloc ( sector->cbUncomp ) ) == NULL )
        {
            error_status = ENOMEM;
            goto exit;
        }

        /* Update sector buffer size */
        folder_ctx->sectors[i].uncompressed_size = sector->cbUncomp;

        /* Reset inflate stream and apply dictionary if needed */
        if ( i )
        {
            if ( ( error_status = inflateReset ( &stream ) ) != Z_OK )
            {
                goto exit;
            }

            if ( ( error_status =
                    inflateSetDictionary ( &stream, folder_ctx->sectors[i - 1].uncompressed,
                        folder_ctx->sectors[i - 1].uncompressed_size ) ) != Z_OK )
            {
                goto exit;
            }
        }

        /* Uncompress data into sector buffer */
        if ( ( error_status =
                uncompress_data ( folder->typeCompress,
                    ( const unsigned char * ) sector + sizeof ( struct CFDATA ), sector->cbData,
                    &folder_ctx->sectors[i], &stream ) ) != 0 )
        {
            goto exit;
        }

        /* Verify checksum if not set to zero */
        if ( sector->csum )
        {
            if ( sector->csum !=
                checksum ( ( const unsigned char * ) sector + sizeof ( struct CFDATA ) -
                    sizeof ( unsigned int ), sector->cbData + sizeof ( unsigned int ) ) )
            {
                printf ( "! checksum is invalid at sector #%u\n", i );
            }
        }

        /* Load next sector offset */
        sector =
            ( const struct CFDATA * ) ( ( const unsigned char * ) sector +
            sizeof ( struct CFDATA ) + sector->cbData );
    }

    /* Update sectors count */
    folder_ctx->n_sectors = folder->cCFData;

    /* Free zlib inflate stream */
    inflateEnd ( &stream );
    stream_freed = TRUE;

    /* Calculate file table offset */
    offset = base + sizeof ( struct CFHEADER ) + header->cFolders * sizeof ( struct CFFOLDER );
    if ( offset >= base + size )
    {
        return ERANGE;
    }

    /* Prepare progress structure */
    progress.n_files = header->cFiles;

    /* Extract each file from folder */
    for ( i = 0; i < header->cFiles; i++ )
    {
        /* Assign file structure pointer */
        file = ( const struct CFFILE * ) offset;
        PTR_ASSERT ( file, sizeof ( struct CFFILE ), base, size );

        /* Skip files of other folders */
        if ( file->iFolder != nfolder )
        {
            offset +=
                sizeof ( struct CFFILE ) + file_name_len ( offset + sizeof ( struct CFFILE ),
                base + size );
            continue;
        }

        /* Update progress structure */
        progress.file = i;

        /* Uncompress single file */
        if ( ( error_status =
                unpack_file ( folder_ctx, file, base + size, &suboffset, prefix,
                    &progress ) ) != 0 )
        {
            goto exit;
        }

        offset += suboffset;
    }

  exit:

    /* Free zlib inflate stream if not done before */
    if ( !stream_freed )
    {
        inflateEnd ( &stream );
    }

    /* Free buffer of each sector */
    for ( i = 0; i < folder->cCFData; i++ )
    {
        if ( folder_ctx->sectors[i].uncompressed != NULL )
        {
            free ( folder_ctx->sectors[i].uncompressed );
        }
    }

    /* Free sectors table */
    free ( folder_ctx->sectors );

    return error_status;
}

/* Unpack files to directory */
static int unpack_files ( const unsigned char *base, size_t size, const char *prefix )
{
    int error_status = 0;
    unsigned short i;
    const struct CFHEADER *header;
    const struct CFFOLDER *folder;
    const unsigned char *offset;
    struct cffolder_ctx *folders;
    struct stat statbuf;

    /* Create directory if not exists */
    if ( stat ( prefix, &statbuf ) < 0 && errno == ENOENT )
    {
        mkdir ( prefix, 0755 );
    }

    /* Assign header structure pointer */
    header = ( const struct CFHEADER * ) base;
    PTR_ASSERT ( header, sizeof ( struct CFHEADER ), base, size );
    offset = base + sizeof ( struct CFHEADER );

    /* Verify header signature */
    if ( header->signature[0] != 0x4d || header->signature[1] != 0x53
        || header->signature[2] != 0x43 || header->signature[3] != 0x46 )
    {
        fprintf ( stderr, "Error: Header signature is invalid\n" );
        return EINVAL;
    }

    /* Show folders count */
    printf ( "Folders count: %5u\n", header->cFolders );

    /* Allocate folders table */
    if ( ( folders =
            ( struct cffolder_ctx * ) malloc ( header->cFolders *
                sizeof ( struct cffolder_ctx ) ) ) == NULL )
    {
        fprintf ( stderr, "Failed to allocate folders table: %i\n", ENOMEM );
        return ENOMEM;
    }

    /* Uncompress each folder */
    for ( i = 0; i < header->cFolders; i++ )
    {
        /* Assign folder structure pointer */
        folder = ( const struct CFFOLDER * ) offset;
        if ( offset + sizeof ( struct CFFOLDER ) >= base + size )
        {
            error_status = ERANGE;
            goto exit;
        }

        /* Load and uncompress folder sectors */
        if ( ( error_status =
                uncompress_folder ( i, folder, &folders[i], base, size, prefix ) ) != 0 )
        {
            fprintf ( stderr, "Failed to uncompress folder: %i\n", error_status );
            goto exit;
        }

        offset += sizeof ( struct CFFOLDER );
    }

  exit:

    /* Print separator line */
    putchar ( '\n' );

    /* Free folders table */
    free ( folders );

    return error_status;
}

/* Show program usage */
static void show_usage ( void )
{
    printf ( "icab-unpack -lu file dest\n" );
}

/* Unpack utility main function */
int main ( int argc, char *argv[] )
{
    int error_status = 0;
    int fd = -1;
    struct stat statbuf;
    void *data = NULL;
    int action;

    /* Show program logo */
    printf ( "CAB unpack - ver. " ICAB_VERSION "\n" );

    /* Reset file stats size */
    statbuf.st_size = 0;

    /* Validate arguments count */
    if ( argc < 3 )
    {
        show_usage (  );
        return 1;
    }

    /* Select operation type */
    if ( !strcmp ( argv[1], "-l" ) )
    {
        action = ACTION_LISTONLY;

    } else if ( !strcmp ( argv[1], "-u" ) )
    {
        action = ACTION_UNPACK;

    } else
    {
        show_usage (  );
        return 1;
    }

    /* Validate arguments count again if neded */
    if ( action == ACTION_UNPACK && argc < 4 )
    {
        show_usage (  );
        return 1;
    }

    /* Open input file */
    if ( ( fd = open ( argv[2], O_RDONLY ) ) < 0 )
    {
        fprintf ( stderr, "Failed to open cabinet file: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    /* Obtain file length */
    if ( fstat ( fd, &statbuf ) < 0 )
    {
        fprintf ( stderr, "Failed to get file size: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    /* Map memory for file content */
    if ( ( data =
            mmap ( NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0 ) ) == NULL )
    {
        fprintf ( stderr, "Failed to map memory: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    /* Close input file fd (no longer needed) */
    close ( fd );
    fd = -1;

    /* Perform selected action */
    if ( action == ACTION_LISTONLY )
    {
        /* List files */
        if ( ( error_status = list_files ( ( unsigned char * ) data, statbuf.st_size ) ) != 0 )
        {
            fprintf ( stderr, "Failed to list files: %i\n", error_status );
            goto exit;
        }

    } else if ( action == ACTION_UNPACK )
    {
        /* Unpack files */
        if ( ( error_status =
                unpack_files ( ( unsigned char * ) data, statbuf.st_size, argv[3] ) ) != 0 )
        {
            fprintf ( stderr, "Failed to unpack files: %i\n", error_status );
            goto exit;
        }

    } else
    {
        show_usage (  );
        return 1;
    }

  exit:

    /* Free mapped memory */
    if ( data != NULL )
    {
        munmap ( data, statbuf.st_size );
    }

    /* Close input file fd */
    if ( fd != -1 )
    {
        close ( fd );
    }

    printf ( "Exit status: %i\n", error_status );

    return error_status;
}
