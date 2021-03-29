#include "fs.h"
#include "disk.h"

/******************************************************************************/
struct super_block
{
    unsigned short fat_idx;    // First block of the FAT
    unsigned short fat_len;    // Length of FAT in blocks
    unsigned short dir_idx;    // First block of directory
    unsigned short dir_len;    // Length of directory in blocks
    unsigned short data_idx;   // First block of file-data
};

struct dir_entry
{
    int used;                   // Is this file-”slot” in use
    char name [MAX_F_NAME + 1]; 
    unsigned int size;          // file size
    unsigned short head;        // first data block of file
    int8_t ref_cnt;             // how many open file descriptors are there?// ref_cnt > 0 -> cannot delete file
};

struct file_descriptor
{
    int used;                    // fdin use
    unsigned short file;         // the first block of the file (f) to which fd refers to
    unsigned int offset;         // position of fd within f
};
/******************************************************************************/
struct super_block fs;
struct file_descriptor fildesArray[MAX_FILDES]; // 32 
unsigned short FAT[FAT_SIZE];                   // Will be populated with the FAT data
struct dir_entry DIR[MAX_F_NUM];                // Will be populated with the directory data                           

//Writes the superblock to the disk based on the current fs
int writeSuperBlock()
{
    char buffer[BLOCK_SIZE] = {0};
    sprintf(&buffer[0], "%-4hu%-4hu%-4hu%-4hu%-4hu", fs.fat_idx, fs.fat_len, fs.dir_idx, fs.dir_len, fs.data_idx);
    
    if(block_write(SUPERBLOCK_LOC, &buffer[0]) == -1)
    {
        return FAILURE;
    }

    return SUCCESS;
}

//Reads the superblock from the disk and updates the fs
int readSuperBlock()
{
    //Read the superblock from the disk
    char buffer[BLOCK_SIZE];
    if(block_read(SUPERBLOCK_LOC, buffer) == -1)
    {
        return FAILURE;
    }

    sscanf(&buffer[0], "%4hu%4hu%4hu%4hu%4hu", &fs.fat_idx, &fs.fat_len, &fs.dir_idx, &fs.dir_len, &fs.data_idx);

    return SUCCESS;
}

//Writes the formatted directory to the disk
int writeDirectory(unsigned short dir_idx)
{
    //Get all of the directory contents into a string
    int i, j;
    int index = 0;
    char w_dir_str[BLOCK_SIZE] = {0};
    for (i = 0; i < MAX_F_NUM; i++)
    {
        index += sprintf(&w_dir_str[index], "%1d" ,  DIR[i].used);
        for (j = 0; j < 16; j++)
        {
            index += sprintf(&w_dir_str[index], "%1c", DIR[i].name[j]);
        }
        index += sprintf(&w_dir_str[index], "%-8i%-4hu%-2hhd", DIR[i].size, DIR[i].head, DIR[i].ref_cnt);
    }

    //Write the directory string to disk
    if (block_write(dir_idx, &w_dir_str[0]) == -1)
    {
        return FAILURE;
    }

    return SUCCESS;
}

//Reads the formatted directory from the disk
int readDirectory(unsigned short dir_idx)
{
    //Define strings for reading
    char r_dir_str[BLOCK_SIZE];

    //Read the directory from the disk
    if (block_read(dir_idx, r_dir_str) == -1)
    {
        return FAILURE;
    }

    //Parse the read directory string to the directory struct
    int i;
    for (i = 0; i < MAX_F_NUM; i++)
    {
        sscanf(&r_dir_str[i*31], "%1d ", &DIR[i].used);
        strncpy(DIR[i].name, &r_dir_str[((i*31) + 1)], sizeof(char[MAX_F_NAME + 1]));
        sscanf(&r_dir_str[(i*31) + 17], "%8i%4hu%2hhd", &DIR[i].size, &DIR[i].head, &DIR[i].ref_cnt);
    }

    return SUCCESS;
}

//Writes the FAT to the disk
int writeFAT(unsigned short fat_idx, unsigned int fat_len)
{
    //Assmble the string representing the FAT
    int i;
    char FAT_str[FAT_LEN * BLOCK_SIZE * 4] = {0};
    int index = 0;
    for (i = 0; i < FAT_SIZE; i++)
    {
        index += sprintf(&FAT_str[index], "%-4hu", FAT[i]);
    }

    //Write the string to each block
    for (i = fat_idx; i < fat_len + fat_idx; i++)
    {
        if (block_write(i, &FAT_str[BLOCK_SIZE * (i - fat_idx)]) == -1)
        {
            return FAILURE;
        }
    }

    return SUCCESS;
}

