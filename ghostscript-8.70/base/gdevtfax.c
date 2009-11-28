/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: gdevtfax.c 9719 2009-05-02 00:38:13Z ray $ */
/* TIFF and TIFF/fax devices */
#include "gdevprn.h"
#include "gdevtifs.h"
#include "strimpl.h"
#include "scfx.h"
#include "gdevfax.h"
#include "gdevtfax.h"		/* for gdev_fax_print_page_stripped proto */

/* ---------------- TIFF/fax output ---------------- */

/* The device descriptors */

static dev_proc_get_params(tfax_get_params);
static dev_proc_put_params(tfax_put_params);
static dev_proc_print_page(tiffcrle_print_page);
static dev_proc_print_page(tiffg3_print_page);
static dev_proc_print_page(tiffg32d_print_page);
static dev_proc_print_page(tiffg4_print_page);

struct gx_device_tfax_s {
    gx_device_common;
    gx_prn_device_common;
    gx_fax_device_common;
    long MaxStripSize;		/* 0 = no limit, other is UNCOMPRESSED limit */
                                /* The type and range of FillOrder follows TIFF 6 spec  */
    int  FillOrder;             /* 1 = lowest column in the high-order bit, 2 = reverse */
    bool  BigEndian;            /* true = big endian; false = little endian*/
    gdev_tiff_state tiff;	/* for TIFF output only */
};
typedef struct gx_device_tfax_s gx_device_tfax;

/* Define procedures that adjust the paper size. */
static const gx_device_procs gdev_tfax_std_procs =
    prn_params_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		     tfax_get_params, tfax_put_params);

#define TFAX_DEVICE(dname, print_page)\
{\
    FAX_DEVICE_BODY(gx_device_tfax, gdev_tfax_std_procs, dname, print_page),\
    0				/* unlimited strip size byte count */,\
    1                           /* lowest column in the high-order bit */,\
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/\
}

const gx_device_tfax gs_tiffcrle_device =
    TFAX_DEVICE("tiffcrle", tiffcrle_print_page);

const gx_device_tfax gs_tiffg3_device =
    TFAX_DEVICE("tiffg3", tiffg3_print_page);

const gx_device_tfax gs_tiffg32d_device =
    TFAX_DEVICE("tiffg32d", tiffg32d_print_page);

const gx_device_tfax gs_tiffg4_device =
    TFAX_DEVICE("tiffg4", tiffg4_print_page);

/* Get/put the MaxStripSize parameter. */
static int
tfax_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    int code = gdev_fax_get_params(dev, plist);
    int ecode = code;
    
    if ((code = param_write_long(plist, "MaxStripSize", &tfdev->MaxStripSize)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "FillOrder", &tfdev->FillOrder)) < 0)
        ecode = code;
    if ((code = param_write_bool(plist, "BigEndian", &tfdev->BigEndian)) < 0)
        ecode = code;
    return ecode;
}
static int
tfax_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    int ecode = 0;
    int code;
    long mss = tfdev->MaxStripSize;
    int fill_order = tfdev->FillOrder;
    const char *param_name;
    bool big_endian = tfdev->BigEndian;

    switch (code = param_read_long(plist, (param_name = "MaxStripSize"), &mss)) {
        case 0:
	    /*
	     * Strip must be large enough to accommodate a raster line.
	     * If the max strip size is too small, we still write a single
	     * line per strip rather than giving an error.
	     */
	    if (mss >= 0)
	        break;
	    code = gs_error_rangecheck;
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 1:
	    break;
    }

    /* Following TIFF spec, FillOrder is integer */ 
    switch (code = param_read_int(plist, (param_name = "FillOrder"), &fill_order)) {
        case 0:
	    if (fill_order == 1 || fill_order == 2)
	        break;
	    code = gs_error_rangecheck;
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 1:
	    break;
    }
    
    /* Read BigEndian option as bool */ 
    switch (code = param_read_bool(plist, (param_name = "BigEndian"), &big_endian)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
        case 0:
	case 1:
	    break;
    }

    if (ecode < 0)
	return ecode;
    code = gdev_fax_put_params(dev, plist);
    if (code < 0)
	return code;

    tfdev->MaxStripSize = mss;
    tfdev->FillOrder = fill_order;
    tfdev->BigEndian = big_endian;
    return code;
}

