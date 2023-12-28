/*
 * free(1) - Display the amount of space for RAM and swap.
 *
 * BSD 2-Clause License
 * 
 * Copyright (c) 2023, rilysh <nightquick@proton.me>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <kvm.h>
#include <getopt.h>
#include <sys/cdefs.h>
#include <sys/sysctl.h>

#ifdef ENABLE_LOCALE
#  include <libintl.h>
#  include <locale.h>
#  define _(msg)    gettext(msg)
#else
#  define _(msg)    msg
#endif

/* Program version */
#define PROGRAM_VERSION    "0.1"

/* Multiply with page size */
#define CONVERT_UNIT(x) (x * (uint64_t)(sysconf(_SC_PAGESIZE)))

/* Units (in decimal) */
#define TO_B    (uint64_t)1
#define TO_K    (uint64_t)1000
#define TO_M    (uint64_t)(TO_K * 1000)
#define TO_G    (uint64_t)(TO_M * 1000)
#define TO_T    (uint64_t)(TO_G * 1000)
#define TO_P    (uint64_t)(TO_T * 1000)
#define TO_E    (uint64_t)(TO_P * 1000)
#define TO_Z    (uint64_t)(TO_E * 1000)
#define TO_Y    (uint64_t)(TO_Z * 1000)

/* Units (in binary) */
#define TO_Bi   (uint64_t)1
#define TO_Ki   (uint64_t)1024
#define TO_Mi   (uint64_t)(TO_Ki * 1024)
#define TO_Gi   (uint64_t)(TO_Mi * 1024)
#define TO_Ti   (uint64_t)(TO_Gi * 1024)
#define TO_Pi   (uint64_t)(TO_Ti * 1024)
#define TO_Ei   (uint64_t)(TO_Pi * 1024)
#define TO_Zi   (uint64_t)(TO_Ei * 1024)
#define TO_Yi   (uint64_t)(To_Zi * 1024)

/* Structure where retrieved values will reside */
struct free_model {
        uint64_t totalram;
	uint64_t freeram;
	uint64_t usedram;
	uint64_t buffer;
	uint64_t shared;
	uint64_t totalswap;
	uint64_t usedswap;
	uint64_t freeswap;
};

/* Option flag structure */
struct opt_flag {
        uint64_t power_flag;
        int human_flag;
	int decimal_flag;
	int total_flag;
	int secs_flag;
	int count_flag;
};

enum {
	/* To decimal options */
	B_OPT        = 1,
	K_OPT        = 2,
	M_OPT        = 3,
	G_OPT        = 4,
	T_OPT        = 5,
	P_OPT        = 6,

	/* To binary options */
	Bi_OPT       = 10,
	Ki_OPT       = 11,
	Mi_OPT       = 12,
	Gi_OPT       = 13,
	Ti_OPT       = 14,
	Pi_OPT       = 15,

	/* Use pow(1000, n) instead of pow(1024, n) */
	DECIMAL_OPT  = 19,

	/* Other options */
	HUMAN_OPT    = 'h',
	TOTAL_OPT    = 't',
	SECS_OPT     = 's',
	COUNT_OPT    = 'c',
	HELP_OPT     = 20,
	VERSION_OPT  = 21,
};

/* Convert string to int */
static int xatoi(const char *src)
{
	char *eptr;
	long val;

	val = strtol(src, &eptr, 10);
	if (eptr == src) {
		fputs(_("free: expected an integer "), stderr);
		fputs(_("but found something else.\n"), stderr);

		/* We'll just exit as we can't return an integer
		   here, as that might be a valid integer, but
		   will get interpreted as an error. */
		exit(EXIT_FAILURE);
	}

	return ((int)((int)(val) & INT32_MAX));
}

/* Format the output bytes to a human readable format.
   e.g.
   Input: 1985596 (in kB, decimal)
   Output: 1.9Gi (in binary) and 2.0G (in decimal) */
