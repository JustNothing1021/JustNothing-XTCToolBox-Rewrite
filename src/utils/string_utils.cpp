#include <typeindex>
#include "utils/string_utils.h"

using namespace std;

// 检测类型T是否可以被fmt格式化
template <typename T, typename = void>
struct is_formattable : false_type {};

template <typename T>
struct is_formattable<T, void_t<decltype(fmt::format("{}", declval<T>()))>> : true_type {};

template <typename, template <typename...> class>
struct is_instance_of_template : false_type {};

template <template <typename...> class T, typename... Args>
struct is_instance_of_template<T<Args...>, T> : true_type {};


string string_utils::demangle(const type_info& ti) {
#ifdef __GNUC__
    // GCC/Clang 平台使用 abi::__cxa_demangle
    int status = -1;
    char* ptr = abi::__cxa_demangle(ti.name(), 0, 0, &status);
    string realname = status == 0 ? ptr : ti.name();
    string realname_n = string_utils::replace(realname, " >", ">");
    free(ptr);
    return realname_n;
#else
    // Windows/MSVC 平台，直接返回类型名
    string realname = ti.name();
    // 简单的清理：移除 class/struct 前缀
    size_t pos = realname.find_last_of(" ");
    if (pos != string::npos) {
        realname = realname.substr(pos + 1);
    }
    return string_utils::replace(realname, " >", ">");
#endif
}

string string_utils::format_any(const any& value, const string& format_str) {
    static const unordered_map<type_index, function<string(const any&, const string&)>> type_formatters = { 
        {typeid(char), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<char>(v)) : fmt::format("{:" + fmt + "}", any_cast<char>(v));
        }},
        {typeid(int), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<int>(v)) : fmt::format("{:" + fmt + "}", any_cast<int>(v));
        }},
        {typeid(short), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<short>(v)) : fmt::format("{:" + fmt + "}", any_cast<short>(v));
        }},
        {typeid(long), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<long>(v)) : fmt::format("{:" + fmt + "}", any_cast<long>(v));
        }},
        {typeid(long long), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<long long>(v)) : fmt::format("{:" + fmt + "}", any_cast<long long>(v));
        }},
        {typeid(unsigned char), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<unsigned char>(v)) : fmt::format("{:" + fmt + "}", any_cast<unsigned char>(v));
        }},
        {typeid(unsigned int), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<unsigned int>(v)) : fmt::format("{:" + fmt + "}", any_cast<unsigned int>(v));
        }},
        {typeid(unsigned short), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<unsigned short>(v)) : fmt::format("{:" + fmt + "}", any_cast<unsigned short>(v));
        }},
        {typeid(unsigned long), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<unsigned long>(v)) : fmt::format("{:" + fmt + "}", any_cast<unsigned long>(v));
        }},
        {typeid(unsigned long long), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<unsigned long long>(v)) : fmt::format("{:" + fmt + "}", any_cast<unsigned long long>(v));
        }},
        {typeid(float), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<float>(v)) : fmt::format("{:" + fmt + "}", any_cast<float>(v));
        }},
        {typeid(double), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<double>(v)) : fmt::format("{:" + fmt + "}", any_cast<double>(v));
        }},
        {typeid(long double), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? to_string(any_cast<long double>(v)) : fmt::format("{:" + fmt + "}", any_cast<long double>(v));
        }},
        {typeid(bool), [](const any& v, const string& fmt) -> string {
            return any_cast<bool>(v) ? "true" : "false";
        }},
        {typeid(string), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? any_cast<string>(v) : fmt::format("{:" + fmt + "}", any_cast<string>(v));
        }},
        {typeid(const char*), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? string(any_cast<const char*>(v)) : fmt::format("{:" + fmt + "}", any_cast<const char*>(v));
        }},
        {typeid(void*), [](const any& v, const string& fmt) -> string {
            return fmt.empty() ? fmt::format("{}", any_cast<void*>(v)) : fmt::format("{:" + fmt + "}", any_cast<void*>(v));
        }}
    };

    try {
        auto it = type_formatters.find(type_index(value.type()));
        if (it != type_formatters.end()) {
            return it->second(value, format_str);
        }
        
        // 处理 vector<any> 类型，避免递归调用
        if (value.type() == typeid(vector<any>)) {
            auto vec = any_cast<vector<any>>(value);
            string result = "[";
            for (size_t i = 0; i < vec.size(); ++i) {
                // 避免递归调用 format_any，直接使用简单的格式化
                if (vec[i].type() == typeid(string)) {
                    result += '"' + any_cast<string>(vec[i]) + '"';
                } else if (vec[i].type() == typeid(int)) {
                    result += to_string(any_cast<int>(vec[i]));
                } else if (vec[i].type() == typeid(double)) {
                    result += to_string(any_cast<double>(vec[i]));
                } else {
                    // 对于复杂类型，使用简单的格式化
                    result += fmt::format("<{}>", demangle(vec[i].type()));
                }
                if (i != vec.size() - 1) {
                    result += ", ";
                }
            }
            result += "]";
            return result;
        }

    } catch (const exception& e) {
        cerr << "格式化对象出错: " << e.what() << endl;
    } catch (...) {
        cerr << "格式化对象出错: 未知错误" << endl;
    }

    // 默认格式化
    try {
        return fmt::format("<{} object at 0x{:016x}>", demangle(value.type()), (uint64_t) (void*) &value);
    } catch (...) {
        return fmt::format("<{} (mangled) object at 0x{:016x}>", value.type().name(), (uint64_t) (void*) &value);
    }
}




