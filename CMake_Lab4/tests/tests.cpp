#include "header.h"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>

// Simple in-memory "view" buffer for testing header functions
struct MemFile {
    std::vector<char> buf;
    MemFile(int capacity) {
        buf.resize(sizeof(BufferHeader) + capacity * RECORD_SIZE);
        BufferHeader* h = reinterpret_cast<BufferHeader*>(buf.data());
        h->capacity = capacity;
        h->head = h->tail = h->count = 0;
    }
    void* view() { return buf.data(); }
};

TEST(RingBufferTest, WriteReadSingle) {
    MemFile mf(3);
    const char* msg = "hello";
    EXPECT_TRUE(RB_Write(mf.view(), msg, strlen(msg)));
    char out[RECORD_SIZE + 1];
    EXPECT_TRUE(RB_Read(mf.view(), out, RECORD_SIZE + 1));
    EXPECT_STREQ(out, "hello");
}

TEST(RingBufferTest, FIFOOrder) {
    MemFile mf(3);
    RB_Write(mf.view(), "A", 1);
    RB_Write(mf.view(), "B", 1);
    RB_Write(mf.view(), "C", 1);
    char out[RECORD_SIZE + 1];
    RB_Read(mf.view(), out, RECORD_SIZE + 1); EXPECT_STREQ(out, "A");
    RB_Read(mf.view(), out, RECORD_SIZE + 1); EXPECT_STREQ(out, "B");
    RB_Read(mf.view(), out, RECORD_SIZE + 1); EXPECT_STREQ(out, "C");
}

TEST(RingBufferTest, WrapAround) {
    MemFile mf(2);
    RB_Write(mf.view(), "X", 1);
    RB_Write(mf.view(), "Y", 1);
    char out[RECORD_SIZE + 1];
    RB_Read(mf.view(), out, RECORD_SIZE + 1); EXPECT_STREQ(out, "X");
    RB_Write(mf.view(), "Z", 1);
    RB_Read(mf.view(), out, RECORD_SIZE + 1); EXPECT_STREQ(out, "Y");
    RB_Read(mf.view(), out, RECORD_SIZE + 1); EXPECT_STREQ(out, "Z");
}
