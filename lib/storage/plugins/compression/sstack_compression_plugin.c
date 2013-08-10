/*************************************************************************
 *
 * SEAMLESSSTACK CONFIDENTIAL
 * __________________________
 *
 *  [2012] - [2013]  SeamlessStack Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of SeamlessStack Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to SeamlessStack Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from SeamlessStack Incorporated.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <lzma.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <policy.h>
#define COMPRESSION_PLUGIN_VERSION 1
#define TMPNAME_MAX 16

/* BSS */
static lzma_stream strm = LZMA_STREAM_INIT;

/* Function signatures */
static bool init_encoder(lzma_stream *);
static bool init_decoder(lzma_stream *);
static bool compress(lzma_stream *, FILE *, FILE *);
static bool decompress(lzma_stream *, const char *,
	FILE *, FILE *);
policy_plugin_t *compression_plugin_init(void);
void compression_plugin_deinit(policy_plugin_t *);
uint32_t compression_plugin_compress(const char *);
uint32_t compression_plugin_decompress(const char *, char *);

/* Functions */

policy_plugin_t *
compression_plugin_init(void)
{
	policy_plugin_t *plugin = NULL;

	plugin = (policy_plugin_t *) malloc(sizeof(policy_plugin_t));
	if (NULL == plugin) 
		return (policy_plugin_t *) NULL;
	plugin->ver = COMPRESSION_PLUGIN_VERSION;
	plugin->is_activated = 0;
	plugin->pp_refcount = 0;
	pthread_spin_init(&plugin->pp_lock, PTHREAD_PROCESS_PRIVATE);
	plugin->init_policy = compression_plugin_init;
	plugin->deinit_policy = compression_plugin_deinit;
	plugin->apply_policy = compression_plugin_compress;
	plugin->remove_policy = compression_plugin_decompress;

	return plugin;
}

void
compression_plugin_deinit(policy_plugin_t *plugin)
{
	if (plugin)
		free(plugin);
}

/*
 * compression_plugin_compress - apply_policy entry point for compression
 *								plugin
 *
 * path - Full path of the file to be compressed. Final output will also
 * 			be stored in the same file
 *
 * Returns 0 on success and negative number on failure.
 */

uint32_t
compression_plugin_compress(const char *path)
{
	FILE *input = NULL;
	FILE *output = NULL;
	char tmpnam[TMPNAME_MAX] = { '\0' };
	int fd = -1;
	int ret = -1;

	if (access(path, R_OK))
		return -1;
	if (!init_encoder(&strm))
		return -1;
	/*
	 * Create a new temporary file to store compressed data.
	 * This file will be renamed to path later
	 */
	strcpy((char *) tmpnam, "/tmp/tempXXXXXX");
	fd = mkostemp(tmpnam, O_RDWR);
	if (fd == -1) 
		return -1;
	input = fopen(path, "r");
	if (NULL == input)
		return -1;
	output = fdopen(fd, "rw");
	if (NULL == output)
		return -1;

	ret = compress(&strm, input, output);
	if (ret != true)
		return -2;

	lzma_end(&strm);
	fclose(input);
	if (fclose(output)) {
		fprintf(stderr, "Write error: %s\n", strerror(errno));
		return -2;
	}

	if (rename(tmpnam, path) == -1)
		return -errno;

	return 0;
}	

/*
 * compression_plugin_compress - apply_policy entry point for compression
 *								plugin
 *
 * path - Full path of the file to be compressed.
 * buffer - Buffer that contains the uncompressed output. This is allocated 
 *			inside this function. It is caller's responsibility to free
 *
 * Returns size of uncompressed data on success and negative number on failure.
 */

