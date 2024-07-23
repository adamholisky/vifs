#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "vfs.h"
#include "afs.h"
#include <sys/stat.h>
#include <dirent.h>

uint8_t *afs_data_root;

afs_drive *drive;
uint64_t drive_size;
afs_block_meta_data *block_meta_data;
afs_block_directory *afs_root_dir;
afs_inode afs_inodes;
afs_inode *afs_inodes_tail;
inode_id afs_id_top;

/**
 * @brief Initalize the AFS primatives
 * 
 * @param drive_size_in_bytes 
 * @param data_root 
 * @return int 
 */
int afs_initalize( uint64_t drive_size_in_bytes, uint8_t *data_root ) {
	vfs_filesystem *afs;

	afs = vfs_register_fs( afs );

	afs->type = FS_TYPE_AFS;
	afs->op.mount = afs_mount;
	afs->op.get_dir_list = afs_dir_list;
	afs->op.read = afs_read;
	afs->op.write = afs_write;
	afs->op.create = afs_create;

	afs_data_root = data_root;
	drive_size = drive_size_in_bytes;

	afs_inodes_tail = &afs_inodes;

	return 0;
}

/**
 * @brief Read a block into memory
 * 
 * @param block_id 
 * @param size 
 * @param data 
 * @return uint8_t* 
 */
uint8_t *afs_read_block( uint32_t block_id, uint64_t size, uint8_t *data ) {
	vfs_disk_read( 0, (block_id * drive->block_size), size, data );

	return data;
}

/**
 * @brief Write a block to disk
 * 
 * @param block_id 
 * @param size 
 * @param data 
 * @return uint8_t* 
 */
uint8_t *afs_write_block( uint32_t block_id, uint64_t size, uint8_t *data ) {
	uint64_t offset = block_id * drive->block_size;
	vfs_disk_write( 0, offset, size, data );
}

/**
 * @brief Writes meta data for block_id to disk
 * 
 * @param block_id 
 * @return int 
 */
int afs_write_meta( uint32_t block_id ) {
	uint64_t offset = sizeof(afs_drive);
	offset = offset + (sizeof(afs_block_meta_data) * (block_id - 1));

	vfs_disk_write( 0, offset, sizeof(afs_block_meta_data), (uint8_t *)&block_meta_data[block_id] );

	return 1;
}

/**
 * @brief Writes directory block at block_id to disk
 * 
 * @param block_id 
 * @param dir 
 * @return int 
 */
int afs_write_directory( uint32_t block_id, afs_block_directory *dir ) {
	uint64_t offset = drive->block_size * (block_id - 1);
	vfs_disk_write( 0, offset, sizeof(afs_block_directory), (uint8_t *)dir );

	return 1;
}

/**
 * @brief Mount the given AFS drive
 * 
 * @param id 
 * @param path 
 * @param data_root 
 * @return int 
 */
int afs_mount( inode_id id, char *path, uint8_t *data_root ) {
	afs_data_root = data_root;
	drive = (afs_drive *)afs_data_root;

	afs_inodes_tail->block_id = drive->root_directory;
	afs_inodes_tail->vfs_id = id;
	afs_inodes_tail->next = NULL;

	afs_root_dir = vfs_malloc( sizeof(afs_block_directory) );
	afs_root_dir = (afs_block_directory *)afs_read_block(drive->root_directory, sizeof(afs_block_directory), (uint8_t *)afs_root_dir);

	uint32_t length_of_block_meta = drive->block_count * sizeof(afs_block_meta_data);
	block_meta_data = vfs_malloc( length_of_block_meta );
	block_meta_data = (afs_block_meta_data *)vfs_disk_read( 1, sizeof(afs_drive), length_of_block_meta, (uint8_t *)block_meta_data );

	afs_load_directory_as_inodes( id, afs_root_dir );

	return 0;
}

/**
 * @brief Reads the given afs file
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset 
 * @return int 
 */
int afs_read( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	afs_inode *inode = afs_lookup_by_inode_id( id );

	vfs_disk_read( 0, offset, size, data );

	return size;
}

/**
 * @brief Create a file with the given parent
 * 
 * @param parent 
 * @param type 
 * @param path 
 * @param name 
 * @return int 
 */
