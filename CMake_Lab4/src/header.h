#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#pragma pack(push,1)
struct BufferHeader {
    int32_t capacity;
    int32_t head;
    int32_t tail;
    int32_t count;
};
#pragma pack(pop)

constexpr std::size_t RECORD_SIZE = 20;

inline std::string MakeSafeName(const std::string& file) {
    std::string s = file;
    for (char& c : s) if (c == '\\' || c == '/' || c == ':' || c == ' ') c = '_';
    return s;
}

// Functions for in-memory ring buffer (operate on mapped view pointer)
inline BufferHeader* GetHeader(void* view) {
    return reinterpret_cast<BufferHeader*>(view);
}
inline char* GetRecordsBase(void* view) {
    return reinterpret_cast<char*>(view) + sizeof(BufferHeader);
}

// Write message (assumes message length < RECORD_SIZE)
// Returns true on success, false on invalid (e.g., capacity==0)
inline bool RB_Write(void* view, const char* msg, std::size_t msglen) {
    if (!view) return false;
    BufferHeader* h = GetHeader(view);
    if (h->capacity <= 0) 
        return false;
    char* base = GetRecordsBase(view);
    char buf[RECORD_SIZE];
    std::memset(buf, 0, RECORD_SIZE);
    std::memcpy(buf, msg, (msglen < RECORD_SIZE) ? msglen : (RECORD_SIZE - 1));
    std::memcpy(base + h->tail * RECORD_SIZE, buf, RECORD_SIZE);
    h->tail = (h->tail + 1) % h->capacity;
    h->count += 1;
    return true;
}

// Read message into out buffer (must be at least RECORD_SIZE+1). Returns true on success.
inline bool RB_Read(void* view, char* out, std::size_t outSize) {
    if (!view || !out || outSize < RECORD_SIZE + 1) 
        return false;
    BufferHeader* h = GetHeader(view);
    if (h->capacity <= 0) 
        return false;
    char* base = GetRecordsBase(view);
    std::memcpy(out, base + h->head * RECORD_SIZE, RECORD_SIZE);
    out[RECORD_SIZE] = '\0';
    h->head = (h->head + 1) % h->capacity;
    h->count -= 1;
    return true;
}
