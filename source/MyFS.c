/** 
 * Compile with:
 *
 *     gcc -Wall util.c MyFS.c `pkg-config fuse3 --cflags --libs` -o MyFS
 * 
 */

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "util.h"

/**
 * init函数用于初始化文件系统，在文件系统被挂载时被调用。
 * 参数说明：
 * 1. conn：包含与文件系统链接相关的信息，如挂载选项/协议版本
 * 2. cfg：用于配置文件系统的参数
*/
static void *bugeater_init(struct fuse_conn_info *conn, struct fuse_config *cfg) 
{
	FilePointerInit();

    printf("\nbugeater_init() called!\n");
	(void) conn;
	//cfg->kernel_cache = 1; // 启用内核缓存，提高文件系统性能

	/* 处理与超级块有关的信息 */
    k_super_block = malloc(sizeof(struct SuperBlock));
    fread(k_super_block,sizeof(struct SuperBlock),1,fp);

	/* inode区的inode全部读入进inodes */
	if (fseek(fp, BLOCK_SIZE * 6, SEEK_SET) != 0) // 将指针移动到inode区的起始位置
        fprintf(stderr, "inodes fseek failed! (func: bugeater_init)\n");
	inodes = malloc(sizeof(struct Inode) * 4096);
	fread(inodes,sizeof(struct Inode),4096,fp);

    printf("bugeater_init() called successfully!\n");
	return NULL;
}

/**
 * getattr函数用于获取文件或目录的属性。
 * 参数说明：
 * 1. path：要获取属性的文件或目录的路径
 * 2. stbuf：类型为struct stat，用于存储属性
 * 3. fi：包含有关文件的更多信息，如文件的打开模式，文件描述符
 * 返回值：
 * 1. 如果成功获取属性信息，返回0
 * 2. 如果出现错误，可以返回相应的错误码
 * ---ENONET 文件或目录不存在
 * ---EACCES 无权限
 * ---ENOMEM 内存不足
*/
// struct stat介绍：
// struct stat {
//     dev_t     st_dev;         /* ID of device containing file */
//     ino_t     st_ino;         /* Inode number */
//     mode_t    st_mode;        /* File type and mode */
//     nlink_t   st_nlink;       /* Number of hard links */
//     uid_t     st_uid;         /* User ID of owner */
//     gid_t     st_gid;         /* Group ID of owner */
//     dev_t     st_rdev;        /* Device ID (if special file) */
//     off_t     st_size;        /* Total size, in bytes */
//     blksize_t st_blksize;     /* Block size for filesystem I/O */
//     blkcnt_t  st_blocks;      /* Number of 512B blocks allocated */
//     // ......
// };
static int bugeater_getattr(const char *path, struct stat *stbuf)
{
    printf("\nbugeater_getattr() called!\n");
	printf("path is %s\n", path);
	int res = 0;
	struct DirTuple *dir_tuple = malloc(sizeof(struct DirTuple));

	if(GetSingleDirTuple(path, dir_tuple) == 0)
	{
		// printf("dir_tuple->i_num is %d\n",dir_tuple->i_num);
		// printf("mode within path is %d\n",inodes[dir_tuple->i_num].st_mode);
		if(inodes[dir_tuple->i_num].st_mode & __S_IFDIR) // 是个目录
		{
			/* 目录必须给x权限，不然打不开 */
			stbuf->st_mode = __S_IFDIR | 0755; // 755表示owner拥有rwx权限，而group和其他只有rx权限
			stbuf->st_size = inodes[dir_tuple->i_num].st_size;
			stbuf->st_nlink = inodes[dir_tuple->i_num].st_nlink;
			printf("it's a DIR\n");
		}
		else if(inodes[dir_tuple->i_num].st_mode & __S_IFREG) // 是个文件
		{
			stbuf->st_mode = __S_IFREG | 0744; 
			stbuf->st_size = inodes[dir_tuple->i_num].st_size;
			stbuf->st_nlink = inodes[dir_tuple->i_num].st_nlink;
			printf("it's a REG\n");
		}
		else
		{
			fprintf(stderr, "file type not recognized! (func: bugeater_getattr)\n");
		}
	}
	else
	{
		res = -ENOENT;
		printf("No such REG or DIR\n");
	}
		
	free(dir_tuple);

    printf("bugeater_getattr() called successfully!\n");
	return res;
}

