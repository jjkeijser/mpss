/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
*/

#include <gtest/gtest.h>
#include <stdexcept>
#include <ostream>
#include <sstream>
#include "MicException.hpp"
#include "EvaluateArgumentCallbackInterface.hpp"
#include "CliParser.hpp"
#include "CliParserImpl.hpp"
#include "MicOutputImpl.hpp"
#include "MicOutput.hpp"
#include "ConsoleOutputFormatter.hpp"

namespace micmgmt
{
    using namespace std;

    //#########################################################################
    //# Common Classes and Data Used by Tests
    namespace
    {
        class EvaluatorCliParserImpl1 : public EvaluateArgumentCallbackInterface
        {
        public:
            virtual bool checkArgumentValue(EvaluateArgumentType type,
                                            const std::string& typeName,
                                            const std::string& subcommandOrOptionName,
                                            const std::string& value)
            {
                ostringstream s;
                EvaluateArgumentType tmp1 = type;
                std::string tmp2 = typeName;
                std::string tmp3 = subcommandOrOptionName;
                std::string tmp4 = value;
                s << tmp1 << tmp2 << tmp3 << tmp4 << endl;
                return false;
            }
        };

        char* simpleArglist[] = {
            (char*)"Test Program Name"
        };

        vector<string> good_help = { "paragraph 1  ", " paragraph    2", "  paragraph         3 \n " };
        vector<string> empty_help = {};
        vector<string> bad_help = { "   \t \n ", "paragraph 2", "paragraph 3" };
        vector<string> section_help = { "section paragraph 1", "section paragraph 2" };

        char* arglist1[] = {
            (char*)"tool",
            (char*)"--first=greg",
            (char*)"--switch",
            (char*)"-l",
            (char*)"doe",
            (char*)"file1",
            (char*)"file2",
            (char*)"file3",
            (char*)"--verbose",
            (char*)"file4",
            (char*)"file5",
            (char*)"file6",
            (char*)"file7",
        };

        char* arglist2[] = {
            (char*)"tool",
            (char*)"--first=greg",
            (char*)"--switch",
            (char*)"file1",
            (char*)"-l",
            (char*)"doe",
            (char*)"file2",
            (char*)"--verbbose",
            (char*)"file3",
            (char*)"file4"
        };

        char* arglist3[] = {
            (char*)"tool",
            (char*)"--first",
            (char*)"--switch",
            (char*)"file1",
            (char*)"-l",
            (char*)"doe",
            (char*)"file2",
            (char*)"file3",
            (char*)"file4"
        };

        char* arglist3_5[] = {
            (char*)"tool",
            (char*)"--first=james",
            (char*)"--switch=30",
            (char*)"file1",
            (char*)"-l",
            (char*)"doe",
            (char*)"file2",
            (char*)"file3",
            (char*)"file4"
        };

        const char* argErr1 = "Error: tool: An unknown long option specified on the command line: 'verbbose'\n\
Please use 'tool --help' to get help.\n\nUsage:\n    tool --version\n    tool --help\n    tool [OPTIONS] [argument [...]]\n";

        const char* argErr2 = "Error: tool: A required option argument is missing from the command line:\n\
'first'\n\
\n\
Option:\n\
    -f, --first <firstName>\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        firstName:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n";

        const char* argErr3 = "Error: tool: Parser error: There are not enough tool positional arguments on\nthe command line\n\
Please use 'tool --help' to get help.\n\nUsage:\n    tool --version\n    tool --help\n    tool [OPTIONS] <file1> <file2> <file3>\n";

        const char* argErr4 = "Error: tool: Parser error: There are too many tool positional arguments on the\ncommand line\n\
Please use 'tool --help' to get help.\n\nUsage:\n    tool --version\n    tool --help\n    tool [OPTIONS] <file1> <file2> <file3>\n";

        const char* optionErr1 = "Error: tool: An unknown short option specified on the command line 'a'\n\
\n\
Usage:\n\
    tool --version\n\
    tool --help\n\
    tool copy --help\n\
    tool copy [OPTIONS] [argument [...]]\n\
    tool move --help\n\
    tool move [OPTIONS] [argument [...]]\n";

        const char* optionErr2 = "Error: tool: The tool's command line parsing expects \
the first non-option\nargument to be a subcommand\n\
\n\
Usage:\n\
    tool --version\n\
    tool --help\n\
    tool copy --help\n\
    tool copy [OPTIONS] [argument [...]]\n\
    tool move --help\n\
    tool move [OPTIONS] [argument [...]]\n";

        const char* optionErr3 = "Error: tool: An option has an unexpected argument: option='--switch',\nargument='30'\n\
\n\
Option:\n\
    -s, --switch\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n";

        char* arglist4[] = {
            (char*)"tool",
            (char*)"--first=john,paul,ringo",
            (char*)"--switch",
            (char*)"file1",
            (char*)"-l",
            (char*)"doe",
            (char*)"file2",
            (char*)"file3",
            (char*)"file4"
        };

        char* arglist5[] = {
            (char*)"tool",
            (char*)"-v",
            (char*)"-s",
            (char*)"copy",
            (char*)"file1",
            (char*)"file2",
            (char*)"file3",
            (char*)"fileN",
            (char*)"folder",
            (char*)"-a"
        };

        char* arglist5_5[] = {
            (char*)"tool",
            (char*)"-v",
            (char*)"-s",
            (char*)"file1",
            (char*)"file2",
            (char*)"file3",
            (char*)"fileN",
            (char*)"folder",
            (char*)"-a"
        };

        char* arglist5_7[] = {
            (char*)"tool",
            (char*)"copy",
            (char*)"-v",
            (char*)"-s",
            (char*)"file1",
            (char*)"file2",
            (char*)"file3"
        };

        char* arglist5_8[] = {
            (char*)"tool",
            (char*)"-v",
            (char*)"-s"
        };

        const char* subErr1 = "\
Error: tool: Parser error: There are too many positional arguments on the\ncommand line for the subcommand: 'copy'\n\n\
Subcommand:\n\
    copy <source> <dest>\n\
        paragraph 1\n\n\
        paragraph 2\n\n\
        paragraph 3\n\n\
        <source>:\n\
            paragraph 1\n\n\
            paragraph 2\n\n\
            paragraph 3\n\n\
        <dest>:\n\
            paragraph 1\n\n\
            paragraph 2\n\n\
            paragraph 3\n";

        const char* subErr1_1 = "\
Error: tool: A subcommand was expected on the command line but was missing\n\n\
Usage:\n\
    tool --version\n\
    tool --help\n\
    tool copy --help\n\
    tool copy [OPTIONS] [argument [...]]\n\
    tool move --help\n\
    tool move [OPTIONS] [argument [...]]\n";

        char* optTest1[] = {
            (char*)"tool",
            (char*)"-o",
            (char*)"arg"
        };

        char* optTest2[] = {
            (char*)"tool",
            (char*)"--option",
            (char*)"arg"
        };

        char* optTest3[] = {
            (char*)"tool",
            (char*)"--option=arg"
        };

        char* subcommand1[] = {
            (char*)"tool",
            (char*)"copy"
        };

        char* subcommand2[] = {
            (char*)"tool",
            (char*)"copyy"
        };

        char* version1[] = {
            (char*)"tool",
            (char*)"--version",
            (char*)"argument"
        };

        char* help1[] = {
            (char*)"tool",
            (char*)"--help",
            (char*)"subcommand",
            (char*)"argument",
        };

