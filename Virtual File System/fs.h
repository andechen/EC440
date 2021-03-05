#ifndef _FS_H_
#define _FS_H_

/******************************************************************************/
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DISK_BLOCKS  8192      /* number of blocks on the disk                */
#define BLOCK_SIZE   4096      /* block size on "disk"                        */
#define MAX_F_NAME   15        /* maximum size of file name                   */
#define MAX_F_NUM    64        /* maximum number of files                     */
#define MAX_FILDES   32        /* maximum number of file descriptors          */
#define FAT_SIZE     8245     /* maximum number of entries in FAT             */
#define SUPERBLOCK_LOC 0       /* Fixed location of the super block on the virtual disk */
#define MAX_F_SIZE   16777216   /* Maximum size of a single file              */

//Defined values for the superblock
#define DIR_IDX     1
#define DIR_LEN     1
#define FAT_IDX     2
#define FAT_LEN     9
#define DATA_IDX    11  

//Symbols used for EOF and N/A numbers in the FAT
#define END_OF_FILE   9000
#define EMPTY         9999

//Return Values
#define SUCCESS      0
#define FAILURE      -1

/******Napkin Math for Meta Information******/
/*
    Super Block : "%4hu%4hu%4hu%4hu%4hu" : each %hu refers to a block with max 8192 = 4 chars.
        Total 16 chars = 16 bytes = 1 Block
    DIR : "%1d%16s%8d%4hu%2hhd" : %d->1 char, %s->16 chars, %d->33554432 max so 8 chars, %hu->4 chars, %hhd->2 chars
        31 chars = 31 bytes per directory entry
        Total 31*64chars = 1984bytes = 1 Block
    FAT : "%4hu" = 4 chars per entry
        8181 data blocks + 64 eof = 8245
        Total 4*8245 = 32980 bytes = 9 blocks
*/

/******************************************************************************/
int make_fs(char* disk_name);
int mount_fs(char* disk_name);
int umount_fs(char* disk_name);
int fs_open(char* name);
int fs_close(int fildes);
int fs_create(char* name);
int fs_delete(char* name);
int fs_read(int fildes, void* buf, size_t nbyte);
int fs_write(int fildes, void* buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_listfiles(char*** files);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);
/******************************************************************************/

#endif