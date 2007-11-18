#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <assert.h>
#include <zlib.h>

#define eprintf(...) fprintf ( stderr, __VA_ARGS__ )

#define BUFSIZE 4096

/**
 * A CPIO archive header
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

#define CPIO_TRAILER_NAME "TRAILER!!!"

/**
 * An archive file
 *
 * This is basically a gzip file, implemented without using the gzio
 * interface present in zlib.  We do this because the gzio interface
 * provides no way to obtain the length of the compressed data stream,
 * and also because it closes the underlying file before we have a
 * chance to append our padding bytes.
 */
struct archive {
	/** File name (used for error messages) */
	const char *name;
	/** Stream */
	FILE *file;
	/** Byte count */
	off_t count;
};

/** Global verbosity level */
int verbosity = 0;

/** Output file name */
char *output_file = NULL;

/**
 * Set value of a CPIO header field
 *
 * @v field		CPIO header field
 * @v value		Value to set
 */
static void set_cpio_field ( char *field, unsigned long value ) {
	static char buf[9] = { 0, };

	snprintf ( buf, sizeof ( buf ), "%08lx", value );
	memcpy ( field, buf, 8 );
}

/**
 * Store raw data within archive file
 *
 * @v archive		Archive file
 * @v buf		Data to write
 * @v len		Length of data to write
 * @ret success		0 on success, -1 on error
 */
static int store_raw ( struct archive *archive, const void *buf, size_t len ) {
	size_t written;

	written = fwrite ( buf, 1, len, archive->file );
	if ( written != len ) {
		eprintf ( "Could not write to %s: %s\n",
			  archive->name, strerror ( errno ) );
		return -1;
	}
	archive->count += len;

	return 0;
}

/**
 * Store data within archive file
 *
 * @v archive		Archive file
 * @v buf		Data to write
 * @v len		Length of data to write
 * @ret success		0 on success, -1 on error
 */
static int store ( struct archive *archive, const void *buf, size_t len ) {
	return store_raw ( archive, buf, len );
}

/**
 * Store contents of a regular file within archive file
 *
 * @v archive		Archive file
 * @v real_path		Path to regular file
 * @v len		Length of data to write
 * @ret success		0 on success, -1 on error
 */
static int store_file ( struct archive *archive,
			const char *real_path, off_t size ) {
	FILE *file;
	char buf[BUFSIZE];
	size_t len;

	file = fopen ( real_path, "r" );
	if ( ! file ) {
		eprintf ( "Could not open %s: %s\n", real_path,
			  strerror ( errno ) );
		return -1;
	}

	while ( size ) {
		len = size;
		if ( len > sizeof ( buf ) )
			len = sizeof ( buf );
		if ( fread ( buf, 1, len, file ) != len ) {
			if ( feof ( file ) ) {
				eprintf ( "%s: too short\n", real_path );
			} else {
				eprintf ( "Could not read %s: %s\n",
					  real_path, strerror ( errno ) );
			}
			fclose ( file );
			return -1;
		}
		if ( store ( archive, buf, len ) < 0 ) {
			fclose ( file );
			return -1;
		}
		size -= len;
	}

	fclose ( file );
	return 0;
}

/**
 * Store contents of a symlink within archive file
 *
 * @v archive		Archive file
 * @v real_path		Path to symlink
 * @v len		Length of data to write
 * @ret success		0 on success, -1 on error
 */
static int store_symlink ( struct archive *archive,
			   const char *real_path, off_t size ) {
	char buf[BUFSIZE];
	int len;

	len = readlink ( real_path, buf, sizeof ( buf ) );
	if ( len < 0 ) {
		eprintf ( "Cannot read link %s: %s\n", real_path,
			  strerror ( errno ) );
		return -1;
	}
	if ( len != size ) {
		eprintf ( "readlink(%s) disagrees with stat(%s)\n",
			  real_path, real_path );
		return -1;
	}
	if ( store ( archive, buf, size ) < 0 )
		return -1;

	return 0;
}

/**
 * Store a directory tree in the archive file
 *
 * @v archive		Archive file
 * @v real_path		Path to the object
 * @v stored_path	Path to use for the object within the archive
 * @ret success		0 on success, -1 on error
 */