/**
 * readdir函数用于将我们自定义的目录信息通过buf变量传递给fuse库
 * 参数说明：
 * 1. path：要读取的目录路径
 * 2. buf：用于填充目录项的缓冲区
 * 3. filler： 用于填充目录项的回调函数
 * 4. offset： 读取目录项的偏移量
 * 5. fi：文件信息结构体
 * tips：
 * 根据官方注释，offset需要看fuse怎么实现的，有的版本会直接忽略offset，有的则不会
*/
static int *bugeater_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
	printf("\nbugeater_readdir() called!\n");
	printf("reading directory %s .....\n", path);

    /* 先获取此path对应的目录项 */
	struct DirTuple *path_dirtuple = malloc(sizeof(struct DirTuple));
	if(GetSingleDirTuple(path, path_dirtuple) != 0)
	{
		return -ENOENT;
	}
	//printf("## %s ##\n",path_dirtuple->f_name); // 应该输出 `.`

	if(!S_ISDIR(inodes[path_dirtuple->i_num].st_mode)) // 不是目录，返回相应的错误值
	{
		return -ENOTDIR;
	}

	ssize_t count = inodes[path_dirtuple->i_num].st_size / sizeof(struct DirTuple); // 表示有多少个目录项
	struct DirTuple *tuples = malloc(sizeof(struct DirTuple) * count);
	//printf("count is %d\n", count);
	tuples = GetMultiDirTuples(path_dirtuple->i_num);

	printf("directory %s has %ld tuples\n", path, count);

	// 在我自定义的目录结构里也是有"."和".."的	
	for(int i = 0; i < count; i++)
	{
		printf("## %s ##\n", tuples[i].f_name);
		char name[16];
		strcpy(name, tuples[i].f_name);
		if (strlen(tuples[i].f_ext) != 0)
		{
			strcat(name, ".");
			strcat(name, tuples[i].f_ext);
		}
		filler(buf, name, NULL, 0, 0);
	}
	free(path_dirtuple); free(tuples);

	printf("bugeater_readdir() called successfully!\n");
	return 0;
}

//创建目录
static int bugeater_mkdir(const char *path, mode_t mode)
{
	printf("\nbugeater_mkdir called!\n");
	//printf("path is %s\n", path);
	
	if (create_file(path, true) != 0)
		return -1;
	printf("bugeater_mkdir called successfully!\n");
	return 0;
}

static int *bugeater_rmdir(const char *path){
    printf("\nbugeater_rmdir called!\n");
	printf("path is %s\n", path);
	
	if(strcmp(path, "/") == 0)
	{
		fprintf(stderr, "not allowed to delete root directory! (func: bugeater_rmdir)");
		return -1;
	}

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
	char target[path_len - s];
	strcpy(target, path + s + 1);

	if (remove_file(parent_path, target, true) != 0)
		return -1;
	printf("bugeater_rmdir called successfully!\n");
	return 0;
}

void *bugeater_mknod(){
    
}

void *bugeater_unlink(){
    
}

void *bugeater_read(){
    
}

void *bugeater_write(){
    
}

static const struct fuse_operations MyFS_ops = {
	.init       = bugeater_init, // 初始化
	.getattr	= bugeater_getattr, // 获取文件属性（包括目录的）
	.readdir	= bugeater_readdir, // 读取目录
	.mkdir  	= bugeater_mkdir, // 创建文件夹
	.rmdir  	= bugeater_rmdir, // 删除文件夹
	.mknod      = bugeater_mknod, // 创建文件
    .unlink     = bugeater_unlink,//删除文件
	.read		= bugeater_read, //读取文件内容
    .write      = bugeater_write, //修改文件内容
};

int main(int argc, char *argv[])
{
    umask(0); // 保证目录权限
    return fuse_main(argc, argv, &MyFS_ops, NULL);
}
