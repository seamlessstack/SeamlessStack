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
#include <sys/mman.h>
#include <sstack_log.h>
//#include <policy.h>
#define COMPRESSION_PLUGIN_VERSION 1
#define TMPNAME_MAX 16
#define BUFSIZE 4096

/* BSS */
static lzma_stream strm = LZMA_STREAM_INIT;
static log_ctx_t *ctx = NULL;

/* Function signatures */
static bool init_encoder(lzma_stream *);
static bool init_decoder(lzma_stream *);
static size_t compress(lzma_stream *, char *, char **, size_t );
static size_t decompress(lzma_stream *, char *, char **, size_t );
void compression_init(void);
void compression_deinit(void);
size_t compression_apply(char *, char **, size_t );
size_t compression_remove(char *, char **, size_t );

/* Functions */

void
compression_init(void)
{
	// Placeholder for now
	// At present, this function only initializes logging
	// Revisit
	ctx = sfs_create_log_ctx();
	if (ctx) {
		(void) sfs_log_init(ctx, SFS_INFO, "compression_plugin");
	}
}

void
compression_deinit(void)
{
	(void) sfs_log_close(ctx);
	(void) sfs_destroy_log_ctx(ctx);	
}


/*
 * compression_apply - Compress plugin "apply"
 *
 * buf - input buffer
 * outbuf - Output buffer (will be allocated inside)
 * size - input buffer size
 *
 * Returns output buffer size if successful. Otherwise returns -1.
 */

size_t
compression_apply(char *buf, char **outbuf, size_t size)
{
	size_t ret;

	if (!init_encoder(&strm)) {
		sfs_log(ctx, SFS_ERR, "%s: init_encoder failed \n", __FUNCTION__);
		return -1;
	}

	ret = compress(&strm, buf, outbuf, size);
	lzma_end(&strm);

	return ret;
}

/*
 * compression_remove - apply_policy entry point for compression
 *								plugin
 *
 * path - Full path of the file to be compressed.
 * buffer - Buffer that contains the uncompressed output. This is allocated 
 *			inside this function. It is caller's responsibility to free
 *
 * Returns size of uncompressed data on success and negative number on failure.
 */

size_t
compression_remove(char *buf, char **outbuf, size_t size)
{
	size_t ret;

	if (!init_decoder(&strm)) {
		sfs_log(ctx, SFS_ERR, "%s: init_decoder failed \n", __FUNCTION__);
		return -1;
	}

	ret = decompress(&strm, buf, outbuf, size);
	lzma_end(&strm);

	return ret;
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
		sfs_log(ctx, SFS_ERR, "%s:Unsupported preset, possibly a bug\n",
			   __FUNCTION__);
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

	sfs_log(ctx, SFS_ERR, "%s: Error initializing the encoder: "
			"%s (error code %u) \n", __FUNCTION__, msg, ret);
	return false;
}


// This function is identical to the one in 01_compress_easy.c.
/*
 * compress - Compress the input buffer using LZMA algorithm
 *
 * strm - LZMA strem. Should be non-NULL.
 * buf - input buffer. Should be non-NULL
 * out_buf - Output buffer. Allocated inside the function
 *
 * Returns output bufer size if successful. Otherwise returns -1.
 */