        char* help2[] = {
            (char*)"tool",
            (char*)"-h",
            (char*)"subcommand",
            (char*)"argument",
        };

        const char* goldenParse1 = "tool version 1.2.3\n\
Copyright (C) 2014, Intel Corporation.\n\
\n\
Usage:\n\
    tool --version\n\
    tool --help\n\
    tool subcommand --help\n\
    tool subcommand [OPTIONS] [argument [...]]\n\
\n\
Description:\n\
    Brief description.\n\
\n\
Subcommands:\n\
    subcommand [argument [...]]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
Options:\n\
    -h, --help\n\
        Display this help.\n\
\n\
    --version\n\
        Display tool version.\n\
\n";

        const char* goldenParse2 = "tool version 1.2.3\n\
Copyright (C) 2014, Intel Corporation.\n\
\n\
Usage:\n\
    tool subcommand --help\n\
    tool subcommand [OPTIONS] [argument [...]]\n\
\n\
Subcommands:\n\
    subcommand [argument [...]]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n";

        const char* goldenParse3 = "Error: tool: Parser error: The '--help' option can only be used on the command\n\
line by itself or with a subcommand\n\
\n\
Option:\n    -h, --help\n        Display this help.\n";

        char* cmdlist1[] = {
            (char*)"tool",
            (char*)"--first=jon",
            (char*)"-l",
            (char*)"doe",
            (char*)"--switch"
        };

        char* cmdlist2[] = {
            (char*)"tool",
            (char*)"--first=jon",
            (char*)"-l",
            (char*)"doe",
            (char*)"--verbose",
            (char*)"high"
        };

        char* freeList1[] = {
            (char*)"tool",
            (char*)"arg1",
            (char*)"arg2",
            (char*)"arg3",
            (char*)"arg4",
            (char*)"arg5"
        };

        char* freeList2[] = {
            (char*)"tool",
            (char*)"arg1",
            (char*)"--switch",
            (char*)"arg2",
            (char*)"arg3",
            (char*)"arg4",
            (char*)"arg5"
        };

        char* toolList1[] = {
            (char*)"tool",
            (char*)"arg1",
            (char*)"--verbose",
            (char*)"arg2",
            (char*)"arg3",
            (char*)"arg4",
            (char*)"arg5"
        };

        char* toolList2[] = {
            (char*)"tool",
            (char*)"arg1",
            (char*)"--switch",
            (char*)"arg2",
            (char*)"arg3",
            (char*)"arg4",
            (char*)"--verbose"
        };

        char* subList1[] = {
            (char*)"tool",
            (char*)"other",
            (char*)"arg1",
            (char*)"--switch",
            (char*)"arg2",
            (char*)"arg3",
            (char*)"arg4",
            (char*)"--verbose"
        };

        char* helpList1[] = {
            (char*)"tool",
            (char*)"--help",
        };

        const char* goldenHelp1 = "\
tool version 1.2.3\n\
Copyright (C) 2014, Intel Corporation.\n\
\n\
Usage:\n\
    tool --version\n\
    tool --help\n\
    tool [OPTIONS] [argument [...]]\n\
\n\
Description:\n\
    Brief description.\n\
\n\
Options:\n\
    -h, --help\n\
        Display this help.\n\
\n\
    -s, --switch\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    -t, --target [file]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        file:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n\
    --verbose\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    --version\n\
        Display tool version.\n\
\n";
        const char* goldenHelp2 = "\
tool version 1.2.3\n\
Copyright (C) 2014, Intel Corporation.\n\
\n\
Usage:\n\
    tool --version\n\
    tool --help\n\
    tool [OPTIONS] <mode> <source> <destination>\n\
\n\
Description:\n\
    Brief description.\n\
\n\
Positional Arguments:\n\
    <mode>:\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    <source>:\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    <destination>:\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
\n\
Options:\n\
    -h, --help\n\
        Display this help.\n\
\n\
    -s, --switch\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    -t, --target [file]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        file:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n\
    --verbose\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    --version\n\
        Display tool version.\n\
\n";

        const char* goldenHelp3 = "\
tool version 1.2.3\n\
Copyright (C) 2014, Intel Corporation.\n\
\n\
Usage:\n\
    tool --version\n\
    tool --help\n\
    tool copy --help\n\
    tool copy [OPTIONS] [argument [...]]\n\
    tool move --help\n\
    tool move [OPTIONS] [argument [...]]\n\
    tool other --help\n\
    tool other [OPTIONS] <sourceFile> <destFile>\n\
\n\
Description:\n\
    Brief description.\n\
\n\
Subcommands:\n\
    copy [argument [...]]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    move [argument [...]]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    other <sourceFile> <destFile>\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        <sourceFile>:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n\
        <destFile>:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n\
Options:\n\
    -h, --help\n\
        Display this help.\n\
\n\
    -s, --switch\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    -t, --target [file]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        file:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n\
    --verbose\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    --version\n\
        Display tool version.\n\
\n";

        const char* goldenHelp3_5 = "\
tool version 1.2.3\n\
Copyright (C) 2014, Intel Corporation.\n\
\n\
Usage:\n\
    tool --version\n\
    tool --help\n\
    tool copy --help\n\
    tool copy [OPTIONS] [argument [...]]\n\
    tool move --help\n\
    tool move [OPTIONS] [argument [...]]\n\
    tool other --help\n\
    tool other [OPTIONS] <sourceFile> <destFile>\n\
\n\
Description:\n\
    Brief description.\n\
\n\
Subcommands:\n\
    copy [argument [...]]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    move [argument [...]]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    other <sourceFile> <destFile>\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        <sourceFile>:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n\
        <destFile>:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n\
Options:\n\
    -h, --help\n\
        Display this help.\n\
\n\
    -s, --switch\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    -t, --target [file]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        file:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n\
    --verbose\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
    --version\n\
        Display tool version.\n\
\n\
My Help Section:\n\
    section paragraph 1\n\
\n\
    section paragraph 2\n\
\n";

        char* helpList2[] = {
            (char*)"tool",
            (char*)"copy",
            (char*)"--help",
        };

        char* helpList3[] = {
            (char*)"tool",
            (char*)"move",
            (char*)"--help",
        };

        char* helpList4[] = {
            (char*)"tool",
            (char*)"other",
            (char*)"--help",
        };

        const char* goldenHelp4 = "\
tool version 1.2.3\n\
Copyright (C) 2014, Intel Corporation.\n\
\n\
Usage:\n\
    tool copy --help\n\
    tool copy [OPTIONS] [argument [...]]\n\
\n\
Subcommands:\n\
    copy [argument [...]]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n";

        const char* goldenHelp5 = "\
tool version 1.2.3\n\
Copyright (C) 2014, Intel Corporation.\n\
\n\
Usage:\n\
    tool move --help\n\
    tool move [OPTIONS] [argument [...]]\n\
\n\
Subcommands:\n\
    move [argument [...]]\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n";

        const char* goldenHelp6 = "tool version 1.2.3\n\
Copyright (C) 2014, Intel Corporation.\n\
\n\
Usage:\n\
    tool other --help\n\
    tool other [OPTIONS] <sourceFile> <destFile>\n\
\n\
Subcommands:\n\
    other <sourceFile> <destFile>\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        <sourceFile>:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n\
        <destFile>:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n\
\n";

        char* shortList1[] = {
            (char*)"tool",
            (char*)"-svt",
            (char*)"myFile"
        };

