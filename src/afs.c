#include "vfs.h"
#include "afs.h"

afs_drive *drive;
afs_block_meta_data *block_meta_data;
afs_block_directory *afs_root_dir;
afs_inode afs_inodes;
afs_inode *afs_inodes_tail;
inode_id afs_id_top;

/**
 * @brief Initalize the AFS primatives
 * 
 * @return int VFS_ERROR_NONE on success, otherwise VFS_ERROR_ on failure
 */
int afs_initalize( void ) {
	vfs_filesystem *afs;

	int reg_err = vfs_register_fs( &afs );

	if( reg_err != 0 ) {
		vfs_debugf( "Register FS error: %d\n", reg_err );
		return reg_err;
	}

	afs->type = FS_TYPE_AFS;
	afs->op.mount = afs_mount;
	afs->op.get_dir_list = afs_dir_list;
	afs->op.read = afs_read;
	afs->op.write = afs_write;
	afs->op.create = afs_create;
	afs->op.open = afs_open;
	afs->op.stat = afs_stat;

	afs_inodes_tail = &afs_inodes;

	return VFS_ERROR_NONE;
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
	data = vfs_disk_read( 0, (block_id * drive->block_size), size, data );

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
	uint64_t offset = (block_id) * drive->block_size;
	vfs_disk_write( 0, offset, size, data );
}

/**
 * @brief Writes meta data for block_id to disk
 * 
 * @param block_id 
 * @return int VFS_ERROR_NONE on success, otherwise VFS_ERROR_
 */
int afs_write_meta( uint32_t block_id ) {
	uint64_t offset = sizeof(afs_drive);
	offset = offset + (sizeof(afs_block_meta_data) * (block_id));

	vfs_disk_write( 0, offset, sizeof(afs_block_meta_data), (uint8_t *)&block_meta_data[block_id] );

	return VFS_ERROR_NONE;
}

/**
 * @brief Writes directory block at block_id to disk
 * 
 * @param block_id 
 * @param dir 
 * @return int VFS_ERROR_NONE on success, otherwise VFS_ERROR_
 */
int afs_write_directory( uint32_t block_id, afs_block_directory *dir ) {
	uint64_t offset = drive->block_size * (block_id);
	vfs_disk_write( 0, offset, sizeof(afs_block_directory), (uint8_t *)dir );

	return VFS_ERROR_NONE;
}

/**
 * @brief Writes drive info block to disk
 * 
 * @param block_id 
 * @param dir 
 * @return int VFS_ERROR_NONE on success, otherwise VFS_ERROR_
 */
int afs_write_drive_info( afs_drive *drive_info ) {
	vfs_disk_write( 0, 0, sizeof(afs_drive), (uint8_t *)drive_info );

	return VFS_ERROR_NONE;
}


/**
 * @brief Mount the given AFS drive
 * 
 * @param id 
 * @param path 
 * @param data_root 
 * @return int VFS_ERROR_NONE if successful, otherwise VFS_ERROR_
 */
int afs_mount( inode_id id, char *path, uint8_t *data_root ) {
	drive = vfs_malloc( sizeof(afs_drive) );

	if( drive == NULL ) {
		return VFS_ERROR_MEMORY;
	}

	drive = (afs_drive *)vfs_disk_read( 0, 0, sizeof(afs_drive), (uint8_t *)drive );

	afs_inodes_tail->block_id = drive->root_directory;
	afs_inodes_tail->vfs_id = id;
	afs_inodes_tail->next = NULL;

	afs_root_dir = vfs_malloc( sizeof(afs_block_directory) );

	if( afs_root_dir == NULL ) {
		return VFS_ERROR_MEMORY;
	}

	afs_root_dir = (afs_block_directory *)afs_read_block(drive->root_directory, sizeof(afs_block_directory), (uint8_t *)afs_root_dir);

	uint32_t length_of_block_meta = drive->block_count * sizeof(afs_block_meta_data);
	block_meta_data = vfs_malloc( length_of_block_meta );

	if( block_meta_data == NULL ) {
		return VFS_ERROR_MEMORY;
	}

	vfs_debugf( "length_of_block_meta: %ld\n", length_of_block_meta );

	vfs_disk_read( 1, sizeof(afs_drive), length_of_block_meta, (uint8_t *)block_meta_data );

	afs_load_directory_as_inodes( id, afs_root_dir );

	return VFS_ERROR_NONE;
}

