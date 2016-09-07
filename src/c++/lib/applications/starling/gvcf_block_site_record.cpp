// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2016 Illumina, Inc.
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

///
/// \author Chris Saunders
///

#include "gvcf_block_site_record.hh"
#include "blt_util/compat_util.hh"



static
bool
check_block_single_tolerance(const stream_stat& ss,
                             const int min,
                             const int tol)
{
    return ((min + tol) >= ss.max()/2.0);       // hack to get nova vcfs into acceptable size ramge, make check less stringent across the board
}



static
bool
check_block_tolerance(const stream_stat& ss,
                      const double frac_tol,
                      const int abs_tol)
{
    const int min(static_cast<int>(compat_round(ss.min())));
//    log_os << min << "\n";
//    return true;
    if (check_block_single_tolerance(ss,min,abs_tol)) return true;
    const int ftol(static_cast<int>(std::floor(min * frac_tol)));
    if (ftol <= abs_tol) return false;
    return check_block_single_tolerance(ss, min, ftol);
}



static
bool
is_new_value_blockable(const int new_val,
                       const stream_stat& ss,
                       const double frac_tol,
                       const int abs_tol,
                       const bool is_new_val = true,
                       const bool is_old_val = true)
{
    if (!(is_new_val && is_old_val)) return (is_new_val == is_old_val);

    stream_stat ss2(ss);
    ss2.add(new_val);
    return check_block_tolerance(ss2,frac_tol,abs_tol);
}

bool
gvcf_block_site_record::
testCanSiteJoinSampleBlockShared(
    const GermlineSiteLocusInfo& locus,
    const unsigned sampleIndex) const
{
    // pos must be +1 from end of record:
    if ((pos+count) != locus.pos) return false;

    const LocusSampleInfo& inputSampleInfo(locus.getSample(sampleIndex));
    const auto& inputSiteSampleInfo(locus.getSiteSample(sampleIndex));

    static const unsigned blockSampleIndex(0);
    const LocusSampleInfo& blockSampleInfo(getSample(blockSampleIndex));
    const auto& blockSiteSampleInfo(getSiteSample(blockSampleIndex));

    // filters must match:
    if (not (filters == locus.filters)) return false;
    if (not (blockSampleInfo.filters == inputSampleInfo.filters)) return false;

    if (is_nonref(blockSampleIndex) || locus.is_nonref(sampleIndex)) return false;

    if (! is_new_value_blockable(
        inputSiteSampleInfo.n_used_calls, block_dpu, frac_tol, abs_tol))
    {
        return false;
    }
    if (! is_new_value_blockable(
        inputSiteSampleInfo.n_unused_calls, block_dpf, frac_tol, abs_tol))
    {
        return false;
    }

    // coverage states must match:
    if (blockSiteSampleInfo.isAnyReadCoverage() != inputSiteSampleInfo.isAnyReadCoverage()) return false;
    if (blockSiteSampleInfo.isUsedReadCoverage() != inputSiteSampleInfo.isUsedReadCoverage()) return false;

    return true;
}



void
gvcf_block_site_record::
joinSiteToSampleBlockShared(
    const GermlineSiteLocusInfo& locus,
    const unsigned sampleIndex)
{
    const LocusSampleInfo& inputSampleInfo(locus.getSample(sampleIndex));
    const auto& inputSiteSampleInfo(locus.getSiteSample(sampleIndex));

    static const unsigned blockSampleIndex(0);
    LocusSampleInfo& blockSampleInfo(getSample(blockSampleIndex));

    if (count == 0)
    {
        pos = locus.pos;
        ref = locus.ref;

        filters = locus.filters;
        blockSampleInfo.filters = inputSampleInfo.filters;
        setNonRef(locus.is_nonref(sampleIndex));
        setSiteSampleInfo(blockSampleIndex, inputSiteSampleInfo);
    }

    block_dpu.add(inputSiteSampleInfo.n_used_calls);
    block_dpf.add(inputSiteSampleInfo.n_unused_calls);

    count += 1;
}



bool
gvcf_block_site_record::
testCanSiteJoinSampleBlock(
    const GermlineDiploidSiteLocusInfo& locus,
    const unsigned sampleIndex) const
{
    if (count==0) return true;

    if (not testCanSiteJoinSampleBlockShared(locus,sampleIndex)) return false;

    const LocusSampleInfo& inputSampleInfo(locus.getSample(sampleIndex));

    if (gt != locus.get_gt(sampleIndex)) return false;

    // ploidy must match
    if (ploidy != inputSampleInfo.getPloidy().getPloidy()) return false;

    // test blocking values:
    if (! is_new_value_blockable(inputSampleInfo.gqx,
                                 block_gqx,frac_tol,abs_tol,
                                 locus.is_gqx(sampleIndex),
                                 isBlockGqxDefined))
    {
        return false;
    }

    return true;
}



void
gvcf_block_site_record::
joinSiteToSampleBlock(
    const GermlineDiploidSiteLocusInfo& locus,
    const unsigned sampleIndex)
{
    const LocusSampleInfo& inputSampleInfo(locus.getSample(sampleIndex));

    const bool is_gqx(locus.is_gqx(sampleIndex));
    if (count == 0)
    {
        gt = locus.get_gt(sampleIndex);
        ploidy = inputSampleInfo.getPloidy().getPloidy();
        isBlockGqxDefined = is_gqx;
    }

    if (is_gqx)
    {
        block_gqx.add(inputSampleInfo.gqx);
    }

    joinSiteToSampleBlockShared(locus, sampleIndex);
}



bool
gvcf_block_site_record::
testCanSiteJoinSampleBlock(
    const GermlineContinuousSiteLocusInfo& locus,
    const unsigned sampleIndex) const
{
    if (count==0) return true;

    if(not testCanSiteJoinSampleBlockShared(locus,sampleIndex)) return false;

    const LocusSampleInfo& inputSampleInfo(locus.getSample(sampleIndex));

    std::ostringstream oss;
    VcfGenotypeUtil::writeGenotype(inputSampleInfo.getPloidy().getPloidy(),inputSampleInfo.max_gt(),oss);
    if (gt != oss.str()) return false;

    if (isBlockGqxDefined)
    {
        // test blocking values:
        if (! is_new_value_blockable(inputSampleInfo.gqx,
                                     block_gqx,frac_tol,abs_tol,
                                     true,
                                     true))
        {
            return false;
        }
    }

    return true;
}



void
gvcf_block_site_record::
joinSiteToSampleBlock(
    const GermlineContinuousSiteLocusInfo& locus,
    const unsigned sampleIndex)
{
    const LocusSampleInfo& inputSampleInfo(locus.getSample(sampleIndex));

    if (count == 0)
    {
        std::ostringstream oss;
        VcfGenotypeUtil::writeGenotype(inputSampleInfo.getPloidy().getPloidy(),inputSampleInfo.max_gt(),oss);
        gt = oss.str();

        // GQX is always defined in continuous call mode:
        isBlockGqxDefined = true;
        ploidy = -1;
    }

    block_gqx.add(inputSampleInfo.gqx);

    joinSiteToSampleBlockShared(locus, sampleIndex);
}
