>Exercise 12. Modify your stack backtrace function to display, for each eip, the function name, source file name, and line number corresponding to that eip.

>Complete the implementation of debuginfo_eip by inserting the call to stab_binsearch to find the line number for an address.

>Add a backtrace command to the kernel monitor

继续完善mon_backtrace()函数，输出信息中对于每个eip，应加上函数调用者的名字、源文件的名字、对应eip的行号、偏移量：
```
Stack backtrace:
  ebp f010ff78  eip f01008ae  args 00000001 f010ff8c 00000000 f0110580 00000000
         kern/monitor.c:143: monitor+106
  ebp f010ffd8  eip f0100193  args 00000000 00001aac 00000660 00000000 00000000
         kern/init.c:49: i386_init+59
  ebp f010fff8  eip f010003d  args 00000000 00000000 0000ffff 10cf9a00 0000ffff
         kern/entry.S:70: <unknown>+0
```

# 分析

在/kern/kdebug.c中，实验提供了`int debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)`函数，该函数输入eip，和一个Eipdebuginfo结构指针，执行完毕后，会将eip对应的信息填充到该结构中。

在/kern/kdebug.h中，Eipdebuginfo结构体的定义：
```
struct Eipdebuginfo {
	const char *eip_file;		// Source code filename for EIP
	int eip_line;			// Source code linenumber for EIP

	const char *eip_fn_name;	// Name of function containing EIP
					//  - Note: not null terminated!
	int eip_fn_namelen;		// Length of function name
	uintptr_t eip_fn_addr;		// Address of start of function
	int eip_fn_narg;		// Number of function arguments
};
```
在/kern/kdebug.c中，stab_binsearch(stabs, region_left, region_right, type, addr)函数在符号表stabs的region_left到region_right区域，搜索对应addr地址、类型为type的符号条目：
```
static void
stab_binsearch(const struct Stab *stabs, int *region_left, int *region_right,
	       int type, uintptr_t addr)
{
	int l = *region_left, r = *region_right, any_matches = 0;

	while (l <= r) {
		int true_m = (l + r) / 2, m = true_m;

		// search for earliest stab with right type
		while (m >= l && stabs[m].n_type != type)
			m--;
		if (m < l) {	// no match in [l, m]
			l = true_m + 1;
			continue;
		}

		// actual binary search
		any_matches = 1;
		if (stabs[m].n_value < addr) {
			*region_left = m;
			l = true_m + 1;
		} else if (stabs[m].n_value > addr) {
			*region_right = m - 1;
			r = m - 1;
		} else {
			// exact match for 'addr', but continue loop to find
			// *region_right
			*region_left = m;
			l = m;
			addr++;
		}
	}

	if (!any_matches)
		*region_right = *region_left - 1;
	else {
		// find rightmost region containing 'addr'
		for (l = *region_right;
		     l > *region_left && stabs[l].n_type != type;
		     l--)
			/* do nothing */;
		*region_left = l;
	}
}

```
在/kern/kdebug.c中，debuginfo_eip函数已经调用stab_binsearch搜索到了所需的其余部分，除了行号部分：
```
int
debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)
{
	……
	// Now we find the right stabs that define the function containing
	// 'eip'.  First, we find the basic source file containing 'eip'.
	// Then, we look in that source file for the function.  Then we look
	// for the line number.

	// Search the entire set of stabs for the source file (type N_SO).
	lfile = 0;
	rfile = (stab_end - stabs) - 1;
	stab_binsearch(stabs, &lfile, &rfile, N_SO, addr);
	if (lfile == 0)
		return -1;

	// Search within that file's stabs for the function definition
	// (N_FUN).
	lfun = lfile;
	rfun = rfile;
	stab_binsearch(stabs, &lfun, &rfun, N_FUN, addr);

	if (lfun <= rfun) {
		// stabs[lfun] points to the function name
		// in the string table, but check bounds just in case.
		if (stabs[lfun].n_strx < stabstr_end - stabstr)
			info->eip_fn_name = stabstr + stabs[lfun].n_strx;
		info->eip_fn_addr = stabs[lfun].n_value;
		addr -= info->eip_fn_addr;
		// Search within the function definition for the line number.
		lline = lfun;
		rline = rfun;
	} else {
		// Couldn't find function stab!  Maybe we're in an assembly
		// file.  Search the whole file for the line number.
		info->eip_fn_addr = addr;
		lline = lfile;
		rline = rfile;
	}
	// Ignore stuff after the colon.
	info->eip_fn_namelen = strfind(info->eip_fn_name, ':') - info->eip_fn_name;


	// Search within [lline, rline] for the line number stab.
	// If found, set info->eip_line to the right line number.
	// If not found, return -1.
	//
	// Hint:
	//	There's a particular stabs type used for line numbers.
	//	Look at the STABS documentation and <inc/stab.h> to find
	//	which one.
	// Your code here.
    ……
}
```
# 实现
完善debuginfo_eip函数：
```
int
debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)
{
	……
	// Search within [lline, rline] for the line number stab.
	// If found, set info->eip_line to the right line number.
	// If not found, return -1.
	//
	// Hint:
	//	There's a particular stabs type used for line numbers.
	//	Look at the STABS documentation and <inc/stab.h> to find
	//	which one.
	// Your code here.

    stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
    if (lline <= rline) {
        info->eip_line = stabs[lline].n_desc;
    }
    else {
        cprintf("line not find\n");
    }
    ……
}
```
完善mon_backtrace函数：
```
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
    // Your code here.
    uint32_t *ebp = (uint32_t *)read_ebp();
    struct Eipdebuginfo eipdebuginfo;
    while (ebp != 0) {
        //打印ebp, eip, 最近的五个参数
        uint32_t eip = *(ebp + 1);
        cprintf("ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", ebp, eip, *(ebp + 2), *(ebp + 3), *(ebp + 4), *(ebp + 5), *(ebp + 6));
        //打印文件名等信息
        debuginfo_eip((uintptr_t)eip, &eipdebuginfo);
        cprintf("%s:%d", eipdebuginfo.eip_file, eipdebuginfo.eip_line);
        cprintf(": %.*s+%d\n", eipdebuginfo.eip_fn_namelen, eipdebuginfo.eip_fn_name, eipdebuginfo.eip_fn_addr);
        //更新ebp
        ebp = (uint32_t *)(*ebp);
    }
    return 0;
}
```
将mon_backtrace加入monitor.c的命令列表中：
```
static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
    { "backtrace", "Display backtrace info", mon_backtrace },
};
```