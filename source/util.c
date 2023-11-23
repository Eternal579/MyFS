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
	// 先获取根目录项
	struct DataBlock *cur_dblock = malloc(sizeof(struct DataBlock));
	if(GetSingleDataBlock(ROOT_DIR_TUPLE_BNO, cur_dblock) == -1)
	{
		fprintf(stderr, "get root dir fail! (func: GetSingleDirTuple)\n");
		return -1;
	}
	struct DirTuple *root_tuple = (struct DirTuple *)(cur_dblock); // 强转过来即可
	*dir_tuple = *root_tuple;

	if(strcmp(path, "/") == 0)
	{
		return 0;
	}
	else
	{
		// printf("###########################\n");

		int len = strlen(path);
		char *cur_path = malloc((len) * sizeof(char));
		strcpy(cur_path, path + 1);
		// printf("cur_path is %s\n", cur_path);

		while(strlen(cur_path) != 0)
		{
			// 获取dir目录文件大小
			off_t cur_size = inodes[dir_tuple->i_num].st_size;

			// 处理出target来
			int cur_len = strlen(cur_path); int s = cur_len;
			for(int i = 0; i < cur_len; i++)
			{
				if(cur_path[i] == '/')
				{
					s = i;
					break;
				}
			}
			char *target = malloc((s + 1) * sizeof(char)); // target是要寻找的文件夹或文件的名字
			strncpy(target, cur_path, s);
			target[s] = '\0';
			// printf("target is %s\n", target);

			ssize_t tuple_no = 0; ssize_t offset = sizeof(struct DirTuple); bool find_flag = false;
			struct DirTuple *tuples = malloc(cur_size);
			tuples = GetMultiDirTuples(dir_tuple->i_num);

			// printf("cur_size / offset is %d\n", cur_size / offset);
			while(tuple_no < cur_size / offset)
			{
				*dir_tuple = tuples[tuple_no];
				if(strcmp(dir_tuple->f_ext, "") == 0) // 当前目录项无后缀名
				{
					// printf("tuple_no is %d, dir_tuple->f_name is %s\n", tuple_no, dir_tuple->f_name);
					if(strcmp(dir_tuple->f_name, target) == 0) // 找到target了
					{
						// printf("target found!\n");

						// 修改cur_path
						if(strlen(target) == strlen(cur_path)) 
						{
							cur_path = "";
						}
						else 
						{
							strcpy(cur_path, cur_path + strlen(target) + 1); 
						}

						// printf("cur_path is $$%s$$\n", cur_path);
						find_flag = true;
						break;
					}
				}
				else // 当前目录项有后缀名，需要拼接
				{
					char *integral_name = (char *)malloc(12 * sizeof(char));
					strncpy(integral_name, dir_tuple->f_name, 8);
					strcat(integral_name, ".");
					strcat(integral_name, dir_tuple->f_ext);
					if(strcmp(integral_name, target) == 0) // 找到target了
					{
						// 修改cur_path
						if(strlen(target) == strlen(cur_path)) 
						{
							cur_path = "";
						}
						else 
						{
							strcpy(cur_path, cur_path + strlen(target) + 1); 
						}

						// printf("cur_path is $$%s$$\n", cur_path);
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
		//printf("###########################\n");
		return 0;
	}
}

/**
 * 一个数据块512字节，一个目录项16字节，一个数据块能存最多32个目录项
 * ino是该目录的inode号
*/
struct DirTuple *GetMultiDirTuples(const int ino)
{
	// printf("call GetMultiDirTuples()\n");
	// printf("ino = %d\n",ino);

	struct DataBlock *tuples_block = malloc(sizeof(struct DataBlock));
	off_t dir_size = inodes[ino].st_size; // 此目录的文件大小
	// printf("dir_size is %ld\n", dir_size);
	struct DirTuple *tuples = malloc(dir_size);
	off_t base = 0; // 每次cp时的基准偏移量 <=> 目前已读的大小
	for(int i = 0; i < 7; i++)
	{
		if(inodes[ino].addr[i] == -1)
			break;
		if(base == dir_size)
			break;
		if(i >= 0 && i <= 3 && GetSingleDataBlock(inodes[ino].addr[i] + ROOT_DIR_TUPLE_BNO, tuples_block) == 0)
		{
			// printf("dir_size is %ld\n", dir_size);
			ssize_t cur_size = (dir_size - base < 512) ? dir_size - base : 512; // 用于表示当前的数据块的文件大小
			// printf("base is %ld, cur_size is %ld\n", base, cur_size);
			// printf("check1\n");

			memcpy((void *)tuples + base, (void *)tuples_block, cur_size);
			// printf("check2\n");
			base += cur_size; 
		}
		else if(i == 4) // 1级间址
		{ 
			printf("use primary address!\n");

			ssize_t file_size = (dir_size - DIRECT_ADDRESS_SCOPE < PRIMARY_ADDRESS_SCOPE) ? dir_size - DIRECT_ADDRESS_SCOPE 
			: PRIMARY_ADDRESS_SCOPE; // 表示1级间址存的文件的大小
			ssize_t blocks_cnt_1 = DivideCeil(file_size, 512L); // 表示1级间址对应多少个数据块
			// printf("1级间址对应着 %ld 个数据块\n", blocks_cnt_1);

			// 存着blocks_cnt_1个short int类型的数字（用于表示叶子数据块号）
			struct DataBlock *primary_dblock = malloc(sizeof(struct DataBlock)); 
			// printf("inodes[ino].addr[4] is %hd\n", inodes[ino].addr[i]);
			if(GetSingleDataBlock(inodes[ino].addr[i] + ROOT_DIR_TUPLE_BNO, primary_dblock) != 0)
			{
				fprintf(stderr, "something wrong in primary address! (func: GetMultiDirTuples)");
			}

			for(ssize_t j = 0; j < blocks_cnt_1; j++)
			{
				short int *block_no_1 = (short int *)(&primary_dblock->data[j * sizeof(short int)]);
				// printf("block_no_1 is %hd\n", *block_no_1);
				if(GetSingleDataBlock(*(block_no_1) + ROOT_DIR_TUPLE_BNO, tuples_block) == 0)
				{
					ssize_t cur_size = (dir_size - base < 512) ? dir_size - base : 512; // 用于表示当前的数据块的文件大小
					memcpy((void *)tuples + base, (void *)tuples_block, cur_size);
					base += cur_size;

					// printf("cur_size is %ld\n", cur_size);
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
				short int *block_no_1 = (short int *)(&primary_dblock->data[j * sizeof(short int)]);

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
					short int *block_no_2 = (short int *)(&secondary_dblock->data[k * sizeof(short int)]);
					if(GetSingleDataBlock(*block_no_2 + ROOT_DIR_TUPLE_BNO, tuples_block) == 0) 
					{
						ssize_t cur_size = (dir_size - base < 512) ? dir_size - base : 512; // 用于表示当前的数据块的文件大小
						memcpy((void *)tuples + base, (void *)tuples_block, cur_size);
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
				short int *block_no_1 = (short int *)(&primary_dblock->data[j * sizeof(short int)]);

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
					short int *block_no_2 = (short int *)(&secondary_dblock->data[k * sizeof(short int)]);

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
						short int *block_no_3 = (short int *)(&triple_dblock[p * sizeof(short int)]);
						if(GetSingleDataBlock(*block_no_3 + ROOT_DIR_TUPLE_BNO, tuples_block) == 0) 
						{
							ssize_t cur_size = (dir_size - base < 512) ? dir_size - base : 512; // 用于表示当前的数据块的文件大小
							memcpy((void *)tuples + base, (void *)tuples_block, cur_size);
							base += cur_size;
						}
					}
				}
			}
		}
	}
	// printf("call GetMultiDirTuples() successfully!\n");
	return tuples;
}

int GetSingleDataBlock(const int bno, struct DataBlock *d_block)
{
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
	return 0;
}

int create_file(const char *path, bool is_dir)
{
	int path_len = strlen(path);
	int s = path_len - 1;
	for(; s >= 0; s--)
	{
		if (path[s] == '/')
			break;
	}
	char parent_path[path_len]; // 父目录的路径
	strncpy(parent_path, path, s + 1); 
	parent_path[s + 1] = '\0';
	// printf("parent_path is %s\n", parent_path);

	struct DirTuple *parent_dir = malloc(sizeof(struct DirTuple)); // 父目录项
	if(GetSingleDirTuple(parent_path, parent_dir) != 0)
	{
		fprintf(stderr, "parent_dir cannot be found! (func: create_file)");
	}

	unsigned short int parent_ino = parent_dir->i_num;
	ssize_t count = inodes[parent_ino].st_size / sizeof(struct DirTuple); // 表示父文件夹中有多少个目录项
	struct DirTuple *tuples = malloc(sizeof(struct DirTuple) * count);
	tuples = GetMultiDirTuples(parent_ino);
	// printf("count is %ld\n", count);

	// 检查是否已存在
	char target[path_len - s];
	strcpy(target, path + s + 1);
	for(ssize_t i = 0; i < count; i++)
	{
		char name[16];
		strcpy(name, tuples[i].f_name);
		if (strlen(tuples[i].f_ext) != 0)
		{
			strcat(name, ".");
			strcat(name, tuples[i].f_ext);
		}
		if(strcmp(name, target) == 0)
		{
			return -EEXIST;
		}
	}
	
	// 检查命名是否规范
	char f_name[path_len]; char f_ext[path_len];
	int target_len = strlen(target);
	int i = target_len - 1;
	for(; i >= 0; i--)
	{
		if(target[i] == '.')
			break;
	}
	if(i == -1) 
	{
		strcpy(f_name, target);
		memset(f_ext, 0, sizeof(f_ext));
	}
	else
	{
		strncpy(f_name, target, i);
		strcpy(f_ext, target + i + 1);
	}
	if(is_dir)
	{
		if(strlen(f_name) > 8)
			return -ENAMETOOLONG;
	}	
	else
	{
		if(strlen(f_name) > 8 || strlen(f_ext) > 3)
			return -ENAMETOOLONG;
	}

	// 可以确认regular或dir均不在父文件夹下
	int target_ino;
	if(is_dir)
		target_ino = DistributeIno(DEFAULT_DIR_SIZE, is_dir); // 分配inode号
	else
		target_ino = DistributeIno(0L, is_dir); // 分配inode号

	// 为新建文件夹的数据块创造两条默认目录项
	if(is_dir)
	{
		printf("Now! create default tuples for %s, whose ino is %d, and 第一个数据块块号为 %hd\n",target, target_ino, inodes[target_ino].addr[0]);
		if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[0]), SEEK_SET) != 0) // 将指针移动到数据块的起始位置
			fprintf(stderr, "new block fseek failed! (func: DistributeBlockNo)\n");

		struct DirTuple *default_tuple1 = malloc(sizeof(struct DirTuple));
		strncpy(default_tuple1->f_name, ".", 8);
		memset(default_tuple1->f_ext, 0, sizeof(default_tuple1->f_ext));
		default_tuple1->i_num = target_ino;
		memset(default_tuple1->spare, 0, sizeof(default_tuple1->spare));
		fwrite(default_tuple1, sizeof(struct DirTuple), 1, fp);

		struct DirTuple *default_tuple2 = malloc(sizeof(struct DirTuple));
		strncpy(default_tuple2->f_name, "..", 8);
		memset(default_tuple2->f_ext, 0, sizeof(default_tuple1->f_ext));
		default_tuple2->i_num = parent_ino;
		memset(default_tuple2->spare, 0, sizeof(default_tuple1->spare));
		fwrite(default_tuple2, sizeof(struct DirTuple), 1, fp);
	}

	if(AddToParentDir(parent_ino, target, target_ino) != 0) // 添加一条记录到父目录中
	{
		fprintf(stderr, "something wrong when adding target to parent dir! (func: create_file)");
	}
	return 0;
}

