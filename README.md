powered by 35's Embedded Systems Inc. A next generation Flashlight firmware solution.                                                                                                                        

### 概述

这是由redstoner_35(from 35's Embedded Systems Inc.)开发的适用于入门级手电筒的固件系统。和FlashLightOS Full采用相同的框架实现，但是移除了大量入门级手电筒所不需要的高阶特性以便降低固件的复杂度并极大的减少整个驱动的硬件成本。也降低了对MCU容量的需求
允许用户使用更便宜的MCU以实现低成本和紧凑的驱动设计。

### 硬件需求

+ MCU : HT32F52352/52342 (QFN-32或LQFP-64) 基于CM0+内核 48MHz主频(**也可以移植到同等内核的MCU上面，需要的MCU配置请参考下面的配置**)
+ 电池电压测量：通过简单的分压电阻方式实现，建议阻值为100：10K
+ 驱动和LED温度测量：通过NTC热敏电阻实现，默认参数为10K@25度，B值3950（可以根据需要修改）
+ 调光信号输出：20KHz PWM输出，频率可以配置

#### MCU的硬件需求

+ PWM Timer:至少一个 具备硬件比较输出
+ General Purpose Timer:至少2个，其中一个用于系统的8Hz心跳时钟,另一个负责给特殊挡位提供Time base
+ GPIO : 至少6个GPIO
+ ADC ： 至少3通道 12bit及以上精度 具有转换完毕(EOC)中断

### 工程结构

此工程的代码结构如下：

+ CMSIS ：包含MCU使用到的一些CMSIS库
+ Config ：包含系统的固件基本参数和硬件配置(例如IO分配等)的头文件
+ ExtPeriphal ：包含系统中一些外设（电池监视器，ADC等）的底层驱动以及顶层的代码实现（例如挡位电流调节，故障保护，侧按按键检测和指示灯控制）逻辑
+ Library ：合泰单片机官方提供的外设驱动和初始化相关的库函数
+ MCUConfig ：合泰官方提供的一些MCU相关的配置，汇编文件等
+ MDK-ARM ：合泰官方提供的HT32的启动文件
+ Logic ：实现系统中的挡位和开关逻辑切换以及一些特殊功能（SOS，爆闪，可编程闪，呼吸，摩尔斯发送）的逻辑

### 与FlashLight OS Full Edition的区别

+ 固件类型名称 ：FlashLightOS Lite  |  FlashLightOS Full 
+ 运行和故障日志系统 ：不支持 | 支持
+ 主动散热风扇控制：支持温控 | 因固件容量限制暂不支持
+ 基于Console的用户配置系统：不支持 | 支持
+ 全挡位任意编程功能： 不支持 | 支持
+ 调光信号输出：单路 PWM输出 | 双路 DAC模拟输出
+ 电池检测和保护：仅基于电压的过放保护 | 基于电压或安时积分的RSOC监测，包括过放和过功率保护
+ FRU存储器（序列号）：不支持 | 支持
+ 灯珠输出监测：不支持 | 支持基于LED类型的Vf异常和及灯珠开短路、过电流检测
+ 自适应输出电流校准：不支持 | 支持
+ 按键锁定功能：支持 | 支持
+ 电池电压查询功能：支持 | 支持
+ 过热关机保护：支持 | 支持
+ 基于PID的自适应外壳温控：支持（参数硬编码在固件内不可调节） | 支持（参数可通过Console配置）
+ 温度查询功能：支持 | 支持
+ 电子保修标签和黑匣子功能：不支持 | 支持
+ 战术模式：仅支持正向点射模式（功能不可配置） | 支持正向点射和反向点射双模式（功能可配置）
+ 按键配置：双色LED共阴侧按键（不支持有源夜光定位功能） | 双色LED共阴侧按键（支持有源夜光定位功能）
+ 挡位配置：1-5档循环双击极亮三击爆闪（用户不可更改）| 多达九种模式，用户可按需可任意编程
+ 无极调光：仅单分区无极调光（长按进入，用户不可配置）| 多分区无极调光（用户可任意配置每个分区的上下限亮度和初始亮度等参数）
+ 硬件平台：任意直驱/升降压恒流和线性恒流 | 仅Xtern-Ripper V3（双DCDC互补纯DC降压恒流）
+ 适用对象：入门级手电筒(或者车灯) | 追求极致客制化和性能的高阶手电

### 总体操作逻辑

#### 车灯模式
该固件具备用于带温控车灯的模式。当在固件内取消`CarLampMode`宏的注释并重新编译后，固件将会被配置为车灯模式。**此时固件所有的指示灯功能和按键换挡操作全部失效**，按键引脚将被配置为车灯点亮控制的ACC线，此时当ACC线对GND短接时固件将点亮主LED。
+ 故障提示：虽然此时固件不支持换挡功能，但是低电量/过热保护和温控风扇功能仍然可用。如果固件在上电时检测到测温电阻异常，则固件将通过主LED的闪烁提示用户发生故障。
  - 闪烁3次：驱动的LED温度测量热敏电阻断线或短路，请检查
  - 闪烁4次：驱动测量自身温度的热敏电阻断线或短路，请检查
  - 闪烁5次：指示驱动的所有温度检测电阻均已损坏，此时驱动将会锁定无法继续开机
+ 温控功能：当固件让主LED点亮后，会实时测量驱动和LED的温度并以此控制风扇以及LED的功率避免驱动和LED发生过热。

#### 常规操作

+ 战术模式：关机状态下四击进入/退出。长按开机按照极亮挡位运行（带温控，电池电量不足时自动降档至30%）松开立即熄灭。
+ 循环挡位模式：关机状态下单击开机，长按关机。开机后单击切换下一个挡位，单击+长按切换上一个挡位，双击极亮，三击爆闪。
+ 一键极亮模式：关机状态下双击直接以极亮模式运行。单击退回到循环档位模式，长按关闭。
+ 一键爆闪模式：关机状态下三击直接以爆闪模式运行（电池电量不足时自动降档至30%）。双击进入极亮，单击退回到循环档位模式，长按关闭。
+ 无极调光模式：关机状态下长按开机，开机后长按调整亮度，亮度到最高或最低时会闪0.5秒指示亮度已到限制。单击则关机。

#### 特殊操作

+ 双击+长按：调用电池电压显示器。调用成功后侧按LED会从红切换到绿色，短时间熄灭然后马上宏色闪一下(**注意：如果此处是黄色闪一下，则表示电池电压读数方式是9.99V量程**)表示显示的是电池电压且显示量程模式为99.9V，延迟一秒后系统将开始通过侧按LED播报电池电压。播报方式如下表所示：
 - 99.9V显示模式：先以红色播报十位,以黄色播报个位,最后以绿色播报小数点后一位，每组播报之间有1秒延迟。例如`红(停顿1秒)黄(停顿1秒)绿-绿-绿`表示当前电池电压为11.3V。
 - 9.99V显示模式：先以红色播报个位,以黄色播报小数点后一位,最后以绿色播报小数点后二位，每组播报之间有1秒延迟。例如`红-红-红(停顿1秒)黄-黄(停顿1秒)绿-绿-绿`表示当前电池电压为3.23V。
+ 三击+长按：调用手电温度显示器，手电筒将通过侧按LED指示LED的温度以及LED和驱动的降档情况。首先系统会通过红黄绿快速切换并绿色闪烁一次（**注意：如果绿色闪烁一次后紧跟着黄色闪烁一次，则表示温度读数为负数**）表示即将播报温度信息。延迟1秒后系统将开始播报LED温度信息。播报方式是先以红色播报百位,以黄色播报十位,最后以绿色播报个位，每组播报之间有1秒延迟。LED温度播报结束之后将会有两次较慢的闪烁分别表示LED温度和驱动自身的温度所对应的降档情况。例如`黄-黄-黄(停顿1秒)绿-绿-绿(停顿1秒)绿(0.5秒停顿)绿`表示当前LED温度为33℃,且驱动和LED都未达到降档阈值。对于后面的两个较慢闪烁的降档指示，各颜色所指示的内容可参考下表：
 - 绿色：对应部件的温度未达到PID恒温阈值，部件散热良好
 - 黄色：对应部件的温度达到PID恒温阈值，部件散热良好
 - 红色：对应部件的温度超过温控启动阈值，部件散热不良请检查
 - 红色快速闪烁2次：该部件的NTC热敏电阻已损坏，驱动无法获知其温度信息。
+ 四击：进入退出战术模式。
+ 五击：锁定/解锁手电。
+ 六击：强制使能/禁用风扇。（在手电极亮结束主动关机后，风扇仍会继续运转给手电散热，直到手电温度降低到指定阈值。此时用户可以通过6击暂时性的强制关闭风扇。关闭风扇后如果再次6击则开启风扇）

### 自检通过提示和电池节数识别

该固件在正常启动完毕后，指示灯会红黄绿过渡并以红色和绿色各快速闪烁一次提示用户。闪烁完毕后固件将会以指定次数的绿色闪烁显示识别到的电池节数，如1节电池将会闪烁1次，2节会闪烁2次并以此类推。电池节数识别为实验性功能，因识别方案基于电压检测，在大于2节串联的情况下会存在识别不准确的问题。对于固件的需要则可通过取消`NoAutoBatteryDetect`宏的注释以禁用电池自动识别功能。此时固件将会按照`BatteryCellCount`宏所指定的电池节数执行电量检测和保护逻辑。如果节数识别不准确，用户可以在固件显示识别到的电池节数的时候双击侧按，此时驱动将会进入电池节数设置模式，并且用两次黄色闪烁以及红色和绿色各快速闪烁一次提示用户，此时用户需要按照电池节数按下侧按指定次数，例如一节电池则按一下。完成后驱动将通过黄色闪烁指示用户设置的电池节数。

### 指示灯信息

+ 绿色持续点亮：手电正常运行中，电量充足
+ 黄色持续点亮：手电正常运行中，电量较为充足
+ 红色持续点亮：手电正常运行中，电量不足
+ 红色慢速闪烁：手电电量严重不足，请充电
+ 红色闪1次停留1秒绿色闪1次：驱动的LED温度测量热敏电阻断线或短路，请检查
+ 红色闪1次停留1秒绿色闪2次：驱动测量自身温度的热敏电阻断线或短路，请检查
+ 红色闪1次停留1秒绿色闪3次：单片机的ADC测量时出现异常
+ 红色闪2次停留1秒绿色闪1次：驱动过热关机
+ 红色闪2次停留1秒绿色闪2次：LED过热导致驱动保护关机
+ 红色闪2次停留1秒绿色闪3次：指示驱动的所有温度检测电阻均已损坏，此时驱动将会锁定无法继续开机
+ 红灯亮0.5秒后转为绿灯：手电已成功解锁，此时所有功能可用
+ 绿灯亮0.5秒后转为红灯：手电已成功锁定，此时侧按按钮的调档和开关功能被禁用，需要再次5击侧按方可解锁。
+ 红灯快速闪3次 ：手电已被锁定，您的操作无效，请解锁手电后再试
+ 黄色闪2次后绿色1次：进入战术模式
+ 黄色闪2次后绿色2次：退出战术模式
+ 红灯快速闪5次 ：手电因电量严重不足已禁止开机，请为手电充电

### 关于驱动NTC自检策略的说明

该固件具有两组分别用于测量LED温度和驱动自身温度的热敏电阻。为了增加系统可靠性和fail-safe能力，该固件具有两种可选择的NTC自检模式。

+ 严格模式：此时驱动的两个热敏电阻均必须存在。如果任一热敏电阻发生损坏（例如开路或短路）驱动将直接进入故障保护模式，此时驱动不响应按键操作并拒绝运行，指示灯将持续显示损坏的热敏电阻所在的位置（参考指示灯信息章节）。
+ 默认模式：此时驱动的两个热敏电阻可以只存在一个。如果任一热敏电阻发生损坏（例如开路或短路）驱动将会检查剩下的一个热敏电阻是否正常运行，如正常则驱动将以该热敏电阻的测量值作为温控系统的处理值。如驱动的两个热敏电阻均发生损坏，则驱动将会进入故障保护模式。此时驱动不响应按键操作并拒绝运行，指示灯持续以红色闪2次停留1秒绿色闪3次，停顿两秒的方式循环。

----------------------------------------------------------------------------------------------------------------------------------
© redstoner_35 @ 35's Embedded Systems Inc.  2024