int afs_create( inode_id parent, uint8_t type, char *path, char *name ) {
	afs_inode *parent_inode = afs_lookup_by_inode_id( parent );

	// Setup the inode
	vfs_inode *vfs_inode_data = vfs_allocate_inode();
	vfs_inode_data->fs_type = FS_TYPE_AFS;
	vfs_inode_data->is_mount_point = false;
	vfs_inode_data->type = type;
	vfs_inode_data->next_inode = NULL;

	vfs_debugf( "inode id: %d\n", vfs_inode_data->id );

	afs_inodes_tail->next = vfs_malloc( sizeof(afs_inode) );
	afs_inode *file_inode = (afs_inode *)afs_inodes_tail->next;
	file_inode->next = NULL;
	file_inode->vfs_id = vfs_inode_data->id;
	file_inode->block_id = drive->next_free;
	afs_inodes_tail = file_inode;

	// Find an open block, fill in meta
	uint32_t block_to_use = drive->next_free;
	drive->next_free++;
	block_meta_data[ block_to_use ].in_use = true;
	strcpy( block_meta_data[ block_to_use ].name, name );
	uint32_t afs_type = AFS_BLOCK_TYPE_UNKNOWN;

	// Format the block
	void *block_data = NULL;
	uint32_t block_data_size = 0;
	afs_block_directory dir;
	afs_file file;

	if( type == VFS_INODE_TYPE_DIR ) {
		afs_type = AFS_BLOCK_TYPE_DIRECTORY;

		memset( &dir, 0, sizeof(afs_block_directory) );
		dir.next_index = 0;
		dir.type = AFS_BLOCK_TYPE_DIRECTORY;

		block_data = &dir;
		block_data_size = sizeof(afs_block_directory);
	} else if( type == VFS_INODE_TYPE_FILE ) {
		afs_type = AFS_BLOCK_TYPE_FILE;

		memset( &file, 0, sizeof(afs_file) );
		file.file_size = 0;
		file.type = AFS_BLOCK_TYPE_FILE;

		block_data = &file;
		block_data_size = sizeof(afs_file);
	}

	// Save the block type
	block_meta_data[ block_to_use ].block_type = afs_type;

	// Find directory, fill in index, increment next_index
	afs_block_directory *parent_dir = vfs_malloc( sizeof(afs_block_directory) );
	parent_dir = (afs_block_directory *)afs_read_block( parent_inode->block_id, sizeof(afs_block_directory), (uint8_t *)parent_dir );
	parent_dir->index[parent_dir->next_index] = block_to_use;
	parent_dir->next_index++;
	
	// Write everything to disk
	afs_write_block( block_to_use, block_data_size, (uint8_t *)block_data );
	afs_write_meta( block_to_use );
	afs_write_directory( parent_inode->block_id, parent_dir );

	return vfs_inode_data->id;
}

/**
 * @brief Write to the given file
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset 
 * @return int 
 */
int afs_write( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	afs_inode *file = afs_lookup_by_inode_id( id );

	afs_write_block( file->block_id, size, (data + offset) );

	return 0;
}


/**
 * @brief Load everything in a directory as a vfs_inode, if it hasn't been done already
 * 
 * @param parent_inode 
 * @param dir 
 * @return int 0 on success, other error code
 */
int afs_load_directory_as_inodes( inode_id parent_inode, afs_block_directory *dir ) {
	for( int i = 0; i < dir->next_index; i++ ) {
		//vfs_debugf( "load dir: index=%d\n", dir->index[i] );
		afs_load_block_as_inode( &block_meta_data[dir->index[i]] );
	}

	return 0;
}

/**
 * @brief Load the given block as a vfs_inode, if it hasn't been done already
 * 
 * @param meta 
 * @return int vfs inode id
 */
int afs_load_block_as_inode( afs_block_meta_data *meta ) {
	inode_id ret_val = 0;

	if( meta == NULL ) {
		return 0;
	}

	if( !afs_find_inode_from_block_id(meta->id) ) {
		afs_inodes_tail->next = vfs_malloc( sizeof(afs_inode) );
		afs_inodes_tail = afs_inodes_tail->next;
		afs_inodes_tail->next = NULL;
		
		vfs_inode *inode = vfs_allocate_inode();
		inode->fs_type = FS_TYPE_AFS;

		switch( meta->block_type ) {
			case AFS_BLOCK_TYPE_DIRECTORY:
				inode->type = VFS_INODE_TYPE_DIR;
				break;
			case AFS_BLOCK_TYPE_FILE:
				inode->type = VFS_INODE_TYPE_FILE;
				break;
		}

		afs_inodes_tail->vfs_id = inode->id;
		afs_inodes_tail->block_id = meta->id;

		ret_val = inode->id;
	}

	return ret_val;
}

/**
 * @brief Gets the vfs inode id of a block id, if it has been assigned
 * 
 * @param block_id 
 * @return inode_id inode id if it exists, otherwise 0
 */
inode_id afs_find_inode_from_block_id( uint32_t block_id ) {
	afs_inode *head = &afs_inodes;
	bool found = false;
	inode_id ret_val = 0;

	do {
		if( head->block_id == block_id ) {
			found = true;
			ret_val = head->vfs_id;
		} else {
			head = head->next;
		}
	} while( head != NULL && !found );

	//vfs_debugf( "Returning vfs_id: %d\n", ret_val );

	return ret_val;
}

/**
 * @brief Returns a list of files in the given directory
 * 
 * @param id 
 * @param list 
 * @return vfs_directory_list* 
 */
