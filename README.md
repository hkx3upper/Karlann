# KARLANN
## It's a kernel-based keylogger for Windows x64.
## Foreword：
**Karlann**是一个Windows内核键盘记录器，Poc驱动截取Win32k发送到Kbdclass的IRP，获取键盘的Scancode，并将Scancode转换成对应的字符。  
## Description：
#### 演示：  
获取某即时通讯软件的键盘输入  
![1](https://user-images.githubusercontent.com/41336794/188272341-167188c8-aff0-4b7b-8110-2164a7362aef.gif)  
#### 原理：
![Karlann](https://user-images.githubusercontent.com/41336794/188293026-0bfcdd72-3e2d-47fe-b604-5458d8a710f9.jpg)    
将Win32k驱动用于读键盘数据的hKeyboard->FileObject->DeviceObject替换为Poc驱动的DeviceObject，
由Poc驱动充当中间层驱动，过滤Win32k和Kbdclass的IRP。  
重点在于获取这个FileObject，这个FileObject由ZwReadFile填在Irp->IrpSp->FileObject中，
并且Kbdclass会在没有键盘数据时将IRP保存在它的DeviceExtension->ReadQueue链表中，
虽然Kbdclass的DeviceExtension结构体没有公开，但其中大部分结构的偏移自从Windows 8开始都是不变的，
所以可以找到ReadQueue链表，使用KeyboardClassDequeueRead函数取出IRP，也就取出了FileObject。  
支持PNP，Poc驱动会在IoCancelIrp时将FileObject->DeviceObject还原，以便于之后设备卸载。
#### 未公开的结构体和函数（kbdclass.sys）：
```
DeviceExtension->RemoveLock（DeviceExtension + REMOVE_LOCK_OFFET_DE）
DeviceExtension->SpinLock（DeviceExtension + SPIN_LOCK_OFFSET_DE）  
DeviceExtension->ReadQueue（DeviceExtension + READ_QUEUE_OFFSET_DE）  
kbdclass!KeyboardClassDequeueRead（在驱动内实现）  
```
## Build & Installation：
1.建议在Windows 8 x64 - Windows 10 x64 21H1（19043.1889）环境运行  
```
已测试系统版本:                    0903        0905
Windows 8 x64                   NOTESTED    PASS
Windows 8.1 x64 6.3(9600)       PASS        NOTESTED
Windows 10 x64 1511(10586.164)  PASS        PASS
Windows 10 x64 1607(14393.447)  PASS        NOTESTED
Windows 10 x64 1703(15063.0)    PASS        PASS
Windows 10 x64 1709(16299.15)   PASS        NOTESTED
Windows 10 x64 1809(17763.2928) PASS        PASS
Windows 10 x64 21H1(19043.1889) PASS        PASS
```
2.使用Visual Studio 2019编译Release x64 Poc驱动  
```
不能编译Debug驱动，IO_REMOVE_LOCK在Debug和Release下的定义不同
```
3.使用OsrLoader加载驱动  
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
https://github.com/HighSchoolSoftwareClub/Windows-Research-Kernel-WRK-  
https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf  
https://github.com/ParkHanbum/HypervisorKeylogger  
https://github.com/minglinchen/WinKernelDev/tree/master/Kb_sniff_Mp  