//Reads the FAT from the disk and updates the global var
int readFAT(unsigned short fat_idx, unsigned int fat_len)
{
    //Read the FAT from the disk, need to loop to get all the blocks it is within
    char FAT_str[FAT_LEN * BLOCK_SIZE] = {0};
    int i;
    for (i = fat_idx; i < fat_len+fat_idx; i++)
    {
        if (block_read(i, &FAT_str[BLOCK_SIZE * (i - fat_idx)]) == -1)
        {
            return FAILURE;
        }
    }
    
    for (i = 0; i < FAT_SIZE; i++)
        sscanf(&FAT_str[i*4], "%4hu", &FAT[i]);

    return SUCCESS;
}

//Reorganizes the FAT after something is removed from it, or a new block is allocated to a file
//i.e. defragments it
void FATdefrag()
{    
    unsigned short emptyIndex;
    int firstEmpty = 0;
    int i;
    for (i = 0; i < FAT_SIZE; i++)
    {
        //Once an empty block is found, when you find the next file in the FAT, copy it
        if (firstEmpty && (FAT[i] != EMPTY))
        {
            int temp = i;
            while (FAT[i] != END_OF_FILE)
            {
                FAT[emptyIndex++] = FAT[i];
                FAT[i++] = EMPTY;
            }
            FAT[emptyIndex] = END_OF_FILE;
            FAT[i] = EMPTY;
            emptyIndex = temp;
        }
        //If you find an empty block then set emptyIndex to it
        else if (!firstEmpty && (FAT[i] == EMPTY))
        {
            firstEmpty = 1;
            emptyIndex = i;
        }        
    }
}

//Finds the next empty block in the file system
int findEmptyBlock()
{
    //Find the next empty block to use
    int b, j;
    int emptyBlock = 0;
    static int previousBlock = DATA_IDX; // Used to improve runtime of finding the next empty block

    //See if block after previous is free first
    for (b = (previousBlock+1); (b < DISK_BLOCKS) && (!emptyBlock); b++)
    {
        //Check if block b exists in the FAT
        for (j = 0; (j < FAT_SIZE) && (FAT[j] != b); j++);
        
        //If it does not than we have found the first empty block
        if (j == FAT_SIZE)
        {
            emptyBlock = b;
            previousBlock = b;
        }
    }

    //If that didn't work then check all the previous blocks
    if(!emptyBlock)
    {
        for (b = fs.data_idx; (b < (previousBlock+1)) && (!emptyBlock); b++)
        {
            //Check if block b exists in the FAT
            for (j = 0; (j < FAT_SIZE) && (FAT[j] != b); j++);
            
            //If it does not than we have found the first empty block
            if (j == FAT_SIZE)
            {
                emptyBlock = b;
                previousBlock = b;
            }
        } 
    }

    return emptyBlock;
}

//Finds a directory entry given a fildes
int findDirectoryEntry(int fildes)
{
    //Find the directory entry for the file
    int d;
    for (d = 0; (d < MAX_F_NUM); d++)
        if((DIR[d].head == fildesArray[fildes].file) && (DIR[d].used))
            break;
    
    //Check that it found a directory entry
    if (d == MAX_F_NUM)
    {
        return FAILURE;
    }

    return d;
}

//Creates an empty file system on virtual disk with disk_name
int make_fs(char* disk_name)
{
    //Call make_disk
    if (make_disk(disk_name) == -1)
    {
        return FAILURE;
    }

    //Open the disk
    if (open_disk(disk_name) == -1)
    {
        return FAILURE;
    }

    //Initialize default values for superblock
    fs.fat_idx = FAT_IDX;
    fs.fat_len = FAT_LEN;
    fs.dir_idx = DIR_IDX;
    fs.dir_len = DIR_LEN;
    fs.data_idx = DATA_IDX;

    //Write Empty Superblock
    if (writeSuperBlock() == -1)
    {
        return FAILURE;
    }

    //Write Empty Directory
    int i;
    for (i = 0; i < MAX_F_NUM; i++)
    {
        DIR[i].used = 0;
    }
    if (writeDirectory(fs.dir_idx) == -1)
    {
        return FAILURE;
    }

    //Write Empty FAT
    for (i = 0; i < FAT_SIZE; i++)
    {
        FAT[i] = EMPTY;
    }
    if (writeFAT(fs.fat_idx, fs.fat_len) == -1)
    {
        return FAILURE;
    }

    //Close the disk
    if (close_disk() == -1)
    {
        return FAILURE;
    }

    return SUCCESS;
}