static char *pretty_format(uint64_t nsz, int is_decimal)
{
	double base, res;
	int idx;
	char *p;

	if (nsz <= 0)
		return strdup("0B");

	p = calloc(30, sizeof(char));
	if (p == NULL) {
		perror("calloc()");
	        abort();
		/* unreachable */
	}

	/* Decimal pow(1000, n) */
	if (is_decimal) {
		const char *suff[10] = {
			"B", "K", "M", "G", "T",
			"P", "E", "Z", "Y"
		};

		base = log10((double)nsz) / log10(1000.0);
		res = round(pow(1000.0, base - floor(base)) * 10.0) / 10.0;
		idx = (int)floor(base);

		snprintf(p, (size_t)30, "%0.1lf%s", res, suff[idx]);

	/* Binary pow(1024, n) */
	} else {
	        const char *suff[10] = {
			"B", "Ki", "Mi", "Gi", "Ti",
			"Pi", "Ei", "Zi", "Yi"
		};

		base = log10((double)nsz) / log10(1024.0);
		res = round(pow(1024.0, base - floor(base)) * 10.0) / 10.0;
		idx = (int)floor(base);

		snprintf(p, (size_t)30, "%0.1lf%s", res, suff[idx]);
	}

	return (p);
}

/* Get the size of total reachable memory by the operating system. */
static void get_total_memory(struct free_model *mod)
{
	uint64_t total = 0;
	size_t sz;
	int ret;

	sz = sizeof(total);
        ret = sysctlbyname("vm.stats.vm.v_page_count", &total, &sz, NULL, 0);
	if (ret == -1) {
	        mod->totalram = (uint64_t)-1;
		return;
	}

	/* Assign retrieved value to "totalram" */
	mod->totalram = CONVERT_UNIT(total);
}

/* Get the size of total free (unused) memory */
static void get_free_memory(struct free_model *mod)
{
	uint64_t free = 0;
	size_t sz;
	int ret;

	sz = sizeof(free);
	ret = sysctlbyname("vm.stats.vm.v_free_count", &free, &sz, NULL, 0);
	if (ret == -1) {
		mod->freeram = (uint64_t)-1;
		return;
	}

	mod->freeram = CONVERT_UNIT(free);
}

/* Get the size of total used memory */
static inline void get_used_memory(struct free_model *mod)
{
	get_total_memory(mod);
	get_free_memory(mod);

	mod->usedram = mod->totalram - mod->freeram;
}

/* Get the size of buffer'd memory
   Note: I'm not sure whether or not kernel buffer
   also is included in "vm.stats.vm.v_active_count".
   We need to look for some documentation on this. */
static void get_buffer_memory(struct free_model *mod)
{
	uint64_t buffer = 0;
	size_t sz;
	int ret;

	sz = sizeof(buffer);
	ret = sysctlbyname("vm.stats.vm.v_active_count", &buffer, &sz, NULL, 0);
	if (ret == -1) {
		mod->buffer = (uint64_t)-1;
		return;
	}

	mod->buffer = CONVERT_UNIT(buffer);
}

/* Get the size of shared memory
   Note: This isn't like the "shared" memory across
   the system, it's rather a constant value that can
   be tune'd.

   e.g.
   sysctl -w kern.ipc.shmmax=123456789 */
static void get_shared_memory(struct free_model *mod, int is_decimal)
{
	uint64_t shared = 0;
	size_t sz;
	int ret;

	sz = sizeof(shared);
	ret = sysctlbyname("kern.ipc.shmmax", &shared, &sz, NULL, 0);
	if (ret == -1) {
		mod->shared = (uint64_t)-1;
		return;
	}

	mod->shared = shared / (is_decimal ? 1000 : 1024);
}

/* Get the size of the total and used swap space
   Note: If you've multiple swap partitions, then
   the calculated value of the total and used swap
   size will be the sum of all swap partitions. */
static void get_total_and_used_swap(struct free_model *mod)
{
	kvm_t *kvm;
	struct kvm_swap kswap;
	int ret;

	kvm = kvm_open(NULL, "/dev/null", "/dev/null", O_RDONLY, "kvm_open");
	if (kvm == NULL) {
		perror("kvm_open()");
		abort();
	}

	ret = kvm_getswapinfo(kvm, &kswap, 1, 0);
	if (ret == -1) {
		/* Ignore return value */
		kvm_close(kvm);
		perror("kvm_getswapinfo()");
		abort();
	}

	mod->totalswap = CONVERT_UNIT(kswap.ksw_total);
	mod->usedswap = CONVERT_UNIT(kswap.ksw_used);

	kvm_close(kvm);
}

