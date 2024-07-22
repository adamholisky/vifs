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

	afs->op.mount = afs_mount;
	afs->op.get_dir_list = afs_dir_list;

	afs_data_root = data_root;
	drive_size = drive_size_in_bytes;

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
	#ifdef VIFS_DEV
		uint64_t offset = block_id * drive->block_size;
		return (afs_data_root + offset);
	#else
		// OS code goes here
	#endif
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
	block_meta_data = (afs_block_meta_data *)vfs_disk_read( 1, sizeof(afs_drive), length_of_block_meta, (uint8_t *)block_meta_data );

	afs_load_directory_as_inodes( id, afs_root_dir );

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
		afs_load_block_as_inode( &block_meta_data[dir->next_index] );
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
		afs_inodes_tail->next = vfs_malloc( sizeof(afs_inodes) );
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

	for( int i = 0; i < list->count; i++ ) {
		strcpy( list->entry[i].name, block_meta_data[dir_block->index[i]].name );
		list->entry[i].id = afs_find_inode_from_block_id( block_meta_data[dir_block->index[i]].id );
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
		}

		inode = inode->next;
	} while( (inode != NULL) && !found );

	if( !found ) {
		return NULL;
	}

	return inode;
}

