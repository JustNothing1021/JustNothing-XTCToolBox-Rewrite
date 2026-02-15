
#include "utils/logger.h"
#include "utils/time_utils.h"
#include "utils/string_utils.h"


using namespace std;
using namespace fmt;
using namespace log_utils;


fstream Logger::log_file;
bool Logger::initialized = false;
uint64_t Logger::log_file_max_count = 10;
mutex Logger::log_mutex;

// 异步日志相关成员定义
bool Logger::use_async = true; // 默认启用异步日志
std::queue<Logger::LogMessage*> Logger::log_queue;
Logger::LogMessagePool Logger::msg_pool(10000, 100000); // 内存池，初始10000个对象，最大100000个
std::mutex Logger::queue_mutex;
std::condition_variable Logger::queue_cond;
std::thread Logger::async_thread;
std::atomic<bool> Logger::async_running(false);
size_t Logger::batch_size = 100;
size_t Logger::queue_max_size = 10000;
std::atomic<uint64_t> Logger::dropped_logs(0);
std::atomic<uint64_t> Logger::pool_misses(0);


const string Logger::LogHandlerFormatMapping::LOG_TIME = "log_time_str";
const string Logger::LogHandlerFormatMapping::LOG_TIMESTAMP = "log_timestamp";
const string Logger::LogHandlerFormatMapping::LOG_LEVEL_DISPLAY_NAME = "log_level_display_name";
const string Logger::LogHandlerFormatMapping::LOG_LEVEL_FULL_NAME = "log_level_full_name";
const string Logger::LogHandlerFormatMapping::LOG_SOURCE = "log_source";
const string Logger::LogHandlerFormatMapping::LOG_MESSAGE = "log_message";
const string Logger::LogHandlerFormatMapping::LOG_FILENAME = "log_filename";
const string Logger::LogHandlerFormatMapping::LOG_LINENO = "log_lineno";
const string Logger::LogHandlerFormatMapping::LOG_FUNCNAME = "log_funcname";
const string Logger::LogHandlerFormatMapping::LOG_THREADID = "log_threadid";
const string Logger::LogHandlerFormatMapping::LOG_THREADNAME = "log_threadname";
const string Logger::LogHandlerFormatMapping::LOG_PROCESSID = "log_processid";
const string Logger::LogHandlerFormatMapping::LOG_PROCESSNAME = "log_processname";

const string Logger::LogHandlerFormatMapping::LOG_LEVEL_TIME_COLOR = "{log_level_time_color}";
const string Logger::LogHandlerFormatMapping::LOG_LEVEL_COLOR = "{log_level_color}";
const string Logger::LogHandlerFormatMapping::LOG_LEVEL_SOURCE_COLOR = "{log_level_source_color}";
const string Logger::LogHandlerFormatMapping::LOG_LEVEL_MESSAGE_COLOR = "{log_level_message_color}";
const string Logger::LogHandlerFormatMapping::LOG_CUSTOM_COLOR = "{log_custom_color}";
const string Logger::LogHandlerFormatMapping::LOG_COLOR_END = "{log_color_end}";

// 定义日志等级常量，必须在创建 LogHandler 之前初始化
const Logger::LogLevel Logger::UNKNOWN = {
    Logger::LogLevelID::UNKNOWN, "UNKNOWN", "?",
    fg(color::blue) | bg(color::gray),
    fg(terminal_color::bright_blue) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(terminal_color::bright_black) | bg(terminal_color::black),
};


const Logger::LogLevel Logger::TRACE = {
    Logger::LogLevelID::TRACE, "TRACE", "T",
    fg(color::blue) | bg(terminal_color::black),
    fg(color::cyan) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(color::cyan) | bg(terminal_color::black),
};

const Logger::LogLevel Logger::DEBUG = {
    Logger::LogLevelID::DEBUG, "DEBUG", "D",
    fg(color::blue) | bg(terminal_color::black),
    fg(terminal_color::bright_blue) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(terminal_color::bright_blue) | bg(terminal_color::black),
};

