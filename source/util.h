#define BLOCK_SIZE 512 // 一个block大小512字节
#define ROOT_DIR_TUPLE_BNO 518 // 根目录的数据块号
#define DIRECT_ADDRESS_SCOPE 4 * 512L // 直接地址能覆盖的文件大小
#define PRIMARY_ADDRESS_SCOPE 256L * 512L // 一次间址能覆盖的文件大小
#define SECONDARY_ADDRESS_SCOPE 256L * 256L * 512L // 二次间址能覆盖的文件大小
#define TRIPLE_ADDRESS_SCOPE 256L * 256L * 256L * 512L // 三次间址能覆盖的文件大小
#define DEFAULT_DIR_SIZE 32L // 当一个目录创建时，一定有两个目录项表示"."和".."

#define RDEXISTS -1 // regular或dir均已存在
#define RDNOEXISTS -2 // regular或dir不存在

#include <sys/types.h>
#include <stdbool.h>
#include <time.h>

// 超级块结构
struct SuperBlock {
    long fs_size;  // 文件系统的大小，以块为单位 8MB/512B=16384块

    long first_blk;  // 数据区的第一块块号为 518（从0开始），根目录也放在此 
    long data_size;  // 数据区大小，最大以块为单位 16384-1-1-4-512=15818块
    long first_inode;    // inode区起始块号为 6
    long inode_area_size;   // inode区大小，以块为单位 512块
    long first_blk_of_inodebitmap;   // inode位图区起始块号为 1
    long inode_bitmap_size;  // inode位图区大小，以块为单位 1块
    long first_blk_of_databitmap;   // 数据块位图起始块号为 2
    long databitmap_size;      // 数据块位图大小，以块为单位 4块
};

// 目录项结构 根目录具有固定inode号：0 由于c中有字节对齐的特性，所以需要通过sizeof才能正确得到类型大小
struct DirTuple{ // 16字节
  char f_name[8]; // 文件名 最多为8字节
  char f_ext[3]; // 文件拓展名，最多为3字节
  unsigned short i_num; // inode号最多为2字节
  char spare[1]; // 备用：1字节
};

// inode结构 由于c中有字节对齐的特性，所以需要通过sizeof才能正确得到类型大小
struct Inode { // 64字节
    short int st_mode; // 前四位用作文件类型，后九位表示用户，组，其他的权限，中间三位表示特殊属性(suid,sgid,sticky)
    short int st_ino; // i-node号，2字节 
    int st_nlink; // 连接数，4字节 
    uid_t st_uid; // 拥有者的用户 ID ，4字节  
    gid_t st_gid; // 拥有者的组 ID，4字节   
    off_t st_size; // 文件大小，4字节 
    struct timespec st_atim; // 16个字节time of last access  
    /* 用-1表示没有分配磁盘 */
    short int addr [7];    // 磁盘地址，14字节      0-3直接地址，4一次间址，5二次间址，6三次间址
    char spare[6]; // 备用：6字节
};

// 数据块结构，大小为 512 bytes，占用1块磁盘块
struct DataBlock {
    char data[BLOCK_SIZE];
};

extern FILE *fp;
extern const char *img_path;
extern struct SuperBlock *k_super_block; // k前缀表示常值不变
extern struct Inode *inodes; // 将所有的inode缓存在内存中（占用内存256KB），加快读取速度。

void FilePointerInit();
int GetSingleDirTuple(const char * path, struct DirTuple *dir_tuple); // 根据path来获取所对应的单个目录项
struct DirTuple* GetMultiDirTuples(const int ino); // 根据ino来获取目录的全部目录项（主要是为了处理有间址的情况）
int GetSingleDataBlock(const int bno, struct DataBlock *d_block); // 根据bno块号（从0开始）来读取一块到d_block
int create_file(const char *path, bool is_dir); // 创建文件或文件夹
short int DistributeIno(ssize_t file_size, bool is_dir); // 根据file_size来分配inode
// 将名为target，inode号为target_ino的目录项插在parent_ino所指出的父目录下
int AddToParentDir(unsigned short int parent_ino, char *target, unsigned short int target_ino);
int DistributeBlockNo(ssize_t file_size, int ino, bool is_dir); // 根据files_size和inode号ino来分配数据块
/**
 * 根据ino获得最后一个目录项的位置，以数组形式表示，下标0的值index1表示是arr[index1]表示的数据块中，依此类推
*/
ssize_t *GetLastTupleByIno(int ino); 
// 此函数为递归函数，用于删除文件或文件夹 
int remove_file(const char*parent_path, const char *target, bool is_dir);
int DelSign(const unsigned short int parent_ino, const int index, const unsigned short int target_ino);
void SetInoMap(const unsigned short int ino, bool status);
void SetBnoMap(const unsigned short int bno, bool status);
