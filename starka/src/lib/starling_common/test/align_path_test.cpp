// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Copyright (c) 2009-2013 Illumina, Inc.
//
// This software is provided under the terms and conditions of the
// Illumina Open Source Software License 1.
//
// You should have received a copy of the Illumina Open Source
// Software License 1 along with this program. If not, see
// <https://github.com/downloads/sequencing/licenses/>.
//

#include "boost/test/unit_test.hpp"

#include "align_path.hh"

//#define DEBUG_AP_TEST

#ifdef DEBUG_AP_TEST
#include <iostream>
namespace {
std::ostream& log_os(std::cerr);
}
#endif



BOOST_AUTO_TEST_SUITE( align_path )


void
test_string_clean(const char* cigar, const char* expect) {
    using namespace ALIGNPATH;
    path_t apath;
    cigar_to_apath(cigar,apath);
    apath_cleaner(apath);

    path_t expect_path;
    cigar_to_apath(expect,expect_path);

#ifdef DEBUG_AP_TEST
    log_os << "cleaned,expect: " << apath << " " << expect_path << "\n";
#endif

    BOOST_CHECK(apath==expect_path);
}


BOOST_AUTO_TEST_CASE( test_align_path_cleaner ) {

    // test cases for apath_cleaner function:
    test_string_clean("29M2I5M1D0M3I20M16S","29M2I5M1D3I20M16S");
    test_string_clean("1M1P1M","2M");
    test_string_clean("1M1D0M1I1M","1M1D1I1M");
    //test_string_clean("0H1H1S0I1M1D0M1I1I1D1I1M1D1M","29M2I5M1D3I20M16S");
}


BOOST_AUTO_TEST_SUITE_END()
