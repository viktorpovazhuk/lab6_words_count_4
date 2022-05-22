//
// Created by vityha on 26.04.22.
//

#ifndef COUNTER_STRINGHASHCOMPARE_H
#define COUNTER_STRINGHASHCOMPARE_H

#include <cstdio>
#include <string>
#include <iostream>

struct StringHashCompare {
    static size_t hash( const std::string& x ) {
        return std::hash<std::string>{}(x);
    }
    //! True if strings are equal
    static bool equal( const std::string& x, const std::string& y ) {
        return x==y;
    }
};

#endif //COUNTER_STRINGHASHCOMPARE_H
