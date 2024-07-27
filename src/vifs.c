#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>					
#include "vfs.h"
#include "rfs.h"
#include "afs.h"

/*

--------------------Kernel Exec Path--------------------
1. Initalize all file systems
2. Mount file systems via vfs_mount


--------------------VFS Layer--------------------
vfs_inode -- represents file, directory, etc...
vfs_operations -- open, close, read, write, etc... always works against vfs_inode
vfs_filesystem -- fs data, inode counter, links to elsewhere

vfs_register_fs() -- puts the fs into the kernel for use
vfs_lookup( path ) -- returns inode associated with path
vfs_mount( fs type, fs data root, fs path )


--------------------FS Layer--------------------
1. Register filesystem
	- Call vfs_register_fs
	- Setup vfs_operations

fs_get_directory_inodes() returns list of inodes from the given directory (itself an inode)



*/

#ifdef VIFS_DEV
FILE *fp;

#define COMMAND_CP 1
#define COMMAND_CPDIR 2
#define COMMAND_BOOTSTRAP 3
#define COMMAND_SET_AFS 4
#define COMMAND_RUN_OS_TESTS 5
#define COMMAND_HELP 6
#define COMMAND_LS 7
#define COMMAND_MKDIR 8
#define COMMAND_CAT 9
#define COMMAND_NEW 10

#define WANT_PATH 0
#define WANT_NAME 1

#define INPUT_IS(x) strcmp( argv[i], x ) == 0
#define verbosef( ... ) if( verbose == true ) printf( __VA_ARGS__ )

bool verbose = false;

int main( int argc, char *argv[] ) {
	bool opt_afs_img = false;
	int command = 0;
	char *param_1 = NULL;
	char *param_2 = NULL;
	char *afs_img = NULL;
	int expect_params = 0;

	if( argc < 2 ) {
		vifs_show_help();
		return 0;
	}

	for( int i = 1; i < argc; i++ ) {
		if( expect_params == 0 ) {
			if( INPUT_IS( "cp" ) ) {
				command = COMMAND_CP;
				expect_params = 2;
			} else if( INPUT_IS( "cpdir" ) ) {
				command = COMMAND_CPDIR;
				expect_params = 2;
			} else if( INPUT_IS( "bootstrap" ) ) {
				command = COMMAND_BOOTSTRAP;
				expect_params = 1;
			} else if( INPUT_IS( "-afs" ) ) {
				opt_afs_img = true;
				expect_params = 1;
			} else if( INPUT_IS( "ostests" ) ) {
				command = COMMAND_RUN_OS_TESTS;
				expect_params = 0;
			} else if( INPUT_IS( "help" ) ) {
				command = COMMAND_HELP;
				expect_params = 0;
			} else if( INPUT_IS( "ls" ) ) {
				command = COMMAND_LS;
				expect_params = 1;
			} else if( INPUT_IS( "mkdir" ) ) {
				command = COMMAND_MKDIR;
				expect_params = 1;
			} else if( INPUT_IS( "cat" ) ) {
				command = COMMAND_CAT;
				expect_params = 1;
			} else if( INPUT_IS( "-v" ) ) {
				verbose = true;
				expect_params = 0;
			} else if( INPUT_IS( "new" ) ) {
				command = COMMAND_NEW;
				expect_params = 1;
			} else {
				printf( "Unexpected command.\n" );
				return 0;
			}
		} else {
			if( opt_afs_img == true ) {
				afs_img = argv[i];
			} else {
				if( param_1 == NULL ) {
					param_1 = argv[i];
					expect_params--;
				} else if( param_2 == NULL ) {
					param_2 = argv[i];
					expect_params--;
				} else {
					printf( "Unexpected command parameter: %s.\n", argv[i] );
					return 0;
				}
			}
		}		
	}

	vifs_vfs_initalize();

	if( command == COMMAND_NEW || command == COMMAND_RUN_OS_TESTS || command == COMMAND_HELP || command == COMMAND_BOOTSTRAP ) {
		// do nothing
	} else {
		if( afs_img != NULL ) {
			vifs_afs_initalize( afs_img );
		} else {
			vifs_afs_initalize( "afs.img" );
		}
	}

	switch( command ) {
		case COMMAND_CP:
			vifs_cp( param_1, param_2 );
			break;
		case COMMAND_CPDIR:
			vifs_cpdir( param_1, param_2 );
			break;
		case COMMAND_BOOTSTRAP:
			if( afs_img == NULL ) {
				vifs_bootstrap( param_1, "afs.img" );
			} else {
				vifs_bootstrap( param_1, afs_img );
			}
			break;
		case COMMAND_RUN_OS_TESTS:
			vifs_run_os_tests();
			break;
		case COMMAND_LS:
			vifs_ls( param_1 );
			break;
		case COMMAND_HELP:
			vifs_show_help();
			break;
		case COMMAND_MKDIR:
			vifs_mkdir( param_1 );
			break;
		case COMMAND_CAT:
			vifs_cat( param_1 );
			break;
		case COMMAND_NEW:
			if( afs_img == NULL ) {
				vifs_new_drive_img( param_1, "afs.img" );
			} else {
				vifs_new_drive_img( param_1, afs_img );
			}
			
			break;
		default:
			printf( "Unknown command.\n" );
	}

	return 0;
}

