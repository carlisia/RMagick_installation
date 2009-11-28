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

/*$Id: gxclist.c 9666 2009-04-20 19:16:24Z mvrhel $ */
/* Command list document- and page-level code. */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gsparams.h"
#include "gxdcolor.h"

extern dev_proc_open_device(pattern_clist_open_device);

/* GC information */
extern_st(st_imager_state);
static
ENUM_PTRS_WITH(device_clist_enum_ptrs, gx_device_clist *cdev)
    if (index < st_device_forward_max_ptrs) {
	gs_ptr_type_t ret = ENUM_USING_PREFIX(st_device_forward, 0);

	return (ret ? ret : ENUM_OBJ(0));
    }
    index -= st_device_forward_max_ptrs;
    if (CLIST_IS_WRITER(cdev)) {
        switch (index) {
        case 0: return ENUM_OBJ((cdev->writer.image_enum_id != gs_no_id ?
                     cdev->writer.clip_path : 0));
        case 1: return ENUM_OBJ((cdev->writer.image_enum_id != gs_no_id ?
                     cdev->writer.color_space.space : 0));
	case 2: return ENUM_OBJ(cdev->writer.pinst);
	case 3: return ENUM_OBJ(cdev->writer.cropping_stack);
        default:
        return ENUM_USING(st_imager_state, &cdev->writer.imager_state,
                  sizeof(gs_imager_state), index - 3);
        }
    }
    else {
        /* 041207
         * clist is reader.
         * We don't expect this code to be exercised at this time as the reader
         * runs under gdev_prn_output_page which is an atomic function of the
         * interpreter. We do this as this situation may change in the future.
         */
        if (index == 0)
            return ENUM_OBJ(cdev->reader.band_complexity_array);
        else if (index == 1)
	    return ENUM_OBJ(cdev->reader.offset_map);
	else
            return 0;
    }
ENUM_PTRS_END
static
RELOC_PTRS_WITH(device_clist_reloc_ptrs, gx_device_clist *cdev)
{
    RELOC_PREFIX(st_device_forward);
    if (CLIST_IS_WRITER(cdev)) {
        if (cdev->writer.image_enum_id != gs_no_id) {
	    RELOC_VAR(cdev->writer.clip_path);
	    RELOC_VAR(cdev->writer.color_space.space);
        }
	RELOC_VAR(cdev->writer.pinst);
	RELOC_VAR(cdev->writer.cropping_stack);
        RELOC_USING(st_imager_state, &cdev->writer.imager_state,
            sizeof(gs_imager_state));
    } else {
        /* 041207
         * clist is reader.
         * See note above in ENUM_PTRS_WITH section.
         */
        RELOC_VAR(cdev->reader.band_complexity_array);
        RELOC_VAR(cdev->reader.offset_map);
    }
} RELOC_PTRS_END
public_st_device_clist();
private_st_clist_writer_cropping_buffer();

/* Forward declarations of driver procedures */
dev_proc_open_device(clist_open);
static dev_proc_output_page(clist_output_page);
static dev_proc_close_device(clist_close);
static dev_proc_get_band(clist_get_band);
/* Driver procedures defined in other files are declared in gxcldev.h. */

/* Other forward declarations */
static int clist_put_current_params(gx_device_clist_writer *cldev);

/* The device procedures */
const gx_device_procs gs_clist_device_procs = {
    clist_open,
    gx_forward_get_initial_matrix,
    gx_default_sync_output,
    clist_output_page,
    clist_close,
    gx_forward_map_rgb_color,
    gx_forward_map_color_rgb,
    clist_fill_rectangle,
    gx_default_tile_rectangle,
    clist_copy_mono,
    clist_copy_color,
    gx_default_draw_line,
    gx_default_get_bits,
    gx_forward_get_params,
    gx_forward_put_params,
    gx_forward_map_cmyk_color,
    gx_forward_get_xfont_procs,
    gx_forward_get_xfont_device,
    gx_forward_map_rgb_alpha_color,
    gx_forward_get_page_device,
    gx_forward_get_alpha_bits,
    clist_copy_alpha,
    clist_get_band,
    gx_default_copy_rop,
    clist_fill_path,
    clist_stroke_path,
    clist_fill_mask,
    clist_fill_trapezoid,
    clist_fill_parallelogram,
    clist_fill_triangle,
    gx_default_draw_thin_line,
    gx_default_begin_image,
    gx_default_image_data,
    gx_default_end_image,
    clist_strip_tile_rectangle,
    clist_strip_copy_rop,
    gx_forward_get_clipping_box,
    clist_begin_typed_image,
    clist_get_bits_rectangle,
    gx_forward_map_color_rgb_alpha,
    clist_create_compositor,
    gx_forward_get_hardware_params,
    gx_default_text_begin,
    gx_default_finish_copydevice,
    NULL,			/* begin_transparency_group */
    NULL,			/* end_transparency_group */
    NULL,			/* begin_transparency_mask */
    NULL,			/* end_transparency_mask */
    NULL,			/* discard_transparency_layer */
    gx_forward_get_color_mapping_procs,
    gx_forward_get_color_comp_index,
    gx_forward_encode_color,
    gx_forward_decode_color,
    clist_pattern_manage,
    gx_default_fill_rectangle_hl_color,
    gx_default_include_color_space,
    gx_default_fill_linear_color_scanline,
    clist_fill_linear_color_trapezoid,
    clist_fill_linear_color_triangle,
    gx_forward_update_spot_equivalent_colors,
    gx_forward_ret_devn_params,
    clist_fillpage
};

