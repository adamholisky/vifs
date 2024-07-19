#if !defined(VFS_V2_INCLUDED)
#define VFS_V2_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

#define VFS_VERSION 2

#define VFS_INODE_TYPE_FILE 1
#define VFS_INODE_TYPE_DIR 2

typedef struct {

} vfs_file;

typedef struct {

} vfs_directory;

typedef struct {
    uint8_t type;   // Type of inode
    uint64_t pos;   // Position of read/write head
    uint64_t size;  // Size of inode
    uint32_t fd;    // File descriptor 
    void *vfs_object; // Pointer to the vfs struct representing the inode
    void *fs_object; // Pointer to the underlying file system object representing the inode
} vfs_inode;

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
    int (*open)( char * );
    void (*close)( int );
} vfs_operations;

#ifdef __cplusplus
}
#endif

#endif