//Mounts fs from virtual disk disk_name
int mount_fs(char* disk_name)
{
    //Open the disk
    if (open_disk(disk_name) == -1)
    {
        return FAILURE;
    }

    //Read the super block
    if (readSuperBlock() == -1)
    {
        return FAILURE;
    }

    //Read the directory
    if (readDirectory(fs.dir_idx) == -1)
    {
        return FAILURE;
    }

    //Read the FAT
    if (readFAT(fs.fat_idx, fs.fat_len) == -1)
    {
        return FAILURE;
    }

    //Set all file descriptors to not used
    int i;
    for (i = 0; i < MAX_FILDES; i++)
    {
        fildesArray[i].used = 0;
    }

    return SUCCESS;
}

//Unmounts fs from virtual disk disk_name
int umount_fs(char* disk_name)
{
    //Write Updated Directory
    if (writeDirectory(fs.dir_idx) == -1)
    {
        return FAILURE;
    }

    //Write Updated FAT
    if (writeFAT(fs.fat_idx, fs.fat_len) == -1)
    {
        return FAILURE;
    }

    //Close the disk
    if (close_disk() == -1)
    {
        return FAILURE;
    }
    
    //Check that all file descriptors are not used
    int i;
    for (i = 0; i < MAX_FILDES; i++)
    {
        if(fildesArray[i].used)
        {
            return FAILURE;
        }  
    }
    
    return SUCCESS; 
}

//Creates a new file with name name in the fs
int fs_create(char* name)
{
    //Check the length of name
    int length = strnlen(name, MAX_F_NAME+1);
    if (length == MAX_F_NAME+1)
    {
        return FAILURE;
    }
    
    //Simultaneously check that a directory entry doesn't exist for this file
    //and that there are less than 64 files
    int d;
    int emptyDir = -1;
    for (d = 0; (d < MAX_F_NUM); d++)
    {
        if (DIR[d].used)
        {
            if (strcmp(DIR[d].name, name) == 0)
            {
                return FAILURE;          
            }
        }

        if ((emptyDir == -1) && (!DIR[d].used))
            emptyDir = d;
    }
    
    //Check if there are already 64 files in use
    if (emptyDir == -1)
    {
        return FAILURE;
    }
    
    //Find the next open block to use
    int emptyBlock = findEmptyBlock();
    
    //Check that we have found an empty block
    if (emptyBlock == 0)
    {
        return FAILURE;
    }

    //Allocate the directory information
    DIR[emptyDir].used = 1;
    strncpy(DIR[emptyDir].name, name, MAX_F_NAME+1);
    DIR[emptyDir].size = 0;
    DIR[emptyDir].head = (unsigned short) emptyBlock;
    DIR[emptyDir].ref_cnt = 0;

    //Find an empty index in the FAT
    int f = 0;
    while (FAT[f] != EMPTY && (f < FAT_SIZE))
    {
         f++;
    }
    
    //Check if we found a place in the FAT
    if ((f >= FAT_SIZE) || (f+1 >= FAT_SIZE))
    {
        return FAILURE;
    }

    //Add the block to the FAT
    FAT[f] = (unsigned short) emptyBlock;
    FAT[f+1] = END_OF_FILE;

    return SUCCESS;
}

//Deletes the file named name and frees all corresponding meta_info and data
int fs_delete(char* name)
{
    //Check the length of name
    int length = strnlen(name, MAX_F_NAME+1);
    if (length == MAX_F_NAME+1)
    {
        return FAILURE;
    }
    
    //Find the directory entry for this file
    int d;
    for (d = 0; (d < MAX_F_NUM); d++)
        if (strncmp(DIR[d].name, name, length) == 0)
            break;
    
    //Check if a directory entry exists for the file (it should)
    if (d == MAX_F_NUM)
    {
        return FAILURE;
    }

    //Check that no file descriptor exists for name
    if (DIR[d].ref_cnt > 0)
    {
        return FAILURE;
    }
    
    //Remove the files blocks from the FAT
    int f;
    for (f = 0; f < FAT_SIZE; f++)
        if (FAT[f] == DIR[d].head)
            break;
    if (f == FAT_SIZE)
    {
        return FAILURE;
    }
    while (FAT[f] != END_OF_FILE)   //Clear the whole file
    {
        FAT[f] = EMPTY;
        f++;
    }
    FAT[f] = EMPTY; //clear END_OF_FILE index

    //Defragment the FAT
    FATdefrag();

    //Delete the directory information for this file
    DIR[d].used = 0;
    memset(DIR[d].name, 0, sizeof(DIR[d].name));

    return SUCCESS;
}

