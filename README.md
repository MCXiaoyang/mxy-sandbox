# mxy-sandbox

一个玩具级系统沙盒：**C++ 模拟虚拟系统，Python 提供 TTY 前端**。

> 不是真正的 Windows Sandbox，不提供安全隔离。
> 它是一个可交互的系统模拟器：C++ 里跑一套虚拟 CPU / 内存 / 进程 / 文件系统，
> Python 通过 TCP 连接，用纯终端界面操作这台"虚拟机"。

## 架构

```
┌──────────────────────┐         TCP          ┌──────────────────────┐
│   Python TTY 前端    │ <------------------> │   C++ 后端           │
│                      │                      │                      │
│  - 命令行交互        │   文本消息 (TXTL)    │  - 虚拟 CPU          │
│  - 行编辑            │ <------------------  │  - 内存              │
│  - 状态显示          │                      │  - 进程调度          │
│                      │   命令 (JSON)        │  - 虚拟文件系统      │
│                      │ -------------------> │  - 帧缓冲            │
│                      │                      │                      │
│                      │   帧缓冲 (FRM1)      │                      │
│                      │ <------------------  │                      │
└──────────────────────┘                      └──────────────────────┘
```

- C++ 监听 `127.0.0.1:9000`
- TTY 前端在终端里跑，无需图形环境
- 帧缓冲通道仍在，供未来的图形前端使用

## 目录结构

```
mxy-sandbox/
├── core/
│   ├── src/
│   │   ├── main.cpp           入口 + 主循环
│   │   ├── bridge.cpp/.h      TCP 服务器 + 协议
│   │   ├── framebuffer.cpp/.h 帧缓冲 + 8x8 位图字体
│   │   ├── cpu.cpp/.h         虚拟 CPU
│   │   ├── memory.h           内存
│   │   ├── machine.cpp/.h     进程表 / 调度 / 系统调用
│   │   ├── vfs.cpp/.h         虚拟文件系统
│   │   ├── programs.cpp/.h    演示程序字节码 + 微型汇编器
│   │   └── socket_util.h      跨平台 socket
│   └── CMakeLists.txt
├── shell/
│   ├── tty_main.py            终端前端（推荐）
│   └── requirements.txt
├── docs/
│   ├── protocol.md            IPC 协议
│   └── isa.md                 指令集
├── Makefile
├── README.md
└── LICENSE
```

## 构建

### Windows (MinGW)

```bash
cd core
g++ -std=c++17 -O2 -o sandbox-core.exe src\framebuffer.cpp src\vfs.cpp src\cpu.cpp src\machine.cpp src\programs.cpp src\bridge.cpp src\main.cpp -lws2_32
```

### Linux / macOS

```bash
cd core
g++ -std=c++17 -O2 -o sandbox-core src/framebuffer.cpp src/vfs.cpp src/cpu.cpp src/machine.cpp src/programs.cpp src/bridge.cpp src/main.cpp -pthread
```

### 用 Makefile

```bash
make            # 编译 core
make clean      # 清理
```

## 运行

**终端 1** — 启动 core：

```bash
cd core
./sandbox-core
```

输出：

```
[core] mxy-sandbox core listening on 127.0.0.1:9000
[core] press Ctrl+C to quit
[core] waiting for shell...
```

**终端 2** — 连接 TTY：

```bash
cd shell
python tty_main.py
```

Windows 上直接双击 `run-tty.bat`，会自动开两个窗口。

连接成功后你会看到：

```
[tty] connected to 127.0.0.1:9000
[tty] type 'help' for commands, 'quit' to exit

mxy-sandbox TTY
type 'help' for commands

> 
```

## TTY 命令

| 命令 | 说明 |
|------|------|
| `help` | 显示帮助 |
| `ls [path]` | 列出虚拟文件 |
| `cat <path>` | 打印文件内容 |
| `ps` | 列出进程（pid / 名称 / 状态 / pc / 周期） |
| `spawn <name>` | 启动程序：`rect-a` / `rect-b` / `line` / `file` |
| `kill <pid>` | 结束进程 |
| `clear` | 清空帧缓冲 |
| `restart` | 重置机器，重新 spawn 演示进程 |
| `quit` | 断开连接 |