/*------------------- Choose the implementation -----------------------

   For chossing the clist i/o implementation by makefile options
   we define global variables, which are initialized with
   file/memory io procs when they are included into the build.
 */
const clist_io_procs_t *clist_io_procs_file_global = NULL;
const clist_io_procs_t *clist_io_procs_memory_global = NULL;

void 
clist_init_io_procs(gx_device_clist *pclist_dev, bool in_memory)
{   
    if (in_memory || clist_io_procs_file_global == NULL)
	pclist_dev->common.page_info.io_procs = clist_io_procs_memory_global;
    else
	pclist_dev->common.page_info.io_procs = clist_io_procs_file_global;
}

/* ------ Define the command set and syntax ------ */

/* Initialization for imager state. */
/* The initial scale is arbitrary. */
const gs_imager_state clist_imager_state_initial =
{gs_imager_state_initial(300.0 / 72.0, false)};

/*
 * The buffer area (data, data_size) holds a bitmap cache when both writing
 * and reading.  The rest of the space is used for the command buffer and
 * band state bookkeeping when writing, and for the rendering buffer (image
 * device) when reading.  For the moment, we divide the space up
 * arbitrarily, except that we allocate less space for the bitmap cache if
 * the device doesn't need halftoning.
 *
 * All the routines for allocating tables in the buffer are idempotent, so
 * they can be used to check whether a given-size buffer is large enough.
 */

/*
 * Calculate the desired size for the tile cache.
 */
static uint
clist_tile_cache_size(const gx_device * target, uint data_size)
{
    uint bits_size =
    (data_size / 5) & -align_cached_bits_mod;	/* arbitrary */

    if (!gx_device_must_halftone(target)) {	/* No halftones -- cache holds only Patterns & characters. */
	bits_size -= bits_size >> 2;
    }
#define min_bits_size 1024
    if (bits_size < min_bits_size)
	bits_size = min_bits_size;
#undef min_bits_size
    return bits_size;
}

/*
 * Initialize the allocation for the tile cache.  Sets: tile_hash_mask,
 * tile_max_count, tile_table, chunk (structure), bits (structure).
 */
static int
clist_init_tile_cache(gx_device * dev, byte * init_data, ulong data_size)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    byte *data = init_data;
    uint bits_size = data_size;
    /*
     * Partition the bits area between the hash table and the actual
     * bitmaps.  The per-bitmap overhead is about 24 bytes; if the
     * average character size is 10 points, its bitmap takes about 24 +
     * 0.5 * 10/72 * xdpi * 10/72 * ydpi / 8 bytes (the 0.5 being a
     * fudge factor to account for characters being narrower than they
     * are tall), which gives us a guideline for the size of the hash
     * table.
     */
    uint avg_char_size =
	(uint)(dev->HWResolution[0] * dev->HWResolution[1] *
	       (0.5 * 10 / 72 * 10 / 72 / 8)) + 24;
    uint hc = bits_size / avg_char_size;
    uint hsize;

    while ((hc + 1) & hc)
	hc |= hc >> 1;		/* make mask (power of 2 - 1) */
    if (hc < 0xff)
	hc = 0xff;		/* make allowance for halftone tiles */
    else if (hc > 0xfff)
	hc = 0xfff;		/* cmd_op_set_tile_index has 12-bit operand */
    /* Make sure the tables will fit. */
    while (hc >= 3 && (hsize = (hc + 1) * sizeof(tile_hash)) >= bits_size)
	hc >>= 1;
    if (hc < 3)
	return_error(gs_error_rangecheck);
    cdev->tile_hash_mask = hc;
    cdev->tile_max_count = hc - (hc >> 2);
    cdev->tile_table = (tile_hash *) data;
    data += hsize;
    bits_size -= hsize;
    gx_bits_cache_chunk_init(&cdev->chunk, data, bits_size);
    gx_bits_cache_init(&cdev->bits, &cdev->chunk);
    return 0;
}