string string_utils::format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int size = vsnprintf(nullptr, 0, format, args) + 1; // 计算长度，+1给'\0'留位置
    va_end(args);
    va_start(args, format);
    char* buffer = new char[size];
    vsnprintf(buffer, size, format, args);
    string result(buffer);
    va_end(args);
    delete[] buffer;
    return result;
}



string string_utils::format_with_map(const string& fmtstr, const unordered_map<string, any>& params) {
    static const regex re(R"(\{([a-zA-Z0-9_]+)(?::([^}]+))?\})");
    string result;
    size_t last = 0;
    auto end = sregex_iterator();
    for (auto it = sregex_iterator(fmtstr.begin(), fmtstr.end(), re); it != end; ++it) {
        auto match = *it;
        result.append(fmtstr, last, match.position() - last);
        string key = match[1];
        string fmt = match[2];
        auto param = params.find(key);
        if (param != params.end()) {
            result += format_any(param->second, fmt);
        } else {
            // 未知参数按原样输出
            result += match.str();
        }
        last = match.position() + match.length();
    }
    result.append(fmtstr, last, string::npos);
    return result;
}

vector<string> string_utils::split(const string& str, const string& sep, long long max_split, bool include_sep) {
    vector<string> result;
    size_t split_pos = 0;
    size_t sep_len = sep.length();

    while (max_split-- != 0) {
        size_t sep_pos = str.find(sep, split_pos);
        if (sep_pos == string::npos) {
            result.push_back(str.substr(split_pos));
            if (include_sep) result.push_back(sep);
            split_pos = str.length();
            break;
        }
        result.push_back(str.substr(split_pos, sep_pos - split_pos));
        split_pos = sep_pos + sep_len;
    }
    if (split_pos < str.length()) result.push_back(str.substr(split_pos));
    return result;
}

string string_utils::join(const vector<string>& strs, const string& sep) {
    string result;
    for (vector<string>::const_iterator it = strs.begin(); it != strs.end(); ++it) {
        if (it != strs.begin()) result += sep;
        result += *it;
    }
    return result;
}

string string_utils::replace(const string& str, const string& old_value, const string& new_value, long long max_replace) {
    if (old_value.empty()) return str; // 避免死循环
    string result;
    size_t pos = 0;
    size_t old_len = old_value.length();
    size_t new_len = new_value.length();
    while (max_replace--) {
        size_t found = str.find(old_value, pos);
        if (found == string::npos) break;
        result.append(str, pos, found - pos);
        result += new_value;
        pos = found + old_len;
    }
    result.append(str, pos, str.length() - pos);
    return result;
}



