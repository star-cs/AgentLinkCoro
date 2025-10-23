#pragma once

#include <memory>
#include <string>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <stdexcept>
#include "base/endian.h"
#include "base/util.h"
#include "base/macro.h"

namespace base
{
static uint32_t EncodeZigzag32(const int32_t &v)
{
    if (v < 0) {
        return ((uint32_t)(-v)) * 2 - 1;
    } else {
        return v * 2;
    }
}

static uint64_t EncodeZigzag64(const int64_t &v)
{
    if (v < 0) {
        return ((uint64_t)(-v)) * 2 - 1;
    } else {
        return v * 2;
    }
}

static int32_t DecodeZigzag32(const uint32_t &v)
{
    return (v >> 1) ^ -(v & 1);
}

static int64_t DecodeZigzag64(const uint64_t &v)
{
    return (v >> 1) ^ -(v & 1);
}

/**
 * @brief 二进制数组,提供基础类型的序列化,反序列化功能
 */
class ByteArray
{
public:
    typedef std::shared_ptr<ByteArray> ptr;

    /**
     * @brief ByteArray的存储节点
     */
    struct Node {
        /**
         * @brief 构造指定大小的内存块
         * @param[in] s 内存块字节数
         */
        Node(size_t s) : ptr(new char[s]), next(nullptr), size(s) {}

        /**
         * 无参构造函数
         */
        Node() : ptr(nullptr), next(nullptr), size(0) {}

        /**
         * 析构函数,释放内存
         */
        ~Node() {}

        /**
         * 释放内存
         */
        void free()
        {
            if (_LIKELY(ptr)) {
                delete[] ptr;
                ptr = nullptr;
            }
        }

        /// 内存块地址指针
        char *ptr;
        /// 下一个内存块地址
        Node *next;
        /// 内存块大小
        size_t size;
    };

    /**
     * @brief 使用指定长度的内存块构造ByteArray
     * @param[in] base_size 内存块大小
     */
    ByteArray(size_t base_size = 4096)
        : m_baseSize(base_size), m_position(0), m_capacity(base_size), m_size(0),
          m_endian(_BIG_ENDIAN), m_owner(true), m_root(new Node(base_size)), m_cur(m_root)
    {
    }

    /**
     * @brief 操作外部已有内存,如果owner为false,不支持写操作
     * @param[in] data 内存指针
     * @param[in] size 数据大小
     * @param[in] owner 是否管理该内存
     */
    ByteArray(void *data, size_t size, bool owner = false)
        : m_baseSize(size), m_position(0), m_capacity(size), m_size(size), m_endian(_BIG_ENDIAN),
          m_owner(owner)
    {
        m_root = new Node();
        m_root->ptr = (char *)data;
        m_root->size = size;
        m_cur = m_root;
    }

    /**
     * @brief 析构函数
     */
    ~ByteArray()
    {
        Node *tmp = m_root;
        while (tmp) {
            m_cur = tmp;
            tmp = tmp->next;
            if (m_owner) {
                m_cur->free();
            }
            delete m_cur;
        }
        m_root = nullptr;
        m_cur = nullptr;
    }

    // 在构造函数后添加移动构造函数和移动赋值运算符
    /**
     * @brief 移动构造函数
     */
    ByteArray(ByteArray &&other) noexcept
        : m_baseSize(other.m_baseSize), m_position(other.m_position), m_capacity(other.m_capacity),
          m_size(other.m_size), m_endian(other.m_endian), m_owner(other.m_owner),
          m_root(other.m_root), m_cur(other.m_cur)
    {
        other.m_root = nullptr;
        other.m_cur = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
        other.m_position = 0;
    }

    /**
     * @brief 移动赋值运算符
     */
    ByteArray &operator=(ByteArray &&other) noexcept
    {
        if (this != &other) {
            // 清理当前资源
            Node *tmp = m_root;
            while (tmp) {
                Node *next = tmp->next;
                if (m_owner) {
                    tmp->free();
                }
                delete tmp;
                tmp = next;
            }

            // 转移资源
            m_baseSize = other.m_baseSize;
            m_position = other.m_position;
            m_capacity = other.m_capacity;
            m_size = other.m_size;
            m_endian = other.m_endian;
            m_owner = other.m_owner;
            m_root = other.m_root;
            m_cur = other.m_cur;

            // 重置原对象
            other.m_root = nullptr;
            other.m_cur = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
            other.m_position = 0;
        }
        return *this;
    }