const Logger::LogLevel Logger::INFO = {
    Logger::LogLevelID::INFO, "INFO", "I",
    fg(color::blue) | bg(terminal_color::black),
    fg(terminal_color::green) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(terminal_color::green) | bg(terminal_color::black),
};

const Logger::LogLevel Logger::WARN = {
    Logger::LogLevelID::WARN, "WARN", "W",
    fg(color::blue) | bg(terminal_color::black),
    fg(terminal_color::yellow) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(terminal_color::yellow) | bg(terminal_color::black),
};

const Logger::LogLevel Logger::ERROR = {
    Logger::LogLevelID::ERROR, "ERROR", "E",
    fg(color::blue) | bg(terminal_color::black),
    fg(terminal_color::red) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(terminal_color::red) | bg(terminal_color::black),
};

const Logger::LogLevel Logger::CRITICAL = {
    Logger::LogLevelID::CRITICAL, "CRITICAL", "C",
    fg(color::blue) | bg(terminal_color::black),
    fg(terminal_color::bright_red) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(terminal_color::bright_red) | bg(terminal_color::black),
};

const Logger::LogLevel Logger::FATAL = {
    Logger::LogLevelID::FATAL, "FATAL", "F",
    fg(color::blue) | bg(terminal_color::black),
    fg(terminal_color::magenta) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(terminal_color::magenta) | bg(terminal_color::black),
};

const Logger::LogLevel Logger::OFF = {
    Logger::LogLevelID::OFF, "OFF", "-",
    fg(color::blue) | bg(terminal_color::black),
    fg(rgb(21, 255, 247)) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(rgb(21, 255, 247)) | bg(terminal_color::black),
};

const Logger::LogLevel Logger::FORCE = {
    Logger::LogLevelID::FORCE, "FORCE", "!",
    fg(color::blue) | bg(terminal_color::black),
    fg(color::white) | bg(terminal_color::black),
    fg(color::dark_gray) | bg(terminal_color::black),
    fg(color::white) | bg(terminal_color::black),
};

// 创建默认的处理器
Logger::LogHandler* stdout_handler = new Logger::LogHandler(
    "stdout", stdout, Logger::TRACE,
    fmt::format(
        "{}{{{}}}{} {{{}}} {}{{{}:<35}} {}{{{}}}\n",
        Logger::LogHandlerFormatMapping::LOG_LEVEL_TIME_COLOR,
        Logger::LogHandlerFormatMapping::LOG_TIME,
        Logger::LogHandlerFormatMapping::LOG_LEVEL_COLOR,
        Logger::LogHandlerFormatMapping::LOG_LEVEL_DISPLAY_NAME,
        Logger::LogHandlerFormatMapping::LOG_LEVEL_SOURCE_COLOR,
        Logger::LogHandlerFormatMapping::LOG_SOURCE,
        Logger::LogHandlerFormatMapping::LOG_LEVEL_MESSAGE_COLOR,
        Logger::LogHandlerFormatMapping::LOG_MESSAGE
    ));

Logger::LogHandler* logfile_handler = new Logger::LogHandler(
    "logfile", &Logger::log_file, Logger::TRACE,
    fmt::format(
        "{{{}}} {{{}:<8}} {:<60} {{{}}}\n",
        Logger::LogHandlerFormatMapping::LOG_TIME,
        Logger::LogHandlerFormatMapping::LOG_LEVEL_FULL_NAME,
        fmt::format(
            "{{{}}}({{{}}}:{{{}}})",
            Logger::LogHandlerFormatMapping::LOG_SOURCE,
            Logger::LogHandlerFormatMapping::LOG_FILENAME,
            Logger::LogHandlerFormatMapping::LOG_LINENO
        ),
        Logger::LogHandlerFormatMapping::LOG_MESSAGE
    ));

vector<Logger::LogHandler*> Logger::handlers = { stdout_handler, logfile_handler };


Logger::LogLevel Logger::LogLevel::getLevelByID(int level_id) {
    switch (level_id) {
        case LogLevelID::TRACE: return TRACE;
        case LogLevelID::DEBUG: return DEBUG;
        case LogLevelID::INFO: return INFO;
        case LogLevelID::WARN: return WARN;
        case LogLevelID::ERROR: return ERROR;
        case LogLevelID::CRITICAL: return CRITICAL;
        case LogLevelID::FATAL: return FATAL;
        case LogLevelID::OFF: return OFF;
        case LogLevelID::FORCE: return FORCE;
        default: return UNKNOWN;
    }
}

