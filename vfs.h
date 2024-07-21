#if !defined(VFS_V2_INCLUDED)
#define VFS_V2_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef VIFS_OS
	#define VIFS_DEV
#endif

// We define function aliases here to support multiple systems
#ifdef VIFS_DEV
	#define vfs_malloc malloc
	#define vfs_realloc realloc
	#define vfs_panic printf
	#define vfs_debugf printf
#else
	#include <kernel_common.h>

	#define vfs_malloc kmalloc
	#define vfs_realloc krealloc
	#define vfs_panic debugf
	#define vfs_debugf debugf
#endif

typedef uint64_t inode_id;

#define VFS_VERSION 2
#define VFS_NAME_MAX 64

#define FS_TYPE_RFS 0
#define FS_TYPE_AFS 1

#define VFS_INODE_TYPE_FILE 1
#define VFS_INODE_TYPE_DIR 2

#define VFS_ERROR_NONE 0



typedef struct {
	uint64_t pos;   // Position of read/write head
	uint64_t size;  // Size of inode
	uint32_t fd;    // File descriptor 
} vfs_file;

typedef struct {
	inode_id id;
	void *next_dir;
} vfs_directory;

typedef struct {
	uint8_t type;   		// Type of inode
	uint8_t fs_type;		// File system that controls this inode
	inode_id id;			// inode id
	bool is_mount_point;	// true if this is a mount point for a fs
	vfs_directory *dir_inodes;	// used for directory listing

	void *next_inode;		// for vfs management
} vfs_inode;

/**
 * @brief Lists directory item names with associated inode ptr. 
 * 
 * Temporary until much later in development...
 * 
 */
typedef struct {
	char name[VFS_NAME_MAX];
	inode_id id;
	vfs_inode *ptr;
} vfs_directory_item;

typedef struct {
	vfs_directory_item *entry;
	uint16_t count;
} vfs_directory_list;

/**
 * @brief Operations to use for the given VFS
 * 
 * inode_number create( char *name, uint8_t type, vfs_directory * )   Creates an inode of type in the given directory
 * int read( int inode_number, uint8_t *buffer, uint64_t size )   Reads size bytes into buffer from inode_number
 * int write( int inode_number, uint8_t *buffer, uint64_t size, uint64_t offset )   Writes size bytes from buffer into inode_number
 * inode_number open( char *path )  Opens an inode at path
 * void close( int inode_number )  Closes the inode number
 */

typedef struct {
	int (*create)( inode_id, uint8_t, char *, char * );
	int (*read)( inode_id, uint8_t *, uint64_t, uint64_t );
	int (*write)( inode_id, uint8_t *, uint64_t, uint64_t );
	int (*mount)( inode_id, uint8_t * );
	vfs_directory_list * (*get_dir_list)( inode_id, vfs_directory_list * );
	int (*open)( inode_id );
	void (*close)( inode_id );
} vfs_operations;

/**
 * @brief File system representation 
 * 
 */
typedef struct {
	uint8_t type;			// type of fs
	inode_id mount_inode;	// inode id of where the file system is mounted
	vfs_operations op;
	void *next_fs; 			// used in vfs code to link to the next FS
} vfs_filesystem;



int vfs_initalize( void );

vfs_filesystem *vfs_register_fs( vfs_filesystem *fs );
vfs_filesystem *vfs_get_fs( uint8_t fs_type );
int vfs_mount( uint8_t fs_type, uint8_t *data, char *path );
int vfs_create( uint8_t type, char *path, char *name );
int vfs_write( inode_id id, uint8_t *data, uint64_t size, uint64_t offset );
int vfs_read( inode_id id, uint8_t *data, uint64_t size, uint64_t offset );
int vfs_mkdir( inode_id parent, char *path, char *name );
inode_id vfs_lookup_inode( char *pathname );
vfs_inode *vfs_lookup_inode_ptr( char *pathname );
vfs_inode *vfs_lookup_inode_ptr_by_id( inode_id id );
vfs_inode *vfs_allocate_inode( void );
vfs_directory_list *vfs_get_directory_list( inode_id id, vfs_directory_list *list );
inode_id vfs_get_from_dir( inode_id id, char *name );

void vfs_test_create_file( char *path, char *name, uint8_t *data, uint64_t size );
void vfs_test_ls( char *path );
void vfs_test_cat( char *pathname );


#ifdef __cplusplus
}
#endif

#endif