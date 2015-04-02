// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Starka
// Copyright (c) 2009-2014 Illumina, Inc.
//
// This software is provided under the terms and conditions of the
// Illumina Open Source Software License 1.
//
// You should have received a copy of the Illumina Open Source
// Software License 1 along with this program. If not, see
// <https://github.com/sequencing/licenses/>
//

///
/// \author Chris Saunders
///

#pragma once

#include "DenovoAlignmentFileOptions.hh"

#include "boost/program_options.hpp"


boost::program_options::options_description
getOptionsDescription(
    DenovoAlignmentFileOptions& opt);


bool
parseOptions(
    const boost::program_options::variables_map& vm,
    DenovoAlignmentFileOptions& opt,
    std::string& errorMsg);