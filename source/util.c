#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


#include "util.h"

FILE *fp;
const char *img_path = "/home/starflow/osExp/MyFS/source/diskimg";
struct SuperBlock *k_super_block;
struct Inode *inodes;

void FilePointerInit() {
    fp = fopen(img_path, "r+");
}

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

	//printf("path is %s&&&\n", path);
	if(strcmp(path, "/") == 0)
	{
		*dir_tuple = *root_tuple;
		//printf("hello\n");
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
 * ino是该目录的inode号
*/
struct DirTuple *GetMultiDirTuples(const int ino)
{
	printf("\ncall GetMultiDirTuples()\n");
	//printf("ino = %d\n",ino);

	struct DataBlock *tuples_block = malloc(sizeof(struct DataBlock));
	off_t dir_size = inodes[ino].st_size; // 此目录的文件大小
	struct DirTuple *tuples = malloc(sizeof(struct DirTuple) * dir_size / sizeof(struct DirTuple));
	ssize_t offset = sizeof(struct DirTuple); 
	off_t base = 0; // 每次cp时的基准偏移量 <=> 目前已读的大小
	for(int i = 0; i < 7; i++)
	{
		if(inodes[ino].addr[i] == -1)
			break;
		if(base == dir_size)
			break;
		//printf("check1\n");	
		if(i >= 0 && i <= 3 && GetSingleDataBlock(inodes[ino].addr[i] + ROOT_DIR_TUPLE_BNO, tuples_block) == 0)
		{
			//printf("check2\n");
			ssize_t tuple_no = 0; 
			ssize_t cur_size = (dir_size - base < 512) ? dir_size - base : 512; // 用于表示当前的数据块的文件大小
			while(tuple_no < cur_size / offset)
			{
				// printf("check3\n");
				// printf("inodes[ino].addr[i] = %d\n", inodes[ino].addr[i]);
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
			ssize_t blocks_cnt_1 = DivideCeil(file_size, 512L); // 表示1级间址对应多少个数据块

			// 存着blocks_cnt_1个short int类型的数字（用于表示叶子数据块号）
			struct DataBlock *primary_dblock = malloc(sizeof(struct DataBlock)); 
			if(GetSingleDataBlock(inodes[ino].addr[i] + ROOT_DIR_TUPLE_BNO, primary_dblock) != 0)
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
			ssize_t blocks_cnt_1 = DivideCeil(file_size, 256L * 512L); // 表示1级间址对应多少个数据块

			// 存着blocks_cnt_1个short int类型的数字（用于表示2级间址的数据块号）
			struct DataBlock *primary_dblock = malloc(sizeof(struct DataBlock)); 
			if(GetSingleDataBlock(inodes[ino].addr[i] + ROOT_DIR_TUPLE_BNO, primary_dblock) != 0)
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
				ssize_t blocks_cnt_2 = (j < blocks_cnt_1) ? 512 : DivideCeil(file_size - (blocks_cnt_1 - 1) * 256L * 512L, 512L);

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
			ssize_t blocks_cnt_1 = DivideCeil(file_size, 256L * 256L * 512L); // 表示1级间址对应多少个数据块

			// 存着blocks_cnt_1个short int类型的数字（用于表示2级间址的数据块号）
			struct DataBlock *primary_dblock = malloc(sizeof(struct DataBlock)); 
			if(GetSingleDataBlock(inodes[ino].addr[i] + ROOT_DIR_TUPLE_BNO, primary_dblock) != 0)
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
					DivideCeil(file_size - (blocks_cnt_1 - 1) * 256L * 256L * 512L, 256L * 512L);

				for(ssize_t k = 0; k < blocks_cnt_2; k++)
				{
					short int *block_no_2 = (short int *)(secondary_dblock + k * sizeof(short int));

					// 存着blocks_cnt_3个short int类型的数字（用于表示叶子数据块号）
					struct DataBlock *triple_dblock = malloc(sizeof(struct DataBlock)); 
					if(GetSingleDataBlock(*block_no_2 + ROOT_DIR_TUPLE_BNO, triple_dblock) != 0)
					{
						fprintf(stderr, "something wrong in triple address! (func: GetMultiDirTuples)");
					}
					ssize_t blocks_cnt_3 = (k < blocks_cnt_2) ? 512 : 
						DivideCeil(file_size - (j * 512L + blocks_cnt_2 - 1) * 256L * 512L, 512L);

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
	printf("call GetMultiDirTuples() successfully!\n");
	return tuples;
}

int GetSingleDataBlock(const int bno, struct DataBlock *d_block)
{
	printf("check3\n");
	if (fseek(fp, BLOCK_SIZE * bno, SEEK_SET) != 0) // 将指针移动到文件的相应的起始位置 
	{
        fprintf(stderr, "block get failed! (func: GetSingleDataBlock)\n");
		return -1;
	}
	printf("check4\n");
	if(fread(d_block,sizeof(struct DataBlock),1,fp) == 0)
	{
		fprintf(stderr, "block get failed! (func: GetSingleDataBlock)\n");
		return -1;
	}
	return 0;
}

short int DistributeIno(ssize_t file_size, bool is_dir)
{
	printf("check1\n");
	short int ino = -1;

	// 先读inode位图
	struct DataBlock *inode_bitmap = malloc(sizeof(struct DataBlock));
	if(GetSingleDataBlock(1, inode_bitmap) != 0)
		fprintf(stderr, "wrong in reading inode bitmap! (func: DistributeIno)");
	bool stop = false;
	printf("check2\n");
	for(ssize_t i = 0; i < BLOCK_SIZE; i += 4)
	{
		unsigned int *res = (unsigned int *)(&inode_bitmap->data[i]);
		unsigned int mask = (1 << 31); int count = 0;
		while(mask > 0)
		{
			if((mask & (*res)) == 0)
			{
				stop = true;
				printf("*res = %u\n", *res);
				break;
			}
			mask >>= 1; count++;
		}
		if(stop) // 找到空的inode了，接下来写回去
		{
			*res |= mask;
			if (fseek(fp, BLOCK_SIZE * 1, SEEK_SET) != 0) // 将指针移动到文件的第二块的起始位置
				fprintf(stderr, "bitmap fseek failed! (func: DistributeIno)\n");
			if (fseek(fp, i, SEEK_CUR) != 0) // 将指针相对当前位置移动 i 个字节
				fprintf(stderr, "bitmap fseek failed! (func: DistributeIno)\n");
			fwrite(res, sizeof(unsigned int), 1, fp); // 已修改inode位图
			ino = (short int)i * 32 + (short int)count;
			break;
		}	
	}

	printf("check3\n");
	printf("ino is %d\n",ino);
	// 已分配好inode号，接下来要分配数据块
	if(DistributeBlockNo(file_size, ino, is_dir) != 0)
	{
		fprintf(stderr, "error in distributing block no! (func: DistributeIno)");
	}
    return ino;
}

int AddToParentDir(unsigned short int parent_ino, char *target, unsigned short int target_ino)
{
	

	inodes[parent_ino].st_nlink = inodes[parent_ino].st_nlink + 1;
	inodes[parent_ino].st_size += 16L;

    return 0;
}

int DistributeBlockNo(ssize_t file_size, int ino, bool is_dir)
{
	printf("check4\n");
	// 先读block位图
	struct DataBlock *block_bitmap = malloc(sizeof(struct DataBlock));
	for(int k = 2; k < 6; k++)
	{
		if(GetSingleDataBlock(k, block_bitmap) != 0)
			fprintf(stderr, "wrong in reading block bitmap! (func: DistributeBlockNo)");
		printf("check5\n");
		bool stop = false;
		for(ssize_t i = 0; i < BLOCK_SIZE; i += 4)
		{
			unsigned int *res = (unsigned int *)(&block_bitmap->data[i]);
			unsigned int mask = (1 << 31); int count = 0;
			while(mask > 0)
			{
				if((mask & (*res)) == 0)
				{
					printf("*res = %u\n", *res);
					stop = true;
					break;
				}
				mask >>= 1; count++;
			}
			if(stop) // 找到空的block了，接下来写回去
			{
				printf("check6\n");
				*res |= mask;
				if (fseek(fp, BLOCK_SIZE * k, SEEK_SET) != 0) // 将指针移动到文件的第k块的起始位置
					fprintf(stderr, "bitmap fseek failed! (func: DistributeBlockNo)\n");
				if (fseek(fp, i, SEEK_CUR) != 0) // 将指针相对当前位置移动 i 个字节
					fprintf(stderr, "bitmap fseek failed! (func: DistributeBlockNo)\n");
				fwrite(res, sizeof(unsigned int), 1, fp); // 已修改block位图
				
				// 修改inodes全局变量
				if(is_dir)
					inodes[ino].st_mode = __S_IFDIR | 0755;
				else
					inodes[ino].st_mode = __S_IFREG | 0744;
				inodes[ino].st_size = file_size;
				inodes[ino].st_nlink = 2;
				inodes[ino].st_ino = ino;
				inodes[ino].addr[0] = (short int)i * 32 + (short int)count; 

				// 把修改的inodes写进diskimg
				if (fseek(fp, BLOCK_SIZE * 6, SEEK_SET) != 0) // 将指针移动到inode区的起始位置
					fprintf(stderr, "inode block fseek failed! (func: DistributeBlockNo)\n");
				if (fseek(fp, ino * sizeof(struct Inode), SEEK_CUR) != 0) // 将指针相对当前位置移动 i 个字节
					fprintf(stderr, "inode block fseek failed! (func: DistributeBlockNo)\n");
				fwrite(&inodes[ino], sizeof(struct Inode), 1, fp); // 已修改inode区
				return 0;
			}	
		}
	}
	return -1; // 已满，无法分配数据块
}

ssize_t *GetLastTupleByIno(int ino)
{
	off_t dir_size = inodes[ino].st_size; // 此目录的文件大小
	off_t count = dir_size / sizeof(struct DirTuple); // 一共有多少目录项

	ssize_t res[5];
	if(dir_size >= 0 && dir_size <= DIRECT_ADDRESS_SCOPE)
	{
		res[0] = (count - 1L) / 32L;
		res[1] = (count - 1L) % 32L;
	}
	else if(dir_size > DIRECT_ADDRESS_SCOPE && dir_size <= DIRECT_ADDRESS_SCOPE + PRIMARY_ADDRESS_SCOPE)
	{
		ssize_t file_size = dir_size - DIRECT_ADDRESS_SCOPE; // 表示1级间址存的文件的大小
		res[0] = 4;
		res[1] = DivideCeil(file_size, 512L);
		res[2] = count - 4L * 32L - (res[1] - 1L) * 32L;
	}
	else if(dir_size > DIRECT_ADDRESS_SCOPE + PRIMARY_ADDRESS_SCOPE && dir_size <= DIRECT_ADDRESS_SCOPE + PRIMARY_ADDRESS_SCOPE + SECONDARY_ADDRESS_SCOPE)
	{
		ssize_t file_size = dir_size - DIRECT_ADDRESS_SCOPE - PRIMARY_ADDRESS_SCOPE; // 表示2级间址存的文件的大小
		res[0] = 5;
		res[1] = DivideCeil(file_size, 256L * 512L);
		res[2] = DivideCeil(file_size - (res[1] - 1L) * 256L * 512L, 512L);
		res[3] = count - 4L *32L - 256L * 32L - (res[1] - 1L) * 256L * 32L - (res[2] - 1L) * 32L;
	}
	else if(dir_size > DIRECT_ADDRESS_SCOPE + PRIMARY_ADDRESS_SCOPE + SECONDARY_ADDRESS_SCOPE&& dir_size <= DIRECT_ADDRESS_SCOPE + PRIMARY_ADDRESS_SCOPE + SECONDARY_ADDRESS_SCOPE + TRIPLE_ADDRESS_SCOPE)
	{
		ssize_t file_size = dir_size - DIRECT_ADDRESS_SCOPE - PRIMARY_ADDRESS_SCOPE - TRIPLE_ADDRESS_SCOPE; // 表示3级间址存的文件的大小
		res[0] = 6;
		res[1] = DivideCeil(file_size, 256L * 256L * 512L);
		res[2] = DivideCeil(file_size - (res[1] - 1L) * 256L * 256L * 512L, 256L * 512L);
		res[3] = DivideCeil(file_size - (res[1] - 1L) * 256L * 256L * 512L - (res[2] - 1L) * 256L * 512L, 512L);
		res[4] = count - 4L *32L - 256L * 32L - (res[1] - 1L) * 256L * 256L * 32L - (res[2] - 1L) * 256L * 32L - (res[3] - 1L) * 32L;
	}
	else
	{
		return NULL;
	}
	return res;
}
