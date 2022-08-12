// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCSIZE 17
#define BUCHASH(blockno) (blockno % BUCSIZE)
extern uint ticks;

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf heads[BUCSIZE];
  struct spinlock buclocks[BUCSIZE];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i = 0; i < BUCSIZE; i++) {
    initlock(&bcache.buclocks[i], "bcache.bucket");
    // Create linked list of buffers
    bcache.heads[i].prev = &bcache.heads[i];
    bcache.heads[i].next = &bcache.heads[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.heads[BUCHASH(b->blockno)].next;
    b->prev = &bcache.heads[BUCHASH(b->blockno)];
    initsleeplock(&b->lock, "buffer");
    bcache.heads[BUCHASH(b->blockno)].next->prev = b;
    bcache.heads[BUCHASH(b->blockno)].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucno = BUCHASH(blockno);
  struct buf *lrub = 0;
  uint lrutick = -1;

  // Is the block already cached?
  acquire(&bcache.buclocks[bucno]);
  for(b = bcache.heads[bucno].next; b != &bcache.heads[bucno]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buclocks[bucno]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buclocks[bucno]);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);
  bucno = BUCHASH(blockno);
  acquire(&bcache.buclocks[bucno]);
  for(b = bcache.heads[bucno].next; b != &bcache.heads[bucno]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buclocks[bucno]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buclocks[bucno]);


  for(int i = 0; i < NBUF; i++){
    b = &bcache.buf[i];
    if(lrub && BUCHASH(b->blockno) != BUCHASH(lrub->blockno)) {
      acquire(&bcache.buclocks[BUCHASH(b->blockno)]);
      if(b->ticks < lrutick && b->refcnt == 0) {
        release(&bcache.buclocks[BUCHASH(lrub->blockno)]);
        lrutick = b->ticks;
        lrub = b;
        continue;
      }
      release(&bcache.buclocks[BUCHASH(b->blockno)]);
    }
    else if(lrub) {
      if(b->ticks < lrutick && b->refcnt == 0) {
        lrutick = b->ticks;
        lrub = b;
      }
    }
    else {
      acquire(&bcache.buclocks[BUCHASH(b->blockno)]);
      if(b->ticks < lrutick && b->refcnt == 0) {
        lrutick = b->ticks;
        lrub = b;
        continue;
      }
      release(&bcache.buclocks[BUCHASH(b->blockno)]);
    }
  }
  if(!lrub)
    panic("bget: no buffers");
  lrub->dev = dev;
  lrub->valid = 0;
  lrub->refcnt = 1;
  if(BUCHASH(lrub->blockno) != BUCHASH(blockno)) {
    int curhold = BUCHASH(lrub->blockno), moveto = BUCHASH(blockno);
    acquire(&bcache.buclocks[moveto]);
    // remove
    lrub->next->prev = lrub->prev;
    lrub->prev->next = lrub->next;

    // insert
    lrub->next = bcache.heads[moveto].next;
    lrub->prev = &bcache.heads[moveto];
    bcache.heads[moveto].next->prev = lrub;
    bcache.heads[moveto].next = lrub;

    // modify blockno
    lrub->blockno = blockno;

    release(&bcache.buclocks[moveto]);
    release(&bcache.buclocks[curhold]);
  }
  else {
    lrub->blockno = blockno;
    release(&bcache.buclocks[BUCHASH(blockno)]);
  }

  release(&bcache.lock);
  acquiresleep(&lrub->lock);
  return lrub;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{  
  int blockno;
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  blockno = b->blockno;
  acquire(&bcache.buclocks[BUCHASH(blockno)]);
  if(blockno != b->blockno)
    printf("not equal\n");
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->ticks = ticks;
  }
  release(&bcache.buclocks[BUCHASH(b->blockno)]);
  
}

void
bpin(struct buf *b) {
  int blockno = b->blockno;
  acquire(&bcache.buclocks[BUCHASH(blockno)]);
  if(blockno != b->blockno)
    printf("not equal\n");
  b->refcnt++;
  release(&bcache.buclocks[BUCHASH(b->blockno)]);
}

void
bunpin(struct buf *b) {
  int blockno = b->blockno;
  acquire(&bcache.buclocks[BUCHASH(blockno)]);
  if(blockno != b->blockno)
    printf("not equal\n");
  b->refcnt--;
  release(&bcache.buclocks[BUCHASH(b->blockno)]);
}