Logger::LogLevel Logger::LogLevel::getLevelByName(const string& name) {
    if (name == TRACE.level_name) return TRACE;
    if (name == DEBUG.level_name) return DEBUG;
    if (name == INFO.level_name) return INFO;
    if (name == WARN.level_name) return WARN;
    if (name == ERROR.level_name) return ERROR;
    if (name == CRITICAL.level_name) return CRITICAL;
    if (name == FATAL.level_name) return FATAL;
    if (name == OFF.level_name) return OFF;
    if (name == FORCE.level_name) return FORCE;
    return UNKNOWN;
}

Logger::LogLevel Logger::LogLevel::getLevelByDisplayName(const string& name) {
    if (name == TRACE.display_name) return TRACE;
    if (name == DEBUG.display_name) return DEBUG;
    if (name == INFO.display_name) return INFO;
    if (name == WARN.display_name) return WARN;
    if (name == ERROR.display_name) return ERROR;
    if (name == CRITICAL.display_name) return CRITICAL;
    if (name == FATAL.display_name) return FATAL;
    if (name == OFF.display_name) return OFF;
    if (name == FORCE.display_name) return FORCE;
    return UNKNOWN;
}

Logger::LogLevel Logger::LogLevel::getLevelByString(const string& level_str) {
    Logger::LogLevel level = LogLevel::getLevelByName(level_str);
    return level == UNKNOWN ? LogLevel::getLevelByDisplayName(level_str) : level;
}


void Logger::initialize(size_t max_log_file_count) {
    if (initialized) return;
    
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hErr, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hErr, dwMode);
        }
    }
