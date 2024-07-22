#if !defined(AFS_V2_INCLUDED)
#define AFS_V2_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include "vfs.h"

/* 

Disk layout
--------------

0		Drive header (afs_drive)
x		Block meta data
n		Free blocks

Where:
x = sizeof(afs_drive);
n = sizeof(afs_drive) + (sizeof(afs_block_meta_data) * afs_drive.block_count)

Block Calculation

1 GiB drive
	1073741824 bytes
	4096 bytes/block
	262,144 blocks
	14417920 bytes of block meta data

*/



#define AFS_VERSION_1 2

#define AFS_DEFAULT_BLOCK_SIZE 4096
#define AFS_MAX_NAME_SIZE 50

#define AFS_BLOCK_TYPE_UNKNOWN 0
#define AFS_BLOCK_TYPE_FILE 1
#define AFS_BLOCK_TYPE_DIRECTORY 2
#define AFS_BLOCK_TYPE_META 3
#define AFS_BLOCK_TYPE_SYSTEM 4
#define AFS_BLOCK_TYPE_NOT_SET 5

typedef struct {
	char		magic[4];		// "AFS "
	uint8_t 	version;		// version of the drive struct
	uint64_t 	size;			// overall size of the drive, in bytes
	uint32_t	block_size;		// Size of blocks
	uint32_t	block_count;	// Number of blocks in the dirve
	uint32_t	root_directory;	// starting block of the dir struct for root
	uint32_t	next_free;		// next free block
	uint32_t	reserved_1;
	uint32_t	reserved_2;
	uint32_t	reserved_3;
	uint32_t	reserved_4;
	uint32_t	reserved_5;
	uint32_t	reserved_6;
	uint32_t	reserved_7;
	uint32_t	reserved_8;
} afs_drive;

typedef struct {
	uint32_t	id;				// unique block id (aka: inode)
	uint8_t		block_type;		// type of data the block holds
	char		name[AFS_MAX_NAME_SIZE];		// name
	bool		in_use;
} afs_block_meta_data;

typedef struct {
	uint8_t		data[AFS_DEFAULT_BLOCK_SIZE];
} afs_generic_block;

typedef struct {
	uint32_t 	type;			// Type, always AFS_BLOCK_TYPE_FILE
	uint32_t	file_size;		// size of the actual file data
	uint32_t	reserved_1;
	uint32_t	reserved_2;
	uint32_t	reserved_3;
	uint32_t	reserved_4;
	uint32_t	reserved_5;
	uint32_t	reserved_6;
	uint32_t	reserved_7;
	uint32_t	reserved_8;
} afs_file;

typedef struct {
	uint32_t 	type;			// Type, always AFS_BLOCK_TYPE_DIRECTORY
	uint32_t	index[256];		// Block index for things in this directory
	uint32_t	next_index;		// next index free
	uint32_t	reserved_1;
	uint32_t	reserved_2;
	uint32_t	reserved_3;
	uint32_t	reserved_4;
	uint32_t	reserved_5;
	uint32_t	reserved_6;
	uint32_t	reserved_7;
	uint32_t	reserved_8;
} afs_block_directory;

typedef struct {
	inode_id vfs_id;
	uint32_t block_id;
	void *next;
} afs_inode;

int afs_initalize( uint64_t drive_size_in_bytes, uint8_t *data_root );
int afs_mount( inode_id id, char *path, uint8_t *data_root );
int afs_load_directory_as_inodes( inode_id parent_inode, afs_block_directory *dir );
int afs_load_block_as_inode( afs_block_meta_data *meta );
vfs_directory_list *afs_dir_list( inode_id id, vfs_directory_list *list );
inode_id afs_find_inode_from_block_id( uint32_t block_id );
afs_inode *afs_lookup_by_inode_id( inode_id id );

#ifdef VIFS_DEV

void afs_bootstrap( FILE *fp, uint64_t size );
bool afs_bootstrap_write( FILE *fp, void *data, uint64_t size );

#endif

#ifdef __cplusplus
}
#endif

#endif