#include "vfs.h"
#include "rfs.h"
#include "afs.h"

#ifdef VIFS_OS_ENV
#include <device.h>
#endif

#undef VFS_CACHE_DEBUG

vfs_filesystem *file_systems;
vfs_inode root_inode;
vfs_inode *inode_index_tail;
inode_id vfs_inode_id_top;
uint8_t fs_id_top;
vfs_directory_list mount_points;

vfs_cache_list cache;
uint64_t cache_hits;
uint64_t cache_misses;
uint64_t cache_read_success;
uint64_t cache_read_fail;
uint64_t cache_write_success;
uint64_t cache_write_fail;
uint64_t cache_write_old;
uint64_t cache_write_new;
uint64_t cache_total_out;
uint64_t cache_total_in;
uint64_t cache_disk_read_calls;
uint64_t cache_disk_write_calls;

/**
 * @brief Initalizes the VFS
 * 
 * @return int VFS_ERROR_NONE on success, otherwise VFS_ERROR_ on failure
 */
int vfs_initalize( void ) {
	file_systems = NULL;

	root_inode.fs_type = 0;
	root_inode.type = VFS_INODE_TYPE_DIR;
	root_inode.id = 1;
	root_inode.is_mount_point = false;
	root_inode.dir_inodes = vfs_malloc( sizeof(vfs_directory) );

	if( root_inode.dir_inodes == NULL ) {
		return VFS_ERROR_MEMORY;
	}

	root_inode.dir_inodes->id = 0;
	root_inode.dir_inodes->next_dir = NULL;

	inode_index_tail = &root_inode;

	vfs_inode_id_top = 2;

	fs_id_top = 1;

	mount_points.count = 0;
	mount_points.entry = NULL;

	vfs_cache_initalize();

	return VFS_ERROR_NONE;
}

/**
 * @brief Registers a file system for use
 * 
 * @param fs 
 * @return int VFS_ERROR_NONE on success, otherwise VFS_ERROR_ on failure
 */
int vfs_register_fs( vfs_filesystem **fs ) {
	*fs = vfs_malloc( sizeof(vfs_filesystem) );

	if( *fs == NULL ) {
		return VFS_ERROR_MEMORY;
	}

	(*fs)->next_fs = NULL;

	vfs_filesystem *root = file_systems;
	if( root == NULL ) {
		file_systems = *fs;
	} else {
		while( root->next_fs != NULL ) {
			root = root->next_fs;
		}

		root->next_fs = (void *)*fs;
	}

	return VFS_ERROR_NONE;
}

/**
 * @brief Returns the file system object of the given type
 * 
 * @param fs_type 
 * @return vfs_filesystem* Pointer to the file system, NULL on failure
 */
vfs_filesystem *vfs_get_fs( uint8_t fs_type ) {
	vfs_filesystem *fs = file_systems;

	if( fs == NULL ) {
		vfs_panic( "No file systems registered.\n" );

		return NULL;
	}

	while( fs != NULL ) {
		if( fs->type == fs_type ) {
			return fs;
		}

		fs = (vfs_filesystem *)fs->next_fs;
	}

	vfs_panic( "Could not locate file system type: %d\n", fs_type );

	return NULL;
}

/**
 * @brief Closes an open file
 * 
 * @param id id of inode to close
 * @return int VFS_ERROR_NONE on success, otherwise VFS_ERROR_ on failure
 */
int vfs_close( inode_id id ) {
	return VFS_ERROR_NONE;
}

/**
 * @brief Creates an inode of type at path with name
 * 
 * @param type 
 * @param path 
 * @param name 
 * @return int inode di that was created (greater than 0), VFS_ERROR_ on failure
 */
