CC = gcc
CFLAGS = -Wall -g -I./include
TARGET = mini_tcp
SRC_DIRS = . core adapter utils
SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
OBJS = $(SRCS:.c=.o)

all: $(TARGET)
	dd if=/dev/urandom of=test_send.dat bs=1M count=1
	dd if=/dev/urandom of=linux_send.dat bs=1M count=1

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ==========================================
# 单元测试 (Unit Tests)
# ==========================================

# 显式指定需要编译的测试文件和依赖文件
TEST_RB_SRCS = tests/test_ringbuf.c utils/ringbuf.c

# 定义 test 伪目标，编译并自动运行测试
test:
	@echo "--- Building Ring Buffer Tests ---"
	$(CC) $(CFLAGS) -o test_rb $(TEST_RB_SRCS)
	@echo "--- Running Tests ---"
	./test_rb

# ... (保留之前的 CC, CFLAGS, TARGET, OBJS 等定义) ...

# ==========================================
# 增强版清理功能
# ==========================================

clean:
	@echo "--- Cleaning build files ---"
	rm -f $(OBJS) $(TARGET) test_rb *.o

	@echo "--- Cleaning transitted files ---"
	rm -f *.dat *.log
	
	@echo "--- Cleaning Mininet leftovers ---"
# 清理 Mininet 拓扑残留
	sudo mn -c > /dev/null 2>&1
	
	@echo "--- Killing zombie controllers ---"
# 强行终止占用 6653 端口的进程（如果存在）
	-sudo fuser -k 6653/tcp > /dev/null 2>&1
	
	@echo "--- Resetting Firewall Rules ---"
# 清理 iptables 中关于 12345 端口的丢弃规则
# 使用 -D 命令。为了防止规则不存在时报错，末尾加了 || true
	-sudo iptables -D OUTPUT -p tcp --sport 12345 --tcp-flags RST RST -j DROP > /dev/null 2>&1 || true

run:
	sudo python3 topo.py
.PHONY: all clean test