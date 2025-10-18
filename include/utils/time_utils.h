#pragma once


#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <ctime>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fmt/core.h>
#include <fmt/chrono.h>


// 时间工具类。
namespace time_utils {
    /**
     * 获取当前时间戳，单位为秒。
     * @return 当前时间戳
     */
    double get_time();


    /**
     * 获取当前时间戳，单位为毫秒。
     * @return 当前时间戳
     */
    double get_time_ms();


    /**
     * 获取当前时间字符串。
     * @param time 时间戳，单位为秒
     * @param fmt 时间格式，默认为"%Y-%m-%d %H:%M:%S"
     */
    std::string get_formatted_time(double time, const std::string& fmt = "%Y-%m-%d %H:%M:%S");

    /**
     * 获取当前时间字符串。
     * @param fmt 时间格式，默认为"%Y-%m-%d %H:%M:%S"
     */
    std::string get_formatted_time(const std::string& fmt = "%Y-%m-%d %H:%M:%S");

    /**
     * 获取当前时间字符串，包含小数。
     * @param time，单位为秒
     * @param fmt 时间格式，默认为"%Y-%m-%d %H:%M:%S"
     * @param precision 小数精度，默认为3
     */
    std::string get_formatted_time_with_frac(double time, std::string fmt = "%Y-%m-%d %H:%M:%S", int precision = 3);


    /**
     * 获取当前时间字符串，包含小数。
     * @param fmt 时间格式，默认为"%Y-%m-%d %H:%M:%S"
     * @param precision 小数精度，默认为3
     */
    std::string get_foramtted_time_with_frac(int precision = 3);

    /**
     * 等待指定的秒数。
     * @param seconds 秒数
     */
    void sleep(double seconds);

    /**
     * 等待指定的毫秒数。
     * @param milliseconds 毫秒数
     */
    void sleep_ms(int milliseconds);

    /**
     * 等待指定的微秒数。
     * @param microseconds 微秒数
     */
    void sleep_us(int microseconds);




    class StopWatch {

    private:
        double start_time;
        double last_stop_time;
        double last_lap_time;
        bool is_running = false;


    public:

        // 默认构造函数，初始化开始时间为当前时间。
        // 构造的一瞬间就开始计时。
        StopWatch();

        // 构造函数，传入一个开始时间。
        // 构造的一瞬间就开始计时。
        StopWatch(double start_time);

        // 开启秒表。
        // @return 开始时间
        double start();

        // 停止秒表，并返回当前时间。
        // @return 从开始到现在的时间间隔
        double stop();

        // 重置秒表，并返回当前时间。
        // @return 重置后的开始时间
        // @note 重置后秒表会重新开始计时。
        double reset();

        // 计次。
        // @return 从上次计次到现在的时间间隔
        double lap();


        // 计次并停止秒表。
        // @return 从上次计次到现在的时间间隔
        // @note 这个函数的行为类似于 lap() + stop() 的组合，
        //      停止后秒表不会继续计时。
        double lap_and_stop();

        // 计次并打印结果。
        // @param os 输出流，默认为 cout
        // @return 从上次计次到现在的时间间隔
        double lap_and_print(const std::ostream& os = std::cout);

        // 计次并重置秒表。
        // @return 从上次计次到现在的时间间隔
        // @note 这个函数的行为类似于 lap() + reset() 的组合，
        //      重置后秒表会重新开始计时。
        double lap_and_reset();

        // 获取从开始到现在的时间间隔。
        // @return 从开始到现在的时间间隔
        double elapsed();

        // 获取开始时间。
        // @return 开始时间
        double get_start_time();

        // 获取上次计次的时间。
        // @return 上次计次的时间
        double get_last_lap_time();

        // 获取上次停止的时间。
        // @return 上次停止的时间
        double get_last_stop_time();
    };
}


#endif // TIME_UTILS_H