string string_utils::wstr2str(const wstring& wstr) {
    try {
        wstring_convert<codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wstr);
    } catch (const range_error& e) {
        throw runtime_error("将宽字符串转换为字符串时出错: " + string(e.what()));
    }
}

wstring string_utils::str2wstr(const string& str) {
    try {
        wstring_convert<codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(str);
    } catch (const range_error& e) {
        throw runtime_error("将字符串转换为宽字符串时出错: " + string(e.what()));
    }
}


string string_utils::wstr2str(const wchar_t* wstr) {
    return wstr2str(wstring(wstr));
}


wstring string_utils::str2wstr(const char* str) {
    return str2wstr(string(str));
}

string string_utils::ltrim(const string& str, const string& chars) {
    auto it = str.begin();
    while (it != str.end() && chars.find(*it) != string::npos) ++it;
    return string(it, str.end());
}

string string_utils::rtrim(const string& str, const string& chars) {
    auto it = str.end();
    while (it != str.begin() && chars.find(*(it - 1)) != string::npos) --it;
    return string(str.begin(), it);
}

string string_utils::trim(const string& str, const string& chars) {
    return ltrim(rtrim(str, chars), chars);
}

char buffer[4096];
string string_utils::strip(const string& str) {
    if (str.length() >= sizeof(buffer)) {   // 需要动态分配内存
        char *buffer2 = new char[str.length() + 1];
        strcpy_s(buffer2, str.length() + 1, str.c_str());
        char *lptr = buffer2, *rptr = buffer2 + str.length() - 1;
        while (lptr <= rptr && isspace(*lptr)) ++lptr;
        while (rptr >= lptr && isspace(*rptr)) --rptr;
        string result(lptr, rptr + 1);
        delete[] buffer2;
        return result;
    } else {
        strcpy_s(buffer, sizeof(buffer), str.c_str());
        char *lptr = buffer, *rptr = buffer + str.length() - 1;
        while (lptr <= rptr && isspace(*lptr)) ++lptr;
        while (rptr >= lptr && isspace(*rptr)) --rptr;
        return string(lptr, rptr + 1);
    }
} 

string string_utils::remove_invalid_utf8(const string& input, const string& replace_with) {
    string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ) {
        uint8_t c = input[i];
        size_t seq_len = 0;
        // utf-8一般用第一位来标记序列的长度
        if (c <= 0x7F) {                 // 0xxxxxxx
            seq_len = 1;
        } else if ((c & 0xE0) == 0xC0) { // 110xxxxx
            seq_len = 2;
        } else if ((c & 0xF0) == 0xE0) { // 1110xxxx
            seq_len = 3;
        } else if ((c & 0xF8) == 0xF0) { // 11110xxx
            seq_len = 4;
        } else {
            output += replace_with;
            i++;
            continue;
        }

        bool valid = true;
        if (i + seq_len > input.size()) {
            valid = false; // 序列不完整
        } else {
            for (size_t j = 1; j < seq_len; j++) {
                uint8_t next_c = static_cast<uint8_t>(input[i + j]);
                if ((next_c & 0xC0) != 0x80) { // 后续字节格式应为10xxxxxx
                    valid = false;
                    break;
                }
            }
        }

        if (!valid) {
            output += replace_with;
            i++; // 跳过当前无效字节
        } else {
            // 将有效序列添加到输出
            output.append(input, i, seq_len);
            i += seq_len;
        }
    }
    return output;
}


string string_utils::to_hex_view(const string& str, char unprintable, size_t width, bool show_ascii) {
    string result;
    const char* c_str = str.c_str();
    while (*c_str) {
        string line_hex = string_utils::format("[0x%08X] ", c_str - str.c_str());
        string line_ascii;
        for (int i = 0; i < width && *c_str; i++) {
            char c = *c_str++;
            line_hex += string_utils::format("%02X ", static_cast<unsigned char>(c));
            if (show_ascii) line_ascii += (c >= 32 && c < 127) ? c : unprintable;
        }
        if (line_hex.length() < 3 * width + 13) line_hex = fmt::format(string_utils::format("{:<%d}",  3 * width + 13), line_hex);
        result += line_hex + (show_ascii ? " | " + line_ascii : "") + "\n";
    }
    return result;
}


