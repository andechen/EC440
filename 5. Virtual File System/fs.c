#include "disk.h"
#include "fs.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_FILENAME_LENGTH 15
#define MAX_NUM_OF_FILES 64
#define MAX_FILE_DESCRIPTOR 32
#define MAX_FILESIZE (1 << 24)

// Structs
/*******************************/

typedef struct{
    int used_block_bitmap_count;
    int used_block_bitmap_offset;
    int inode_metadata_blocks;
    int inode_metadata_offset;
    int d_offset;
    int d_length;
    int data_offset;
}Superblock;

typedef struct Inode{
    char type;
    int next_direct_offset;
}Inode;

typedef struct{
    bool used;
    int inode_number;
    char name[MAX_FILENAME_LENGTH + 1];
    int size;
    int count;
}Directory_Entry;

typedef struct{
    bool used;
    int inode_number;
    int offset;
}File_Descriptor;

// Global vars
/*******************************/
// Super Block
Superblock* super;

// Inode Table
Inode* Table;

// Used Block Bitmap
char* used_block_bitmap;

// File Descriptor Array
File_Descriptor fd_array[MAX_FILE_DESCRIPTOR];

// Directory Entry Array
Directory_Entry* Directory;

// File System is Mounted
static bool mounted = false;

// Functions
/*******************************/

int make_fs(const char *disk_name){
    // Make the virtual disk
    if(make_disk(disk_name) == -1){
        return -1;
    }

    // Open the virtual disk
    if(open_disk(disk_name) == -1){
        return -1;
    }
    
    // Initialising the Super Block and the relative offsets
    Superblock sb;

    // Directory offset and length
    sb.d_offset = 1;
    sb.d_length = (sizeof(Directory_Entry) * MAX_NUM_OF_FILES) / BLOCK_SIZE + 1;
    
    // Bitmap offset and length
    sb.used_block_bitmap_offset = sb.d_length + sb.d_offset;
    sb.used_block_bitmap_count = (sizeof(char) * DISK_BLOCKS) / BLOCK_SIZE;

    // Inode table offset and length
    sb.inode_metadata_offset = sb.used_block_bitmap_count + sb.used_block_bitmap_offset;
    sb.inode_metadata_blocks = (sizeof(Inode) * DISK_BLOCKS) / BLOCK_SIZE;

    // Offset to the start of the data
    sb.data_offset = sb.inode_metadata_blocks + sb.inode_metadata_offset;

    // Creating a buffer for writing
    char* buffer = calloc(1, BLOCK_SIZE);
    
    // Writing to disk
    memcpy((void*) buffer, (void*) &sb, sizeof(Superblock));
    if(block_write(0, buffer) == -1){
        return -1;
    }

    // Initialising the directory
    Directory_Entry empty_dir;
    empty_dir.used = false;
    empty_dir.name[0] = '\0';
    empty_dir.inode_number = -1;
    empty_dir.size = 0;
    empty_dir.count = 0;

    // Writing to disk
    for(int i = 0; i < MAX_NUM_OF_FILES; i++){
        memcpy((void*) (buffer + i * sizeof(Directory_Entry)), (void*) &empty_dir, sizeof(Directory_Entry));
    }
    if(block_write(sb.d_offset, buffer) == -1){
        return -1;
    }

    // Initialising the Inode Table
    Inode empty_inode;
    empty_inode.next_direct_offset = -2;
    
    // Writing to disk
    for(int i = 0; i < sb.inode_metadata_blocks; i++){
        for(int j = 0; j < BLOCK_SIZE / sizeof(Inode); j++){
            memcpy((void*) (buffer + j * sizeof(Inode)), (void*) &empty_inode, sizeof(Inode));
        }

        if(block_write(sb.inode_metadata_offset + i, buffer) == -1){
            return -1;
        }
    }

    // Initialising the Bitmap
    char empty_bitmap = '0';
    for(int i = 0; i < sb.used_block_bitmap_count; i++){
        memcpy((void*) (buffer + i * sizeof(char)), (void*) &empty_bitmap, sizeof(char));
    }
    if(block_write(sb.used_block_bitmap_offset, buffer) == -1){
        return -1;
    }
    
    // Close the disk
    if(close_disk(disk_name) == -1){
        return -1;
    }

    return 0;
}

