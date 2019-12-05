>Exercise 8. We have omitted a small fragment of code - the code necessary to print octal numbers using patterns of the form "%o". Find and fill in this code fragment.

找到并完善格式化输出函数，使之可以输出八进制数。

格式化输出及控制台文件解读见实验笔记。

真正实现字符串输出的是vprintfmt()函数，在函数中找到`case 'o'`的地方补充代码：
```
            // 从ap指向的可变字符串中获取输出的值
            num = getuint(&ap, lflag);
            //设置基数为8
            base = 8;
            goto number;
```