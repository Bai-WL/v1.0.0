# 菜单系统与 RS485 通信整合：完整实现与代码映射指南

本文档结合了前期的“本地缓存 + 异步任务队列”设计架构，以及针对现有 `Application/Rs485_rtu/` 目录下代码的深入分析，提供了一份从理论机制到实际代码落地的完整实施规范。

现有底层通信架构已经具备了非常优秀的“双队列”和“本地存储”雏形。我们可以完美地将理论设计方案与现有代码结合，实现 UI 层与通信层的彻底解耦。

---

## 1. 核心架构映射与数据结构

为了实现 UI 与底层通信的完全解耦，严禁在 UI 渲染时直接调用 RS485 收发函数。系统的三大核心机制与现有代码对应如下：

- **异步任务队列 (Async Queue)** <=> 对应 `user_mb_controller.c` 中的 `priority_queue` 以及 `user_poll.c` 中的 `WriteValue()`。
- **本地哈希缓存 (Local Hash Cache)** <=> 替换原有的 `device_storage.c` 映射，自行实现基于哈希算法的 `HashCache_SetWord()` / `HashCache_GetWord()` 接口，以极低 RAM 占用管理离散的 Modbus 地址。
- **页面级按需订阅 (Page Subscription)** <=> 对应 `poll_manager.c` 中的 `PollManager_RegisterScreenAddresses()`。

---

## 2. 异步任务队列的结合与优化

### 2.1 现有机制分析
在 `user_mb_controller.c` 中，已经实现了一个严格优先级的状态机：
- `priority_queue`（深度 8）：专用于写请求（0x05, 0x06, 0x10），优先级最高。
- `normal_queue`（深度 8）：专用于读请求（0x01, 0x03）。

在 `user_poll.c` 中，`WriteValue` 函数负责封装用户的写操作并调用 `MBController_Request` 压入 `priority_queue`。这完全符合菜单系统“异步无阻塞”的需求。

### 2.2 菜单系统的调用方式
当用户在菜单中修改了参数并按下“确认”时：
1. UI 层**绝对不**直接操作串口，而是直接调用：
   `WriteValue(item->reg_addr, item->data_width, new_value);`
2. `WriteValue` 会将写请求打包压入队列后立即返回，UI 层无缝衔接，继续响应按键或渲染“保存中”的动画。
3. 底层 `MBController_Process` 任务在后台循环中检测到 `priority_queue` 非空，自动优先向 RS485 侧发出写指令。

### 2.3 关键代码优化建议（必须执行）
**现状缺陷**：现有的 `WriteValue` 函数中使用了 `malloc` 来分配写入数据内存，并在 `MBController_Process` 发送完成后使用 `free` 释放：
```c
uint16_t *data = (uint16_t *)malloc(sizeof(uint16_t));
```
在嵌入式单片机且用户频繁操作菜单的场景下，频繁的 `malloc/free` 极易引发**内存碎片**甚至导致死机。

**优化方案**：修改 `user_mb_controller.h` 中的 `MB_Request` 结构体，直接在结构体内部预留 4 字节（双字）的数据空间，彻底消除动态内存分配。
```c
typedef struct {
    MB_ReqType type;
    uint8_t slave_addr;
    uint16_t reg_addr;
    union {
        struct {
            // 将指针改为固定数组，消除 malloc
            uint16_t data_payload[2]; 
            uint8_t len_of_data;
        };
        struct {
            uint8_t data_len; // 读取长度
        };
    };
} MB_Request;
```
随后修改 `WriteValue`，直接将值赋给 `req.data_payload` 即可。

---

## 3. 本地哈希缓存 (Hash Cache) 与 UI 渲染的无缝对接

为了解决设备数据离散且不连续的问题，放弃原有的 `device_storage.c`（它可能为连续地址空间分配了过多无效内存），转而自行开发一套紧凑的哈希缓存系统。

### 3.1 哈希算法设计与冲突解决
我们采用“静态预分配+除留余数法+线性探测法”的组合策略来管理离散的寄存器地址：
1. **数据结构**：
   ```c
   #define CACHE_TABLE_SIZE 127 // 根据菜单总数选一个合适的素数
   typedef struct {
       uint16_t reg_addr;
       uint16_t reg_value;
       bool is_valid;
   } CacheNode;
   CacheNode g_HashCache[CACHE_TABLE_SIZE];
   ```