/* Get the size of free (unused) swap space */
static inline void get_free_swap(struct free_model *mod)
{
	get_total_and_used_swap(mod);

	mod->freeswap = mod->totalswap - mod->usedswap;
}

/* Print all collected information about RAM and swap.
   These are, "totalram", "freeram", "usedram",
   "buffer", "shared", "totalswap", "freeswap",
   "usedswap", "total_ram_swap", "free_ram_swap",
   and "used_ram_swap". */
static void print_general_memory(
	struct free_model *mod, int is_pretty, int is_decimal, int is_total)
{
	/* Hold the values temporarily */
	char *totalram, *freeram, *usedram,
		*buffer, *shared, *totalswap,
		*freeswap, *usedswap, *total_ram_swap,
		*free_ram_swap, *used_ram_swap;
	uint64_t unit;

	fprintf(stdout,
		"               total        free        used        buffer       shared\n");
	if (is_pretty) {
		/* RAM information */
		totalram = pretty_format(mod->totalram, is_decimal);
		freeram = pretty_format(mod->freeram, is_decimal);
		usedram = pretty_format(mod->usedram, is_decimal);
		buffer = pretty_format(mod->buffer, is_decimal);
		shared = pretty_format(mod->shared, is_decimal);

		/* Swap information */
		totalswap = pretty_format(mod->totalswap, is_decimal);
		freeswap = pretty_format(mod->freeswap, is_decimal);
		usedswap = pretty_format(mod->usedswap, is_decimal);

		fprintf(stdout,
			"Mem: %15s %11s %11s %13s %12s\n", totalram, freeram,
			usedram, buffer, shared);
		fprintf(stdout,
			"Swap: %14s %11s %11s\n",
		        totalswap, freeswap, usedswap);

		if (is_total) {
			total_ram_swap = pretty_format(mod->totalram + mod->totalswap, is_decimal);
			free_ram_swap = pretty_format(mod->freeram + mod->freeswap, is_decimal);
			used_ram_swap = pretty_format(mod->usedram + mod->usedswap, is_decimal);

			fprintf(stdout,
				"Total: %13s %11s %11s\n",
				total_ram_swap, free_ram_swap, used_ram_swap);

			/* Free allocated buffers */
			free(total_ram_swap);
			free(free_ram_swap);
			free(used_ram_swap);
		}

		/* RAM */
		free(totalram);
		free(freeram);
		free(usedram);
		free(buffer);
		free(shared);

		/* Swap */
		free(totalswap);
		free(freeswap);
		free(usedswap);
	} else {
		unit = is_decimal ? 1000 : 1024;
		fprintf(stdout,
			"Mem: %15lu %11lu %11lu %13lu %12lu\n",
			mod->totalram / unit, mod->freeram / unit, mod->usedram / unit,
			mod->buffer / unit, mod->shared / unit);
		fprintf(stdout,
			"Swap: %14lu %11lu %11lu\n", mod->totalswap / unit,
			mod->freeswap / unit, mod->usedswap / unit);

		if (is_total) {
			fprintf(stdout,
				"Total: %13lu %11lu %11lu\n",
				(mod->totalram + mod->totalswap) / unit,
				(mod->freeram + mod->freeswap) / unit,
				(mod->usedram + mod->usedswap) / unit);
		}
	}
}

/* Print all collected information about RAM and swap,
   and divide them with a divisor.
   Printed values are, "totalram", "freeram", "usedram",
   "buffer", "shared", "totalswap", "freeswap", and
   "usedswap". */
static void print_unit_memory(struct free_model *mod, uint64_t unit)
{
	fprintf(stdout,
		"               total        free        used        buffer       shared\n");
	fprintf(stdout,
		"Mem: %15lu %11lu %11lu %13lu %12lu\n",
	        mod->totalram / unit, mod->freeram / unit,
		mod->usedram / unit, mod->buffer / unit,
		mod->shared / unit);
	fprintf(stdout,
		"Swap: %14lu %11lu %11lu\n",
		mod->totalswap / unit, mod->freeswap / unit,
		mod->usedswap / unit);
}

/* Collect information about RAM and swap and
   pass them to the function print_unit_memory(...). */
static void print_all_uinfo(struct free_model *mod, uint64_t unit)
{
	get_total_memory(mod);
	get_free_memory(mod);
	get_buffer_memory(mod);
	get_used_memory(mod);
	get_shared_memory(mod, 1);

	get_total_and_used_swap(mod);
	get_free_swap(mod);
	print_unit_memory(mod, unit);
}

