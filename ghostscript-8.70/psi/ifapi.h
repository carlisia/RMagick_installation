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

/* $Id: ifapi.h 9742 2009-05-15 12:13:25Z ken $ */
/* Font API interface */

#ifndef ifapi_INCLUDED
#  define ifapi_INCLUDED

#include "iplugin.h"
#include "gstypes.h"
#include "gsmatrix.h"


typedef int FracInt; /* A fractional integer with statically unknown number of fraction bits. 
                        The number of bits depends on plugin and is being specified in
                        FAPI_server::frac_shift.
                     */
typedef int FAPI_retcode;

typedef enum {
    FAPI_FONT_FEATURE_FontMatrix,
    FAPI_FONT_FEATURE_UniqueID,
    FAPI_FONT_FEATURE_BlueScale,
    FAPI_FONT_FEATURE_Weight,
    FAPI_FONT_FEATURE_ItalicAngle,
    FAPI_FONT_FEATURE_IsFixedPitch,
    FAPI_FONT_FEATURE_UnderLinePosition,
    FAPI_FONT_FEATURE_UnderlineThickness,
    FAPI_FONT_FEATURE_FontType,
    FAPI_FONT_FEATURE_FontBBox,
    FAPI_FONT_FEATURE_BlueValues_count, 
    FAPI_FONT_FEATURE_BlueValues,
    FAPI_FONT_FEATURE_OtherBlues_count, 
    FAPI_FONT_FEATURE_OtherBlues,
    FAPI_FONT_FEATURE_FamilyBlues_count, 
    FAPI_FONT_FEATURE_FamilyBlues,
    FAPI_FONT_FEATURE_FamilyOtherBlues_count, 
    FAPI_FONT_FEATURE_FamilyOtherBlues,
    FAPI_FONT_FEATURE_BlueShift,
    FAPI_FONT_FEATURE_BlueFuzz,
    FAPI_FONT_FEATURE_StdHW,
    FAPI_FONT_FEATURE_StdVW,
    FAPI_FONT_FEATURE_StemSnapH_count, 
    FAPI_FONT_FEATURE_StemSnapH,
    FAPI_FONT_FEATURE_StemSnapV_count, 
    FAPI_FONT_FEATURE_StemSnapV,
    FAPI_FONT_FEATURE_ForceBold,
    FAPI_FONT_FEATURE_LanguageGroup,
    FAPI_FONT_FEATURE_lenIV,
    FAPI_FONT_FEATURE_GlobalSubrs_count,
    FAPI_FONT_FEATURE_Subrs_count,
    FAPI_FONT_FEATURE_Subrs_total_size,
    FAPI_FONT_FEATURE_TT_size,
    /* Multiple Master specifics */
    FAPI_FONT_FEATURE_DollarBlend,
    FAPI_FONT_FEATURE_DollarBlend_length,
    FAPI_FONT_FEATURE_BlendAxisTypes_count,
    FAPI_FONT_FEATURE_BlendAxisTypes,
    FAPI_FONT_FEATURE_BlendPrivate_count,
    FAPI_FONT_FEATURE_BlendFontInfo_count,
    FAPI_FONT_FEATURE_WeightVector_count,
    FAPI_FONT_FEATURE_WeightVector,
    FAPI_FONT_FEATURE_BlendDesignPositionsArrays_count,
    FAPI_FONT_FEATURE_BlendDesignPositionsArrayValue,
    FAPI_FONT_FEATURE_BlendDesignMapArrays_count,
    FAPI_FONT_FEATURE_BlendDesignMapSubArrays_count,
    FAPI_FONT_FEATURE_BlendDesignMapArrayValue,
    /* End MM specifics */
} fapi_font_feature;

typedef enum {
  FAPI_METRICS_NOTDEF,
  FAPI_METRICS_ADD, /* Add to native glyph width. */
  FAPI_METRICS_REPLACE_WIDTH, /* Replace the native glyph width. */
  FAPI_METRICS_REPLACE /* Replace the native glyph width and lsb. */
} FAPI_metrics_type;

typedef struct {
    int char_code;        /* Backwards compatibility. Depricated. */
    int client_char_code; /* Debug purpose. */
    int char_codes[4];
    int char_codes_count;
    bool is_glyph_index; /* true if char_code contains glyph index */
    const unsigned char *char_name; /* to be used exclusively with char_code. */
    unsigned int char_name_length;
    FAPI_metrics_type metrics_type;
    FracInt sb_x, sb_y, aw_x, aw_y; /* replaced PS metrics. */
    int metrics_scale; /* Scale for replaced PS metrics. 
		          Zero means "em box size". */
} FAPI_char_ref;

typedef struct FAPI_font_s FAPI_font;
struct FAPI_font_s {
    /* server's data : */
    void *server_font_data;
    bool need_decrypt;
    /* client's data : */
    const gs_memory_t *memory;
    const char *font_file_path;
    int subfont;
    bool is_type1; /* Only for non-disk fonts; dirty for disk fonts. */
    bool is_cid;
    bool is_outline_font;
    bool is_mtx_skipped; /* Ugly. Only UFST needs. */
    bool is_vertical;
    void *client_ctx_p;
    void *client_font_data;
    void *client_font_data2;
    const void *char_data;
    int char_data_len;
    unsigned short (*get_word )(FAPI_font *ff, fapi_font_feature var_id, int index);
    unsigned long  (*get_long )(FAPI_font *ff, fapi_font_feature var_id, int index);
    float          (*get_float)(FAPI_font *ff, fapi_font_feature var_id, int index);
    int            (*get_name) (FAPI_font *ff, fapi_font_feature var_id, int index, char *buffer, int len);
    int		   (*get_proc) (FAPI_font *ff, fapi_font_feature var_id, int index, char *Buffer);
    unsigned short (*get_gsubr)(FAPI_font *ff, int index,     byte *buf, ushort buf_length);
    unsigned short (*get_subr) (FAPI_font *ff, int index,     byte *buf, ushort buf_length);
    unsigned short (*get_glyph)(FAPI_font *ff, int char_code, byte *buf, ushort buf_length);
    unsigned short (*serialize_tt_font)(FAPI_font *ff, void *buf, int buf_size);
};

