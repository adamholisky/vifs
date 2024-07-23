#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vfs.h"
#include "rfs.h"
#include "afs.h"

vfs_filesystem *file_systems;
vfs_inode root_inode;
vfs_inode *inode_index_tail;
inode_id vfs_inode_id_top;

/**
 * @brief Initalizes the VFS
 * 
 * @return int 
 */
int vfs_initalize( void ) {
	file_systems = NULL;

	root_inode.fs_type = 0;
	root_inode.type = VFS_INODE_TYPE_DIR;
	root_inode.id = 1;
	root_inode.is_mount_point = false;
	root_inode.dir_inodes = vfs_malloc( sizeof(vfs_directory) );
	root_inode.dir_inodes->id = 0;
	root_inode.dir_inodes->next_dir = NULL;

	inode_index_tail = &root_inode;

	vfs_inode_id_top = 2;

	return 0;
}

/**
 * @brief Registers a file system for use
 * 
 * @param fs 
 * @return int 
 */
vfs_filesystem *vfs_register_fs( vfs_filesystem *fs ) {
	fs = vfs_malloc( sizeof(vfs_filesystem) );
	fs->next_fs = NULL;

	vfs_filesystem *root = file_systems;
	if( root == NULL ) {
		file_systems = fs;
	} else {
		while( root->next_fs != NULL ) {
			root = root->next_fs;
		}

		root->next_fs = (void *)fs;
	}

	return fs;
}

/**
 * @brief Returns the file system object of the given type
 * 
 * @param fs_type 
 * @return vfs_filesystem* 
 */
vfs_filesystem *vfs_get_fs( uint8_t fs_type ) {
	vfs_filesystem *fs = file_systems;

	if( fs == NULL ) {
		vfs_panic( "No file systems registered.\n" );

		return NULL;
	}

	while( fs != NULL ) {
		if( fs->type == fs_type ) {
			return fs;
		}

		fs = (vfs_filesystem *)fs->next_fs;
	}

	vfs_panic( "Could not locate file system type: %d\n", fs_type );

	return NULL;
}

/**
 * @brief Mounts a file system at the given location
 * 
 * @param fs_type 
 * @param data 
 * @param path 
 * @return int 
 */
int vfs_mount( uint8_t fs_type, uint8_t *data, char *path ) {
	vfs_filesystem *fs = vfs_get_fs( fs_type );
	vfs_inode *mount_point = NULL;

	mount_point = vfs_lookup_inode_ptr( path );
	
	if( mount_point == NULL ) {
		vfs_debugf( "Path \"%s\" not found, mount failed.\n", path );
		return -1;
	}

	if( mount_point->is_mount_point ) {
		if( mount_point->fs_type != 0 ) {
			vfs_debugf( "Mount point \"%s\" already claimed with fs_type %d\n", path, mount_point->type );
			return -1;
		}
	}

	if( mount_point->type != VFS_INODE_TYPE_DIR ) {
		vfs_debugf( "Mount point \"%s\" is not a directory.\n", path );
		return -1;
	}
	
	mount_point->fs_type = fs_type;
	mount_point->is_mount_point = true;

	return fs->op.mount( mount_point->id, path, data );
}

/**
 * @brief Gets the inode id of path
 * 
 * @param path 
 * @return inode_id 
 */
inode_id vfs_lookup_inode( char *pathname ) {
	vfs_inode *node = vfs_lookup_inode_ptr( pathname );

	if( node != NULL ) {
		return node->id;
	}
	
	return 0;
}

/**
 * @brief Gets inode structure from inode id
 * 
 * @param id 
 * @return vfs_inode* 
 */
vfs_inode *vfs_lookup_inode_ptr_by_id( inode_id id ) {
	vfs_inode *ret_val = NULL;
	bool found = false;
	vfs_inode *head = &root_inode;

	do {
		if( head->id == id ) {
			ret_val = head;
			found = true;
		}

		head = head->next_inode;
	} while( head != NULL && !found );

	return ret_val;
}

/**
 * @brief Returns a vfs_inode object represented by path
 * 
 * @param pathname ABSOLUTE path to look up
 * @return vfs_inode* 
 */
vfs_inode *vfs_lookup_inode_ptr( char *pathname ) {
	vfs_inode *ret_val = NULL;
	uint32_t path_length = strlen( pathname );

	if( strcmp( pathname, "/" ) == 0 ) {
		return &root_inode;
	}

	// Break up path by seperator
	// Test each element in turn
	// Return last element
	
	bool keep_going = true;
	inode_id parent_inode_id = 0;
	char *c = pathname;
	int element_index = 0;
	int path_index = 0;
	char name[VFS_NAME_MAX];
	memset( name, 0, VFS_NAME_MAX );

	do {	
		if( *c != '/' && *c != 0 ) {
			// build the element

			name[element_index] = *c;
			element_index++;
		} else {
			// done building the element, check for it
			//vfs_debugf( "Element: \"%s\"\n", name );

			if( element_index == 0 ) {
				parent_inode_id = 1;
				// ignore root dir
			} else {
				parent_inode_id = vfs_get_from_dir( parent_inode_id, name );
				if( parent_inode_id != 0 ) {
					// do it again
					memset( name, 0, VFS_NAME_MAX );
					element_index = 0;
				} else {
					// coudln't find it, fail overall
					return NULL;
				}
			}
		}

		c++;
		path_index++;

		if( path_index > path_length ) {
			keep_going = false;
		}
	} while( keep_going );
	
	ret_val = vfs_lookup_inode_ptr_by_id( parent_inode_id );

	return ret_val;
}