string string_utils::to_hex_view(const char *str, size_t size, char unprintable, size_t width, bool show_ascii) {
    string result;
    const char *c_str = str;
    while (c_str < str + size) {
        string line_hex = string_utils::format("[0x%08X] ", c_str - str);
        string line_ascii;
        for (int i = 0; i < width && c_str < str + size; i++) {
            char c = *c_str++;
            line_hex += string_utils::format("%02X ", static_cast<unsigned char>(c));
            if (show_ascii) line_ascii += (c >= 32 && c < 127) ? c : unprintable;
        }
        if (line_hex.length() < 3 * width + 13) line_hex = fmt::format(string_utils::format("{:<%d}",  3 * width + 13), line_hex);
        result += line_hex + (show_ascii ? " | " + line_ascii : "") + "\n";
    }
    return result;
}

size_t string_utils::find_fully_match(const string& str, const string& pattern, size_t offset, const string& ignores) {
    while (true) {
        size_t res = str.find(pattern, offset);
        if (res == string::npos) return res;
        if (res > 0) {
            char prev = str[res - 1];
            if (isspace(prev) ||
                ignores.find(prev) != string::npos) {
                return res;
            } else {
                return string::npos;
            }
        }
        if (res + pattern.length() < str.length()) {
            char next = str[res + pattern.length()];
            if (isspace(next) ||
                ignores.find(next) != string::npos) {
                return res;
            } else {
                return string::npos;
            }
        }
        return res;
    }
    
}


