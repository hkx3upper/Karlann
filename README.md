
# KARLANN
## It's a kernel-based keylogger for Windows x64.
## Foreword：
**Karlann**是一个Windows内核键盘记录器，驱动通过定时扫描的方式获取kbdclass.sys缓冲区的键盘Scancode，并将Scancode转换成对应的大小写字符。  
![Karlann](Karlann.gif)
## Description：
#### 原理：
kbdclass.sys的三个关键函数：KeyboardClassHandleRead、KeyboardClassServiceCallback和KeyboardClassReadCopyData  
![Karlann](Karlann.jpg)  
函数KeyboardClassReadCopyData作用是将Scancode从kbdclass.sys的缓冲区拷贝到IRP中；  
对于函数KeyboardClassHandleRead，希望它将IRP插入链表ReadQueue，而不是直接从缓冲区拷贝Scancode到IRP并返回IRP；  
对于函数KeyboardClassServiceCallback，希望它将Scancode拷贝到缓冲区，而不是直接拷贝到IRP并返回IRP。  
因此创建三个线程，分别为：PocDequeueReadThread，PocReadCopyDataThread，PocMoveDatatoIrpThread：  
**PocDequeueReadThread**：用于从kbdclass的IRP链表ReadQueue中抢夺IRP，防止KeyboardClassServiceCallback获得IRP；  
**PocReadCopyDataThread**：用于和KeyboardClassServiceCallback竞争从下层驱动传入的Scancode，暂存到TempBuffer中，防止KeyboardClassHandleRead的InputCount != 0的情况；  
**PocMoveDatatoIrpThread**：用于将TempBuffer中的Scancode传入PocDequeueReadThread抢夺的IRP中，结束并返回IRP。  
#### 不足：
因为没有hook函数，只是用线程抢夺IRP和Scancode，所以小概率会漏掉Scancode（主要发生在键盘按键太快时，通常Makecode和BreakCode只会漏一个，目前基本支持四键无冲），可以额外处理一下获取的Scancode，确保得到正确的键盘数据。  
#### 未公开的结构体和函数（kbdclass.sys）：
```
DeviceExtension->SpinLock（DeviceExtension + SPIN_LOCK_OFFSET_DE）  
DeviceExtension->ReadQueue（DeviceExtension + READ_QUEUE_OFFSET_DE）  
kbdclass!KeyboardClassReadCopyData（在kbdclass.sys内扫描函数的特征码）  
kbdclass!KeyboardClassDequeueRead（在驱动内实现）  
```
## Build & Installation：
1.建议在Windows 8.1 x64 6.3（9600） - Windows 10 x64 21H1（19043.1889）环境运行  
```
已测试系统版本:  
Windows 8.1 x64 6.3(9600)
Windows 10 x64 1511(10586.164)
Windows 10 x64 1709(16299.15)
Windows 10 x64 1809(17763.2928) 
Windows 10 x64 1903(18362.30) 
Windows 10 x64 21H1(19043.1889)  
```
2.使用Visual Studio 2019编译Debug x64 Poc驱动  
3.使用OsrLoader或者kdmapper加载驱动  
## License：
**Karlann**, and all its submodules and repos, unless a license is otherwise specified, are licensed under **GPLv3** LICENSE.  
Dependencies are licensed by their own.  
## Warning：
Using this program might render your computer into an unstable state.  
For educational purposes only, use at your own responsibility.  
## References：
https://github.com/Aekras1a/Labs/tree/master/Labs/WinDDK/7600.16385.1/src/input/kbdclass  
https://github.com/ZoloZiak/WinNT4/tree/master/private/ntos/dd/kbdclass  
https://github.com/ZoloZiak/WinNT4/tree/master/private/ntos/dd/i8042prt  
https://github.com/reactos/reactos/tree/master/drivers/hid/kbdhid  
https://github.com/ZoloZiak/WinNT4/tree/master/private/ntos/w32/ntuser/kernel  