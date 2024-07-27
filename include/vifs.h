#if !defined(VIFS_V2_INCLUDED)
#define VIFS_V2_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef VIFS_OS
	#define VIFS_DEV
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <stdint.h>
    #include <stdbool.h>
    #include <sys/stat.h>
    #include <dirent.h>
    #include <unistd.h>
    #include <errno.h>
#endif

#ifdef VIFS_DEV
void vifs_show_help( void );
int vifs_vfs_initalize( void );
int vifs_afs_initalize( char *afs_img );
int vifs_run_os_tests( void );
void vifs_mkdir( char *pathname );
void vifs_ls( char *pathname );
void vifs_cat( char *pathname );
void vifs_cp( char *src, char *dest );
void vifs_cpdir( char *src, char *dest );
void vifs_bootstrap( char *level, char *afs_image );
void vifs_new_drive_img( char *size, char *afs_image );
void vifs_pathname_to_path( char *pathname, char *path );
void vifs_pathname_to_name( char *pathname, char *name );
void vifs_parse_pathname( char *pathname, int path_or_name, char *data );
void vfs_test_cp_real_file( char *real_file_pathname, char *vifs_path, char *vifs_name );
void vfs_test_ramfs( void );
void vfs_test_afs( void );
void vfs_test_create_file( char *path, char *name, uint8_t *data, uint64_t size );
void vfs_test_create_dir( char *path, char *name );
void vfs_test_ls( char *path );
void vfs_test_cat( char *pathname );
#endif

#ifdef __cplusplus
}
#endif

#endif