int64_t string_utils::calc_expr(string expr, const unordered_map<string, int64_t> &vars) {

    string expr_orig = expr;

    // 移除所有空格
    // 注："::isspace"的"::"指的是全局命名空间（确保isspace是C提供的那个，不然有歧义）
    //      而不是当前使用的命名空间（现在using namespace std）
    expr.erase(remove_if(expr.begin(), expr.end(), ::isspace), expr.end());

    map<char, int> opPriority = {
        {'+', 1},
        {'-', 1},
        {'*', 2},
        {'/', 2},
    };

    stack<uint64_t> values;
    stack<char> ops;
    int i;


    function<void()> skipWhiteSpace = [&]() {
        while (i < expr.length() && isspace(expr[i])) i++;
        };

    function<int64_t()> readNextNumber = [&]() -> int64_t {
        skipWhiteSpace();
        int64_t result = 0;
        bool negative = false;
        if (expr[i] == '-') {
            negative = true;
            i++;
        } else if (expr[i] == '+') {
            i++;
        }
        if (i + 2 < expr.length()) {
            if (expr[i] == '0' && expr[i + 1] != 'x') {
                // 八进制数据
                char* end;
                result = strtoll(&expr[i], &end, 8);
                i = end - expr.c_str();
                return negative ? -result : result;
            } else if (expr[i] == '0' && expr[i + 1] == 'x') {
                // 十六进制数据
                char* end;
                result = strtoll(&expr[i], &end, 16);
                i = end - expr.c_str();
                return negative ? -result : result;
            }
        }
        char* end;
        result = strtoll(&expr[i], &end, 10);
        i = end - expr.c_str();
        return negative ? -result : result;
    };

    function<int64_t(int64_t, char, int64_t)> applyOp = [&](int64_t a, char op, int64_t b) -> int64_t {
        switch (op) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/':
                if (b == 0) {
                    throw runtime_error("以零为除数的错误");
                }
                return a / b;
            default:
                throw runtime_error(fmt::format("未知操作符: {}", op));
        }
    };

    function<bool(char)> isOperator = [&](char c) -> bool {
        return opPriority.find(c) != opPriority.end();
    };

    // 处理变量
    for (auto& [name, value] : vars) {
        while (true) {
            size_t pos = string_utils::find_fully_match(expr, name, 0, "+-*/");
            if (pos == string::npos) break;
            expr.replace(pos, name.length(), to_string(value));
        }
    }

    // 处理表达式
    for (i = 0; i < expr.length(); ) {



        if (    // 如果你们注释没有对齐的话开下参数补全试试
               (values.empty() && expr[i] == '-')                          // 如果在开头
            || (i > 0 && expr[i] == '-' && isOperator(expr[i - 1]) // 或者前一个字符是操作符（也就是这个减号是负数）
            || (i > 0 && expr[i] == '-' && expr[i - 1] == '(')            // 如果前面是括号                                          
            )) { // 特判开头的负数，不然会当成减号
            values.push(readNextNumber());
        }

        else if (!(isdigit(expr[i]) || isOperator(expr[i]) || expr[i] == '(' || expr[i] == ')')) {
            throw runtime_error("表达式\"" + expr_orig + "\"包含非法字符/未定义变量，位于第" + to_string(i) + "个字符");
        } else if (isOperator(expr[i])) {
            // 这里是个易爆点
            // 一定要要求isOperator(expr[i])，再接着运算
            // 因为opPriority[ops.top()]的访问在!isOperator(ops.top())的时候
            // 会让非运算符在opPriority里面被赋予初始值0
            // 因为map在访问到不存在的key的时候会自动创建一个值为默认值的key
            // 然后就会导致非操作符被当作操作符处理，出现操作数不足的错误
            // 这个while的条件表达式其实是利用了c--的短路求值特性
            // 这一串与计算只要有一个为false程序就就不会再计算后面的了
            // 所以就不会改变opPriority的内容了
            // （c--竟然没有KeyError，太阴了）
            while (!ops.empty() && isOperator(ops.top()) && opPriority[ops.top()] >= opPriority[expr[i]]) {
                if (values.size() < 2) {
                    throw runtime_error("表达式\"" + expr_orig + "\"格式错误，整理优先级计算时操作数不足");
                }
                int64_t b = values.top();
                values.pop();
                int64_t a = values.top();
                values.pop();
                char op = ops.top();
                ops.pop();
                int64_t result;
                try {
                    result = applyOp(a, op, b);
                } catch (const runtime_error& e) {
                    throw runtime_error("表达式\"" + expr + "\"出现计算错误，位于第" + to_string(i) + "个字符: " + e.what());
                }
                values.push(result);
            }
            ops.push(expr[i]);
            i++;
        } else if (expr[i] == '(') {

            ops.push(expr[i]);
            i++;

        } else if (expr[i] == ')') {
            while (!ops.empty() && ops.top() != '(') {
                if (values.size() < 2) {
                    throw runtime_error("表达式\"" + expr_orig + "\"格式错误，合并括号时操作数不足");
                }
                int64_t b = values.top();
                values.pop();
                int64_t a = values.top();
                values.pop();
                char op = ops.top();
                ops.pop();
                int64_t result = applyOp(a, op, b);
                values.push(result);
            }
            if (ops.empty() || ops.top() != '(') {
                throw runtime_error("表达式\"" + expr + "\"括号不匹配，位于第" + to_string(i) + "个字符");
            }

            ops.pop();
            i++;

        } else {
            values.push(readNextNumber());
        }
    }
    while (!ops.empty()) {
        if (ops.top() == '(') {
            throw runtime_error("表达式\"" + expr + "\"括号不匹配");
        }
        if (values.size() < 2) {
            throw runtime_error("表达式\"" + expr + "\"格式错误，合并结果时操作数不足");
        }
        int64_t b = values.top();
        values.pop();
        int64_t a = values.top();
        values.pop();
        char op = ops.top();
        ops.pop();
        int64_t result = applyOp(a, op, b);
        values.push(result);
    }
    if (values.size() > 1) {
        throw runtime_error("表达式\"" + expr + "\"格式错误，操作数过多");
    }
    if (values.empty()) {
        throw runtime_error("表达式\"" + expr + "\"为空");
    }
    return values.top();
}