short int DistributeIno(ssize_t file_size, bool is_dir)
{
	// printf("DistributeIno start!\n");
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
			if (fseek(fp, BLOCK_SIZE * 1, SEEK_SET) != 0) // 将指针移动到文件的第二块的起始位置
				fprintf(stderr, "bitmap fseek failed! (func: DistributeIno)\n");
			if (fseek(fp, i, SEEK_CUR) != 0) // 将指针相对当前位置移动 i 个字节
				fprintf(stderr, "bitmap fseek failed! (func: DistributeIno)\n");
			fwrite(res, sizeof(unsigned int), 1, fp); // 已修改inode位图
			ino = (short int)i * 8 + (short int)count;
			break;
		}	
	}

	// printf("ino is %hd\n", ino);
	// 已分配好inode号，接下来要分配数据块
	if(DistributeBlockNo(file_size, ino, is_dir) != 0)
	{
		fprintf(stderr, "error in distributing block no! (func: DistributeIno)");
	}
	// printf("DistributeIno called successfully!\n");
    return ino;
}

int AddToParentDir(unsigned short int parent_ino, char *target, unsigned short int target_ino) 
{
	// printf("AddToParentDir start!\n");
	ssize_t *res = malloc(sizeof(ssize_t) * 5);
	if((res = GetLastTupleByIno(parent_ino)) == NULL)
		fprintf(stderr, "can't get last tuple! (func: AddToParentDir)");
	
	if(res[0] >= 0 && res[0] <= 3)
	{
		if(res[1] < 32)
		{
			// printf("res[0] = %ld, res[1] = %ld\n", res[0], res[1]);
			if (fseek(fp, BLOCK_SIZE * (inodes[parent_ino].addr[res[0]] + ROOT_DIR_TUPLE_BNO) + res[1] * sizeof(struct DirTuple), SEEK_SET) != 0)
				fprintf(stderr, "fseek failed! (func: AddToParentDir)\n");
		}
		else // addr[res[0]]表示的数据块被用满了，需要使用后面的了
		{
			// printf("addr[res[0]]表示的数据块被用满了，需要使用后面的了\n");
			if(res[0] >= 0 && res[0] <= 2)
			{
				// printf("还没必要用1级间址\n");
				DistributeBlockNo(inodes[parent_ino].st_size + 16L, parent_ino, true);
				if (fseek(fp, BLOCK_SIZE * (inodes[parent_ino].addr[res[0] + 1] + ROOT_DIR_TUPLE_BNO), SEEK_SET) != 0)
					fprintf(stderr, "fseek failed! (func: AddToParentDir)\n");
			}
			else // 需要使用1级间址了
			{
				// printf("需要使用1级间址了\n");
				DistributeBlockNo(inodes[parent_ino].st_size + 16L, parent_ino, true);

				struct DataBlock *tmp_db = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(inodes[parent_ino].addr[res[0] + 1] + ROOT_DIR_TUPLE_BNO, tmp_db);
				short int *first_leaf_blk = (short int *)(tmp_db->data);
				if (fseek(fp, BLOCK_SIZE * (*first_leaf_blk + ROOT_DIR_TUPLE_BNO), SEEK_SET) != 0)
					fprintf(stderr, "fseek failed! (func: AddToParentDir)\n");
			}
		}
	}
	else if(res[0] == 4)
	{
		if(res[1] < 256)
		{
			if(res[2] < 32)
			{
				// printf("现在向1级间址的叶子数据块添加一个目录项\n");
				// printf("res[2] = %ld\n", res[2]);
				struct DataBlock *tmp_db = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(inodes[parent_ino].addr[res[0]] + ROOT_DIR_TUPLE_BNO, tmp_db);
				short int *target_leaf_blk = (short int *)(&tmp_db->data[(res[1] - 1L) * sizeof(short int)]);
				// printf("target_leaf_blk is %hd\n", *target_leaf_blk);
				if (fseek(fp, BLOCK_SIZE * ((*target_leaf_blk) + ROOT_DIR_TUPLE_BNO) + res[2] * sizeof(struct DirTuple), SEEK_SET) != 0)
					fprintf(stderr, "fseek failed! (func: AddToParentDir)\n");
			}
			else // 需要再扩充一个叶子数据块
			{
				// printf("addr[4]-res[1]表示的叶子数据块被用满了，需要新弄一个叶子数据块\n");
				DistributeBlockNo(inodes[parent_ino].st_size + 16L, parent_ino, true);

				struct DataBlock *tmp_db = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(inodes[parent_ino].addr[res[0]] + ROOT_DIR_TUPLE_BNO, tmp_db);
				short int *target_leaf_blk = (short int *)(&tmp_db->data[res[1] * sizeof(short int)]);
				// printf("target_leaf_blk is %hd\n", *target_leaf_blk);
				if (fseek(fp, BLOCK_SIZE * ((*target_leaf_blk) + ROOT_DIR_TUPLE_BNO), SEEK_SET) != 0)
					fprintf(stderr, "fseek failed! (func: AddToParentDir)\n");
			}
		}
		else // 1级间址也用满了，需要申请二级间址
		{

		}
	}
	else if(res[0] == 5)
	{

	}
	else if(res[0] == 6)
	{

	}

	struct DirTuple *new_tuple = malloc(sizeof(struct DirTuple));
	int target_len = strlen(target);
	int i = target_len - 1;
	for(; i >= 0; i--)
	{
		if(target[i] == '.')
			break;
	}
	if(i == -1) 
	{
		strcpy(new_tuple->f_name, target);
		memset(new_tuple->f_ext, 0, sizeof(new_tuple->f_ext)); // 这句话千万不能忘，不然出一堆bug
	}
	else
	{
		strncpy(new_tuple->f_name, target, i);
		strcpy(new_tuple->f_ext, target + i + 1);
	}
	new_tuple->i_num = target_ino;
	memset(new_tuple->spare, 0, sizeof(new_tuple->spare));
	fwrite(new_tuple, sizeof(struct DirTuple), 1, fp);
	free(new_tuple);

	inodes[parent_ino].st_nlink ++;
	inodes[parent_ino].st_size += 16L;
	ModifyInodeZone(parent_ino);

	// printf("AddToParentDir called successfully!\n");
    return 0;
}