void vifs_show_help( void ) {
	printf( "vifs [command] [parameters] [options]\n" );
	printf( "     Commands and Parameters:\n");
	printf( "         bootstrap <level>\n" );
	printf( "              Formats a drive to a default state\n" );
	printf( "              Level 0 = empty drive\n" );
	printf( "              Level 1 = test data\n" );
	printf( "         cat <pathname>\n" );
	printf( "              Sends file to stdout\n" );
	printf( "         cp <source_file> <dest_file>\n" );
	printf( "              Copies host's source_file to AFS drive at dest_file\n" );
	printf( "         cpdir <source_directory> <dest_dir>\n" );
	printf( "              Recursively copies host's source dir to AFF's dest dir\n" );
	printf( "         ls <directory>\n" );
	printf( "              Lists the contents of a directory\n" );
	printf( "         mkdir <pathname>\n" );
	printf( "              Creates the given directory (non-recursive)\n" );
	printf( "         new <size>\n" );
	printf( "              Creates an empty image file of size\n" );
	printf( "         ostests\n" );
	printf( "              Runs series of tests on RamFS and AFS drives\n" );
	printf( "\n" );
	printf( "     Options:\n" );
	printf( "         -afs <afs_image_file>\n" );
	printf( "              Specify afs file, otherwise use afs.img\n" );
	printf( "         -v   Turns on verbose mode\n" );
}

void vifs_new_drive_img( char *size, char *afs_image ) {
	// dd if=/dev/zero of=./afs.img bs=4k iflag=fullblock,count_bytes count=200M

	char size_line[50];
	memset( size_line, 0, 50 );
	strcpy( size_line, "count=" );
	strcat( size_line, size );
	strcat( size_line, "M" );

	char of_line[255];
	memset( of_line, 0, 255 );
	strcpy( of_line, "of=" );
	strcat( of_line, afs_image );

	printf( "size: %s\n", size );
	printf( "size line: %s\n", size_line );


	char *argv[] = {
		"dd",
		"if=/dev/zero",
		of_line,
		"bs=4k",
		"iflag=fullblock,count_bytes",
		size_line,
		NULL
	};

	int error = execvp( argv[0], argv );

	if( error == -1 ) {
		printf( "Error: %s\n", strerror(errno) );
	}
}

/**
 * @brief 
 * 
 * @param src 
 * @param dest 
 */
void vifs_cp( char *src, char *dest ) {
	char path[255];
	char name[VFS_NAME_MAX];

	vifs_pathname_to_path( dest, path );
	vifs_pathname_to_name( dest, name );

	verbosef( "cp from %s to %s in file %s\n", src, path, name );

	vfs_test_cp_real_file( src, path, name );
}

/**
 * @brief 
 * 
 * @param src 
 * @param dest 
 */