2. **初始化注入**：在系统启动（或菜单初始化）时，遍历所有菜单项，提取出所有的 `reg_addr`，依次使用哈希算法 `index = reg_addr % CACHE_TABLE_SIZE` 计算槽位。
3. **冲突解决**：如果计算出的槽位 `is_valid` 且 `reg_addr` 不同，则使用线性探测法 `index = (index + 1) % CACHE_TABLE_SIZE` 找下一个空槽位，并标记该槽位属于这个 `reg_addr`。
4. **运行时操作**：在之后的运行过程中，底层通信层更新数据、UI 层读取数据时，由于所有的槽位已经在初始化时完全确定，只需按同样的哈希算法快速查找即可，无需进行任何动态内存分配。

### 3.2 通信层的数据更新
在 `user_mb_controller.c` 的 `MBController_Process` 接收到 `MODBUS_SUCESS` 响应后，移除原本对 `DeviceStorage_SetWord` 的调用，改为调用我们新设计的哈希缓存接口：
```c
HashCache_SetWord(cache_addr, rx_buffer[i]);
```

### 3.3 菜单系统的渲染调用
1. **无阻塞渲染**：在 UI 的 `render_menu_item` 中，当需要绘制数值时，直接从哈希缓存中极速读取：
   ```c
   uint16_t val = 0;
   if (HashCache_GetWord(item->reg_addr, &val)) {
       // 转换为字符串并显示 val
   } else {
       // 显示 "--" (表示数据尚未就绪)
   }
   ```
2. **自动刷新**：UI 只需要维持例如 200ms 的定期局部刷新即可。底层通信任务读到新数据更新 Cache 后，UI 的下一次刷新会自动将最新数据显示在屏幕上。

---

## 4. 动态页面轮询机制的结合

### 4.1 现有机制分析
`poll_manager.c` 已经具备了非常完善的页面管理功能：
- `screenAddrs` 数组管理各个页面的地址。
- `PollManager_RegisterScreenAddresses()` 允许外部注册页面地址。
- `PollManager_GetPollingAddresses()` 会将“当前活动页面地址”与“常驻地址（alwaysAddrs）”结合。

### 4.2 菜单系统的调用方式
1. **页面进入 (Enter Page)**：在菜单跳转并构建好新页面的结构后，遍历当前页面的所有 `MenuItem`，提取所有的 `reg_addr` 形成数组，并调用：
   `PollManager_RegisterScreenAddresses(current_screen_id, addr_array, type_array, count);`
2. **设置活动页**：调用 `PollManager_SetActiveScreen(current_screen_id);` 通知底层只轮询当前页面的数据。
3. **智能合并读请求**：底层 `user_poll.c` 中的 `generate_requests` 函数已经实现了极具智慧的**地址块合并 (merge_with_gap)** 逻辑，这会让连续或相近的菜单地址自动合并为一条 `0x03` 批量读指令，极大提升了总线利用率。

---

## 5. 告警与全局事件驱动机制

对于非菜单项（如系统告警）等需要高频监控的地址，采用“独立常驻高频区与全局事件驱动”的机制：
1. **常驻轮询**：利用 `poll_manager.c` 中的 `alwaysAddrs` 数组，在系统初始化时直接硬编码注册告警、核心状态等地址，保证全局常驻读取。
2. **跳变检测**：`PollManager_CheckAndUpdateValues()` 已经具备了值跳变检测功能。
3. **UI 层全局响应**：在主循环中，当检测到上述函数返回变化数量（`changedCount > 0`）并且属于告警地址时，调用预先注册的回调触发 UI 层全局事件。不论当前处于什么页面，UI 层接收到告警事件后，立即触发全局告警弹窗（MessageBox）打断当前显示，强行提示用户。

---

## 6. 总结与后续开发核心任务清单

当前的 `Rs485_rtu` 代码基础非常好，与设计的“缓存+异步队列”架构契合度高达 90%。后续核心开发工作明确如下：

1. **底层优化**：修改 `MB_Request` 结构体，并移除 `WriteValue` 中的 `malloc` 逻辑（详见 2.3 节），防止内存碎片。
2. **生命周期接入**：在菜单系统（如 `menu_system.c`）的页面切换（跳转进入）逻辑中，加入对 `PollManager_RegisterScreenAddresses` 和 `PollManager_SetActiveScreen` 的调用。
3. **渲染接入**：在菜单系统的 UI 渲染逻辑中，接入我们自行开发的 `HashCache_GetWord` 等哈希接口以获取数据显示。
4. **事件接入**：在菜单系统修改保存时调用 `WriteValue`，并处理告警跳变的弹窗逻辑。