    // 删除拷贝构造函数和拷贝赋值运算符，防止浅拷贝问题
    ByteArray(const ByteArray &) = delete;
    ByteArray &operator=(const ByteArray &) = delete;

    /**
     * @brief 写入固定长度int8_t类型的数据
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeFint8(int8_t value) { write(&value, sizeof(value)); }
    /**
     * @brief 写入固定长度uint8_t类型的数据
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeFuint8(uint8_t value) { write(&value, sizeof(value)); }
    /**
     * @brief 写入固定长度int16_t类型的数据(大端/小端)
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeFint16(int16_t value)
    {
        if (m_endian != _BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }
    /**
     * @brief 写入固定长度uint16_t类型的数据(大端/小端)
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeFuint16(uint16_t value)
    {
        if (m_endian != _BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    /**
     * @brief 写入固定长度int32_t类型的数据(大端/小端)
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeFint32(int32_t value)
    {
        if (m_endian != _BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    /**
     * @brief 写入固定长度uint32_t类型的数据(大端/小端)
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeFuint32(uint32_t value)
    {
        if (m_endian != _BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    /**
     * @brief 写入固定长度int64_t类型的数据(大端/小端)
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeFint64(int64_t value)
    {
        if (m_endian != _BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    /**
     * @brief 写入固定长度uint64_t类型的数据(大端/小端)
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeFuint64(uint64_t value)
    {
        if (m_endian != _BYTE_ORDER) {
            value = byteswap(value);
        }
        write(&value, sizeof(value));
    }

    /**
     * @brief 写入有符号Varint32类型的数据
     * @post m_position += 实际占用内存(1 ~ 5)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeInt32(int32_t value) { writeUint32(EncodeZigzag32(value)); }
    /**
     * @brief 写入无符号Varint32类型的数据
     * @post m_position += 实际占用内存(1 ~ 5)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeUint32(uint32_t value)
    {
        uint8_t tmp[5];
        uint8_t i = 0;
        while (value >= 0x80) {
            tmp[i++] = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        tmp[i++] = value;
        write(tmp, i);
    }

    /**
     * @brief 写入有符号Varint64类型的数据
     * @post m_position += 实际占用内存(1 ~ 10)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeInt64(int64_t value) { writeUint64(EncodeZigzag64(value)); }

    /**
     * @brief 写入无符号Varint64类型的数据
     * @post m_position += 实际占用内存(1 ~ 10)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeUint64(uint64_t value)
    {
        uint8_t tmp[10];
        uint8_t i = 0;
        while (value >= 0x80) {
            tmp[i++] = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        tmp[i++] = value;
        write(tmp, i);
    }

    /**
     * @brief 写入float类型的数据
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeFloat(float value)
    {
        uint32_t v;
        memcpy(&v, &value, sizeof(value));
        writeFuint32(v);
    }

    /**
     * @brief 写入double类型的数据
     * @post m_position += sizeof(value)
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeDouble(double value)
    {
        uint64_t v;
        memcpy(&v, &value, sizeof(value));
        writeFuint64(v);
    }

    /**
     * @brief 写入std::string类型的数据,用uint16_t作为长度类型
     * @post m_position += 2 + value.size()
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeStringF16(const std::string &value)
    {
        writeFuint16(value.size());
        write(value.c_str(), value.size());
    }

    /**
     * @brief 写入std::string类型的数据,用uint32_t作为长度类型
     * @post m_position += 4 + value.size()
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeStringF32(const std::string &value)
    {
        writeFuint32(value.size());
        write(value.c_str(), value.size());
    }

    /**
     * @brief 写入std::string类型的数据,用uint64_t作为长度类型
     * @post m_position += 8 + value.size()
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeStringF64(const std::string &value)
    {
        writeFuint64(value.size());
        write(value.c_str(), value.size());
    }

    /**
     * @brief 写入std::string类型的数据,用无符号Varint64作为长度类型
     * @post m_position += Varint64长度 + value.size()
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeStringVint(const std::string &value)
    {
        writeUint64(value.size());
        write(value.c_str(), value.size());
    }

    /**
     * @brief 写入std::string类型的数据,无长度
     * @post m_position += value.size()
     *       如果m_position > m_size 则 m_size = m_position
     */
    void writeStringWithoutLength(const std::string &value) { write(value.c_str(), value.size()); }

