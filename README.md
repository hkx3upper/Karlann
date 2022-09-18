# KARLANN
## It's a kernel-based keylogger for Windows x86/x64. <a href="https://github.com/hkx3upper/Karlann/wiki">English</a>  
## Foreword：
**Karlann**是一个Windows内核键盘记录器，Poc驱动拦截Win32k发送到Kbdclass的IRP，获取键盘的Scancode，并通过UDP将Scancode转换成的字符发送到服务端。  
## Description：
#### Demo：  
获取某即时通讯软件的键盘输入  
![1](https://user-images.githubusercontent.com/41336794/188272341-167188c8-aff0-4b7b-8110-2164a7362aef.gif)  
#### Mechanism：
![Karlann](https://user-images.githubusercontent.com/41336794/188293026-0bfcdd72-3e2d-47fe-b604-5458d8a710f9.jpg)    
```
1.将Win32k驱动用于读键盘数据的hKeyboard->FileObject->DeviceObject替换为Poc驱动的DeviceObject，
由Poc驱动充当中间层驱动，过滤Win32k和Kbdclass的IRP。  
2.重点在于获取这个FileObject，这个FileObject由ZwReadFile填在Irp->IrpSp->FileObject中，
并且Kbdclass会在没有键盘数据时将IRP保存在它的DeviceExtension->ReadQueue链表中，
虽然Kbdclass的DeviceExtension结构体没有公开，但其中大部分结构的偏移自从Windows 8开始都是不变的，
所以可以找到ReadQueue链表，使用KeyboardClassDequeueRead函数取出IRP，也就取出了FileObject。  
3.为支持PNP，Poc驱动会在IoCancelIrp时将FileObject->DeviceObject还原，以便于之后设备卸载。
4.使用libwsk库(下方References)，把它的C++库做了一些调整，libwsk.h所有函数声明加前缀extern "C"后编译，
实现了通过UDP传输键盘数据的功能。
5.增加按键映射的功能，在Kbd.c->PocConfigureKeyMapping。
6.支持Windows 7 x86 - Windows 10 x86。
```
#### Undocumented struct（kbdclass.sys）：
```
DeviceExtension->RemoveLock（DeviceExtension + REMOVE_LOCK_OFFET_DE）
DeviceExtension->SpinLock（DeviceExtension + SPIN_LOCK_OFFSET_DE）  
DeviceExtension->ReadQueue（DeviceExtension + READ_QUEUE_OFFSET_DE）  
kbdclass!KeyboardClassDequeueRead（在驱动内实现）  
```
## Build & Installation：
1.建议在Windows 7 x86/x64 6.1（7601）SP1 - Windows 10 x86/x64 21H1（19043.1889）环境运行  
```
已测试系统版本:                  0903        0905        0906
Windows 7  x64 6.1(7601) SP1			        PASS
Windows 8  x64 6.2(9200)        NOTESTED    PASS        PASS
Windows 8.1x64 6.3(9600)        PASS        NOTESTED    NT
Windows 10 x64 1511(10586.164)  PASS        PASS        PASS
Windows 10 x64 1607(14393.447)  PASS        PASS        PASS
Windows 10 x64 1703(15063.0)    PASS        PASS        NT
Windows 10 x64 1709(16299.15)   PASS        PASS        PASS
Windows 10 x64 1809(17763.2928) PASS        PASS        PASS
Windows 10 x64 21H1(19043.1889) PASS        PASS        PASS

Windows 7  x86 6.1(7601) SP1			        PASS
Windows 10 x86 1909(18363.592)			        PASS
```
2.修改global.h中的POC_IP_ADDRESS（SocketTest所在电脑的IP）和POC_UDP_PORT，修改项目->属性->Driver Setting->Target OS Vserion为对应的版本，使用Visual Studio 2019编译Release x86/x64 Poc驱动  
```
不能编译Debug驱动，IO_REMOVE_LOCK在Debug和Release下的定义不同
```
3.系统开启测试模式，cmd以管理员身份运行，输入`bcdedit /set testsigning on`后重启电脑  
4.驱动日志输出（可选）  
```
找到注册表项：HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\Debug Print Filter  
没有Debug Print Filter就新建，在这个键下新建dword值 “default”，十六进制为0xF，然后重启电脑  
以管理员身份运行DebugView，设置`Capture->Capture Kernel`显示驱动日志  
```
5.使用OsrLoader加载驱动  
6.在局域网使用SocketTest监听global.h里的端口（可选）  
![SocketTest](https://user-images.githubusercontent.com/41336794/188532624-a1cb49bf-748e-4fe9-ae2a-f7c3f41f2996.JPG)
## License：
**Karlann**, and all its submodules and repos, unless a license is otherwise specified, are licensed under **GPLv3** LICENSE.  
Dependencies are licensed by their own.  
## Warning：
For educational purposes only, use at your own responsibility.  
And using this program might render your computer into an unstable state.  
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
https://github.com/MiroKaku/libwsk  
https://github.com/akshath/SocketTest  