int vfs_create( uint8_t type, char *path, char *name ) {
	vfs_inode *parent_node = vfs_lookup_inode_ptr( path );

	if( parent_node == NULL ) {
		//vfs_debugf( "Cannot find path \"%s\"", path );
		return VFS_ERROR_PATH_NOT_FOUND;
	}

	if( parent_node->type != VFS_INODE_TYPE_DIR ) {
		vfs_debugf( "Cannot create at \"%s\", not a directory.\n", path );
		return VFS_ERROR_NOT_A_DIRECTORY;
	}

	vfs_filesystem *fs = vfs_get_fs( parent_node->fs_type );

	if( fs == NULL ) {
		return VFS_ERROR_UNKNOWN_FS;
	}

	return fs->op.create( parent_node->id, type, path, name );
}

/**
 * @brief Returns each file in the provided directory
 * 
 * @param id 
 * @param list 
 * @return vfs_directory_list* Pointer to list, NULL on failure
 */
vfs_directory_list *vfs_get_directory_list( inode_id id, vfs_directory_list *list ) {
	vfs_inode *dir = vfs_lookup_inode_ptr_by_id( id );

	if( dir == NULL ) {
		//vfs_debugf( "could not find inode %ld for directory listing.\n", id );
		return NULL;
	}

	if( dir->type != VFS_INODE_TYPE_DIR ) {
		//vfs_debugf( "indoe ID %ld is not a directory.\n", id );
		return NULL;
	}

	vfs_filesystem *fs = vfs_get_fs( dir->fs_type );

	return fs->op.get_dir_list( id, list );
}

/**
 * @brief Creates a diretory
 * 
 * @param parent 
 * @param path 
 * @param name 
 * @return int inode id on success, VFS_ERROR_ on failure
 */
int vfs_mkdir( inode_id parent, char *path, char *name ) {
	return vfs_create( VFS_INODE_TYPE_DIR, path, name );
}

/**
 * @brief Mounts a file system at the given location
 * 
 * @param fs_type 
 * @param data 
 * @param path 
 * @return int VFS_ERROR_NONE on success, otherwise error number
 */
int vfs_mount( uint8_t fs_type, uint8_t *data, char *path ) {
	vfs_filesystem *fs = vfs_get_fs( fs_type );
	vfs_inode *mount_point = NULL;

	mount_point = vfs_lookup_inode_ptr( path );
	
	if( mount_point == NULL ) {
		vfs_debugf( "Path \"%s\" not found, mount failed.\n", path );
		return VFS_ERROR_PATH_NOT_FOUND;
	}

	if( mount_point->is_mount_point ) {
		if( mount_point->fs_type != 0 ) {
			vfs_debugf( "Mount point \"%s\" already claimed with fs_type %d\n", path, mount_point->type );
			return VFS_ERROR_OBJECT_ALREADY_IN_USE;
		}
	}

	if( mount_point->type != VFS_INODE_TYPE_DIR ) {
		vfs_debugf( "Mount point \"%s\" is not a directory.\n", path );
		return VFS_ERROR_NOT_A_DIRECTORY;
	}
	
	mount_point->fs_type = fs_type;
	mount_point->fs_id = fs_id_top++;
	mount_point->is_mount_point = true;

	vfs_directory_item *mp_list_item = NULL;
	bool found = false;
	if( mount_points.count == 0 ) {
		mount_points.entry = vfs_malloc(sizeof(vfs_directory_item));
		mount_points.count++;
		mp_list_item = mount_points.entry;
	} else {
		vfs_directory_item *head = mount_points.entry;
		do {
			if( head->next == NULL ) {
				head->next = vfs_malloc(sizeof(vfs_directory_item));
				mp_list_item = head->next;
				found = true;
			}

			head = head->next;
		} while( head != NULL && !found );
	}

	mp_list_item->id = mount_point->id;
	mp_list_item->ptr = mount_point;
	strcpy(mp_list_item->name, path);
	mount_points.count++;
		
	return fs->op.mount( mount_point->id, path, data );
}

/*
 * @brief Opens a file
 * 
 * @param id 
 * @return 0 on success, VFS_ERROR_ on failure
 */
