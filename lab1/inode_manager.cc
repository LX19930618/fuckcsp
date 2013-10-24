#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------
uint32_t
block_manager::check_block(blockid_t id)
{
    if(id < sb.niblocks || id > sb.nblocks)
    {
        printf("bad block");
        return 1;
    }
    return 0;
}

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */

    blockid_t bno;
    register struct superblock *sp;
    register char *bp;
    
    sp = &sb;
   //get a free block from the queue
    
   do {
        if(sp->nbfree <= 0)
                goto nospace;
        if (sp->nbfree > NICFREE) {
                printf("Bad free count");
                goto nospace;
        }
        bno = sp->bfrees[--sp->nbfree];
        if(bno == 0)
                goto nospace;
    } while (check_block(bno));
    if(sp->nbfree <= 0) {
        bp = (char *)malloc(BLOCK_SIZE);
        read_block(bno,bp);
        sp->nbfree = ((FBLKP)bp)->nbfree;
        memcpy(sp->bfrees,((FBLKP)bp)->bfrees,sizeof(sp->bfrees));
        if (sp->nbfree <=0)
            goto nospace;
    }
    return bno;

nospace:
    sb.nbfree = 0;
    printf("no free block to alloc");
    return 0;
}

void
block_manager::free_block(blockid_t bno)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
    register struct superblock *sp;
    register char *bp;

    sp = &sb;
    if (check_block(bno))
            return;
    if(sp->nbfree <= 0) {
            sp->nbfree = 1;
            sp->bfrees[0] = 0;
    }
    if(sp->nbfree >= NICFREE) {
        bp = (char *)malloc(BLOCK_SIZE);
        bzero(bp,BLOCK_SIZE);
        ((FBLKP)bp)->nbfree = sp->nbfree;
        memcpy(((FBLKP)bp)->bfrees,sp->bfrees,sizeof(sp->bfrees));
        sp->nbfree = 0;
        write_block(bno,bp);
    }
    sp->bfrees[sp->nbfree++] = bno;
    return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|

// the id of super block is 1 not 0
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nmblocks = BLOCK_NUM / BPB + 3;
  sb.niblocks = INODE_NUM / IPB + BLOCK_NUM / BPB + 3;  //the first data block id must be bigger than it
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
  
  //initialize the free list, you should read it from the super block in the disk in real.
  //initialize the free block list
  sb.nbfree = 1;
  sb.bfrees[0] = 0;
  for(blockid_t bno=sb.niblocks;bno<sb.nblocks;bno++)
  {
      free_block(bno);
  }
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
    register struct superblock *fs;
    bm = new block_manager();
    fs = &bm->sb;
    //set first free inode as 1
    bm->sb.nifree = NICINOD;
    for(int i=0;i<NICINOD;i++)
        fs->ifrees[i] = NICINOD - i;
  
    uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
      printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
      exit(0);
    }
}

/* Create a new file.
 * Return its inum. */
inodeid_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  //scan the inodes for a unalloced one
    register struct superblock *fp;
    register char *bp;
    register struct inode *ip;
    inodeid_t ino;

    fp = &bm->sb;
loop:
    if(fp->nifree > 0) 
    {
        ino = fp->ifrees[--fp->nifree];
        if (ino < ROOTINO)
            goto loop;
        bp = (char*)malloc(BLOCK_SIZE);
        bm->read_block(IBLOCK(ino, fp->nblocks), bp);
        ip = (struct inode*)bp + ino%IPB;
        ip->type = type;
        bm->write_block(IBLOCK(ino, fp->nblocks), bp);
        return ino;
    }
    /*
     * Inode was allocated after all.
     * Look some more.
     */
    ino = 3;
    bp = (char*)malloc(BLOCK_SIZE);
    for (uint32_t i = fp->nmblocks+1; i < fp->niblocks; i++)
    {
       bm->read_block(i, bp);
       for(uint32_t j = 0;j<IPB;j++)
       {
           ip = (struct inode*)bp + j;
           if (ip->type == 0) 
               fp->ifrees[fp->nifree++] = ino;
           if(fp->nifree >= NICINOD)
               goto loop;
           ino++;
       }
    }

    if(fp->nifree > 0)
        goto loop;
    printf("Out of inodes");
    return 0;
}