/*
 * Initialize the allocation for the bands.  Requires: target.  Sets:
 * page_band_height (=page_info.band_params.BandHeight), nbands.
 */
static int
clist_init_bands(gx_device * dev, gx_device_memory *bdev, uint data_size,
		 int band_width, int band_height)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int nbands;
    ulong space;

    if (dev->procs.open_device == pattern_clist_open_device) {
	/* We don't need bands really. */
	cdev->page_band_height = dev->height;
	cdev->nbands = 1;
	return 0;
    }
    if (gdev_mem_data_size(bdev, band_width, band_height, &space) < 0 ||
	space > data_size)
	return_error(gs_error_rangecheck);
    cdev->page_band_height = band_height;
    nbands = (cdev->target->height + band_height - 1) / band_height;
    cdev->nbands = nbands;
#ifdef DEBUG
    if (gs_debug_c('l') | gs_debug_c(':'))
	dlprintf4("[:]width=%d, band_width=%d, band_height=%d, nbands=%d\n",
		  bdev->width, band_width, band_height, nbands);
#endif
    return 0;
}

/*
 * Initialize the allocation for the band states, which are used only
 * when writing.  Requires: nbands.  Sets: states, cbuf, cend.
 */
static int
clist_init_states(gx_device * dev, byte * init_data, uint data_size)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    ulong state_size = cdev->nbands * (ulong) sizeof(gx_clist_state);
    /* Align to the natural boundary for ARM processors, bug 689600 */
    long alignment = (-(long)init_data) & (sizeof(init_data) - 1);

    /*
     * The +100 in the next line is bogus, but we don't know what the
     * real check should be. We're effectively assuring that at least 100
     * bytes will be available to buffer command operands.
     */
    if (state_size + sizeof(cmd_prefix) + cmd_largest_size + 100 + alignment > data_size)
	return_error(gs_error_rangecheck);
    /* The end buffer position is not affected by alignment */
    cdev->cend = init_data + data_size;
    init_data +=  alignment;
    cdev->states = (gx_clist_state *) init_data;
    cdev->cbuf = init_data + state_size;
    return 0;
}

/*
 * Initialize all the data allocations.  Requires: target.  Sets:
 * page_tile_cache_size, page_info.band_params.BandWidth,
 * page_info.band_params.BandBufferSpace, + see above.
 */
static int
clist_init_data(gx_device * dev, byte * init_data, uint data_size)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    gx_device *target = cdev->target;
    /* BandWidth can't be smaller than target device width */
    const int band_width =
	cdev->page_info.band_params.BandWidth = max(target->width, cdev->band_params.BandWidth);
    int band_height = cdev->band_params.BandHeight;
    bool page_uses_transparency = cdev->page_uses_transparency;
    const uint band_space =
    cdev->page_info.band_params.BandBufferSpace =
	(cdev->band_params.BandBufferSpace ?
	 cdev->band_params.BandBufferSpace : data_size);
    byte *data = init_data;
    uint size = band_space;
    uint bits_size;
    gx_device_memory bdev;
    gx_device *pbdev = (gx_device *)&bdev;
    int code;

    /* the clist writer has its own color info that depends upon the 
       transparency group color space (if transparency exists).  The data that is
       used in the clist writing. Here it is initialized with 
       the target device color info.  The values will be pushed and popped
       in a stack if we have changing color spaces in the transparency groups. */

    cdev->clist_color_info.depth = dev->color_info.depth;
    cdev->clist_color_info.polarity = dev->color_info.polarity;
    cdev->clist_color_info.num_components = dev->color_info.num_components;
    
    /* Call create_buf_device to get the memory planarity set up. */
    cdev->buf_procs.create_buf_device(&pbdev, target, 0, NULL, NULL, clist_get_band_complexity(0, 0));
    /* HACK - if the buffer device can't do copy_alpha, disallow */
    /* copy_alpha in the commmand list device as well. */
    if (dev_proc(pbdev, copy_alpha) == gx_no_copy_alpha)
	cdev->disable_mask |= clist_disable_copy_alpha;
    if (cdev->procs.open_device == pattern_clist_open_device) {
	bits_size = data_size / 2;
    } else if (band_height) {
	/*
	 * The band height is fixed, so the band buffer requirement
	 * is completely determined.
	 */
	ulong band_data_size;

	if (gdev_mem_data_size(&bdev, band_width, band_height, &band_data_size) < 0 ||
	    band_data_size >= band_space)
	    return_error(gs_error_rangecheck);
	bits_size = min(band_space - band_data_size, data_size >> 1);
    } else {
	/*
	 * Choose the largest band height that will fit in the
	 * rendering-time buffer.
	 */
	bits_size = clist_tile_cache_size(target, band_space);
	bits_size = min(bits_size, data_size >> 1);
	band_height = gdev_mem_max_height(&bdev, band_width,
			  band_space - bits_size, page_uses_transparency);
	if (band_height == 0)
	    return_error(gs_error_rangecheck);
    }
    cdev->ins_count = 0;
    code = clist_init_tile_cache(dev, data, bits_size);
    if (code < 0)
	return code;
    cdev->page_tile_cache_size = bits_size;
    data += bits_size;
    size -= bits_size;
    code = clist_init_bands(dev, &bdev, size, band_width, band_height);
    if (code < 0)
	return code;
    return clist_init_states(dev, data, data_size - bits_size);
}
/*
 * Reset the device state (for writing).  This routine requires only
 * data, data_size, and target to be set, and is idempotent.
 */