#endif
    
    string log_file_name = time_utils::get_formatted_time(Logger::initializer.construct_time, "log_%Y%m%d_%H%M%S.log");
    auto log_file_path = filesystem::current_path() / "log" / log_file_name;
    filesystem::create_directory(filesystem::current_path() / "log");

    // 清理日志文件
    vector<filesystem::directory_entry> log_files;
    for (const auto& entry : filesystem::directory_iterator(filesystem::current_path() / "log")) {
        if (entry.is_regular_file()) {
            if (entry.path().extension() == ".log") {
                log_files.push_back(entry);
            }
        }
    }

    if (log_files.size() > max_log_file_count) {
        int files_to_remove = log_files.size() - max_log_file_count;
        sort(log_files.begin(), log_files.end(),
            [](const filesystem::directory_entry& a, const filesystem::directory_entry& b) {
                return a.path().filename() < b.path().filename();
            }
        );
        for (int i = 0; i < files_to_remove - 1; i++)
            filesystem::remove(log_files[i].path());
    }

    log_file.open(log_file_path, ios::out);
    if (log_file.is_open()) {
        initialized = true;
        LINFO("Logger::initialize", "日志记录器初始化成功");

        if (use_async) {
            async_running = true;
            async_thread = thread([]() {
                extern unordered_map<string, any> log_context;
                while (async_running.load()) {
                    vector<LogMessage*> messages;
                    messages.reserve(batch_size);
                    { 
                        unique_lock<mutex> lock(queue_mutex);
                        queue_cond.wait_for(lock, chrono::milliseconds(100), []() {
                            return !log_queue.empty() || !async_running.load();
                        });

                        while (!log_queue.empty() && messages.size() < batch_size) {
                            messages.push_back(log_queue.front());
                            log_queue.pop();
                        }
                    }


                    for (const auto msg : messages) {
                        // 处理单条日志消息的逻辑（复制自log方法）
                        log_context[Logger::LogHandlerFormatMapping::LOG_TIME] = time_utils::get_formatted_time_with_frac(msg->timestamp, "%Y-%m-%d %H:%M:%S.%f", 3);
                        log_context[Logger::LogHandlerFormatMapping::LOG_TIMESTAMP] = msg->timestamp;
                        log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = msg->message;
                        log_context[Logger::LogHandlerFormatMapping::LOG_SOURCE] = msg->source;
                        log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_DISPLAY_NAME] = msg->level.display_name;
                        log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_FULL_NAME] = msg->level.level_name;
                        log_context[Logger::LogHandlerFormatMapping::LOG_FILENAME] = msg->file;
                        log_context[Logger::LogHandlerFormatMapping::LOG_LINENO] = msg->line;

                        for (const string& line : string_utils::split(msg->message, "\n")) {
                            log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = string_utils::trim(line, "\n");
                            if (line.empty()) continue;

                            for (Logger::LogHandler* handler : Logger::handlers) {
                                if (*handler->level > msg->level) continue;
                                if (handler->file_p != nullptr) {
                                    size_t pos = 0;
                                    string replaced_string = string_utils::format_with_map(handler->formatter, log_context);
                                    while (pos < replaced_string.size()) {
                                        size_t time_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_LEVEL_TIME_COLOR, pos);
                                        size_t level_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_LEVEL_COLOR, pos);
                                        size_t source_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_LEVEL_SOURCE_COLOR, pos);
                                        size_t message_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_LEVEL_MESSAGE_COLOR, pos);
                                        size_t end_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_COLOR_END, pos);
                                        multimap<size_t, pair<string, fmt::v11::text_style>> color_tags = {
                                            {time_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_TIME_COLOR, msg->level.time_display_color}},
                                            {level_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_COLOR, msg->level.level_display_color}},
                                            {source_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_SOURCE_COLOR, msg->level.source_display_color}},
                                            {message_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_MESSAGE_COLOR, msg->level.message_display_color}},
                                            {end_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_COLOR_END, fmt::v11::text_style()}}
                                        };
                                        size_t first_tag_pos;
                                        string first_tag_content;
                                        fmt::v11::text_style first_tag_style;
                                        size_t second_tag_pos;
                                        string second_tag_content;
                                        fmt::v11::text_style second_tag_style;
                                        for (auto it = color_tags.begin(); it != color_tags.end(); ++it) {
                                            if (it == color_tags.begin()) {
                                                first_tag_pos = it->first;
                                                first_tag_content = it->second.first;
                                                first_tag_style = it->second.second;
                                            } else {
                                                second_tag_pos = it->first;
                                                second_tag_content = it->second.first;
                                                second_tag_style = it->second.second;
                                                break;
                                            }
                                        }
                                        size_t slice_left_pos = first_tag_pos + first_tag_content.size();
                                        size_t slice_right_pos = second_tag_pos;

                                        if (slice_left_pos == string::npos) {
                                            first_tag_style = fmt::v11::text_style();
                                            slice_left_pos = pos;
                                        };
                                        if (slice_right_pos == string::npos) slice_right_pos = replaced_string.size();

                                        if (slice_left_pos > replaced_string.size()) slice_left_pos = pos;
                                        if (slice_right_pos > replaced_string.size()) slice_right_pos = replaced_string.size();
                                        fmt::print(handler->file_p, first_tag_style, "{}",
                                            string_utils::remove_invalid_utf8(
                                            replaced_string.substr(slice_left_pos, slice_right_pos - slice_left_pos)
                                            )
                                        );
                                        fflush(handler->file_p);
                                        pos = slice_right_pos;
                                    }
                                } else if (handler->ostream_p != nullptr) {
                                    *handler->ostream_p << string_utils::format_with_map(handler->formatter, log_context);
                                    handler->ostream_p->flush();
                                } else if (handler->func_p != nullptr) {
                                    (*(handler->func_p)) (string_utils::format_with_map(handler->formatter, log_context));
                                }
                            }
                        }
                        
                        // 回收日志消息对象
                        msg_pool.deallocate(msg);
                    }
                }

                // 处理剩余的日志消息
                vector<LogMessage*> remaining;
                { 
                    unique_lock<mutex> lock(queue_mutex);
                    while (!log_queue.empty()) {
                        remaining.push_back(log_queue.front());
                        log_queue.pop();
                    }
                }

                // 处理剩余的日志
                for (const auto msg : remaining) {
                    log_context[Logger::LogHandlerFormatMapping::LOG_TIME] = time_utils::get_formatted_time_with_frac(msg->timestamp, "%Y-%m-%d %H:%M:%S.%f", 3);
                    log_context[Logger::LogHandlerFormatMapping::LOG_TIMESTAMP] = msg->timestamp;
                    log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = msg->message;
                    log_context[Logger::LogHandlerFormatMapping::LOG_SOURCE] = msg->source;
                    log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_DISPLAY_NAME] = msg->level.display_name;
                    log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_FULL_NAME] = msg->level.level_name;
                    log_context[Logger::LogHandlerFormatMapping::LOG_FILENAME] = msg->file;
                    log_context[Logger::LogHandlerFormatMapping::LOG_LINENO] = msg->line;

                    for (const string& line : string_utils::split(msg->message, "\n")) {
                        log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = string_utils::trim(line, "\n");
                        if (line.empty()) continue;

                        for (Logger::LogHandler* handler : Logger::handlers) {
                            if (*handler->level > msg->level) continue;
                            if (handler->ostream_p != nullptr) {
                                *handler->ostream_p << string_utils::format_with_map(handler->formatter, log_context);
                                handler->ostream_p->flush();
                            } else if (handler->func_p != nullptr) {
                                (*(handler->func_p)) (string_utils::format_with_map(handler->formatter, log_context));
                            }
                        }
                    }
                    
                    // 回收日志消息对象
                    msg_pool.deallocate(msg);
                }

                LINFO("Logger::setAsync", "异步日志线程已停止");
            });
        }
    } else {
        cerr << "打开日志文件夹失败: " << log_file_path << endl;
    }
}



