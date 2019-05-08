/*
 -------------------------------------------------------------------------------------
                            iCAB - Pack CAB Archive Utility
 -------------------------------------------------------------------------------------
 */

#include "icab.h"

/* Show program usage */
static void show_usage ( void )
{
    printf ( "icab-pack schema 0..9 output.cab\n" );
}

/* Obtain files count from folders schema */
unsigned short get_files_count ( const char *schema )
{
    unsigned short n;

    for ( n = 0; ( schema = strchr ( schema, '\n' ) ) != NULL; schema++ )
    {
        n++;
    }

    return n;
}

/* Obtain folders count from folders schema */
unsigned short get_folders_count ( const char *schema )
{
    unsigned int value = 0;
    unsigned int max;

    for ( max = 0; *schema != '\0'; schema++ )
    {
        if ( sscanf ( schema, "%u", &value ) <= 0 )
        {
            break;
        }

        if ( value > max )
        {
            max = value;
        }

        if ( ( schema = strchr ( schema, '\n' ) ) == NULL )
        {
            break;
        }
    }

    return ( unsigned short ) ( max + 1 );
}

/* Obtain folder statistics */
int get_folder_stats ( const char *schema, unsigned short folder, unsigned short *n_files,
    size_t * uncompressed_size )
{
    unsigned int chkfolder = 0;
    size_t copy_len;
    const char *separator;
    struct stat statbuf;
    char path[2048];

    for ( *n_files = 0, *uncompressed_size = 0; *schema != '\0'; schema++ )
    {
        if ( sscanf ( schema, "%u", &chkfolder ) <= 0 )
        {
            return EINVAL;
        }

        if ( chkfolder != folder )
        {
            if ( ( schema = strchr ( schema, '\n' ) ) == NULL )
            {
                break;
            }
            continue;
        }

        *n_files += 1;

        if ( ( schema = strchr ( schema, ',' ) ) == NULL )
        {
            return ESRCH;
        }

        schema++;

        if ( ( separator = strchr ( schema, '\n' ) ) == NULL )
        {
            separator = schema + strlen ( schema );
        }

        if ( ( copy_len = separator - schema ) >= sizeof ( path ) )
        {
            return ENOBUFS;
        }

        memcpy ( path, schema, copy_len );
        path[copy_len] = '\0';

        if ( stat ( path, &statbuf ) < 0 )
        {
            return errno;
        }

        if ( statbuf.st_size < 0 )
        {
            return EINVAL;
        }

        *uncompressed_size += statbuf.st_size;

        if ( *separator == '\0' )
        {
            break;
        }

        schema = separator;
    }

    return 0;
}

/* Skip slashes in file name */
static void skip_slahses ( const char **filename )
{
    const char *slash_ptr;
    const char *end_ptr;

  again:

    if ( ( slash_ptr = strchr ( *filename, '/' ) ) == NULL )
    {
        return;
    }

    if ( ( end_ptr = strchr ( *filename, '\n' ) ) == NULL )
    {
        end_ptr = *filename + strlen ( *filename );
    }

    if ( slash_ptr < end_ptr )
    {
        *filename = slash_ptr + 1;
        goto again;
    }
}

