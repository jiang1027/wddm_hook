---
title: "Windows WDDM Hook Driver"
date: 2021-02-01
draft: true

---





# Windows 10 WDDM Hook Driver

该驱动使用WDDM hook机制，在Windows DXGK驱动之上，建立了一个额外的device object，用来拦截处理`DxgkInitialize`下发的IOCTL，用于捕获显卡驱动下发的`DRIVER_INITIALIZATION_DATA`结构，这样就可以做各种hook动作。

[^]: Win7和Win10系统上，`DxgkInitialize`下发的IOCTL是不一样的。该API是静态编译，可以直接通过逆向工程的方式，得到`DxgkInitialize`的行为逻辑。

