#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "vfs.h"
#include "afs.h"
#include <sys/stat.h>
#include <dirent.h>

#define afsfile "/usr/local/osdev/versions/vi/afs.img"

typedef struct {
	char 	name[25];
	int		type;
	char	loc[256];
	char	original_loc[256];
} drive_item;

/* drive_item items[] = {
	{
		.name = "bin",
		.type = AFS_BLOCK_TYPE_DIRECTORY,
		.loc = "/"
	},
	{
		.name = "lib",
		.type = AFS_BLOCK_TYPE_DIRECTORY,
		.loc = "/"
	},
	{
		.name = "etc",
		.type = AFS_BLOCK_TYPE_DIRECTORY,
		.loc = "/"
	},
	{
		.name = "libmyshare.so",
		.type = AFS_BLOCK_TYPE_FILE,
		.loc = "/lib/",
		.original_loc = "/usr/local/osdev/versions/v/support/libtest/libmyshare.so"
	},
	{
		.name = "picard_history.txt",
		.type = AFS_BLOCK_TYPE_FILE,
		.loc = "/lib/",
		.original_loc = "/usr/local/osdev/versions/v/support/afsdrive/picardhistory.txt"
	},
	{
		.name = "magic_key",
		.type = AFS_BLOCK_TYPE_FILE,
		.loc = "/etc/",
		.original_loc = "/usr/local/osdev/versions/v/support/afsdrive/magic_key"
	},
	{
		.name = "cat",
		.type = AFS_BLOCK_TYPE_FILE,
		.loc = "/bin/",
		.original_loc = "/usr/local/osdev/versions/v/support/apptest/myapp"
	},
	{
		.name = "vera.sfn",
		.type = AFS_BLOCK_TYPE_FILE,
		.loc = "/etc/",
		.original_loc = "/usr/local/osdev/versions/v/support/afsdrive/vera.sfn"
	}
}; */

#define MAX_DRIVE_ITEMS 256

uint32_t drive_size_in_bytes = 1024 * 1024 * 2;
char str_test[] = "Hello, world!";
drive_item items[ MAX_DRIVE_ITEMS ];
int	drive_item_count = 0;

afs_drive * bs_drive;
afs_string_table * bs_string_table;
afs_block_directory *bs_root_dir;
afs_block_directory *bs_bin_dir;

uint8_t *afs_data_root;
afs_block_directory *afs_root_dir;
afs_string_table *afs_main_string_table;
afs_drive *drive;
afs_inode afs_inodes;
afs_inode *afs_inodes_tail;
inode_id afs_id_top;



int afs_initalize( uint8_t *data_root ) {
	vfs_filesystem *afs;

	afs = vfs_register_fs( afs );

	afs->op.mount = afs_mount;

	return 0;
}

int afs_mount( inode_id id, char *path, uint8_t *data_root ) {
	afs_data_root = data_root;
	drive = (afs_drive *)afs_data_root;
	afs_root_dir = (afs_block_directory *)(afs_data_root + drive->root_directory);
	afs_main_string_table = (afs_string_table *)(afs_data_root + sizeof(afs_drive) );

	afs_inodes.id = id;
	afs_inodes.block_offset = drive->root_directory;
	strcpy( afs_inodes.name, path );
	afs_inodes.next = NULL;
	afs_inodes_tail = &afs_inodes;

	afs_load_directory_as_inodes( id, afs_root_dir );

	return 0;
}

int afs_load_directory_as_inodes( inode_id parent_inode, afs_block_directory *dir ) {
	for( int i = 0; i < afs_root_dir->next_index; i++ ) {
		afs_inodes_tail->next = vfs_malloc( sizeof(afs_inodes) );
		afs_inodes_tail = afs_inodes_tail->next;
		
		vfs_inode *inode = vfs_allocate_inode();
		inode->fs_type = FS_TYPE_AFS;
		
		afs_generic_block gen_block;
		vfs_disk_read( 1, dir->index[i].start, sizeof(afs_generic_block), &gen_block );
		switch( gen_block.type ) {
			case AFS_BLOCK_TYPE_DIRECTORY:
				inode->type = VFS_INODE_TYPE_DIR;
				break;
			case AFS_BLOCK_TYPE_FILE:
				inode->type = VFS_INODE_TYPE_FILE;
				break;
		}

		afs_inodes_tail->id = inode->id;
		afs_inodes_tail->block_offset = dir->index[i].start;
		strcpy( afs_inodes_tail->name, afs_main_string_table->string[dir->index[i].name_index] );

		afs_inodes_tail->next = NULL;
	}
}

