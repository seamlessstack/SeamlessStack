/* 
 * Generate the system wide configuration for the BluDS lib.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bds_types.h"
#include "bds_sys.h"

bds_system_info_t g_sys_info;

/* cpuid function, that sends CPUID instruction to the processor */
#ifdef _i386_
static inline 
void cpuid(unsigned int *eax, unsigned int *ebx,
			unsigned int *ecx, unsigned int *edx)
{
	/* ecx is often an input as well as an output. */
	asm volatile("pushl %%ebx	\n\t"
		"cpuid	\n\t"
		"movl %%ebx, %1	\n\t"
		"popl %%ebx	\n\t"
	    : "=a" (*eax),
	      "=r" (*ebx),
	      "=c" (*ecx),
	      "=d" (*edx)
	    : "0" (*eax), "2" (*ecx));
}
#elif defined _x86_64_
static inline 
void cpuid(unsigned int *eax, unsigned int *ebx,
			unsigned int *ecx, unsigned int *edx)
{
	/* ecx is often an input as well as an output. */
	asm volatile("cpuid"
	    : "=a" (*eax),
	      "=b" (*ebx),
	      "=c" (*ecx),
	      "=d" (*edx)
	    : "0" (*eax), "2" (*ecx));
}

#endif

void getvendorid(void)
{
#if defined (USE_OS_INFO) && defined (_LINUX_)
	FILE *fp = fopen("/proc/cpuinfo", "r");
	if (fp) {
		char buf [512];
		while (!feof(fp)) {
			fgets(buf, 512, fp);
			if (!strncmp(buf,"vendor_id", 9)) {
				strtok(buf, ":\n\t");
				if (strstr(buf, "Intel"))
					g_sys_info.vendor = "Intel";
				else if (strstr(buf, "AMD"))
					g_sys_info.vendor = "AMD";
				break;
			}
		}
		fclose(fp);
	}
#else
	bds_uint eax = 0,ebx = 0,ecx = 0,edx = 0;
	char *p[3];
	cpuid(&eax,&ebx,&ecx,&edx);
	if (ebx == 0x68747541 && ecx == 0x444D4163 && edx == 0x69746E65) 
		g_sys_info.vendor = "AMD";
	else if (ebx == 0x756e6547 && ecx == 0x6c65746e && edx == 0x49656e69) 
		g_sys_info.vendor = "Intel";
#endif
}
/* Get the page shift */
inline
bds_int __getpageshift(void) 
{
	return (ffs(getpagesize()) - 1); 
}

/* In case of multicore environment get the 
   numner of cores in the processors */
static bds_int __get_nr_processors(void)
{
	unsigned int nr_processors = 0;
#if defined (_x86_64_) || defined (_i386_)
	bds_uint eax = 0x80000008,ebx = 0,ecx = 0,edx = 0;
	if (!strcmp(g_sys_info.vendor, "AMD")) {
		cpuid(&eax,&ebx,&ecx,&edx);
		nr_processors = (ecx & 0xff) + 1;
	} else if (!strcmp(g_sys_info.vendor, "Intel")) {
		/* The check the cpuid level */
		eax = 1, ebx =0, ecx = 0, edx = 0;
		cpuid (&eax, &ebx, &ecx, &edx);
		if (eax < 4)
			nr_processors = 1;
		else {
			bds_int level = 0;
			bds_int max_threads = 0;
			bds_int threads_per_core = 0;
			do {
				eax = 0xb, ebx = 0,ecx = level, edx = 0;
				cpuid(&eax,&ebx,&ecx,&edx);
				if (ebx) {
					if (level == 0) 
						threads_per_core = ebx & 0xFFFF;
					else if (level == 1) {
						max_threads =  ebx&0xffff;
						nr_processors = max_threads / threads_per_core;
					}
				}
				++level;
			} while (eax != 0 || ebx != 0);


		}
	}
#endif
#if defined (USE_OS_INFO) && defined (_LINUX_)
	FILE *fp = fopen("/proc/cpuinfo", "r");
	if (fp) {
		char buf [512];
		while (!feof(fp)) {
			fgets(buf, 512, fp);
			if (!strncmp(buf,"cpu cores", 9)) {
				strtok(buf, ":\n");
				nr_processors = atoi(strtok(NULL, ":\n"));
				break;
			}
		}
		fclose(fp);
	}
#endif
	return nr_processors;
}

bds_uint __getcachelinesize(void)
{
	bds_uint cache_line_size = 0;
#if defined (USE_OS_INFO) && defined (_LINUX_)
	FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", 
			"r");
	if (fp) {
		fscanf(fp,"%d",&cache_line_size);
		fclose(fp);
	}
#endif
#if (defined (_x86_64_) || defined (_i386_)) && !defined (USE_OS_INFO)
	bds_uint eax = 0x80000005,ebx = 0,ecx = 0,edx = 0;
	getvendorid();
	if (!strncmp(g_sys_info.vendor, "AMD",3)) {
		cpuid(&eax,&ebx,&ecx,&edx);
		cache_line_size = (ecx & 0xff);
	} else if (!strncmp(g_sys_info.vendor, "Intel",5)) {
		eax = 4, ecx = 0;
		cpuid(&eax, &ebx, &ecx, &edx);
		cache_line_size = (ebx & 0xfff) + 1;
	}
#endif
	return cache_line_size;
}

/* Populate the system info structure */
void bds_generate_sysinfo(void)
{
	getvendorid();
	g_sys_info.page_size = getpagesize();
	g_sys_info.page_shift = __getpageshift();
	g_sys_info.cache_line_size = __getcachelinesize();
	g_sys_info.nr_processors = __get_nr_processors();
}
