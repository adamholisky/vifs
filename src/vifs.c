#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

int main( int argc, char *argv[] ) {
	int vfs_init_err = vfs_initalize();
	if( vfs_init_err != 0 ) {
		vfs_panic( "VFS initalizatoin failed.\n" );

		return 1;
	}

	vfs_debugf( "VFS Initalizing done.\n" );

	int rfs_init_err = rfs_initalize();
	if( rfs_init_err != 0 ) {
		vfs_panic( "RFS initalization failed.\n" );

		return 1;
	}

	vfs_debugf( "RFS initalizing done.\n" );

	int mount_err = vfs_mount( FS_TYPE_RFS, NULL, "/" );
	if( mount_err != 0 ) {
		vfs_panic( "Could not mount root fs.\n" );

		return 1;
	}

	vfs_debugf( "Mounted root.\n" );

	vfs_test_create_dir( "/", "afs" );

	fp = fopen( "afs.img", "r+" );
	fseek( fp, 0, SEEK_END );
	uint64_t size = ftell( fp );
	rewind( fp );
	
	//vfs_debugf( "AFS drive size: %ld\n", size );

	afs_bootstrap( fp, size );

	vfs_debugf( "AFS bootstrapping done.\n" );

	uint8_t *drive_data = vfs_malloc( size );

	rewind( fp );
	uint64_t bytes_read = fread( drive_data, size, 1, fp );

	if( bytes_read != 1 ) {
		vfs_panic( "Bytes read failed. Got %ld, expected %ld.\n", bytes_read, size );

		return 1;
	}
	
	int afs_init_err = afs_initalize( size, drive_data );
	if( afs_init_err != 0 ) {
		vfs_panic( "AFS initalization failed.\n" );

		return 1;
	}

	vfs_debugf( "AFS initalizing done.\n" );
	
	int afs_mount_err = vfs_mount( FS_TYPE_AFS, drive_data, "/afs" );
	if( afs_mount_err != 0 ) {
		vfs_panic( "Could not mount afs drive.\n" );

		return 1;
	}

	vfs_debugf( "Mounted /afs.\n" );

	vfs_test_ramfs();
	vfs_test_afs();
	
	return 0;
}

void vfs_test_afs( void ) {
	char hello_data[] = "World of AFS!";

	vfs_test_create_file( "/afs", "hello", hello_data, sizeof(hello_data) );
	vfs_test_create_dir( "/afs", "bin" );
	vfs_test_create_dir( "/afs", "etc" );
	vfs_test_create_dir( "/afs", "usr" );
	vfs_test_create_dir( "/afs/usr", "share" );
	vfs_test_create_dir( "/afs/usr/share", "fonts" );
	vfs_test_create_dir( "/afs/usr/share", "test_data" );

	vfs_test_ls( "/afs" );
	vfs_test_ls( "/afs/usr" );
	vfs_test_ls( "/afs/usr/share" );
	
	vfs_test_cat( "/afs/hello" );
	vfs_test_cp_real_file( 
		"/usr/local/osdev/versions/vifs/afs_img_files/share/fonts/Gomme10x20n.bdf", 
		"/afs/usr/share/fonts",
		"Gomme10x20n.bdf"
	);

	vfs_test_cp_real_file( 
		"/usr/local/osdev/versions/vifs/afs_img_files/share/test_data/picard_history.txt",
		"/afs/usr/share/test_data",
		"picard_history.txt"
	);
	
	vfs_test_cat( "/afs/usr/share/test_data/picard_history.txt" );

	afs_dump_diagnostic_data();
}

void vfs_test_ramfs( void ) {
	char hello_data[] = "Hello, world!";
	char hello_data_b[] = "Another testing file.\nMulti-line?\nWooho!\n";
	char proc_build_num[] = "1984";
	char proc_magic[] = "NCC-1701-D";

	vfs_test_create_file( "/", "test.txt", hello_data, sizeof(hello_data) );
	vfs_test_create_file( "/", "test_2.txt", hello_data_b, sizeof(hello_data_b) );
	vfs_test_create_dir( "/", "proc" );
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