static int
clist_reset(gx_device * dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code = clist_init_data(dev, cdev->data, cdev->data_size);
    int nbands;

    if (code < 0)
	return (cdev->permanent_error = code);
    /* Now initialize the rest of the state. */
    cdev->permanent_error = 0;
    nbands = cdev->nbands;
    cdev->ymin = cdev->ymax = -1;	/* render_init not done yet */
    memset(cdev->tile_table, 0, (cdev->tile_hash_mask + 1) *
       sizeof(*cdev->tile_table));
    cdev->cnext = cdev->cbuf;
    cdev->ccl = 0;
    cdev->band_range_list.head = cdev->band_range_list.tail = 0;
    cdev->band_range_min = 0;
    cdev->band_range_max = nbands - 1;
    {
	int band;
	gx_clist_state *states = cdev->states;

	for (band = 0; band < nbands; band++, states++) {
	    static const gx_clist_state cls_initial =
	    {cls_initial_values};

	    *states = cls_initial;
	}
    }
    /*
     * Round up the size of the per-tile band mask so that the bits,
     * which follow it, stay aligned.
     */
    cdev->tile_band_mask_size =
	((nbands + (align_bitmap_mod * 8 - 1)) >> 3) &
	~(align_bitmap_mod - 1);
    /*
     * Initialize the all-band parameters to impossible values,
     * to force them to be written the first time they are used.
     */
    memset(&cdev->tile_params, 0, sizeof(cdev->tile_params));
    cdev->tile_depth = 0;
    cdev->tile_known_min = nbands;
    cdev->tile_known_max = -1;
    cdev->imager_state = clist_imager_state_initial;
    cdev->clip_path = NULL;
    cdev->clip_path_id = gs_no_id;
    cdev->color_space.byte1 = 0;
    cdev->color_space.id = gs_no_id;
    cdev->color_space.space = 0;
    {
	int i;

	for (i = 0; i < countof(cdev->transfer_ids); ++i)
	    cdev->transfer_ids[i] = gs_no_id;
    }
    cdev->black_generation_id = gs_no_id;
    cdev->undercolor_removal_id = gs_no_id;
    cdev->device_halftone_id = gs_no_id;
    cdev->image_enum_id = gs_no_id;
    cdev->cropping_min = cdev->save_cropping_min = 0;
    cdev->cropping_max = cdev->save_cropping_max = cdev->height;
    cdev->cropping_saved = false;
    cdev->cropping_stack = NULL;
    cdev->cropping_level = 0;
    cdev->mask_id_count = cdev->mask_id = cdev->temp_mask_id = 0;
    return 0;
}
/*
 * Initialize the device state (for writing).  This routine requires only
 * data, data_size, and target to be set, and is idempotent.
 */
static int
clist_init(gx_device * dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code = clist_reset(dev);

    if (code >= 0) {
	cdev->image_enum_id = gs_no_id;
	cdev->error_is_retryable = 0;
	cdev->driver_call_nesting = 0;
	cdev->ignore_lo_mem_warnings = 0;
    }
    return code;
}

