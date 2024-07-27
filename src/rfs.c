#include "vfs.h"
#include "rfs.h"

rfs_file_list rfs_files;

/**
 * @brief Initalizes the Ram File System
 * 
 * @return int VFS_ERROR_NONE on success, otherwise VFS_ERROR_ on failure
 */
int rfs_initalize( void ) {
	vfs_filesystem *rfs;

	int reg_err = vfs_register_fs( &rfs );

	if( reg_err != 0 ) {
		vfs_debugf( "Register FS error: %d\n", reg_err );
		return reg_err;
	}

	rfs->op.open = rfs_open;
	rfs->op.mount = rfs_mount;
	rfs->op.create = rfs_create;
	rfs->op.write = rfs_write;
	rfs->op.read = rfs_read;
	rfs->op.get_dir_list = rfs_dir_list;
	rfs->op.stat = rfs_stat;

	rfs_files.head = NULL;
	rfs_files.tail = NULL;
	rfs_files.count = 0;

	return VFS_ERROR_NONE;
}

/**
 * @brief Opens an RFS file for use
 * 
 * @param id 
 * @return int VFS_ERROR_NONE on success, VFS_ERROR_ on failure
 */
int rfs_open( inode_id id ) {

	return VFS_ERROR_NONE;
}

/**
 * @brief Returns statistics for the given file
 * 
 * @param id 
 * @param stat 
 * @return int VFS_ERROR_NONE on success, VFS_ERROR_ on failure
 */
