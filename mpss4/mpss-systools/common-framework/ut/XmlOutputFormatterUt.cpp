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
#include <sstream>
#include "XmlOutputFormatter.hpp"

namespace micmgmt
{
    using namespace std;

    string buildXml(const char* xml)
    {
        string rv = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone='yes'?><tool toolname=\"tool\" timestamp=\"*\">";
        rv += xml;
        rv += "</tool>";
        return rv;
    }

    const char* xmlOut1 = "<line>This is a test.</line><line /><section name=\"SUBSECTION1\"><line>Testing...</line>\
<section name=\"SUBSECTION2\"><line>Testing...</line></section></section><line>Testing...</line>";

    const char* xmlOut2 = "<line>This is a test.</line><line /><section name=\"SUBSECTION1\"><line>Testing...</line>\
<section name=\"SUBSECTION2\"><line>Testing...</line></section></section>";

    const char* xmlOut3 = "<line>This is a test.</line><line /><line /><section><section><section><section>\
</section></section></section></section>";

    const char* xmlOut4 = "<section name=\"A\"><section name=\"B\"><section name=\"C\"><section name=\"D\">\
</section></section></section></section>";


    bool compareWithMask(const std::string& output, const std::string& mask)
    {
        if (mask.empty() == true)
        {
            return (output == mask);
        }
        // split on "*" then check each part.  Match if all marts match.
        vector<string> parts;
        size_t star = mask.find('*');
        size_t last = 0;
        if (star > 0)
        {
            parts.push_back(mask.substr(0, star));
        }
        while (star != string::npos)
        {
            size_t start = star + 1;
            size_t end = mask.find('*', start);
            if (end != string::npos)
            {
                --end;
            }
            string p = mask.substr(start, (end - start + 1));
            last = start;
            if (p.empty() == false)
            {
                parts.push_back(p);
            }
            star = mask.find('*', last);
        }
        if (parts.size() == 0)
        {
            parts.push_back(mask);
        }
        size_t count = 0;
        star = 0;
        for (auto it = parts.begin(); it != parts.end(); ++it)
        {
            string f = *it;
            star = output.find(f, star);
            if (star != string::npos)
            {
                ++count;
                star += f.length();
            }
            else
            {
                break;
            }
        }
        return (count == parts.size());
    }