void vifs_cpdir( char *src, char *dest ) {
	verbosef( "cpdir from %s to %s\n", src, dest );

	DIR *d;
	struct dirent *dir;
	char src_loc_full[256];
	char dest_loc_full[256];

	d = opendir( src );

	if(d) {
		while( (dir = readdir(d)) != NULL ) {
			memset( src_loc_full, 0, 256 );
			memset( dest_loc_full, 0, 256 );

			if( strcmp( dir->d_name, "." ) == 0 ) {
				// Skip and continue
				continue;
			}

			if( strcmp( dir->d_name, ".." ) == 0 ) {
				// Skip and continue
				continue;
			}

			strcpy( src_loc_full, src );
			strcat( src_loc_full, "/" );
			strcat( src_loc_full, dir->d_name );

			strcpy( dest_loc_full, dest );
			if( strcmp( dest, "/" ) != 0 ) {
				strcat( dest_loc_full, "/" );
			}
			strcat( dest_loc_full, dir->d_name );

			printf( "%s -> %s\n", src_loc_full, dest_loc_full );

			if( dir->d_type == DT_REG ) {
				vifs_cp( src_loc_full, dest_loc_full );
			} else if( dir->d_type == DT_DIR ) {
				vifs_mkdir( dest_loc_full );
				vifs_cpdir( src_loc_full, dest_loc_full );
			}
		}

		closedir(d);
	} else {
		printf( "d is empty.\n" );
		return;
	}
}

/**
 * @brief Create a dir at pathname
 * 
 * @param pathname 
 */
void vifs_mkdir( char *pathname ) {
	char path[255];
	char name[VFS_NAME_MAX];

	vifs_pathname_to_path( pathname, path );
	vifs_pathname_to_name( pathname, name );

	verbosef( "mkdir path = \"%s\", name = \"%s\"\n", path, name );

	vfs_test_create_dir( path, name );
}

/**
 * @brief Prints out file pathname from the vifs
 * 
 * @param pathname 
 */
void vifs_cat( char *pathname ) {
	verbosef( "cat pathname = \"%s\"\n", pathname );

	vfs_test_cat( pathname );
}

/**
 * @brief Put the path part of pathname into path
 * 
 * @param pathname 
 * @param path 
 */
void vifs_pathname_to_path( char *pathname, char *path ) {
	return vifs_parse_pathname( pathname, WANT_PATH, path );
}

/**
 * @brief Put the name part of pathname into name
 * 
 * @param pathname 
 * @param name 
 */
void vifs_pathname_to_name( char *pathname, char *name ) {
	return vifs_parse_pathname( pathname, WANT_NAME, name );
}

/**
 * @brief Break apart pathname to path or name
 * 
 * @param pathname 
 * @param path_or_name 
 * @param data 
 */
void vifs_parse_pathname( char *pathname, int path_or_name, char *data ) {
	char parsedata[255];
	strcpy( parsedata, pathname );

	char path[255] = "";
	char name[VFS_NAME_MAX] = "";
	int i = 0;
	char *token = NULL;
	char *last_token = NULL;

	token = strtok( parsedata, "/" );

	while( token != NULL ) {
		last_token = token;

		token = strtok( NULL, "/" );

		if( token != NULL ) {
			strcat( path, "/" );
			strcat( path, last_token );
		} else {
			strcpy( name, last_token );
		}
	}

	if( strcmp(path, "") == 0 ) {
		strcpy( path, "/" );
	}

	switch( path_or_name ) {
		case WANT_PATH:
			strcpy( data, path );
			break;
		case WANT_NAME:
			strcpy( data, name );
			break;
	}
}

/**
 * @brief 
 * 
 * @param level 
 * @param afs_image 
 */
void vifs_bootstrap( char *level, char *afs_image ) {
	verbosef( "boostrap level: %s on %s\n", level, afs_image );

	// Open and size afs.img
	fp = fopen( afs_image, "r+" );

	if( fp == NULL ) {
		printf( "Could not open %s.\n", afs_image );
		return;
	}
	
	fseek( fp, 0, SEEK_END );
	uint64_t size = ftell( fp );
	rewind( fp );
	
	// Boostrap afs.img
	afs_bootstrap( fp, size );

	if( atoi(level) == 1 ) {
		char hello_data[] = "World of AFS!";

		vfs_test_create_file( "/", "hello", hello_data, sizeof(hello_data) );
		vfs_test_create_dir( "/", "bin" );
		vfs_test_create_dir( "/", "etc" );
		vfs_test_create_dir( "/", "usr" );
		vfs_test_create_dir( "/usr", "share" );
		vfs_test_create_dir( "/usr/share", "fonts" );
		vfs_test_create_dir( "/usr/share", "test_data" );

		vfs_test_cp_real_file( 
			"/usr/local/osdev/versions/vifs/afs_img_files/share/test_data/picard_history.txt",
			"/usr/share/test_data",
			"picard_history.txt"
		);
	}

	printf( "AFS bootstrapping done.\n" );
}

/**
 * @brief 
 * 
 * @param pathname 
 */