int DistributeBlockNo(ssize_t file_size, int ino, bool is_dir)
{
	// printf("DistributeBlockNo start!\n");
	// 先读block位图
	struct DataBlock *block_bitmap = malloc(sizeof(struct DataBlock));
	for(int k = 2; k < 6; k++)
	{
		if(GetSingleDataBlock(k, block_bitmap) != 0)
			fprintf(stderr, "wrong in reading block bitmap! (func: DistributeBlockNo)");
		bool stop = false;
		for(ssize_t i = 0; i < BLOCK_SIZE; i += 4)
		{
			// printf("i is %ld\n", i);
			unsigned int *res = (unsigned int *)(&block_bitmap->data[i]);
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
			// printf("stop is %d\n", stop);
			// printf("count is %d\n", count);
			if(stop) // 找到空的block了，接下来写回去
			{
				*res |= mask;
				if (fseek(fp, BLOCK_SIZE * k, SEEK_SET) != 0) // 将指针移动到文件的第k块的起始位置
					fprintf(stderr, "bitmap fseek failed! (func: DistributeBlockNo)");
				if (fseek(fp, i, SEEK_CUR) != 0) // 将指针相对当前位置移动 i 个字节
					fprintf(stderr, "bitmap fseek failed! (func: DistributeBlockNo)");
				fwrite(res, sizeof(unsigned int), 1, fp); // 已修改block位图

				if(inodes[ino].addr[0] == -1) // 可以使用此条件来判断是新建一个文件夹，还是为文件夹或文件拓展一个数据块
				{
					// 修改inodes全局变量
					if(is_dir)
						inodes[ino].st_mode = __S_IFDIR | 0755;
					else
						inodes[ino].st_mode = __S_IFREG | 0666;
					inodes[ino].st_size = file_size;
					inodes[ino].st_nlink = 2;
					inodes[ino].st_ino = ino;
					inodes[ino].addr[0] = (short int)i * 8 + (short int)count + (short int)(k -2) * 4096; 
					printf("ino = %d, inodes[ino].addr[0] = %hd\n", ino, inodes[ino].addr[0]);
					clock_gettime(CLOCK_REALTIME, &inodes[ino].st_atim);
					// printf("为这个新建文件夹分配的第一个数据块号为 %hd\n", inodes[ino].addr[0]);
				}
				else
				{
					// printf("现在为文件夹拓展一个数据块\n");
					for(int p = 1; p < 7; p++)
					{
						// printf("p = %d, inodes[ino].addr[p] = %hd\n", p, inodes[ino].addr[p]);
						if(p >= 1 && p <= 3)
						{
							if(inodes[ino].addr[p] == -1)
							{
								inodes[ino].addr[p] = (short int)i * 8 + (short int)count + (short int)(k -2) * 4096; 
								// printf("inodes[ino].addr[p] = %hd\n", inodes[ino].addr[p]);
								// printf("check4\n");
								break;
							}
						}
						else if(p == 4)
						{
							// printf("$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
							if(inodes[ino].addr[p] == -1)
							{
								// printf("分配数据块用于存储256个short int\n");
								inodes[ino].addr[p] = (short int)i * 8 + (short int)count + (short int)(k -2) * 4096; 
								DistributeBlockNo(inodes[ino].st_size, ino, is_dir);
							}
							else
							{
								// printf("分配数据块用于作为叶子数据块\n");
								ssize_t *res = malloc(sizeof(ssize_t) * 5);
								if((res = GetLastTupleByIno(ino)) == NULL)
									fprintf(stderr, "can't get last tuple! (func: DistributeBlockNo)");
								short int tmp = (short int)i * 8 + (short int)count + (short int)(k -2) * 4096; 
								if(res[0] != 4)
								{
									// printf("我就知道\n");
									res[1] = 0;
								}
								else
								{
									// printf("res[1] = %ld\n", res[1]);
								}
								if(fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + inodes[ino].addr[p]) + res[1] * 2L, SEEK_SET) != 0) // 移动到存着256个short int的数据块
									fprintf(stderr, "block zone fseek failed! (func: DistributeBlockNo)");
								fwrite(&tmp, sizeof(short int), 1, fp);
							}
						}
						else if(p == 5)
						{

						}
						else if(p == 6)
						{
							
						}
					}
				}
				printf("block no distributed is %hd\n", (short int)i * 8 + (short int)count + (short int)(k -2) * 4096);
				
				free(block_bitmap);
				ModifyInodeZone(ino);
				return 0;
			}
		}
	}
	free(block_bitmap);
	// printf("DistributeBlockNo called successfully!\n");
	return -1; // 已满，无法分配数据块
}