//File specified by name is opened for reading/writing
int fs_open(char* name)
{
    //Check the length of name
    int length = strnlen(name, MAX_F_NAME+1);
    if (length == MAX_F_NAME+1)
    {
        return FAILURE;
    }
    
    //Find the next open file descriptor
    int fildes;
    for (fildes = 0; fildes < MAX_FILDES; fildes++)
    {
        if(!fildesArray[fildes].used)
            break;
    }
    //Check that there was an open file descriptor
    if (fildes == MAX_FILDES)
    {
        return FAILURE;
    }

    //Find the directory entry for this file
    int d;
    for (d = 0; (d < MAX_F_NUM); d++)
        if (strncmp(DIR[d].name, name, length) == 0)
            break;
    
    //Check if a directory entry exists for the file (it should)
    if (d == MAX_F_NUM)
    {
        return FAILURE;
    }

    //Change directory ref count for the file and update file descriptor array
    DIR[d].ref_cnt++;
    fildesArray[fildes].file = DIR[d].head;
    fildesArray[fildes].offset = 0;
    fildesArray[fildes].used = 1;

    return fildes;
}

//File descriptor fildes is closed
int fs_close(int fildes)
{
    //Check that the file descriptor is within the bounds, and open
    if ((fildes < 0) || (fildes >= MAX_FILDES) || (!fildesArray[fildes].used))
    {
        return FAILURE;
    }   
        
    //Close the file descriptor
    fildesArray[fildes].used = 0;

    //Find the directory entry for the file
    int d = findDirectoryEntry(fildes);
    if (d == FAILURE)
    {
        return FAILURE;
    }

    //Decrement the ref count
    DIR[d].ref_cnt--;

    return SUCCESS;
}

//Read nbyte bytes of data from file specified by fildes into buf
int fs_read(int fildes, void* buf, size_t nbyte)
{
    //Check that the file descriptor is within the bounds, and open
    if ((fildes < 0) || (fildes >= MAX_FILDES) || (!fildesArray[fildes].used))
    {
        return FAILURE;
    }

    //Find the directory entry for the file
    int d = findDirectoryEntry(fildes);
    if (d == FAILURE)
    {
        return FAILURE;
    }

    //Adjust nbytes to fit the size of the file in actuallity
    if ((nbyte+fildesArray[fildes].offset) > DIR[d].size)
    {
        nbyte = DIR[d].size - fildesArray[fildes].offset;
    }

    //Check for offset at the end of the file
    if(nbyte == 0)
    {
        return 0;
    }

    //Find Block containing the offset, and offset into that block
    int firstblock_idx = fildesArray[fildes].offset / BLOCK_SIZE;
    int firstblock_offset = fildesArray[fildes].offset % BLOCK_SIZE;

    //Determine how many blocks need to be read from the file
    int blockNum = ((nbyte + firstblock_offset) / BLOCK_SIZE)  \
                + (((nbyte + firstblock_offset) % BLOCK_SIZE) != 0);

    //Search the FAT for the correct block to read first
    int f;
    for (f = 0; f < FAT_SIZE; f++)
        if (FAT[f] == fildesArray[fildes].file)
            break;
    
    //Index to the correct offset block
    f += firstblock_idx;

    //Read to readBuffer from the disk
    char* readBuffer = (char*) malloc(BLOCK_SIZE*blockNum);
    int i;
    for (i = 0; (FAT[f] != END_OF_FILE) && (i< blockNum); f++, i++)
    {
        block_read(FAT[f], &readBuffer[i*BLOCK_SIZE]);
    }

    //Copy the correct section of readBuffer
    memcpy(buf, &readBuffer[firstblock_offset], nbyte);
    free(readBuffer);

    //Implicitly increment offset
    fildesArray[fildes].offset += nbyte;

    return nbyte;
}

