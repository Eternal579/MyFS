#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "util.h"

const char *img_path = "/home/starflow/osExp/MyFS/source/diskimg";
struct SuperBlock *k_super_block;
struct Inode *inodes;

ssize_t DivideCeil(ssize_t x, ssize_t y)
{
	return (x + y - 1L) / y;
}

int GetSingleDirTuple(const char *path, struct DirTuple *dir_tuple)
{
	//printf("check1\n");
	// 先获取根目录项
	struct DataBlock *cur_dblock = malloc(sizeof(struct DataBlock));
	//printf("check2\n");
	if(GetSingleDataBlock(ROOT_DIR_TUPLE_BNO, cur_dblock) == -1)
	{
		fprintf(stderr, "get root dir fail! (func: GetSingleDirTuple)\n");
		return -1;
	}
	struct DirTuple *root_tuple = (struct DirTuple *)(cur_dblock); // 强转过来即可
	// 获取根目录文件大小
	off_t cur_size = inodes[root_tuple->i_num].st_size;
	//printf("already get root tuple: %s\n",root_tuple->f_name);

	if(strcmp(path, "/") == 0)
	{
		*dir_tuple = *root_tuple;
		return 0;
	}
	else
	{
		int len = strlen(path);
		char *cur_path = (char*)malloc((len) * sizeof(char));
		strncpy(cur_path, path + 1, len - 1);
		cur_path[len - 1] = '\0';
		while(cur_path)
		{
			int cur_len = strlen(cur_path); int s = cur_len;
			for(int i = 0; i < cur_len; i++)
			{
				if(cur_path[i] == '/')
				{
					s = i;
					break;
				}
			}
			char *target = (char *)malloc((s + 1) * sizeof(char)); // target是要寻找的文件夹或文件的名字
			strncpy(target, cur_path, s);
			target[s] = '\0';

			int tuple_no = 0; int offset = sizeof(struct DirTuple); bool find_flag = false;
			while(tuple_no < cur_size / offset)
			{
				/* 以下写法只会修改函数指针副本所指向的内存，并不会把原有内存修改，所以是错误的 */
				// dir_tuple = (struct DirTuple *)(&cur_dblock->data[0] + tuple_no * offset); 
				/* 这样才会修改原有内存的数据 */
				memcpy(dir_tuple, &cur_dblock->data[0] + tuple_no * offset, sizeof(struct DirTuple));
				if(strcmp(dir_tuple->f_ext, "") == 0) // 当前目录项无后缀名
				{
					if(strcmp(dir_tuple->f_name, target) == 0) // 找到target了
					{
						/* 把target所在的数据块存到cur_dblock里 */
						if(GetSingleDataBlock(inodes[dir_tuple->i_num].addr[0] + ROOT_DIR_TUPLE_BNO, cur_dblock) == -1)
						{
							fprintf(stderr, "get target dir tuple fail! (func: GetSingleDirTuple)\n");
							return -1;
						}
						strcpy(cur_path, cur_path + strlen(target) + 1); // 修改cur_path
						find_flag = true;
						break;
					}
				}
				else // 当前目录项有后缀名，需要拼接
				{
					char *integral_name = (char *)malloc(12 * sizeof(char));
					strncpy(integral_name, dir_tuple->f_name, 8);
					strcat(integral_name, dir_tuple->f_ext);
					if(strcmp(integral_name, target) == 0) // 找到target了
					{ 
						/* 把target所在的数据块存到cur_dblock里 */
						if(GetSingleDataBlock(inodes[dir_tuple->i_num].addr[0] + ROOT_DIR_TUPLE_BNO, cur_dblock) == -1)
						{
							fprintf(stderr, "get target dir tuple fail! (func: GetSingleDirTuple)\n");
							return -1;
						}
						strcpy(cur_path, cur_path + strlen(target) + 1); // 修改cur_path
						find_flag = true;
						break;
					}
				}
				tuple_no++;
			}
			if(!find_flag) // path不存在于文件系统中，直接return -1
			{
				return -1;
			}
		}
		return 0;
	}
}

/**
 * 一个数据块512字节，一个目录项16字节，一个数据块能存最多32个目录项
 * Ino是该目录的inode号
*/
struct DirTuple *GetMultiDirTuples(const int Ino)
{
	printf("\ncall GetMultiDirTuples()");
	//printf("Ino = %d\n",Ino);