typedef struct FAPI_face_s FAPI_face;
struct FAPI_face_s {
    gs_id font_id;
    gs_matrix ctm;
    gs_log2_scale_point log2_scale;
    bool align_to_pixels; 
    float HWResolution[2];
};

typedef struct FAPI_path_s FAPI_path;
struct FAPI_path_s {
    void *olh; /* Client's data. */
    int shift;
    int (*moveto   )(FAPI_path *, FracInt, FracInt);
    int (*lineto   )(FAPI_path *, FracInt, FracInt);
    int (*curveto  )(FAPI_path *, FracInt, FracInt, FracInt, FracInt, FracInt, FracInt);
    int (*closepath)(FAPI_path *);
};

typedef struct FAPI_font_scale_s {
    FracInt matrix[6]; 
    FracInt HWResolution[2]; 
    int subpixels[2];
    bool align_to_pixels; 
} FAPI_font_scale;

typedef struct FAPI_metrics_s {
    int bbox_x0, bbox_y0, bbox_x1, bbox_y1; /* design units */
    int escapement; /* design units */
    int em_x, em_y; /* design units */
} FAPI_metrics;

typedef struct { /* 1bit/pixel only, rows are byte-aligned. */
    void *p;
    int width, height, line_step;
    int orig_x, orig_y; /* origin, 1/16s pixel */
    int left_indent, top_indent;
    int black_width, black_height;
} FAPI_raster;

#ifndef FAPI_server_DEFINED
#define FAPI_server_DEFINED
typedef struct FAPI_server_s FAPI_server;
#endif

typedef enum FAPI_descendant_code_s {  /* Possible values are descendant font indices and 4 ones defined below. */
    FAPI_DESCENDANT_PREPARED = -1, /* See FAPI_prepare_font in zfapi.c . */
    FAPI_TOPLEVEL_PREPARED = -2,
    FAPI_TOPLEVEL_BEGIN = -3,
    FAPI_TOPLEVEL_COMPLETE = -4
} FAPI_descendant_code;

struct FAPI_server_s {
    i_plugin_instance ig;
    int frac_shift; /* The number of fractional bits in coordinates. */
    FAPI_face face;
    FAPI_font ff;
    FAPI_retcode (*ensure_open)(FAPI_server *server, const byte * param, int param_size);
    FAPI_retcode (*get_scaled_font)(FAPI_server *server, FAPI_font *ff, const FAPI_font_scale *scale, const char *xlatmap, FAPI_descendant_code dc);
    FAPI_retcode (*get_decodingID)(FAPI_server *server, FAPI_font *ff, const char **decodingID);
    FAPI_retcode (*get_font_bbox)(FAPI_server *server, FAPI_font *ff, int BBox[4]);
    FAPI_retcode (*get_font_proportional_feature)(FAPI_server *server, FAPI_font *ff, bool *bProportional);
    FAPI_retcode (*can_retrieve_char_by_name)(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, int *result);
    FAPI_retcode (*can_replace_metrics)(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, int *result);
    FAPI_retcode (*get_char_width)(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, FAPI_metrics *metrics);
    FAPI_retcode (*get_char_raster_metrics)(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, FAPI_metrics *metrics);
    FAPI_retcode (*get_char_raster)(FAPI_server *server, FAPI_raster *r);
    FAPI_retcode (*get_char_outline_metrics)(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, FAPI_metrics *metrics);
    FAPI_retcode (*get_char_outline)(FAPI_server *server, FAPI_path *p);
    FAPI_retcode (*release_char_data)(FAPI_server *server);
    FAPI_retcode (*release_typeface)(FAPI_server *server, void *server_font_data);
    FAPI_retcode (*check_cmap_for_GID)(FAPI_server *server, uint *index);
    /*  Some people get confused with terms "font cache" and "character cache".
        "font cache" means a cache for scaled font objects, which mainly
        keep the font header information and rules for adjusting it to specific raster.
        "character cahce" is a cache for specific character outlines and/or character rasters.
    */
    /*  get_scaled_font opens a typeface with a server and scales it according to CTM and HWResolution.
        This creates a server's scaled font object.
        Since UFST doesn't provide a handle to this object,
        we need to build the data for it and call this function whenever scaled font data may change.
        The server must cache scaled fonts internally.
        Note that FreeType doesn't provide internal font cache,
        so the bridge must do.
    */
    /*  GS cannot provide information when a scaled font to be closed.
        Therefore we don't provide close_scaled_font function in this interface.
        The server must cache scaled fonts, and close ones which were
        not in use during a long time.
    */
    /*  Due to the interpreter fallback with CDevProc,
        get_char_raster_metrics leaves some data kept by the server,
	so taht get_char_raster uses them and release_char_data releases them.
        Therefore calls from GS to these functions must not 
        interfer with different characters.
    */
};


#endif /* ifapi_INCLUDED */