//Writes nbytes of data to fildes from buf
int fs_write(int fildes, void* buf, size_t nbyte)
{ 
    //Check that the file descriptor is within the bounds, and open
    if ((fildes < 0) || (fildes >= MAX_FILDES) || (!fildesArray[fildes].used))
    {
        return FAILURE;
    }

    //Find the directory entry for the file
    int d = findDirectoryEntry(fildes);
    if (d == FAILURE)
    {
        return FAILURE;
    }

    //Find Block containing the offset, and offset into that block
    int firstblock_idx = fildesArray[fildes].offset / BLOCK_SIZE;
    int firstblock_offset = fildesArray[fildes].offset % BLOCK_SIZE;

    //Truncate writes to the limit of 16MB
    if (nbyte > MAX_F_SIZE - fildesArray[fildes].offset)
        nbyte = MAX_F_SIZE - fildesArray[fildes].offset;

    //Get the start of the entry to the FAT for the file
    int f;
    for (f = 0; f < FAT_SIZE; f++)
            if (FAT[f] == fildesArray[fildes].file)
                break;

    //Calculate the number of blocks needed and allocated to the file
    int blocksRequired = ((fildesArray[fildes].offset + nbyte) / BLOCK_SIZE)  \
                        + (((fildesArray[fildes].offset + nbyte) % BLOCK_SIZE) != 0);
    int fileBlocks = ((DIR[d].size) / BLOCK_SIZE) + (((DIR[d].size) % BLOCK_SIZE) != 0);
    if (fileBlocks == 0) fileBlocks = 1;

    //Check if nbyte is within the number of blocks allocated to the file
    if (blocksRequired > fileBlocks)
    {
        //We need to allocate more blocks to the file (if there is space on disk)
        //"Cut" the entry from the FAT
        int i;
        unsigned short* FAT_entry_cpy = calloc(fileBlocks + 1, sizeof(unsigned short));
        for (i=0; (i < fileBlocks) && ((f < FAT_SIZE)); i++)
        {
            FAT_entry_cpy[i] = FAT[f];
            FAT[f++] = EMPTY;
        }
        FAT_entry_cpy[i] = END_OF_FILE;
        FAT[f] = EMPTY;

        //Defragment the FAT to fill in the newly created empty space in the FAT
        FATdefrag();

        //Find the next empty slot in the FAT and "paste" our copy
        for (f = f-i; f < FAT_SIZE; f++) //start f from where we found the file
        {
            if (FAT[f] == EMPTY)
                break;
        }
        for (i=0; (i < (fileBlocks+1)) && ((f < FAT_SIZE)); i++)
        {
            FAT[f++] = FAT_entry_cpy[i];
        }
        free(FAT_entry_cpy);

        //Calculate the number of blocks we need to add if possible
        int addBlocks =  blocksRequired - fileBlocks;

        f--; //This is now the current location of END_OF_FILE for the file

        //Loop through the number of blocks we need to add
        for (i = 0; i < addBlocks; i++)
        {
            //Find the next empty block to use
            int emptyBlock = findEmptyBlock();

            //Check that we have found an empty block
            if (emptyBlock == 0)
                break;

            //Add the empty block to the FAT entry for this file
            FAT[f++] = emptyBlock;
        }

        //Check if we ran out of disk space
        if (i == 0)
        {
            return 0; //Disk entirely out of space for the write
        }
        else if (i < addBlocks)
        {
            blocksRequired -= (addBlocks - i);
            nbyte = (blocksRequired*BLOCK_SIZE) - fildesArray[fildes].offset;
        }
        FAT[f] = END_OF_FILE;

        //set f to the start of the file
        f = f - blocksRequired;
    }

    //Actually do the writing
    char blockBuffer[BLOCK_SIZE] = {0};
    int buffer_iterator = 0;

    //Find the block which contains the offset
    f += firstblock_idx; 

    //Read the block which contains the offset if needed
    int firstBlockWriteBytes = BLOCK_SIZE - (firstblock_offset);
    if (firstBlockWriteBytes)
    {
        block_read(FAT[f], &blockBuffer[0]);

        //Check that the number of bytes to write is not less than the end of the block
        if (firstBlockWriteBytes > nbyte)
        {
            firstBlockWriteBytes = nbyte;
        }

        //Write the needed data to the first block
        memcpy(&blockBuffer[firstblock_offset], buf, firstBlockWriteBytes);
        block_write(FAT[f], &blockBuffer[0]);

        //Update iterators
        buffer_iterator += firstBlockWriteBytes;
        f++;
    }   
    
    //Write the rest of the data a block at a time
    for (; (buffer_iterator < nbyte) && (FAT[f] != END_OF_FILE); f++)
    {
        if((buffer_iterator + BLOCK_SIZE) < nbyte)
        {
            memcpy(&blockBuffer[0], buf + buffer_iterator, BLOCK_SIZE);
            buffer_iterator += BLOCK_SIZE;
        }
        else
        {
            memset(&blockBuffer[0], 0, BLOCK_SIZE);
            memcpy(&blockBuffer[0], buf + buffer_iterator, nbyte - buffer_iterator);
        }

        block_write(FAT[f], &blockBuffer[0]);
    }

    //Update meta information
    if (DIR[d].size < (nbyte + fildesArray[fildes].offset))
        DIR[d].size = nbyte + fildesArray[fildes].offset;
    fildesArray[fildes].offset += nbyte;

    return nbyte;
}

