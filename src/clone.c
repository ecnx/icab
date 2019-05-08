/*
 --------------------------------------------------------------------------------------
                            iCAB - Clone CAB Archive Utility
 --------------------------------------------------------------------------------------
 */

#include "icab.h"

/* Show program usage */
static void show_usage ( void )
{
    printf ( "icab-clone input.cab output.cab\n" );
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

/* Clone archive info */
static int icab_clone ( const unsigned char *imem, size_t ilen, unsigned char *omem, size_t olen )
{
    size_t i;
    size_t s_filename;
    size_t d_filename;
    const struct CFHEADER *s_header;
    struct CFHEADER *d_header;
    const struct CFFOLDER *s_folder;
    struct CFFOLDER *d_folder;
    const struct CFFILE *s_file;
    struct CFFILE *d_file;
    const unsigned char *s_offset;
    unsigned char *d_offset;
//    const unsigned char *sector;

    /* Assign source header structure pointer */
    s_header = ( const struct CFHEADER * ) imem;
    PTR_ASSERT ( s_header, sizeof ( struct CFHEADER ), imem, ilen );
    s_offset = imem + sizeof ( struct CFHEADER );

    /* Assign dest header structure pointer */
    d_header = ( struct CFHEADER * ) omem;
    PTR_ASSERT ( d_header, sizeof ( struct CFHEADER ), omem, olen );
    d_offset = omem + sizeof ( struct CFHEADER );

    /* Verify source header signature */
    if ( s_header->signature[0] != 0x4d || s_header->signature[1] != 0x53
        || s_header->signature[2] != 0x43 || s_header->signature[3] != 0x46 )
    {
        fprintf ( stderr, "Error: Input archive signature is invalid\n" );
        return EINVAL;
    }

    /* Verify source header signature */
    if ( d_header->signature[0] != 0x4d || d_header->signature[1] != 0x53
        || d_header->signature[2] != 0x43 || d_header->signature[3] != 0x46 )
    {
        fprintf ( stderr, "Error: Output archive signature is invalid\n" );
        return EINVAL;
    }

    printf ( "signatures check passed.\n" );

    /* Compare and clone headers if possible */
    if ( d_header->reserved1[0] != s_header->reserved1[0]
        || d_header->reserved1[1] != s_header->reserved1[1]
        || d_header->reserved1[2] != s_header->reserved1[2]
        || d_header->reserved1[3] != s_header->reserved1[3] )
    {
        printf ( "Warning: correcting reserved1 ... " );
        memcpy ( d_header->reserved1, s_header->reserved1, sizeof ( s_header->reserved1 ) );
        printf ( " ok\n" );
    }

    if ( d_header->reserved2[0] != s_header->reserved2[0]
        || d_header->reserved2[1] != s_header->reserved2[1]
        || d_header->reserved2[2] != s_header->reserved2[2]
        || d_header->reserved2[3] != s_header->reserved2[3] )
    {
        printf ( "Warning: correcting reserved2 ... " );
        memcpy ( d_header->reserved2, s_header->reserved2, sizeof ( s_header->reserved2 ) );
        printf ( " ok\n" );
    }

    if ( d_header->coffFiles != s_header->coffFiles )
    {
        fprintf ( stderr, "Error: coffFiles is different!\n" );
        return EINVAL;
    }

    if ( d_header->reserved3[0] != s_header->reserved3[0]
        || d_header->reserved3[1] != s_header->reserved3[1]
        || d_header->reserved3[2] != s_header->reserved3[2]
        || d_header->reserved3[3] != s_header->reserved3[3] )
    {
        printf ( "Warning: correcting reserved3 ... " );
        memcpy ( d_header->reserved3, s_header->reserved3, sizeof ( s_header->reserved3 ) );
        printf ( " ok\n" );
    }

    if ( d_header->versionMinor != s_header->versionMinor )
    {
        printf ( "Warning: correcting versionMinor ... " );
        d_header->versionMinor = s_header->versionMinor;
        printf ( " ok\n" );
    }

    if ( d_header->versionMajor != s_header->versionMajor )
    {
        printf ( "Warning: correcting versionMajor ... " );
        d_header->versionMajor = s_header->versionMajor;
        printf ( " ok\n" );
    }

    if ( d_header->cFolders != s_header->cFolders )
    {
        fprintf ( stderr, "Error: cFolders is different!\n" );
        return EINVAL;
    }

    if ( d_header->cFiles != s_header->cFiles )
    {
        fprintf ( stderr, "Error: cFiles is different!\n" );
        return EINVAL;
    }

    if ( d_header->flags != s_header->flags )
    {
        printf ( "Warning: correcting flags ... " );
        d_header->flags = s_header->flags;
        printf ( " ok\n" );
    }

    if ( d_header->setID != s_header->setID )
    {
        printf ( "Warning: correcting setID ... " );
        d_header->setID = s_header->setID;
        printf ( " ok\n" );
    }

    if ( d_header->iCabinet != s_header->iCabinet )
    {
        printf ( "Warning: correcting iCabinet ... " );
        d_header->iCabinet = s_header->iCabinet;
        printf ( " ok\n" );
    }

    printf ( "done checking header.\n" );

    /* Compare folder structures */
    for ( i = 0; i < s_header->cFolders; i++ )
    {
        /* Assign folder structures pointer */
        PTR_ASSERT ( s_offset, sizeof ( struct CFFOLDER ), imem, ilen );
        s_folder = ( const struct CFFOLDER * ) s_offset;
        PTR_ASSERT ( d_offset, sizeof ( struct CFFOLDER ), omem, olen );
        d_folder = ( struct CFFOLDER * ) d_offset;

        /* Compare folders structures */
        if ( d_folder->typeCompress != s_folder->typeCompress )
        {
            fprintf ( stderr, "Error: folders compression types differ!\n" );
            return EINVAL;
        }

        /* Seek to next folder structures */
        s_offset += sizeof ( struct CFFOLDER );
        d_offset += sizeof ( struct CFFOLDER );
    }

    printf ( "done checking folders.\n" );

    /* Compare files structures */
    for ( i = 0; i < s_header->cFiles; i++ )
    {
        /* Assign file structures pointer */
        PTR_ASSERT ( s_offset, sizeof ( struct CFFILE ), imem, ilen );
        s_file = ( const struct CFFILE * ) s_offset;
        PTR_ASSERT ( d_offset, sizeof ( struct CFFILE ), omem, olen );
        d_file = ( struct CFFILE * ) d_offset;

        if ( d_file->iFolder != s_file->iFolder )
        {
            fprintf ( stderr, "Error: files folder number differ!\n" );
            return EINVAL;
        }

        if ( d_file->date != s_file->date )
        {
            printf ( "Warning: correcting date ... " );
            d_file->date = s_file->date;
            printf ( " ok\n" );
        }

        if ( d_file->time != s_file->time )
        {
            printf ( "Warning: correcting time ... " );
            d_file->time = s_file->time;
            printf ( " ok\n" );
        }

        if ( d_file->attribs != s_file->attribs )
        {
            printf ( "Warning: correcting attribs ... " );
            d_file->attribs = s_file->attribs;
            printf ( " ok\n" );
        }

        /* Calculate file names lengths */
        s_filename = file_name_len ( s_offset + sizeof ( struct CFFILE ), imem + ilen );
        d_filename = file_name_len ( d_offset + sizeof ( struct CFFILE ), omem + olen );

        /* Compare file names lengths */
        if ( s_filename != d_filename )
        {
            fprintf ( stderr, "Error: files names lengths differ!\n" );
            return EINVAL;
        }

        /* Validate file names */
        PTR_ASSERT ( s_offset, sizeof ( struct CFFILE ) + s_filename, imem, ilen );
        PTR_ASSERT ( d_offset, sizeof ( struct CFFILE ) + d_filename, omem, olen );

        /* Compare file names */
        if ( memcmp ( d_offset + sizeof ( struct CFFILE ), s_offset + sizeof ( struct CFFILE ),
                s_filename ) )
        {
            fprintf ( stderr, "Error: files names differ!\n" );
            printf ( "A file name: |%s|\n", ( char * ) s_offset + sizeof ( struct CFFILE ) );
            printf ( "B file name: |%s|\n", ( char * ) d_offset + sizeof ( struct CFFILE ) );
            return EINVAL;
        }

        /* Seek to next file structures */
        s_offset += sizeof ( struct CFFILE ) + s_filename;
        d_offset += sizeof ( struct CFFILE ) + d_filename;
    }

    printf ( "done checking files.\n" );

    return 0;
}

/* Clone utility main function */
int main ( int argc, char *argv[] )
{
    int error_status = 0;
    int ifd = -1;
    int ofd = -1;
    size_t ilen = 0;
    unsigned char *imem = NULL;
    size_t olen = 0;
    unsigned char *omem = NULL;
    struct stat statbuf;

    /* Show program logo */
    printf ( "CAB clone - ver. " ICAB_VERSION "\n" );

    /* Validate arguments count */
    if ( argc < 3 )
    {
        show_usage (  );
        return 1;
    }

    /* Open input file */
    if ( ( ifd = open ( argv[1], O_RDONLY ) ) < 0 )
    {
        fprintf ( stderr, "Failed to open input file: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    /* Open output file */
    if ( ( ofd = open ( argv[2], O_RDWR ) ) < 0 )
    {
        fprintf ( stderr, "Failed to open output file: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    /* Obtain input file size */
    if ( fstat ( ifd, &statbuf ) < 0 )
    {
        fprintf ( stderr, "Failed to obtain input file size: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    ilen = statbuf.st_size;

    /* Obtain output file size */
    if ( fstat ( ofd, &statbuf ) < 0 )
    {
        fprintf ( stderr, "Failed to obtain output file size: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    olen = statbuf.st_size;

    /* Map input file */
    if ( ( imem =
            ( unsigned char * ) mmap ( NULL, ilen, PROT_READ, MAP_PRIVATE | MAP_POPULATE, ifd,
                0 ) ) == NULL )
    {
        fprintf ( stderr, "Failed to map input file: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    /* Map output file */
    if ( ( omem =
            ( unsigned char * ) mmap ( NULL, olen, PROT_READ | PROT_WRITE, MAP_SHARED, ofd,
                0 ) ) == MAP_FAILED )
    {
        fprintf ( stderr, "Failed to map input file: %i\n", errno );
        error_status = errno;
        goto exit;
    }

    /* Close input file fd (no longer needed) */
    close ( ifd );
    ifd = -1;

    /* Close output file fd (no longer needed) */
    close ( ofd );
    ofd = -1;

    /* Clone archive info */
    if ( ( error_status = icab_clone ( imem, ilen, omem, olen ) ) != 0 )
    {
        goto exit;
    }

    /* Sync output file content */
    if ( msync ( ( void * ) omem, olen, MS_SYNC ) < 0 )
    {
        fprintf ( stderr, "Failed to sync output file: %i\n", errno );
        error_status = errno;
    }

  exit:

    /* Free input file mapped memory */
    if ( imem != NULL )
    {
        munmap ( imem, ilen );
    }

    /* Free output file mapped memory */
    if ( omem != NULL )
    {
        munmap ( omem, olen );
    }

    /* Close input file fd */
    if ( ifd != -1 )
    {
        close ( ifd );
    }

    /* Close output file fd */
    if ( ofd != -1 )
    {
        close ( ofd );
    }

    printf ( "Exit status: %i\n", error_status );

    return error_status;
}
