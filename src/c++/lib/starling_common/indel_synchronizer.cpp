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
/// \author Mitch Bekritsky
///

#include "starling_common/indel_synchronizer.hh"

#include "blt_util/binomial_test.hh"
#include "blt_util/blt_exception.hh"
#include "blt_util/log.hh"
#include "starling_common/starling_indel_error_prob.hh"

#include <cstdlib>

#include <iostream>
#include <sstream>


void
indel_sync_data::
register_sample(
    indel_buffer& ib,
    const depth_buffer& db,
    const depth_buffer& db2,
    const starling_sample_options& sample_opt,
    const double max_depth,
    const sample_id_t sample_no)
{
    if (_idata.test_key(sample_no))
    {
        log_os << "ERROR: sample_no " << sample_no << " repeated in indel sync registration\n";
        exit(EXIT_FAILURE);
    }
    _idata.insert(sample_no,indel_sample_data(ib,db,db2,sample_opt,max_depth));
}



bool
indel_synchronizer::
insert_indel(const indel_observation& obs)
{
    // first insert indel into this sample:
    bool is_synced_sample(false);
    bool is_repeat_obs(false);

    const bool is_novel(ibuff(_sample_order).insert_indel(obs,is_synced_sample,is_repeat_obs));

    // then insert indel into synchronized samples:
    is_synced_sample=true;
    const unsigned isds(idata().size());
    for (unsigned i(0); i<isds; ++i)
    {
        if (i == _sample_order) continue;
        ibuff(i).insert_indel(obs,is_synced_sample,is_repeat_obs);
    }
    return is_novel;
}



