# 自定义 TCP 协议栈 (mini_tcp) 核心架构与阶段演进说明

## 项目概述

本项目旨在用户态通过 C 语言从零实现一个遵循 RFC 标准的极简 TCP (Transmission Control Protocol) 协议栈。项目采用分阶段迭代的开发模式，涵盖了从基础的数据封装与状态机流转，到复杂的滑动窗口、拥塞控制、乱序重组及极端边界情况处理。

---

## 阶段三：基础可靠传输与流量控制 (Flow Control)

本阶段确立了 TCP 的核心收发机制与端到端流量控制。

* **3.1 基础数据发送 (`tcp_write` & `tcp_push`)**
* 实现应用层字节流到 TCP 数据段 (Segment) 的封装。
* 构建 TCP 头部，计算 Checksum，并向网络层下发携带 `PSH` 和 `ACK` 标志位的数据包。


* **3.2 基础数据接收与确认 (`tcp_input` & ACK)**
* 实现接收端对传入序列号 (`seq`) 的校验与数据提取。
* 按序将有效载荷 (Payload) 写入接收环形缓冲区 (`rcv_buf`)，推进期望序列号 (`rcv_nxt`)，并向对端回复基础确认报文。


* **3.3 滑动窗口与动态反馈 (Sliding Window)**
* 实现基于缓冲区的动态窗口通告机制 (Window Update)。
* 发送端根据接收端返回的 `Window Size` 严格限制未确认数据量 (In-flight bytes)，防止接收方缓冲区溢出。
* 应用层读取数据后，底层自动触发窗口更新报文，形成完整的流量控制闭环。



---

## 阶段四：拥塞控制与动态重传 (Congestion Control & Reliability)

本阶段引入了网络状态感知机制，提升了协议栈在复杂网络环境下的鲁棒性。

* **4.1 拥塞窗口 (`cwnd`) 的引入**
* 在发送侧实现拥塞窗口机制，结合对端通告窗口 (`rwnd`)，计算实际有效发送窗口 (`effective_wnd = min(cwnd, rwnd)`)。
* 初步实现拥塞避免逻辑，限制网络突发流量。


* **4.2 动态 RTT 测量与 RTO 收敛 (Jacobson/Karels Algorithm)**
* 废弃固定超时机制，实现基于网络真实延迟的动态采样。
* 通过采集 `SampleRTT`，利用平滑因子动态计算 `SRTT` (平滑往返时间) 与 `RTTVAR` (往返时间抖动)。
* 依据算法公式动态推演 `RTO` (重传超时时间)，适应不同网络拓扑的延迟波动。


* **4.3 快速重传机制 (Fast Retransmit / 3 DupACK)**
* 接收端在收到乱序报文时，立即回复携带当前 `rcv_nxt` 的重复确认 (Duplicate ACK)。
* 发送端实现连续收到 3 个相同 DupACK 时，不等待 RTO 超时，立即重传缺失的数据段，大幅降低丢包带来的传输延迟。



---

## 阶段五：高级优化与极端边界处理 (Advanced Edge Cases)

本阶段解决了协议栈在恶劣网络条件下的性能瓶颈与潜在死锁问题，达到了工业级健壮性。

* **5.2 乱序重组缓冲区 (Out-of-Order Buffer)**
* 在接收侧引入 OOO 链表结构。
* 当网络发生丢包或乱序到达时，不再粗暴丢弃未来报文，而是将其暂存至 OOO 缓存中。
* 当缺失的“空洞”被重传报文填补后，自动合并 OOO 缓存中的连续数据段，消除 Go-Back-N 带来的大量无效重传，逼近 Selective Repeat (选择性重传) 的性能。


* **5.3 零窗口探测与防死锁 (Zero Window Probe & Persist Timer)**
* 处理因接收方应用层卡顿导致的 `Window Size = 0` 且后续 Window Update 报文丢失引发的永久死锁危机。
* 实现坚持定时器 (Persist Timer) 与指数退避算法。
* 当发送端感知到零窗口时，定期发送携带无意义数据（或旧序列号）的探测报文，强制对方回复最新窗口状态，确保连接状态的最终一致性。



---

## 测试与验证 (Validation)

* **网络仿真：** 依赖 Mininet 构建受控网络拓扑，注入可控的延迟 (`delay`)、抖动 (`jitter`) 与丢包 (`loss`)。
* **交叉验证：** 将本协议栈与标准 Linux 内核 TCP 协议栈（通过 `nc` 建立连接）进行双向大文件 (100KB) 打流测试。
* **一致性标准：** 在存在 5% 丢包及 50ms 延迟的恶劣网络中，双向传输后的大文件 MD5 哈希值严格保持 100% 一致。


# 测试方法

## AI-TCP ==> Linux
```bash
make clean && make all && make run
h2 nc -l -p 8080 > linux_recv.dat
h1 ./mini_tcp_send
diff linux_recv.dat test_send.dat
```

## Linux ==> AI-TCP
```bash
make clean && make all && make run
h2 nc -l -p 8080 < linux_send.dat
h1 ./mini_tcp_recv
diff linux_send.dat test_recv.dat
```