        char* shortList2[] = {
            (char*)"tool",
            (char*)"-stv",
            (char*)"myFile"
        };

        const char* shortErr1 = "Error: tool: A required option argument is missing from the command line:\n'target'\n\
\n\
Option:\n\
    -t, --target <file>\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        file:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n";

        const char* shortErr2 = "Error: tool: A short option was not the last option in a group of short options\n\
but requires a argument, this is not allowed: 't'\n\
\n\
Option:\n\
    -t, --target <file>\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        file:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n";

        char* longList1[] = {
            (char*)"tool",
            (char*)"--target",
            (char*)"--switch"
        };

        char* longList2[] = {
            (char*)"tool",
            (char*)"--target",
            (char*)"-switch"
        };

        const char* longErr1 = "Error: tool: A required option argument is missing from the command line:\n\
'target'\n\
\n\
Option:\n\
    -t, --target <file>\n\
        paragraph 1\n\
\n\
        paragraph 2\n\
\n\
        paragraph 3\n\
\n\
        file:\n\
            paragraph 1\n\
\n\
            paragraph 2\n\
\n\
            paragraph 3\n";

        const char* longErr2 = "Error: tool: An unknown short option specified on the command line 'w'\n\
Please use 'tool --help' to get help.\n\nUsage:\n    tool --version\n    tool --help\n    tool [OPTIONS] [argument [...]]\n";

        char* listArg1[] = {
            (char*)"tool",
            (char*)"--device=mic1,mic3,mic0,mic5-mic9,mic2"
        };

        char* listArg2[] = {
            (char*)"tool",
            (char*)"--device=mic1,mic3, mic0,mic5-mic9,mic2"
        };

        char* listArg3[] = {
            (char*)"tool",
            (char*)"--device=mic1,mic3,mic0,mic5+mic9,mic2"
        };

        char* listArg4[] = {
            (char*)"tool",
            (char*)"--device=mic-1,mic3,mic0,mic5-mic9,mic2"
        };

        char* listArg5[] = {
            (char*)"tool",
            (char*)"--device=mic1,mic3,mic0,mc5-mic9,mic2"
        };

        char* listArg6[] = {
            (char*)"tool",
            (char*)"--device"
        };

        string dumpList(vector<string> list)
        {
            if (list.empty() == true)
            {
                return "(empty)";
            }
            string rv = "{ ";
            for (auto it = list.begin(); it != list.end(); ++it)
            {
                if (it != list.begin())
                {
                    rv += ", ";
                }
                rv += *it;
            }
            rv += " }";
            return rv;
        }

        char* listPosArg1[] = {
            (char*)"tool",
            (char*)"start",
            (char*)"mic1,mic3,mic0,mic5-mic9,mic2"
        };

        char* listPosArg1_5[] = {
            (char*)"tool",
            (char*)"start",
            (char*)"1,3,0,5-9,2"
        };

        char* listPosArg2[] = {
            (char*)"tool",
            (char*)"start",
            (char*)"mic1,mic3, mic0,mic5-mic9,mic2"
        };

        char* listPosArg3[] = {
            (char*)"tool",
            (char*)"start",
            (char*)"mic1,mic3,mic0,mic5+mic9,mic2"
        };

        char* listPosArg4[] = {
            (char*)"tool",
            (char*)"start",
            (char*)"mic-1,mic3,mic0,mic5-mic9,mic2"
        };

        char* listPosArg5[] = {
            (char*)"tool",
            (char*)"start",
            (char*)"mic1,mic3,mic0,mc5-mic9,mic2"
        };

        char* listPosArg6[] = {
            (char*)"tool",
            (char*)"start",
            (char*)"john,paul,george,ringo"
        };

        char* listPosArg24[] = {
            (char*)"tool",
            (char*)"arg1",
            (char*)"arg2",
            (char*)"arg3"
        };

        string goldenHelp24 = "tool version 1.2.3\nCopyright (C) 2014, Intel Corporation.\n\nUsage:\n    tool --version\n    tool --help\n    \
tool [OPTIONS] <arg1> <arg2> <arg3>\n\nDescription:\n    Description.\n\nPositional Arguments:\n    <arg1>:\n        Arg Help\n\n    \
<arg2>:\n        Arg Help\n\n    <arg3>:\n        Arg Help\n\n\nOptions:\n    -h, --help\n        Display this help.\n\n    \
--version\n        Display tool version.\n\n";
        string goldenVersion24 = "tool version 1.2.3\nCopyright (C) 2014, Intel Corporation.\n";

    }; // empty namespace