/* (Re)init open band files for output (set block size, etc). */
static int	/* ret 0 ok, -ve error code */
clist_reinit_output_file(gx_device *dev)
{    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code = 0;

    /* bfile needs to guarantee cmd_blocks for: 1 band range, nbands */
    /*  & terminating entry */
    int b_block = sizeof(cmd_block) * (cdev->nbands + 2);

    /* cfile needs to guarantee one writer buffer */
    /*  + one end_clip cmd (if during image's clip path setup) */
    /*  + an end_image cmd for each band (if during image) */
    /*  + end_cmds for each band and one band range */
    int c_block =
	cdev->cend - cdev->cbuf + 2 + cdev->nbands * 2 + (cdev->nbands + 1);

    /* All this is for partial page rendering's benefit, do only */
    /* if partial page rendering is available */
    if ( clist_test_VMerror_recoverable(cdev) )
	{ if (cdev->page_bfile != 0)
	    code = cdev->page_info.io_procs->set_memory_warning(cdev->page_bfile, b_block);
	if (code >= 0 && cdev->page_cfile != 0)
	    code = cdev->page_info.io_procs->set_memory_warning(cdev->page_cfile, c_block);
	}
    return code;
}

/* Write out the current parameters that must be at the head of each page */
/* if async rendering is in effect */
static int
clist_emit_page_header(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int code = 0;

    if ((cdev->disable_mask & clist_disable_pass_thru_params)) {
	do
	    if ((code = clist_put_current_params(cdev)) >= 0)
	        break;
	while ((code = clist_VMerror_recover(cdev, code)) >= 0);
	cdev->permanent_error = (code < 0 ? code : 0);
	if (cdev->permanent_error < 0)
	    cdev->error_is_retryable = 0;
    }
    return code;
}

/* Reset parameters for the beginning of a page. */
static void
clist_reset_page(gx_device_clist_writer *cwdev)
{
    cwdev->page_bfile_end_pos = 0;
    /* Indicate that the colors_used information hasn't been computed. */
    cwdev->page_info.scan_lines_per_colors_used = 0;
    memset(cwdev->page_info.band_colors_used, 0,
	   sizeof(cwdev->page_info.band_colors_used));
}

/* Open the device's bandfiles */
static int
clist_open_output_file(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    char fmode[4];
    int code;

    if (cdev->do_not_open_or_close_bandfiles)
	return 0; /* external bandfile open/close managed externally */
    cdev->page_cfile = 0;	/* in case of failure */
    cdev->page_bfile = 0;	/* ditto */
    code = clist_init(dev);
    if (code < 0)
	return code;
    strcpy(fmode, "w+");
    strcat(fmode, gp_fmode_binary_suffix);
    cdev->page_cfname[0] = 0;	/* create a new file */
    cdev->page_bfname[0] = 0;	/* ditto */
    clist_reset_page(cdev);
    if ((code = cdev->page_info.io_procs->fopen(cdev->page_cfname, fmode, &cdev->page_cfile,
			    cdev->bandlist_memory, cdev->bandlist_memory,
			    true)) < 0 ||
	(code = cdev->page_info.io_procs->fopen(cdev->page_bfname, fmode, &cdev->page_bfile,
			    cdev->bandlist_memory, cdev->bandlist_memory,
			    false)) < 0 ||
	(code = clist_reinit_output_file(dev)) < 0
	) {
	clist_close_output_file(dev);
	cdev->permanent_error = code;
	cdev->error_is_retryable = 0;
    }
    return code;
}

/* Close, and free the contents of, the temporary files of a page. */
/* Note that this does not deallocate the buffer. */
int
clist_close_page_info(gx_band_page_info_t *ppi)
{
    if (ppi->cfile != NULL) {
	ppi->io_procs->fclose(ppi->cfile, ppi->cfname, true);
	ppi->cfile = NULL;
    }
    if (ppi->bfile != NULL) {
	ppi->io_procs->fclose(ppi->bfile, ppi->bfname, true);
	ppi->bfile = NULL;
    }
    return 0;
}

/* Close the device by freeing the temporary files. */
/* Note that this does not deallocate the buffer. */
int
clist_close_output_file(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;

    return clist_close_page_info(&cdev->page_info);
}

/* Open the device by initializing the device state and opening the */
/* scratch files. */
int
clist_open(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    bool save_is_open = dev->is_open;
    int code;

    cdev->permanent_error = 0;
    cdev->is_open = false;
    code = clist_init(dev);
    if (code < 0)
	return code;
    code = clist_open_output_file(dev);
    if ( code >= 0)
	code = clist_emit_page_header(dev);
    if (code >= 0)
       dev->is_open = save_is_open;
     return code;
}