/**
 * @brief Returns a list of files in the given directory
 * 
 * @param id 
 * @param list 
 * @return vfs_directory_list* 
 */
vfs_directory_list *afs_dir_list( inode_id id, vfs_directory_list *list ) {
	afs_inode *inode = rfs_lookup_by_inode_id( id );

	if( inode == NULL ) {
		vfs_debugf( "afs inode not found.\n" );
		return NULL;
	}

	if( inode->type != VFS_INODE_TYPE_DIR ) {
		vfs_debugf( "afs inode is not a direcotry.\n" );
		return NULL;
	}

	if( dir->dir_list == NULL ) {
		vfs_debugf( "dir_list in rfs_file is NULL for id %ld.\n", id );
		return NULL;
	}

	afs_block_directory dir_block;
	vfs_disk_read( 1, inode->block_offset, sizeof(afs_block_directory), &dir_block );
	
	for( int i = 0; i < dir_block.next_index; i++ ) {
		afs_inodes_tail->next = vfs_malloc( sizeof(afs_inodes) );
		afs_inodes_tail = afs_inodes_tail->next;
		
		vfs_inode *inode = vfs_allocate_inode();
		inode->fs_type = FS_TYPE_AFS;
		
		afs_generic_block gen_block;
		vfs_disk_read( 1, dir->index[i].start, sizeof(afs_generic_block), &gen_block );
		switch( gen_block.type ) {
			case AFS_BLOCK_TYPE_DIRECTORY:
				inode->type = VFS_INODE_TYPE_DIR;
				break;
			case AFS_BLOCK_TYPE_FILE:
				inode->type = VFS_INODE_TYPE_FILE;
				break;
		}

		afs_inodes_tail->id = inode->id;
		afs_inodes_tail->block_offset = dir->index[i].start;
		strcpy( afs_inodes_tail->name, afs_main_string_table->string[dir->index[i].name_index] );

		afs_inodes_tail->next = NULL;
	}

	rfs_file_list *rfs_list = (rfs_file_list *)dir->dir_list;

	list->count = rfs_list->count;
	list->entry = vfs_malloc( sizeof(vfs_directory_item) * list->count );

	rfs_file_list_el *head = rfs_list->head;

	for( int i = 0; i < list->count; i++ ) {
		strcpy( list->entry[i].name, head->file->name );
		list->entry[i].id = head->file->vfs_inode_id;

		head = head->next;
	}
}

/**
 * @brief 
 * 
 * @param id 
 * @return rfs_file* 
 */
afs_inode *afs_lookup_by_inode_id( inode_id id ) {
	afs_inode *inode = &afs_inodes;
	bool found = false;

	do {
		if( inode->id == id ) {
			found = true;
		}

		inode = inode->next;
	} while( (inode != NULL) && !found );

	if( !found ) {
		return NULL;
	}

	return inode;
}




















/**
 * @brief Opens a file, returning FP
 * 
 * @param fs Filesystem object
 * @param filename Name of the file
 * @param mode Mode (libc like): r, w, r+
 * 
 * @return vv_file* Pointer to the file struct
 */
vv_file * afs_fopen( vv_file_internal *fs, const char * filename, const char * mode ) {
	afs_file *f = afs_get_file( fs, filename );
	vv_file *fp;

	// Bail if not found
	if( f == NULL ) {
		return NULL;
	}

	// Use the next available fp
	if( fs->next_fd == MAX_FD - 1 ) {
		return NULL;
	}

	fp = &fs->fd[fs->next_fd];
	fp->fd = fs->next_fd;
	fp->base = (uint8_t *)(f);
	fp->base = fp->base + sizeof(afs_file);
	fp->size = f->file_size;
	fp->position = 0;
	fp->dirty = false;
	fp->name = fs->string_table->string[ f->name_index ];

	//dump_afs_file( fs, f );

	fs->next_fd++;

	return fp;
}

/**
 * @brief Reads data from the given stream
 * 
 * @param fs pointer to the file system object
 * @param ptr pointer to buffer to put read data into
 * @param size number of bytes of each element to read
 * @param nmemb number of elements
 * @param fp pointer to the file pointer object
 * 
 * @return uint32_t number of members read
 */
uint32_t afs_fread( vv_file_internal *fs, void *ptr, uint32_t size, uint32_t nmemb, vv_file *fp ) {
	uint32_t num_read = 0;

	for( int i = 0; i < nmemb; i++ ) {
		if( fp->position + size > fp->size ) {
			i = nmemb + 1;
		}

		memcpy( (ptr + (size * i)), ((uint8_t *)fp->base + fp->position), size );

		fp->position = fp->position + size;
		num_read++;
	}
	
	return num_read;
}

