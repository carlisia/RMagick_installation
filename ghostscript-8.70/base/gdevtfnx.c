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

/* $Id: gdevtfnx.c 9719 2009-05-02 00:38:13Z ray $ */
/* 12-bit & 24-bit RGB uncompressed TIFF driver */
#include "gdevprn.h"
#include "gdevtifs.h"

/*
 * Thanks to Alan Barclay <alan@escribe.co.uk> for donating the original
 * version of this code to Ghostscript.
 */

/* ------ The device descriptors ------ */

/* Default X and Y resolution */
#define X_DPI 72
#define Y_DPI 72

static dev_proc_print_page(tiff12_print_page);
static dev_proc_print_page(tiff24_print_page);

static const gx_device_procs tiff12_procs =
prn_color_params_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb,
		tiff_get_params, tiff_put_params);
static const gx_device_procs tiff24_procs =
prn_color_params_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb,
		tiff_get_params, tiff_put_params);

const gx_device_tiff gs_tiff12nc_device = {
    prn_device_std_body(gx_device_tiff, tiff12_procs, "tiff12nc",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,
			24, tiff12_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/      
};

const gx_device_tiff gs_tiff24nc_device = {
    prn_device_std_body(gx_device_tiff, tiff24_procs, "tiff24nc",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,
			24, tiff24_print_page),
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/      
};

/* ------ Private definitions ------ */

/* Define our TIFF directory - sorted by tag number */
typedef struct tiff_rgb_directory_s {
    TIFF_dir_entry BitsPerSample;
    TIFF_dir_entry Compression;
    TIFF_dir_entry Photometric;
    TIFF_dir_entry FillOrder;
    TIFF_dir_entry SamplesPerPixel;
} tiff_rgb_directory;
typedef struct tiff_rgb_values_s {
    TIFF_ushort bps[3];
} tiff_rgb_values;

static const tiff_rgb_directory dir_rgb_template =
{
	/* C's ridiculous rules about & and arrays require bps[0] here: */
    {TIFFTAG_BitsPerSample, TIFF_SHORT | TIFF_INDIRECT, 3, offset_of(tiff_rgb_values, bps[0])},
    {TIFFTAG_Compression, TIFF_SHORT, 1, Compression_none},
    {TIFFTAG_Photometric, TIFF_SHORT, 1, Photometric_RGB},
    {TIFFTAG_FillOrder, TIFF_SHORT, 1, FillOrder_MSB2LSB},
    {TIFFTAG_SamplesPerPixel, TIFF_SHORT, 1, 3},
};

static const tiff_rgb_values val_12_template = {
    {4, 4, 4}
};

static const tiff_rgb_values val_24_template = {
    {8, 8, 8}
};

/* ------ Private functions ------ */

static int
tiff12_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code, i;
    bool swap_bytes = tfdev->BigEndian != arch_is_big_endian;
    tiff_rgb_values val_12_swapped = val_12_template;
    if (swap_bytes)
    {
        for (i=0; i<3; i++)
            SWAP_SHORT(val_12_swapped.bps[i]);
    }
    
    /* Write the page directory. */
    code = gdev_tiff_begin_page(pdev, &tfdev->tiff, file,
				(const TIFF_dir_entry *)&dir_rgb_template,
			        sizeof(dir_rgb_template) / sizeof(TIFF_dir_entry),
				(const byte *)&val_12_swapped,
				sizeof(val_12_swapped), 0, swap_bytes);
    if (code < 0)
	return code;

    /* Write the page data. */
    {
	int y;
	int raster = gdev_prn_raster(pdev);
	byte *line = gs_alloc_bytes(pdev->memory, raster, "tiff12_print_page");
	byte *row;

	if (line == 0)
	    return_error(gs_error_VMerror);

	for (y = 0; y < pdev->height; ++y) {
	    const byte *src;
	    byte *dest;
	    int x;

	    code = gdev_prn_get_bits(pdev, y, line, &row);
	    if (code < 0)
		break;

	    for (src = row, dest = line, x = 0; x < raster;
		 src += 6, dest += 3, x += 6
		) {
		dest[0] = (src[0] & 0xf0) | (src[1] >> 4);
		dest[1] = (src[2] & 0xf0) | (src[3] >> 4);
		dest[2] = (src[4] & 0xf0) | (src[5] >> 4);
	    }
	    fwrite(line, 1, (pdev->width * 3 + 1) >> 1, file);
	}

	gdev_tiff_end_strip(&tfdev->tiff, file);
	gdev_tiff_end_page(&tfdev->tiff, file, swap_bytes);
	gs_free_object(pdev->memory, line, "tiff12_print_page");
    }

    return code;
}

static int
tiff24_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;
    int code, i;
    bool swap_bytes = tfdev->BigEndian != arch_is_big_endian;
    tiff_rgb_values val_24_swapped = val_24_template;
    if (swap_bytes)
    {
        for (i=0; i<3; i++)
            SWAP_SHORT(val_24_swapped.bps[i]);
    }

    /* Write the page directory. */
    code = gdev_tiff_begin_page(pdev, &tfdev->tiff, file,
				(const TIFF_dir_entry *)&dir_rgb_template,
			        sizeof(dir_rgb_template) / sizeof(TIFF_dir_entry),
				(const byte *)&val_24_swapped,
				sizeof(val_24_swapped), 0, swap_bytes);
    if (code < 0)
	return code;

    /* Write the page data. */
    {
	int y;
	int raster = gdev_prn_raster(pdev);
	byte *line = gs_alloc_bytes(pdev->memory, raster, "tiff24_print_page");
	byte *row;

	if (line == 0)
	    return_error(gs_error_VMerror);
	for (y = 0; y < pdev->height; ++y) {
	    code = gdev_prn_get_bits(pdev, y, line, &row);
	    if (code < 0)
		break;
	    fwrite((char *)row, raster, 1, file);
	}
	gdev_tiff_end_strip(&tfdev->tiff, file);
	gdev_tiff_end_page(&tfdev->tiff, file, swap_bytes);
	gs_free_object(pdev->memory, line, "tiff24_print_page");
    }

    return code;
}