static int
clist_close(gx_device *dev)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;

    if (cdev->do_not_open_or_close_bandfiles)
	return 0;	
    if (cdev->procs.open_device == pattern_clist_open_device) {
	gs_free_object(cdev->bandlist_memory, cdev->data, "clist_close");
	cdev->data = NULL;
    }
    return clist_close_output_file(dev);
}

/* The output_page procedure should never be called! */
static int
clist_output_page(gx_device * dev, int num_copies, int flush)
{
    return_error(gs_error_Fatal);
}

/* Reset (or prepare to append to) the command list after printing a page. */
int
clist_finish_page(gx_device *dev, bool flush)
{
    gx_device_clist_writer * const cdev =	&((gx_device_clist *)dev)->writer;
    int code;

    /* If this is a reader clist, which is about to be reset to a writer,
     * free any band_complexity_array memory used by same.
     * since we have been rendering, shut down threads
     */
    if (!CLIST_IS_WRITER((gx_device_clist *)dev)) {
	gx_clist_reader_free_band_complexity_array( (gx_device_clist *)dev );
	clist_teardown_render_threads(dev);
    }

    if (flush) {
	if (cdev->page_cfile != 0)
	    cdev->page_info.io_procs->rewind(cdev->page_cfile, true, cdev->page_cfname);
	if (cdev->page_bfile != 0)
	    cdev->page_info.io_procs->rewind(cdev->page_bfile, true, cdev->page_bfname);
	clist_reset_page(cdev);
    } else {
	if (cdev->page_cfile != 0)
	    cdev->page_info.io_procs->fseek(cdev->page_cfile, 0L, SEEK_END, cdev->page_cfname);
	if (cdev->page_bfile != 0)
	    cdev->page_info.io_procs->fseek(cdev->page_bfile, 0L, SEEK_END, cdev->page_bfname);
    }
    code = clist_init(dev);		/* reinitialize */
    if (code >= 0)
	code = clist_reinit_output_file(dev);
    if (code >= 0)
	code = clist_emit_page_header(dev);

    return code;
}

/* ------ Writing ------ */

/* End a page by flushing the buffer and terminating the command list. */
int	/* ret 0 all-ok, -ve error code, or +1 ok w/low-mem warning */
clist_end_page(gx_device_clist_writer * cldev)
{
    int code = cmd_write_buffer(cldev, cmd_opv_end_page);
    cmd_block cb;
    int ecode = 0;

    if (code >= 0) {
	/*
	 * Write the terminating entry in the block file.
	 * Note that because of copypage, there may be many such entries.
	 */
	cb.band_min = cb.band_max = cmd_band_end;
	cb.pos = (cldev->page_cfile == 0 ? 0 : cldev->page_info.io_procs->ftell(cldev->page_cfile));
	code = cldev->page_info.io_procs->fwrite_chars(&cb, sizeof(cb), cldev->page_bfile);
	if (code > 0)
	    code = 0;
    }
    if (code >= 0) {
	clist_compute_colors_used(cldev);
	ecode |= code;
	cldev->page_bfile_end_pos = cldev->page_info.io_procs->ftell(cldev->page_bfile);
    }
    if (code < 0)
	ecode = code;

    /* Reset warning margin to 0 to release reserve memory if mem files */
    if (cldev->page_bfile != 0)
	cldev->page_info.io_procs->set_memory_warning(cldev->page_bfile, 0);
    if (cldev->page_cfile != 0)
	cldev->page_info.io_procs->set_memory_warning(cldev->page_cfile, 0);

#ifdef DEBUG
    if (gs_debug_c('l') | gs_debug_c(':'))
	dlprintf2("[:]clist_end_page at cfile=%ld, bfile=%ld\n",
		  (long)cb.pos, (long)cldev->page_bfile_end_pos);
#endif
    return 0;
}

/* Compute the set of used colors in the page_info structure. 
 *
 * NB: Area for improvement, move states[band] and page_info to clist
 * rather than writer device, or remove completely as this is used by the old planar devices 
 * to operate on a plane at a time.  
 */

void
clist_compute_colors_used(gx_device_clist_writer *cldev)
{
    int nbands = cldev->nbands;
    int bands_per_colors_used =
	(nbands + PAGE_INFO_NUM_COLORS_USED - 1) /
	PAGE_INFO_NUM_COLORS_USED;
    int band;

    cldev->page_info.scan_lines_per_colors_used =
	cldev->page_band_height * bands_per_colors_used;
    memset(cldev->page_info.band_colors_used, 0,
	   sizeof(cldev->page_info.band_colors_used));
    for (band = 0; band < nbands; ++band) {
	int entry = band / bands_per_colors_used;

	cldev->page_info.band_colors_used[entry].or |=
	    cldev->states[band].colors_used.or;
	cldev->page_info.band_colors_used[entry].slow_rop |=
	    cldev->states[band].colors_used.slow_rop;

    }
}

