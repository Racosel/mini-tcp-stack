#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ringbuf.h"

// 简单的测试宏，如果不通过会直接报错退出
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("[FAIL] %s\n", msg); \
            assert(cond); \
        } \
    } while(0)

int main() {
    printf("=== Starting Ring Buffer Unit Tests ===\n");

    // 1. 初始化测试 (容量 10 字节)
    struct ringbuf *rb = rb_new(10);
    TEST_ASSERT(rb_free_space(rb) == 10, "Init free space error");
    TEST_ASSERT(rb_used_space(rb) == 0, "Init used space error");

    // 2. 基础写入测试
    uint8_t data1[] = "Hello";
    int w = rb_write(rb, data1, 5);
    TEST_ASSERT(w == 5, "Basic write length error");
    TEST_ASSERT(rb_used_space(rb) == 5, "Used space after write error");

    // 3. 基础读取测试
    uint8_t out[15] = {0};
    int r = rb_read(rb, out, 3); // 读走 "Hel"
    TEST_ASSERT(r == 3, "Basic read length error");
    TEST_ASSERT(memcmp(out, "Hel", 3) == 0, "Basic read content error");
    TEST_ASSERT(rb_used_space(rb) == 2, "Used space after read error"); // 剩下 "lo"

    // 4. 关键：指针回绕测试 (Wrap-around)
    // 当前剩余空间 8，写入 8 字节 "World123"，这会越过数组物理末尾回到头部
    uint8_t data2[] = "World123";
    w = rb_write(rb, data2, 8); 
    TEST_ASSERT(w == 8, "Wrap-around write length error");
    TEST_ASSERT(rb_used_space(rb) == 10, "Buffer should be full");

    // 5. 溢出保护测试
    w = rb_write(rb, (uint8_t *)"!", 1);
    TEST_ASSERT(w == 0, "Overflow protection failed (should return 0)");

    // 6. 关键：偏移读取测试 (Peek offset - 用于重传)
    // 此时缓冲区内逻辑上的数据是: "loWorld123"
    memset(out, 0, sizeof(out));
    r = rb_peek_offset(rb, 2, out, 5); // 偏移 2 字节开始，读 5 字节
    TEST_ASSERT(r == 5, "Peek offset length error");
    TEST_ASSERT(memcmp(out, "World", 5) == 0, "Peek offset content error");
    // Peek 不应该改变实际数据量
    TEST_ASSERT(rb_used_space(rb) == 10, "Peek should not consume data");

    // 7. 回绕读取测试
    memset(out, 0, sizeof(out));
    r = rb_read(rb, out, 10); // 一次性全部读完
    TEST_ASSERT(r == 10, "Wrap-around read length error");
    TEST_ASSERT(memcmp(out, "loWorld123", 10) == 0, "Wrap-around read content error");
    TEST_ASSERT(rb_used_space(rb) == 0, "Buffer should be empty");

    rb_free(rb);
    printf("=== All Ring Buffer Tests Passed! ===\n");
    return 0;
}