# mxy-sandbox

一个玩具级系统沙盒：**C++ 模拟虚拟系统，Python 渲染屏幕**。

> 这不是真正的 Windows Sandbox，不提供安全隔离。
> 它是一个可交互的**系统模拟器**：C++ 里跑一套虚拟 CPU / 内存 / 进程 / 文件系统，
> Python 通过 socket 接收帧缓冲并显示，同时把键盘鼠标事件转发回去。

## 架构

```
┌──────────────────────┐         TCP          ┌──────────────────────┐
│   Python 前端        │ <------------------> │   C++ 后端           │
│                      │                      │                      │
│  - pygame 显示帧     │   帧缓冲 (RGBA)      │  - 虚拟 CPU          │
│  - 键盘/鼠标转发     │ -------------------> │  - 内存              │
│  - HUD 覆盖层        │   输入事件 (JSON)    │  - 进程调度          │
│                      │ <------------------  │  - 虚拟文件系统      │
│                      │                      │  - 帧缓冲            │
└──────────────────────┘                      └──────────────────────┘
```

- C++ 监听 `127.0.0.1:9000`
- Python 启动后连接
- C++ 每帧发送 `640x480x4` 字节的 RGBA
- Python 发 JSON 行作为输入

## 目录结构

```
mxy-sandbox/
├── core/
│   ├── src/
│   │   ├── main.cpp          入口 + 主循环
│   │   ├── bridge.cpp/.h     TCP 服务器 + 协议解析
│   │   ├── framebuffer.cpp/.h 帧缓冲 + 8x8 位图字体
│   │   ├── cpu.cpp/.h        虚拟 CPU
│   │   ├── memory.h          内存（头文件内联）
│   │   ├── machine.cpp/.h    进程表 / 调度器 / 系统调用
│   │   ├── vfs.cpp/.h        虚拟文件系统
│   │   ├── programs.cpp/.h   演示程序字节码 + 微型汇编器
│   │   └── socket_util.h     跨平台 socket
│   └── CMakeLists.txt
├── shell/
│   ├── main.py
│   ├── client.py
│   ├── display.py
│   ├── input.py
│   └── requirements.txt
├── docs/
│   ├── protocol.md
│   └── isa.md
├── README.md
└── LICENSE
```

## 构建

### C++ 后端

```bash
cd core
mkdir -p build && cd build
cmake ..
cmake --build . --config Release
```

Windows MinGW 单命令：

```bash
cd core
g++ -std=c++17 -O2 -o sandbox-core.exe src/*.cpp -lws2_32
```

Linux：

```bash
cd core
g++ -std=c++17 -O2 -o sandbox-core src/*.cpp -pthread
```

### Python 前端

```bash
cd shell
pip install -r requirements.txt
```

## 运行

终端 1：

```bash
./core/build/sandbox-core
# 或 ./core/sandbox-core
```

终端 2：

```bash
cd shell
python main.py
```

可选参数：`python main.py <host> <port>`。

## 操作

| 按键 | 作用 |
|------|------|
| F1   | 切换 HUD 覆盖层 |
| R    | 重置并重新生成演示进程 |
| Space| 清空帧缓冲 |
| L    | 列出虚拟文件系统根目录 |
| P    | 列出进程 |

## 已实现

- **阶段 1**：TCP 通信打通，Python 显示 C++ 帧
- **阶段 2**：帧缓冲绘制（矩形、直线、8x8 位图文字），键盘鼠标事件转发
- **阶段 3**：虚拟 CPU（32 寄存器、8 字节定长指令），每进程 1 MB 内存
- **阶段 4**：轮转调度（默认 10000 周期一片）、虚拟文件系统、8 个系统调用
- **阶段 5**：通过 `cmd` 消息实现的简易 shell（`spawn` / `kill` / `ls` / `cat` / `ps` / `clear` / `restart`）

## 与原始设计文档的差异

1. **指令编码改为 8 字节定长**。原文档的 4 字节编码只有 8 位立即数，
   无法表示像素索引（最大 307199）或跳转地址。8 字节编码把 `imm` 扩到 32 位，
   代价是代码体积翻倍——对玩具项目完全可接受。
2. **每个进程独立 1 MB 内存**，而不是全局共享。这样两个进程互不干扰，
   更贴近"多进程"的语义。
3. **JSON 解析是扁平的**。控制命令改成扁平结构
   （`{"type":"cmd","name":"spawn","arg":"rect-a"}`），
   避免引入完整 JSON 库。需要嵌套时替换 `bridge.cpp` 里的 `FlatJson` 即可。
4. **状态面板用 pygame HUD 覆盖层**，而不是 tkinter。
   避免两个 GUI 事件循环打架。

## 性能

- 640×480×4 = 1.2 MB/帧，30 fps 约 36 MB/s
- 本地 loopback 完全够用
- 每帧模拟预算 200,000 个周期
- 如需更高分辨率，优先考虑脏矩形而不是全帧发送

## 可选升级

1. 接 RISC-V：用 GCC 编译真程序进沙盒
2. 图形 API：`draw_line`、`draw_circle`、blit sprite（系统调用形式暴露）
3. 音频：Python 侧收 PCM 数据播放
4. 网络模拟：两个 sandbox 实例通过虚拟网卡通信
5. 调试器：Python 侧暂停、单步、查看寄存器
6. 磁盘镜像：虚拟 FS 持久化到本地文件

## 参考

- [Writing a Simple Operating System from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf)
- [Nand2Tetris](https://www.nand2tetris.org/)
- [RISC-V Spec](https://riscv.org/technical/specifications/)
- [QEMU](https://www.qemu.org/)
- [pygame docs](https://www.pygame.org/docs/)

## License

MIT