uint32_t
compression_plugin_decompress(const char *path, char *buffer)
{
	FILE *input = NULL;
	FILE *output = NULL;
	char tmpnam[TMPNAME_MAX] = { '\0' };
	int fd = -1;
	int ret = -1;
	struct stat statbuf = { '\0' };
	size_t size;

	if (access(path, R_OK))
		return -1;
	if (!init_decoder(&strm))
		return -1;
	/*
	 * Create a new temporary file to store compressed data.
	 * This file will be renamed to path later
	 */
	strcpy(tmpnam, "/tmp/tempXXXXXX");
	fd = mkostemp(tmpnam, O_RDWR);
	if (fd == -1) 
		return -1;
	input = fopen(path, "r");
	if (NULL == input)
		return -1;
	output = fdopen(fd, "rw");
	if (NULL == output)
		return -1;

	ret = decompress(&strm, path, input, output);
	if (ret != true)
		return -2;

	lzma_end(&strm);
	fclose(input);
	if (fclose(output)) {
		fprintf(stderr, "Write error: %s\n", strerror(errno));
		return -2;
	}
	close(fd);

	// Return size of the decompressed output and return buffer
	if (stat(tmpnam, &statbuf) == -1) {
		fprintf(stderr, "Failed to stat: %s\n", strerror(errno));
		return -3;
	}
	buffer = malloc(statbuf.st_size);
	if (NULL == buffer)
		return -ENOMEM;

	fd = open(tmpnam, O_RDONLY);
	if (fd == -1)
		return -errno;
	size = read(fd, buffer, statbuf.st_size);
	// Try once more if read was interrupted
	if (size == 0 && errno == EINTR) {
		size = read(fd, buffer, statbuf.st_size);
	}

	if (size < statbuf.st_size) {
		close(fd);
		return -1;
	}
	close(fd);		

	unlink(tmpnam);

	return statbuf.st_size;
}

static bool
init_encoder(lzma_stream *strm)
{
	// Use the default preset (6) for LZMA2.
	//
	// The lzma_options_lzma structure and the lzma_lzma_preset() function
	// are declared in lzma/lzma.h (src/liblzma/api/lzma/lzma.h in the
	// source package or e.g. /usr/include/lzma/lzma.h depending on
	// the install prefix).
	lzma_options_lzma opt_lzma2;
	if (lzma_lzma_preset(&opt_lzma2, LZMA_PRESET_DEFAULT)) {
		// It should never fail because the default preset
		// (and presets 0-9 optionally with LZMA_PRESET_EXTREME)
		// are supported by all stable liblzma versions.
		//
		// (The encoder initialization later in this function may
		// still fail due to unsupported preset *if* the features
		// required by the preset have been disabled at build time,
		// but no-one does such things except on embedded systems.)
		fprintf(stderr, "Unsupported preset, possibly a bug\n");
		return false;
	}

	// Now we could customize the LZMA2 options if we wanted. For example,
	// we could set the the dictionary size (opt_lzma2.dict_size) to
	// something else than the default (8 MiB) of the default preset.
	// See lzma/lzma.h for details of all LZMA2 options.
	//
	// The x86 BCJ filter will try to modify the x86 instruction stream so
	// that LZMA2 can compress it better. The x86 BCJ filter doesn't need
	// any options so it will be set to NULL below.
	//
	// Construct the filter chain. The uncompressed data goes first to
	// the first filter in the array, in this case the x86 BCJ filter.
	// The array is always terminated by setting .id = LZMA_VLI_UNKNOWN.
	//
	// See lzma/filter.h for more information about the lzma_filter
	// structure.
	lzma_filter filters[] = {
		{ .id = LZMA_FILTER_X86, .options = NULL },
		{ .id = LZMA_FILTER_LZMA2, .options = &opt_lzma2 },
		{ .id = LZMA_VLI_UNKNOWN, .options = NULL },
	};

	// Initialize the encoder using the custom filter chain.
	lzma_ret ret = lzma_stream_encoder(strm, filters, LZMA_CHECK_CRC64);

	if (ret == LZMA_OK)
		return true;

	const char *msg;
	switch (ret) {
	case LZMA_MEM_ERROR:
		msg = "Memory allocation failed";
		break;

	case LZMA_OPTIONS_ERROR:
		// We are no longer using a plain preset so this error
		// message has been edited accordingly compared to
		// 01_compress_easy.c.
		msg = "Specified filter chain is not supported";
		break;

	case LZMA_UNSUPPORTED_CHECK:
		msg = "Specified integrity check is not supported";
		break;

	default:
		msg = "Unknown error, possibly a bug";
		break;
	}

	fprintf(stderr, "Error initializing the encoder: %s (error code %u)\n",
			msg, ret);
	return false;
}


