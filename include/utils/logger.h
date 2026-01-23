#pragma once


#ifndef LOGGER_H
#define LOGGER_H

#include <map>
#include <any>
#include <mutex>
#include <vector>
#include <thread>
#include <string>
#include <variant>
#include <fstream>
#include <stdint.h>
#include <iostream>
#include <filesystem>
#include <functional>
#include <queue>
#include <atomic>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/color.h>
#include "time_utils.h"
#include "string_utils.h"

#ifdef _WIN32
#include <windows.h>
#include <shared_mutex>
#else
#include <unistd.h>
#include <shared_mutex>
#endif

#ifdef ERROR
#undef ERROR
#endif


 // 记录追踪日志。
#define LTRACE(source,message,...) \
    log_utils::Logger::log(log_utils::Logger::TRACE,source,__FILE__,__LINE__,log_utils::formatter(message,##__VA_ARGS__))

// 记录调试日志。
#define LDEBUG(source,message,...) \
    log_utils::Logger::log(log_utils::Logger::DEBUG,source,__FILE__,__LINE__,log_utils::formatter(message,##__VA_ARGS__))

// 记录信息级别日志。
#define LINFO(source,message,...) \
    log_utils::Logger::log(log_utils::Logger::INFO,source,__FILE__,__LINE__,log_utils::formatter(message,##__VA_ARGS__)) 

// 记录警告级别日志。
#define LWARN(source,message,...) \
    log_utils::Logger::log(log_utils::Logger::WARN,source,__FILE__,__LINE__,log_utils::formatter(message,##__VA_ARGS__)) 

// 记录错误级别日志。
#define LERROR(source,message,...) \
    log_utils::Logger::log(log_utils::Logger::ERROR,source,__FILE__,__LINE__,log_utils::formatter(message,##__VA_ARGS__)) 

// 记录致命错误级别日志。
#define LCRIT(source,message,...) \
    log_utils::Logger::log(log_utils::Logger::CRITICAL,source,__FILE__,__LINE__,log_utils::formatter(message,##__VA_ARGS__)) 

// 记录致命错误级别日志。
#define LFATAL(source,message,...) \
    log_utils::Logger::log(log_utils::Logger::FATAL,source,__FILE__,__LINE__,log_utils::formatter(message,##__VA_ARGS__)) 


namespace log_utils {

    // 可变参数格式化函数。
    // 这个函数不能放别的文件实现，因为模板在编译的时候就定好了
    template<typename ...Args>
    std::string formatter(const std::string& fmt, Args ...args) {
        if constexpr (sizeof...(args) == 0) {
            return fmt;
        } else {
            return fmt::sprintf(fmt, args...);
        }
    }

    class Logger {

    public:

        class LogLevelID {
            
        public:
            constexpr static int UNKNOWN = -2; // 未知
            constexpr static int TRACE = -1;
            constexpr static int DEBUG = 0;
            constexpr static int INFO = 1;
            constexpr static int WARN = 2;
            constexpr static int ERROR = 3;
            constexpr static int CRITICAL = 4;
            constexpr static int FATAL = 5;
            constexpr static int OFF = 6;
            constexpr static int FORCE = 7;
        };


        class LogLevel {

        public:
            int level_id;                               // 日志级别ID
            std::string level_name;                          // 日志级别名称
            std::string display_name;                        // 日志级别显示名称
            fmt::v11::text_style time_display_color = fmt::fg(fmt::color::white);    // 日志时间部分显示颜色
            fmt::v11::text_style level_display_color = fmt::fg(fmt::color::white);   // 日志级别部分显示颜色
            fmt::v11::text_style source_display_color = fmt::fg(fmt::color::white);  // 日志来源部分显示颜色
            fmt::v11::text_style message_display_color = fmt::fg(fmt::color::white); // 日志消息部分显示颜色


            bool operator==(const LogLevel& other) const {
                return level_id == other.level_id;
            }

            bool operator<(const LogLevel& other) const {
                return level_id < other.level_id;
            }

            bool operator>(const LogLevel& other) const {
                return level_id > other.level_id;
            }

            bool operator<=(const LogLevel& other) const {
                return level_id <= other.level_id;
            }

            bool operator>=(const LogLevel& other) const {
                return level_id >= other.level_id;
            }

            bool operator!=(const LogLevel& other) const {
                return level_id != other.level_id;
            }

            /**
             * 默认构造函数。
             */
            LogLevel() : level_id(0), 
                        level_name("UNKNOWN"), 
                        display_name("UNK"),
                        time_display_color(fmt::fg(fmt::color::white)),
                        level_display_color(fmt::fg(fmt::color::white)),
                        source_display_color(fmt::fg(fmt::color::white)),
                        message_display_color(fmt::fg(fmt::color::white)) {
            }

            /**
             * 构造函数。
             * @param level_id 日志级别ID
             * @param level_name 日志级别名称
             * @param display_name 日志级别显示名称
             * @param color 等级在控制台显示颜色
             */
            LogLevel(int level_id, std::string level_name, std::string display_name,
                fmt::v11::text_style time_display_color, fmt::v11::text_style level_display_color,
                fmt::v11::text_style source_display_color, fmt::v11::text_style message_display_color) {
                this->level_id = level_id;
                this->level_name = level_name;
                this->display_name = display_name;
                this->time_display_color = time_display_color;
                this->level_display_color = level_display_color;
                this->source_display_color = source_display_color;
                this->message_display_color = message_display_color;
            };


            /**
             * 构造函数，通过日志级别ID获取日志级别对象。
             * @param level_id 日志级别ID
             */
            LogLevel(int level_id) {
                LogLevel level = getLevelByID(level_id);
                this->level_id = level.level_id;
                this->level_name = level.level_name;
                this->display_name = level.display_name;
                this->time_display_color = level.time_display_color;
                this->level_display_color = level.level_display_color;
                this->source_display_color = level.source_display_color;
                this->message_display_color = level.message_display_color;
            }

            /**
             * 构造函数，通过日志级别名称获取日志级别对象。
             * @param level_name 日志级别名称
             */
            LogLevel(const std::string& level_name) {
                LogLevel level = getLevelByName(level_name);
                this->level_id = level.level_id;
                this->level_name = level.level_name;
                this->display_name = level.display_name;
                this->time_display_color = level.time_display_color;
                this->level_display_color = level.level_display_color;
                this->source_display_color = level.source_display_color;
                this->message_display_color = level.message_display_color;
            }


            /**
             * 根据日志级别ID来获取日志等级。
             * @param level_id 日志级别ID
             * @return 日志等级
             */
            static LogLevel getLevelByID(int level_id);

            /**
             * 根据日志级别名称来获取日志等级。
             * @param level_name 日志级别名称
             * @return 日志等级
             */
            static LogLevel getLevelByName(const std::string& level_name);

            /**
             * 根据日志级别显示名称来获取日志等级。
             * @param display_name 日志级别显示名称
             * @return 日志等级
             */
            static LogLevel getLevelByDisplayName(const std::string& display_name);

            /**
             * 根据一个字符串获得日志等级，优先匹配具体名称。
             * @param level_str 日志等级字符串
             * @return 日志等级
             */
            static LogLevel getLevelByString(const std::string& level_str);
        };

        using OutputType = std::variant<std::ostream*, FILE*, std::function<std::any(std::string)>*>;
        class LogHandler {
        public:
            std::string name;
            OutputType output;
            std::string formatter;
            std::ostream* ostream_p;
            FILE* file_p;
            const LogLevel* level;
            std::function<std::any(std::string)>* func_p;
            LogHandler(const std::string& name, OutputType output, const LogLevel& level, const std::string& format) {
                this->name = name;
                this->level = &level;
                this->output = output;
                this->formatter = format;
                std::visit(
                    [&](auto&& handler) -> auto {
                        // decltype(handler)是在推导handler的类型
                        // std::decay_t是用来把本来是variant的类型转成真实的类型
                        // 然后再用is_same_v来比较类型是否相同
                        using T = std::decay_t<decltype(handler)>;
                        if constexpr (std::is_same_v<T, FILE*>) {
                            if (handler) {
                                this->file_p = handler;
                                this->ostream_p = nullptr;
                                this->func_p = nullptr;
                            }
                        } else if constexpr (std::is_same_v<T, std::function<std::any(std::string)> *>) {
                            if (handler) {
                                this->func_p = handler;
                                this->file_p = nullptr;
                                this->ostream_p = nullptr;
                            }
                        } else if constexpr (std::is_same_v<T, std::ostream*>) {
                            if (handler) {
                                this->ostream_p = handler;
                                this->file_p = nullptr;
                                this->func_p = nullptr;
                            }
                        }
                    },
                    output
                );
            };
        };

        class LogHandlerFormatMapping {
        public:
            static const std::string LOG_TIME;                          // 日志记录时间，格式为 "1145-01-04 01:09:19.810"
            static const std::string LOG_TIMESTAMP;                     // 日志记录时间戳，浮点数，单位为秒
            static const std::string LOG_LEVEL_DISPLAY_NAME;  // 日志级别显示名称，字符串
            static const std::string LOG_LEVEL_FULL_NAME;       // 日志级别称，字符串
            static const std::string LOG_SOURCE;                          // 日志来源，字符串
            static const std::string LOG_MESSAGE;                       // 日志内容，字符串  
            static const std::string LOG_FILENAME;                      // 日志记录文件名，字符串
            static const std::string LOG_LINENO;                          // 日志记录行号，整数
            static const std::string LOG_FUNCNAME;                      // 日志记录函数名，字符串
            static const std::string LOG_THREADID;                      // 日志记录线程ID，整数    
            static const std::string LOG_THREADNAME;                  // 日志记录线程名，字符串
            static const std::string LOG_PROCESSID;                    // 日志记录进程ID，整数
            static const std::string LOG_PROCESSNAME;                // 日志记录进程名，字符串
            static const std::string LOG_LEVEL_TIME_COLOR;          // 日志级别时间显示颜色，会进行分割显示颜色
            static const std::string LOG_LEVEL_COLOR;               // 日志级别显示颜色，会进行分割显示颜色
            static const std::string LOG_LEVEL_SOURCE_COLOR;         // 日志来源显示颜色，会进行分割显示颜色
            static const std::string LOG_LEVEL_MESSAGE_COLOR;       // 日志内容显示颜色，会进行分割显示颜色
            static const std::string LOG_CUSTOM_COLOR;             // 自定义颜色，会进行分割显示颜色
            static const std::string LOG_COLOR_END;                // 颜色结束符，用于分割显示颜色

            enum class ColorTag {
                None, Time, Level, Source, Message
            };
            ColorTag get_color_tag(const std::string& tag) {
                if (tag == LOG_LEVEL_TIME_COLOR) return ColorTag::Time;
                if (tag == LOG_LEVEL_COLOR) return ColorTag::Level;
                if (tag == LOG_LEVEL_SOURCE_COLOR) return ColorTag::Source;
                if (tag == LOG_LEVEL_MESSAGE_COLOR) return ColorTag::Message;
                return ColorTag::None;
            }

            fmt::text_style get_style(ColorTag tag, const Logger::LogLevel& level) {
                switch (tag) {
                    case ColorTag::Time:    return level.time_display_color;
                    case ColorTag::Level:   return level.level_display_color;
                    case ColorTag::Source:  return level.source_display_color;
                    case ColorTag::Message: return level.message_display_color;
                    default:                return fmt::text_style();
                }
            }
        };

        // 初始化日志记录器。
        static void initialize(size_t max_log_file_count = -1);

        // 用于初始化日志记录器的类。
        class Initializer {

        public:
            double construct_time = 0.0;
            // 构造函数，用于初始化日志记录器。
            Initializer() {
                construct_time = time_utils::get_time();
                Logger::initialize(Logger::log_file_max_count);
            }
        };

        static bool initialized; // 是否已初始化
        static std::fstream log_file; // 默认日志文件
        static uint64_t log_file_max_count; // 日志文件保留数量
        static Initializer initializer; // 初始化器
        static std::vector<LogHandler*> handlers; // 日志处理器
        static std::mutex log_mutex; // 日志互斥锁（向后兼容）
        static std::shared_mutex log_rw_mutex; // 读写锁，用于优化多线程性能

        // 日志消息结构体
        struct LogMessage {
            LogLevel level;
            std::string source;
            std::string file;
            int line;
            std::string message;
            double timestamp;
            LogMessage* next; // 用于内存池的链表指针
            
            // 构造函数
            LogMessage() : line(0), timestamp(0), next(nullptr) {}
            
            // 重置函数，用于内存池回收时清理资源
            void reset() {
                source.clear();
                file.clear();
                message.clear();
                next = nullptr;
            }
        };
        
        // 简单的内存池实现
        class LogMessagePool {
        public:
            LogMessagePool(size_t initial_size = 1000, size_t max_size = 10000) 
                : m_initial_size(initial_size), m_max_size(max_size), m_free_count(0), m_total_count(0) {
                // 预分配初始数量的对象
                for (size_t i = 0; i < initial_size; ++i) {
                    LogMessage* msg = new LogMessage();
                    msg->next = m_free_list;
                    m_free_list = msg;
                    m_free_count++;
                    m_total_count++;
                }
            }
            
            ~LogMessagePool() {
                // 释放所有对象
                while (m_free_list) {
                    LogMessage* msg = m_free_list;
                    m_free_list = msg->next;
                    delete msg;
                }
            }
            
            // 分配对象
            LogMessage* allocate() {
                std::lock_guard<std::mutex> lock(m_mutex);
                
                if (m_free_list) {
                    // 从空闲列表中获取对象
                    LogMessage* msg = m_free_list;
                    m_free_list = msg->next;
                    m_free_count--;
                    return msg;
                } else if (m_total_count < m_max_size) {
                    // 分配新对象
                    LogMessage* msg = new LogMessage();
                    m_total_count++;
                    return msg;
                }
                
                // 内存池已满，返回nullptr
                return nullptr;
            }
            
            // 回收对象
            void deallocate(LogMessage* msg) {
                if (!msg) return;
                
                std::lock_guard<std::mutex> lock(m_mutex);
                
                // 重置对象
                msg->reset();
                
                // 将对象添加到空闲列表
                msg->next = m_free_list;
                m_free_list = msg;
                m_free_count++;
            }
            
            // 获取当前可用对象数量
            size_t getFreeCount() const {
                return m_free_count;
            }
            
            // 获取总分配对象数量
            size_t getTotalCount() const {
                return m_total_count;
            }
            
        private:
            size_t m_initial_size; // 初始大小
            size_t m_max_size; // 最大大小
            LogMessage* m_free_list = nullptr; // 空闲对象链表
            size_t m_free_count; // 空闲对象数量
            size_t m_total_count; // 总对象数量
            std::mutex m_mutex; // 互斥锁
        };
        
        static bool use_async; // 是否使用异步日志
        static std::queue<LogMessage*> log_queue; // 日志消息队列（使用指针）
        static LogMessagePool msg_pool; // 日志消息内存池
        static std::mutex queue_mutex; // 队列互斥锁
        static std::condition_variable queue_cond; // 队列条件变量
        static std::thread async_thread; // 异步处理线程
        static std::atomic<bool> async_running; // 异步线程是否运行
        static size_t batch_size; // 批量处理大小
        static size_t queue_max_size; // 队列最大大小
        static std::atomic<uint64_t> dropped_logs; // 丢弃的日志数量
        static std::atomic<uint64_t> pool_misses; // 内存池未命中次数

        const static LogLevel UNKNOWN;  // 未知类型
        const static LogLevel TRACE;    // 调试到追踪每一步
        const static LogLevel DEBUG;    // 调试信息
        const static LogLevel INFO;     // 普通信息
        const static LogLevel WARN;     // 警告信息
        const static LogLevel ERROR;    // 错误信息
        const static LogLevel CRITICAL; // 严重错误，但不致命
        const static LogLevel FATAL;    // 致命错误
        const static LogLevel OFF;      // 关闭日志记录
        const static LogLevel FORCE;    // 强制日志记录（即使日志已经关闭）

        /**
         * 设置日志级别。
         * @param level 日志级别
         */
        static void setLogLevel(const LogLevel& level);

        /**
         * 设置日志级别。
         * @param level 日志级别ID
         */
        static void setLogLevel(int level) {
            return setLogLevel((LogLevel) level);
        };

        /**
         * 设置日志级别。
         * @param level_name 日志级别名称
         */
        static void setLogLevel(const std::string& level_name) {
            return setLogLevel((LogLevel) level_name);
        };



        /**
         * 设置日志文件。
         * @param file 日志文件
         * @param append 是否追加
         * @return 是否成功
         */
        static bool setLogFile(const std::string& file, bool append = true);

        /**
         * 设置是否使用异步日志。
         * @param async 是否使用异步日志
         */
        static void setAsync(bool async);

        /**
         * 获取当前丢弃的日志数量。
         * @return 丢弃的日志数量
         */
        static uint64_t getDroppedLogs();

        /**
         * 设置批量处理大小。
         * @param size 批量大小
         */
        static void setBatchSize(size_t size);

        /**
         * 设置队列最大大小。
         * @param size 队列大小
         */
        static void setQueueMaxSize(size_t size);

        /**
         * 记录一行日志。
         * @param level 日志等级
         * @param message 日志消息
         * @param source 日志来源，最好写，方便排查位置
         * @param file 文件名
         * @param line 行号
         */
        static void log(
            const LogLevel& level,
            const std::string& source,
            const std::string& file,
            int line,
            const std::string& message
        );

        /**
         * 记录一行日志。
         * @param level_name 日志等级
         * @param message 日志消息
         * @param source 日志来源，最好写，方便排查位置
         */
        static void log(
            const std::string& level,
            const std::string& source,
            const std::string& file,
            int line,
            const std::string& message
        ) {
            log(LogLevel::getLevelByString(level), source, file, line, message);
        };

        /**
         * 记录一个错误。
         * @param message 错误消息
         * @param source 错误来源，最好写，方便排查位置
         * @param exception 异常的指针，如果有的话
         * @param level 日志等级，默认为ERROR
         * @param traceback 是否记录堆栈信息，默认为false
         */
        static void log_exc(
            const std::string& message,
            const std::string& source,
            const std::exception_ptr& exception,
            const LogLevel& level, 
            bool traceback
        );


        /**
         * 记录一个错误。
         * @param message 错误消息
         * @param source 错误来源，最好写，方便排查位置
         * @param exception 异常的指针，如果有的话
         * @param level_name 日志等级的名称，默认为"E"
         * @param traceback 是否记录堆栈信息，默认为false
         */
        static void log_exc(
            const std::string& message,
            const std::string& source,
            const std::exception_ptr& exception,
            const std::string& level_name,
            bool traceback
        ) {
            return log_exc(message, source, exception, LogLevel::getLevelByString(level_name), traceback);
        };


    };
}

#endif // LOGGER_H
