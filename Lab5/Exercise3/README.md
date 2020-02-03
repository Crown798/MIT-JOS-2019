>Exercise 3. Use free_block as a model to implement alloc_block in fs/fs.c, which should find a free disk block in the bitmap, mark it used, and return the number of that block. When you allocate a block, you should immediately flush the changed bitmap block to disk with flush_block, to help file system consistency.

Use make grade to test your code. Your code should now pass "alloc_block".

# 代码

bitmap 数组中一个元素占 32 位，即表示 32 个 block 的情况。
```
alloc_block(void)
{
    // The bitmap consists of one or more blocks.  A single bitmap block
    // contains the in-use bits for BLKBITSIZE blocks.  There are
    // super->s_nblocks blocks in the disk altogether.

    // LAB 5: Your code here.
    uint32_t bmpblock_start = 2;
    for (uint32_t blockno = 0; blockno < super->s_nblocks; blockno++) {
        if (block_is_free(blockno)) {                   
            bitmap[blockno / 32] &= ~(1 << (blockno % 32));     //标记已使用
            flush_block(diskaddr(bmpblock_start + (blockno / 32) / NINDIRECT)); //将修改的 bitmap block 写回磁盘
            return blockno;
        }
    }
    
    return -E_NO_DISK;
}
```