int rfs_stat( inode_id id, vfs_stat_data *stat ) {
	rfs_file *f = rfs_lookup_by_inode_id( id );

	if( f == NULL ) {
		//vfs_debugf( "f is null in rfs_stat.\n" );
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	stat->size = f->size;

	return VFS_ERROR_NONE;
}

/**
 * @brief Mounts the RFS fs
 * 
 * @param id 
 * @param data_root 
 * @return int VFS_ERROR_NONE if successful, otherwise VFS_ERROR_
 */
int rfs_mount( inode_id id, char *path, uint8_t *data_root ) {
	rfs_files.count = 1;
	rfs_files.head = vfs_malloc( sizeof(rfs_file_list_el) );

	if( rfs_files.head == NULL ) {
		return VFS_ERROR_MEMORY;
	}

	rfs_files.tail = rfs_files.head;

	rfs_files.head->file = vfs_malloc( sizeof(rfs_file) );

	if( rfs_files.head->file == NULL ) {
		return VFS_ERROR_MEMORY;
	}

	rfs_files.head->file->rfs_file_type = RFS_FILE_TYPE_DIR;
	rfs_files.head->file->vfs_inode_id = id;
	rfs_files.head->file->vfs_parent_inode_id = 0;
	strcpy( rfs_files.head->file->name, "/" );

	rfs_file_list *root_dir_list = vfs_malloc( sizeof(rfs_file_list) );

	if( root_dir_list == NULL ) {
		return VFS_ERROR_MEMORY;
	}

	root_dir_list->head = NULL;
	root_dir_list->tail = NULL;
	root_dir_list->count = 0;
	rfs_files.head->file->dir_list = (void *)root_dir_list;

	return VFS_ERROR_NONE;
}

/**
 * @brief Creates an inode
 * 
 * @param type 
 * @param parent 
 * @param path 
 * @param name 
 * @return int inode id on success, otherwise VFS_ERROR_ on failure
 */
int rfs_create( inode_id parent, uint8_t type, char *path, char *name ) {
	// Allocate a VFS inode for this object, fill in details
	vfs_inode *node = vfs_allocate_inode();
	node->fs_type = FS_TYPE_RFS;
	node->type = type;

	// Allocate a RFS file, fill in details
	rfs_file *f = vfs_malloc( sizeof(rfs_file) );
	f->vfs_inode_id = node->id;
	f->size = 0;
	f->vfs_parent_inode_id = parent;
	strcpy( f->name, name );

	switch( type ) {
		case VFS_INODE_TYPE_DIR:
			f->rfs_file_type = RFS_FILE_TYPE_DIR;
			break;
		case VFS_INODE_TYPE_FILE:
			f->rfs_file_type = RFS_FILE_TYPE_FILE;
			break;
	}

	// If dir, initalize the RFS dir_list
	if( type == VFS_INODE_TYPE_DIR ) {
		rfs_file_list *d_list = vfs_malloc( sizeof(rfs_file_list) );
		f->dir_list = (void *)d_list;
		d_list->count = 0;
		d_list->head = NULL;
		d_list->tail = NULL;
	} else {
		f->dir_list = NULL;
	}

	// Insert into parent directory
	rfs_file *rfs_parent = rfs_lookup_by_inode_id( parent );
	rfs_file_list *parent_dir = (rfs_file_list *)rfs_parent->dir_list;
	rfs_file_list_el *list_el = vfs_malloc( sizeof(rfs_file_list_el) );
	list_el->next = NULL;
	list_el->file = f;

	if( parent_dir->head == NULL ) {
		parent_dir->head = list_el;
		parent_dir->tail = list_el;
	} else {
		parent_dir->tail->next = list_el;
		parent_dir->tail = list_el;
	}
	parent_dir->count++;

	// Attach completed file to the master list
	rfs_file_list_el *main_file_list_el = vfs_malloc( sizeof(rfs_file_list_el) );
	main_file_list_el->file = f;
	main_file_list_el->next = NULL;
	rfs_files.tail->next = main_file_list_el;
	rfs_files.tail = main_file_list_el;

	return node->id;
}

/**
 * @brief Gets the rfs_file pointer for the given vfs inode id
 * 
 * @param id vfs inode_id
 * @return rfs_file* pointer to rfs_file pointer, NULL on failure
 */
rfs_file *rfs_lookup_by_inode_id( inode_id id ) {
	rfs_file *f = NULL;
	rfs_file_list_el *head = rfs_files.head;
	bool keep_going = true;
	bool found = false;

	do {
		f = head->file;

		if( f->vfs_inode_id == id ) {
			found = true;
		} else if( head->next == NULL ) {
			keep_going = false;
		} else {
			head = head->next;
		}
	} while( keep_going && !found );

	if( !found ) {
		return NULL;
	}

	return f;
}

/**
 * @brief Writes to an RFS file
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset 
 * @return int Bytes written (greater than 0), otherwise VFS_ERROR_ on failure
 */
int rfs_write( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	rfs_file *f = rfs_lookup_by_inode_id( id );

	if( f == NULL ) {
		//vfs_debugf( "rfs_file not found.\n" );
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	// If no size, then it's the first write, so just create the mem and copy the data
	if( f->size == 0 ) {
		f->data = vfs_malloc( size );
		
		if( f->data == NULL ) {
			//vfs_debugf( "Could not allocate space for file.\n" );
			return VFS_ERROR_MEMORY;
		}

		f->size = size;
	}

	// Do a realloc if we don't have enough space
	uint64_t space_to_realloc = 0;

	if( f->size < offset + size ) {
		space_to_realloc = size + offset;
	}

	if( space_to_realloc != 0 ) {
		f->data = vfs_realloc( f->data, space_to_realloc );
	}

	// Copy over the data
	memcpy( (f->data + (uint8_t)offset), data, size );

	return size;
}

/**
 * @brief Reads from the given inode
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset 
 * @return int size of bytes read, otherwise VFS_ERROR_
 */
int rfs_read( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	rfs_file *f = rfs_lookup_by_inode_id( id );

	if( f == NULL ) {
		//vfs_debugf( "rfs_file not found.\n" );
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	memcpy( data + (uint8_t)offset, f->data, size );

	return size;
}

/**
 * @brief Returns a list of files in the given directory
 * 
 * @param id 
 * @param list 
 * @return vfs_directory_list* Directory list on success, NULL on failure
 */
vfs_directory_list *rfs_dir_list( inode_id id, vfs_directory_list *list ) {
	rfs_file *dir = rfs_lookup_by_inode_id( id );

	if( dir == NULL ) {
		//vfs_debugf( "rfs_file not found.\n" );
		return NULL;
	}

	if( dir->rfs_file_type != RFS_FILE_TYPE_DIR ) {
		//vfs_debugf( "rfs_file is not a direcotry.\n" );
		return NULL;
	}

	if( dir->dir_list == NULL ) {
		//vfs_debugf( "dir_list in rfs_file is NULL for id %ld.\n", id );
		return NULL;
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