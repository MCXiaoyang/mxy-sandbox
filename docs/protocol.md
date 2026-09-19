# IPC 协议

C++ 后端监听 `127.0.0.1:9000`，Python 前端主动连接。
连接建立后，双方全双工通信。

## 帧缓冲：C++ → Python

每帧固定 `640 * 480 * 4 = 1,228,800` 字节，RGBA 字节序，行优先。

```
[4 字节 magic 0x46524D31][1228800 字节像素数据]
```

magic 按小端序发送，字节序列为 `31 4D 52 46`。

Python 侧用 `pygame.image.frombuffer(data, (640, 480), "RGBA")` 直接构造 surface。

## 输入事件：Python → C++

每行一个 JSON 对象，以 `\n` 结尾，UTF-8 编码。

### 键盘

```json
{"type":"key","code":65,"pressed":true}
```

`code` 直接使用 pygame 的 `event.key` 常量值。

### 鼠标按键

```json
{"type":"mouse","x":320,"y":240,"button":1,"pressed":true}
```

### 鼠标移动

```json
{"type":"mouse_move","x":320,"y":240}
```

### 退出

```json
{"type":"quit"}
```

## 控制命令：Python → C++

```json
{"type":"cmd","name":"spawn","arg":"rect-a","argi":0}
{"type":"cmd","name":"kill","argi":3}
{"type":"cmd","name":"ls","arg":"/"}
{"type":"cmd","name":"cat","arg":"/etc/motd"}
{"type":"cmd","name":"ps"}
{"type":"cmd","name":"clear"}
{"type":"cmd","name":"restart"}
```

C++ 侧解析是"扁平 JSON"——不支持嵌套对象。如果后续需要嵌套，
把 `core/src/bridge.cpp` 里的 `FlatJson` 换成完整的 JSON 库即可。

## 断连

Python 关闭 socket 后，C++ 的接收线程 `recv` 返回 0，
`connected_` 置 false，主循环退出内层循环，回到 `waitForClient()` 重新等待。