	struct DataBlock *tuples_block = malloc(sizeof(struct DataBlock));
	off_t dir_size = inodes[Ino].st_size; // 此目录的文件大小
	struct DirTuple *tuples = malloc(sizeof(struct DirTuple) * dir_size / sizeof(struct DirTuple));
	ssize_t offset = sizeof(struct DirTuple); 
	off_t base = 0; // 每次cp时的基准偏移量 <=> 目前已读的大小
	for(int i = 0; i < 7; i++)
	{
		if(inodes[Ino].addr[i] == -1)
			break;
		if(base == dir_size)
			break;
		//printf("check1\n");	
		if(i >= 0 && i <= 3 && GetSingleDataBlock(inodes[Ino].addr[i] + ROOT_DIR_TUPLE_BNO, tuples_block) == 0)
		{
			//printf("check2\n");
			ssize_t tuple_no = 0; 
			ssize_t cur_size = (dir_size - base < 512) ? dir_size - base : 512; // 用于表示当前的数据块的文件大小
			while(tuple_no < cur_size / offset)
			{
				// printf("check3\n");
				// printf("inodes[Ino].addr[i] = %d\n", inodes[Ino].addr[i]);
				memcpy(tuples + base + tuple_no * offset, &tuples_block->data[0] + tuple_no * offset, sizeof(struct DirTuple));
				tuple_no++;
			}
			base += cur_size; 
			// printf("check4\n");
			// printf("base = %ld\n", base);
			// printf("tuple's name = %s\n",tuples->f_name);
		}
		else if(i == 4) // 1级间址
		{ 
			printf("use primary address!\n");

			ssize_t file_size = (dir_size - DIRECT_ADDRESS_SCOPE < PRIMARY_ADDRESS_SCOPE) ? dir_size - DIRECT_ADDRESS_SCOPE 
			: PRIMARY_ADDRESS_SCOPE; // 表示1级间址存的文件的大小
			ssize_t blocks_cnt_1 = DivideCeil(file_size, 512); // 表示1级间址对应多少个数据块

			// 存着blocks_cnt_1个short int类型的数字（用于表示叶子数据块号）
			struct DataBlock *primary_dblock = malloc(sizeof(struct DataBlock)); 
			if(GetSingleDataBlock(inodes[Ino].addr[i] + ROOT_DIR_TUPLE_BNO, primary_dblock) != 0)
			{
				fprintf(stderr, "something wrong in primary address! (func: GetMultiDirTuples)");
			}

			for(ssize_t j = 0; j < blocks_cnt_1; j++)
			{
				short int *block_no_1 = (short int *)(primary_dblock + j * sizeof(short int));
				if(GetSingleDataBlock(*block_no_1 + ROOT_DIR_TUPLE_BNO, tuples_block) != 0)
				{
					ssize_t tuple_no = 0; 
					ssize_t cur_size = (dir_size - base < 512) ? dir_size - base : 512; // 用于表示当前的数据块的文件大小
					while(tuple_no < cur_size / offset)
					{
						memcpy(tuples + base + tuple_no * offset, tuples_block + tuple_no * offset, sizeof(struct DirTuple));
						tuple_no++;
					}
					base += cur_size;
				}
			}
		}
		else if(i == 5) // 2级间址
		{
			printf("use secondary address!\n");

			ssize_t file_size = (dir_size - DIRECT_ADDRESS_SCOPE - PRIMARY_ADDRESS_SCOPE < SECONDARY_ADDRESS_SCOPE) ? 
			dir_size - DIRECT_ADDRESS_SCOPE - PRIMARY_ADDRESS_SCOPE 
			: SECONDARY_ADDRESS_SCOPE;  // 表示2级间址存的文件的大小
			ssize_t blocks_cnt_1 = DivideCeil(file_size, 512 * 512); // 表示1级间址对应多少个数据块

			// 存着blocks_cnt_1个short int类型的数字（用于表示2级间址的数据块号）
			struct DataBlock *primary_dblock = malloc(sizeof(struct DataBlock)); 
			if(GetSingleDataBlock(inodes[Ino].addr[i] + ROOT_DIR_TUPLE_BNO, primary_dblock) != 0)
			{
				fprintf(stderr, "something wrong in primary address! (func: GetMultiDirTuples)");
			}

			for(ssize_t j = 0; j < blocks_cnt_1; j++)
			{
				short int *block_no_1 = (short int *)(primary_dblock + j * sizeof(short int));

				// 存着blocks_cnt_2个short int类型的数字（用于表示叶子数据块号）
				struct DataBlock *secondary_dblock = malloc(sizeof(struct DataBlock)); 
				if(GetSingleDataBlock(*block_no_1 + ROOT_DIR_TUPLE_BNO, secondary_dblock) != 0)
				{
					fprintf(stderr, "something wrong in secondary address! (func: GetMultiDirTuples)");
				}
				ssize_t blocks_cnt_2 = (j < blocks_cnt_1) ? 512 : DivideCeil(file_size - (blocks_cnt_1 - 1) * 512L * 512L, 512L);

				for(ssize_t k = 0; k < blocks_cnt_2; k++)
				{
					// block_no_2才是叶子数据块的块号
					short int *block_no_2 = (short int *)(secondary_dblock + k * sizeof(short int));
					if(GetSingleDataBlock(*block_no_2 + ROOT_DIR_TUPLE_BNO, tuples_block) != 0) 
					{
						ssize_t tuple_no = 0; 
						ssize_t cur_size = (dir_size - base < 512) ? dir_size - base : 512; // 用于表示当前的数据块的文件大小
						while(tuple_no < cur_size / offset)
						{
							memcpy(tuples + base + tuple_no * offset, tuples_block + tuple_no * offset, sizeof(struct DirTuple));
							tuple_no++;
						}
						base += cur_size;
					}
				}
			}
		}
		else if(i == 6) // 3级间址
		{
			printf("use triple address!\n");

			ssize_t file_size = (dir_size - DIRECT_ADDRESS_SCOPE - PRIMARY_ADDRESS_SCOPE - SECONDARY_ADDRESS_SCOPE < TRIPLE_ADDRESS_SCOPE) ? 
			dir_size - DIRECT_ADDRESS_SCOPE - PRIMARY_ADDRESS_SCOPE - SECONDARY_ADDRESS_SCOPE 
			: TRIPLE_ADDRESS_SCOPE;  // 表示3级间址存的文件的大小
			ssize_t blocks_cnt_1 = DivideCeil(file_size, 512 * 512 * 512); // 表示1级间址对应多少个数据块

			// 存着blocks_cnt_1个short int类型的数字（用于表示2级间址的数据块号）
			struct DataBlock *primary_dblock = malloc(sizeof(struct DataBlock)); 
			if(GetSingleDataBlock(inodes[Ino].addr[i] + ROOT_DIR_TUPLE_BNO, primary_dblock) != 0)
			{
				fprintf(stderr, "something wrong in primary address! (func: GetMultiDirTuples)");
			}

			for(ssize_t j = 0; j < blocks_cnt_1; j++)
			{
				short int *block_no_1 = (short int *)(primary_dblock + j * sizeof(short int));

				// 存着blocks_cnt_2个short int类型的数字（用于表示叶子数据块号）
				struct DataBlock *secondary_dblock = malloc(sizeof(struct DataBlock)); 
				if(GetSingleDataBlock(*block_no_1 + ROOT_DIR_TUPLE_BNO, secondary_dblock) != 0)
				{
					fprintf(stderr, "something wrong in secondary address! (func: GetMultiDirTuples)");
				}
				ssize_t blocks_cnt_2 = (j < blocks_cnt_1) ? 512 : 
					DivideCeil(file_size - (blocks_cnt_1 - 1) * 512L * 512L * 512L, 512L * 512L);

				for(ssize_t k = 0; k < blocks_cnt_2; k++)
				{
					short int *block_no_2 = (short int *)(secondary_dblock + k * sizeof(short int));

					// 存着blocks_cnt_3个short int类型的数字（用于表示叶子数据块号）
					struct DataBlock *triple_dblock = malloc(sizeof(struct DataBlock)); 
					if(GetSingleDataBlock(*block_no_2 + ROOT_DIR_TUPLE_BNO, triple_dblock) != 0)
					{
						fprintf(stderr, "something wrong in triple address! (func: GetMultiDirTuples)");
					}
					// todo
					ssize_t blocks_cnt_3 = (k < blocks_cnt_2) ? 512 : 
						DivideCeil(file_size - (j * 512L + blocks_cnt_2 - 1) * 512L * 512L, 512L);

					for(ssize_t p = 0; p < blocks_cnt_3; p++)
					{
						// block_no_3才是叶子数据块的块号
						short int *block_no_3 = (short int *)(triple_dblock + p * sizeof(short int));
						if(GetSingleDataBlock(*block_no_3 + ROOT_DIR_TUPLE_BNO, tuples_block) != 0) 
						{
							ssize_t tuple_no = 0; 
							ssize_t cur_size = (dir_size - base < 512) ? dir_size - base : 512; // 用于表示当前的数据块的文件大小
							while(tuple_no < cur_size / offset)
							{
								memcpy(tuples + base + tuple_no * offset, tuples_block + tuple_no * offset, sizeof(struct DirTuple));
								tuple_no++;
							}
							base += cur_size;
						}
					}
				}
			}
		}
	}
	printf("\n");
	return tuples;
}