int mount_fs(const char *disk_name){
    // If the disk does not exist, throw an error
    if(disk_name == NULL){
        return -1;
    }

    // If the disk is already mounted, throw an error
    if(mounted){
        return -1;
    }

    // Open the disk
    if(open_disk(disk_name) == -1){ 
        return -1;
    }

    // Creating a buffer for reading
    char* buffer = calloc(1, BLOCK_SIZE);
    super = (Superblock*) malloc(sizeof(Superblock));

    // Read the Super Block info
    if(block_read(0, buffer) == -1){
        return -1;
    }
    memcpy((void*) super, (void*) buffer, sizeof(Superblock));

    // Read the Directory info
    Directory = calloc(MAX_NUM_OF_FILES, sizeof(Directory_Entry));
    if(block_read(super->d_offset, buffer) == -1){
        return -1;
    }
    memcpy((void*) Directory, (void*) buffer, sizeof(Directory) * MAX_NUM_OF_FILES);

    // Reading the Inode table info
    Table = calloc(DISK_BLOCKS, sizeof(Inode));
    for(int i = 0; i < super->inode_metadata_blocks; i++){
        if(block_read(super->inode_metadata_offset + i, buffer) == -1){
            return -1;
        }
        memcpy((void*) (Table + i * BLOCK_SIZE / sizeof(Inode)), (void*) buffer, BLOCK_SIZE);
    }
    
    // Reading the Bitmap
    used_block_bitmap = (char*) malloc(DISK_BLOCKS);
    if(block_read(super->used_block_bitmap_offset, buffer) == -1){
        return -1;
    }
    memcpy(used_block_bitmap, buffer, sizeof(char) * super->used_block_bitmap_count);

    // Initialising the File Descriptor array info
    for(int i = 0; i < MAX_FILE_DESCRIPTOR; i++){
        fd_array[i].used = false;
    }

    // Mount the file system
    mounted = true;

    return 0;
}

int umount_fs(const char *disk_name){
    // If the disk does not exist, throw an error
    if(disk_name == NULL) {
        return -1;
    }

    // If the disk is not mounted, throw an error
    if(!mounted){
        return -1;
    }

    // Create a buffer for writing
    char* buffer = calloc(1, BLOCK_SIZE);

    // Writing the Super Block info
    memcpy((void*) buffer, (void*) super, sizeof(Superblock));
    if(block_write(0, buffer) == -1){
        return -1;
    }

    // Writing the Directory info
    memcpy(buffer, Directory, sizeof(Directory_Entry) * MAX_NUM_OF_FILES);
    if(block_write(super->d_offset, buffer) == -1){
        return -1;
    }

    // Writing the Inode Table info
    for(int i = 0; i < super->inode_metadata_blocks; i++){
        memcpy((void*) buffer, (void*) (Table + i * BLOCK_SIZE / sizeof(Inode)), BLOCK_SIZE);
        if(block_write(super->inode_metadata_offset + i, buffer) == -1){
            return -1;
        }
    }

    // Zero out the File Descriptor array
    for(int i = 0; i < MAX_FILE_DESCRIPTOR; i++){
        if(fd_array[i].used == true){
            fd_array[i].used = false;
            fd_array[i].inode_number = -1;
            fd_array[i].offset = 0;
        }
    }

    // Close the disk
    if(close_disk() == -1){
        return -1;
    }

    // Unmount the file system
    mounted = false;

    return 0;
}

int fs_open(const char *name){
    int fildes;
    // Find an unused file descriptor
    for(fildes = 0; fildes < MAX_FILE_DESCRIPTOR; fildes++){
        if(!fd_array[fildes].used){
            break;
        }
        else if(fildes == MAX_FILE_DESCRIPTOR - 1){
            return -1;
        }
    }

    // Find the file in the directory with the matching name
    for(int i = 0; i < MAX_NUM_OF_FILES; i++){
        if(strcmp(name, Directory[i].name) == 0){
            // Open the file
            fd_array[fildes].used = true;
            fd_array[fildes].inode_number = Directory[i].inode_number;
            fd_array[fildes].offset = 0;
            (Directory[i].count)++;
            
            return fildes;
        }
    }

    return -1;
}

int fs_close(int fildes){
    int index = -1;
    // Find the matching directory entry
    for(int i = 0; i < MAX_NUM_OF_FILES; i++){
        if(Directory[i].used && Directory[i].inode_number == fd_array[fildes].inode_number){
            index = i;
            break;
        }
    }
    
    if(index == -1 || !fd_array[fildes].used){
        return -1;
    }

    // Close the file
    fd_array[fildes].used = false;
    fd_array[fildes].inode_number = -1;
    fd_array[fildes].offset = 0;
    (Directory[index].count)--;

    return 0;
}

