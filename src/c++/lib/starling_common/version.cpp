//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2018 Illumina, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

#include "version.hh"

#include "common/config.h"


const char*
getVersion()
{
    return WORKFLOW_VERSION;
}

inline
const char*
buildTime()
{
    return BUILD_TIME;
}

inline
const char*
cxxCompilerName()
{
    return CXX_COMPILER_NAME;
}

inline
const char*
compilerVersion()
{
    return COMPILER_VERSION;
}