ssize_t *GetLastTupleByIno(int ino)
{
	// printf("GetLastTupleByIno start!\n");
	off_t dir_size = inodes[ino].st_size; // 此目录的文件大小
	off_t count = dir_size / sizeof(struct DirTuple); // 一共有多少目录项

	ssize_t *res = malloc(sizeof(ssize_t) * 5);
	for(int i = 0; i < 5; i++) res[i] = -1;
	if(dir_size >= 0 && dir_size <= DIRECT_ADDRESS_SCOPE)
	{
		res[0] = (count - 1L) / 32L;
		res[1] = count - res[0] * 32L;
	}
	else if(dir_size > DIRECT_ADDRESS_SCOPE && dir_size <= DIRECT_ADDRESS_SCOPE + PRIMARY_ADDRESS_SCOPE)
	{
		ssize_t file_size = dir_size - DIRECT_ADDRESS_SCOPE; // 表示1级间址存的文件的大小
		// printf("file_size is %ld\n", file_size);
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
	// printf("GetLastTupleByIno called succesfully!\n");
	return res;
}

int remove_file(const char*parent_path, const char *target, bool is_dir)
{
	// printf("remove_file start!\n");
	struct DirTuple *parent_dir = malloc(sizeof(struct DirTuple)); // 父目录项
	if(GetSingleDirTuple(parent_path, parent_dir) != 0)
	{
		fprintf(stderr, "parent_dir cannot be found! (func: remove_file)");
	}

	// printf("check1\n");
	unsigned short int parent_ino = parent_dir->i_num;
	ssize_t count = inodes[parent_ino].st_size / sizeof(struct DirTuple); // 表示父文件夹中有多少个目录项
	struct DirTuple *tuples = malloc(sizeof(struct DirTuple) * count);
	tuples = GetMultiDirTuples(parent_ino);
	unsigned short int target_ino;

	// printf("check2\n");
	bool is_exist = false;
	ssize_t i = 0;
	for(; i < count; i++)
	{
		char name[16];
		strcpy(name, tuples[i].f_name);
		if (strlen(tuples[i].f_ext) != 0)
		{
			strcat(name, ".");
			strcat(name, tuples[i].f_ext);
		}
		if(strcmp(name, target) == 0)
		{
			is_exist = true;
			target_ino = tuples[i].i_num;
			break;
		}
	}
	// printf("check3\n");
	// printf("is_exist is %d\n", is_exist);
	if(!is_exist)
		return -ENOENT;
	if(is_dir)
	{
		if(!S_ISDIR(inodes[target_ino].st_mode))
			return -ENOTDIR;
	}
	else
	{
		if(S_ISDIR(inodes[target_ino].st_mode))
			return -EISDIR;
	}

	if(is_dir) // 文件夹的话需要递归删除
	{
		// printf("check4\n");
		int new_parent_path_len = strlen(parent_path) + strlen(target) + 2;
		char *new_parent_path = malloc(sizeof(char) * new_parent_path_len);
		strcpy(new_parent_path, parent_path);
		strcat(new_parent_path, target);
		strcat(new_parent_path, "/");
		// printf("new_parent_path is %s\n", new_parent_path);

		ssize_t new_count = inodes[target_ino].st_size / sizeof(struct DirTuple); // 表示target文件夹有多少个目录项
		struct DirTuple *new_tuples = malloc(sizeof(struct DirTuple) * new_count);
		new_tuples = GetMultiDirTuples(target_ino);

		// printf("递归删除target文件夹下的file\n");
		// 递归删除target文件夹下的file
		for(ssize_t new_i = 2; new_i < new_count; new_i++)
		{
			char new_target[16];
			strcpy(new_target, new_tuples[new_i].f_name);
			if (strlen(new_tuples[new_i].f_ext) != 0)
			{
				strcat(new_target, ".");
				strcat(new_target, new_tuples[new_i].f_ext);
			}
			bool new_is_dir = false;
			if(inodes[new_i].st_mode & __S_IFDIR) new_is_dir = true;
			if(remove_file(new_parent_path, new_target, new_is_dir) == RDNOEXISTS)
			{
				fprintf(stderr, "the new_target not found in new_parent_dir! (func: remove_file)");
			}
		}
		// printf("开始删target这个文件夹\n");

		// 开始删target这个文件夹
		DelSign(parent_ino, i, target_ino);
		// printf("check7\n");

	}
	else // 普通文件就不需要递归删除
	{
		DelSign(parent_ino, i, target_ino);
	}

	// printf("remove_file called successfully!\n");
	return 0;
}

int DelSign(const unsigned short int parent_ino, const int index, const unsigned short int target_ino)
{
	// printf("DelSign start!\n");
	// printf("parent_ino is %hu, index is %d\n", parent_ino, index);

	// 根据parent_ino来删除第index的目录项，（注意！需要把后面的目录项移到前面来） ToDo: 只考虑了最简单的只使用addr[0]的情况
	int start;
	if(index < 4 * 32)
	{
		start = index / 32;
	}
	else if(index >= 4 * 32 && index < (4 + 256) * 32)
	{
		start = 4;
	}

	if(inodes[parent_ino].st_size - (index + 1) * sizeof(struct DirTuple) == 0) // 如果是最后一个目录项
	{
		if(start != 4)
		{
			struct DataBlock *tmp_db = malloc(sizeof(struct DataBlock));
			GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[parent_ino].addr[start], tmp_db);
			memset(&tmp_db->data[(index % 32) * sizeof(struct DirTuple)], 0, sizeof(struct DirTuple));
			if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + inodes[parent_ino].addr[start]), SEEK_SET) != 0) // 将指针移动到文件的相应的起始位置 
			{
				fprintf(stderr, "fseek failed! (func: DelSign)\n");
				return -1;
			}
			fwrite(tmp_db, sizeof(struct DataBlock), 1, fp);
		}
		else
		{
			ssize_t *res = malloc(sizeof(ssize_t) * 5);
			res = GetLastTupleByIno(parent_ino);

			struct DataBlock *tmp_db_1 = malloc(sizeof(struct DataBlock));
			GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[parent_ino].addr[start], tmp_db_1);
			short int *leaf_blk_no = (short int *)(&tmp_db_1->data[(res[1] - 1L) * sizeof(short int)]);

			struct DataBlock *leaf_blk = malloc(sizeof(struct DataBlock));
			GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + *leaf_blk_no, leaf_blk);
			memset(&leaf_blk->data[(index % 32) * sizeof(struct DirTuple)], 0, sizeof(struct DirTuple));
			if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + *leaf_blk_no), SEEK_SET) != 0) // 将指针移动到文件的相应的起始位置 
			{
				fprintf(stderr, "fseek failed! (func: DelSign)\n");
				return -1;
			}
			fwrite(leaf_blk, sizeof(struct DataBlock), 1, fp);
		}
	}
	else 
	{
		// printf("现在删除的是中间的目录项\n");
		// printf("index = %d\n", index);
		bool flag = false;
		ssize_t *res = malloc(sizeof(ssize_t) * 5);
		res = GetLastTupleByIno(parent_ino);

		for(int k = start; k <= res[0]; k++)
		{
			if(k != 4)
			{
				// printf("这是k != 4的情况\n");
				struct DataBlock *tmp_db_1 = malloc(sizeof(struct DataBlock));
				if(!flag) // 如果flag为false表示从index开始覆盖，如果flag为true表示从开头覆盖
				{
					GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[parent_ino].addr[k], tmp_db_1);
					
					if(index % 32 != 31) // 如果是当前数据块最后一个目录项，则不需要执行以下操作
						memcpy(&tmp_db_1->data[(index % 32) * sizeof(struct DirTuple)], &tmp_db_1->data[((index % 32) + 1) * sizeof(struct DirTuple)], BLOCK_SIZE - ((index % 32) + 1) * sizeof(struct DirTuple));

					flag = true;
				}
				else
				{
					GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[parent_ino].addr[k], tmp_db_1);
					memcpy(&tmp_db_1->data[0], &tmp_db_1->data[sizeof(struct DirTuple)], BLOCK_SIZE - sizeof(struct DirTuple));
				}

				// 处理数据块的最后一个目录项
				if(k + 1 != 4) 
				{
					struct DataBlock *tmp_db_2 = malloc(sizeof(struct DataBlock));
					GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[parent_ino].addr[k + 1], tmp_db_2);
					memcpy(&tmp_db_1->data[31 * sizeof(struct DirTuple)], &tmp_db_2->data[0], sizeof(struct DirTuple));
				}
				else
				{
					struct DataBlock *tmp_db_2 = malloc(sizeof(struct DataBlock));
					GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[parent_ino].addr[k + 1], tmp_db_2);
					short int *first_leaf_blk_no = (short int *)(&tmp_db_2->data[0]);

					struct DataBlock *first_leaf_blk = malloc(sizeof(struct DataBlock));
					GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + *first_leaf_blk_no, first_leaf_blk);
					memcpy(&tmp_db_1->data[31 * sizeof(struct DirTuple)], &first_leaf_blk->data[0], sizeof(struct DirTuple));
				}

				if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + inodes[parent_ino].addr[k]), SEEK_SET) != 0) // 将指针移动到文件的相应的起始位置 
				{
					fprintf(stderr, "fseek failed! (func: DelSign)\n");
					return -1;
				}
				fwrite(tmp_db_1, sizeof(struct DataBlock), 1, fp);
			}
			else if(k == 4)
			{
				// printf("这是k == 4的情况\n");
				// printf("inodes[parent_ino].addr[4] = %hd\n", inodes[parent_ino].addr[k]);
				struct DataBlock *primary_blk = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[parent_ino].addr[k], primary_blk);
				int primary_no = 0;
				if(index >= 128)
					primary_no = (index - 128) / 32;

				for(; primary_no < res[1]; primary_no++)
				{
					// printf("primary_no = %d\n", primary_no);
					short int *leaf_blk_no = (short int *)(&primary_blk->data[primary_no * sizeof(short int)]);
					// printf("*leaf_blk_no = %hd\n", *leaf_blk_no);

					struct DataBlock *leaf_blk = malloc(sizeof(struct DataBlock));
					if(!flag) // 如果flag为false表示从index开始覆盖，如果flag为true表示从开头覆盖
					{
						// printf("从index开始覆盖\n");
						GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + *leaf_blk_no, leaf_blk);
						
						if(index % 32 != 31) // 如果是当前数据块最后一个目录项，则不需要执行以下操作
							memcpy(&leaf_blk->data[(index % 32) * sizeof(struct DirTuple)], &leaf_blk->data[((index % 32) + 1) * sizeof(struct DirTuple)], BLOCK_SIZE - ((index % 32) + 1) * sizeof(struct DirTuple));

						flag = true;
					}
					else
					{
						GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + *leaf_blk_no, leaf_blk);
						memcpy(&leaf_blk->data[0], &leaf_blk->data[sizeof(struct DirTuple)], BLOCK_SIZE - sizeof(struct DirTuple));
					}

					// 处理数据块的最后一个目录项
					if(primary_no + 1 < 256)
					{
						short int *leaf_blk_no_2 = (short int *)(&primary_blk->data[(primary_no + 1) * sizeof(short int)]);
						
						struct DataBlock *leaf_blk_2 = malloc(sizeof(struct DataBlock));
						GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + *leaf_blk_no_2, leaf_blk_2);
						memcpy(&leaf_blk->data[31 * sizeof(struct DirTuple)], &leaf_blk_2->data[0], sizeof(struct DirTuple));
					}
					else
					{

					}

					if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + *leaf_blk_no), SEEK_SET) != 0) // 将指针移动到文件的相应的起始位置 
					{
						fprintf(stderr, "fseek failed! (func: DelSign)\n");
						return -1;
					}
					fwrite(leaf_blk, sizeof(struct DataBlock), 1, fp);
				}	
			}
			else
			{

			}
		}
	}
	
	// 移除target的inode和block
	SetInoMap(target_ino, false);
	for(int i = 0; i < 7; i++)
	{
		if(i >= 0 && i < 4)
		{
			SetBnoMap(inodes[target_ino].addr[i], false);
		}
	}

	// 父文件夹的大小也要减小16
	inodes[parent_ino].st_size -= 16L;
	ModifyInodeZone(parent_ino);

	// 因为在创建文件夹时我用inodes[target_ino].addr[0]来判断是新建文件夹还是为已有文件夹新增数据块，所以在这里要消除这个影响
	inodes[target_ino].addr[0] = -1;
	ModifyInodeZone(target_ino);

	//printf("DelSign called successfully!\n");
	return 0;
}

