/*= -*- c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *
 * librsync -- the library for network deltas
 *
 * Copyright 2000, 2001, 2014, 2015 by Martin Pool <mbp@sourcefrog.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

                              /*
                               | Is it possible that software is not
                               | like anything else, that it is meant
                               | to be discarded: that the whole point
                               | is to always see it as a soap bubble?
                               |        -- Alan Perlis
                               */



#include "config.h"

#include <assert.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "librsync.h"

#include "trace.h"
#include "fileutil.h"
#include "sumset.h"
#include "job.h"
#include "buf.h"
#include "whole.h"
#include "util.h"

/**
 * Run a job continuously, with input to/from the two specified files.
 * The job should already be set up, and must be free by the caller
 * after return.
 *
 * Buffers of ::rs_inbuflen and ::rs_outbuflen are allocated for
 * temporary storage.
 *
 * \param in_file Source of input bytes, or NULL if the input buffer
 * should not be filled.
 *
 * \return RS_DONE if the job completed, or otherwise an error result.
 */
rs_result
rs_whole_run(rs_job_t *job, FILE *in_file, FILE *out_file)
{
    rs_buffers_t    buf;
    rs_result       result;
    rs_filebuf_t    *in_fb = NULL, *out_fb = NULL;

    if (in_file)
        in_fb = rs_filebuf_new(in_file, rs_inbuflen);

    if (out_file)
        out_fb = rs_filebuf_new(out_file, rs_outbuflen);

    result = rs_job_drive(job, &buf,
                          in_fb ? rs_infilebuf_fill : NULL, in_fb,
                          out_fb ? rs_outfilebuf_drain : NULL, out_fb);

    if (in_fb)
        rs_filebuf_free(in_fb);

    if (out_fb)
        rs_filebuf_free(out_fb);

    return result;
}



rs_result
rs_sig_file(FILE *old_file, FILE *sig_file, size_t new_block_len,
            size_t strong_len,
	    rs_magic_number sig_magic,
	    rs_stats_t *stats)
{
    rs_job_t        *job;
    rs_result       r;

    job = rs_sig_begin(new_block_len, strong_len, sig_magic);
    r = rs_whole_run(job, old_file, sig_file);
    if (stats)
        memcpy(stats, &job->stats, sizeof *stats);
    rs_job_free(job);

    return r;
}


rs_result
rs_loadsig_file(FILE *sig_file, rs_signature_t **sumset, rs_stats_t *stats)
{
    rs_job_t            *job;
    rs_result           r;

    job = rs_loadsig_begin(sumset);
    r = rs_whole_run(job, sig_file, NULL);
    if (stats)
        memcpy(stats, &job->stats, sizeof *stats);
    rs_job_free(job);

    return r;
}



rs_result
rs_delta_file(rs_signature_t *sig, FILE *new_file, FILE *delta_file,
              rs_stats_t *stats)
{
    rs_job_t            *job;
    rs_result           r;

    job = rs_delta_begin(sig);

    r = rs_whole_run(job, new_file, delta_file);

    if (stats)
        memcpy(stats, &job->stats, sizeof *stats);

    rs_job_free(job);

    return r;
}



rs_result rs_patch_file(FILE *basis_file, FILE *delta_file, FILE *new_file,
                        rs_stats_t *stats)
{
    rs_job_t            *job;
    rs_result           r;

    job = rs_patch_begin(rs_file_copy_cb, basis_file);

    r = rs_whole_run(job, delta_file, new_file);
    
    if (stats)
        memcpy(stats, &job->stats, sizeof *stats);

    rs_job_free(job);

    return r;
}

rs_result rs_rdiff_sig(char *basis_name, char *sig_name, size_t block_len)
{
    FILE            *basis_file, *sig_file;
    rs_stats_t      stats;
    rs_result       result;
    rs_long_t       sig_magic;

    if (block_len == 0) {
        block_len = RS_DEFAULT_BLOCK_LEN;
    }

    basis_file = rs_file_open(basis_name, "rb");
    if (!basis_file)
        return RS_IO_ERROR;

    sig_file = rs_file_open(sig_name, "wb");
    if (!sig_file) {
        rs_file_close(basis_file);
        return RS_IO_ERROR;
    }

    result = rs_sig_file(basis_file, sig_file, block_len, 0,
        RS_BLAKE2_SIG_MAGIC, &stats);

    rs_file_close(sig_file);
    rs_file_close(basis_file);
    if (result != RS_DONE)
        return result;

    return result;
}

rs_result rs_rdiff_delta(char* sig_name, char* new_name, char* delta_name)
{
	FILE            *sig_file, *new_file, *delta_file;
	rs_result       result;
	rs_signature_t  *sumset;
	rs_stats_t      stats;

	sig_file = rs_file_open(sig_name, "rb");
	if (!sig_file)
		return RS_IO_ERROR;

	new_file = rs_file_open(new_name, "rb");
	if (!new_file) {
		rs_file_close(sig_file);
		return RS_IO_ERROR;
	}

	delta_file = rs_file_open(delta_name, "wb");
	if (!delta_file) {
		rs_file_close(new_file);
		rs_file_close(sig_file);
		return RS_IO_ERROR;
	}

	result = rs_loadsig_file(sig_file, &sumset, &stats);
	if (result != RS_DONE) {
		rs_file_close(delta_file);
		rs_file_close(new_file);
		rs_file_close(sig_file);
		return result;
	}

	if ((result = rs_build_hash_table(sumset)) != RS_DONE) {
		rs_free_sumset(sumset);
		rs_file_close(delta_file);
		rs_file_close(new_file);
		rs_file_close(sig_file);
		return result;
	}

	result = rs_delta_file(sumset, new_file, delta_file, &stats);

	rs_free_sumset(sumset);

	rs_file_close(delta_file);
	rs_file_close(new_file);
	rs_file_close(sig_file);

	return result;
}

rs_result rs_rdiff_patch(char *basis_name, char *delta_name, char *new_name)
{
    /*  patch BASIS [DELTA [NEWFILE]] */
    FILE               *basis_file, *delta_file, *new_file;
    rs_stats_t          stats;
    rs_result           result;

    basis_file = rs_file_open(basis_name, "rb");
    if (!basis_file)
        return RS_IO_ERROR;

    delta_file = rs_file_open(delta_name, "rb");
    if (!delta_file) {
        rs_file_close(basis_file);
        return RS_IO_ERROR;
    }

    new_file = rs_file_open(new_name, "wb");
    if (!new_file) {
        rs_file_close(delta_file);
        rs_file_close(basis_file);
        return RS_IO_ERROR;
    }

    result = rs_patch_file(basis_file, delta_file, new_file, &stats);

    rs_file_close(new_file);
    rs_file_close(delta_file);
    rs_file_close(basis_file);

    return result;
}
