#if !defined(RFS_V2_INCLUDED)
#define RFS_V2_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include "vfs.h"

#define RFS_FILE_TYPE_DIR 0
#define RFS_FILE_TYPE_FILE 1
#define RFS_FILE_TYPE_LINK 2
#define RFS_FILE_TYPE_DEVICE 3

typedef struct {
    inode_id vfs_inode_id;
    inode_id vfs_parent_inode_id;

    uint8_t rfs_file_type;
    char name[VFS_NAME_MAX];
    uint8_t *data;
    uint64_t size;

    void *dir_list;
} rfs_file;

typedef struct {
    void *next;
    rfs_file *file;
} rfs_file_list_el;

typedef struct {
    rfs_file_list_el *head;
    rfs_file_list_el *tail;
    uint64_t count;
} rfs_file_list;



int rfs_initalize( void );
rfs_file *rfs_lookup_by_inode_id( inode_id id );
int rfs_open( inode_id id );
int rfs_mount( inode_id id, uint8_t *data_root );
int rfs_create( uint8_t type, inode_id parent, char *path, char *name );
int rfs_write( inode_id id, uint8_t *data, uint64_t size, uint64_t offset );

#ifdef __cplusplus
}
#endif

#endif