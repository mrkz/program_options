// Copyright Vladimir Prus 2002-2004.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <boost/config.hpp>

#define BOOST_PROGRAM_OPTIONS_SOURCE
#include <boost/program_options/config.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>
#define DECL BOOST_PROGRAM_OPTIONS_DECL



#include <boost/program_options/detail/cmdline.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/environment_iterator.hpp>
#include <boost/program_options/detail/convert.hpp>
#include <boost/bind.hpp>


#if !defined(__GNUC__) || __GNUC__ < 3
#include <iostream>
#else
#include <istream>
#endif

#ifdef _WIN32
#include <stdlib.h>
#else
#include <unistd.h>
#endif

// It appears that on Mac OS X the 'environ' variable is not
// available to dynamically linked libraries.
#if defined(__APPLE__) && defined(__DYNAMIC__)
#include <crt_externs.h>
static char** environ = *_NSGetEnviron();
#endif

using namespace std;

namespace boost { namespace program_options {

    namespace {
        woption woption_from_option(const option& opt)
        {
            woption result;
            result.string_key = opt.string_key;
            result.position_key = opt.position_key;
            
            std::transform(opt.value.begin(), opt.value.end(),
                           back_inserter(result.value),
                           bind(from_utf8, _1));
            return result;
        }
    }

    basic_parsed_options<wchar_t>
    ::basic_parsed_options(const parsed_options& po)
    : description(po.description),
      utf8_encoded_options(po)
    {
        for (unsigned i = 0; i < po.options.size(); ++i)
            options.push_back(woption_from_option(po.options[i]));
    }

    namespace detail
    {
        void
        parse_command_line(cmdline& cmd, parsed_options& result);
    }

    basic_command_line_parser<char>::
    basic_command_line_parser(const std::vector<std::string>& args)
    : m_style(0), m_desc(0), m_positional(0), m_args(args)
    {}

    basic_command_line_parser<char>::
    basic_command_line_parser(int argc, char* argv[])
    :  m_style(0), m_desc(0), m_positional(0)
#if ! defined(BOOST_NO_TEMPLATED_ITERATOR_CONSTRUCTORS)
    ,m_args(argv+1, argv+argc)
#endif
    {       
#if defined(BOOST_NO_TEMPLATED_ITERATOR_CONSTRUCTORS)
        m_args.reserve(argc);
        copy(argv+1, argv+argc, inserter(m_args, m_args.end()));
#endif
    }
    
    parsed_options 
    basic_command_line_parser<char>::run() const
    {
        parsed_options result(m_desc);
        detail::cmdline cmd(m_args, m_style, true);
        cmd.set_additional_parser(m_ext);

        if (m_desc) {
            set<string> keys = m_desc->primary_keys();
            for (set<string>::iterator i = keys.begin(); i != keys.end(); ++i) {
                const option_description& d = m_desc->find(*i);
                char s = d.short_name().empty() ? '\0' : d.short_name()[0];

                shared_ptr<const value_semantic> vs = d.semantic();
                char flags;
                if (vs->zero_tokens())
                    flags = '|';
                else
                    if (vs->is_implicit()) 
                        if (vs->is_multitoken())
                            flags = '*';
                        else
                            flags = '?';
                    else if (vs->is_multitoken())
                        flags = '+';
                    else flags = ':';

                cmd.add_option(d.long_name(), s, flags, 1);
            }
        }

        parse_command_line(cmd, result);

        if (m_positional)
        {
            unsigned position = 0;
            for (unsigned i = 0; i < result.options.size(); ++i) {
                option& opt = result.options[i];
                if (opt.position_key != -1) {
                    if (position >= m_positional->max_total_count())
                    {
                        throw too_many_positional_options_error(
                            "too much positional options");
                    }
                    opt.string_key = m_positional->name_for_position(position);
                    ++position;
                }
            }
        }

        return result;        
    }

    namespace {
        std::vector<string> utf8_args(
            const std::vector<std::wstring>& args)
        {
            std::vector<string> r;
            r.reserve(args.size());
            transform(args.begin(), args.end(), back_inserter(r),
                      bind(to_utf8, _1));
            return r;
        }

        // This version exists since some compiler crash on 
        // template vector ctor, so we can't just create vector inline
        // and pass it to the above function.
        std::vector<string> utf8_args(int argc, wchar_t* argv[])
        {   
            std::vector<string> r;    
            r.reserve(argc);
            transform(argv + 1, argv +  argc, back_inserter(r),
                      bind(to_utf8, _1));
            return r;
        }
    }

    
    basic_command_line_parser<wchar_t>::
    basic_command_line_parser(const std::vector<std::wstring>& args)
    : m_style(0), m_desc(0), m_positional(0), m_args(utf8_args(args))
    {}

