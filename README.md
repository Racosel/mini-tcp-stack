这是一份为你量身定制的、极其专业的 README.md 测试指南。你可以直接将这段内容复制并保存到你的项目文档中。它不仅记录了 3.3 阶段的核心测试逻辑，也是向任何人（比如面试官或导师）展示你严谨工程能力的绝佳材料。

测试指南：自定义 TCP 协议栈接收逻辑与滑动窗口 (Stage 3.3)
📝 测试目标
本测试旨在验证自定义 TCP 协议栈（mini_tcp）在 Stage 3.3 阶段的核心能力：

可靠数据接收：正确按序接收来自标准 Linux 内核的 TCP 数据段，并写入环形缓冲区 (rcv_buf)。

滑动窗口联动：应用层持续读取数据释放缓冲区后，协议栈能通过 ACK 正确通告更新后的接收窗口（Window Update）。

数据完整性：在理想网络下完成大文件传输，确保字节级的一致性（零漏发、零错位）。

状态机流转与优雅关闭：正确处理 SYN 建立连接，以及接收到 Linux 发送的 FIN 后完成被动关闭（CLOSE_WAIT -> LAST_ACK -> CLOSED）。

⚠️ 前置准备：配置理想网络环境
极其重要：Stage 3.3 尚未实现高级拥塞控制（如 Fast Retransmit）和乱序重组（OOO Buffer）。因此，必须在完美无损的网络环境下进行测试，否则遇到偶尔的丢包会导致协议栈退化为低效的 RTO 死等模式。

请检查并修改 topo.py，确保链路配置中去除了所有延迟和丢包参数：

Python

# 确保 topo.py 中的 addLink 是纯净的局域网配置
self.addLink(h1, s1, bw=10)
self.addLink(h2, s1, bw=10)
🚀 测试执行步骤
1. 编译并启动 Mininet 环境
在宿主机终端进入项目目录，清理并重新编译协议栈，随后启动拓扑：

Bash

make clean && make
sudo mn -c
sudo python3 topo.py
2. 准备测试载荷 (Payload)
在 Mininet 提示符下，使用 dd 命令在 h2（Linux 侧）生成一个 1MB 的随机二进制文件作为测试弹药：

Bash

mininet> h2 dd if=/dev/urandom of=h2_send.dat bs=1K count=1000
3. 启动 Linux 端服务器 (被动发送方)
利用 nc (Netcat) 工具在 h2 启动监听，并将刚刚生成的文件重定向至标准输入。
原理：只要有客户端建立连接，nc 就会立刻将该文件的数据全速灌入 TCP 管道。

Bash

mininet> h2 nc -l -p 8080 < h2_send.dat &
(注：末尾的 & 表示将其放入后台运行，不阻塞当前终端)

4. 启动自定义协议栈 (主动接收方)
在 h1 启动 mini_tcp。程序将主动向 h2 发起三次握手，并在连接建立后开始全速接收数据，应用层逻辑会持续将数据落盘到 recv_from_h2.dat。

Bash

mininet> h1 ./mini_tcp
此时可以通过宿主机开启另一个终端，使用 ls -lh recv_from_h2.dat 观察文件大小是否在快速增加。

5. 触发被动关闭 (四次挥手)
当文件传输完毕后，nc 默认会保持连接不断开（表现为终端停止打印，抓包可见阶段性 Window Probe 或纯空闲状态）。
此时需要人为触发断开操作：

切换回 Mininet 终端，输入 fg 将 nc 调回前台（或者直接 killall nc）。

按下 Ctrl + C 中断 nc 进程。

观察 h1 的终端，期望看到：

Plaintext

[App] Peer closed, closing now...
[App] File recv_from_h2.dat closed successfully.
说明：Linux 发送了 FIN，自定义协议栈成功进入 CLOSE_WAIT 并回复了 FIN 完成挥手。