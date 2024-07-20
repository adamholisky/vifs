#include <stdio.h>
#include "vfs.h"
#include "rfs.h"

#ifdef VIFS_DEV
#include <stdlib.h>
#include <string.h>
#endif

rfs_file_list rfs_files;

/**
 * @brief 
 * 
 * @return int 
 */
int rfs_initalize( void ) {
    vfs_filesystem *rfs;

    rfs = vfs_register_fs( rfs );

    rfs->op.open = rfs_open;
    rfs->op.mount = rfs_mount;
    rfs->op.create = rfs_create;

    rfs_files.head = NULL;
    rfs_files.tail = NULL;
    rfs_files.count = 0;

    return 0;
}

/**
 * @brief 
 * 
 * @param id 
 * @return int 
 */
int rfs_open( inode_id id ) {

}

/**
 * @brief 
 * 
 * @param id 
 * @param data_root 
 * @return int 
 */
int rfs_mount( inode_id id, uint8_t *data_root ) {
    rfs_files.count = 1;
    rfs_files.head = vfs_malloc( sizeof(rfs_file_list_el) );
    rfs_files.tail = rfs_files.head;

    rfs_files.head->file = vfs_malloc( sizeof(rfs_file) );
    rfs_files.head->file->rfs_file_type = RFS_FILE_TYPE_DIR;
    rfs_files.head->file->vfs_inode_id = id;
    rfs_files.head->file->vfs_parent_inode_id = 0;

    rfs_file_list *root_dir_list = vfs_malloc( sizeof(rfs_file_list) );
    root_dir_list->head = NULL;
    root_dir_list->tail = NULL;
    root_dir_list->count = 0;
    rfs_files.head->file->dir_list = (void *)root_dir_list;

    return 0;
}

/**
 * @brief 
 * 
 * @param type 
 * @param parent 
 * @param path 
 * @param name 
 * @return int 
 */
int rfs_create( uint8_t type, inode_id parent, char *path, char *name ) {
    // Allocate a VFS inode for this object, fill in details
    vfs_inode *node = vfs_allocate_inode();
    node->fs_type = FS_TYPE_RFS;
    node->type = type;

    // Allocate a RFS file, fill in details
    rfs_file *f = vfs_malloc( sizeof(rfs_file) );
    f->vfs_inode_id = node->id;
    f->size = 0;
    f->vfs_parent_inode_id = parent;

    switch( type ) {
        case VFS_INODE_TYPE_DIR:
            f->rfs_file_type = RFS_FILE_TYPE_DIR;
            break;
        case VFS_INODE_TYPE_FILE:
            f->rfs_file_type = RFS_FILE_TYPE_FILE;
            break;
    }

    // If dir, initalize the RFS dir_list
    if( type == VFS_INODE_TYPE_DIR ) {
        rfs_file_list *d_list = vfs_malloc( sizeof(rfs_file_list) );
        f->dir_list = (void *)d_list;
        d_list->count = 0;
        d_list->head = NULL;
        d_list->tail = NULL;
    } else {
        f->dir_list = NULL;
    }

    // Insert into parent directory
    rfs_file *rfs_parent = rfs_lookup_by_inode_id( parent );
    rfs_file_list *parent_dir = (rfs_file_list *)rfs_parent->dir_list;
    rfs_file_list_el *list_el = vfs_malloc( sizeof(rfs_file_list_el) );
    list_el->next = NULL;

    if( parent_dir->head == NULL ) {
        parent_dir->head = list_el;
        parent_dir->tail = list_el;
        parent_dir->count++;
    } else {
        parent_dir->tail->next = list_el;
        parent_dir->tail = list_el;
    }

    // Attach completed file to the master list
    rfs_file_list_el *main_file_list_el = vfs_malloc( sizeof(rfs_file_list_el) );
    main_file_list_el->file = f;
    main_file_list_el->next = NULL;
    rfs_files.tail->next = main_file_list_el;
    rfs_files.tail = main_file_list_el;

    return 0;
}

/**
 * @brief 
 * 
 * @param id 
 * @return rfs_file* 
 */
rfs_file *rfs_lookup_by_inode_id( inode_id id ) {
    rfs_file *f = NULL;
    rfs_file_list_el *head = rfs_files.head;
    bool keep_going = true;
    bool found = false;

    do {
        f = head->file;

        if( f->vfs_inode_id == id ) {
            found = true;
        } else if( head->next == NULL ) {
            keep_going = false;
        } else {
            head = head->next;
        }
    } while( keep_going && !found );

    if( !found ) {
        return NULL;
    }

    return f;
}

/**
 * @brief 
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset 
 * @return int 
 */
int rfs_write( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {

}