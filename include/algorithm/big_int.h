
#pragma once

#ifndef BIG_INT_H
#define BIG_INT_H

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

namespace algorithm {
    class big_int {
    public:
        big_int();
        big_int(const big_int& other);
        big_int(const std::string& str);
        big_int(const int num);
        ~big_int();

        big_int& operator=(const big_int& other);
        big_int& operator=(const std::string& str);
        big_int& operator=(const int num);

        big_int& operator++();
        big_int& operator--();
        big_int operator++(int);
        big_int operator--(int);


        big_int operator+(const big_int& other);
        big_int operator-(const big_int& other);
        big_int operator*(const big_int& other);
        big_int operator/(const big_int& other);
        big_int operator%(const big_int& other);
        big_int operator&(const big_int& other);
        big_int operator|(const big_int& other);
        big_int operator^(const big_int& other);
        big_int operator<<(const big_int& other);
        big_int operator>>(const big_int& other);
        big_int operator~();
        
        big_int& operator+=(const big_int& other);
        big_int& operator-=(const big_int& other);
        big_int& operator*=(const big_int& other);
        big_int& operator/=(const big_int& other);
        big_int& operator%=(const big_int& other);
        big_int& operator&=(const big_int& other);
        big_int& operator|=(const big_int& other);
        big_int& operator^=(const big_int& other);
        big_int& operator<<=(const big_int& other);
        big_int& operator>>=(const big_int& other);


        bool operator==(const big_int& other);  
        bool operator!=(const big_int& other);
        bool operator<(const big_int& other);
        bool operator>(const big_int& other);
        bool operator<=(const big_int& other);
        bool operator>=(const big_int& other);

        friend std::ostream& operator<<(std::ostream& os, const big_int& num);
        friend std::istream& operator>>(std::istream& is, big_int& num);
        friend std::string std::to_string(algorithm::big_int& num);


        std::vector<int> digits;
        bool negative;

    };
};



#endif // BIG_INT_H