bool Logger::setLogFile(const string& file_name, bool append) {
    if (log_file.is_open()) {
        log_file.close();
    }
    log_file.open(file_name, append ? ios::app : ios::out);
    return log_file.is_open();
}

void Logger::setAsync(bool async) {
    if (async == use_async) return;
    
    use_async = async;
    
    if (async) {

        if (!async_running.load()) {
            async_running = true;
            async_thread = thread([]() {
                extern unordered_map<string, any> log_context;
                while (async_running.load()) {
                    vector<LogMessage*> batch;
                    batch.reserve(batch_size);

                    // 从队列中获取日志消息
                    { 
                        unique_lock<mutex> lock(queue_mutex);
                        queue_cond.wait_for(lock, chrono::milliseconds(100), []() {
                            return !log_queue.empty() || !async_running.load();
                        });

                        // 批量获取日志消息
                        while (!log_queue.empty() && batch.size() < batch_size) {
                            batch.push_back(log_queue.front());
                            log_queue.pop();
                        }
                    }

                    // 处理批量日志消息
                    for (const auto msg : batch) {
                        // 处理单条日志消息的逻辑（复制自log方法）
                        log_context[Logger::LogHandlerFormatMapping::LOG_TIME] = time_utils::get_formatted_time_with_frac(msg->timestamp, "%Y-%m-%d %H:%M:%S.%f", 3);
                        log_context[Logger::LogHandlerFormatMapping::LOG_TIMESTAMP] = msg->timestamp;
                        log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = msg->message;
                        log_context[Logger::LogHandlerFormatMapping::LOG_SOURCE] = msg->source;
                        log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_DISPLAY_NAME] = msg->level.display_name;
                        log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_FULL_NAME] = msg->level.level_name;
                        log_context[Logger::LogHandlerFormatMapping::LOG_FILENAME] = msg->file;
                        log_context[Logger::LogHandlerFormatMapping::LOG_LINENO] = msg->line;

                        for (const string& line : string_utils::split(msg->message, "\n")) {
                            log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = string_utils::trim(line, "\n");
                            if (line.empty()) continue;

                            for (Logger::LogHandler* handler : Logger::handlers) {
                                if (*handler->level > msg->level) continue;
                                if (handler->file_p != nullptr) {
                                    size_t pos = 0;
                                    string replaced_string = string_utils::format_with_map(handler->formatter, log_context);
                                    while (pos < replaced_string.size()) {
                                        size_t time_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_LEVEL_TIME_COLOR, pos);
                                        size_t level_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_LEVEL_COLOR, pos);
                                        size_t source_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_LEVEL_SOURCE_COLOR, pos);
                                        size_t message_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_LEVEL_MESSAGE_COLOR, pos);
                                        size_t end_color_tag_pos = replaced_string.find(
                                            Logger::LogHandlerFormatMapping::LOG_COLOR_END, pos);
                                        multimap<size_t, pair<string, fmt::v11::text_style>> color_tags = {
                                            {time_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_TIME_COLOR, msg->level.time_display_color}},
                                            {level_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_COLOR, msg->level.level_display_color}},
                                            {source_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_SOURCE_COLOR, msg->level.source_display_color}},
                                            {message_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_MESSAGE_COLOR, msg->level.message_display_color}},
                                            {end_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_COLOR_END, fmt::v11::text_style()}}
                                        };
                                        size_t first_tag_pos;
                                        string first_tag_content;
                                        fmt::v11::text_style first_tag_style;
                                        size_t second_tag_pos;
                                        string second_tag_content;
                                        fmt::v11::text_style second_tag_style;
                                        for (auto it = color_tags.begin(); it != color_tags.end(); ++it) {
                                            if (it == color_tags.begin()) {
                                                first_tag_pos = it->first;
                                                first_tag_content = it->second.first;
                                                first_tag_style = it->second.second;
                                            } else {
                                                second_tag_pos = it->first;
                                                second_tag_content = it->second.first;
                                                second_tag_style = it->second.second;
                                                break;
                                            }
                                        }
                                        size_t slice_left_pos = first_tag_pos + first_tag_content.size();
                                        size_t slice_right_pos = second_tag_pos;

                                        if (slice_left_pos == string::npos) {
                                            first_tag_style = fmt::v11::text_style();
                                            slice_left_pos = pos;
                                        };
                                        if (slice_right_pos == string::npos) slice_right_pos = replaced_string.size();

                                        if (slice_left_pos > replaced_string.size()) slice_left_pos = pos;
                                        if (slice_right_pos > replaced_string.size()) slice_right_pos = replaced_string.size();
                                        fmt::print(handler->file_p, first_tag_style, "{}",
                                            string_utils::remove_invalid_utf8(
                                            replaced_string.substr(slice_left_pos, slice_right_pos - slice_left_pos)
                                            )
                                        );
                                        fflush(handler->file_p);
                                        pos = slice_right_pos;
                                    }
                                } else if (handler->ostream_p != nullptr) {
                                    *handler->ostream_p << string_utils::format_with_map(handler->formatter, log_context);
                                    handler->ostream_p->flush();
                                } else if (handler->func_p != nullptr) {
                                    (*(handler->func_p)) (string_utils::format_with_map(handler->formatter, log_context));
                                }
                            }
                        }
                        
                        // 回收日志消息对象
                        msg_pool.deallocate(msg);
                    }
                }

                // 处理剩余的日志消息
                vector<LogMessage*> remaining;
                { 
                    unique_lock<mutex> lock(queue_mutex);
                    while (!log_queue.empty()) {
                        remaining.push_back(log_queue.front());
                        log_queue.pop();
                    }
                }

                // 处理剩余的日志
                for (const auto msg : remaining) {
                    log_context[Logger::LogHandlerFormatMapping::LOG_TIME] = time_utils::get_formatted_time_with_frac(msg->timestamp, "%Y-%m-%d %H:%M:%S.%f", 3);
                    log_context[Logger::LogHandlerFormatMapping::LOG_TIMESTAMP] = msg->timestamp;
                    log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = msg->message;
                    log_context[Logger::LogHandlerFormatMapping::LOG_SOURCE] = msg->source;
                    log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_DISPLAY_NAME] = msg->level.display_name;
                    log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_FULL_NAME] = msg->level.level_name;
                    log_context[Logger::LogHandlerFormatMapping::LOG_FILENAME] = msg->file;
                    log_context[Logger::LogHandlerFormatMapping::LOG_LINENO] = msg->line;

                    for (const string& line : string_utils::split(msg->message, "\n")) {
                        log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = string_utils::trim(line, "\n");
                        if (line.empty()) continue;

                        for (Logger::LogHandler* handler : Logger::handlers) {
                            if (*handler->level > msg->level) continue;
                            if (handler->ostream_p != nullptr) {
                                *handler->ostream_p << string_utils::format_with_map(handler->formatter, log_context);
                                handler->ostream_p->flush();
                            } else if (handler->func_p != nullptr) {
                                (*(handler->func_p)) (string_utils::format_with_map(handler->formatter, log_context));
                            }
                        }
                    }
                    
                    // 回收日志消息对象
                    msg_pool.deallocate(msg);
                }

                LINFO("Logger::setAsync", "异步日志线程已停止");
            });
        }
    } else {
        // 停止异步线程
        async_running = false;
        queue_cond.notify_one();
        if (async_thread.joinable()) {
            async_thread.join();
        }
        
        // 处理剩余的日志消息
        vector<LogMessage*> remaining;
        { 
            unique_lock<mutex> lock(queue_mutex);
            while (!log_queue.empty()) {
                remaining.push_back(log_queue.front());
                log_queue.pop();
            }
        }

        // 处理剩余的日志
        for (const auto msg : remaining) {
            log(msg->level, msg->source, msg->file, msg->line, msg->message);
            // 回收日志消息对象
            msg_pool.deallocate(msg);
        }
    }
}

