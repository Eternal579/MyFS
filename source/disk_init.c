#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

#include "util.h"

int main(){
    FilePointerInit();

    // 1.超级块初始化
    struct SuperBlock *super_blk = malloc(sizeof(struct SuperBlock));
    {
        super_blk->fs_size = 16384;
        super_blk->first_blk = 518;
        super_blk->data_size = 15818;
        super_blk->first_inode = 6;
        super_blk->inode_area_size = 512;
        super_blk->first_blk_of_inodebitmap = 1;
        super_blk->inode_bitmap_size = 1;
        super_blk->first_blk_of_databitmap = 2;
        super_blk->databitmap_size = 4;
    }

    fwrite(super_blk, sizeof(struct SuperBlock), 1, fp);
    printf("initiate superblock success!\n");

    // 2.初始化Inode位图
    if (fseek(fp, BLOCK_SIZE * 1, SEEK_SET) != 0) // 将指针移动到文件的第二块的起始位置
        fprintf(stderr, "bitmap fseek failed!\n");

    // 目前只有inum为0的inode被分配给根目录，inode位图占一个块，512字节= 128*4B
    unsigned int tmp_1 = 1<<31; 
    fwrite(&tmp_1,sizeof(tmp_1),1,fp); // 写入位图的前32位，从左往右（下标从0开始）
    int tmp_arr_1[127]; 
    memset(tmp_arr_1,0,sizeof(tmp_arr_1)); fwrite(tmp_arr_1,sizeof(tmp_arr_1),1,fp); // 写入inode位图剩下的位，并全部初始为0

    printf("initiate Inode map success!\n");

    // 3.初始化数据块位图
    // 目前只有数据区的第一个数据块被分配给根目录了
    unsigned int tmp_2 = 1<<31;
    fwrite(&tmp_2,sizeof(tmp_2),1,fp); // 写入数据块位图的前32位
    int tmp_arr_2[127 + 128 * 3];
    memset(tmp_arr_2,0,sizeof(tmp_arr_2)); fwrite(tmp_arr_2,sizeof(tmp_arr_2),1,fp); // 写入剩下的位

    printf("initiate block map success!\n");

    // 4. 初始化inode区 大小：512块
    struct Inode inode_table[4096];
    for(int i = 0; i < 4096; i++)
        for(int j = 0; j < 7; j++)
            inode_table[i].addr[j] = -1;
    inode_table[0].st_mode = __S_IFDIR | 0755; // 此inode的st_mode说明为一个文件夹（目录）
    //printf("root dir's mode is %d\n",inode_table[0].st_mode); // 输出16877
    inode_table[0].st_ino = 0;
    inode_table[0].st_nlink = '1'; // 根目录没有父文件夹，只有一个指向自身的目录项
    inode_table[0].st_uid = 579;
    inode_table[0].st_gid = 768288;
    inode_table[0].st_size = 16; // 刚开始只有一个指向自身的目录项，故占16字节
    clock_gettime(CLOCK_REALTIME, &inode_table[0].st_atim); // 获取从UTC时间1970年1月1日零时开始经过的秒数
    inode_table[0].addr[0] = 0; // 根目录的数据块号为0
    fwrite(inode_table,sizeof(inode_table),1,fp);
    printf("initiate inode success!\n");

    // 5. 初始化数据块区大小：15818块
    // 先单独的把根目录写入
    struct DataBlock *tmp_db_1 = malloc(sizeof(struct DataBlock));
    struct DirTuple *tmp_dtuple = malloc(sizeof(struct DirTuple));
    strncpy(tmp_dtuple->f_name, ".", 8);
    memset(tmp_dtuple->f_ext, 0, sizeof(tmp_dtuple->f_ext));
    tmp_dtuple->i_num = 0;
    memset(tmp_dtuple->spare, 0, sizeof(tmp_dtuple->spare));
    memcpy(&tmp_db_1->data[0], tmp_dtuple, sizeof(struct DirTuple));

    fwrite(tmp_db_1, sizeof(struct DataBlock), 1, fp); 
    free(tmp_db_1); free(tmp_dtuple);

    // 再初始化其他的数据块
    struct DataBlock *tmp_db_2 = malloc(sizeof(struct DataBlock));
    fwrite(tmp_db_2, sizeof(struct DataBlock), 15817, fp);
    free(tmp_db_2); 
    fclose(fp);

    printf("initiate data block succes!\n");
}