/* Collect information about RAM and swap and
   pass them to the function print_general_memory(...). */
static void print_all_ginfo(struct free_model *mod, int is_pretty, int is_decimal, int is_total)
{
	get_total_memory(mod);
	get_free_memory(mod);
	get_buffer_memory(mod);
	get_used_memory(mod);
	get_shared_memory(mod, is_decimal);

	get_total_and_used_swap(mod);
	get_free_swap(mod);
	print_general_memory(mod, is_pretty, is_decimal, is_total);
}

/* Show the usage. */
_Noreturn
static void usage(int status)
{
        fputs(_("Usage: free [OPTION]...\n"), stdout);
	fputs(_("Display the amount of space for RAM and swap.\n\n"), stdout);
	fputs(_("Options:\n"), stdout);
	fputs(_("  --bytes        show the output in bytes\n"), stdout);
	fputs(_("  --kilo         show the output in kilobytes\n"), stdout);
	fputs(_("  --mega         show the output in megabytes\n"), stdout);
	fputs(_("  --giga         show the output in gigabytes\n"), stdout);
	fputs(_("  --tera         show the output in terabytes\n"), stdout);
	fputs(_("  --peta         show the output in petabytes\n"), stdout);
	fputs(_("  --kibi         show the output in kibibytes\n"), stdout);
	fputs(_("  --mibi         show the output in mibibytes\n"), stdout);
	fputs(_("  --gibi         show the output in gibibytes\n"), stdout);
	fputs(_("  --tibi         show the output in tibibytes\n"), stdout);
	fputs(_("  --pibi         show the output in pibibytes\n"), stdout);
	fputs(_("  --decimal      use decimal format, e.g. pow(1000, n)\n"), stdout);
	fputs(_("  -h, --human    show the output in human readable form, e.g. 2.3G\n"), stdout);
	fputs(_("  -t, --total    show the sum of total, free, and used RAM and swap\n"), stdout);
	fputs(_("  -s, --secs     continue printing in every N seconds\n"), stdout);
	fputs(_("  -c, --count    continue printing N times and exit\n"), stdout);
	fputs(_("  --help         print this help section\n"), stdout);
	fputs(_("  --version      print the current version\n"), stdout);
	exit(status);
}

