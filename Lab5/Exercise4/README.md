>Exercise 4. Implement file_block_walk and file_get_block. file_block_walk maps from a block offset within a file to the pointer for that block in the struct File or the indirect block, very much like what pgdir_walk did for page tables. file_get_block goes one step further and maps to the actual disk block, allocating a new one if necessary.

Use make grade to test your code. Your code should pass "file_open", "file_get_block", and "file_flush/file_truncated/file rewrite", and "testfile".

# 代码

f->f_direct[filebno] 和 f->f_indirect 中存放的都是块号，diskaddr 用于将块号转换为FS进程的虚存地址。ppdiskbno 最终指向块号。
```
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
    // LAB 5: Your code here.
    int bn;
    uint32_t *indirects;
    if (filebno >= NDIRECT + NINDIRECT)
        return -E_INVAL;

    if (filebno < NDIRECT) {
        *ppdiskbno = &(f->f_direct[filebno]);
    } else {
        if (f->f_indirect) {
            indirects = diskaddr(f->f_indirect);
            *ppdiskbno = &(indirects[filebno - NDIRECT]);
        } else {                   // 需要分配索引块
            if (!alloc)
                return -E_NOT_FOUND;
            if ((bn = alloc_block()) < 0)
                return bn;
            f->f_indirect = bn;
            flush_block(diskaddr(bn));
            indirects = diskaddr(bn);
            *ppdiskbno = &(indirects[filebno - NDIRECT]);
        }
    }

    return 0;
}
```

blk 最终指向 filebno 块上的起始内容。
```
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
       // LAB 5: Your code here.
        int r;
        uint32_t *pdiskbno;
        if ((r = file_block_walk(f, filebno, &pdiskbno, true)) < 0) {
            return r;
        }

        int bn;
        if (*pdiskbno == 0) {           //该块需要分配空间
            if ((bn = alloc_block()) < 0) {
                return bn;
            }
            *pdiskbno = bn;
            flush_block(diskaddr(bn));
        }
        *blk = diskaddr(*pdiskbno);
        return 0;
}
```