static int store_tree ( struct archive *archive,
			const char *real_path, const char *stored_path ) {
	struct stat st;
	struct cpio_header cpio;
	static const char padding[] = { 0, 0, 0 };
	size_t real_path_len;
	size_t stored_path_len;
	off_t filesize;
	unsigned int pad_len;

	/* Tidy up stored path */
	while ( *stored_path == '/' )
		stored_path++;
	real_path_len = strlen ( real_path );
	stored_path_len = strlen ( stored_path );

	if ( verbosity > 0 )
		eprintf ( "%s => %s\n", real_path, stored_path );

	/* Stat file */
	if ( lstat ( real_path, &st ) < 0 ) {
		eprintf ( "Could not stat %s: %s\n", real_path,
			  strerror ( errno ) );
		return -1;
	}

	/* If this is *not* the root node, create a CPIO entry */
	if ( stored_path_len ) {

		/* Construct and store cpio header */
		memcpy ( cpio.c_magic, CPIO_MAGIC, sizeof ( cpio.c_magic ) );
		filesize = st.st_size;
		if ( ! ( S_ISREG ( st.st_mode ) || S_ISLNK ( st.st_mode ) ) )
			filesize = 0;
		set_cpio_field ( cpio.c_ino, st.st_ino );
		set_cpio_field ( cpio.c_mode, st.st_mode );
		set_cpio_field ( cpio.c_uid, 0 /* always squash UID */ );
		set_cpio_field ( cpio.c_gid, 0 /* always squash GID */ );
		set_cpio_field ( cpio.c_nlink, st.st_nlink );
		set_cpio_field ( cpio.c_mtime, st.st_mtime );
		set_cpio_field ( cpio.c_filesize, filesize );
		set_cpio_field ( cpio.c_maj, major ( st.st_dev ) );
		set_cpio_field ( cpio.c_min, minor ( st.st_dev ) );
		set_cpio_field ( cpio.c_rmaj, major ( st.st_rdev ) );
		set_cpio_field ( cpio.c_rmin, minor ( st.st_rdev ) );
		set_cpio_field ( cpio.c_namesize, ( stored_path_len + 1 ) );
		set_cpio_field ( cpio.c_chksum, 0 );
		if ( store ( archive, &cpio, sizeof ( cpio ) ) < 0 )
			return -1;

		/* Store path name */
		if ( store ( archive, stored_path,
			     ( stored_path_len + 1 ) ) < 0 )
			return -1;
		pad_len = ( -( sizeof ( cpio ) + stored_path_len + 1 ) & 3 );
		if ( store ( archive, padding, pad_len ) < 0 )
			return -1;

		/* Read and store object data */
		if ( S_ISREG ( st.st_mode ) ) {
			if ( store_file ( archive, real_path, filesize ) < 0 )
				return -1;
		} else if ( S_ISLNK ( st.st_mode ) ) {
			if ( store_symlink ( archive, real_path,
					     filesize ) < 0 )
				return -1;
		} else {
			assert ( filesize == 0 );
		}
		pad_len = ( ( -filesize ) & 3 );
		if ( store ( archive, padding, pad_len ) < 0 )
			return -1;
	}

	/* If this is a directory, recurse into it */
	if ( S_ISDIR ( st.st_mode ) ) {
		DIR *dir;
		struct dirent *dirent;

		dir = opendir ( real_path );
		if ( ! dir ) {
			eprintf ( "Could not open directory %s: %s\n",
				  real_path, strerror ( errno ) );
			return -1;
		}

		while ( ( dirent = readdir ( dir ) ) ) {
			size_t name_len = strlen ( dirent->d_name );
			size_t new_real_path_len =
				( real_path_len + 1 + name_len );
			size_t new_stored_path_len =
				( stored_path_len + 1 + name_len );
			char new_real_path[ new_real_path_len + 1 ];
			char new_stored_path[ new_stored_path_len + 1 ];

			if ( ( strcmp ( dirent->d_name, "." ) == 0 ) ||
			     ( strcmp ( dirent->d_name, ".." ) == 0 ) )
				continue;

			snprintf ( new_real_path, sizeof ( new_real_path ),
				   "%s/%s", real_path, dirent->d_name );
			snprintf ( new_stored_path, sizeof ( new_stored_path ),
				   "%s/%s", stored_path, dirent->d_name );
			if ( store_tree ( archive, new_real_path,
					  new_stored_path ) < 0 ) {
				closedir ( dir );
				return -1;
			}
		}

		closedir ( dir );
	}

	return 0;
}

