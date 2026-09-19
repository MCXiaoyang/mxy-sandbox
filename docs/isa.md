# 沙盒指令集

## 寄存器

32 个通用 32 位寄存器 `r0`..`r31`。没有硬性约定，但系统调用按如下约定：

- 参数：`r1`, `r2`, `r3`
- 返回值：`r0`

## 指令编码

每条指令 **8 字节**，小端序：

```
偏移  长度  含义
0     1     opcode
1     1     rd      （低 5 位有效）
2     1     rs1     （低 5 位有效）
3     1     保留，必须为 0
4     4     imm     （int32，小端）
```

PC 是字节偏移，每条指令执行后 `pc += 8`（跳转指令除外）。

## 指令表

| Opcode | 助记符        | 语义                          | 周期 |
|--------|---------------|-------------------------------|------|
| 0x01   | MOV  rd, imm  | `rd = imm`                    | 1    |
| 0x02   | MOVR rd, rs1  | `rd = rs1`                    | 1    |
| 0x03   | ADD  rd, rs1  | `rd = rd + rs1`               | 1    |
| 0x04   | SUB  rd, rs1  | `rd = rd - rs1`               | 1    |
| 0x05   | ADDI rd, imm  | `rd = rd + imm`               | 1    |
| 0x06   | MUL  rd, rs1  | `rd = rd * rs1`               | 3    |
| 0x07   | SHL  rd, imm  | `rd = rd << (imm & 31)`       | 1    |
| 0x08   | JMP  imm      | `pc = imm`                    | 2    |
| 0x09   | JZ   rd, imm  | `if rd == 0: pc = imm`        | 2    |
| 0x0A   | JNZ  rd, imm  | `if rd != 0: pc = imm`        | 2    |
| 0x0B   | LD   rd, [rs1]| `rd = mem32[rs1]`             | 2    |
| 0x0C   | ST   [rd],rs1| `mem32[rd] = rs1`             | 2    |
| 0x0D   | STB  [rd],rs1| `mem8[rd] = rs1 & 0xFF`       | 2    |
| 0x0E   | LDB  rd, [rs1]| `rd = mem8[rs1]`              | 2    |
| 0x0F   | DRAW rd, rs1  | `framebuffer[rd] = rs1`       | 2    |
| 0x10   | SYS  imm      | 系统调用，见下表              | 2~3  |
| 0xFF   | HALT          | 停机                          | -    |

`DRAW` 的 `rd` 是**像素索引**（`y * 640 + x`），`rs1` 是 RGBA 值（0xAABBGGRR）。

## 系统调用

| 编号 | 名称  | 参数                       | 返回              |
|------|-------|----------------------------|-------------------|
| 1    | exit  | r1 = code                  | -                 |
| 2    | sleep | r1 = ticks                 | -                 |
| 3    | draw  | r1 = x, r2 = y, r3 = color | -                 |
| 4    | open  | r1 = pathPtr, r2 = mode    | r0 = fd 或 -1     |
| 5    | read  | r1 = fd, r2 = buf, r3 = len| r0 = 读到字节数   |
| 6    | write | r1 = fd, r2 = buf, r3 = len| r0 = 写入字节数   |
| 7    | close | r1 = fd                    | r0 = 0 或 -1      |
| 8    | print | r1 = strPtr                | -                 |

`mode`：`0` = 只读，`1` = 写（不存在则创建）。
文件描述符从 3 开始分配，每个进程最多 16 个。

## 示例程序

在 (0,0) 处画一个 240×320 的红色矩形：

```asm
        MOV  r1, 240          ; 行计数
        MOV  r8, 0            ; 起始像素索引
        MOV  r4, 0xFF0000FF   ; 红色
outer:
        MOVR r9, r8           ; 本行起始索引
        MOV  r2, 320          ; 列计数
inner:
        DRAW r9, r4
        ADDI r9, 1
        ADDI r2, -1
        JNZ  r2, inner
        ADDI r8, 640          ; 下一行
        ADDI r1, -1
        JNZ  r1, outer
        HALT
```
