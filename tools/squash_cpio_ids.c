#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define eprintf(...) fprintf ( stderr, __VA_ARGS__ )

#define BUFSIZE 4096
static char buf[BUFSIZE];

/** A CPIO archive header
 *
 * All field are hexadecimal ASCII numbers padded with '0' on the
 * left to the full width of the field.
 */
struct cpio_header {
	/** The string "070701" or "070702" */
	char c_magic[6];
	/** File inode number */
	char c_ino[8];
	/** File mode and permissions */
	char c_mode[8];
	/** File uid */
	char c_uid[8];
	/** File gid */
	char c_gid[8];
	/** Number of links */
	char c_nlink[8];
	/** Modification time */
	char c_mtime[8];
	/** Size of data field */
	char c_filesize[8];
	/** Major part of file device number */
	char c_maj[8];
	/** Minor part of file device number */
	char c_min[8];
	/** Major part of device node reference */
	char c_rmaj[8];
	/** Minor part of device node reference */
	char c_rmin[8];
	/** Length of filename, including final NUL */
	char c_namesize[8];
	/** Checksum of data field if c_magic is 070702, othersize zero */
	char c_chksum[8];
} __attribute__ (( packed ));

#define CPIO_MAGIC "070701"

static unsigned long parse_cpio ( char *field ) {
	static char buf[9] = { 0, };
	unsigned long value;
	char *endp;

	memcpy ( buf, field, 8 );
	value = strtoul ( buf, &endp, 16 );
	if ( *endp ) {
		eprintf ( "Invalid field content %s\n", buf );
		exit ( 1 );
	}
	return value;
}

int main ( int argc, char **argv ) {
	struct cpio_header cpio;
	size_t namesize;
	size_t filesize;
	size_t len;
	unsigned long offset = 0;

	if ( argc != 1 ) {
		eprintf ( "Syntax: %s < input.cpio > output.cpio\n", argv[0] );
		exit ( 1 );
	}

	while ( 1 ) {

		/* Read and patch cpio header */
		if ( fread ( &cpio, 1, sizeof ( cpio ),
			     stdin ) != sizeof ( cpio ) ) {
			eprintf ( "Input truncated at offset %ld\n", offset );
			exit ( 1 );
		}		
		if ( memcmp ( cpio.c_magic, CPIO_MAGIC,
			      sizeof ( cpio.c_magic ) ) != 0 ) {
			eprintf ( "Bad magic value at offset %ld\n", offset );
			exit ( 1 );
		}
		memset ( cpio.c_uid, '0', sizeof ( cpio.c_uid ) );
		memset ( cpio.c_gid, '0', sizeof ( cpio.c_gid ) );
		namesize = parse_cpio ( cpio.c_namesize );
		filesize = parse_cpio ( cpio.c_filesize );
		if ( fwrite ( &cpio, 1, sizeof ( cpio ),
			      stdout ) != sizeof ( cpio ) ) {
			eprintf ( "Output truncated at offset %ld\n", offset );
			exit ( 1 );
		}
		offset += sizeof ( cpio );

		/* Read filename */
		namesize = ((( offset + namesize + 0x03 ) & ~0x03 ) - offset );
		if ( fread ( buf, 1, namesize, stdin ) != namesize ) {
			eprintf ( "Input truncated\n ");
			exit ( 1 );
		}
		if ( fwrite ( buf, 1, namesize, stdout ) != namesize ) {
			eprintf ( "Output truncated at offset %ld\n", offset );
			exit ( 1 );
		}
		offset += namesize;
		if ( memcmp ( buf, "TRAILER!!!", 11 ) == 0 )
			break;

		/* Read file content */
		filesize = ((( offset + filesize + 0x03 ) & ~0x03 ) - offset );
		while ( filesize ) {
			len = filesize;
			if ( len > sizeof ( buf ) )
				len = sizeof ( buf );
			if ( fread ( buf, 1, len, stdin ) != len ) {
				eprintf ( "Input truncated at offset %ld\n",
					  offset );
				exit ( 1 );
			}
			if ( fwrite ( buf, 1, len, stdout ) != len ) {
				eprintf ( "Output truncated at offset %ld\n",
					  offset );
				exit ( 1 );
			}
			filesize -= len;
			offset += len;
		}
	}
	
	/* Copy any trailing data */
	while ( 1 ) {
		len = fread ( buf, 1, sizeof ( buf ), stdin );
		if ( ! len )
			break;
		if ( fwrite ( buf, 1, len, stdout ) != len ) {
			eprintf ( "Output truncated at offset %ld\n", offset );
			exit ( 1 );
		}
	}
	if ( ferror ( stdin ) ) {
		eprintf ( "Could not read: %s\n", strerror ( errno ) );
		exit ( 1 );
	}

	return 0;
}