void vifs_ls( char *pathname ) {
	vfs_test_ls( pathname );
}

/**
 * @brief 
 * 
 * @return int 
 */
int vifs_vfs_initalize( void ) {
	// Initalize VFS
	int vfs_init_err = vfs_initalize();
	if( vfs_init_err != 0 ) {
		printf( "VFS initalizatoin failed.\n" );

		return 1;
	}

	verbosef( "VFS Initalizing done.\n" );
}

/**
 * @brief 
 * 
 * @param afs_img 
 * @return int 
 */
int vifs_afs_initalize( char *afs_img ) {
	// Open and size afs.img
	fp = fopen( afs_img, "r+" );

	if( fp == NULL ) {
		printf( "Cannot find %s\n", afs_img );
		return 0;
	}

	fseek( fp, 0, SEEK_END );
	uint64_t size = ftell( fp );
	rewind( fp );
	
	// Read afs.img
	uint8_t *drive_data = vfs_malloc( size );
	rewind( fp );
	uint64_t bytes_read = fread( drive_data, size, 1, fp );

	if( bytes_read != 1 ) {
		printf( "Bytes read failed. Got %ld, expected %ld.\n", bytes_read, size );

		return 1;
	}
	
	// Initalize AFS
	int afs_init_err = afs_initalize( size, drive_data );
	if( afs_init_err != 0 ) {
		printf( "AFS initalization failed.\n" );

		return 1;
	}
	verbosef( "AFS initalizing done.\n" );
	
	// Mount AFS
	int afs_mount_err = vfs_mount( FS_TYPE_AFS, drive_data, "/" );
	if( afs_mount_err != 0 ) {
		printf( "Could not mount afs drive.\n" );

		return 1;
	}
	verbosef( "Mounted afs to /.\n" );
}

/**
 * @brief 
 * 
 * @return int 
 */
int vifs_run_os_tests( void ) {
	// Open and size afs.img
	fp = fopen( "afs.img", "r+" );
	fseek( fp, 0, SEEK_END );
	uint64_t size = ftell( fp );
	rewind( fp );
	
	// Read afs.img
	uint8_t *drive_data = vfs_malloc( size );
	rewind( fp );
	uint64_t bytes_read = fread( drive_data, size, 1, fp );

	if( bytes_read != 1 ) {
		vfs_panic( "Bytes read failed. Got %ld, expected %ld.\n", bytes_read, size );

		return 1;
	}
	
	// Initalize AFS
	int afs_init_err = afs_initalize( size, drive_data );
	if( afs_init_err != 0 ) {
		vfs_panic( "AFS initalization failed.\n" );

		return 1;
	}
	vfs_debugf( "AFS initalizing done.\n" );
	
	// Mount AFS
	int afs_mount_err = vfs_mount( FS_TYPE_AFS, drive_data, "/" );
	if( afs_mount_err != 0 ) {
		vfs_panic( "Could not mount afs drive.\n" );

		return 1;
	}
	vfs_debugf( "Mounted /afs.\n" );

	// Directory for RFS
	vfs_mkdir( 1, "/", "proc" );

	// Initalize RFS
	int rfs_init_err = rfs_initalize();
	if( rfs_init_err != 0 ) {
		vfs_panic( "RFS initalization failed.\n" );

		return 1;
	}
	vfs_debugf( "RFS initalizing done.\n" );

	// Mount RFS
	int mount_err = vfs_mount( FS_TYPE_RFS, NULL, "/proc" );
	if( mount_err != 0 ) {
		vfs_panic( "Could not mount root fs.\n" );

		return 1;
	}
	vfs_debugf( "Mounted RFS.\n" );

	vfs_test_ramfs();
	vfs_test_afs();
}

/**
 * @brief 
 * 
 */
void vfs_test_afs( void ) {
	vfs_test_ls( "/" );
	vfs_test_ls( "/usr" );
	vfs_test_ls( "/usr/share" );
	
	vfs_test_cat( "/hello" );
	
	vfs_test_cat( "/usr/share/test_data/picard_history.txt" );

	afs_dump_diagnostic_data();
}

/**
 * @brief 
 * 
 */