// This function is identical to the one in 01_compress_easy.c.
static bool
compress(lzma_stream *strm, FILE *infile, FILE *outfile)
{
	lzma_action action = LZMA_RUN;

	uint8_t inbuf[BUFSIZ];
	uint8_t outbuf[BUFSIZ];

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = outbuf;
	strm->avail_out = sizeof(outbuf);

	while (true) {
		if (strm->avail_in == 0 && !feof(infile)) {
			strm->next_in = inbuf;
			strm->avail_in = fread(inbuf, 1, sizeof(inbuf),
					infile);

			if (ferror(infile)) {
				fprintf(stderr, "Read error: %s\n",
						strerror(errno));
				return false;
			}

			if (feof(infile))
				action = LZMA_FINISH;
		}

		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			size_t write_size = sizeof(outbuf) - strm->avail_out;

			if (fwrite(outbuf, 1, write_size, outfile)
					!= write_size) {
				fprintf(stderr, "Write error: %s\n",
						strerror(errno));
				return false;
			}

			strm->next_out = outbuf;
			strm->avail_out = sizeof(outbuf);
		}

		if (ret != LZMA_OK) {
			if (ret == LZMA_STREAM_END)
				return true;

			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_DATA_ERROR:
				msg = "File size limits exceeded";
				break;

			default:
				msg = "Unknown error, possibly a bug";
				break;
			}

			fprintf(stderr, "Encoder error: %s (error code %u)\n",
					msg, ret);
			return false;
		}
	}
}


static bool
init_decoder(lzma_stream *strm)
{
	// Initialize a .xz decoder. The decoder supports a memory usage limit
	// and a set of flags.
	//
	// The memory usage of the decompressor depends on the settings used
	// to compress a .xz file. It can vary from less than a megabyte to
	// a few gigabytes, but in practice (at least for now) it rarely
	// exceeds 65 MiB because that's how much memory is required to
	// decompress files created with "xz -9". Settings requiring more
	// memory take extra effort to use and don't (at least for now)
	// provide significantly better compression in most cases.
	//
	// Memory usage limit is useful if it is important that the
	// decompressor won't consume gigabytes of memory. The need
	// for limiting depends on the application. In this example,
	// no memory usage limiting is used. This is done by setting
	// the limit to UINT64_MAX.
	//
	// The .xz format allows concatenating compressed files as is:
	//
	//     echo foo | xz > foobar.xz
	//     echo bar | xz >> foobar.xz
	//
	// When decompressing normal standalone .xz files, LZMA_CONCATENATED
	// should always be used to support decompression of concatenated
	// .xz files. If LZMA_CONCATENATED isn't used, the decoder will stop
	// after the first .xz stream. This can be useful when .xz data has
	// been embedded inside another file format.
	//
	// Flags other than LZMA_CONCATENATED are supported too, and can
	// be combined with bitwise-or. See lzma/container.h
	// (src/liblzma/api/lzma/container.h in the source package or e.g.
	// /usr/include/lzma/container.h depending on the install prefix)
	// for details.
	lzma_ret ret = lzma_stream_decoder(
			strm, UINT64_MAX, LZMA_CONCATENATED);

	// Return successfully if the initialization went fine.
	if (ret == LZMA_OK)
		return true;

	// Something went wrong. The possible errors are documented in
	// lzma/container.h (src/liblzma/api/lzma/container.h in the source
	// package or e.g. /usr/include/lzma/container.h depending on the
	// install prefix).
	//
	// Note that LZMA_MEMLIMIT_ERROR is never possible here. If you
	// specify a very tiny limit, the error will be delayed until
	// the first headers have been parsed by a call to lzma_code().
	const char *msg;
	switch (ret) {
	case LZMA_MEM_ERROR:
		msg = "Memory allocation failed";
		break;

	case LZMA_OPTIONS_ERROR:
		msg = "Unsupported decompressor flags";
		break;

	default:
		// This is most likely LZMA_PROG_ERROR indicating a bug in
		// this program or in liblzma. It is inconvenient to have a
		// separate error message for errors that should be impossible
		// to occur, but knowing the error code is important for
		// debugging. That's why it is good to print the error code
		// at least when there is no good error message to show.
		msg = "Unknown error, possibly a bug";
		break;
	}

	fprintf(stderr, "Error initializing the decoder: %s (error code %u)\n",
			msg, ret);
	return false;
}


