#pragma once

#include <cstdint>
#include <map>
#include <string>

struct Variables
{
    long long x = 0;
    long long y = 0;
    long long z = 0;

    long long get(char name) const;
    void set(char name, long long value);
    std::string toString() const;
};

long long evaluateExpression(const std::string &text, const Variables &vars);