void
inode_manager::free_inode(inodeid_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
    register struct superblock *fp;
    register char *bp;
    register inode *ip;
    fp = &bm->sb;
    bp = (char*)malloc(BLOCK_SIZE);
    bm->read_block(IBLOCK(inum, fp->nblocks), bp);
    ip = (struct inode*)bp + inum%IPB;
    bzero(ip,sizeof(struct inode));
    bm->write_block(IBLOCK(inum, fp->nblocks), bp);
    
    if(fp->nifree >= NICINOD)
        return;
    fp->ifrees[fp->nifree++] = inum;
    return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */


  //calculate the direct num and the indirect num
    char *bp;
    blockid_t bn;
    unsigned n, on;
    register inode *ip;
    ip = get_inode(inum);
    *size = ip->size;
    n = ip->size >> BSHIFT;
    on = ip->size & BMASK;

    
    bp = (char*)malloc(BLOCK_SIZE);
    *buf_out = (char*)malloc(ip->size);
    int rsize = 0;
    unsigned i;
    if(n < NDIRECT)
    {
        for(i=0; i<=n;i++)
        {
            bn = ip->blocks[i];
            bm->read_block(bn,bp);
            memcpy(*buf_out + i * BLOCK_SIZE,bp ,MIN(BLOCK_SIZE,ip->size - rsize));
            rsize += BLOCK_SIZE;
        }
    }else{
        bn = ip->blocks[NDIRECT];
        blockid_t blocks[NINDIRECT];
        bzero(blocks,BLOCK_SIZE);
        bm->read_block(bn,bp);
        memcpy(blocks,bp ,BLOCK_SIZE);
        for(i=0; i<=n;i++)
        {
            bn = blocks[i];
            bm->read_block(bn,bp);
            memcpy(*buf_out + i * BLOCK_SIZE,bp ,MIN(BLOCK_SIZE,ip->size - rsize));
            rsize += BLOCK_SIZE;
        }
    }
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
    
    char *bp;
    blockid_t bn;
    register unsigned n, on;
    register inode *ip;

    if(size <= 0) {
        printf("wrong file size");
        return;
    }
    n = size >> BSHIFT;
    on = size & BMASK;

    ip = get_inode(inum);
    bp = (char*)malloc(BLOCK_SIZE);
    ip->size = size;
    int wsize = 0;
    unsigned i;
    if(n < NDIRECT)
    {
        for(i=0; i<=n;i++)
        {
            bn = bm->alloc_block();
            bzero(bp,BLOCK_SIZE);
            memcpy(bp,buf + i * BLOCK_SIZE,MIN(BLOCK_SIZE,size - wsize));
            bm->write_block(bn,bp);
            ip->blocks[i] = bn;
            wsize += BLOCK_SIZE;
        }
        for(;i<NDIRECT;i++)
        {
            if(ip->blocks[i]){
                bm->free_block(ip->blocks[i]);
                ip->blocks[i] = 0;
            }
        }
    }else{
        if(ip->blocks[NDIRECT] == 0)
            ip->blocks[NDIRECT] = bm->alloc_block();
        blockid_t indblocks[NINDIRECT];
        bzero(indblocks,BLOCK_SIZE);
        for(i=0; i<=n;i++)
        {
            bn = bm->alloc_block();
            bzero(bp,BLOCK_SIZE);
            memcpy(bp,buf + i * BLOCK_SIZE,MIN(BLOCK_SIZE,size - wsize));
            bm->write_block(bn,bp);
            indblocks[i] = bn;
            wsize += BLOCK_SIZE;
        }
        for(;i<NINDIRECT;i++)
        {
            if(indblocks[i]){
                bm->free_block(indblocks[i]);
                indblocks[i] = 0;
            }
        }
        bzero(bp,BLOCK_SIZE);
        memcpy(bp,indblocks,BLOCK_SIZE);
        bm->write_block(ip->blocks[NDIRECT],bp);
    }
    put_inode(inum,ip);
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode *ino_disk;
  char buf[BLOCK_SIZE];

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;

  memcpy(&a, ino_disk, sizeof(a));
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   */
    register inode *ip;
    register char *bp;
    uint i;
    ip = get_inode(inum);
    
    if(ip->type == 0)
    {
        printf("file not exits");
        return;
    }
    
    if(ip-> type == extent_protocol::T_DIR)
    {
        ip->type = 0;
        put_inode(inum,ip);
        return;
    }
        
    for(i=0;i<NDIRECT;i++)
    {
        if(ip->blocks[i]){
            bm->free_block(ip->blocks[i]);
            ip->blocks[i] = 0;
        }
    }
    if(ip->blocks[NDIRECT])
    {
        blockid_t bno = ip->blocks[NDIRECT];
        bp = (char*)malloc(BLOCK_SIZE);
        bm->read_block(bno,bp);
        blockid_t blocks[NINDIRECT];
        memcpy(blocks,bp,BLOCK_SIZE);
        for(i=0;i<NINDIRECT;i++)
        {
            if(blocks[i]){
                bm->free_block(blocks[i]);
                blocks[i] = 0;
            }
        }
    }
    ip->type = 0;
    ip->size = 0;
    put_inode(inum,ip);
  return;
}
