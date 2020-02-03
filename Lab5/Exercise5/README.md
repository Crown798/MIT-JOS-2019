>Exercise 5. Implement serve_read in fs/serv.c.
```
serve_read's heavy lifting will be done by the already-implemented file_read in fs/fs.c (which, in turn, is just a bunch of calls to file_get_block). serve_read just has to provide the RPC interface for file reading. Look at the comments and code in serve_set_size to get a general idea of how the server functions should be structured.

Use make grade to test your code. Your code should pass "serve_open/file_stat/file_close" and "file_read" for a score of 70/150.
```

# 代码

```
int
serve_read(envid_t envid, union Fsipc *ipc)
{
    struct Fsreq_read *req = &ipc->read;
    struct Fsret_read *ret = &ipc->readRet;

    // Lab 5: Your code here:
    struct OpenFile *o;
    int r;
    r = openfile_lookup(envid, req->req_fileid, &o);  //通过fileid找到Openfile结构
    if (r < 0)      
        return r;
    if ((r = file_read(o->o_file, ret->ret_buf, req->req_n, o->o_fd->fd_offset)) < 0)   //调用fs.c中函数进行真正的读操作
        return r;
    o->o_fd->fd_offset += r;
    
    return r;
}
```