/* 
 * The System information struture
 * 
 * Copyright 2011 Shubhro Sinha
 * Author : Shubhro Sinha <shubhro22@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; Version 2 of the License.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _BDS_SYS_H
#define _BDS_SYS_H

typedef struct {
	char *vendor;
	bds_uint page_size;
	bds_uint page_shift;
	bds_uint nr_processors;
	bds_uint cache_line_size;
} bds_system_info_t;

inline bds_int __getpageshift(void);
bds_uint __getcachelinesize(void);
#define NR_CPUS	g_sys_info.nr_processors
#define PAGE_SIZE g_sys_info.page_size
#define PAGE_SHIFT g_sys_info.page_shift
#define CACHE_LINE_SIZE g_sys_info.cache_line_size

#endif /* BDS_SYS_H */