## 快速体验

启动后依次输入：

```
> ps
> ls /
> cat /etc/motd
> spawn line
> ps
> kill 3
> restart
> quit
```

## 指令集

32 个 32 位通用寄存器，8 字节定长指令。

每条指令编码：

```
偏移  长度  含义
0     1     opcode
1     1     rd
2     1     rs1
3     1     保留
4     4     imm32（小端）
```

| Opcode | 助记符 | 语义 |
|--------|--------|------|
| 0x01 | MOV  rd, imm   | rd = imm |
| 0x02 | MOVR rd, rs1   | rd = rs1 |
| 0x03 | ADD  rd, rs1   | rd += rs1 |
| 0x04 | SUB  rd, rs1   | rd -= rs1 |
| 0x05 | ADDI rd, imm   | rd += imm |
| 0x06 | MUL  rd, rs1   | rd *= rs1 |
| 0x07 | SHL  rd, imm   | rd <<= imm |
| 0x08 | JMP  imm       | pc = imm |
| 0x09 | JZ   rd, imm   | if rd == 0: pc = imm |
| 0x0A | JNZ  rd, imm   | if rd != 0: pc = imm |
| 0x0B | LD   rd, [rs1] | rd = mem32[rs1] |
| 0x0C | ST   [rd],rs1  | mem32[rd] = rs1 |
| 0x0D | STB  [rd],rs1  | mem8[rd] = rs1 |
| 0x0E | LDB  rd, [rs1] | rd = mem8[rs1] |
| 0x0F | DRAW rd, rs1   | fb[rd] = rs1 |
| 0x10 | SYS  imm       | 系统调用 |
| 0xFF | HALT           | 停机 |

## 系统调用

| 编号 | 名称  | 参数 | 返回 |
|------|-------|------|------|
| 1 | exit  | r1 = code | - |
| 2 | sleep | r1 = ticks | - |
| 3 | draw  | r1 = x, r2 = y, r3 = color | - |
| 4 | open  | r1 = path, r2 = mode | r0 = fd |
| 5 | read  | r1 = fd, r2 = buf, r3 = len | r0 = n |
| 6 | write | r1 = fd, r2 = buf, r3 = len | r0 = n |
| 7 | close | r1 = fd | r0 = 0 |
| 8 | print | r1 = str | - |

## 已实现

- **虚拟 CPU**：32 个 32 位寄存器，8 字节定长指令，每条带周期计数
- **内存**：每进程独立 1 MB
- **进程调度**：轮转，默认每 10000 周期切换
- **虚拟文件系统**：`/etc`、`/home`、`/tmp`，预置 `/etc/motd`
- **系统调用**：8 个
- **帧缓冲**：640×480 RGBA，支持矩形、直线、8×8 位图文字
- **TTY 前端**：行编辑，外部输出不打断输入
- **TCP 协议**：文本通道 + 帧缓冲通道双路复用

## 工作原理

1. C++ 启动，监听 `127.0.0.1:9000`
2. Python 连接，C++ 主循环进入运行状态
3. 每帧：
   - 处理收到的输入事件（键盘、命令）
   - 执行 200,000 个 CPU 周期
   - 把日志推给 TTY
   - 发送 640×480×4 字节的帧缓冲
4. 调度器在两个演示进程之间轮转，你会在帧缓冲上看到两个矩形交替生长

## 可选升级

1. **图形 API**：`draw_line`、`draw_circle`、blit sprite 作为系统调用暴露
2. **调试器**：`pause` / `step` / `regs`，单步执行、查看寄存器
3. **RISC-V**：替换自定义指令集为 RV32I，用 GCC 编译真程序进沙盒
4. **持久化 FS**：把 VFS 存成 `.img` 文件，重启不丢
5. **多语言客户端**：用 Java / Rust 写另一份前端，展示协议通用性

## 参考

- [Writing a Simple Operating System from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf)
- [Nand2Tetris](https://www.nand2tetris.org/)
- [RISC-V Spec](https://riscv.org/technical/specifications/)

## License

MIT