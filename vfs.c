#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vfs.h"
#include "rfs.h"

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

	int create_err = vfs_create( VFS_INODE_TYPE_FILE, "/", "test.txt" );
	if( create_err != 0 ) {
		vfs_panic( "Could not create test.txt\n" );
	}



	return 0;
}

/******************************************************************************/
/*        Everything below here should be ran by the kernel at some point     */
/******************************************************************************/

vfs_filesystem *file_systems;
vfs_inode root_inode;
vfs_inode *inode_index_tail;
inode_id vfs_inode_id_top;

/**
 * @brief 
 * 
 * @return int 
 */
int vfs_initalize( void ) {
	file_systems = NULL;

	root_inode.fs_type = 0;
	root_inode.type = VFS_INODE_TYPE_DIR;
	root_inode.id = 1;
	root_inode.is_mount_point = false;

	inode_index_tail = &root_inode;

	return 0;
}

/**
 * @brief 
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

		root->next_fs = fs;
	}

	return fs;
}

/**
 * @brief 
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

		fs->next_fs;
	}

	vfs_panic( "Could not locate file system type: %d\n", fs_type );

	return NULL;
}

/**
 * @brief 
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

	return fs->op.mount( mount_point->id, path );
}

/**
 * @brief Gets the inode id of path
 * 
 * @param path 
 * @return inode_id 
 */
inode_id vfs_lookup_inode( char *path ) {
	vfs_inode *node = vfs_lookup_inode_ptr( path );

	if( node != NULL ) {
		return node->id;
	}
	
	return 0;
}

/**
 * @brief Returns a vfs_inode object represented by path
 * 
 * @param path 
 * @return vfs_inode* 
 */
vfs_inode *vfs_lookup_inode_ptr( char *path ) {
	vfs_inode *ret_val = NULL;

	if( strcmp( path, "/" ) == 0 ) {
		return &root_inode;
	}

	// TODO... (lol)

	return ret_val;
}

/**
 * @brief 
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