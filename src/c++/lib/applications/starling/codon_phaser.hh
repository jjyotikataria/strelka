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
/*
 * Codonphaser.hh
 *
 *  Test for codon-phasing.
 *
 *  Created on: Aug 10, 2013
 *  Author: Morten Kallberg
 */

#pragma once

#include "gvcf_locus_info.hh"
#include "starling_common/starling_base_shared.hh"
#include "starling_common/starling_read_buffer.hh"

#include <climits>


/// short-range phasing utility for het-snps
///
/// this object requires extended preservation of the read buffer so that it
/// can go back and recover phase information form a phasing block
///
/// \TODO generally recognized development direction is to record some kind of
///       read id in SNP pileups and indel support info so that we can go back
///       and phase from the hets without having to keep the whole read buffer (and so
///       read filtration, etc. is an exact match to the pileup).
///       Will this be worth doing before we transition to a haplotype assembly model
///       for short-range phasing?
///
struct Codon_phaser
{
    Codon_phaser(
        const starling_base_options& init_opt,
        starling_read_buffer& init_read_buffer,
        const unsigned init_max_read_len)
        : opt(init_opt),
          read_buffer(init_read_buffer),
          max_read_len(init_max_read_len)
    {
        clear();
    }

    /// add site to buffer
    ///
    /// \returns true when the buffer should be printed as a phased block
    bool add_site(const site_info& si);

    static
    bool
    is_phasable_site(
        const site_info& si)
    {
        return (si.dgt.is_snp && si.is_het());
    }

    // clear all object data
    void clear();

    void write_out_buffer() const;      // debugging feature, print current buffer to std
    void write_out_alleles() const;     // print allele evidence

    /// Are we currently in a phasing block?
    bool is_in_block() const
    {
        return block_start != -1;
    }

    /// buffer of het snp calls
    ///
    const std::vector<site_info>&
    buffer() const
    {
        return _buffer;
    }

private:
    void make_record();                 // make phased record

    void
    collect_read_segment_evidence(
        const read_segment& rseg);

    void collect_read_evidence();       // fill in allele counter
    void construct_reference();         // assemble the reference allele for the record
    void create_phased_record();        // fill in the si record and decide if we have sufficient evidence for a phased call
    unsigned get_block_length() const
    {
        return (this->block_end-this->block_start+1);
    }

    std::vector<site_info> _buffer;
    const starling_base_options& opt;
    starling_read_buffer& read_buffer;  // pass along the relevant read-buffer
    int max_read_len;                   // the length of the input reads

    int block_start,block_end;          // position of the first and last added het site to block
    int het_count;                      // total hets observed in buffer
    int total_reads,total_reads_unused; // total used and unused reads spanning phasing region
    std::string reference;              // the phased allele reference
    typedef std::map<std::string,int> allele_map;
    allele_map observations;
};