    /**
     * @brief 读取int8_t类型的数据
     * @pre getReadSize() >= sizeof(int8_t)
     * @post m_position += sizeof(int8_t);
     * @exception 如果getReadSize() < sizeof(int8_t) 抛出 std::out_of_range
     */
    int8_t readFint8()
    {
        int8_t v;
        read(&v, sizeof(v));
        return v;
    }

    /**
     * @brief 读取uint8_t类型的数据
     * @pre getReadSize() >= sizeof(uint8_t)
     * @post m_position += sizeof(uint8_t);
     * @exception 如果getReadSize() < sizeof(uint8_t) 抛出 std::out_of_range
     */
    uint8_t readFuint8()
    {
        uint8_t v;
        read(&v, sizeof(v));
        return v;
    }

#define XX(type)                                                                                   \
    type v;                                                                                        \
    read(&v, sizeof(v));                                                                           \
    if (m_endian == _BYTE_ORDER) {                                                                 \
        return v;                                                                                  \
    } else {                                                                                       \
        return byteswap(v);                                                                        \
    }
    /**
     * @brief 读取int16_t类型的数据
     * @pre getReadSize() >= sizeof(int16_t)
     * @post m_position += sizeof(int16_t);
     * @exception 如果getReadSize() < sizeof(int16_t) 抛出 std::out_of_range
     */
    int16_t readFint16() { XX(int16_t); }

    /**
     * @brief 读取uint16_t类型的数据
     * @pre getReadSize() >= sizeof(uint16_t)
     * @post m_position += sizeof(uint16_t);
     * @exception 如果getReadSize() < sizeof(uint16_t) 抛出 std::out_of_range
     */
    uint16_t readFuint16() { XX(uint16_t); }

    /**
     * @brief 读取int32_t类型的数据
     * @pre getReadSize() >= sizeof(int32_t)
     * @post m_position += sizeof(int32_t);
     * @exception 如果getReadSize() < sizeof(int32_t) 抛出 std::out_of_range
     */
    int32_t readFint32() { XX(int32_t); }

    /**
     * @brief 读取uint32_t类型的数据
     * @pre getReadSize() >= sizeof(uint32_t)
     * @post m_position += sizeof(uint32_t);
     * @exception 如果getReadSize() < sizeof(uint32_t) 抛出 std::out_of_range
     */
    uint32_t readFuint32() { XX(uint32_t); }

    /**
     * @brief 读取int64_t类型的数据
     * @pre getReadSize() >= sizeof(int64_t)
     * @post m_position += sizeof(int64_t);
     * @exception 如果getReadSize() < sizeof(int64_t) 抛出 std::out_of_range
     */
    int64_t readFint64() { XX(int64_t); }

    /**
     * @brief 读取uint64_t类型的数据
     * @pre getReadSize() >= sizeof(uint64_t)
     * @post m_position += sizeof(uint64_t);
     * @exception 如果getReadSize() < sizeof(uint64_t) 抛出 std::out_of_range
     */
    uint64_t readFuint64() { XX(uint64_t); }
#undef XX

    /**
     * @brief 读取有符号Varint32类型的数据
     * @pre getReadSize() >= 有符号Varint32实际占用内存
     * @post m_position += 有符号Varint32实际占用内存
     * @exception 如果getReadSize() < 有符号Varint32实际占用内存 抛出 std::out_of_range
     */
    int32_t readInt32() { return DecodeZigzag32(readUint32()); }