    TEST(common_framework, TC_KNL_mpsstools_XmlOutputFormatter_001)
    {
        ostringstream* oStrStream = new ostringstream();
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1");
            formatter.outputLine("Testing...");
            formatter.startSection("SUBSECTION2");
            formatter.outputLine("Testing...");
            formatter.endSection();
            formatter.endSection();
            EXPECT_THROW(formatter.endSection(), logic_error);
            EXPECT_THROW(formatter.endSection(), logic_error);
            formatter.outputLine("Testing...");
        }
        EXPECT_TRUE(compareWithMask(oStrStream->str(), buildXml(xmlOut1)));
        oStrStream->str(""); oStrStream->clear(); // clear
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.startSection("SUBSECTION1");
            formatter.outputLine("Testing...");
            formatter.startSection("SUBSECTION2");
            formatter.outputLine("Testing...");
        }
        EXPECT_TRUE(compareWithMask(oStrStream->str(), buildXml(xmlOut2)));
        oStrStream->str(""); oStrStream->clear(); // clear
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            formatter.outputLine("This is a test.");
            formatter.outputLine();
            formatter.outputLine();
            formatter.startSection("");
            formatter.startSection("");
            formatter.startSection("");
            formatter.startSection("");
        }
        EXPECT_TRUE(compareWithMask(oStrStream->str(), buildXml(xmlOut3)));
        oStrStream->str(""); oStrStream->clear(); // clear
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            formatter.startSection("A");
            formatter.startSection("B");
            formatter.startSection("C");
            formatter.startSection("D");
        }
        EXPECT_TRUE(compareWithMask(oStrStream->str(), buildXml(xmlOut4)));
        oStrStream->str(""); oStrStream->clear(); // clear

        delete oStrStream;
    }

    const char* errOut1 = "<error>Raw error!</error>";

    const char* errOut2 = "<section name=\"S\"><error>Raw error!</error></section>";

    const char* errOut3 = "<error code=\"0x20000000\">Error: &apos;ERROR&apos;!</error>";

    const char* errOut4 = "<error code=\"0x20030000\">&quot;ERROR&quot;!</error>";

    const char* errOut5 = "<error code=\"0x00000001\">ERROR&amp;CODE!</error>";

    TEST(common_framework, TC_KNL_mpsstools_XmlOutputFormatter_002)
    {
        ostringstream* oStrStream = new ostringstream();
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            formatter.outputError("Raw error!", MicErrorCode::eSuccess);
        }
        EXPECT_TRUE(compareWithMask(oStrStream->str(),buildXml(errOut1)));
        oStrStream->str(""); // clear
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            formatter.startSection("S");
            formatter.outputError("Raw error!", MicErrorCode::eSuccess);
            formatter.endSection();
            EXPECT_THROW(formatter.endSection(), logic_error);
        }
        EXPECT_TRUE(compareWithMask(oStrStream->str(),buildXml(errOut2)));
        oStrStream->str(""); oStrStream->clear(); // clear
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            formatter.outputError("'ERROR'!", MicErrorCode::eGeneralError);
        }
        oStrStream->str(""); oStrStream->clear(); // clear
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            formatter.outputError("ERROR&CODE!", MicErrorCode::eGeneralError);
        }
        EXPECT_TRUE(compareWithMask(oStrStream->str(),buildXml(errOut5)));
        oStrStream->str(""); oStrStream->clear(); // clear

        delete oStrStream;
    }

    const char* nameV1 = "<data name=\"Core Temperature\" units=\"C\">101.2</data><data name=\"Core Temperature\">101.2</data><data name=\"Core Temperature\" />";

    TEST(common_framework, TC_KNL_mpsstools_XmlOutputFormatter_003)
    {
        ostringstream* oStrStream = new ostringstream();
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            EXPECT_THROW(formatter.outputNameValuePair("Core Temperature", "101.2", "<C>"), invalid_argument);
            EXPECT_NO_THROW(formatter.outputNameValuePair("Core Temperature", "101.2", "C"));
            EXPECT_NO_THROW(formatter.outputNameValuePair("Core Temperature", "101.2"));
            EXPECT_NO_THROW(formatter.outputNameValuePair("Core Temperature", ""));
        }
        EXPECT_TRUE(compareWithMask(oStrStream->str(), buildXml(nameV1))) << oStrStream->str();
        oStrStream->str(""); oStrStream->clear(); // clear

        delete oStrStream;
    }

    TEST(common_framework, TC_KNL_mpsstools_XmlOutputFormatter_004)
    {
        ostringstream* oStrStream = new ostringstream();
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            string str = formatter.convertToXmlString("");
            EXPECT_STREQ("", str.c_str());

            str = formatter.convertToXmlString("&");
            EXPECT_STREQ("&amp;", str.c_str());

            str = formatter.convertToXmlString("<");
            EXPECT_STREQ("&lt;", str.c_str());

            str = formatter.convertToXmlString(">");
            EXPECT_STREQ("&gt;", str.c_str());

            str = formatter.convertToXmlString("'");
            EXPECT_STREQ("&apos;", str.c_str());

            str = formatter.convertToXmlString("\"");
            EXPECT_STREQ("&quot;", str.c_str());

            str = formatter.convertToXmlString("&amp;");
            EXPECT_STREQ("&amp;", str.c_str());

            str = formatter.convertToXmlString("&lt;");
            EXPECT_STREQ("&lt;", str.c_str());

            str = formatter.convertToXmlString("&gt;");
            EXPECT_STREQ("&gt;", str.c_str());

            str = formatter.convertToXmlString("&apos;");
            EXPECT_STREQ("&apos;", str.c_str());

            str = formatter.convertToXmlString("&quot;");
            EXPECT_STREQ("&quot;", str.c_str());
        }
        oStrStream->str(""); oStrStream->clear(); // clear

        delete oStrStream;
    }

    TEST(common_framework, TC_KNL_mpsstools_XmlOutputFormatter_005)
    {
        ostringstream* oStrStream = new ostringstream();
        {
            XmlOutputFormatter formatter(oStrStream, "tool");
            string str = formatter.convertToXmlString("Begin: <something to think about & ponder> Then: Get the \"answer\". 'cute' :)");
            EXPECT_STREQ("Begin: &lt;something to think about &amp; ponder&gt; Then: Get the &quot;answer&quot;. &apos;cute&apos; :)", str.c_str());

            str = formatter.convertToXmlString("\"Begin: <something to think about & ponder> Then: Get the 'answer'.\"");
            EXPECT_STREQ("&quot;Begin: &lt;something to think about &amp; ponder&gt; Then: Get the &apos;answer&apos;.&quot;", str.c_str());
        }
        oStrStream->str(""); oStrStream->clear(); // clear

        delete oStrStream;
    }
}; // namespace micmgmt