/**
 * @brief Tests if the file exists
 * 
 * @param fs Filesystem object
 * @param filename Name of the file
 * @return bool true if the file exists, otherwise false
 */
uint32_t afs_exists( vv_file_internal *fs, const char * filename ) {
	return afs_get_file( fs, filename ) ? true : false;
}

/**
 * @brief Tests if the file exists
 * 
 * @param fs Filesystem object
 * @param filename Name of the file
 * @return uint32_t byte location of the file's afs_file if found, otherwise 0
 */
uint32_t afs_get_file_location( vv_file_internal *fs, const char *filename ) {
	afs_block_directory *d = fs->root_dir;
	uint32_t loc = 0;

	for( int i = 0; i < d->next_index; i++ ) {
		if( strcmp( fs->string_table->string[ d->index[i].name_index ], filename ) == 0 ) {
			loc = d->index[i].start;
			i = 1000000;
		}
	}

	return loc;
}

/**
 * @brief Returns bytes of file in the given dir
 * 
 * @param fs Filesystem object
 * @param dir Directory object
 * @param filename Name of the file
 * @return uint32_t byte location of the file's afs_file if found, otherwise 0
 */
uint32_t afs_get_file_location_in_dir( vv_file_internal *fs, afs_block_directory *d, const char *filename ) {
	uint32_t loc = 0;

	for( int i = 0; i < d->next_index; i++ ) {
		if( strcmp( fs->string_table->string[ d->index[i].name_index ], filename ) == 0 ) {
			loc = d->index[i].start;
			i = 1000000;
		}
	}

	return loc;
}

/**
 * @brief Returns an afs_file object for the given filename
 * 
 * @param fs Pointer to the filesystem object
 * @param filename Name of the file to retrieve
 * @return afs_file* Pointer to the given file struct
 */
afs_file* afs_get_file( vv_file_internal *fs, const char *filename ) {
	afs_file *file = NULL;
	uint32_t loc;

	loc = afs_get_file_location( fs, filename );

	if( loc == 0 ) {
		return NULL;
	}

	file = (afs_file *)( (uint8_t *)fs->drive + loc );

	return file;
}

/**
 * @brief Dump drive diagnostic data
 * 
 * @param buff Pointer to the buffer containing the drive
 */
void afs_disply_diagnostic_data( uint8_t * buff ) {

	// First dump the drive info

	afs_drive * drive = (afs_drive *)buff;

	printf( "drive.version: %d\n", drive->version );
	printf( "drive.size: %d\n", drive->size );
	printf( "drive.name_index: %d\n", drive->name_index );
	printf( "drive.root_directory: %d\n", drive->root_directory );
	printf( "drive.next_free: %d\n", drive->next_free );
	printf( "\n" );

	// Dump string index

	afs_string_table *st = (afs_string_table *)(buff + sizeof(afs_drive));

	for( int i = 0; i < st->next_free; i++ ) {
		printf( "string_table[%d] = \"%s\"\n", i, st->string[i] );
	}
	printf( "\n" );

	// Dump dir struct

	afs_block_directory * d = (afs_block_directory *)(buff + drive->root_directory);

	printf( "dir.next_index = %d\n", d->next_index );
	printf( "dir.name_index = %d\n", d->name_index );
	
	for( int i = 0; i < d->next_index; i++ ) {
		printf( "dir.index[%d].start = 0x%X\n", i, d->index[i].start );
		printf( "dir.index[%d].name_index = \"%s\"\n", i, st->string[d->index[i].name_index] );
	}
	printf( "\n" );

	// Dump file stucts

	for( int i = 0; i < d->next_index; i++ ) {
		afs_file *f = (afs_file *)(buff + d->index[i].start );

		printf( "f.block_size: %d\n", f->block_size );
		printf( "f.file_size: %d\n", f->file_size );
		printf( "f.name_index = \"%s\"\n", st->string[f->name_index]);
		printf( "\n" );

		printf( "%s\n\n", (char *)((char *)f + sizeof( afs_file )) );
	}
}

void dump_afs_file( vv_file_internal *fs, afs_file *f ) {
	afs_string_table *st = fs->string_table;

	printf( "f.block_size: %d\n", f->block_size );
	printf( "f.file_size: %d\n", f->file_size );
	printf( "f.name_index = \"%s\"\n", st->string[f->name_index]);
	printf( "\n" );

	printf( "%s\n\n", (char *)((char *)f + sizeof( afs_file )) );
}

/**
 * @brief Add name to the string table
 * 
 * @param name Text string to add
 * 
 * @return uint32_t Index that the name was added at
 */
