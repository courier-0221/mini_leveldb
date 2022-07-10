# 简介
操作日志，所有的写操作都必须先成功的append到操作日志中，然后再更新内存memtable。这样做有两点：
1.可以将随机的写IO变成append，极大的提高写磁盘速度；
2.防止在节点down机导致内存数据丢失，造成数据丢失，这对系统来说是个灾难。

# 格式
```shell
The log file contents are a sequence of 32KB blocks. 
The only exception is that the tail of thefile may contain a partial block.
Each block consists of a sequence of records:
block:= record* trailer?
record :=
checksum: uint32     // crc32c of type and data[] ; little-endian
length: uint16       // little-endian
type: uint8          // One of FULL,FIRST, MIDDLE, LAST
data: uint8[length]
```
日志是分块写的，每块大小为32K，每条记录有7个字节的头部，前四字节为CRC校验，中间两字节为长度，最后一字节为记录类型。块大小固定有限，而记录有可能跨块，因此有三个枚举值kFirstType、kMiddleType、kLastType分别用来标记记录位置。

Log Type有4种：FULL = 1、FIRST = 2、MIDDLE = 3、LAST = 4。FULL类型表明该log record包含了完整的user record；而user record可能内容很多，超过了block的可用大小，就需要分成几条log record，第一条类型为FIRST，中间的为MIDDLE，最后一条为LAST。也就是：
```shell
FULL，说明该log record包含一个完整的user record；
FIRST，说明是user record的第一条log record
MIDDLE，说明是user record中间的log record
LAST，说明是user record最后的一条log record
```