void SetInoMap(const unsigned short int ino, bool status)
{
	// printf("SetInoMap start!\n");
	int index = ino / 32; int offset = ino % 32;
	// printf("index is %d, offset is %d\n", index, offset);
	if (fseek(fp, BLOCK_SIZE * 1 + index * 4L, SEEK_SET) != 0) 
        fprintf(stderr, "bitmap fseek failed! (func: SetInoMap)");
	unsigned int *tmp = malloc(sizeof(unsigned int));
	fread(tmp, sizeof(unsigned int), 1, fp);
	if(status)
		*tmp |= (unsigned int)(1 << (31 - offset));
	else
		*tmp ^= (unsigned int)(1 << (31 - offset));
	if (fseek(fp, BLOCK_SIZE * 1 + index * 4L, SEEK_SET) != 0) 
        fprintf(stderr, "bitmap fseek failed! (func: SetInoMap)");
	fwrite(tmp, sizeof(unsigned int), 1, fp);

	// printf("SetInoMap called successfully!\n");
}

void SetBnoMap(const unsigned short int bno, bool status)
{
	int index = bno / 32; int offset = bno % 32;
	if (fseek(fp, BLOCK_SIZE * 2 + index * 4L, SEEK_SET) != 0) 
        fprintf(stderr, "bitmap fseek failed! (func: SetBnoMap)");
	unsigned int *tmp = malloc(sizeof(unsigned int));
	fread(tmp, sizeof(unsigned int), 1, fp);
	if(status)
		*tmp |= (unsigned int)(1 << (31 - offset));
	else
		*tmp ^= (unsigned int)(1 << (31 - offset));
	if (fseek(fp, BLOCK_SIZE * 2 + index * 4L, SEEK_SET) != 0) 
        fprintf(stderr, "bitmap fseek failed! (func: SetBnoMap)");
	fwrite(tmp, sizeof(unsigned int), 1, fp);
}