/* Show the current version. */
_Noreturn
static void show_version(void)
{
	fputs("free: v"PROGRAM_VERSION"\n", stdout);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
        int opt, secs, count;
        struct option longopts[] = {
		{ "bytes",    no_argument,       NULL, B_OPT },
		{ "kilo",     no_argument,       NULL, K_OPT },
		{ "mega",     no_argument,       NULL, M_OPT },
		{ "giga",     no_argument,       NULL, G_OPT },
		{ "tera",     no_argument,       NULL, T_OPT },
		{ "peta",     no_argument,       NULL, P_OPT },
	        { "kibi",     no_argument,       NULL, Ki_OPT },
		{ "mibi",     no_argument,       NULL, Mi_OPT },
		{ "gibi",     no_argument,       NULL, Gi_OPT },
		{ "tibi",     no_argument,       NULL, Ti_OPT },
		{ "pibi",     no_argument,       NULL, Pi_OPT },
	        { "human",    no_argument,       NULL, HUMAN_OPT },
		{ "decimal",  no_argument,       NULL, DECIMAL_OPT },
		{ "total",    no_argument,       NULL, TOTAL_OPT },
		{ "secs",     required_argument, NULL, SECS_OPT },
		{ "count",    required_argument, NULL, COUNT_OPT },
		{ "help",     no_argument,       NULL, HELP_OPT },
		{ "version",  no_argument,       NULL, VERSION_OPT },
		{ NULL,       0,                 NULL, 0 },
	};
	struct opt_flag flag = {0};
	struct free_model mod = {0};

	opt = secs = count = 0;

	/* Enable localization */
#ifdef ENABLE_LOCALE
	setlocale(LC_ALL, "");
	bindtextdomain("free", "/usr/share/locale/");
	textdomain("free");
#endif

	if (argc >= 2 && argv[1][0] != '-')
		usage(EXIT_FAILURE);
	if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == '\0')
		usage(EXIT_FAILURE);

	while ((opt = getopt_long(argc, argv, "hts:c:", longopts, NULL)) != -1) {
		switch (opt) {
		case B_OPT:
			/* option: --bytes */
			flag.power_flag = TO_B;
		        break;

		case K_OPT:
			/* option: --kilo */
			flag.power_flag = TO_K;
		        break;

		case M_OPT:
			/* option: --mega */
			flag.power_flag = TO_M;
			break;

		case G_OPT:
			/* option: --giga */
			flag.power_flag = TO_G;
		        break;

		case T_OPT:
			/* option: --tera */
			flag.power_flag = TO_T;
		        break;

		case P_OPT:
			/* option: --peta */
			flag.power_flag = TO_P;
		        break;

		case Ki_OPT:
			/* option: --kibi */
		        flag.power_flag = TO_Ki;
			break;

		case Mi_OPT:
			/* option: --mibi */
		        flag.power_flag = TO_Mi;
			break;

		case Gi_OPT:
			/* option: --gibi */
			flag.power_flag = TO_Gi;
			break;

		case Ti_OPT:
			/* option: --tibi */
			flag.power_flag = TO_Ti;
			break;

		case Pi_OPT:
			/* option: --pibi */
			flag.power_flag = TO_Pi;
			break;

		case HUMAN_OPT:
			/* option: --human */
			flag.human_flag = 1;
			break;

		case DECIMAL_OPT:
			/* option: --decimal */
			flag.decimal_flag = 1;
			break;

		case TOTAL_OPT:
			/* option: --total */
			flag.total_flag = 1;
			break;

		case SECS_OPT:
			/* option: --secs */
		        flag.secs_flag = 1;
			if (optarg == NULL)
				usage(EXIT_FAILURE);

		        secs = xatoi(optarg);
			if (secs < 1) {
				fputs(_("free: oops, seconds must not be "),
				      stderr);
				fputs(_("smaller than 1.\n"), stderr);
				exit(EXIT_FAILURE);
			}

			if (secs > (60 * 60 * 60)) {
				fputs(_("free: oops, seconds musn't be "),
				      stderr);
				fputs(_("larger than 216000.\n"), stderr);
				exit(EXIT_FAILURE);
			}
			break;

		case COUNT_OPT:
			/* option: --count */
			flag.count_flag = 1;
		        if (optarg == NULL)
				usage(EXIT_FAILURE);

			count = xatoi(optarg);
			if (count < 1) {
				fputs(_("free: oops, counting must not be "),
				      stderr);
				fputs(_("smaller than 1.\n"), stderr);
				exit(EXIT_FAILURE);
			}

			if (count > 100) {
				fputs(_("free: oops, counting musn't be "),
				      stderr);
				fputs(_("larger than 100.\n"), stderr);
				exit(EXIT_FAILURE);
			}
			break;

		case HELP_OPT:
			/* option: --help */
			usage(EXIT_SUCCESS);
			/* unreachable */

		case VERSION_OPT:
			/* option: --version */
			show_version();
			/* unreachable */

		default:
			/* Exit if the first argument is invalid.
			   Don't iterate for other argument(s). */
		        exit(EXIT_FAILURE);
		}
	}

	if (optind != argc)
		usage(EXIT_FAILURE);

	/* Main loop, it will go on if flag.secs_flag or flag.count_flag
	   is provided as an argument. */
	do {
		if (flag.power_flag) {
			print_all_uinfo(&mod, flag.power_flag);

			if (!flag.count_flag && !flag.secs_flag)
				break;
		}

		if (flag.human_flag)
			print_all_ginfo(&mod, 1, flag.decimal_flag, flag.total_flag);

		/* Default output (with no arguments provided) */
		if (!flag.power_flag && !flag.human_flag)
			print_all_ginfo(&mod, 0, flag.decimal_flag, flag.total_flag);

		if (flag.secs_flag) {
			sleep(secs);
			fputc('\n', stdout);
		}

		if (flag.count_flag) {
			/* If still counting, decrease threshold
			   and add a newline. */
			if (--count > 0)
				fputc('\n', stdout);
			else
				exit(EXIT_SUCCESS);
		}
	} while (flag.secs_flag || flag.count_flag);

	exit(EXIT_SUCCESS);
}
