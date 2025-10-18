#pragma once


#ifndef REDIRECTOR_H
#define REDIRECTOR_H

#include <fstream>
#include <sstream>
#include <iostream>
#include <streambuf>


#ifdef DATATYPES_USE_INDEPENDENT_NAMESP
#define DATATYPES_NAMESPACE_BEGIN namespace datatypes {
#define DATATYPES_NAMESPACE_END }
#else
#define DATATYPES_NAMESPACE_BEGIN
#define DATATYPES_NAMESPACE_END
#endif

DATATYPES_NAMESPACE_BEGIN
/**
 * 重定向缓冲区。
 * 将输出重定向到两个缓冲区。
 */
class TeeBuffer : public std::streambuf {

private:
    std::streambuf* orig_buffer;
    std::streambuf* copy_buffer;

public:
    /**
     * 构造函数。
     * @param orig_buf 原始缓冲区
     * @param copy_buf 复制到的缓冲区
     */
    TeeBuffer(std::streambuf* orig, std::streambuf* copy) :
        orig_buffer(orig), copy_buffer(copy) {};

protected:
    /**
     * 重写输出缓冲区的overflow方法。
     */
    virtual int overflow(int c) override {
        if (c != EOF) {
            orig_buffer->sputc(c);
            copy_buffer->sputc(c);
        }
        return c;
    };
    /**
     * 重写输出缓冲区的sync方法。
     */
    virtual int sync() override {
        // 同步两个流
        int r1 = orig_buffer->pubsync();
        int r2 = copy_buffer->pubsync();
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    };
};

/**
 * 重定向输出流。
 */
class Redirector : public std::ostream {

private:
    std::ostream& orig_stream;     // 原始输出流
    std::stringstream copy_stream; // 副本流（用于存储复制的数据）
    TeeBuffer tee_buffer;              // 自定义缓冲区

public:
    Redirector(std::ostream& stream) :
        std::ostream(&tee_buffer),
        orig_stream(stream),
        copy_stream(),
        tee_buffer(stream.rdbuf(), copy_stream.rdbuf()) {
        rdbuf(&tee_buffer);
    }

    /**
     * 获取复制的内容。
     */
    std::string get_copy_content() { return copy_stream.str(); }

    /**
     * 清空缓冲区。
     */
    void clear() {
        copy_stream.str("");
        copy_stream.clear();
    }

    /**
     * 获取原始流。
     */
    std::ostream& get_orig_stream() { return orig_stream; }

    /**
     * 获取复制流的引用。
     */
    std::stringstream& get_copy_stream() { return copy_stream; }
};


DATATYPES_NAMESPACE_END

#endif // REDIRECTOR_H