int fs_create(const char *name){
    if(strlen(name) > MAX_FILENAME_LENGTH){
        return -1;
    }

    int count = 0;
    // Check how many files exist
    for(int i = 0; i < MAX_NUM_OF_FILES; i++){
        if(Directory[i].used){
            if(strcmp(name, Directory[i].name) == 0){
                return -1;
            }
            count++;
        }
    }
    if(count == MAX_NUM_OF_FILES){
        return -1;
    }

    int inode;
    // Find a free entry in the bitmap
    for(inode = super->data_offset; inode < DISK_BLOCKS; inode++){
        if(used_block_bitmap[inode] == 0){
            // Mark as used
            used_block_bitmap[inode] = 1;
            // Fill the Inode Table
            Table[inode].next_direct_offset = -1;
            Table[inode].type = 1;
        
            // Fill in the directory entry
            for(int i = 0; i < MAX_NUM_OF_FILES; i++){
                if(!Directory[i].used){
                    Directory[i].used = true;
                    strcpy(Directory[i].name, name);
                    Directory[i].size = 0;
                    Directory[i].inode_number = inode;
                    Directory[i].count = 0;

                    return 0;
                }
            }
        }
    }

    return -1;
}

int fs_delete(const char *name){
    int index;
    // Finding the index in the directory
    for(index = 0; index < MAX_NUM_OF_FILES; index++){
        if(strcmp(Directory[index].name, name) == 0){
            if(Directory[index].count > 0){
                return -1;
            }
            break;
        }
        else if(index == MAX_NUM_OF_FILES - 1){
            return -1;
        }
    }

    int inode = Directory[index].inode_number, prev_inode;

    // Iterating through the Inode Table and freeing these entries
    while(Table[inode].next_direct_offset != -1){
        used_block_bitmap[inode] = 0;
        prev_inode = inode;
        
        inode = Table[inode].next_direct_offset;
        Table[prev_inode].next_direct_offset = -2;
    }
    used_block_bitmap[inode] = 0;
    Table[inode].next_direct_offset = -2;

    // Freeing the directory entry
    Directory[index].used = false;
    Directory[index].inode_number = -1;
    Directory[index].size = 0;
    memset(Directory[index].name, '\0', MAX_FILENAME_LENGTH);
    Directory[index].count = 0;

    return 0;
}

int fs_read(int fildes, void *buf, size_t nbyte){
    if(fildes < 0 || fildes > MAX_FILE_DESCRIPTOR || !fd_array[fildes].used || nbyte <= 0){
        return -1;
    }

    int index;
    // Finding the index in the directory
    for(index = 0; index < MAX_NUM_OF_FILES; index++){
        if(Directory[index].used && Directory[index].inode_number == fd_array[fildes].inode_number){
            break;
        }
    }

    int inode = Directory[index].inode_number;
    
    // Creating a buffer to read into
    char* buffer = calloc(1, BLOCK_SIZE * ((Directory[index].size - 1) / BLOCK_SIZE + 1));
    for(int i = 0; i < (Directory[index].size - 1) / BLOCK_SIZE + 1; i++){
        if(block_read(inode, buffer + i * BLOCK_SIZE) == -1){
            return -1;
        }
        
        inode = Table[inode].next_direct_offset;
        if(inode == -1){
            break;
        }
        
    }

    // Copying the contents to the provided buffer
    memcpy(buf, (void*) (buffer + fd_array[fildes].offset), nbyte);
    
    // If the requested read was longer than the file, only return the amount of bytes that was read
    if(fd_array[fildes].offset + nbyte > Directory[index].size){
        int offset = fd_array[fildes].offset;
        fd_array[fildes].offset = Directory[index].size;
        return (Directory[index].size - offset);
    }

    // Setting the new offset
    fd_array[fildes].offset += nbyte;
    
    return nbyte;
}