/* Recover recoverable VM error if possible without flushing */
int	/* ret -ve err, >= 0 if recovered w/# = cnt pages left in page queue */
clist_VMerror_recover(gx_device_clist_writer *cldev,
		      int old_error_code)
{
    int code = old_error_code;
    int pages_remain;

    if (!clist_test_VMerror_recoverable(cldev) ||
	!cldev->error_is_retryable ||
	old_error_code != gs_error_VMerror
	)
	return old_error_code;

    /* Do some rendering, return if enough memory is now free */
    do {
	pages_remain =
	    (*cldev->free_up_bandlist_memory)( (gx_device *)cldev, false );
	if (pages_remain < 0) {
	    code = pages_remain;	/* abort, error or interrupt req */
	    break;
	}
	if (clist_reinit_output_file( (gx_device *)cldev ) == 0) {
	    code = pages_remain;	/* got enough memory to continue */
	    break;
	}
    } while (pages_remain);

    if_debug1('L', "[L]soft flush of command list, status: %d\n", code);
    return code;
}

/* If recoverable VM error, flush & try to recover it */
int	/* ret 0 ok, else -ve error */
clist_VMerror_recover_flush(gx_device_clist_writer *cldev,
			    int old_error_code)
{
    int free_code = 0;
    int reset_code = 0;
    int code;

    /* If the device has the ability to render partial pages, flush
     * out the bandlist, and reset the writing state. Then, get the
     * device to render this band. When done, see if there's now enough
     * memory to satisfy the minimum low-memory guarantees. If not, 
     * get the device to render some more. If there's nothing left to
     * render & still insufficient memory, declare an error condition.
     */
    if (!clist_test_VMerror_recoverable(cldev) ||
	old_error_code != gs_error_VMerror
	)
	return old_error_code;	/* sorry, don't have any means to recover this error */
    free_code = (*cldev->free_up_bandlist_memory)( (gx_device *)cldev, true );

    /* Reset the state of bands to "don't know anything" */
    reset_code = clist_reset( (gx_device *)cldev );
    if (reset_code >= 0)
	reset_code = clist_open_output_file( (gx_device *)cldev );
    if ( reset_code >= 0 &&
	 (cldev->disable_mask & clist_disable_pass_thru_params)
	 )
	reset_code = clist_put_current_params(cldev);
    if (reset_code < 0) {
	cldev->permanent_error = reset_code;
	cldev->error_is_retryable = 0;
    }
 
    code = (reset_code < 0 ? reset_code : free_code < 0 ? old_error_code : 0);
    if_debug1('L', "[L]hard flush of command list, status: %d\n", code);
    return code;
}

/* Write the target device's current parameter list */
static int	/* ret 0 all ok, -ve error */
clist_put_current_params(gx_device_clist_writer *cldev)
{
    gx_device *target = cldev->target;
    gs_c_param_list param_list;
    int code;

    /*
     * If a put_params call fails, the device will be left in a closed
     * state, but higher-level code won't notice this fact.  We flag this by
     * setting permanent_error, which prevents writing to the command list.
     */

    if (cldev->permanent_error)
	return cldev->permanent_error;
    gs_c_param_list_write(&param_list, cldev->memory);
    code = (*dev_proc(target, get_params))
	(target, (gs_param_list *)&param_list);
    if (code >= 0) {
	gs_c_param_list_read(&param_list);
	code = cmd_put_params( cldev, (gs_param_list *)&param_list );
    }
    gs_c_param_list_release(&param_list);

    return code;
}

/* ---------------- Driver interface ---------------- */

static int
clist_get_band(gx_device * dev, int y, int *band_start)
{
    gx_device_clist_writer * const cdev =
	&((gx_device_clist *)dev)->writer;
    int band_height = cdev->page_band_height;
    int start;

    if (y < 0)
	y = 0;
    else if (y >= dev->height)
	y = dev->height;
    *band_start = start = y - y % band_height;
    return min(dev->height - start, band_height);
}

/* copy constructor if from != NULL
 * default constructor if from == NULL
 */
void 
clist_copy_band_complexity(gx_band_complexity_t *this, const gx_band_complexity_t *from)
{
    if (from) {
	memcpy(this, from, sizeof(gx_band_complexity_t));
    } else {
	/* default */
	this->uses_color = false;
	this->nontrivial_rops = false;
#if 0
	/* todo: halftone phase */

	this->x0 = 0;
	this->y0 = 0;
#endif
    }
}