    basic_command_line_parser<wchar_t>::
    basic_command_line_parser(int argc, wchar_t* argv[])
    : m_style(0), m_desc(0), m_positional(0), m_args(utf8_args(argc, argv))
    {}

    wparsed_options 
    basic_command_line_parser<wchar_t>::run() const
    {
        command_line_parser ascii_parser(m_args);
        ascii_parser.style(m_style);
        if (m_desc)
            ascii_parser.options(*m_desc);
        if (m_positional)
            ascii_parser.positional(*m_positional);
        ascii_parser.extra_parser(m_ext);
        
        return wparsed_options(ascii_parser.run());
    }


    namespace detail {
        void
        parse_command_line(cmdline& cmd, parsed_options& result)
        {
            int position(0);
            
            while(++cmd) {
                
                option n;
                
                if (cmd.at_option()) {
                    if (*cmd.option_name().rbegin() != '*') {
                        n.string_key = cmd.option_name();
                    }
                    else {
                        n.string_key = cmd.raw_option_name();
                    }
                    n.value = cmd.option_values();
                } else {
                    n.position_key = position++;
                    n.value.clear();
                    n.value.push_back(cmd.argument());
                }
                result.options.push_back(n);
            }
        }
    }

    template<class charT>
    basic_parsed_options<charT>
    parse_config_file(std::basic_istream<charT>& is, 
                      const options_description& desc)
    {    
        set<string> allowed_options;
        set<string> pm = desc.primary_keys();
        for (set<string>::iterator i = pm.begin(); i != pm.end(); ++i) {
            const option_description& d = desc.find(*i);

            if (d.long_name().empty())
                throw error("long name required for config file");

            allowed_options.insert(d.long_name());
        }

        // Parser return char strings
        parsed_options result(&desc);        
        copy(detail::basic_config_file_iterator<charT>(is, allowed_options), 
             detail::basic_config_file_iterator<charT>(), 
             back_inserter(result.options));
        // Convert char strings into desired type.
        return basic_parsed_options<charT>(result);
    }

    template
    basic_parsed_options<char>
    parse_config_file(std::basic_istream<char>& is, 
                      const options_description& desc);

    template
    basic_parsed_options<wchar_t>
    parse_config_file(std::basic_istream<wchar_t>& is, 
                      const options_description& desc);
    
// This versio, which accepts any options without validation, is disabled,
// in the hope that nobody will need it and we cant drop it altogether.
// Besides, probably the right way to handle all options is the '*' name.
#if 0
    DECL parsed_options
    parse_config_file(std::istream& is)
    {
        detail::config_file_iterator cf(is, false);
        parsed_options result(0);
        copy(cf, detail::config_file_iterator(), 
             back_inserter(result.options));
        return result;
    }
#endif

    DECL parsed_options
    parse_environment(const options_description& desc, 
                      const function1<std::string, std::string>& name_mapper)
    {
        parsed_options result(&desc);
        
        // MinGW unconditionally #defines 'environ',
        // so the following line won't compile.
        // If initialization is in the same line as declaration
        // VC 7.1 parser chokes.
        // Finally, if the variable is called 'environ', then VC refuses
        // to link.
        char **env;
        #if defined(_WIN32) && !defined( __MINGW32__ )
        env = _environ;
        #else
        env = environ;
        #endif
        for(environment_iterator i(environ), e; i != e; ++i) {
            string option_name = name_mapper(i->first);

            if (!option_name.empty()) {
                option n;
                n.string_key = option_name;
                n.value.push_back(i->second);
                result.options.push_back(n);
            }                
        }

        return result;
    }

    namespace {
        class prefix_name_mapper {
        public:
            prefix_name_mapper(const std::string& prefix)
            : prefix(prefix)
            {}

            std::string operator()(const std::string& s)
            {
                string result;
                if (s.find(prefix) == 0) {
                    for(string::size_type n = prefix.size(); n < s.size(); ++n) 
                    {
                        result.push_back(tolower(s[n]));
                    }
                }
                return result;
            }
        private:
            std::string prefix;
        };
    }

    DECL parsed_options
    parse_environment(const options_description& desc, 
                      const std::string& prefix)
    {
        return parse_environment(desc, prefix_name_mapper(prefix));
    }

    DECL parsed_options
    parse_environment(const options_description& desc, const char* prefix)
    {
        return parse_environment(desc, string(prefix));
    }




}}