void ModifyInodeZone(int ino)
{
	// 把修改的inodes写进diskimg
	if (fseek(fp, BLOCK_SIZE * 6 + ino * sizeof(struct Inode), SEEK_SET) != 0) // 将指针移动到inode区的正确位置
		fprintf(stderr, "inode block fseek failed! (func: DistributeBlockNo)\n");
	fwrite(&inodes[ino], sizeof(struct Inode), 1, fp); // 已修改inode区
}

int GetTargetInoByPath(const char *path)
{
	int target_ino = -1;

	int path_len = strlen(path);
	int s = path_len - 1;
	for(; s >= 0; s--)
	{
		if (path[s] == '/')
			break;
	}
	char parent_path[path_len]; // 父目录的路径
	strncpy(parent_path, path, s + 1); 
	parent_path[s + 1] = '\0';
	//printf("parent_path is %s\n", parent_path);

	struct DirTuple *parent_dir = malloc(sizeof(struct DirTuple)); // 父目录项
	if(GetSingleDirTuple(parent_path, parent_dir) != 0)
	{
		fprintf(stderr, "parent_dir cannot be found! (func: GetTargetInoByPath)");
	}

	unsigned short int parent_ino = parent_dir->i_num;
	ssize_t count = inodes[parent_ino].st_size / sizeof(struct DirTuple); // 表示父文件夹中有多少个目录项
	struct DirTuple *tuples = malloc(sizeof(struct DirTuple) * count);
	tuples = GetMultiDirTuples(parent_ino);

	char target[path_len - s];
	strcpy(target, path + s + 1);
	for(ssize_t i = 0; i < count; i++)
	{
		char name[16];
		strcpy(name, tuples[i].f_name);
		if (strlen(tuples[i].f_ext) != 0)
		{
			strcat(name, ".");
			strcat(name, tuples[i].f_ext);
		}
		if(strcmp(name, target) == 0)
		{
			target_ino = tuples[i].i_num;
			break;
		}
	}

	return target_ino;
}