vfs_directory_list *afs_dir_list( inode_id id, vfs_directory_list *list ) {
	afs_inode *afs_ino = afs_lookup_by_inode_id( id );

	if( afs_ino == NULL ) {
		vfs_debugf( "afs inode not found.\n" );
		return NULL;
	}

	if( block_meta_data[afs_ino->block_id].block_type != AFS_BLOCK_TYPE_DIRECTORY ) {
		vfs_debugf( "afs inode is not a direcotry.\n" );
		return NULL;
	}

	afs_block_directory *dir_block = vfs_malloc( sizeof(afs_block_directory) );
	dir_block = (afs_block_directory *)afs_read_block( afs_ino->block_id, sizeof(afs_block_directory), (uint8_t *)dir_block );
	
	afs_load_directory_as_inodes( id, dir_block );

	list->count = dir_block->next_index;
	list->entry = vfs_malloc( sizeof(vfs_directory_item) * list->count );

	//vfs_debugf( "count: %d\n", list->count );

	for( int i = 0; i < list->count; i++ ) {
		strcpy( list->entry[i].name, block_meta_data[dir_block->index[i]].name );
		list->entry[i].id = afs_find_inode_from_block_id( block_meta_data[dir_block->index[i]].id );

		//vfs_debugf( "i %d: name=\"%s\" id=%d\n", i, list->entry[i].name, list->entry[i].id );
	}

	return list;
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
		if( inode->vfs_id == id ) {
			found = true;
		} else {
			inode = inode->next;
		}
	} while( (inode != NULL) && !found );

	if( !found ) {
		return NULL;
	}

	return inode;
}

void afs_bootstrap( FILE *fp, uint64_t size ) {
	afs_drive bs_drive;

	bs_drive.size = size;
	bs_drive.block_size = AFS_DEFAULT_BLOCK_SIZE;
	bs_drive.block_count = bs_drive.size / bs_drive.block_size;
	bs_drive.magic[0] = 'A';
	bs_drive.magic[1] = 'F';
	bs_drive.magic[2] = 'S';
	bs_drive.magic[3] = ' ';
	bs_drive.version = 2;

	// Calculate the number of meta data blocks
	uint64_t meta_size_in_bytes = sizeof(afs_block_meta_data) * bs_drive.block_count;
	uint64_t meta_blocks = meta_size_in_bytes / bs_drive.block_size;
	meta_blocks++;
	//vfs_debugf( "Block count: %ld\n", bs_drive.block_count );
	//vfs_debugf( "Meta blocks: %ld\n", meta_blocks );

	bs_drive.root_directory = meta_blocks + 1;
	bs_drive.next_free = meta_blocks + 3; // +2 would be for the first file

	// Setup root directory
	afs_block_directory root_dir;
	root_dir.type = AFS_BLOCK_TYPE_DIRECTORY;
	root_dir.index[0] = meta_blocks + 2;
	root_dir.next_index = 1;

	// Write the drive header
	afs_bootstrap_write( fp, (void *)&bs_drive, sizeof(afs_drive) );

	// Write the meta data
	for( int i = 0; i < bs_drive.block_count; i++ ) {
		afs_block_meta_data bs_meta;
		memset( &bs_meta, 0, sizeof(afs_block_meta_data) );
		bs_meta.id = i;
		
		if( i == 0 ) {
			bs_meta.block_type = AFS_BLOCK_TYPE_SYSTEM;
			bs_meta.in_use = true;
		} else if( i <= meta_blocks ) {
			bs_meta.block_type = AFS_BLOCK_TYPE_META;
			bs_meta.in_use = true;
		} else if( i == meta_blocks + 1 ) {
			bs_meta.block_type = AFS_BLOCK_TYPE_DIRECTORY;
			bs_meta.in_use = true;
			strcpy( bs_meta.name, "/" );
		} else if( i == meta_blocks + 2 ) {
			bs_meta.block_type = AFS_BLOCK_TYPE_FILE;
			bs_meta.in_use = true;
			strcpy( bs_meta.name, "magic" );
		} else {
			bs_meta.block_type = AFS_BLOCK_TYPE_NOT_SET;
			bs_meta.in_use = false;
		}

		afs_bootstrap_write( fp, (void *)&bs_meta, sizeof(afs_block_meta_data) );
	}

	// Write the root directory
	fseek( fp, bs_drive.root_directory * bs_drive.block_size, SEEK_SET );
	afs_bootstrap_write( fp, (void *)&root_dir, sizeof(afs_block_directory) );
}

bool afs_bootstrap_write( FILE *fp, void *data, uint64_t size ) {
	uint64_t result = fwrite( data, size, 1, fp );

	if( result != 1 ) {
		vfs_debugf( "fwrite failed!\n" );
		return false;
	}

	return true;
}