uint32_t afs_add_string( vv_file_internal *fs, char * name ) {
	strcpy( fs->string_table->string[ fs->string_table->next_free ], name );
	fs->string_table->next_free++;

	return fs->string_table->next_free - 1;
}

/**
 * @brief Send list of files to stdout
 * 
 * @param fs Pointer to the file system object
 * @param path Path of directory to list
 */
void afs_ls( vv_file_internal *fs, char *path ) {
	afs_generic_block *block = afs_get_generic_block( fs, path );

	if( block == NULL ) {
		printf( "Directory not found.\n" );
		return;
	}

	if( block->type == AFS_BLOCK_TYPE_DIRECTORY ) {
		afs_block_directory *dir_to_list = (afs_block_directory *)block;

		for( int i = 0; i < dir_to_list->next_index; i++ ) {
			printf( "%s  ", fs->string_table->string[ dir_to_list->index[i].name_index ] );
		}
	} else {
		printf( "Not a directory." );
	}
	
	printf( "\n" );
}

bool afs_mkdir( vv_file_internal *fs, afs_block_directory *d, char *name ) {
	d->index[ d->next_index ].start = fs->drive->next_free;
	d->index[ d->next_index ].name_index = afs_add_string( fs, name );
}

#undef KDEBUG_GET_GENERIC_BLOCK
/**
 * @brief Get the generic block of the item located at filename
 * 
 * @param fs Pointer to the filesystem object
 * @param filename Filename (and path) of the block to get
 * @return afs_generic_block* Generic block, acceptable to cast to given type
 */
afs_generic_block* afs_get_generic_block( vv_file_internal *fs, char *filename ) {
	char full_filename[256];
	int i = 0;
	char item_name[256];
	afs_block_directory *d = fs->root_dir;
	bool keep_going = true;
	bool found = false;
	afs_generic_block *result = NULL;

	memset( full_filename, 0, 256 );

	// If first char isn't /, then it's a relative path
		// pre-pend with working dir
	
	if( filename[0] != '/' ) {
		strcpy( full_filename, fs->working_directory );
		
		int wd_len = strlen( fs->working_directory );

		if( full_filename[wd_len] != '/' ) {
			full_filename[wd_len] = '/';
			full_filename[wd_len + 1] = 0;
		}

		strcat( full_filename, filename );
	} else {
		strcpy( full_filename, filename );
	}

	#ifdef KDEBUG_GET_GENERIC_BLOCK
	printf( "full_filename: \"%s\"\n", full_filename );
	#endif

	// If first char is /, then it's an absolute path
		// iterate over each /
		// result is final iteration
	
	while( full_filename[i] != 0 && keep_going ) {
		memset( item_name, 0, 256 );

		// Copy the current element into item_name
		for( int x = 0; x < 256; x++, i++ ) {
			item_name[x] = full_filename[i];

			if( full_filename[i] == '/' ) {
				x = 256;
			}

			if( full_filename[i] == 0 ) {
				x = 256;
			}
		}

		#ifdef KDEBUG_GET_GENERIC_BLOCK
		printf( "item_name: \"%s\"\n", item_name );
		#endif

		// If item name is /, then continue
		if( strcmp(item_name, "/") == 0 ) {
			d = fs->root_dir;

			// If this is it, then just return the root directory
			if( strcmp(full_filename, "/") == 0 ) {
				return (afs_generic_block *)d;
			}

			found = true;
		} else {
			// Test if item_name exists
			uint32_t loc = afs_exists_in_dir( fs, d, item_name );

			if( loc == 0 ) {
				keep_going = false;
				found = false;
			} else {
				found = true;
				
				#ifdef KDEBUG_GET_GENERIC_BLOCK
				printf( "loc: 0x%X\n", loc );
				#endif

				result = (afs_generic_block *)((uint8_t *)fs->drive + loc );
				if( result->type == AFS_BLOCK_TYPE_DIRECTORY ) {
					d = (afs_block_directory *)result;
				} 

				#ifdef KDEBUG_GET_GENERIC_BLOCK
				printf( "block type: %d\n", result->type );
				#endif
			}
		}
		
		#ifdef KDEBUG_GET_GENERIC_BLOCK
		printf( "found: %d\n", found );
		#endif
	}

	return result;
}

uint32_t afs_exists_in_dir( vv_file_internal *fs, afs_block_directory *d, char *name ) {
	uint32_t result = 0;

	for( int i = 0; i < d->next_index; i++ ) {
		if( strcmp( fs->string_table->string[ d->index[i].name_index ], name ) == 0 ) {
			result = d->index[i].start;
		}
	}

	return result;
}
