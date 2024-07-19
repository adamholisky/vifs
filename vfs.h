#if !defined(VFS_V2_INCLUDED)
#define VFS_V2_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

#define VIFS_DEV

// We define function aliases here to support multiple systems
#ifdef VIFS_DEV
	#define vfs_malloc malloc
	#define vfs_panic printf
	#define vfs_debugf printf
#else
	#define vfs_malloc kmalloc
#endif

typedef uint64_t inode_id;

#define VFS_VERSION 2

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

} vfs_directory;

typedef struct {
	uint8_t type;   // Type of inode
	uint8_t fs_type;	// File system that controls this inode
	inode_id id;	// inode id
	bool is_mount_point;	// true if this is a mount point for a fs
} vfs_inode;

typedef struct {
	uint8_t type;			// type of fs

	inode_id mount_inode;	// inode id of where the file system is mounted


	vfs_operations op;

	void *next_fs; // used in vfs code to link to the next FS
} vfs_filesystem;

/**
 * @brief Operations to use for the given VFS
 * 
 * inode_number create( char *name, uint8_t type, vfs_directory * )   Creates an inode of type in the given directory
 * int read( int inode_number, uint8_t *buffer, uint64_t size )   Reads size bytes into buffer from inode_number
 * int write( int inode_number, uint8_t *buffer, uint64_t size )   Writes size bytes from buffer into inode_number
 * inode_number open( char *path )  Opens an inode at path
 * void close( int inode_number )  Closes the inode number
 */

typedef struct {
	int (*create)( char *, uint8_t, vfs_directory * );
	int (*read)( int, uint8_t *, uint64_t );
	int (*write)( int, uint8_t *, uint64_t );
	int (*mount)( inode_id, uint8_t * );
	int (*open)( inode_id );
	void (*close)( int );
} vfs_operations;

int vfs_register_fs( vfs_filesystem *fs );
vfs_filesystem *vfs_get_fs( uint8_t fs_type );
int vfs_mount( uint8_t fs_type, uint8_t *data, char *path );

#ifdef __cplusplus
}
#endif

#endif