int GetSingleDataBlock(const int bno, struct DataBlock *d_block)
{
	FILE *fp = fopen(img_path,"r+");
	//printf("check3\n");
	if (fseek(fp, BLOCK_SIZE * bno, SEEK_SET) != 0) // 将指针移动到文件的相应的起始位置 
	{
        fprintf(stderr, "block get failed! (func: GetSingleDataBlock)\n");
		return -1;
	}
	//printf("check4\n");
	if(fread(d_block,sizeof(struct DataBlock),1,fp) == 0)
	{
		fprintf(stderr, "block get failed! (func: GetSingleDataBlock)\n");
		return -1;
	}
	fclose(fp);
	return 0;
}

short int DistributeIno(ssize_t file_size)
{
	short int ino = -1;

	// 先读inode位图
	struct DataBlock *inode_bitmap = malloc(sizeof(struct DataBlock));
	if(GetSingleDataBlock(1, inode_bitmap) != 0)
		fprintf(stderr, "wrong in reading inode bitmap! (func: DistributeIno)");
	bool stop = false;
	for(ssize_t i = 0; i < BLOCK_SIZE; i += 4)
	{
		unsigned int *res = (unsigned int *)(&inode_bitmap->data[i]);
		unsigned int mask = (1 << 31); int count = 0;
		while(mask > 0)
		{
			if((mask & (*res)) == 0)
			{
				stop = true;
				break;
			}
			mask >>= 1; count++;
		}
		if(stop) // 找到空的inode了，接下来写回去
		{
			*res |= mask;
			FILE* fp = NULL;
			if(!(fp = fopen(img_path, "r+")))
				fprintf(stderr, "file open fail (func: DistributeIno)\n");
			if (fseek(fp, BLOCK_SIZE * 1, SEEK_SET) != 0) // 将指针移动到文件的第二块的起始位置
				fprintf(stderr, "bitmap fseek failed! (func: DistributeIno)\n");
			if (fseek(fp, i, SEEK_CUR) != 0) // 将指针相对当前位置移动 i 个字节
				fprintf(stderr, "bitmap fseek failed! (func: DistributeIno)\n");
			fwrite(res, sizeof(unsigned int), 1, fp); // 已修改inode位图
			ino = (short int)i * 32 + (short int)count;
			break;
		}	
	}

	// 已分配好inode号，接下来要分配数据块
	if(DistributeBlockNo(file_size, ino) != 0)
	{
		fprintf(stderr, "error in distributing block no! (func: DistributeIno)");
	}
	inodes[ino].st_size = file_size;
	inodes[ino].st_nlink = '2';
	inodes[ino].ino = ino;
	
    return ino;
}