/* Load signle file content */
static int load_file ( const char *schema, unsigned short folder, unsigned char *uncompressed,
    size_t uncompressed_size, struct CFFILE_FN *file, size_t * schema_off )
{
    int error_status = 0;
    int fd;
    size_t copy_len;
    unsigned int chkfolder;
    struct stat statbuf;
    const char *base = schema;
    const char *separator = NULL;
    char path[2048];

    for ( chkfolder = 0; *schema != '\0'; schema++ )
    {
        if ( sscanf ( schema, "%u", &chkfolder ) <= 0 )
        {
            return EINVAL;
        }

        if ( chkfolder != folder )
        {
            if ( ( schema = strchr ( schema, '\n' ) ) == NULL )
            {
                break;
            }
            continue;
        }

        if ( ( schema = strchr ( schema, ',' ) ) == NULL )
        {
            return ESRCH;
        }

        schema++;

        if ( ( separator = strchr ( schema, '\n' ) ) == NULL )
        {
            separator = schema + strlen ( schema );
        }

        if ( ( copy_len = separator - schema ) >= sizeof ( path ) )
        {
            return ENOBUFS;
        }

        memcpy ( path, schema, copy_len );
        path[copy_len] = '\0';

        file->filename = schema;
        skip_slahses ( &file->filename );

        break;
    }

    if ( separator == NULL )
    {
        return ESRCH;
    }

    *schema_off = separator - base + 1;

    if ( ( fd = open ( path, O_RDONLY ) ) < 0 )
    {
        fprintf ( stderr, "Failed to open file %s: %i\n", path, errno );
        return errno;
    }

    if ( fstat ( fd, &statbuf ) < 0 )
    {
        error_status = errno;
        goto exit;
    }

    if ( statbuf.st_size < 0 )
    {
        return EINVAL;
    }

    if ( ( size_t ) statbuf.st_size > uncompressed_size )
    {
        error_status = ENOBUFS;
        goto exit;
    }

    file->cbFile = statbuf.st_size;

    if ( read ( fd, uncompressed, statbuf.st_size ) != statbuf.st_size )
    {
        return errno ? errno : EIO;
    }

  exit:

    close ( fd );

    return error_status;
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

/* Pack files of single folder into cabinet archive */
static int pack_folder ( const char *schema, unsigned short nfolder, struct CFFILE_FN *files,
    size_t n_files, size_t files_off, size_t uncompressed_size, struct folder_mem_ctx *folder_mem,
    unsigned int level )
{
    int error_status = 0;
    int z_status;
    size_t i;
    size_t osum;
    size_t schema_off;
    size_t uncompressed_off = 0;
    size_t length;
    unsigned char *uncompressed = NULL;
    z_stream stream;
    struct CFDATA *cfdata;
    int stream_freed = TRUE;

    /* Reset folder memory context */
    folder_mem->n_cfdata = 0;
    folder_mem->compressed = NULL;

    /* Calulcate maximal compressed data length */
    folder_mem->compressed_size =
        uncompressed_size + n_files * ( sizeof ( struct CFDATA ) + 2 ) + 32768;

    /* Allocate uncompressed data buffer */
    if ( ( uncompressed = ( unsigned char * ) malloc ( uncompressed_size ) ) == NULL )
    {
        error_status = ENOMEM;
        goto exit;
    }

    /* Allocate compressed data buffer */
    if ( ( folder_mem->compressed =
            ( unsigned char * ) malloc ( folder_mem->compressed_size ) ) == NULL )
    {
        error_status = ENOMEM;
        goto exit;
    }

    /* Load each file into uncompressed buffer */
    for ( i = 0; i < n_files; i++ )
    {
        /* Prepare file structure */
        files[files_off + i].uoffFolderStart = uncompressed_off;
        files[files_off + i].iFolder = nfolder;
        files[files_off + i].date = 0;
        files[files_off + i].time = 0;
        files[files_off + i].attribs = 0;
        files[files_off + i].filename = NULL;

        /* Load file name and content */
        if ( ( error_status = load_file ( schema, nfolder, uncompressed + uncompressed_off,
                    uncompressed_size - uncompressed_off, files + files_off + i,
                    &schema_off ) ) != 0 )
        {
            goto exit;
        }

        /* Update uncompressed data offset */
        uncompressed_off += files[files_off + i].cbFile;

        /* Update schema offset */
        schema += schema_off;
    }

    /* Prepare zlib deflate stream */
    memset ( &stream, '\0', sizeof ( stream ) );
    stream.zalloc = ( alloc_func ) NULL;
    stream.zfree = ( free_func ) NULL;
    stream.opaque = ( voidpf ) NULL;

    /* Initialize deflate stream for raw data */
    if ( ( error_status =
            deflateInit2 ( &stream, level, Z_DEFLATED, -15, MAX_MEM_LEVEL,
                Z_DEFAULT_STRATEGY ) ) != Z_OK )
    {
        goto exit;
    }

    /* Compress files data with deflate stream */
    for ( i = 0, osum = 0; i < uncompressed_off;
        i += length, osum += stream.total_out, folder_mem->n_cfdata += 1 )
    {
        /* Assign sector structure pointer */
        cfdata = ( struct CFDATA * ) ( folder_mem->compressed + osum );
        osum += sizeof ( struct CFDATA );

        /* Place ms-zip header */
        folder_mem->compressed[osum++] = 0x43;
        folder_mem->compressed[osum++] = 0x4b;

        /* Allow 32768 bytes max */
        if ( ( length = uncompressed_off - i ) > 32768 )
        {
            length = 32768;
        }

        /* Reset deflate stream and apply dictionary if needed */
        if ( i )
        {
            if ( ( error_status = deflateReset ( &stream ) ) != Z_OK )
            {
                goto exit;
            }

            if ( ( error_status =
                    deflateSetDictionary ( &stream, uncompressed + i - 32768, 32768 ) ) != Z_OK )
            {
                goto exit;
            }
        }

        /* Prepare compression parameters */
        stream.next_out = folder_mem->compressed + osum;
        stream.avail_out = folder_mem->compressed_size - osum;
        stream.next_in = uncompressed + i;
        stream.avail_in = length;

        /* Ccompress data with RFC 1951 deflate */
        z_status = deflate ( &stream, Z_FINISH );
        if ( z_status != Z_OK && z_status != Z_STREAM_END )
        {
            error_status = z_status;
            goto exit;
        }

        /* Update sectore structure */
        cfdata->cbData = 2 + stream.total_out;
        cfdata->cbUncomp = length;
        cfdata->csum =
            checksum ( folder_mem->compressed + osum - sizeof ( unsigned int ) - 2,
            stream.total_out + sizeof ( unsigned int ) + 2 );
    }

    /* Free zlib deflate stream */
    deflateEnd ( &stream );
    stream_freed = TRUE;


    /* Update compressed block size */
    folder_mem->compressed_size = osum;

  exit:

    /* Free zlib deflate stream if not done before */
    if ( !stream_freed )
    {
        deflateEnd ( &stream );
    }

    /* Free uncompressed data buffer */
    if ( uncompressed != NULL )
    {
        free ( uncompressed );
    }

    /* Free compressed data buffer on error */
    if ( error_status && folder_mem->compressed != NULL )
    {
        free ( folder_mem->compressed );
        folder_mem->compressed = NULL;
    }

    return error_status;
}

/* Pack files into cabinet archive */
int pack_files ( const char *schema, unsigned int level, int fd )
{
    int error_status = 0;
    unsigned char nullchr = '\0';
    unsigned short f_files = 0;
    size_t i;
    size_t folders_len;
    size_t folders_mem_len;
    size_t files_len;
    size_t files_off = 0;
    size_t uncompressed_size;
    size_t filename_len;
    size_t cfdata_off;
    const char *end_ptr;
    struct CFHEADER header;
    struct CFFOLDER *folders = NULL;
    struct CFFILE_FN *files = NULL;
    struct timeval tv;
    struct folder_mem_ctx *folders_mem = NULL;

    /* Prepare header structure */
    memset ( &header, '\0', sizeof ( header ) );

    /* Obtain current time */
    memset ( &tv, '\0', sizeof ( tv ) );
    gettimeofday ( &tv, NULL );

    /* Obtain folders count */
    header.signature[0] = 0x4d;
    header.signature[1] = 0x53;
    header.signature[2] = 0x43;
    header.signature[3] = 0x46;
    header.versionMinor = 3;
    header.versionMajor = 1;
    header.flags = 0;
    header.cFolders = get_folders_count ( schema );
    header.cFiles = get_files_count ( schema );
    header.setID = tv.tv_usec;
    header.iCabinet = 0;

    /* Show folders count */
    printf ( "Folders: %5u\n", header.cFolders );

    /* Show files count */
    printf ( "Files: %5u\n", header.cFiles );

    /* Allocate folders table */
    folders_len = header.cFolders * sizeof ( struct CFFOLDER );
    if ( ( folders = ( struct CFFOLDER * ) malloc ( folders_len ) ) == NULL )
    {
        error_status = ENOMEM;
        goto exit;
    }

    /* Allocate folders table */
    files_len = header.cFiles * sizeof ( struct CFFILE_FN );
    if ( ( files = ( struct CFFILE_FN * ) malloc ( files_len ) ) == NULL )
    {
        error_status = ENOMEM;
        goto exit;
    }

    memset ( files, '\0', files_len );

    /* Allocate folders memory table */
    folders_mem_len = header.cFolders * sizeof ( struct folder_mem_ctx );
    if ( ( folders_mem = ( struct folder_mem_ctx * ) malloc ( folders_mem_len ) ) == NULL )
    {
        error_status = ENOMEM;
        goto exit;
    }

    memset ( folders_mem, '\0', folders_mem_len );

    /* Obtain folder stats and pack files */
    for ( i = 0; i < header.cFolders && f_files < header.cFiles; i++ )
    {
        if ( ( error_status = get_folder_stats ( schema, i, &f_files, &uncompressed_size ) ) != 0 )
        {
            goto exit;
        }

        if ( ( error_status =
                pack_folder ( schema, i, files, f_files, files_off, uncompressed_size,
                    &folders_mem[i], level ) ) != 0 )
        {
            goto exit;
        }

        printf ( "Packed folder %u/%u (%u files)\n", ( unsigned short ) i, header.cFolders,
            f_files );

        files_off += f_files;
    }

    /* Set cabinet header total size and files offset */
    header.cbCabinet = sizeof ( struct CFHEADER ) + header.cFolders * sizeof ( struct CFFOLDER );
    header.coffFiles = header.cbCabinet;

    /* Apply files structures length sum */
    for ( i = 0; i < header.cFiles; i++ )
    {
        if ( ( end_ptr = strchr ( files[i].filename, '\n' ) ) == NULL )
        {
            filename_len = strlen ( files[i].filename );
        } else
        {
            filename_len = end_ptr - files[i].filename;
        }

        header.cbCabinet += sizeof ( struct CFFILE ) + filename_len + 1;
    }

    /* Set cabinet sectors offset */
    cfdata_off = header.cbCabinet;

    /* Apply files content length sum */
    for ( i = 0; i < header.cFolders; i++ )
    {
        header.cbCabinet += folders_mem[i].compressed_size;
    }

    /* Set folders structures */
    for ( i = 0; i < header.cFolders; i++ )
    {
        /* Set folder offset, sectors count and compression type */
        folders[i].coffCabStart = cfdata_off;
        folders[i].cCFData = folders_mem[i].n_cfdata;
        folders[i].typeCompress = 1;    /* ms-zip */

        /* Skip sectors of current folder */
        cfdata_off += folders_mem[i].compressed_size;
    }

    /* Save header structure to file */
    if ( ( size_t ) write ( fd, &header, sizeof ( header ) ) != sizeof ( header ) )
    {
        error_status = errno ? errno : EIO;
        goto exit;
    }

    /* Save folders structures to file */
    if ( ( size_t ) write ( fd, folders, folders_len ) != folders_len )
    {
        error_status = errno ? errno : EIO;
        goto exit;
    }

    /* Save files structures and file names to file */
    for ( i = 0; i < header.cFiles; i++ )
    {
        if ( ( end_ptr = strchr ( files[i].filename, '\n' ) ) == NULL )
        {
            filename_len = strlen ( files[i].filename );

        } else
        {
            filename_len = end_ptr - files[i].filename;
        }

        if ( ( size_t ) write ( fd, ( struct CFFILE * ) &files[i],
                sizeof ( struct CFFILE ) ) != sizeof ( struct CFFILE ) )
        {
            error_status = errno ? errno : EIO;
            goto exit;
        }

        if ( ( size_t ) write ( fd, files[i].filename, filename_len ) != filename_len )
        {
            error_status = errno ? errno : EIO;
            goto exit;
        }

        if ( ( size_t ) write ( fd, &nullchr, 1 ) != 1 )
        {
            error_status = errno ? errno : EIO;
            goto exit;
        }
    }

    /* Save files content to file */
    for ( i = 0; i < header.cFolders; i++ )
    {
        if ( ( size_t ) write ( fd, folders_mem[i].compressed,
                folders_mem[i].compressed_size ) != folders_mem[i].compressed_size )
        {
            error_status = errno ? errno : EIO;
            goto exit;
        }
    }

  exit:

    /* Free folders table */
    if ( folders != NULL )
    {
        free ( folders );
    }

    /* Free files table */
    if ( files != NULL )
    {
        free ( files );
    }

    /* Return if buffer not allocated */
    if ( folders_mem == NULL )
    {
        return error_status;
    }

    /* Free folders memory table buffers */
    for ( i = 0; i < header.cFolders; i++ )
    {
        if ( folders_mem[i].compressed != NULL )
        {
            free ( folders_mem[i].compressed );
        }
    }

    /* Free folders memory table */
    free ( folders_mem );

    return error_status;
}

/* Load cabinet schema file content */
char *load_schema ( const char *path )
{
    int fd;
    struct stat statbuf;
    ssize_t len;
    char *content;

    if ( ( fd = open ( path, O_RDONLY ) ) < 0 )
    {
        return NULL;
    }

    if ( fstat ( fd, &statbuf ) < 0 )
    {
        return NULL;
    }

    if ( ( content = ( char * ) malloc ( statbuf.st_size + 1 ) ) == NULL )
    {
        close ( fd );
        errno = ENOMEM;
        return NULL;
    }

    if ( ( len = read ( fd, content, statbuf.st_size ) ) != statbuf.st_size )
    {
        free ( content );
        close ( fd );
        if ( !errno )
        {
            errno = EIO;
        }
        return NULL;
    }

    content[statbuf.st_size] = '\0';
    return content;
}

/* Pack utility main function */
int main ( int argc, char *argv[] )
{
    int error_status = 0;
    int fd = -1;
    unsigned int level = 0;
    char *schema = NULL;

    /* Show program logo */
    printf ( "CAB pack - ver. " ICAB_VERSION "\n" );

    /* Validate arguments count */
    if ( argc < 4 )
    {
        show_usage (  );
        return 1;
    }

    /* Parse compression level */
    if ( sscanf ( argv[2], "%u", &level ) <= 0 )
    {
        show_usage (  );
        return 1;
    }

    /* Validate compression level */
    if ( level > 9 )
    {
        show_usage (  );
        return 1;
    }

    /* Load folders schema */
    if ( ( schema = load_schema ( argv[1] ) ) == NULL )
    {
        fprintf ( stderr, "Failed to load schema: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    /* Open output file for writing */
    if ( ( fd = open ( argv[3], O_CREAT | O_WRONLY | O_TRUNC, 0644 ) ) < 0 )
    {
        fprintf ( stderr, "Failed to open output file: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    /* Pack files into archive */
    error_status = pack_files ( schema, level, fd );

  exit:

    /* Free folders schema */
    if ( schema != NULL )
    {
        free ( schema );
    }

    /* Close output file fd */
    if ( fd != -1 )
    {
        close ( fd );
    }

    printf ( "Exit status: %i\n", error_status );

    return error_status;
}
