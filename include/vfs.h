#if !defined(VFS_V2_INCLUDED)
#define VFS_V2_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef VIFS_OS_ENV
	#define VIFS_DEV
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
#endif

// We define function aliases here to support multiple systems
#ifdef VIFS_DEV
	#define vfs_malloc malloc
	#define vfs_realloc realloc
	#define vfs_free free
	#define vfs_panic printf
	#define vfs_debugf printf
	#define vfs_disk_read vfs_disk_read_test
	#define vfs_disk_read_no_cache vfs_disk_read_test_no_cache
	#define vfs_disk_write vfs_disk_write_test
	#define vfs_disk_write_no_cache vfs_disk_write_test_no_cache
#else
	#include <kernel_common.h>
	#include <kmemory.h>
	#include <ahci.h>

	#define vfs_malloc kmalloc
	#define vfs_realloc krealloc
	#define vfs_free kfree
	#define vfs_panic debugf
	#define vfs_debugf debugf
#endif

typedef uint64_t inode_id;

#define VFS_VERSION 2
#define VFS_NAME_MAX 50

#define FS_TYPE_RFS 0
#define FS_TYPE_AFS 1

#define VFS_INODE_TYPE_FILE 1
#define VFS_INODE_TYPE_DIR 2

#define VFS_ERROR_NONE 0
#define VFS_ERROR_UNKNOWN -1
#define VFS_ERROR_MEMORY -2
#define VFS_ERROR_PATH_NOT_FOUND -3
#define VFS_ERROR_OBJECT_ALREADY_IN_USE -4
#define VFS_ERROR_NOT_A_DIRECTORY -5
#define VFS_ERROR_NOT_A_FILE -6
#define VFS_ERROR_UNKNOWN_FS -7
#define VFS_ERROR_FILE_NOT_FOUND -8

/**
 * @brief Directory list
 * 
 */
typedef struct {
	inode_id id;
	void *next_dir;
} vfs_directory;

/**
 * @brief Inode structure, representing a file/dir/etc on disk
 * 
 */
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

/**
 * @brief List of direct items
 * 
 */
typedef struct {
	vfs_directory_item *entry;
	uint16_t count;
} vfs_directory_list;

/**
 * @brief Stat structure, representing a file
 * 
 */
typedef struct {
	uint32_t size;
} vfs_stat_data;

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
	void (*close)( inode_id );
	int (*create)( inode_id, uint8_t, char *, char * );
	vfs_directory_list * (*get_dir_list)( inode_id, vfs_directory_list * );
	int (*mount)( inode_id, char *, uint8_t * );
	int (*open)( inode_id );
	int (*read)( inode_id, uint8_t *, uint64_t, uint64_t );
	int (*stat)( inode_id, vfs_stat_data *stat_data );
	int (*write)( inode_id, uint8_t *, uint64_t, uint64_t );
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

/**
 * @brief Cache item
 * 
 */
typedef struct {
	uint64_t address;
	uint32_t size;
	uint8_t *data;
	bool dirty;

	uint64_t read_count;
	uint64_t write_count;

	void *next;
} vfs_cache_item;

/**
 * @brief List of cache items
 * 
 */
typedef struct {
	vfs_cache_item *head;
	vfs_cache_item *tail;
} vfs_cache_list;

// Initalizations
int vfs_initalize( void );

// File system management
int vfs_register_fs( vfs_filesystem **fs );
vfs_filesystem *vfs_get_fs( uint8_t fs_type );

// File system operations
int vfs_close( inode_id id );
int vfs_create( uint8_t type, char *path, char *name );
vfs_directory_list *vfs_get_directory_list( inode_id id, vfs_directory_list *list );
int vfs_mkdir( inode_id parent, char *path, char *name );
int vfs_mount( uint8_t fs_type, uint8_t *data, char *path );
int vfs_open( inode_id id );
int vfs_read( inode_id id, uint8_t *data, uint64_t size, uint64_t offset );
int vfs_stat( inode_id id, vfs_stat_data *stat );
int vfs_write( inode_id id, uint8_t *data, uint64_t size, uint64_t offset );

// Inode management
inode_id vfs_lookup_inode( char *pathname );
vfs_inode *vfs_lookup_inode_ptr( char *pathname );
vfs_inode *vfs_lookup_inode_ptr_by_id( inode_id id );
vfs_inode *vfs_allocate_inode( void );
inode_id vfs_get_from_dir( inode_id id, char *name );

// Cache management
void vfs_cache_initalize( void );
vfs_cache_item *vfs_cache_is_cached( uint64_t addr, uint32_t size );
bool vfs_cache_read( uint64_t addr, uint32_t size, uint8_t *data );
bool vfs_cache_write( uint64_t addr, uint32_t size, uint8_t *data, bool dirty );
bool vfs_cache_flush( vfs_cache_item *ci );
void vfs_cache_flush_all( void );
void vfs_cache_diagnostic( void );

#ifdef VIFS_DEV
	uint8_t *vfs_disk_read_test( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data );
	bool vfs_disk_read_test_no_cache( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data );
	uint8_t *vfs_disk_write_test( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data );
	bool vfs_disk_write_test_no_cache( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data );
#else
	uint8_t *vfs_disk_read( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data );
	uint8_t *vfs_disk_read_no_cache( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data );
	uint8_t *vfs_disk_write( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data );
	uint8_t *vfs_disk_write_no_cache( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data );
#endif

#ifdef __cplusplus
}
#endif

#endif