int AddToParentDir(unsigned short int parent_ino, char *target, unsigned short int target_ino)
{
    return 0;
}

int DistributeBlockNo(ssize_t file_size, int ino)
{
	FILE* fp = NULL;
	if(!(fp = fopen(img_path, "r+")))
		fprintf(stderr, "file open fail (func: DistributeBlockNo)\n");

	// 先读block位图
	struct DataBlock *block_bitmap = malloc(sizeof(struct DataBlock));
	for(int k = 2; k < 6; k++)
	{
		if(GetSingleDataBlock(k, block_bitmap) != 0)
			fprintf(stderr, "wrong in reading block bitmap! (func: DistributeBlockNo)");
		bool stop = false;
		for(ssize_t i = 0; i < BLOCK_SIZE; i += 4)
		{
			unsigned int *res = (unsigned int *)(&inode_bitmap->data[i]);
			unsigned int mask = (1 << 31); int count = 0;
			while(mask > 0)
			{
				if((mask & (*res)) == 0)
				{
					stop = true;
					break;
				}
				mask >>= 1; count++;
			}
			if(stop) // 找到空的block了，接下来写回去
			{
				*res |= mask;
				FILE* fp = NULL;
				if (fseek(fp, BLOCK_SIZE * k, SEEK_SET) != 0) // 将指针移动到文件的第k块的起始位置
					fprintf(stderr, "bitmap fseek failed! (func: DistributeBlockNo)\n");
				if (fseek(fp, i, SEEK_CUR) != 0) // 将指针相对当前位置移动 i 个字节
					fprintf(stderr, "bitmap fseek failed! (func: DistributeBlockNo)\n");
				fwrite(res, sizeof(unsigned int), 1, fp); // 已修改inode位图
				inodes[ino].addr[0] = (short int)i * 32 + (short int)count;
				return 0;
			}	
		}
	}
	return -1; // 已满，无法分配数据块
}
