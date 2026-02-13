CC = gcc
CFLAGS = -Wall -g -I./include
TARGET = mini_tcp
SRC_DIRS = . core adapter utils
SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

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

# 更新 clean 目标，把生成的测试二进制文件也删掉
clean:
	rm -f $(OBJS) $(TARGET) test_rb