int 
clist_writer_push_no_cropping(gx_device_clist_writer *cdev)
{
    clist_writer_cropping_buffer_t *buf = gs_alloc_struct(cdev->memory, 
		clist_writer_cropping_buffer_t,
		&st_clist_writer_cropping_buffer, "clist_writer_transparency_push");

    if (buf == NULL)
	return_error(gs_error_VMerror);
    if_debug1('v', "[v]push cropping[%d]\n", cdev->cropping_level);
    buf->next = cdev->cropping_stack;
    cdev->cropping_stack = buf;
    buf->cropping_min = cdev->cropping_min;
    buf->cropping_max = cdev->cropping_max;
    buf->mask_id = cdev->mask_id;
    buf->temp_mask_id = cdev->temp_mask_id;
    cdev->cropping_level++;
    return 0;
}

int 
clist_writer_push_cropping(gx_device_clist_writer *cdev, int ry, int rheight)
{
    int code = clist_writer_push_no_cropping(cdev);
    
    if (code < 0)
	return 0;
    cdev->cropping_min = max(cdev->cropping_min, ry);
    cdev->cropping_max = min(cdev->cropping_max, ry + rheight);
    return 0;
}

int 
clist_writer_pop_cropping(gx_device_clist_writer *cdev)
{
    clist_writer_cropping_buffer_t *buf = cdev->cropping_stack;

    if (buf == NULL)
	return_error(gs_error_unregistered); /*Must not happen. */
    cdev->cropping_min = buf->cropping_min;
    cdev->cropping_max = buf->cropping_max;
    cdev->mask_id = buf->mask_id;
    cdev->temp_mask_id = buf->temp_mask_id;
    cdev->cropping_stack = buf->next;
    cdev->cropping_level--;
    if_debug1('v', "[v]pop cropping[%d]\n", cdev->cropping_level);
    gs_free_object(cdev->memory, buf, "clist_writer_transparency_pop");
    return 0;
}

int 
clist_writer_check_empty_cropping_stack(gx_device_clist_writer *cdev)
{
    if (cdev->cropping_stack != NULL) {
	if_debug1('v', "[v]Error: left %d cropping(s)\n", cdev->cropping_level);
	return_error(gs_error_unregistered); /* Must not happen */
    }
    return 0;
}

/* Retrieve total size for cfile and bfile. */
int clist_data_size(const gx_device_clist *cdev, int select)
{
    const gx_band_page_info_t *pinfo = &cdev->common.page_info;
    clist_file_ptr pfile = (!select ? pinfo->bfile : pinfo->cfile);
    const char *fname = (!select ? pinfo->bfname : pinfo->cfname);
    int code, size;

    code = pinfo->io_procs->fseek(pfile, 0, SEEK_END, fname);
    if (code < 0)
	return_error(gs_error_unregistered); /* Must not happen. */
    code = pinfo->io_procs->ftell(pfile);
    if (code < 0)
	return_error(gs_error_unregistered); /* Must not happen. */
    size = code;
    return size;
}

/* Get command list data. */
int
clist_get_data(const gx_device_clist *cdev, int select, int offset, byte *buf, int length)
{
    const gx_band_page_info_t *pinfo = &cdev->common.page_info;
    clist_file_ptr pfile = (!select ? pinfo->bfile : pinfo->cfile);
    const char *fname = (!select ? pinfo->bfname : pinfo->cfname);
    int code;

    code = pinfo->io_procs->fseek(pfile, offset, SEEK_SET, fname);
    if (code < 0)
	return_error(gs_error_unregistered); /* Must not happen. */
    /* This assumes that fread_chars doesn't return prematurely
       when the buffer is not fully filled and the end of stream is not reached. */
    return pinfo->io_procs->fread_chars(buf, length, pfile);
}

/* Put command list data. */
int
clist_put_data(const gx_device_clist *cdev, int select, int offset, const byte *buf, int length)
{
    const gx_band_page_info_t *pinfo = &cdev->common.page_info;
    clist_file_ptr pfile = (!select ? pinfo->bfile : pinfo->cfile);
    int code;

    code = pinfo->io_procs->ftell(pfile);
    if (code < 0)
	return_error(gs_error_unregistered); /* Must not happen. */
    if (code != offset) {
	/* Assuming a consecutive writing only. */
	return_error(gs_error_unregistered); /* Must not happen. */
    }
    /* This assumes that fwrite_chars doesn't return prematurely
       when the buffer is not fully written, except with an error. */
    return pinfo->io_procs->fwrite_chars(buf, length, pfile);
}