int write_file(const char *buf, size_t size, off_t offset, int target_ino)
{
	if(size <= 512L)
	{
		int k;
		if(inodes[target_ino].st_size <= DIRECT_ADDRESS_SCOPE)
			k = (inodes[target_ino].st_size - 1) / 512; 
		else if(inodes[target_ino].st_size <= PRIMARY_ADDRESS_SCOPE)
			k = 4;
		
		if(k >= 0 && k <= 3 && inodes[target_ino].addr[k] != -1)
		{
			ssize_t cur_size = inodes[target_ino].st_size - k * 512L;
			if(512L - cur_size >= size) // 剩下的空间可以写入size大小的数据
			{
				struct DataBlock *tmp_db = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k], tmp_db);
				memcpy(&tmp_db->data[offset % 512], buf, size);
				if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k]), SEEK_SET) != 0)
					fprintf(stderr, "new block fseek failed! (func: bugeater_write)\n");
				fwrite(tmp_db, sizeof(struct DataBlock), 1, fp);
			}
			else
			{
				struct DataBlock *tmp_db = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k], tmp_db);
				memcpy(&tmp_db->data[offset % 512], buf, 512L - cur_size); // 把能写进去的先写进去
				if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k]), SEEK_SET) != 0)
					fprintf(stderr, "new block fseek failed! (func: bugeater_write)\n");
				fwrite(tmp_db, sizeof(struct DataBlock), 1, fp);

				DistributeBlockNo(inodes[target_ino].st_size, target_ino, false); // 为target文件新建一个数据块
				if(k < 3)
				{
					struct DataBlock *tmp_db_2 = malloc(sizeof(struct DataBlock));
					// printf("inodes[target_ino].addr[k + 1] is %hd\n", inodes[target_ino].addr[k + 1]);
					GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k + 1], tmp_db_2);
					memcpy(&tmp_db_2->data[0], &buf[512L - cur_size], size - (512L - cur_size)); // 写入还没写进去的数据
					if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k + 1]), SEEK_SET) != 0)
						fprintf(stderr, "new block fseek failed! (func: bugeater_write)\n");
					fwrite(tmp_db_2, sizeof(struct DataBlock), 1, fp);
				}
				else
				{
					struct DataBlock *primary_blk = malloc(sizeof(struct DataBlock));
					GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k + 1], primary_blk);
					short int *leaf_blk_no = (short int *)(&primary_blk->data[0]);
					GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + *leaf_blk_no, tmp_db);
					memcpy(&tmp_db->data[0], &buf[512L - cur_size], size - (512L - cur_size)); // 写入还没写进去的数据
					if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + *leaf_blk_no), SEEK_SET) != 0)
						fprintf(stderr, "new block fseek failed! (func: bugeater_write)\n");
					fwrite(tmp_db, sizeof(struct DataBlock), 1, fp);
				}
			}
		}
		else if(k == 4 && inodes[target_ino].addr[k] != -1)
		{
			ssize_t *res = malloc(sizeof(ssize_t) * 5);
			res = GetLastTupleByIno(target_ino);
			ssize_t cur_size = inodes[target_ino].st_size - (k + res[1] - 1) * 512L;

			if(512L - cur_size >= size) // 剩下的空间可以写入size大小的数据
			{
				// printf("写的下\n");
				struct DataBlock *primary_blk = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k], primary_blk);
				short int *leaf_blk_no = (short int *)(&primary_blk->data[(res[1] - 1) * 2L]);
				// printf("*leaf_blk_no = %hd\n", *leaf_blk_no);
				
				struct DataBlock *tmp_db = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + *leaf_blk_no, tmp_db);
				memcpy(&tmp_db->data[offset % 512], buf, size);
				if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + *leaf_blk_no), SEEK_SET) != 0)
					fprintf(stderr, "new block fseek failed! (func: bugeater_write)\n");
				fwrite(tmp_db, sizeof(struct DataBlock), 1, fp);
			}
			else
			{
				// printf("写不下\n");
				struct DataBlock *primary_blk = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k], primary_blk);
				short int *leaf_blk_no = (short int *)(&primary_blk->data[(res[1] - 1) * 2L]);
				// printf("*leaf_blk_no = %hd\n", *leaf_blk_no);
				
				struct DataBlock *tmp_db = malloc(sizeof(struct DataBlock));
				GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + *leaf_blk_no, tmp_db);
				memcpy(&tmp_db->data[offset % 512], buf, 512L - cur_size);
				if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + *leaf_blk_no), SEEK_SET) != 0)
					fprintf(stderr, "new block fseek failed! (func: bugeater_write)\n");
				fwrite(tmp_db, sizeof(struct DataBlock), 1, fp);

				DistributeBlockNo(inodes[target_ino].st_size, target_ino, false); // 为target文件新建一个数据块
				GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + inodes[target_ino].addr[k], primary_blk);
				leaf_blk_no = (short int *)(&primary_blk->data[res[1] * 2L]);
				// printf("*leaf_blk_no = %hd\n", *leaf_blk_no);

				GetSingleDataBlock(ROOT_DIR_TUPLE_BNO + *leaf_blk_no, tmp_db);
				memcpy(&tmp_db->data[0], &buf[512L - cur_size], size - (512L - cur_size)); // 写入还没写进去的数据
				if (fseek(fp, BLOCK_SIZE * (ROOT_DIR_TUPLE_BNO + *leaf_blk_no), SEEK_SET) != 0)
					fprintf(stderr, "new block fseek failed! (func: bugeater_write)\n");
				fwrite(tmp_db, sizeof(struct DataBlock), 1, fp);
			}
		}
	}
	else // size大于512
	{
		ssize_t tmp_size = 0L;
		while(tmp_size != size)
		{
			// writen_size表示要写入的部分的大小
			ssize_t writen_size = size - tmp_size <= 512L ? size - tmp_size : 512L; 
			// writen表示要写入的数据
			char *writen = malloc(sizeof(char) * writen_size);
			strncpy(writen, &buf[tmp_size], writen_size);		
			writen[writen_size] = '\0';
			write_file(writen, writen_size, 0L, target_ino);
			tmp_size += writen_size;
			inodes[target_ino].st_size += writen_size;
		}
	}
	return 0;
}

