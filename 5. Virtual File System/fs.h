#ifndef INCLUDE_FS_H
#define INCLUDE_FS_H
#include <sys/types.h>

// Make a new file system
int make_fs(const char *disk_name);

// Mount an existing file system
int mount_fs(const char *disk_name);

// Unmount an existing file system
int umount_fs(const char *disk_name);

// Open a file
int fs_open(const char *name);

// Close a file
int fs_close(int fildes);

// Create a file
int fs_create(const char *name);

// Delete a file
int fs_delete(const char *name);

// Read nbyte bytes from a file
int fs_read(int fildes, void *buf, size_t nbyte);

// Write nbyte bytes to a file
int fs_write(int fildes, void *buf, size_t nbyte);

// Get the file size
int fs_get_filesize(int fildes);

// List the files in the directory
int fs_listfiles(char ***files);

// Seek to the specified offset in a file
int fs_lseek(int fildes, off_t offset);

// Truncate a file by the provided length
int fs_truncate(int fildes, off_t length);

#endif /* INCLUDE_FS_H */
