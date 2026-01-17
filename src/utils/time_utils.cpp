#include "utils/time_utils.h"


using namespace std;
using namespace chrono;

double time_utils::get_time() {
    return (double) duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count() / 1e9;
}

double time_utils::get_time_ms() {
    return (double) duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
}

string strftime_expand(const string& fmt, const tm& tm) {
    size_t size = fmt.size() + 64;
    vector<char> buf(size);
    while (true) {
        size_t len = strftime(buf.data(), buf.size(), fmt.c_str(), &tm);
        if (len > 0)
            return string(buf.data(), len);
        buf.resize(buf.size() * 2);
    }
}

string time_utils::get_formatted_time_with_frac(double time, string fmt, int precision) {
    time_t sec = static_cast<time_t>(time);
    double frac = abs(time - sec);
    
    // 使用 C++17 的 std::localtime，避免平台差异
    std::tm tm_time;
    auto time_ptr = std::localtime(&sec);
    if (time_ptr) {
        tm_time = *time_ptr;
    } else {
        // 如果获取时间失败，使用当前时间
        time_t now = std::time(nullptr);
        time_ptr = std::localtime(&now);
        if (time_ptr) {
            tm_time = *time_ptr;
        }
    }

    // 先格式化主串

    // 构造小数字符串
    string frac_str;
    if (precision > 0) {
        int frac_val = static_cast<int>(round(frac * pow(10, precision)));
        if (frac_val >= static_cast<int>(pow(10, precision))) frac_val = 0;
        frac_str = fmt::format("{:0{}}", frac_val, precision);
    }

    size_t pos = fmt.find("%f");
    while (pos != string::npos) {
        fmt.replace(pos, 2, frac_str);
        pos = fmt.find("%f", pos + frac_str.size());
    }

    string out = strftime_expand(fmt, tm_time);
    return out;
}

string time_utils::get_formatted_time(double time, const string& fmt) {
    time_t sec = static_cast<time_t>(time);
    
    // 使用 C++17 的 std::localtime，避免平台差异
    std::tm tm_time;
    auto time_ptr = std::localtime(&sec);
    if (time_ptr) {
        tm_time = *time_ptr;
    } else {
        // 如果获取时间失败，使用当前时间
        time_t now = std::time(nullptr);
        time_ptr = std::localtime(&now);
        if (time_ptr) {
            tm_time = *time_ptr;
        }
    }
    
    return strftime_expand(fmt, tm_time);
}


string time_utils::get_formatted_time(const string &fmt) {
    return time_utils::get_formatted_time(time_utils::get_time(), fmt);
}


string time_utils::get_foramtted_time_with_frac(int precision) {
    return time_utils::get_formatted_time_with_frac(get_time(), "%Y-%m-%d %H:%M:%S", precision);
}

void time_utils::sleep(double seconds) {
    if (seconds <= 0) return;
    this_thread::sleep_for(chrono::duration<double>(seconds));
}

void time_utils::sleep_ms(int milliseconds) {
    if (milliseconds <= 0) return;
    this_thread::sleep_for(chrono::milliseconds(milliseconds));
}

void time_utils::sleep_us(int microseconds) {
    if (microseconds <= 0) return;
    this_thread::sleep_for(chrono::microseconds(microseconds));
}

void sleep_ns(int nanoseconds) {
    if (nanoseconds <= 0) return;
    this_thread::sleep_for(chrono::nanoseconds(nanoseconds));
}

time_utils::StopWatch::StopWatch() {
    start_time = get_time();
    last_lap_time = start_time;
    is_running = true;
}

time_utils::StopWatch::StopWatch(double start_time) {
    this->start_time = start_time;
    last_lap_time = start_time;
    is_running = true;
}

double time_utils::StopWatch::start() {
    if (is_running) {
        printf("当前停表 <StopWatch(%p)> 已经在运行了", this);
        return start_time;
    }
    start_time = get_time();
    last_lap_time = start_time;
    is_running = true;
    return start_time;
}

double time_utils::StopWatch::stop() {
    double now = get_time();
    if (!is_running) {
        printf("当前停表 <StopWatch(%p)> 已经停止了", this);
        return now - start_time;
    }
    last_stop_time = now;
    is_running = false;
    return now - start_time;
}

double time_utils::StopWatch::reset() {
    stop();
    start_time = get_time();
    return start();
}

double time_utils::StopWatch::lap() {
    double now = get_time();
    if (!is_running) {
        printf("当前停表 <StopWatch(%p)> 已经停止了", this);
        return now - last_lap_time;
    }
    double lap_time = now - last_lap_time;
    last_lap_time = now;
    return lap_time;
}

double time_utils::StopWatch::lap_and_stop() {
    double result = lap();
    stop();
    return result;
}

double time_utils::StopWatch::lap_and_print(const ostream& os) {
    double result = lap();
    cout << "自上次计次 " << result << " s" << endl;
    return result;
}

double time_utils::StopWatch::lap_and_reset() {
    double result = lap();
    reset();
    return result;
}
double time_utils::StopWatch::elapsed() {
    return get_time() - start_time;
}

double time_utils::StopWatch::get_start_time() {
    return start_time;
}

double time_utils::StopWatch::get_last_lap_time() {
    return last_lap_time;
}

double time_utils::StopWatch::get_last_stop_time() {
    return last_stop_time;
}

