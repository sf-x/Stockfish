// Copyright 2007 Edd Dawson.
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file BOOST_LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#ifndef CHAR_ARRAY_BUFFER_HPP_1819_13042007_
#define CHAR_ARRAY_BUFFER_HPP_1819_13042007_

#include <streambuf>

class char_array_buffer : public std::streambuf
{
    public:
        char_array_buffer(const char *begin, const char *end);
        explicit char_array_buffer(const char *str);

    private:
        int_type uflow();
        int_type underflow();
        int_type pbackfail(int_type ch);
        std::streamsize showmanyc();
        
        // copy ctor and assignment not implemented;
		// copying not allowed
		char_array_buffer(const char_array_buffer &);
		char_array_buffer &operator= (const char_array_buffer &);

    private:
        const char * const begin_;
        const char * const end_;
        const char * current_;
};

#endif

