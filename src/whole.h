/*= -*- c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *
 * librsync -- the library for network deltas
 *
 * Copyright (C) 2001 by Martin Pool <mbp@sourcefrog.net>
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

#if defined(_WIN32) || defined(WIN32)
  #define RDIFF_EXPORT __declspec(dllexport)
#else
  #define RDIFF_EXPORT
#endif

rs_result rs_whole_run(rs_job_t *job, FILE *in_file, FILE *out_file);

RDIFF_EXPORT rs_result rs_rdiff_sig(char* basis_name, char* sig_name, size_t block_len);
RDIFF_EXPORT rs_result rs_rdiff_delta(char* sig_name, char* new_name, char* delta_name);
RDIFF_EXPORT rs_result rs_rdiff_patch(char *basis_name, char *delta_name, char *new_name);