/* Send the page to the printer. */
/* Print a page with a specified width, which may differ from	*/
/* the width stored in the device. The TIFF file may have	*/
/* multiple strips of height 'rows'.				*/
static int
gdev_stream_print_page_strips(gx_device_printer * pdev, FILE * prn_stream,
			      const stream_template * temp, stream_state * ss,
			      int width, long rows_per_strip)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)pdev;
    int row, row_next;
    int code = 0;

    for (row = 0; row < pdev->height; row = row_next) {
	row_next = min(row + rows_per_strip, pdev->height);
	code = gdev_fax_print_strip(pdev, prn_stream, temp, ss, width,
				    row, row_next);
	if (code < 0)
	    break;
	gdev_tiff_end_strip(&tfdev->tiff, prn_stream);

    }
    return code;
}

/* Print a page with a specified width, which may differ from the */
/* width stored in the device. */
static int
gdev_stream_print_page_width(gx_device_printer * pdev, FILE * prn_stream,
			     const stream_template * temp, stream_state * ss,
			     int width)
{
    return gdev_stream_print_page_strips(pdev, prn_stream, temp, ss,
					 width, pdev->height);
}

static int
gdev_stream_print_page(gx_device_printer * pdev, FILE * prn_stream,
		       const stream_template * temp, stream_state * ss)
{
    return gdev_stream_print_page_width(pdev, prn_stream, temp, ss,
					pdev->width);
}

/* Print a fax page.  Other fax drivers use this. */
int
gdev_fax_print_page_stripped(gx_device_printer * pdev, FILE * prn_stream,
		    stream_CFE_state * ss, long rows)
{
    return gdev_stream_print_page_strips(pdev, prn_stream, &s_CFE_template,
					 (stream_state *)ss, ss->Columns,
					 rows);
}

/* ---------------- Other TIFF output ---------------- */

#include "slzwx.h"
#include "srlx.h"

/* Device descriptors for TIFF formats other than fax. */
static dev_proc_print_page(tifflzw_print_page);
static dev_proc_print_page(tiffpack_print_page);

const gx_device_tfax gs_tifflzw_device = {
    prn_device_std_body(gx_device_tfax, gdev_tfax_std_procs, "tifflzw",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,	/* margins */
			1, tifflzw_print_page),
    1				/* AdjustWidth, not used */,
    0				/* unlimited strip size byte count */,
    1                           /* lowest column in the high-order bit, not used */,
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/
};

const gx_device_tfax gs_tiffpack_device = {
    prn_device_std_body(gx_device_tfax, gdev_tfax_std_procs, "tiffpack",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,	/* margins */
			1, tiffpack_print_page),
    1				/* AdjustWidth, not used */,
    0				/* unlimited strip size byte count */,
    1                           /* lowest column in the high-order bit, not used */,
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/
};

/* Define the TIFF directory we use, beyond the standard entries. */
/* NB: this array is sorted by tag number (assumed below) */
typedef struct tiff_mono_directory_s {
    TIFF_dir_entry BitsPerSample;
    TIFF_dir_entry Compression;
    TIFF_dir_entry Photometric;
    TIFF_dir_entry FillOrder;
    TIFF_dir_entry SamplesPerPixel;
    TIFF_dir_entry T4T6Options;
    /* Don't use CleanFaxData. */
    /*  TIFF_dir_entry  CleanFaxData;   */
} tiff_mono_directory;
static const tiff_mono_directory dir_mono_template =
{
    {TIFFTAG_BitsPerSample, TIFF_SHORT, 1, 1},
    {TIFFTAG_Compression, TIFF_SHORT, 1, Compression_CCITT_T4},
    {TIFFTAG_Photometric, TIFF_SHORT, 1, Photometric_min_is_white},
    {TIFFTAG_FillOrder, TIFF_SHORT, 1, FillOrder_MSB2LSB},
    {TIFFTAG_SamplesPerPixel, TIFF_SHORT, 1, 1},
    {TIFFTAG_T4Options, TIFF_LONG, 1, 0},
	/* { TIFFTAG_CleanFaxData,      TIFF_SHORT, 1, CleanFaxData_clean }, */
};

/* Forward references */
static int tfax_begin_page(gx_device_tfax *, FILE *,
			    const tiff_mono_directory *, int);