    //#########################################################################
    //# CliParserImpl Tests
    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_001) // Class Constructor
    {
        // Function no loger throws errors in this case
        EXPECT_THROW(CliParserImpl(0, NULL, "1.0", "2014", "testprogram", "Short description present."), MicException);
        //EXPECT_THROW(CliParserImpl(1, simpleArglist, "  \n \t \f \r ", "2014", "testprogram", ""), MicException);
        //EXPECT_THROW(CliParserImpl(1, simpleArglist, "1.0", "  \n \t \f \r ", "testprogram", ""), MicException);

        EXPECT_NO_THROW(CliParserImpl(1, simpleArglist, "1.0", "2014", "  \n \t \f \r ", ""));
        EXPECT_NO_THROW(CliParserImpl(1, simpleArglist, "testprogram", "2014", "1.0", "Short description present."));
        EXPECT_NO_THROW(CliParserImpl(1, simpleArglist, " testprogram  ", "\t2014\n ", "     1.0 ", "         Short  description   present.         "));
        EXPECT_NO_THROW(CliParserImpl(1, simpleArglist, "testprogram", "2014", "1.0", ""));

        CliParser parser(1, simpleArglist, "1.0", "2014", "", "\tShort         description    present.\n");
        EXPECT_STREQ("Test Program Name", parser.toolName().c_str());
        EXPECT_EQ((unsigned int)2, parser.impl_->optionDefinitions_.size());

        EXPECT_TRUE(parser.impl_->optionDefinitions_.end() != parser.impl_->optionDefinitions_.find("help"));
        EXPECT_TRUE(parser.impl_->optionDefinitions_.end() != parser.impl_->optionDefinitions_.find("version"));

        EXPECT_STREQ(simpleArglist[0], parser.impl_->programName_.c_str());
        EXPECT_STREQ("1.0", parser.impl_->version_.c_str());

        string golden = "Copyright (C) 2014, Intel Corporation.";
        EXPECT_STREQ(golden.c_str(), parser.impl_->copyright_.c_str());
        EXPECT_STREQ("Short description present.", parser.impl_->description_.c_str());
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_002) // Method addOption
    {
        EvaluatorCliParserImpl1 eval = EvaluatorCliParserImpl1();
        CliParserImpl parser(1, simpleArglist, "1.0", "2015", "", "Short description present.");

        EXPECT_THROW(parser.addOption("\n ", good_help, 0, false, "", empty_help, "", NULL), MicException);
        EXPECT_THROW(parser.addOption("opt", empty_help, 'o', false, "", empty_help, "", NULL), MicException);
        EXPECT_THROW(parser.addOption("opt", bad_help, 'o', false, "", empty_help, "", NULL), MicException);
        EXPECT_THROW(parser.addOption("opt", good_help, 0, true, "", good_help, "", NULL), MicException);
        EXPECT_THROW(parser.addOption("opt", good_help, 'o', true, "", good_help, "", NULL), MicException);
        EXPECT_THROW(parser.addOption("opt", good_help, 'o', true, "\treqArgName \n", empty_help, "", &eval), MicException);

        ASSERT_NO_THROW(parser.addOption("opt1", good_help, 'o', false, "", empty_help, "", NULL));
        ASSERT_NO_THROW(parser.addOption("\t opt2\n", good_help, 0, false, "", empty_help, "", &eval));
        ASSERT_NO_THROW(parser.addOption("\t opt3\n", good_help, 'O', false, "reqArg", good_help, "default", NULL));
        ASSERT_NO_THROW(parser.addOption("\t opt4\n", good_help, 'p', true, "reqArg", good_help, "", &eval));

        ASSERT_EQ((unsigned int)6, parser.optionDefinitions_.size());
        EXPECT_NE(parser.optionDefinitions_.end(), parser.optionDefinitions_.find("help"));
        EXPECT_NE(parser.optionDefinitions_.end(), parser.optionDefinitions_.find("version"));
        EXPECT_NE(parser.optionDefinitions_.end(), parser.optionDefinitions_.find("opt1"));
        EXPECT_NE(parser.optionDefinitions_.end(), parser.optionDefinitions_.find("opt2"));
        EXPECT_NE(parser.optionDefinitions_.end(), parser.optionDefinitions_.find("opt3"));
        EXPECT_EQ(parser.optionDefinitions_.end(), parser.optionDefinitions_.find("opt"));
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_003) // Method addToolPositionalArg
    {
        EvaluatorCliParserImpl1 eval = EvaluatorCliParserImpl1();
        CliParserImpl parser(1, simpleArglist, "1.0", "2017", "", "Short description present.");

        EXPECT_THROW(parser.addToolPositionalArg("", good_help, NULL), MicException);
        EXPECT_THROW(parser.addToolPositionalArg("\n   \t\n ", good_help, NULL), MicException);
        EXPECT_THROW(parser.addToolPositionalArg(" posArgName\r", empty_help, NULL), MicException);
        EXPECT_THROW(parser.addToolPositionalArg(" posArgName\r", bad_help, NULL), MicException);

        ASSERT_NO_THROW(parser.addToolPositionalArg(" posArgName1\r", good_help, NULL));
        EXPECT_THROW(parser.addToolPositionalArg(" posArgName1\r", good_help, &eval), MicException);
        ASSERT_NO_THROW(parser.addToolPositionalArg(" posArgName2\r", good_help, &eval));

        EXPECT_EQ((unsigned int)2, parser.toolPositionalDefinitions_.size());
        EXPECT_STREQ("posArgName1", parser.toolPositionalDefinitions_.at(0).name().c_str());
        EXPECT_STREQ("posArgName2", parser.toolPositionalDefinitions_.at(1).name().c_str());
        EXPECT_TRUE(parser.toolPositionalDefinitions_.at(0).isRequired());
        EXPECT_TRUE(parser.toolPositionalDefinitions_.at(1).isRequired());
        EXPECT_EQ(EvaluateArgumentType::ePositionalArgument, parser.toolPositionalDefinitions_.at(0).type());
        EXPECT_EQ(EvaluateArgumentType::ePositionalArgument, parser.toolPositionalDefinitions_.at(1).type());

        EXPECT_TRUE(parser.toolPositionalDefinitions_.at(0).checkArgumentValue(""));
        EXPECT_FALSE(parser.toolPositionalDefinitions_.at(1).checkArgumentValue(""));
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_004) // Method addSubcommand
    {
        EvaluatorCliParserImpl1 eval = EvaluatorCliParserImpl1();
        CliParserImpl parser(1, simpleArglist, "1.0", "2101", "", "Short description present.");

        EXPECT_THROW(parser.addSubcommand("", good_help), MicException);
        EXPECT_THROW(parser.addSubcommand("\n   \t\n ", good_help), MicException);
        EXPECT_THROW(parser.addSubcommand(" sub \n \t\n", empty_help), MicException);
        EXPECT_THROW(parser.addSubcommand(" sub \n \t\n", bad_help), MicException);

        ASSERT_NO_THROW(parser.addSubcommand(" sub1 \n \t\n", good_help));
        EXPECT_THROW(parser.addSubcommand(" sub1 \n \t\n", good_help), MicException);
        ASSERT_NO_THROW(parser.addSubcommand("sub2", good_help));

        EXPECT_EQ((unsigned int)2, parser.subcommandDefinitions_.size());
        EXPECT_TRUE(parser.subcommandDefinitions_.end() != parser.subcommandDefinitions_.find("sub1"));
        EXPECT_TRUE(parser.subcommandDefinitions_.end() != parser.subcommandDefinitions_.find("sub2"));
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_005) // Method addSubcommandPositionalArg
    {
        EvaluatorCliParserImpl1 eval = EvaluatorCliParserImpl1();
        CliParserImpl parser(1, simpleArglist, "1.0", "2014", "", "Short description present.");

        ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
        EXPECT_THROW(parser.addSubcommandPositionalArg("", "source", good_help, NULL), MicException);
        EXPECT_THROW(parser.addSubcommandPositionalArg(" \t \f \r\r\r\n ", "source", good_help, NULL), MicException);
        EXPECT_THROW(parser.addSubcommandPositionalArg("copy", "", good_help, NULL), MicException);
        EXPECT_THROW(parser.addSubcommandPositionalArg("copy", " \t \f \r\r\r\n ", good_help, NULL), MicException);
        EXPECT_THROW(parser.addSubcommandPositionalArg("\t  copy \n", " source \t\n", empty_help, NULL), MicException);
        EXPECT_THROW(parser.addSubcommandPositionalArg("\t  copy \n", " source \t\n", bad_help, NULL), MicException);

        ASSERT_NO_THROW(parser.addSubcommandPositionalArg("\t  copy \n", " source \t\n", good_help, NULL));
        EXPECT_THROW(parser.addSubcommandPositionalArg("copy", "source", good_help, NULL), MicException);
        ASSERT_NO_THROW(parser.addSubcommandPositionalArg("copy", "destination", good_help, &eval));
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_006) // Method setOutputFormatters
    {
        ostringstream* stdOut = new ostringstream();
        ostringstream* stdErr = new ostringstream();
        ostringstream* fileOut = new ostringstream();

        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        MicOutputFormatterBase* err = new ConsoleOutputFormatter(stdErr);
        MicOutputFormatterBase* fil = new ConsoleOutputFormatter(fileOut);
        {
            EvaluatorCliParserImpl1 eval = EvaluatorCliParserImpl1();
            CliParserImpl parser(1, simpleArglist, "1.0", "2014", "", "Short description present.");

            EXPECT_TRUE((bool)(parser.output_->impl_->standardOut_ != NULL));
            EXPECT_TRUE((bool)(parser.output_->impl_->standardErr_ != NULL));
            EXPECT_TRUE((bool)(parser.output_->impl_->fileOut_ == NULL));

            EXPECT_EQ(&cout, dynamic_cast<ConsoleOutputFormatter*>(parser.output_->impl_->standardOut_)->stream_);
            EXPECT_EQ(&cerr, dynamic_cast<ConsoleOutputFormatter*>(parser.output_->impl_->standardErr_)->stream_);

            parser.setOutputFormatters(out, err, fil);

            EXPECT_EQ(stdOut,  dynamic_cast<ConsoleOutputFormatter*>(parser.output_->impl_->standardOut_)->stream_);
            EXPECT_EQ(stdErr,  dynamic_cast<ConsoleOutputFormatter*>(parser.output_->impl_->standardErr_)->stream_);
            EXPECT_EQ(fileOut, dynamic_cast<ConsoleOutputFormatter*>(parser.output_->impl_->fileOut_)->stream_);

            parser.setOutputFormatters();

            EXPECT_TRUE((bool)(parser.output_->impl_->standardOut_ != NULL));
            EXPECT_TRUE((bool)(parser.output_->impl_->standardErr_ != NULL));
            EXPECT_TRUE((bool)(parser.output_->impl_->fileOut_ == NULL));

            EXPECT_EQ(&cout, dynamic_cast<ConsoleOutputFormatter*>(parser.output_->impl_->standardOut_)->stream_);
            EXPECT_EQ(&cerr, dynamic_cast<ConsoleOutputFormatter*>(parser.output_->impl_->standardErr_)->stream_);
        }
        delete fileOut;
        delete stdErr;
        delete stdOut;

        delete fil;
        delete err;
        delete out;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_007) // Method setOutputFormatters
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        {
            CliParserImpl parser(1, simpleArglist, "1.2.3", "2014", "", "Brief description.");
            ASSERT_NO_THROW(parser.setOutputFormatters(out));
            ASSERT_NO_THROW(parser.parse(true)) << stdOut->str();
            ASSERT_NO_THROW(parser.outputVersion());

            string golden = string(simpleArglist[0]) + " version 1.2.3\nCopyright (C) 2014, Intel Corporation.\n";
            EXPECT_STREQ(golden.c_str(), stdOut->str().c_str());
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_008) // Method parse #1 (options and unrestricted arguments)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        {
            CliParserImpl parser(13, arglist1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)12, parser.rawArguments_.size());
            EXPECT_EQ((unsigned int)4, parser.options_.size());
            EXPECT_THROW(parser.parse(true), MicException) << stdOut->str();
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(13, arglist1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(13, arglist1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)12, parser.rawArguments_.size());
            EXPECT_EQ((unsigned int)4, parser.options_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(6, arglist2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)3, parser.options_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(9, arglist2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(argErr1, stdOut->str().c_str());
            EXPECT_EQ((unsigned int)3, parser.options_.size());
            EXPECT_STREQ(argErr1, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(8, arglist3, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)0, parser.options_.size());
            EXPECT_STREQ(argErr2, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(9, arglist3_5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(optionErr3, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_009) // Method parse #2 (options and tool positional arguments)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        { // too few
            CliParserImpl parser(7, arglist4, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("file1", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("file2", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("file3", good_help, NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(argErr3, stdOut->str().c_str());
            EXPECT_EQ((unsigned int)2, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        { // just right
            CliParserImpl parser(8, arglist4, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("file1", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("file2", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("file3", good_help, NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)3, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        { // too many
            CliParserImpl parser(9, arglist4, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("file1", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("file2", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("file3", good_help, NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(argErr4, stdOut->str().c_str());
            EXPECT_EQ((unsigned int)3, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_010) // Method parse #3 (options and subcommands)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        {
            CliParser parser(9, arglist5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            EXPECT_THROW(parser.addSubcommand("copy", good_help), MicException);
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)5, parser.impl_->arguments_.size());
            EXPECT_STREQ("copy", parser.parsedSubcommand().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(10, arglist5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            EXPECT_THROW(parser.addSubcommand("copy", good_help), MicException);
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)5, parser.arguments_.size());
            EXPECT_EQ((unsigned int)2, parser.options_.size());
            EXPECT_STREQ(optionErr1, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(8, arglist5_5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            EXPECT_THROW(parser.addSubcommand("copy", good_help), MicException);
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(parser.parseError());
            EXPECT_EQ((unsigned int)2, parser.options_.size());
            EXPECT_STREQ(optionErr2, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(7, arglist5_7, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            EXPECT_THROW(parser.addSubcommand("copy", good_help), MicException);
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("copy", "source", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("copy", "dest", good_help, NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)2, parser.options_.size());
            EXPECT_STREQ(subErr1, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        // arglist5_8
        {
            CliParserImpl parser(3, arglist5_8, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            EXPECT_THROW(parser.addSubcommand("copy", good_help), MicException);
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)2, parser.options_.size());
            EXPECT_STREQ(subErr1_1, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_011) // Method parse #4 (options and subcommands w/ positional args)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        { // not enough
            CliParserImpl parser(5, arglist5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("copy", "source", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("copy", "dest", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));
            EXPECT_THROW(parser.addSubcommand("copy", good_help), MicException);

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)1, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        { // just right
            CliParserImpl parser(6, arglist5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("copy", "source", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("copy", "dest", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));
            EXPECT_THROW(parser.addSubcommand("copy", good_help), MicException);

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)2, parser.arguments_.size());
            EXPECT_STREQ("copy", parser.parsedSubcommand().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        { // too many
            CliParserImpl parser(7, arglist5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("copy", "source", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("copy", "dest", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", good_help, "", NULL));
            EXPECT_THROW(parser.addSubcommand("copy", good_help), MicException);

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_EQ((unsigned int)2, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_012) // Method parse #5 (all options)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        // optional option arg and unexpected option arg
        {
            CliParserImpl parser(2, optTest1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', false, "", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_FALSE(parser.options_["option"].first);
            EXPECT_STREQ("", parser.options_["option"].second.c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, optTest1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', false, "", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_FALSE(parser.options_["option"].first);
            EXPECT_STREQ("", parser.options_["option"].second.c_str());
            EXPECT_STREQ("arg", parser.arguments_.at(0).c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, optTest1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', false, "argName", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(parser.options_["option"].first);
            EXPECT_STREQ("arg", parser.options_["option"].second.c_str());
            EXPECT_EQ((unsigned int)0, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, optTest2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', false, "", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_FALSE(parser.options_["option"].first);
            EXPECT_STREQ("", parser.options_["option"].second.c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, optTest2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', false, "", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_FALSE(parser.options_["option"].first);
            EXPECT_STREQ("", parser.options_["option"].second.c_str());
            EXPECT_STREQ("arg", parser.arguments_.at(0).c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, optTest2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', false, "argName", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(parser.options_["option"].first);
            EXPECT_STREQ("arg", parser.options_["option"].second.c_str());
            EXPECT_EQ((unsigned int)0, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, optTest3, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', false, "", good_help, "", NULL));
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_FALSE(parser.options_["option"].first);
            EXPECT_STREQ("", parser.options_["option"].second.c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, optTest3, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', false, "argName", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(parser.options_["option"].first);
            EXPECT_STREQ("arg", parser.options_["option"].second.c_str());
            EXPECT_EQ((unsigned int)0, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        // required option arg
        {
            CliParserImpl parser(2, optTest1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', true, "argName", good_help, "", NULL));
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_FALSE(parser.options_["option"].first);
            EXPECT_STREQ("", parser.options_["option"].second.c_str());
            EXPECT_EQ((unsigned int)0, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, optTest1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', true, "argName", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(parser.options_["option"].first);
            EXPECT_STREQ("arg", parser.options_["option"].second.c_str());
            EXPECT_EQ((unsigned int)0, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, optTest2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', true, "argName", good_help, "", NULL));
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_FALSE(parser.options_["option"].first);
            EXPECT_STREQ("", parser.options_["option"].second.c_str());
            EXPECT_EQ((unsigned int)0, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, optTest2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', true, "argName", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(parser.options_["option"].first);
            EXPECT_STREQ("arg", parser.options_["option"].second.c_str());
            EXPECT_EQ((unsigned int)0, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, optTest3, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("option", good_help, 'o', true, "argName", good_help, "", NULL));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(parser.options_["option"].first);
            EXPECT_STREQ("arg", parser.options_["option"].second.c_str());
            EXPECT_EQ((unsigned int)0, parser.arguments_.size());
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_013) // Method parse #6 (subcommands)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        // optional option arg and unexpected option arg
        {
            CliParserImpl parser(2, subcommand1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ("copy", parser.parsedSubcommand().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, subcommand2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_014) // Method parse #7 (version)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        // optional option arg and unexpected option arg
        {
            CliParserImpl parser(2, version1, "1.2.3", string(__DATE__).substr(7), "", "Brief description.");
            parser.setOutputFormatters(out, out);
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            string golden = "tool version 1.2.3\nCopyright (C) " + string(__DATE__).erase(0, 7) + ", Intel Corporation.\n";
            EXPECT_STREQ(golden.c_str(), stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, version1, "1.2.3", string(__DATE__).substr(7), "", "Brief description.");
            parser.setOutputFormatters(out, out);
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_015) // Method parse #7 (help)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        // optional option arg and unexpected option arg
        {
            CliParserImpl parser(2, help1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("subcommand", good_help));
            EXPECT_FALSE(parser.parse(true));
            EXPECT_STREQ(goldenParse1, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, help1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("subcommand", good_help));
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ("subcommand", parser.parsedSubcommand().c_str());
            EXPECT_STREQ(goldenParse2, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(4, help1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("subcommand", good_help));
            EXPECT_FALSE(parser.parse(true));
            EXPECT_STREQ(goldenParse3, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, help2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("subcommand", good_help));
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(goldenParse1, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, help2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("subcommand", good_help));
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ("subcommand", parser.parsedSubcommand().c_str());
            EXPECT_STREQ(goldenParse2, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(4, help2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("subcommand", good_help));
            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(goldenParse3, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_016) // Method parse #8 (options retrieval)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        {
            CliParserImpl parser(13, arglist1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ("greg", get<2>(parser.parsedOption("first")).c_str());
            EXPECT_STREQ("doe", get<2>(parser.parsedOption("last")).c_str());
            EXPECT_TRUE(get<0>(parser.parsedOption("switch")));
            EXPECT_TRUE(get<0>(parser.parsedOption("verbose")));
            EXPECT_FALSE(get<1>(parser.parsedOption("switch")));
            EXPECT_FALSE(get<1>(parser.parsedOption("verbose")));
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(4, cmdlist1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));

            ASSERT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(get<0>(parser.parsedOption("first")));
            EXPECT_TRUE(get<1>(parser.parsedOption("first")));
            EXPECT_STREQ("jon", get<2>(parser.parsedOption("first")).c_str());
            EXPECT_TRUE(get<0>(parser.parsedOption("last")));
            EXPECT_TRUE(get<1>(parser.parsedOption("last")));
            EXPECT_STREQ("doe", get<2>(parser.parsedOption("last")).c_str());
            EXPECT_FALSE(get<0>(parser.parsedOption("switch")));
            EXPECT_FALSE(get<0>(parser.parsedOption("verbose")));
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(5, cmdlist1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));

            ASSERT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(get<0>(parser.parsedOption("first")));
            EXPECT_TRUE(get<1>(parser.parsedOption("first")));
            EXPECT_STREQ("jon", get<2>(parser.parsedOption("first")).c_str());
            EXPECT_TRUE(get<0>(parser.parsedOption("last")));
            EXPECT_TRUE(get<1>(parser.parsedOption("last")));
            EXPECT_STREQ("doe", get<2>(parser.parsedOption("last")).c_str());
            EXPECT_TRUE(get<0>(parser.parsedOption("switch")));
            EXPECT_FALSE(get<0>(parser.parsedOption("verbose")));
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, cmdlist1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', false, "lastName", good_help, "default", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));

            ASSERT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(get<0>(parser.parsedOption("first")));
            EXPECT_TRUE(get<1>(parser.parsedOption("first")));
            EXPECT_STREQ("jon", get<2>(parser.parsedOption("first")).c_str());
            EXPECT_TRUE(get<0>(parser.parsedOption("last")));
            EXPECT_FALSE(get<1>(parser.parsedOption("last")));
            EXPECT_STREQ("default", get<2>(parser.parsedOption("last")).c_str());
            EXPECT_FALSE(get<0>(parser.parsedOption("switch")));
            EXPECT_FALSE(get<0>(parser.parsedOption("verbose")));
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(6, cmdlist2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "level", good_help, "normal", NULL));

            ASSERT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(get<0>(parser.parsedOption("first")));
            EXPECT_TRUE(get<1>(parser.parsedOption("first")));
            EXPECT_STREQ("jon", get<2>(parser.parsedOption("first")).c_str());
            EXPECT_TRUE(get<0>(parser.parsedOption("last")));
            EXPECT_TRUE(get<1>(parser.parsedOption("last")));
            EXPECT_STREQ("doe", get<2>(parser.parsedOption("last")).c_str());
            EXPECT_FALSE(get<0>(parser.parsedOption("switch")));
            EXPECT_TRUE(get<0>(parser.parsedOption("verbose")));
            EXPECT_TRUE(get<1>(parser.parsedOption("verbose")));
            EXPECT_STREQ("high", get<2>(parser.parsedOption("verbose")).c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(5, cmdlist2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("first", good_help, 'f', true, "firstName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("last", good_help, 'l', true, "lastName", good_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "level", good_help, "normal", NULL));

            ASSERT_TRUE(parser.parse(true)) << stdOut->str();
            EXPECT_TRUE(get<0>(parser.parsedOption("first")));
            EXPECT_TRUE(get<1>(parser.parsedOption("first")));
            EXPECT_STREQ("jon", get<2>(parser.parsedOption("first")).c_str());
            EXPECT_TRUE(get<0>(parser.parsedOption("last")));
            EXPECT_TRUE(get<1>(parser.parsedOption("last")));
            EXPECT_STREQ("doe", get<2>(parser.parsedOption("last")).c_str());
            EXPECT_FALSE(get<0>(parser.parsedOption("switch")));
            EXPECT_TRUE(get<0>(parser.parsedOption("verbose")));
            EXPECT_FALSE(get<1>(parser.parsedOption("verbose")));
            EXPECT_STREQ("normal", get<2>(parser.parsedOption("verbose")).c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_017) // Method parse #8 (position arguments retrieval)
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        { // Free arguments
            CliParserImpl parser(6, freeList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_TRUE(parser.parse(true)) << stdOut->str();

            ASSERT_EQ((unsigned int)5, parser.arguments_.size());
            auto it = parser.parsedPositionalArgumentBegin();
            EXPECT_STREQ("arg1", it->c_str());
            ++it;
            EXPECT_STREQ("arg2", it->c_str());
            ++it;
            EXPECT_STREQ("arg3", it->c_str());
            ++it;
            EXPECT_STREQ("arg4", it->c_str());
            ++it;
            EXPECT_STREQ("arg5", it->c_str());
            ++it;
            EXPECT_EQ(parser.parsedPositionalArgumentEnd(), it);
        }
        {
            CliParserImpl parser(7, freeList2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_TRUE(parser.parse(true)) << stdOut->str();

            ASSERT_EQ((unsigned int)5, parser.arguments_.size());
            auto it = parser.parsedPositionalArgumentBegin();
            EXPECT_STREQ("arg1", it->c_str());
            ++it;
            EXPECT_STREQ("arg2", it->c_str());
            ++it;
            EXPECT_STREQ("arg3", it->c_str());
            ++it;
            EXPECT_STREQ("arg4", it->c_str());
            ++it;
            EXPECT_STREQ("arg5", it->c_str());
            ++it;
            EXPECT_EQ(parser.parsedPositionalArgumentEnd(), it);
        }
        { // tool positional arguments
            CliParserImpl parser(7, toolList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("arg1Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("arg2Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("arg3Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("arg4Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("arg5Name", good_help, NULL));
            ASSERT_TRUE(parser.parse(true)) << stdOut->str();

            ASSERT_EQ((unsigned int)5, parser.arguments_.size());
            auto it = parser.parsedPositionalArgumentBegin();
            EXPECT_STREQ("arg1", it->c_str());
            ++it;
            EXPECT_STREQ("arg2", it->c_str());
            ++it;
            EXPECT_STREQ("arg3", it->c_str());
            ++it;
            EXPECT_STREQ("arg4", it->c_str());
            ++it;
            EXPECT_STREQ("arg5", it->c_str());
            ++it;
            EXPECT_EQ(parser.parsedPositionalArgumentEnd(), it);
        }
        {
            CliParserImpl parser(7, toolList2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("arg1Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("arg2Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("arg3Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("arg4Name", good_help, NULL));
            ASSERT_TRUE(parser.parse(true)) << stdOut->str();

            ASSERT_EQ((unsigned int)4, parser.arguments_.size());
            auto it = parser.parsedPositionalArgumentBegin();
            EXPECT_STREQ("arg1", it->c_str());
            ++it;
            EXPECT_STREQ("arg2", it->c_str());
            ++it;
            EXPECT_STREQ("arg3", it->c_str());
            ++it;
            EXPECT_STREQ("arg4", it->c_str());
            ++it;
            EXPECT_EQ(parser.parsedPositionalArgumentEnd(), it);
        }
        {
            CliParserImpl parser(8, subList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("other", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "arg1Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "arg2Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "arg3Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "arg4Name", good_help, NULL));
            ASSERT_TRUE(parser.parse(true)) << stdOut->str();

            ASSERT_EQ((unsigned int)4, parser.arguments_.size());
            auto it = parser.parsedPositionalArgumentBegin();
            EXPECT_STREQ("arg1", it->c_str());
            ++it;
            EXPECT_STREQ("arg2", it->c_str());
            ++it;
            EXPECT_STREQ("arg3", it->c_str());
            ++it;
            EXPECT_STREQ("arg4", it->c_str());
            ++it;
            EXPECT_EQ(parser.parsedPositionalArgumentEnd(), it);
        }
        {
            CliParserImpl parser(8, subList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("other", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "arg1Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "arg2Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "arg3Name", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "arg4Name", good_help, NULL));
            ASSERT_TRUE(parser.parse(true)) << stdOut->str();

            ASSERT_EQ((unsigned int)4, parser.arguments_.size());
            for (vector<string>::size_type i = 0; i < 4; ++i)
            {
                string p = "arg" + to_string((unsigned long long)(i + 1));
                EXPECT_STREQ(p.c_str(), parser.parsedPositionalArgumentAt(i).c_str());
            }

            EXPECT_THROW(parser.parsedPositionalArgumentAt(5), MicException);
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_018) // Method outputHelp #1
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        { // Uses goldenHelp1
            CliParserImpl parser(2, helpList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));

            ASSERT_NO_THROW(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(goldenHelp1, stdOut->str().c_str());
        }
        stdOut->str(""); stdOut->clear(); // clear
        { // Uses goldenHelp2
            CliParserImpl parser(2, helpList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("mode", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("source", good_help, NULL));
            ASSERT_NO_THROW(parser.addToolPositionalArg("destination", good_help, NULL));

            ASSERT_NO_THROW(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(goldenHelp2, stdOut->str().c_str());
        }
        stdOut->str(""); stdOut->clear(); // clear
        { // Uses goldenHelp3
            CliParser parser(2, helpList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));
            ASSERT_NO_THROW(parser.addSubcommand("other", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "sourceFile", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "destFile", good_help, NULL));

            ASSERT_NO_THROW(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(goldenHelp3, stdOut->str().c_str());
        }
        stdOut->str(""); stdOut->clear(); // clear
        { // Uses goldenHelp3_5
            CliParser parser(2, helpList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));
            ASSERT_NO_THROW(parser.addSubcommand("other", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "sourceFile", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "destFile", good_help, NULL));
            EXPECT_THROW(parser.addHelpSection("", section_help), MicException) << stdOut->str();
            EXPECT_THROW(parser.addHelpSection("My Help Section", empty_help), MicException) << stdOut->str();
            EXPECT_THROW(parser.addHelpSection("My Help Section", bad_help), MicException) << stdOut->str();
            ASSERT_NO_THROW(parser.addHelpSection("My Help Section", section_help)) << stdOut->str();

            ASSERT_NO_THROW(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(goldenHelp3_5, stdOut->str().c_str());
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_019) // Method outputHelp #2
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        { // Uses goldenHelp4
            CliParserImpl parser(3, helpList2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));
            ASSERT_NO_THROW(parser.addSubcommand("other", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "sourceFile", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "destFile", good_help, NULL));

            ASSERT_NO_THROW(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(goldenHelp4, stdOut->str().c_str());
        }
        stdOut->str(""); stdOut->clear(); // clear
        { // Uses goldenHelp5
            CliParserImpl parser(3, helpList3, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));
            ASSERT_NO_THROW(parser.addSubcommand("other", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "sourceFile", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "destFile", good_help, NULL));

            ASSERT_NO_THROW(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(goldenHelp5, stdOut->str().c_str());
        }
        stdOut->str(""); stdOut->clear(); // clear
        { // Uses goldenHelp6
            CliParserImpl parser(3, helpList4, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 0, false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));
            ASSERT_NO_THROW(parser.addSubcommand("other", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("copy", good_help));
            ASSERT_NO_THROW(parser.addSubcommand("move", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "sourceFile", good_help, NULL));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("other", "destFile", good_help, NULL));

            ASSERT_NO_THROW(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(goldenHelp6, stdOut->str().c_str());
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_020) // Other group short option tests
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        {
            CliParserImpl parser(3, shortList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, shortList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, shortList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', true, "file", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(shortErr1, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, shortList2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(2, shortList2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', true, "file", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(shortErr2, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, shortList2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', true, "file", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(shortErr2, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_021) // Other option error tests
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        {
            CliParserImpl parser(2, longList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', true, "file", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(longErr1, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, longList1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', true, "file", good_help, "", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(longErr1, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        {
            CliParserImpl parser(3, longList2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("switch", good_help, 's', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("verbose", good_help, 'v', false, "", empty_help, "", NULL));
            ASSERT_NO_THROW(parser.addOption("target", good_help, 't', false, "file", good_help, "/dev/null", NULL));

            EXPECT_FALSE(parser.parse(true)) << stdOut->str();
            EXPECT_STREQ(longErr2, stdOut->str().c_str());
            stdOut->str(""); stdOut->clear(); // clear
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_022) // option list tests
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        {
            CliParserImpl parser(2, listArg1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', true, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_NO_THROW(list = parser.parsedOptionArgumentAsVector("device", "mic"));
            auto it = list.begin();
            EXPECT_STREQ("mic0", (it++)->c_str());
            EXPECT_STREQ("mic1", (it++)->c_str());
            EXPECT_STREQ("mic2", (it++)->c_str());
            EXPECT_STREQ("mic3", (it++)->c_str());
            EXPECT_STREQ("mic5", (it++)->c_str());
            EXPECT_STREQ("mic6", (it++)->c_str());
            EXPECT_STREQ("mic7", (it++)->c_str());
            EXPECT_STREQ("mic8", (it++)->c_str());
            EXPECT_STREQ("mic9", (it++)->c_str());
        }
        {
            CliParser parser(2, listArg2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', true, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            string arg = get<2>(parser.parsedOption("device"));
            vector<string> list = parser.parsedOptionArgumentAsVector("device", "mic");
            auto it = list.begin();
            EXPECT_STREQ("mic0", (it++)->c_str());
            EXPECT_STREQ("mic1", (it++)->c_str());
            EXPECT_STREQ("mic2", (it++)->c_str());
            EXPECT_STREQ("mic3", (it++)->c_str());
            EXPECT_STREQ("mic5", (it++)->c_str());
            EXPECT_STREQ("mic6", (it++)->c_str());
            EXPECT_STREQ("mic7", (it++)->c_str());
            EXPECT_STREQ("mic8", (it++)->c_str());
            EXPECT_STREQ("mic9", (it++)->c_str());
        }
        {
            CliParserImpl parser(2, listArg3, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', true, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_THROW(list = parser.parsedOptionArgumentAsVector("device", "mic"), MicException) << dumpList(list);
        }
        {
            CliParserImpl parser(2, listArg4, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', true, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_THROW(list = parser.parsedOptionArgumentAsVector("device", "mic"), MicException) << dumpList(list);
        }
        {
            CliParserImpl parser(2, listArg5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', true, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_THROW(list = parser.parsedOptionArgumentAsVector("device", "mic"), MicException) << dumpList(list);
        }
        {
            CliParserImpl parser(2, listArg6, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', false, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_NO_THROW(list = parser.parsedOptionArgumentAsVector("device", "mic")) << dumpList(list);
            EXPECT_STREQ("all", list[0].c_str());
        }
        {
            CliParserImpl parser(1, listArg6, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', false, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_NO_THROW(list = parser.parsedOptionArgumentAsVector("device", "mic")) << dumpList(list);
            EXPECT_STREQ("all", list[0].c_str());
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_023) // Positional list tests
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        {
            CliParser parser(3, listPosArg1, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("start", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("start", "deviceList", good_help, NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_NO_THROW(list = parser.parsedPositionalArgumentAsVector(0, "mic")) << stdOut->str();
            auto it = list.begin();
            EXPECT_STREQ("mic0", (it++)->c_str());
            EXPECT_STREQ("mic1", (it++)->c_str());
            EXPECT_STREQ("mic2", (it++)->c_str());
            EXPECT_STREQ("mic3", (it++)->c_str());
            EXPECT_STREQ("mic5", (it++)->c_str());
            EXPECT_STREQ("mic6", (it++)->c_str());
            EXPECT_STREQ("mic7", (it++)->c_str());
            EXPECT_STREQ("mic8", (it++)->c_str());
            EXPECT_STREQ("mic9", (it++)->c_str());
        }
        {
            CliParserImpl parser(3, listPosArg1_5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("start", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("start", "deviceList", good_help, NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            list = parser.parsedPositionalArgumentAsVector(0);
            EXPECT_NO_THROW(list = parser.parsedPositionalArgumentAsVector(0)) << stdOut->str();
            auto it = list.begin();
            EXPECT_STREQ("0", (it++)->c_str());
            EXPECT_STREQ("1", (it++)->c_str());
            EXPECT_STREQ("2", (it++)->c_str());
            EXPECT_STREQ("3", (it++)->c_str());
            EXPECT_STREQ("5", (it++)->c_str());
            EXPECT_STREQ("6", (it++)->c_str());
            EXPECT_STREQ("7", (it++)->c_str());
            EXPECT_STREQ("8", (it++)->c_str());
            EXPECT_STREQ("9", (it++)->c_str());
        }
        {
            CliParserImpl parser(3, listPosArg2, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', true, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_THROW(list = parser.parsedPositionalArgumentAsVector(0, "mic"), MicException) << dumpList(list);
        }
        {
            CliParserImpl parser(3, listPosArg3, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', true, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_THROW(list = parser.parsedPositionalArgumentAsVector(0, "mic"), MicException) << dumpList(list);
        }
        {
            CliParserImpl parser(3, listPosArg4, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', true, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_THROW(list = parser.parsedPositionalArgumentAsVector(0, "mic"), MicException) << dumpList(list);
        }
        {
            CliParserImpl parser(3, listPosArg5, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addOption("device", good_help, 'd', true, "deviceList", good_help, "all", NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            EXPECT_THROW(list = parser.parsedPositionalArgumentAsVector(0, "mic"), MicException) << dumpList(list);
        }
        {
            CliParserImpl parser(3, listPosArg6, "1.2.3", "2014", "", "Brief description.");
            parser.setOutputFormatters(out, out);
            ASSERT_NO_THROW(parser.addSubcommand("start", good_help));
            ASSERT_NO_THROW(parser.addSubcommandPositionalArg("start", "deviceList", good_help, NULL));

            EXPECT_TRUE(parser.parse(true)) << stdOut->str();

            vector<string> list;
            list = parser.parsedPositionalArgumentAsVector(0);
            EXPECT_NO_THROW(list = parser.parsedPositionalArgumentAsVector(0)) << stdOut->str();
            auto it = list.begin();
            EXPECT_STREQ("john", (it++)->c_str());
            EXPECT_STREQ("paul", (it++)->c_str());
            EXPECT_STREQ("george", (it++)->c_str());
            EXPECT_STREQ("ringo", (it++)->c_str());
        }
        delete out;
        delete stdOut;
    }

    TEST(common_framework, TC_KNL_mpsstools_CliParserImpl_024) // More for coverage
    {
        ostringstream* stdOut = new ostringstream();
        MicOutputFormatterBase* out = new ConsoleOutputFormatter(stdOut);
        vector<string> help = { "Arg Help" };
        CliParser parser(4, listPosArg24, "1.2.3", "2014", "", "Description.");
        parser.setOutputFormatters(out, out);
        parser.addToolPositionalArg("arg1", help);
        parser.addToolPositionalArg("arg2", help, NULL);
        parser.addToolPositionalArg("arg3", help);

        EXPECT_FALSE(parser.parsed());

        EXPECT_TRUE(parser.parse()) << stdOut->str();

        EXPECT_TRUE(parser.parsed());

        size_t count = 0;
        for (auto it = parser.parsedPositionalArgumentBegin(); it != parser.parsedPositionalArgumentEnd(); ++it)
        {
            ++count;
        }
        EXPECT_EQ(parser.parsedPositionalArgumentSize(), count);

        stdOut->str("");
        parser.outputHelp();
        EXPECT_STREQ(goldenHelp24.c_str(), stdOut->str().c_str());

        stdOut->str("");
        parser.outputVersion();
        EXPECT_STREQ(goldenVersion24.c_str(), stdOut->str().c_str());

        delete out;
        delete stdOut;
    }
}; // namespace micmgmt
