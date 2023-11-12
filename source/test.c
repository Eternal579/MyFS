#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "util.h"

int main(){
    FILE *fp = fopen("diskimg","r+");
    struct DataBlock *tmp_record = malloc(sizeof(struct DataBlock));

    GetSingleDataBlock(ROOT_DIR_TUPLE_BNO, tmp_record);
    struct DirTuple *tuple = (struct DirTuple *)(tmp_record->data);
    printf("DirTuple:   %s!!!!!!\n",tuple->f_name);
    printf("DirTuple:   %s!!!!!!\n",tuple->f_ext);
    printf("DirTuple:   %hu!!!!!!\n",tuple->i_num);

    GetSingleDataBlock(1, tmp_record);
    unsigned int *tmp = (unsigned int *)(tmp_record->data);
    printf("aweqwer: %u((()))\n",*tmp);
    printf("sizeof(unsigned int) is %d\n",sizeof(unsigned int));

    GetSingleDataBlock(6, tmp_record);
    struct Inode *tmp_inode = (struct Inode *)(tmp_record->data);
    printf("%d\n",tmp_inode->st_uid);
    return 0;
}