/**
 * @brief Create a new inode for use
 * 
 * @return vfs_inode* 
 */
vfs_inode *vfs_allocate_inode( void ) {
	vfs_inode *node = vfs_malloc( sizeof(vfs_inode) );

	node->fs_type = 0;
	node->id = vfs_inode_id_top++;
	node->type = 0;
	node->is_mount_point = false;

	inode_index_tail->next_inode = node;
	inode_index_tail = node;

	return node;
}

/**
 * @brief Creates an inode of type at path with name
 * 
 * @param type 
 * @param path 
 * @param name 
 * @return int 
 */
int vfs_create( uint8_t type, char *path, char *name ) {
	vfs_inode *parent_node = vfs_lookup_inode_ptr( path );

	if( parent_node == NULL ) {
		vfs_debugf( "Cannot find path \"%s\"", path );
		return -1;
	}

	if( parent_node->type != VFS_INODE_TYPE_DIR ) {
		vfs_debugf( "Cannot create at \"%s\", not a directory.\n", path );
		return -1;
	}

	vfs_filesystem *fs = vfs_get_fs( parent_node->fs_type );

	return fs->op.create( parent_node->id, type, path, name );
}

/**
 * @brief Writes data to inode id startign at offset for size bytes
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset 
 * @return int number of bytes written, -1 if error
 */
int vfs_write( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	vfs_inode *node = vfs_lookup_inode_ptr_by_id( id );

	if( node == NULL ) {
		vfs_debugf( "inode ID %ld not found, aborting write.\n", id );
		return -1;
	}

	vfs_filesystem *fs = vfs_get_fs( node->fs_type );

	return fs->op.write( id, data, size, offset );
}

/**
 * @brief Reads size bytes from inode id + offset into data
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset
 * @return int 
 */
int vfs_read( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	vfs_inode *node = vfs_lookup_inode_ptr_by_id( id );

	if( node == NULL ) {
		vfs_debugf( "inode ID %ld not found, aborting read.\n", id );
		return -1;
	}

	vfs_filesystem *fs = vfs_get_fs( node->fs_type );

	return fs->op.read( id, data, size, offset );
}

/**
 * @brief Creates a diretory
 * 
 * @param parent 
 * @param path 
 * @param name 
 * @return int 
 */
int vfs_mkdir( inode_id parent, char *path, char *name ) {
	return vfs_create( VFS_INODE_TYPE_DIR, path, name );
}

/**
 * @brief Returns each file in the provided directory
 * 
 * @param id 
 * @param list 
 * @return vfs_directory_list* 
 */
vfs_directory_list *vfs_get_directory_list( inode_id id, vfs_directory_list *list ) {
	vfs_inode *dir = vfs_lookup_inode_ptr_by_id( id );

	if( dir == NULL ) {
		vfs_debugf( "could not find inode %ld for directory listing.\n", id );
		return NULL;
	}

	if( dir->type != VFS_INODE_TYPE_DIR ) {
		vfs_debugf( "indoe ID %ld is not a directory.\n", id );
		return NULL;
	}

	vfs_filesystem *fs = vfs_get_fs( dir->fs_type );

	return fs->op.get_dir_list( id, list );
}

/**
 * @brief Gets the inode id of file name from the parent dir
 * 
 * @param parent_id parent inode id
 * @param name name of the file 
 * @return inode_id id of the found file, or 0 if failed
 */
inode_id vfs_get_from_dir( inode_id id, char *name ) {
	vfs_directory_list list;
	vfs_get_directory_list( id, &list );

	for( int i = 0; i < list.count; i++ ) {
		if( strcmp(list.entry[i].name, name ) == 0 ) {
			return list.entry[i].id;
		}
	}

	return 0;
}

extern FILE *fp;

/**
 * @brief Simulate a disk read
 * 
 * @param drive
 * @param data 
 * @param offset 
 * @param length 
 */
uint8_t *vfs_disk_read_test( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	fseek( fp, offset, SEEK_SET );
	
	int read_err = fread( data, length, 1, fp );

	if( read_err != 1 ) {
		vfs_debugf( "vfs_disk_read_test: fread failed.\n" );
	}

	return data;
}

/**
 * @brief Simulate a disk write
 * 
 * @param drive 
 * @param offset 
 * @param length 
 * @param data 
 * @return uint8_t* 
 */
uint8_t *vfs_disk_write_test( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	fseek( fp, offset, SEEK_SET );

	int write_err = fwrite( data, length, 1, fp );

	if( write_err != 1 ) {
		vfs_debugf( "vfs_disk_write_test: fwrite failed.\n" );
	}

	fflush(fp);

	return data;
}
