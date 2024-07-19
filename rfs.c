#include <stdio.h>
#include "vfs.h"
#include "rfs.h"


int rfs_initalize( void ) {
    vfs_filesystem *rfs;

    vfs_register_fs( rfs );

    rfs->op.open = rfs_open;
    rfs->op.mount = rfs_mount;
}

int rfs_open( inode_id id ) {

}

int rfs_mount( inode_id id, uint8_t *data_root ) {

}