//Returns the size of file referrenced by fildes
int fs_get_filesize(int fildes)
{
    //Check that the file descriptor is within the bounds, and open
    if ((fildes < 0) || (fildes >= MAX_FILDES) || (!fildesArray[fildes].used))
    {
        return FAILURE;
    }
    
    //Find the directory entry for the file
    int d = findDirectoryEntry(fildes);
    if (d == FAILURE)
    {
        return FAILURE;
    }

    return DIR[d].size;
}

//Creates and populates an array of file names currently known to the fs
int fs_listfiles(char*** files)
{
    //Count the number of used directory entries
    int numberOfFiles = 0;
    int d;
    for (d = 0; (d < MAX_F_NUM); d++)
        if(DIR[d].used)
            numberOfFiles++;
    
    //Allocate memory dynamically for the array
    *files = (char**) calloc(numberOfFiles+1, sizeof(char*));
    int i;
    for (i = 0; i < numberOfFiles; i++)
    {
        files[0][i] = (char*) calloc((MAX_F_NAME+1), sizeof(char));
    }
    files[0][numberOfFiles] = (char*) malloc(sizeof(char));

    //Fill in the array
    i = 0;
    for (d = 0; (d < MAX_F_NUM); d++)
    {
        if(DIR[d].used)
        {
            strncpy(files[0][i], DIR[d].name, sizeof(char[MAX_F_NAME + 1]));
            i++;
        }
    }
    files[0][numberOfFiles] = NULL;

    return SUCCESS;
}

//Sets the file pointer to offset, but not beyond EOF
int fs_lseek(int fildes, off_t offset)
{
    //Check that the file descriptor is within the bounds, and open
    if ((fildes < 0) || (fildes >= MAX_FILDES) || (!fildesArray[fildes].used))
    {
        return FAILURE;
    }

    //Find the directory entry for the file
    int d = findDirectoryEntry(fildes);
    if (d == FAILURE)
    {
        return FAILURE;
    }

    //Check that the offset is less than the file size (not seeking beyond the file)
    if((offset < 0) || ((off_t) DIR[d].size < offset))
    {
        return FAILURE;
    }

    //Actually lseek!
    fildesArray[fildes].offset = offset;

    return 0;
}

//file fildes truncated to length bytes
int fs_truncate(int fildes, off_t length)
{
    //Check that the file descriptor is within the bounds, and open
    if ((fildes < 0) || (fildes >= MAX_FILDES) || (!fildesArray[fildes].used))
    {
        return FAILURE;
    }

    //Find the directory entry for the file
    int d = findDirectoryEntry(fildes);
    if (d == FAILURE)
    {
        return FAILURE;
    }

    //Check if length > file size
    if (length > DIR[d].size)
    {
        return FAILURE;
    }

    //Get the start of the entry to the FAT for the file
    int f;
    for (f = 0; f < FAT_SIZE; f++)
            if (FAT[f] == fildesArray[fildes].file)
                break;

    //Remove uneccessary blocks from the FAT (if needed)
    int newBlockNum = (length / BLOCK_SIZE) + ((length % BLOCK_SIZE) != 0);
    int fileBlocks = ((DIR[d].size) / BLOCK_SIZE) + (((DIR[d].size) % BLOCK_SIZE) != 0);
    if (newBlockNum < fileBlocks)
    {
        f += newBlockNum;
        FAT[f] = END_OF_FILE;
        f++;
        for (; f < fileBlocks; f++)
            FAT[f] = EMPTY;
    }
    
    //Remove introduced spaces in the FAT
    FATdefrag();

    DIR[d].size = length;

    return 0;
}