uint64_t Logger::getDroppedLogs() {
    return dropped_logs.load(memory_order_relaxed);
}

void Logger::setBatchSize(size_t size) {
    if (size > 0) {
        batch_size = size;
    }
}

void Logger::setQueueMaxSize(size_t size) {
    if (size > 0) {
        queue_max_size = size;
    }
}



void Logger::log(
    const LogLevel& level,
    const string& source,
    const string& file,
    int line,
    const string& message
) {
    double time = time_utils::get_time();

    // 如果使用异步模式，将日志消息放入队列
    if (use_async) {
        // 从内存池分配对象
        LogMessage* msg = msg_pool.allocate();
        if (!msg) {
            // 内存池已满，使用new分配
            msg = new LogMessage();
            pool_misses.fetch_add(1, memory_order_relaxed);
        }
        
        // 设置日志消息内容
        msg->level = level;
        msg->source = source;
        msg->file = file;
        msg->line = line;
        msg->message = message;
        msg->timestamp = time;
        
        unique_lock<mutex> lock(queue_mutex, defer_lock);
        
        // 尝试非阻塞获取锁，如果失败则放弃
        if (lock.try_lock()) {
            if (log_queue.size() < queue_max_size) {
                log_queue.push(msg);
                queue_cond.notify_one();
            } else {
                // 队列已满，丢弃日志
                dropped_logs.fetch_add(1, memory_order_relaxed);
                // 回收对象
                msg_pool.deallocate(msg);
            }
        } else {
            // 获取锁失败，丢弃日志
            dropped_logs.fetch_add(1, memory_order_relaxed);
            // 回收对象
            msg_pool.deallocate(msg);
        }
        return;
    }

    // 同步模式，保持原有的处理逻辑
    Logger::log_mutex.lock();
    string formatted_time = time_utils::get_formatted_time_with_frac(time, "%Y-%m-%d %H:%M:%S.%f", 3);
    log_context[Logger::LogHandlerFormatMapping::LOG_TIME] = formatted_time;
    log_context[Logger::LogHandlerFormatMapping::LOG_TIMESTAMP] = time;
    log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = message;
    log_context[Logger::LogHandlerFormatMapping::LOG_SOURCE] = source;
    log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_DISPLAY_NAME] = level.display_name;
    log_context[Logger::LogHandlerFormatMapping::LOG_LEVEL_FULL_NAME] = level.level_name;
    log_context[Logger::LogHandlerFormatMapping::LOG_FILENAME] = file;
    log_context[Logger::LogHandlerFormatMapping::LOG_LINENO] = line;
    for (const string& line : string_utils::split(message, "\n")) {
        log_context[Logger::LogHandlerFormatMapping::LOG_MESSAGE] = string_utils::trim(line, "\n");
        if (line.empty()) continue;

        for (Logger::LogHandler* handler : Logger::handlers) {
            if (*handler->level > level) continue;  //  信息的等级太低了就不记录
            if (handler->file_p != nullptr) {
                size_t pos = 0;
                string replaced_string = string_utils::format_with_map(handler->formatter, log_context);
                while (pos < replaced_string.size()) {
                    size_t time_color_tag_pos = replaced_string.find(
                        Logger::LogHandlerFormatMapping::LOG_LEVEL_TIME_COLOR, pos);
                    size_t level_color_tag_pos = replaced_string.find(
                        Logger::LogHandlerFormatMapping::LOG_LEVEL_COLOR, pos);
                    size_t source_color_tag_pos = replaced_string.find(
                        Logger::LogHandlerFormatMapping::LOG_LEVEL_SOURCE_COLOR, pos);
                    size_t message_color_tag_pos = replaced_string.find(
                        Logger::LogHandlerFormatMapping::LOG_LEVEL_MESSAGE_COLOR, pos);
                    size_t end_color_tag_pos = replaced_string.find(
                        Logger::LogHandlerFormatMapping::LOG_COLOR_END, pos);
                    multimap<size_t, pair<string, fmt::v11::text_style>> color_tags = {
                        {time_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_TIME_COLOR, level.time_display_color}},
                        {level_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_COLOR, level.level_display_color}},
                        {source_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_SOURCE_COLOR, level.source_display_color}},
                        {message_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_LEVEL_MESSAGE_COLOR, level.message_display_color}},
                        {end_color_tag_pos, {Logger::LogHandlerFormatMapping::LOG_COLOR_END, fmt::v11::text_style()}}
                    };
                    size_t first_tag_pos;
                    string first_tag_content;
                    fmt::v11::text_style first_tag_style;
                    size_t second_tag_pos;
                    string second_tag_content;
                    fmt::v11::text_style second_tag_style;
                    for (auto it = color_tags.begin(); it != color_tags.end(); ++it) {
                        if (it == color_tags.begin()) {
                            first_tag_pos = it->first;
                            first_tag_content = it->second.first;
                            first_tag_style = it->second.second;
                        } else {
                            second_tag_pos = it->first;
                            second_tag_content = it->second.first;
                            second_tag_style = it->second.second;
                            break;
                        }
                    }
                    size_t slice_left_pos = first_tag_pos + first_tag_content.size();
                    size_t slice_right_pos = second_tag_pos;


                    if (slice_left_pos == string::npos) {
                        first_tag_style = fmt::v11::text_style();
                        slice_left_pos = pos;
                    };
                    if (slice_right_pos == string::npos) slice_right_pos = replaced_string.size();

                    if (slice_left_pos > replaced_string.size()) slice_left_pos = pos;
                    if (slice_right_pos > replaced_string.size()) slice_right_pos = replaced_string.size();
                    fmt::print(handler->file_p, first_tag_style, "{}",
                        string_utils::remove_invalid_utf8(
                        replaced_string.substr(slice_left_pos, slice_right_pos - slice_left_pos)
                        )
                    );
                    fflush(handler->file_p);
                    pos = slice_right_pos;
                }

            } else if (handler->ostream_p != nullptr) {
                *handler->ostream_p << string_utils::format_with_map(handler->formatter, log_context);
                handler->ostream_p->flush();

            } else if (handler->func_p != nullptr) {
                (*(handler->func_p)) (string_utils::format_with_map(handler->formatter, log_context));
            }
        }
    }
    Logger::log_mutex.unlock();
};

void Logger::log_exc(
    const string& message,
    const string& source,
    const exception_ptr& exception,
    const LogLevel& level,
    bool traceback
) {
    // TODO: 实现异常日志记录
};

Logger::Initializer Logger::initializer;

Logger::Initializer::~Initializer() {
    try {
        Logger::setAsync(false);
    } catch (...) {}

    if (Logger::log_file.is_open()) {
        Logger::log_file.flush();
        Logger::log_file.close();
    }
}