/**
 * Store CPIO trailer within archive file
 *
 * @v archive		Archive file
 * @ret success		0 on success, -1 on error
 */
static int store_trailer ( struct archive *archive ) {
	struct cpio_header cpio;
	static const char trailer_name[] = CPIO_TRAILER_NAME;
	static const char padding[] = { 0, 0, 0 };
	unsigned int pad_len;

	/* Construct and store cpio header */
	memset ( &cpio, '0', sizeof ( cpio ) );
	memcpy ( cpio.c_magic, CPIO_MAGIC, sizeof ( cpio.c_magic ) );
	set_cpio_field ( cpio.c_namesize, sizeof ( trailer_name ) );
	if ( store ( archive, &cpio, sizeof ( cpio ) ) < 0 )
		return -1;
	if ( store ( archive, trailer_name, sizeof ( trailer_name ) ) < 0 )
		return -1;
	pad_len = ( -( sizeof ( cpio ) + sizeof ( trailer_name ) ) & 3 );
	if ( store ( archive, padding, pad_len ) < 0 )
		return -1;
	return 0;
}

/**
 * Store optionally-mapped directory tree within archive file
 *
 * @v archive		Archive file
 * @v mapped_path	Path specification
 * @ret success		0 on success, -1 on error
 */
static int store_mapped_path ( struct archive *archive,
			       char *mapped_path ) {
	char *real_path = mapped_path;
	char *stored_path;
	char *separator;

	separator = strchr ( mapped_path, '=' );
	if ( separator ) {
		*separator = '\0';
		stored_path = ( separator + 1 );
	} else {
		stored_path = real_path;
	}

	return store_tree ( archive, real_path, stored_path );
}

/**
 * Parse command-line options
 *
 * @v argc		Argument count
 * @v argv		Arguments
 * @ret optind		First non-option argument, or -1 on error
 */
int parseopts ( const int argc, char **argv ) {
	static const struct option long_options[] = {
		{ "verbose", 0, NULL, 'v' },
		{ "quiet", 0, NULL, 'q' },
		{ "help", 0, NULL, 'h' },
		{ "output", required_argument, NULL, 'o' },
		{ 0, 0, 0, 0 }
	};
	static const char usage[] = 
		"Usage: %s [-v|-q] [-o output.bp] dir[=dir] ...\n";
	int c;

	while ( 1 ) {
		int option_index = 0;

		c = getopt_long ( argc, argv, "qvho:",
				  long_options, &option_index );
		if ( c == -1 )
			break;

		switch ( c ) {
		case 'v':
			verbosity++;
			break;
		case 'q':
			verbosity--;
			break;
		case 'h':
			eprintf ( usage, argv[0] );
			return -1;
		case 'o':
			output_file = optarg;
			break;
		case '?':
			return -1;
		default:
			eprintf ( "Warning: unrecognised option %c\n", c );
			return -1;
		}
	}

	return optind;
}

int main ( int argc, char **argv ) {
	struct archive archive;
	static const char padding[] = { 0, 0, 0 };
	int arg;
	unsigned int pad_len;

	/* Parse command-line options */
	arg = parseopts ( argc, argv );
	if ( arg < 0 )
		exit ( 1 );

	/* Initialise archive */
	memset ( &archive, 0, sizeof ( archive ) );
	if ( output_file ) {
		archive.name = output_file;
		archive.file = fopen ( output_file, "w" );
		if ( ! archive.file ) {
			eprintf ( "Could not open %s for writing: %s\n",
				  output_file, strerror ( errno ) );
			exit ( 1 );
		}
	} else {
		archive.name = "stdout";
		archive.file = stdout;
	}

	/* Store directory trees */
	for ( ; arg < argc ; arg++ ) {
		if ( store_mapped_path ( &archive, argv[arg] ) < 0 )
			exit ( 1 );
	}

	/* Store CPIO trailer */
	if ( store_trailer ( &archive ) < 0 )
		exit ( 1 );

	/* Pad file to a multiple of 4 bytes */
	pad_len = ( -( archive.count ) & 3 );
	if ( store_raw ( &archive, padding, pad_len ) < 0 )
		return -1;

	/* Close archive */
	if ( fclose ( archive.file ) != 0 ) {
		eprintf ( "Could not close %s: %s\n", archive.name,
			  strerror ( errno ) );
	}

	return 0;
}
