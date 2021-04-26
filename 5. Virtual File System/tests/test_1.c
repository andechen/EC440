#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "fs.h"
#include "disk.h"

int main() {
  char* disk_name = "mydisk";
  

  assert(make_fs(disk_name) == 0);

  assert(mount_fs(disk_name) == 0);
  
  assert(fs_create("myfile") == 0);

  

  assert(umount_fs(disk_name) == 0);



  assert(mount_fs(disk_name) == 0);

  assert(fs_delete("myfile") == 0);

  assert(umount_fs(disk_name) == 0); 
  
  return 0;
}