void vfs_test_ramfs( void ) {
	char hello_data[] = "Hello, world!";
	char hello_data_b[] = "Another testing file.\nMulti-line?\nWooho!\n";
	char proc_build_num[] = "1984";
	char proc_magic[] = "NCC-1701-D";

	vfs_test_create_file( "/proc", "test.txt", hello_data, sizeof(hello_data) );
	vfs_test_create_file( "/proc", "test_2.txt", hello_data_b, sizeof(hello_data_b) );
	vfs_test_create_dir( "/proc", "build" );
	vfs_test_create_file( "/proc", "magic", proc_magic, sizeof(proc_magic) );
	vfs_test_create_file( "/proc/build", "number", proc_build_num, sizeof(proc_build_num) );

	vfs_test_ls( "/" );
	vfs_test_ls( "/proc" );
	vfs_test_ls( "/proc/build" );
	
	vfs_test_cat( "/proc/magic" );
	vfs_test_cat( "/proc/build/number" );
}

/**
 * @brief Copies a file from the host drive into the vifs drive (and writes to disk as apporpriate)
 * 
 * @param real_file_pathname 
 * @param vifs_name 
 * @param data 
 * @param size 
 */
void vfs_test_cp_real_file( char *real_file_pathname, char *vifs_path, char *vifs_name ) {
	FILE *f = fopen( real_file_pathname, "r" );

	if( f == NULL ) {
		vfs_debugf( "Cannot copy %s, f returned NULL.\n", real_file_pathname );
		return;
	}

	struct stat file_meta;
	stat( real_file_pathname, &file_meta );

	uint8_t *buff = vfs_malloc( file_meta.st_size );

	if( fread( buff, file_meta.st_size, 1, f ) != 1 ) {
		vfs_debugf( "Read failed on %s, fread returned not 1.\n", real_file_pathname );
		return;
	}

	vfs_test_create_file( vifs_path, vifs_name, buff, file_meta.st_size );

	vfs_free( buff );
}

/**
 * @brief Test creating a file
 * 
 * @param path 
 * @param name 
 * @param data 
 * @param size 
 */
void vfs_test_create_file( char *path, char *name, uint8_t *data, uint64_t size ) {
	int file_inode = vfs_create( VFS_INODE_TYPE_FILE, path, name );
	if( file_inode < 0 ) {
		vfs_panic( "Could not create %s\n", name );
	}

	int write_err = vfs_write( file_inode, data, size, 0 );
	if( write_err < 0 ) {
		vfs_panic( "Error when writing.\n" );
	}
}

/**
 * @brief Test creating a directory
 * 
 * @param path 
 * @param name 
 */
void vfs_test_create_dir( char *path, char *name ) {
	int file_inode = vfs_mkdir( vfs_lookup_inode(path), path, name );
	if( file_inode < 0 ) {
		vfs_panic( "Could not create %s\n", name );
	}
}

/**
 * @brief Test listing a file
 * 
 * @param path 
 */
void vfs_test_ls( char *path ) {
	char type_dir[] = "DIR ";
	char type_file[] = "FILE";
	char type_unknown[] = "????";

	vfs_debugf( "Listing: %s\n", path );
	vfs_directory_list *dir_list = vfs_malloc( sizeof(vfs_directory_list) );
	vfs_get_directory_list( vfs_lookup_inode(path), dir_list );

	for( int i = 0; i < dir_list->count; i++ ) {
		char *type = NULL;

		vfs_inode *n = vfs_lookup_inode_ptr_by_id( dir_list->entry[i].id );
		switch( n->type ) {
			case VFS_INODE_TYPE_DIR:
				type = type_dir;
				break;
			case VFS_INODE_TYPE_FILE:
				type = type_file;
				break;
			default:
				type = type_unknown;
		}

		vfs_debugf( "    %03ld %s %s\n", dir_list->entry[i].id, type, dir_list->entry[i].name );
	}

	vfs_debugf( "\n" );
}

/**
 * @brief Test cat of a file
 * 
 * @param pathname 
 */
void vfs_test_cat( char *pathname ) {
	vfs_stat_data stats;

	vfs_stat( vfs_lookup_inode(pathname), &stats );

	char *data = vfs_malloc( stats.size );
	int read_err = vfs_read( vfs_lookup_inode(pathname), data, stats.size, 0 );
	if( read_err < 0 ) {
		vfs_panic( "Error when reading.\n" );
	}

	data[ stats.size ] = 0;

	vfs_debugf( "cat %s\n", pathname );
	vfs_debugf( "size: %d\n", stats.size );
	vfs_debugf( "%s\n", data );
	vfs_debugf( "\n" );
}


#endif