/* Print a fax-encoded page. */
static int
tifff_print_page(gx_device_printer * dev, FILE * prn_stream,
		 stream_CFE_state * pstate, tiff_mono_directory * pdir)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    int code;

    pdir->FillOrder.value = tfdev->FillOrder;
    tfax_begin_page(tfdev, prn_stream, pdir, pstate->Columns);
    pstate->FirstBitLowOrder = tfdev->FillOrder == 2;
    code = gdev_fax_print_page_stripped(dev, prn_stream, pstate, tfdev->tiff.rows);
    gdev_tiff_end_page(&tfdev->tiff, prn_stream, tfdev->BigEndian != arch_is_big_endian);
    return code;
}
static int
tiffcrle_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    stream_CFE_state state;
    tiff_mono_directory dir;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)dev);
    state.EndOfLine = false;
    state.EncodedByteAlign = true;
    dir = dir_mono_template;
    dir.Compression.value = Compression_CCITT_RLE;
    dir.T4T6Options.tag = TIFFTAG_T4Options;
    dir.T4T6Options.value = T4Options_fill_bits;
    return tifff_print_page(dev, prn_stream, &state, &dir);
}
static int
tiffg3_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    stream_CFE_state state;
    tiff_mono_directory dir;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)dev);
    state.EndOfLine = true;
    state.EncodedByteAlign = true;
    dir = dir_mono_template;
    dir.Compression.value = Compression_CCITT_T4;
    dir.T4T6Options.tag = TIFFTAG_T4Options;
    dir.T4T6Options.value = T4Options_fill_bits;
    return tifff_print_page(dev, prn_stream, &state, &dir);
}
static int
tiffg32d_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    stream_CFE_state state;
    tiff_mono_directory dir;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)dev);
    state.K = (dev->y_pixels_per_inch < 100 ? 2 : 4);
    state.EndOfLine = true;
    state.EncodedByteAlign = true;
    dir = dir_mono_template;
    dir.Compression.value = Compression_CCITT_T4;
    dir.T4T6Options.tag = TIFFTAG_T4Options;
    dir.T4T6Options.value = T4Options_2D_encoding | T4Options_fill_bits;
    return tifff_print_page(dev, prn_stream, &state, &dir);
}
static int
tiffg4_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    stream_CFE_state state;
    tiff_mono_directory dir;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)dev);
    state.K = -1;
    /*state.EncodedByteAlign = false; *//* no fill_bits option for T6 */
    dir = dir_mono_template;
    dir.Compression.value = Compression_CCITT_T6;
    dir.T4T6Options.tag = TIFFTAG_T6Options;
    return tifff_print_page(dev, prn_stream, &state, &dir);
}

/* Print an LZW page. */
static int
tifflzw_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    tiff_mono_directory dir;
    stream_LZW_state state;
    int code;

    dir = dir_mono_template;
    dir.Compression.value = Compression_LZW;
    dir.FillOrder.value = FillOrder_MSB2LSB;
    tfax_begin_page(tfdev, prn_stream, &dir, dev->width);
    state.InitialCodeLength = 8;
    state.FirstBitLowOrder = false;
    state.BlockData = false;
    state.EarlyChange = 1;	/* PLRM is sort of confusing, but this is correct */
    code = gdev_stream_print_page(dev, prn_stream, &s_LZWE_template,
				  (stream_state *) & state);
    gdev_tiff_end_page(&tfdev->tiff, prn_stream, tfdev->BigEndian != arch_is_big_endian);
    return code;
}

/* Print a PackBits page. */
static int
tiffpack_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    tiff_mono_directory dir;
    stream_RLE_state state;
    int code;

    dir = dir_mono_template;
    dir.Compression.value = Compression_PackBits;
    dir.FillOrder.value = FillOrder_MSB2LSB;
    tfax_begin_page(tfdev, prn_stream, &dir, dev->width);
    state.EndOfData = false;
    state.record_size = gdev_mem_bytes_per_scan_line((gx_device *) dev);
    code = gdev_stream_print_page(dev, prn_stream, &s_RLE_template,
				  (stream_state *) & state);
    gdev_tiff_end_page(&tfdev->tiff, prn_stream, tfdev->BigEndian != arch_is_big_endian);
    return code;
}

/* Begin a TIFF fax page. */
static int
tfax_begin_page(gx_device_tfax * tfdev, FILE * fp,
		const tiff_mono_directory * pdir, int width)
{
    /* Patch the width to reflect fax page width adjustment. */
    int save_width = tfdev->width;
    int code;

    tfdev->width = width;
    code = gdev_tiff_begin_page((gx_device_printer *) tfdev,
				&tfdev->tiff, fp,
				(const TIFF_dir_entry *)pdir,
				sizeof(*pdir) / sizeof(TIFF_dir_entry),
				NULL, 0, tfdev->MaxStripSize, tfdev->BigEndian != arch_is_big_endian);
    tfdev->width = save_width;
    return code;
}