/**
 * @brief Reads the given afs file
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset 
 * @return int number of bytes read, otherwise VFS_ERROR_
 */
int afs_read( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	afs_inode *inode = afs_lookup_by_inode_id( id );

	if( inode == NULL ) {
		//vfs_debugf( "read: inode returned NULL. Aborting.\n" );
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	uint64_t final_offset = inode->block_id * drive->block_size;
	final_offset = final_offset + offset;

	vfs_disk_read( 0, final_offset, size, data );

	return size;
}

/**
 * @brief Create a file with the given parent
 * 
 * @param parent 
 * @param type 
 * @param path 
 * @param name 
 * @return int inode id on success (greater than 0), otherwise VFS_ERROR_ on failure
 */
int afs_create( inode_id parent, uint8_t type, char *path, char *name ) {
	afs_inode *parent_inode = afs_lookup_by_inode_id( parent );

	// Setup the inode
	vfs_inode *vfs_inode_data = vfs_allocate_inode();
	vfs_inode_data->fs_type = FS_TYPE_AFS;
	vfs_inode_data->is_mount_point = false;
	vfs_inode_data->type = type;
	vfs_inode_data->next_inode = NULL;

	//vfs_debugf( "inode id: %d\n", vfs_inode_data->id );

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
		block_data = &file;
		block_data_size = sizeof(afs_file);

		block_meta_data[ block_to_use ].file_size = 0;
		block_meta_data[ block_to_use ].starting_block = block_to_use;
		block_meta_data[ block_to_use ].num_blocks = 1;
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
	afs_write_drive_info( drive );

	return vfs_inode_data->id;
}

/**
 * @brief Write to the given file
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset 
 * @return int number of bytes written
 */
int afs_write( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	afs_inode *node = afs_lookup_by_inode_id( id );

	if( block_meta_data[node->block_id].block_type != AFS_BLOCK_TYPE_FILE ) {
		//vfs_debugf( "Block is not a file.\n" );
		return VFS_ERROR_NOT_A_FILE;
	}

	// TODO: Current assumes we're only writing full files starting at offset 0
	block_meta_data[node->block_id].file_size = size;
	block_meta_data[node->block_id].num_blocks = 1 + (size/drive->block_size);

	// TODO: Current assumes we're writing immediately at the blocks right after the first one
	if( block_meta_data[node->block_id].num_blocks != 1 ) {
		for( int i = 1; i < block_meta_data[node->block_id].num_blocks; i++ ) {
			block_meta_data[drive->next_free].block_type = AFS_BLOCK_TYPE_FILE;
			block_meta_data[drive->next_free].in_use = true;
			afs_write_meta( drive->next_free );

			drive->next_free++;
		}
	}

	afs_write_meta( node->block_id );
	afs_write_drive_info( drive );
	vfs_disk_write( 0, drive->block_size * node->block_id, size, data );

	return size;
}


/**
 * @brief Load everything in a directory as a vfs_inode, if it hasn't been done already
 * 
 * @param parent_inode 
 * @param dir 
 * @return int VFS_ERROR_NONE on success, other error code
 */
int afs_load_directory_as_inodes( inode_id parent_inode, afs_block_directory *dir ) {
	for( int i = 0; i < dir->next_index; i++ ) {
		//vfs_debugf( "load dir: index=%d\n", dir->index[i] );
		afs_load_block_as_inode( &block_meta_data[dir->index[i]] );
	}

	return VFS_ERROR_NONE;
}

/**
 * @brief Load the given block as a vfs_inode, if it hasn't been done already
 * 
 * @param meta 
 * @return int vfs inode id on success, 0 on failure
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
 * @return vfs_directory_list* directory list on success, NULL on failure
 */
vfs_directory_list *afs_dir_list( inode_id id, vfs_directory_list *list ) {
	afs_inode *afs_ino = afs_lookup_by_inode_id( id );

	if( afs_ino == NULL ) {
		//vfs_debugf( "afs inode not found.\n" );
		return NULL;
	}

	if( block_meta_data[afs_ino->block_id].block_type != AFS_BLOCK_TYPE_DIRECTORY ) {
		//vfs_debugf( "afs inode is not a direcotry.\n" );
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
 * @brief Lookup an afs_inode by VFS inode id
 * 
 * @param id VFS inode id
 * @return afs_file* Pointer to the AFS inode struct, NULL on failure
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

/**
 * @brief Opens the inode id for use
 * 
 * @param id 
 * @return int 0 on success, VFS_ERROR_ on failure
 */
int afs_open( inode_id id ) {
	afs_inode *inode = afs_lookup_by_inode_id( id );

	if( inode == NULL ) {
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	inode->open = true;

	return VFS_ERROR_NONE;
}

/**
 * @brief Returns stats on given inode
 * 
 * @param id id of inode
 * @param stat pointer to already allocated vfs_stat_data struct
 * @return int VFS_ERROR_NONE on success, VFS_ERROR_ on failure
 */
int afs_stat( inode_id id, vfs_stat_data *stat ) {
	afs_inode *inode = afs_lookup_by_inode_id( id );

	if( inode == NULL ) {
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	stat->size = block_meta_data[ inode->block_id ].file_size;

	return VFS_ERROR_NONE;
}

/**
 * @brief Displays diagnostic data about the drive
 * 
 */
void afs_dump_diagnostic_data( void ) {
	//vfs_debugf( "    \n", dd_drive-> );

	// Drive Info
	afs_drive *dd_drive = vfs_malloc( sizeof(afs_drive) );
	vfs_disk_read( 0, 0, sizeof(afs_drive), (uint8_t *)dd_drive );
	vfs_debugf( "afs_drive:\n" );
	vfs_debugf( "    magic: \"%c%c%c%c\"\n", dd_drive->magic[0], dd_drive->magic[1], dd_drive->magic[2], dd_drive->magic[3]);
	vfs_debugf( "    version: %d\n", dd_drive->version );
	vfs_debugf( "    size: %ld\n", dd_drive->size );
	vfs_debugf( "    block_size: %d\n", dd_drive->block_size );
	vfs_debugf( "    block_count: %d\n", dd_drive->block_count );
	vfs_debugf( "    root_directory: %d\n", dd_drive->root_directory );
	vfs_debugf( "    next_free: %d\n", dd_drive->next_free );
	vfs_debugf( "\n" );

	// Meta data 
	afs_block_meta_data *dd_meta_data = vfs_malloc( sizeof(afs_block_meta_data) );
	for( int i = 0; i < dd_drive->block_count; i++ ) {
		uint64_t offset = sizeof(afs_drive) + (sizeof(afs_block_meta_data) * i );
		vfs_disk_read( 0, offset, sizeof(afs_block_meta_data), (uint8_t *)dd_meta_data );

		if( dd_meta_data->in_use == true && dd_meta_data->block_type != AFS_BLOCK_TYPE_META && dd_meta_data->file_size != 0 ) {
			vfs_debugf( "afs_block_meta_data for block %d\n", dd_meta_data->id );
			vfs_debugf( "    block_type: %d\n", dd_meta_data->block_type );
			vfs_debugf( "    name: \"%s\"\n", dd_meta_data->name );

			if( dd_meta_data->block_type == AFS_BLOCK_TYPE_FILE ) {
				vfs_debugf( "    starting_block: %d\n", dd_meta_data->starting_block );
				vfs_debugf( "    size: %d\n", dd_meta_data->file_size );
				vfs_debugf( "    num_blocks: %d (expected: %d)\n", dd_meta_data->num_blocks, 1 + (dd_meta_data->file_size / dd_drive->block_size) );
			}
		}
	}
	vfs_debugf( "\n" );

	// Root Directory
	afs_block_directory *dd_root_dir = vfs_malloc( sizeof(afs_block_directory) );
	vfs_disk_read( 0, dd_drive->block_size * dd_drive->root_directory, sizeof(afs_block_directory), (uint8_t *)dd_root_dir );
	vfs_debugf( "Root Directory:\n" );
	vfs_debugf( "    type: %d\n", dd_root_dir->type );
	vfs_debugf( "    next_index: %d\n", dd_root_dir->next_index );
	
	for( int i = 0; i < dd_root_dir->next_index; i++ ) {
		if( dd_root_dir->index[i] != 0 ) {
			vfs_debugf( "    index[%d] = %d\n", i, dd_root_dir->index[i] );
		}
	}
	vfs_debugf( "\n" );
}

/**********************************************/
/* Everything below here for vifs dev tooling */
/**********************************************/

#ifdef VIFS_DEV
/**
 * @brief Boostrap function for vifs tool
 * 
 * @param fp 
 * @param size 
 */
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
	bs_drive.next_free = meta_blocks + 2;
	// Setup root directory
	afs_block_directory root_dir;
	root_dir.type = AFS_BLOCK_TYPE_DIRECTORY;
	root_dir.next_index = 0;

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
#endif