#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "util.h"

int main(){
    FilePointerInit();
    //struct DataBlock *tmp_record = malloc(sizeof(struct DataBlock));

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

    if (fseek(fp, 0, SEEK_SET) != 0) // 将指针移动到文件的相应的起始位置 
	{
        fprintf(stderr, "block get failed! (func: GetSingleDataBlock)\n");
		return -1;
	}
    // int tmp = 579;
    // fwrite(&tmp, sizeof(int), 1, fp);
    // while(1)
    // {
        
    // }
    //fclose(fp);
    // if (fseek(fp, 0, SEEK_SET) != 0) // 将指针移动到文件的相应的起始位置 
	// {
    //     fprintf(stderr, "block get failed! (func: GetSingleDataBlock)\n");
	// 	return -1;
	// }
    int *q = malloc(sizeof(int));
    if(fread(q, sizeof(int), 1, fp) == 0)
        fprintf(stderr, "error");
    printf("### %d\n", *q);
    fclose(fp);

    return 0;
}