int read_file(char *buf, size_t size, int target_ino)
{
	struct DataBlock *tmp_db = malloc(sizeof(struct DataBlock));
	off_t base = 0; off_t file_size = inodes[target_ino].st_size;
	for(int i = 0; i < 7; i++)
	{
		if(inodes[target_ino].addr[i] == -1)
			break;
		if(base == size)
			break;
		if(i >= 0 && i <= 3 && GetSingleDataBlock(inodes[target_ino].addr[i] + ROOT_DIR_TUPLE_BNO, tmp_db) == 0)
		{
			ssize_t cur_size = (file_size - base < 512) ? file_size - base : 512; // 用于表示当前的数据块的文件大小
			memcpy(buf + base, &tmp_db->data[0], cur_size);
			base += cur_size; 
		}
		else if(i == 4) 
		{ 
			ssize_t primary_size = (file_size - DIRECT_ADDRESS_SCOPE < PRIMARY_ADDRESS_SCOPE) ? file_size - DIRECT_ADDRESS_SCOPE 
			: PRIMARY_ADDRESS_SCOPE; 
			ssize_t blocks_cnt_1 = DivideCeil(primary_size, 512L);

			struct DataBlock *primary_dblock = malloc(sizeof(struct DataBlock)); 
			GetSingleDataBlock(inodes[target_ino].addr[i] + ROOT_DIR_TUPLE_BNO, primary_dblock);
			for(ssize_t j = 0; j < blocks_cnt_1; j++)
			{
				short int *block_no_1 = (short int *)(&primary_dblock->data[j * sizeof(short int)]);
				if(GetSingleDataBlock(*(block_no_1) + ROOT_DIR_TUPLE_BNO, tmp_db) == 0)
				{
					ssize_t cur_size = (file_size - base < 512) ? file_size - base : 512; // 用于表示当前的数据块的文件大小
					memcpy(buf + base, &tmp_db->data[0], cur_size);
					base += cur_size;
				}
			}
		}
	}
	return 0;
}