#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "util.h"

int *func(){
    int arr[2] = {1,2};
    return arr;
}

int main(){
    int *tmp = malloc(sizeof(int) * 2);
    tmp = func();
    printf("%d\n",tmp[0]);
    // FilePointerInit();
    // struct DataBlock *tmp_record = malloc(sizeof(struct DataBlock));

    // GetSingleDataBlock(ROOT_DIR_TUPLE_BNO, tmp_record);
    // struct DirTuple *tuple = (struct DirTuple *)(tmp_record->data);
    // printf("DirTuple:   %s!!!!!!\n",tuple->f_name);
    // printf("DirTuple:   %s!!!!!!\n",tuple->f_ext);
    // printf("DirTuple:   %hu!!!!!!\n",tuple->i_num);

    // GetSingleDataBlock(1, tmp_record);
    // unsigned int *tmp = (unsigned int *)(tmp_record->data);
    // printf("aweqwer: %u((()))\n",*tmp);
    // printf("sizeof(unsigned int) is %d\n",sizeof(unsigned int));

    // GetSingleDataBlock(6, tmp_record);
    // struct Inode *tmp_inode = (struct Inode *)(tmp_record->data);
    // printf("%d\n",tmp_inode->st_uid);
    // printf("sizeof(struct Inode) is %d\n", sizeof(struct Inode));

    // int arr[5] = {1,2,3,4,5};
    // int sz = sizeof(arr) / sizeof(int);
    // printf("sz is %d\n", sz);
    return 0;
}