static bool
decompress(lzma_stream *strm, const char *inname, FILE *infile, FILE *outfile)
{
	// When LZMA_CONCATENATED flag was used when initializing the decoder,
	// we need to tell lzma_code() when there will be no more input.
	// This is done by setting action to LZMA_FINISH instead of LZMA_RUN
	// in the same way as it is done when encoding.
	//
	// When LZMA_CONCATENATED isn't used, there is no need to use
	// LZMA_FINISH to tell when all the input has been read, but it
	// is still OK to use it if you want. When LZMA_CONCATENATED isn't
	// used, the decoder will stop after the first .xz stream. In that
	// case some unused data may be left in strm->next_in.
	lzma_action action = LZMA_RUN;

	uint8_t inbuf[BUFSIZ];
	uint8_t outbuf[BUFSIZ];

	strm->next_in = NULL;
	strm->avail_in = 0;
	strm->next_out = outbuf;
	strm->avail_out = sizeof(outbuf);

	while (true) {
		if (strm->avail_in == 0 && !feof(infile)) {
			strm->next_in = inbuf;
			strm->avail_in = fread(inbuf, 1, sizeof(inbuf),
					infile);

			if (ferror(infile)) {
				fprintf(stderr, "%s: Read error: %s\n",
						inname, strerror(errno));
				return false;
			}

			// Once the end of the input file has been reached,
			// we need to tell lzma_code() that no more input
			// will be coming. As said before, this isn't required
			// if the LZMA_CONATENATED flag isn't used when
			// initializing the decoder.
			if (feof(infile))
				action = LZMA_FINISH;
		}

		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			size_t write_size = sizeof(outbuf) - strm->avail_out;

			if (fwrite(outbuf, 1, write_size, outfile)
					!= write_size) {
				fprintf(stderr, "Write error: %s\n",
						strerror(errno));
				return false;
			}

			strm->next_out = outbuf;
			strm->avail_out = sizeof(outbuf);
		}

		if (ret != LZMA_OK) {
			// Once everything has been decoded successfully, the
			// return value of lzma_code() will be LZMA_STREAM_END.
			//
			// It is important to check for LZMA_STREAM_END. Do not
			// assume that getting ret != LZMA_OK would mean that
			// everything has gone well or that when you aren't
			// getting more output it must have successfully
			// decoded everything.
			if (ret == LZMA_STREAM_END)
				return true;

			// It's not LZMA_OK nor LZMA_STREAM_END,
			// so it must be an error code. See lzma/base.h
			// (src/liblzma/api/lzma/base.h in the source package
			// or e.g. /usr/include/lzma/base.h depending on the
			// install prefix) for the list and documentation of
			// possible values. Many values listen in lzma_ret
			// enumeration aren't possible in this example, but
			// can be made possible by enabling memory usage limit
			// or adding flags to the decoder initialization.
			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_FORMAT_ERROR:
				// .xz magic bytes weren't found.
				msg = "The input is not in the .xz format";
				break;

			case LZMA_OPTIONS_ERROR:
				// For example, the headers specify a filter
				// that isn't supported by this liblzma
				// version (or it hasn't been enabled when
				// building liblzma, but no-one sane does
				// that unless building liblzma for an
				// embedded system). Upgrading to a newer
				// liblzma might help.
				//
				// Note that it is unlikely that the file has
				// accidentally became corrupt if you get this
				// error. The integrity of the .xz headers is
				// always verified with a CRC32, so
				// unintentionally corrupt files can be
				// distinguished from unsupported files.
				msg = "Unsupported compression options";
				break;

			case LZMA_DATA_ERROR:
				msg = "Compressed file is corrupt";
				break;

			case LZMA_BUF_ERROR:
				// Typically this error means that a valid
				// file has got truncated, but it might also
				// be a damaged part in the file that makes
				// the decoder think the file is truncated.
				// If you prefer, you can use the same error
				// message for this as for LZMA_DATA_ERROR.
				msg = "Compressed file is truncated or "
						"otherwise corrupt";
				break;

			default:
				// This is most likely LZMA_PROG_ERROR.
				msg = "Unknown error, possibly a bug";
				break;
			}

			fprintf(stderr, "%s: Decoder error: "
					"%s (error code %u)\n",
					inname, msg, ret);
			return false;
		}
	}
}