static size_t
compress(lzma_stream *strm, char *buf, char **out_buf,  size_t size)
{
	lzma_action action = LZMA_RUN;
	int covered = 0;
	int total_written = 0;
	int ret = -1;
	char outbuf[BUFSIZE];
	char *local_buf = NULL;

	strm->next_in = (uint8_t *) NULL;
	strm->avail_in = 0;

	strm->next_out = (uint8_t *) outbuf;
	strm->avail_out = sizeof(outbuf);

	*out_buf = NULL;

	while(1) {
		if (covered == size)
			action = LZMA_FINISH;

		if (strm->avail_in == 0 && covered < size) {
			if ((size - covered) < BUFSIZE)
				strm->avail_in = (size - covered);
			else
				strm->avail_in = BUFSIZE;
			strm->next_in = buf + covered;

			if (covered == size)
				action = LZMA_FINISH;

			covered +=  strm->avail_in;
		}
		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			local_buf = realloc(*out_buf, (sizeof(outbuf) - strm->avail_out));
			if (NULL == local_buf) {
				sfs_log(ctx, SFS_ERR, "%s: Realloc failed on loca_buf \n",
						__FUNCTION__);
				free (*out_buf);
				return -1;
			}
			*out_buf = local_buf;
			memcpy(*out_buf + total_written , outbuf, sizeof(outbuf) -
							strm->avail_out);
			total_written += ((sizeof(outbuf) - strm->avail_out));
			strm->next_out = outbuf;
			strm->avail_out = sizeof(outbuf);
		}

		if (ret != LZMA_OK) {

			if (ret == LZMA_STREAM_END)
				return strm->total_out;

			switch (ret) {
				case LZMA_MEM_ERROR:
					sfs_log(ctx, SFS_ERR, "%s: Memory allocation failed \n",
							__FUNCTION__);
					return -1;
	
				case LZMA_DATA_ERROR:
					sfs_log(ctx, SFS_ERR, "%s: File size limits exceeded \n",
							__FUNCTION__);
					return -1;
	
				default:
					sfs_log(ctx, SFS_ERR, "%s: Unknown error, possibly a bug \n",
							__FUNCTION__);
					return -1;
			}
		}
	}

	return -1;
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

	sfs_log(ctx, SFS_ERR, "%s: Error initializing the decoder: "
			"%s (error code %u) \n", __FUNCTION__, msg, ret);
	return false;
}


static size_t
decompress(lzma_stream *strm, char *buf, char **out_buf, size_t size)
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
	int covered = 0;
	int total_written = 0;
	int ret = -1;
	char outbuf[BUFSIZE];
	char *local_buf = NULL;

	strm->next_in = (uint8_t *) NULL;
	strm->avail_in = 0;
	strm->next_out =(uint8_t *) outbuf;
	strm->avail_out = sizeof(outbuf);

	while (1) {
		if (covered == size)
			action = LZMA_FINISH;

		if ((strm->avail_in == 0) && (covered < size)) {
			if ((size - covered) < BUFSIZE)
				strm->avail_in = (size - covered);
			else
				strm->avail_in = BUFSIZE;
			strm->next_in = buf + covered;

			// Once the end of the input has been reached,
			// we need to tell lzma_code() that no more input
			// will be coming. As said before, this isn't required
			// if the LZMA_CONATENATED flag isn't used when
			// initializing the decoder.
			if (covered == size)
				action = LZMA_FINISH;

			covered += strm->avail_in;
		}

		lzma_ret ret = lzma_code(strm, action);

		if (strm->avail_out == 0 || ret == LZMA_STREAM_END) {
			local_buf = realloc(*out_buf, (sizeof(outbuf) - strm->avail_out));
			if (NULL == local_buf) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
						"local_buf \n", __FUNCTION__);
				free (*out_buf);
				return -1;
			}
			*out_buf = local_buf;

			memcpy(*out_buf + total_written, outbuf, sizeof(outbuf) -
							strm->avail_out);
			total_written += ((sizeof(outbuf) - strm->avail_out));
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
				return strm->total_out;

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
				// Memory allocation failed
				errno = ENOMEM;

				return -1;

			case LZMA_FORMAT_ERROR:
				// .xz magic bytes weren't found.
				errno = EINVAL;

				return -1;

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
				errno = EINVAL;
				return -1;

			case LZMA_DATA_ERROR:
				errno = EIO;

				return -1;

			case LZMA_BUF_ERROR:
				// Typically this error means that a valid
				// file has got truncated, but it might also
				// be a damaged part in the file that makes
				// the decoder think the file is truncated.
				// If you prefer, you can use the same error
				// message for this as for LZMA_DATA_ERROR.
				errno = EIO;

				return -1;

			default:
				// This is most likely LZMA_PROG_ERROR.
				errno = EINVAL;

				return -1;
			}
		}
	}
}
