// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include "extent_protocol.h" // TODO: delete it

//Wu Jiang's Definations 
#define NICFREE 50
#define NICINOD 100
#define ROOTINO 1 //the inode id for root dir
#define BSHIFT 9 //the shift bits for a block
#define	BMASK 0777 // BLOCK_SIZE - 1
//end of Wu Jiang's Definations


#define DISK_SIZE  1024*1024*16
#define BLOCK_SIZE 512
#define BLOCK_NUM  (DISK_SIZE/BLOCK_SIZE)

typedef uint32_t blockid_t;
typedef uint16_t inodeid_t;  //maxmum of the inodes is 1024, so user short int 

// disk layer -----------------------------------------

class disk {
 private:
  unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

 public:
  disk();
  void read_block(blockid_t id, char *buf);
  void write_block(blockid_t id, const char *buf);
};

// block layer -----------------------------------------
struct fblk
{
    int    	nbfree;
    blockid_t	bfrees[NICFREE];
};

typedef	struct fblk *FBLKP;

typedef struct superblock {
  uint32_t size;
  uint32_t nmblocks;    //size of the bitmap blocks
  //size of the inode blocks and the bitmap blocks 
  //which is the id of the first data block (added by Wu Jiang)
  uint32_t niblocks;    
  uint32_t nblocks;     //size of the blocks on tthe volume
  uint32_t ninodes;     //size of the inodes
  
  //use a FILO queue to store the free list
  uint32_t nbfree;       //number of addresses in bfrees
  blockid_t bfrees[NICFREE];     //free block list
  uint32_t nifree;       //number of addresses in ifrees
  inodeid_t ifrees[NICINOD];    //free inode list 
} superblock_t;

class block_manager {
 private:
  disk *d;
  std::map <uint32_t, int> using_blocks;
  uint32_t check_block(blockid_t id);
 public:
  block_manager();
  struct superblock sb;

  uint32_t alloc_block();
  void free_block(blockid_t id);
  void read_block(blockid_t id, char *buf);
  void write_block(blockid_t id, const char *buf);
};

// inode layer -----------------------------------------

#define INODE_NUM  1024

// Inodes per block. (3)
#define IPB           (BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
#define IBLOCK(i, nblocks)     ((nblocks)/BPB + (i)/IPB + 3)

// Bitmap bits per block
#define BPB           (BLOCK_SIZE*8)

// Block containing bit for block b
#define BBLOCK(b) ((b)/BPB + 2)

#define NDIRECT 32
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

typedef struct inode {
  short type;
  unsigned int size;
  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;
  blockid_t blocks[NDIRECT+1];   // Data block addresses
} inode_t;

class inode_manager {
 private:
  block_manager *bm;
  struct inode* get_inode(uint32_t inum);
  void put_inode(uint32_t inum, struct inode *ino);

 public:
  inode_manager();
  inodeid_t alloc_inode(uint32_t type);
  void free_inode(inodeid_t inum);
  void read_file(uint32_t inum, char **buf, int *size);
  void write_file(uint32_t inum, const char *buf, int size);
  void remove_file(uint32_t inum);
  void getattr(uint32_t inum, extent_protocol::attr &a);
};

#endif