int vfs_open( inode_id id ) {
	vfs_inode *node = vfs_lookup_inode_ptr_by_id( id );

	if( node == NULL ) {
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	vfs_filesystem *fs = vfs_get_fs( node->fs_type );

	if( fs == NULL ) {
		return VFS_ERROR_UNKNOWN_FS;
	}

	return fs->op.open( id );
}

/**
 * @brief Reads size bytes from inode id + offset into data
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset
 * @return int 
 */
int vfs_read( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	vfs_inode *node = vfs_lookup_inode_ptr_by_id( id );

	if( node == NULL ) {
		//vfs_debugf( "inode ID %ld not found, aborting read.\n", id );
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	vfs_filesystem *fs = vfs_get_fs( node->fs_type );

	return fs->op.read( id, data, size, offset );
}

/**
 * @brief Get statistics on an inode
 * 
 * @param id inode id to stat
 * @param stat_data Pointer to already allocated vfs_stat_data struct
 * @return int VFS_ERROR_NONE on succcess, VFS_ERROR_ on failure
 */
int vfs_stat( inode_id id, vfs_stat_data *stat_data ) {
	vfs_inode *node = vfs_lookup_inode_ptr_by_id( id );

	if( node == NULL ) {
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	vfs_filesystem *fs = vfs_get_fs( node->fs_type );

	if( fs == NULL ) {
		return VFS_ERROR_UNKNOWN_FS;
	}

	return fs->op.stat( id, stat_data );
}

/**
 * @brief Writes data to inode id startign at offset for size bytes
 * 
 * @param id 
 * @param data 
 * @param size 
 * @param offset 
 * @return int number of bytes written, -1 if error
 */
int vfs_write( inode_id id, uint8_t *data, uint64_t size, uint64_t offset ) {
	vfs_inode *node = vfs_lookup_inode_ptr_by_id( id );

	if( node == NULL ) {
		//vfs_debugf( "inode ID %ld not found, aborting write.\n", id );
		return VFS_ERROR_FILE_NOT_FOUND;
	}

	#ifdef VIFS_OS_ENV
	if( node->type == VFS_INODE_TYPE_DEVICE ) {
		device *dev = node->dev->data;

		return dev->write( id, data, size, offset );
	}
	#endif

	vfs_filesystem *fs = vfs_get_fs( node->fs_type );

	if( fs == NULL ) {
		return VFS_ERROR_UNKNOWN_FS;
	}

	return fs->op.write( id, data, size, offset );
}

/**
 * @brief Gets the inode id of path
 * 
 * @param path 
 * @return inode_id ID of inode if successful, otherwise 0
 */
inode_id vfs_lookup_inode( char *pathname ) {
	vfs_inode *node = vfs_lookup_inode_ptr( pathname );

	if( node != NULL ) {
		return node->id;
	}
	
	return 0;
}

/**
 * @brief Returns a vfs_inode object represented by path
 * 
 * @param pathname ABSOLUTE path to look up
 * @return vfs_inode* Pointer to inode if found, NULL on failure
 */
vfs_inode *vfs_lookup_inode_ptr( char *pathname ) {
	vfs_inode *ret_val = NULL;
	uint32_t path_length = vfs_strlen( pathname );

	if( strcmp( pathname, "/" ) == 0 ) {
		return &root_inode;
	}

	// Break up path by seperator
	// Test each element in turn
	// Return last element
	
	bool keep_going = true;
	inode_id parent_inode_id = 0;
	char *c = pathname;
	int element_index = 0;
	int path_index = 0;
	char name[VFS_NAME_MAX];
	memset( name, 0, VFS_NAME_MAX );

	do {	
		if( *c != '/' && *c != 0 ) {
			// build the element

			name[element_index] = *c;
			element_index++;
		} else {
			// done building the element, check for it
			//vfs_debugf( "Element: \"%s\"\n", name );

			if( element_index == 0 ) {
				parent_inode_id = 1;
				// ignore root dir
			} else {
				parent_inode_id = vfs_get_from_dir( parent_inode_id, name );
				if( parent_inode_id != 0 ) {
					// do it again
					memset( name, 0, VFS_NAME_MAX );
					element_index = 0;
				} else {
					// coudln't find it, fail overall
					return NULL;
				}
			}
		}

		c++;
		path_index++;

		if( path_index > path_length ) {
			keep_going = false;
		}
	} while( keep_going );
	
	ret_val = vfs_lookup_inode_ptr_by_id( parent_inode_id );

	return ret_val;
}

/**
 * @brief Gets inode structure from inode id
 * 
 * @param id 
 * @return vfs_inode* Pointer to inode structure on success, NULL on failure
 */
vfs_inode *vfs_lookup_inode_ptr_by_id( inode_id id ) {
	vfs_inode *ret_val = NULL;
	bool found = false;
	vfs_inode *head = &root_inode;

	do {
		if( head->id == id ) {
			ret_val = head;
			found = true;
		}

		head = head->next_inode;
	} while( head != NULL && !found );

	return ret_val;
}


/**
 * @brief Create a new inode for use
 * 
 * @return vfs_inode* Pointer to inode structure on success, NULL on failure
 */
vfs_inode *vfs_allocate_inode( void ) {
	vfs_inode *node = vfs_malloc( sizeof(vfs_inode) );

	node->fs_type = 0;
	node->fs_id = 0;
	node->id = vfs_inode_id_top++;
	node->type = 0;
	node->is_mount_point = false;

	inode_index_tail->next_inode = node;
	inode_index_tail = node;

	return node;
}

/**
 * @brief Gets the inode id of file name from the parent dir
 * 
 * @param parent_id parent inode id
 * @param name name of the file 
 * @return inode_id id of the found file, or VFS_ERROR_ if failed
 */
inode_id vfs_get_from_dir( inode_id parent_id, char *name ) {
	vfs_directory_list list;
	vfs_get_directory_list( parent_id, &list );

	for( int i = 0; i < list.count; i++ ) {
		if( strcmp(list.entry[i].name, name ) == 0 ) {
			return list.entry[i].id;
		}
	}

	return VFS_ERROR_FILE_NOT_FOUND;
}

/**
 * @brief Initalizes the vfs cache
 * 
 */
void vfs_cache_initalize( void ) {
	cache.head = NULL;
	cache.tail = NULL;
	cache_hits = 0;
	cache_misses = 0;
	cache_read_success = 0;
	cache_read_fail = 0;
	cache_write_success = 0;
	cache_write_fail = 0;
	cache_write_old = 0;
	cache_write_new = 0;
	cache_total_in = 0;
	cache_total_out = 0;
	cache_disk_read_calls = 0;
	cache_disk_write_calls = 0;
}

/**
 * @brief Returns true if the memory at addr and size is cached
 * 
 * @param addr Address to test if it's cached
 * @param size Size of data we want to access in cache
 * @return pointer Cache item
 * @return NULL Cache miss
 */
vfs_cache_item *vfs_cache_is_cached( uint64_t addr, uint32_t size ) {
	if( cache.head == NULL ) {
		return false;
	}

	vfs_cache_item *ci = cache.head;

	do {
		// If the address is greater than or equal to the cached addr, look more
		// otherwise go to the next item
		if( addr >= ci->address ) {
			// If the searched for size is less than or equal to the cached size,
			// then look more. Otherwise go to the next item.
			if( size <= ci->size ) {
				// It's possible from above that the we get addr = 10, size = 3
				// and ci.addr = 0, ci.size = 10. This would cause a buffer overflow.
				// If addr + size are within ci.addr + ci.size, we have a hit for
				// sure, otherwise fail for now (but TODO: expand the cache area).
				// This expansion will prevent overlapping addresses in MOST situations
				if( addr + size <= ci->address + ci->size ) {
					#ifdef VFS_CACHE_DEBUG
					vfs_debugf( "Cache hit @ %ld\n", addr );
					#endif

					cache_hits++;
					return ci;
				} else {
					// Expand the cached area
				}
			}
		}

		ci = ci->next;
	} while( ci != NULL );

	#ifdef VFS_CACHE_DEBUG
	vfs_debugf( "Cache miss @ %ld\n", addr );
	#endif 

	cache_misses++;
	return NULL;
}

/**
 * @brief Gets the cache at addr, copies it into data, for size length
 * 
 * @param addr 
 * @param size 
 * @param data 
 * 
 * @return true if we read from the cache
 */
bool vfs_cache_read( uint64_t addr, uint32_t size, uint8_t *data ) {
	vfs_cache_item *ci = vfs_cache_is_cached( addr, size );

	if( ci != NULL ) {
		uint32_t offset = addr - ci->address;
		memcpy( data, ci->data + offset, size );
		ci->read_count++;
		cache_read_success++;
		cache_total_out =+ size;
		return true;
	}

	cache_read_fail++;
	return false;
}

/**
 * @brief Writes data to the cache at addr for size
 * 
 * @param addr 
 * @param size 
 * @param data 
 * 
 * @return true if we wrote to the cache, false otherwise
 */
bool vfs_cache_write( uint64_t addr, uint32_t size, uint8_t *data, bool dirty ) {
	vfs_cache_item *ci = vfs_cache_is_cached( addr, size );

	if( ci != NULL ) {
		uint32_t offset = addr - ci->address;
		memcpy( ci->data + offset, data, size );
		ci->dirty = dirty;
		ci->write_count++;
		cache_write_old++;
		cache_write_success++;
		cache_total_in = cache_total_in + size;
		return true;
	} else {
		ci = vfs_malloc( sizeof(vfs_cache_item) );

		if( ci == NULL ) {
			cache_write_fail++;
			return false;
		}

		if( cache.head == NULL ) {
			cache.head = ci;
			cache.tail = ci;
		} else {
			cache.tail->next = ci;
			cache.tail = ci;
		}

		ci->address = addr;
		ci->size = size;
		ci->next = NULL;
		ci->data = vfs_malloc( size );
		ci->dirty = dirty;
		if( dirty ) {
			ci->write_count = 1;
		} else {
			ci->write_count = 0;
		}
		ci->read_count = 0;
		memcpy( ci->data, data, size );
		
		cache_write_success++;
		cache_write_new++;
		cache_total_in = cache_total_in + size;
		return true;
	}

	cache_write_fail++;
	return false;
}

/**
 * @brief Flushes the given cache block to disk
 * 
 * @param ci 
 * @return true Successful flush
 * @return false Flush failure
 */
bool vfs_cache_flush( vfs_cache_item *ci ) {
	if( ci->dirty == true ) {
		vfs_disk_write_no_cache( 0, ci->address, ci->size, ci->data );

		ci->dirty = false;
	}

	return true;
}

/**
 * @brief Flush all cache items to disk
 * 
 */
void vfs_cache_flush_all( void ) {
	vfs_cache_item *ci = cache.head;

	if( ci == NULL ) {
		return;
	}

	do {
		vfs_cache_flush( ci );

		ci = ci->next;
	} while( ci != NULL );
}

void *vfs_get_device_struct_from_inode_id( inode_id id ) {
	vfs_inode *ino = vfs_lookup_inode_ptr_by_id( id );

	if( ino == NULL ) {
		//return VFS_ERROR_FILE_NOT_FOUND;
		return NULL;
	}

	if( ino->type != VFS_INODE_TYPE_DEVICE ) {
		//return VFS_ERROR_NOT_A_DEVICE;
		return NULL;
	}

	return ino->dev->data;	
}

/**
 * @brief Display cache diagnostic data
 * 
 */
void vfs_cache_diagnostic( void ) {
	uint64_t total_size = 0;
	uint64_t total_objects = 0;

	vfs_cache_item *ci = cache.head;

	if( ci == NULL ) {
		vfs_debugf( "No cache.\n" );
		return;
	}

	do {
		total_size = total_size + ci->size;
		total_objects++;

		ci = ci->next;
	} while( ci != NULL );

	vfs_debugf( "Cache objects:       %ld\n", total_objects );
	vfs_debugf( "Cache total size:    %ld\n", total_size );
	vfs_debugf( "Cache total out:     %ld\n", cache_total_out );
	vfs_debugf( "Cache total in:      %ld\n", cache_total_in );
	vfs_debugf( "Cache hits:          %ld\n", cache_hits );
	vfs_debugf( "Cache misses:        %ld\n", cache_misses );
	vfs_debugf( "Cache read success:  %ld\n", cache_read_success );
	vfs_debugf( "Cache read fail:     %ld\n", cache_read_fail );
	vfs_debugf( "Cache write success: %ld\n", cache_write_success );
	vfs_debugf( "Cache write fail:    %ld\n", cache_write_fail );
	vfs_debugf( "Cache write new:     %ld\n", cache_write_new );
	vfs_debugf( "Cache write old:     %ld\n", cache_write_old );
	vfs_debugf( "Cache disk r calls:  %ld\n", cache_disk_read_calls );
	vfs_debugf( "Cache disk w calls:  %ld\n", cache_disk_write_calls );

	ci = cache.head;

	do {
		vfs_debugf( "Cache Object 0x%X:\n", ci->address );
		vfs_debugf( "    Dirty:   %X    --    Size:    0x%X\n", ci->dirty, ci->size );
		vfs_debugf( "    Reads:   %ld    --    Writes:  %ld\n", ci->read_count, ci->write_count );

		ci = ci->next;
	} while( ci != NULL );
}

#ifdef VIFS_DEV

extern FILE *fp;

/**
 * @brief Simulate a disk read
 * 
 * @param drive
 * @param data 
 * @param offset 
 * @param length 
 */
uint8_t *vfs_disk_read_test( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	cache_disk_read_calls++;

	if( vfs_cache_read( offset, length, data ) == true ) {
		return data;
	}

	if( vfs_disk_read_no_cache( drive, offset, length, data ) == true ) {
		vfs_cache_write( offset, length, data, false );
	}

	return data;
}

/**
 * @brief Read directly from the disk, bypassing cache
 * 
 * @param drive 
 * @param offset 
 * @param length 
 * @param data 
 * @return true 
 * @return false 
 */
bool vfs_disk_read_test_no_cache( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	fseek( fp, offset, SEEK_SET );
	
	int read_err = fread( data, length, 1, fp );

	if( read_err != 1 ) {
		vfs_debugf( "vfs_disk_read_test: fread failed.\n" );
		return false;
	}

	return true;
}

/**
 * @brief Simulate a disk write
 * 
 * @param drive 
 * @param offset 
 * @param length 
 * @param data 
 * @return uint8_t* 
 */
uint8_t *vfs_disk_write_test( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	cache_disk_write_calls++;

	if( vfs_cache_write(offset, length, data, true ) == true ) {
		return data;
	}

	vfs_disk_write_test_no_cache( drive, offset, length, data );

	return data;
}

/**
 * @brief Write directly to the disk, bypassing cache
 * 
 * @param drive 
 * @param offset 
 * @param length 
 * @param data 
 * @return true 
 * @return false 
 */
bool vfs_disk_write_test_no_cache( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	fseek( fp, offset, SEEK_SET );

	int write_err = fwrite( data, length, 1, fp );

	if( write_err != 1 ) {
		vfs_debugf( "vfs_disk_write_test: fwrite failed.\n" );
		return false;
	}

	fflush(fp);

	return true;
}

#else

uint8_t *vfs_disk_read( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	cache_disk_read_calls++;

	if( vfs_cache_read( offset, length, data ) == true ) {
		return data;
	}

	if( vfs_disk_read_no_cache( drive, offset, length, data ) != NULL ) {
		vfs_cache_write( offset, length, data, false );
	}

	return data;
}

uint8_t *vfs_disk_read_no_cache( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	if( !ahci_read_at_byte_offset_512_chunks( offset, length, data ) ) {
		vfs_debugf( "Could not read from ahci drive.\n" );
		return NULL;
	}

	return data;
}

uint8_t *vfs_disk_write( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	cache_disk_write_calls++;

	if( vfs_cache_write(offset, length, data, true ) == true ) {
		return data;
	}

	vfs_disk_write_no_cache( drive, offset, length, data );

	return data;
}

uint8_t *vfs_disk_write_no_cache( uint64_t drive, uint64_t offset, uint64_t length, uint8_t *data ) {
	// TODO
}

#endif