bool
indel_synchronizer::
is_candidate_indel_impl_test(
    const indel_key& ik,
    const indel_data& id,
    const indel_data* idsp[],
    const unsigned isds) const
{
    // check whether the candidate has been externally specified:
    for (unsigned i(0); i<isds; ++i)
    {
        if (idsp[i]->is_external_candidate) return true;
    }

    // have we already tested if the indel is much more likely to be noise than sample variation?
    bool is_indel_noise_checked(false);

    //////////////////////////////////////
    // if candidate is in a hompolymer
    // test against homopolymer error model
    //
    {
        starling_indel_report_info iri;
        get_starling_indel_report_info(ik,id,_ref,iri);

        // check to see if indel meets homopolymer criteria
        if (iri.repeat_unit.size() == 1 && (iri.ref_repeat_count >= 2 || iri.indel_repeat_count >= 2))
        {
            is_indel_noise_checked=true;

            double ref_error_prob(0.);
            double indel_error_prob(0.);
            // indel_error_prob does not factor in to this calculation, but is
            // required by get_indel_error_prob

            // get expected per-read error rate for this homopolymer
            get_indel_error_prob(_opt,iri,ref_error_prob,indel_error_prob);

            bool is_pass_indel_noise_check(false);

            for (unsigned i(0); i<isds; ++i)
            {
                // for each sample, get the number of tier 1 reads supporting the indel
                // and the total number of tier 1 reads at this locus
                const unsigned n_indel_reads = idsp[i]->all_read_ids.size();
                unsigned n_total_reads = ebuff(i).val(ik.pos-1);

                // total reads and indel reads are measured in different ways here, so the nonsensical
                // result of indel_reads>total_reads is possible. The total is fudged below to appear sane
                // before going into the count test:
                n_total_reads = std::max(n_total_reads,n_indel_reads);

                // test to see if the observed indel coverage has a binomial exact test
                // p-value below the rejection threshold.  If it does not, it cannot be
                // a candidate indel
                is_pass_indel_noise_check=(is_reject_binomial_gte_n_success_exact(_opt.tumor_min_hpol_pval, ref_error_prob,
                                                                                  n_indel_reads, n_total_reads));

                // if any sample passes the homopolymer noise check (i.e. indel has high enough
                // coverage that it may not be noise), the indel is a candidate
                if (is_pass_indel_noise_check)
                {
                    break;
                }
            }

            if (! is_pass_indel_noise_check) return false;
        }
    }


    if (! is_indel_noise_checked)
    {
        //////////////////////////////////////
        // test against min read count:
        //
        {
            bool is_min_count(false);

            int n_total_reads(0);
            for (unsigned i(0); i<isds; ++i)
            {
                const int n_reads(idsp[i]->all_read_ids.size());

                // do the candidate reads exceed the (possibly lower than
                // default) sample specific threshold?:
                if (n_reads >= sample_opt(i).min_candidate_indel_reads)
                {
                    is_min_count=true;
                    break;
                }
                n_total_reads += n_reads;
            }

            // do reads from all samples exceed the default threshold?:
            if (n_total_reads >= _opt.default_min_candidate_indel_reads) is_min_count=true;

            if (! is_min_count) return false;
        }

        //////////////////////////////////////
        // test against min read frac:
        //
        {
            bool is_min_frac(false);

            double min_large_indel_frac(_opt.min_candidate_indel_read_frac);
            const bool is_small_indel(static_cast<int>(std::max(ik.length,ik.swap_dlength)) <= _opt.max_small_candidate_indel_size);


            for (unsigned i(0); i<isds; ++i)
            {
                // this value is used to get around type-mismatch error in
                // std::max() when used below
                static const unsigned one(1);

                // note estdepth is based on genomic reads only, so
                // readfrac can be > 1:
                //
                const unsigned n_reads(idsp[i]->all_read_ids.size());
                const unsigned estdepth(std::max(one,ebuff(i).val(ik.pos-1)));
                const double readfrac(static_cast<double>(n_reads)/static_cast<double>(estdepth));

                double min_indel_frac(min_large_indel_frac);
                if (is_small_indel)
                {
                    min_indel_frac=std::max(min_indel_frac,sample_opt(i).min_small_candidate_indel_read_frac);
                }

                // min_frac threshold only needs to pass in one sample to
                // be a candidate in all synchronized samples:
                if (readfrac >= min_indel_frac)
                {
                    is_min_frac=true;
                    break;
                }
            }

            if (! is_min_frac) return false;
        }
    }

    /////////////////////////////////////////
    // test against short open-ended segments:
    //
    // call get_insert_size() instead of asking for the insert seq
    // so as to not finalize any incomplete insertions:
    if (ik.is_breakpoint() &&
        (_opt.min_candidate_indel_open_length > id.get_insert_size()))
    {
        return false;
    }

    /////////////////////////////////////////
    // test against max_depth:
    //
    for (unsigned i(0); i<isds; ++i)
    {
        const double max_depth(idata().get_value(i).max_depth);
        if (max_depth <= 0.) continue;

        const unsigned estdepth(ebuff(i).val(ik.pos-1));
        const unsigned estdepth2(ebuff2(i).val(ik.pos-1));
        if ((estdepth+estdepth2) > max_depth) return false;
    }

    return true;
}



void
indel_synchronizer::
is_candidate_indel_impl(
    const indel_key& ik,
    const indel_data& id) const
{
    //////////////////////////////////////
    // lookup all indel_data objects:
    //
    const indel_data* idsp[MAX_SAMPLE];

    const unsigned isds(idata().size());
    for (unsigned i(0); i<isds; ++i)
    {
        if (i==_sample_order)
        {
            idsp[i] = &id;
        }
        else
        {
            idsp[i] = ibuff(i).get_indel_data_ptr(ik);
            assert(nullptr != idsp[i]);
        }
    }

    const bool is_candidate(is_candidate_indel_impl_test(ik,id,idsp,isds));

    for (unsigned i(0); i<isds; ++i)
    {
        idsp[i]->status.is_candidate_indel=is_candidate;
        idsp[i]->status.is_candidate_indel_cached=true;
    }
}


void
indel_synchronizer::
find_data_exception(const indel_key& ik) const
{
    std::ostringstream oss;
    oss << "ERROR: could not find indel_data for indel: " << ik;
    throw blt_exception(oss.str().c_str());
}

