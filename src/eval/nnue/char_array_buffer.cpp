// Copyright 2007 Edd Dawson.
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file BOOST_LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#include "char_array_buffer.hpp"

#include <functional>
#include <cassert>
#include <cstring>

char_array_buffer::char_array_buffer(const char *begin, const char *end) :
    begin_(begin),
    end_(end),
    current_(begin_)
{
    assert(std::less_equal<const char *>()(begin_, end_));
}

char_array_buffer::char_array_buffer(const char *str) :
    begin_(str),
    end_(begin_ + std::strlen(str)),
    current_(begin_)
{
}

char_array_buffer::int_type char_array_buffer::underflow()
{
    if (current_ == end_)
        return traits_type::eof();

    return *current_;
}

char_array_buffer::int_type char_array_buffer::uflow()
{
    if (current_ == end_)
        return traits_type::eof();

    return *current_++;
}

char_array_buffer::int_type char_array_buffer::pbackfail(int_type ch)
{
    if (current_ == begin_ || (ch != traits_type::eof() && ch != current_[-1]))
        return traits_type::eof();

    return *--current_;
}

std::streamsize char_array_buffer::showmanyc()
{
    assert(std::less_equal<const char *>()(current_, end_));
    return end_ - current_;
}