    /**
     * @brief 读取无符号Varint32类型的数据
     * @pre getReadSize() >= 无符号Varint32实际占用内存
     * @post m_position += 无符号Varint32实际占用内存
     * @exception 如果getReadSize() < 无符号Varint32实际占用内存 抛出 std::out_of_range
     */
    uint32_t readUint32()
    {
        uint32_t result = 0;
        for (int i = 0; i < 32; i += 7) {
            uint8_t b = readFuint8();
            if (b < 0x80) {
                result |= ((uint32_t)b) << i;
                break;
            } else {
                result |= (((uint32_t)(b & 0x7f)) << i);
            }
        }
        return result;
    }

    /**
     * @brief 读取有符号Varint64类型的数据
     * @pre getReadSize() >= 有符号Varint64实际占用内存
     * @post m_position += 有符号Varint64实际占用内存
     * @exception 如果getReadSize() < 有符号Varint64实际占用内存 抛出 std::out_of_range
     */
    int64_t readInt64() { return DecodeZigzag64(readUint64()); }

    /**
     * @brief 读取无符号Varint64类型的数据
     * @pre getReadSize() >= 无符号Varint64实际占用内存
     * @post m_position += 无符号Varint64实际占用内存
     * @exception 如果getReadSize() < 无符号Varint64实际占用内存 抛出 std::out_of_range
     */
    uint64_t readUint64()
    {
        uint64_t result = 0;
        for (int i = 0; i < 64; i += 7) {
            uint8_t b = readFuint8();
            if (b < 0x80) {
                result |= ((uint64_t)b) << i;
                break;
            } else {
                result |= (((uint64_t)(b & 0x7f)) << i);
            }
        }
        return result;
    }

    /**
     * @brief 读取float类型的数据
     * @pre getReadSize() >= sizeof(float)
     * @post m_position += sizeof(float);
     * @exception 如果getReadSize() < sizeof(float) 抛出 std::out_of_range
     */
    float readFloat()
    {
        uint32_t v = readFuint32();
        float value;
        memcpy(&value, &v, sizeof(v));
        return value;
    }

    /**
     * @brief 读取double类型的数据
     * @pre getReadSize() >= sizeof(double)
     * @post m_position += sizeof(double);
     * @exception 如果getReadSize() < sizeof(double) 抛出 std::out_of_range
     */
    double readDouble()
    {
        uint64_t v = readFuint64();
        double value;
        memcpy(&value, &v, sizeof(v));
        return value;
    }

