#include "expression.hpp"

#include <cctype>
#include <stdexcept>
#include <sstream>

long long Variables::get(char name) const
{
    switch (name)
    {
    case 'X': return x;
    case 'Y': return y;
    case 'Z': return z;
    default: throw std::runtime_error(std::string("неизвестная переменная: ") + name);
    }
}

void Variables::set(char name, long long value)
{
    switch (name)
    {
    case 'X': x = value; break;
    case 'Y': y = value; break;
    case 'Z': z = value; break;
    default: throw std::runtime_error(std::string("нельзя присвоить переменной: ") + name);
    }
}

std::string Variables::toString() const
{
    std::ostringstream out;
    out << "X=" << x << " Y=" << y << " Z=" << z;
    return out.str();
}

class Parser
{
public:
    Parser(const std::string &source, const Variables &variables)
        : text(source), vars(variables)
    {
    }

    long long parse()
    {
        long long value = parseExpression();
        skipSpaces();
        if (pos != text.size())
        {
            throw std::runtime_error("лишние символы в конце выражения");
        }
        return value;
    }

private:
    const std::string &text;
    const Variables &vars;
    std::size_t pos = 0;

    void skipSpaces()
    {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
        {
            ++pos;
        }
    }

    bool consume(char expected)
    {
        skipSpaces();
        if (pos < text.size() && text[pos] == expected)
        {
            ++pos;
            return true;
        }
        return false;
    }

    long long parseExpression()
    {
        long long value = parseTerm();

        while (true)
        {
            if (consume('+'))
            {
                value += parseTerm();
            }
            else if (consume('-'))
            {
                value -= parseTerm();
            }
            else
            {
                return value;
            }
        }
    }

    long long parseTerm()
    {
        long long value = parseFactor();

        while (true)
        {
            if (consume('*'))
            {
                value *= parseFactor();
            }
            else if (consume('/'))
            {
                long long divisor = parseFactor();
                if (divisor == 0)
                {
                    throw std::runtime_error("деление на ноль");
                }
                value /= divisor;
            }
            else
            {
                return value;
            }
        }
    }

    long long parseFactor()
    {
        skipSpaces();

        if (consume('-'))
        {
            return -parseFactor();
        }

        if (consume('+'))
        {
            return parseFactor();
        }

        if (consume('('))
        {
            long long value = parseExpression();
            if (!consume(')'))
            {
                throw std::runtime_error("не закрыта скобка");
            }
            return value;
        }

        if (pos >= text.size())
        {
            throw std::runtime_error("ожидался операнд");
        }

        char c = text[pos];

        if (c == 'X' || c == 'Y' || c == 'Z')
        {
            ++pos;
            return vars.get(c);
        }

        if (std::isdigit(static_cast<unsigned char>(c)))
        {
            long long value = 0;
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
            {
                value = value * 10 + (text[pos] - '0');
                ++pos;
            }
            return value;
        }

        throw std::runtime_error("ожидался операнд");
    }
};

long long evaluateExpression(const std::string &text, const Variables &vars)
{
    Parser parser(text, vars);
    return parser.parse();
}