int fs_write(int fildes, void *buf, size_t nbyte){
    if(fildes < 0 || fildes > MAX_FILE_DESCRIPTOR || !fd_array[fildes].used){
        return -1;
    }

    int index;
    // Finding the directory entry
    for(index = 0; index < MAX_NUM_OF_FILES; index++){
        if(Directory[index].used && Directory[index].inode_number == fd_array[fildes].inode_number){
            break;
        }
    }

    if(fd_array[fildes].offset + nbyte > MAX_FILESIZE){
        nbyte = MAX_FILESIZE - fd_array[fildes].offset;
    }

    int inode = Directory[index].inode_number;

    // Allocating new inode offsets to fit the new file
    for(int i = 0; i < (fd_array[fildes].offset + nbyte - 1) / BLOCK_SIZE; i++){
        // If it is the end of the file, grab a new block
        if(Table[inode].next_direct_offset == -1){
            // Find a free block
            for(int j = super->data_offset; j < DISK_BLOCKS; j++){
                if(used_block_bitmap[j] == 0){
                    // Mark as taken
                    used_block_bitmap[j] = 1;

                    // Record current & next offset
                    Table[inode].next_direct_offset = j;
                    Table[j].next_direct_offset = -1;

                    break;
                }
                else if(j == DISK_BLOCKS - 1){
                    return -1;
                }
            }
        }

        // Traverse through the Inode Table for this file
        inode = Table[inode].next_direct_offset;
    }

    // Record the new file size
    if(fd_array[fildes].offset + nbyte > Directory[index].size){
        Directory[index].size = fd_array[fildes].offset + nbyte;
    }

    // Allocate a buffer for writing
    char* buffer = calloc(1, BLOCK_SIZE * ((Directory[index].size - 1) / BLOCK_SIZE + 1));
    
    // Resetting the variable to the start of the file
    inode = Directory[index].inode_number;
    
    // Reading the whole file before write
    for(int read = 0; read < (Directory[index].size - 1) / BLOCK_SIZE + 1; read++){
        if(block_read(inode, buffer + read * BLOCK_SIZE) == -1){
            return -1;
        }
        inode = Table[inode].next_direct_offset;
    }

    // Reset again
    inode = Directory[index].inode_number;

    // Adding the new stuff
    memcpy((void*) (buffer + fd_array[fildes].offset), buf, nbyte);
    
    // Finally writing the new file
    for(int write = 0; write < (Directory[index].size - 1) / BLOCK_SIZE + 1; write++){
        if(block_write(inode, buffer + write * BLOCK_SIZE) == -1){
            return -1;
        }
        inode = Table[inode].next_direct_offset;
    }

    // Recording the new offset
    fd_array[fildes].offset += nbyte;

    return nbyte;
}

int fs_get_filesize(int fildes){
    // Find the directory entry and return its size
    for(int i = 0; i < MAX_NUM_OF_FILES; i++){
        if(Directory[i].used && Directory[i].inode_number == fd_array[fildes].inode_number){
            return Directory[i].size;
        }
    }

    return -1;
}

int fs_listfiles(char ***files){
    int num_of_files = 0;
    // Count the number of files present
    for(int i = 0; i < MAX_NUM_OF_FILES; i++){
        if(Directory[i].used){
            num_of_files++;
        }
    }

    // Allocate enough space for the file names
    *files = (char**) calloc(num_of_files + 1, sizeof(char*));
    for(int i = 0; i < num_of_files; i++){
        files[0][i] = (char*) calloc(MAX_FILENAME_LENGTH + 1, sizeof(char));
    }
    files[0][num_of_files] = (char*) malloc(sizeof(char));

    int index = 0;
    // Fill the names of files
    for(int i = 0; i < MAX_NUM_OF_FILES; i++){
        if(Directory[i].used){
            strncpy(files[0][index], Directory[i].name, sizeof(char[MAX_FILENAME_LENGTH + 1]));
            index++;
        }
    }
    files[0][num_of_files] = NULL;

    return 0;
}

int fs_lseek(int fildes, off_t offset){
    if(fildes < 0 || fildes > MAX_FILE_DESCRIPTOR || !fd_array[fildes].used){
        return -1;
    }

    // Find the directory entry and set the new offset
    for(int i = 0; i < MAX_NUM_OF_FILES; i++){
        if(Directory[i].used && Directory[i].inode_number == fd_array[fildes].inode_number){
            if(offset < 0 || offset > Directory[i].size){
                return -1;
            }

            fd_array[fildes].offset = offset;
            return 0;
        }
    }

    return -1;
}

int fs_truncate(int fildes, off_t length){
    if(fildes < 0 || fildes > MAX_FILE_DESCRIPTOR || !fd_array[fildes].used){
        return -1;
    }

    int inode;
    // Find the directory entry
    for(int i = 0; i < MAX_NUM_OF_FILES; i++){
        if(Directory[i].used && Directory[i].inode_number == fd_array[fildes].inode_number){
            if(length > Directory[i].size){
                return -1;
            }

            // Set the new length
            Directory[i].size = length;

            // Get the inode offset
            inode = Directory[i].inode_number;

            break;
        }
    }

    // Leave the length of the file untouched
    for(int i = 0; i < (length - 1) / BLOCK_SIZE; i++){
        inode = Table[inode].next_direct_offset;
    }

    int tmp, truncate = inode;
    // Delete the remaining info
    while(inode != -1){
        tmp = inode;
        inode = Table[inode].next_direct_offset;
        Table[tmp].next_direct_offset = -2;
    }
    // New EOF
    Table[truncate].next_direct_offset = -1;

    return 0;
}