    /**
     * @brief 读取std::string类型的数据,用uint16_t作为长度
     * @pre getReadSize() >= sizeof(uint16_t) + size
     * @post m_position += sizeof(uint16_t) + size;
     * @exception 如果getReadSize() < sizeof(uint16_t) + size 抛出 std::out_of_range
     */
    std::string readStringF16()
    {
        uint16_t len = readFuint16();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    /**
     * @brief 读取std::string类型的数据,用uint32_t作为长度
     * @pre getReadSize() >= sizeof(uint32_t) + size
     * @post m_position += sizeof(uint32_t) + size;
     * @exception 如果getReadSize() < sizeof(uint32_t) + size 抛出 std::out_of_range
     */
    std::string readStringF32()
    {
        uint32_t len = readFuint32();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    /**
     * @brief 读取std::string类型的数据,用uint64_t作为长度
     * @pre getReadSize() >= sizeof(uint64_t) + size
     * @post m_position += sizeof(uint64_t) + size;
     * @exception 如果getReadSize() < sizeof(uint64_t) + size 抛出 std::out_of_range
     */
    std::string readStringF64()
    {
        uint64_t len = readFuint64();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    /**
     * @brief 读取std::string类型的数据,用无符号Varint64作为长度
     * @pre getReadSize() >= 无符号Varint64实际大小 + size
     * @post m_position += 无符号Varint64实际大小 + size;
     * @exception 如果getReadSize() < 无符号Varint64实际大小 + size 抛出 std::out_of_range
     */
    std::string readStringVint()
    {
        uint64_t len = readUint64();
        std::string buff;
        buff.resize(len);
        read(&buff[0], len);
        return buff;
    }

    /**
     * @brief 清空ByteArray
     * @post m_position = 0, m_size = 0
     */
    void clear()
    {
        m_position = m_size = 0;
        m_capacity = m_baseSize;
        Node *tmp = m_root->next;
        while (tmp) {
            m_cur = tmp;
            tmp = tmp->next;
            if (m_owner) {
                m_cur->free();
            }
            delete m_cur;
        }
        m_cur = m_root;
        m_root->next = NULL;
    }

    /**
     * @brief 写入size长度的数据
     * @param[in] buf 内存缓存指针
     * @param[in] size 数据大小
     * @post m_position += size, 如果m_position > m_size 则 m_size = m_position
     */
    void write(const void *buf, size_t size)
    {
        if (_UNLIKELY(size == 0)) {
            return;
        }
        if (_UNLIKELY(buf == nullptr)) {
            throw std::invalid_argument("write buffer is null");
        }

        addCapacity(size);

        size_t npos = m_position % m_baseSize;
        size_t ncap = m_cur->size - npos;
        size_t bpos = 0;

        while (size > 0) {
            if (ncap >= size) {
                memcpy(m_cur->ptr + npos, (const char *)buf + bpos, size);
                if (m_cur->size == (npos + size)) {
                    m_cur = m_cur->next;
                }
                m_position += size;
                bpos += size;
                size = 0;
            } else {
                memcpy(m_cur->ptr + npos, (const char *)buf + bpos, ncap);
                m_position += ncap;
                bpos += ncap;
                size -= ncap;
                m_cur = m_cur->next;
                ncap = m_cur->size;
                npos = 0;
            }
        }

        if (m_position > m_size) {
            m_size = m_position;
        }
    }

    /**
     * @brief 读取size长度的数据
     * @param[out] buf 内存缓存指针
     * @param[in] size 数据大小
     * @post m_position += size, 如果m_position > m_size 则 m_size = m_position
     * @exception 如果getReadSize() < size 则抛出 std::out_of_range
     */
    void read(void *buf, size_t size)
    {
        if (size > getReadSize()) {
            throw std::out_of_range("** not enough len size=" + std::to_string(size)
                                    + " read_size=" + std::to_string(getReadSize()));
        }
        if (_UNLIKELY(buf == nullptr && size > 0)) {
            throw std::invalid_argument("buf is null");
        }

        size_t npos = m_position % m_baseSize;
        size_t ncap = m_cur->size - npos;
        size_t bpos = 0;
        while (size > 0) {
            if (ncap >= size) {
                memcpy((char *)buf + bpos, m_cur->ptr + npos, size);
                if (m_cur->size == (npos + size)) {
                    m_cur = m_cur->next;
                }
                m_position += size;
                bpos += size;
                size = 0;
            } else {
                memcpy((char *)buf + bpos, m_cur->ptr + npos, ncap);
                m_position += ncap;
                bpos += ncap;
                size -= ncap;
                m_cur = m_cur->next;
                ncap = m_cur->size;
                npos = 0;
            }
        }
    }

    /**
     * @brief 读取size长度的数据
     * @param[out] buf 内存缓存指针
     * @param[in] size 数据大小
     * @param[in] position 读取开始位置
     * @exception 如果 (m_size - position) < size 则抛出 std::out_of_range
     */
    void read(void *buf, size_t size, size_t position) const
    {
        if (size > (m_size - position)) {
            throw std::out_of_range("== not enough len\n" + base::BacktraceToString());
        }

        size_t npos = position % m_baseSize;
        size_t ncap = m_cur->size - npos;
        size_t bpos = 0;
        Node *cur = m_cur;
        while (size > 0) {
            if (ncap >= size) {
                memcpy((char *)buf + bpos, cur->ptr + npos, size);
                if (cur->size == (npos + size)) {
                    cur = cur->next;
                }
                position += size;
                bpos += size;
                size = 0;
            } else {
                memcpy((char *)buf + bpos, cur->ptr + npos, ncap);
                position += ncap;
                bpos += ncap;
                size -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
        }
    }

    /**
     * @brief 返回ByteArray当前位置
     */
    size_t getPosition() const { return m_position; }

    /**
     * @brief 设置ByteArray当前位置
     * @post 如果m_position > m_size 则 m_size = m_position
     * @exception 如果m_position > m_capacity 则抛出 std::out_of_range
     */
    void setPosition(size_t v)
    {
        if (_UNLIKELY(v > m_capacity)) {
            throw std::out_of_range("set_position out of range");
        }
        m_position = v;
        if (m_position > m_size) {
            m_size = m_position;
        }
        m_cur = m_root;
        while (v > m_cur->size) {
            v -= m_cur->size;
            m_cur = m_cur->next;
        }
        if (v == m_cur->size) {
            m_cur = m_cur->next;
        }
    }

    /**
     * @brief 把ByteArray的数据写入到文件中
     * @param[in] name 文件名
     */
    bool writeToFile(const std::string &name, bool with_md5 = false) const
    {
        std::ofstream ofs;
        ofs.open(name, std::ios::trunc | std::ios::binary);
        if (_UNLIKELY(!ofs)) {
            return false;
        }

        int64_t read_size = getReadSize();
        int64_t pos = m_position;
        Node *cur = m_cur;

        while (read_size > 0) {
            int diff = pos % m_baseSize;
            int64_t len = (read_size > (int64_t)m_baseSize ? m_baseSize : read_size) - diff;
            ofs.write(cur->ptr + diff, len);
            cur = cur->next;
            pos += len;
            read_size -= len;
        }

        if (with_md5) {
            std::ofstream ofs_md5(name + ".md5");
            ofs_md5 << getMd5();
        }

        return true;
    }

    /**
     * @brief 从文件中读取数据
     * @param[in] name 文件名
     */
    bool readFromFile(const std::string &name)
    {
        std::ifstream ifs;
        ifs.open(name, std::ios::binary);
        if (_UNLIKELY(!ifs)) {
            return false;
        }

        std::shared_ptr<char> buff(new char[m_baseSize], [](char *ptr) { delete[] ptr; });
        while (!ifs.eof()) {
            ifs.read(buff.get(), m_baseSize);
            write(buff.get(), ifs.gcount());
        }
        return true;
    }

    /**
     * @brief 返回内存块的大小
     */
    size_t getBaseSize() const { return m_baseSize; }

    /**
     * @brief 返回可读取数据大小
     */
    size_t getReadSize() const { return m_size - m_position; }

    /**
     * @brief 是否是小端
     */
    bool isLittleEndian() const { return m_endian == _LITTLE_ENDIAN; }

    /**
     * @brief 设置是否为小端
     */
    void setIsLittleEndian(bool val)
    {
        if (val) {
            m_endian = _LITTLE_ENDIAN;
        } else {
            m_endian = _BIG_ENDIAN;
        }
    }

    /**
     * @brief 将ByteArray里面的数据[m_position, m_size)转成std::string
     */
    std::string toString() const
    {
        std::string str;
        str.resize(getReadSize());
        if (_UNLIKELY(str.empty())) {
            return str;
        }
        read(&str[0], str.size(), m_position);
        return str;
    }

    /**
     * @brief 将ByteArray里面的数据[m_position, m_size)转成16进制的std::string(格式:FF FF FF)
     */
    std::string toHexString() const
    {
        std::string str = toString();
        std::stringstream ss;

        for (size_t i = 0; i < str.size(); ++i) {
            if (i > 0 && i % 32 == 0) {
                ss << std::endl;
            }
            ss << std::setw(2) << std::setfill('0') << std::hex << (int)(uint8_t)str[i] << " ";
        }

        return ss.str();
    }

    /**
     * @brief 获取可读取的缓存,保存成iovec数组
     * @param[out] buffers 保存可读取数据的iovec数组
     * @param[in] len 读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
     * @return 返回实际数据的长度
     */
    uint64_t getReadBuffers(std::vector<iovec> &buffers, uint64_t len = ~0ull) const
    {
        if (_UNLIKELY(len == 0)) {
            return 0;
        }
        len = len > getReadSize() ? getReadSize() : len;
        uint64_t size = len;

        size_t npos = m_position % m_baseSize;
        size_t ncap = m_cur->size - npos;
        struct iovec iov;
        Node *cur = m_cur;

        while (len > 0) {
            if (ncap >= len) {
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = len;
                len = 0;
            } else {
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = ncap;
                len -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
            buffers.push_back(iov);
        }
        return size;
    }

    /**
     * @brief 获取可读取的缓存,保存成iovec数组,从position位置开始
     * @param[out] buffers 保存可读取数据的iovec数组
     * @param[in] len 读取数据的长度,如果len > getReadSize() 则 len = getReadSize()
     * @param[in] position 读取数据的位置
     * @return 返回实际数据的长度
     */
    uint64_t getReadBuffers(std::vector<iovec> &buffers, uint64_t len, uint64_t position) const
    {
        if (_UNLIKELY(len == 0)) {
            return 0;
        }
        len = len > getReadSize() ? getReadSize() : len;
        uint64_t size = len;

        size_t npos = position % m_baseSize;
        size_t count = position / m_baseSize;
        Node *cur = m_root;
        while (count > 0) {
            cur = cur->next;
            --count;
        }
        // 检查cur是否为空
        if (_UNLIKELY(cur == nullptr)) {
            return 0;
        }

        size_t ncap = cur->size - npos;
        struct iovec iov;
        while (len > 0) {
            if (ncap >= len) {
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = len;
                len = 0;
            } else {
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = ncap;
                len -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
            buffers.push_back(iov);
        }
        return size;
    }

    /**
     * @brief 获取可写入的缓存,保存成iovec数组
     * @param[out] buffers 保存可写入的内存的iovec数组
     * @param[in] len 写入的长度
     * @return 返回实际的长度
     * @post 如果(m_position + len) > m_capacity 则 m_capacity扩容N个节点以容纳len长度
     */
    uint64_t getWriteBuffers(std::vector<iovec> &buffers, uint64_t len)
    {
        if (_UNLIKELY(len == 0)) {
            return 0;
        }
        addCapacity(len);
        uint64_t size = len;

        size_t npos = m_position % m_baseSize;
        size_t ncap = m_cur->size - npos;
        struct iovec iov;
        Node *cur = m_cur;
        while (len > 0) {
            if (ncap >= len) {
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = len;
                len = 0;
            } else {
                iov.iov_base = cur->ptr + npos;
                iov.iov_len = ncap;

                len -= ncap;
                cur = cur->next;
                ncap = cur->size;
                npos = 0;
            }
            buffers.push_back(iov);
        }
        return size;
    }
    /**
     * @brief 返回数据的长度
     */
    size_t getSize() const { return m_size; }

private:
    /**
     * @brief 扩容ByteArray,使其可以容纳size个数据(如果原本可以可以容纳,则不扩容)
     */
    void addCapacity(size_t size)
    {
        if (_UNLIKELY(size == 0)) {
            return;
        }

        size_t old_cap = getCapacity();
        if (_UNLIKELY(old_cap >= size)) {
            return;
        }
        // 添加溢出检查
        if (_UNLIKELY(size > SIZE_MAX - old_cap)) {
            throw std::overflow_error("capacity overflow");
        }

        size = size - old_cap;
        size_t count = (size_t)ceil(1.0 * size / m_baseSize);
        Node *tmp = m_root;
        while (tmp->next) {
            tmp = tmp->next;
        }

        Node *first = NULL;
        for (size_t i = 0; i < count; ++i) {
            tmp->next = new Node(m_baseSize);
            if (first == NULL) {
                first = tmp->next;
            }
            tmp = tmp->next;
            if (m_capacity > SIZE_MAX - m_baseSize) {
                throw std::overflow_error("total capacity overflow");
            }
            m_capacity += m_baseSize;
        }

        if (old_cap == 0) {
            m_cur = first;
        }
    }

    /**
     * @brief 获取当前的可写入容量
     */
    size_t getCapacity() const { return m_capacity - m_position; }

    std::string getMd5() const
    {
        std::vector<iovec> buffers;
        getReadBuffers(buffers, -1, 0);
        return base::md5sum(buffers);
    }

private:
    /// 内存块的大小
    size_t m_baseSize;
    /// 当前操作位置
    size_t m_position;
    /// 当前的总容量
    size_t m_capacity;
    /// 当前数据的大小
    size_t m_size;
    /// 字节序,默认大端
    int8_t m_endian;
    /// 是否拥有数据的管理权限
    bool m_owner;
    /// 第一个内存块指针
    Node *m_root;
    /// 当前操作的内存块指针
    Node *m_cur;
};

} // namespace base
