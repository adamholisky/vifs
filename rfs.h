#if !defined(RFS_V2_INCLUDED)
#define RFS_V2_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include "vfs.h"

int rfs_initalize( void );
int rfs_open( inode_id id );
int rfs_mount( inode_id id, uint8_t *data_root );

#ifdef __cplusplus
}
#endif

#endif