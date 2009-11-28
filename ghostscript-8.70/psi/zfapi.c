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

/* $Id: zfapi.c 9760 2009-05-24 10:16:57Z giles $ */
/* Font API client */

#include "memory_.h"
#include "math_.h"
#include "ghost.h"
#include "gp.h"
#include "oper.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxchar.h"
#include "gxpath.h"
#include "gxfcache.h"
#include "gxchrout.h"
#include "gximask.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gsfont.h"
#include "gspath.h"
#include "bfont.h"
#include "dstack.h"
#include "ichar.h"
#include "idict.h"
#include "iname.h"
#include "ifont.h"
#include "icid.h"
#include "igstate.h"
#include "icharout.h"
#include "ifapi.h"
#include "iplugin.h"
#include "store.h"
#include "gzstate.h"
#include "gdevpsf.h"
#include "stream.h"		/* for files.h */
#include "gscrypt1.h"
#include "gxfcid.h"
#include "gsstype.h"
#include "gxchar.h"		/* for st_gs_show_enum */
#include "ipacked.h"	    /* for packed_next */
#include "iddict.h"

/* -------------------------------------------------------- */

typedef struct FAPI_outline_handler_s {
    struct gx_path_s *path;
    fixed x0, y0;
    bool close_path, need_close; /* This stuff fixes unclosed paths being rendered with UFST */
} FAPI_outline_handler;

static inline int import_shift(int x, int n)
{   return n > 0 ? x << n : x >> -n;
}

static inline int export_shift(int x, int n)
{   return n > 0 ? x >> n : x << -n;
}

static inline int fapi_round(double x)
{   return (int)(x + 0.5);
}

static int add_closepath(FAPI_path *I)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;

    olh->need_close = false;
    return gx_path_close_subpath_notes(olh->path, 0);
}

static int add_move(FAPI_path *I, FracInt x, FracInt y)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;
    int code;

    if (olh->need_close && olh->close_path)
        if ((code = add_closepath(I)) < 0)
	    return code;
    olh->need_close = false;
    return gx_path_add_point(olh->path, import_shift(x, I->shift) + olh->x0, -import_shift(y, I->shift) + olh->y0);
}

static int add_line(FAPI_path *I, FracInt x, FracInt y)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;

    olh->need_close = true;
    return gx_path_add_line_notes(olh->path, import_shift(x, I->shift) + olh->x0, -import_shift(y, I->shift) + olh->y0, 0);
}

static int add_curve(FAPI_path *I, FracInt x0, FracInt y0, FracInt x1, FracInt y1, FracInt x2, FracInt y2)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;

    olh->need_close = true;
    return gx_path_add_curve_notes(olh->path, import_shift(x0, I->shift) + olh->x0, -import_shift(y0, I->shift) + olh->y0, 
					      import_shift(x1, I->shift) + olh->x0, -import_shift(y1, I->shift) + olh->y0, 
					      import_shift(x2, I->shift) + olh->x0, -import_shift(y2, I->shift) + olh->y0, 0);
}

static FAPI_path path_interface_stub = { NULL, 0, add_move, add_line, add_curve, add_closepath };

static inline bool IsCIDFont(const gs_font_base *pbfont)
{   return (pbfont->FontType == ft_CID_encrypted ||
            pbfont->FontType == ft_CID_user_defined ||
            pbfont->FontType == ft_CID_TrueType);
    /* The font type 10 (ft_CID_user_defined) must not pass to FAPI. */
}

static inline bool IsType1GlyphData(const gs_font_base *pbfont)
{   return pbfont->FontType == ft_encrypted ||
           pbfont->FontType == ft_encrypted2 ||
           pbfont->FontType == ft_CID_encrypted;
}

/* -------------------------------------------------------- */

typedef struct sfnts_reader_s sfnts_reader;
struct sfnts_reader_s { 
    ref *sfnts;
    const gs_memory_t *memory;
    const byte *p;
    long index;
    uint offset;
    uint length;
    bool error;
    byte (*rbyte)(sfnts_reader *r);
    ushort (*rword)(sfnts_reader *r);
    ulong (*rlong)(sfnts_reader *r);
    void (*rstring)(sfnts_reader *r, byte *v, int length);
    void (*seek)(sfnts_reader *r, ulong pos);
};

static void sfnts_next_elem(sfnts_reader *r)
{   ref s;

    if (r->error)
        return;
    r->index++;
    r->error |= (array_get(r->memory, r->sfnts, r->index, &s) < 0);
    if (r->error)
        return;
    r->p = s.value.const_bytes;
    r->length = r_size(&s) & ~(uint)1; /* See Adobe Technical Note # 5012, section 4.2. */
    r->offset = 0;
}

static inline byte sfnts_reader_rbyte_inline(sfnts_reader *r)
{   if (r->offset >= r->length)
        sfnts_next_elem(r);
    return (r->error ? 0 : r->p[r->offset++]);
}

static byte sfnts_reader_rbyte(sfnts_reader *r) /* old compiler compatibility */
{   return sfnts_reader_rbyte_inline(r);
}

static ushort sfnts_reader_rword(sfnts_reader *r)
{   return (sfnts_reader_rbyte_inline(r) << 8) + sfnts_reader_rbyte_inline(r);
}

static ulong sfnts_reader_rlong(sfnts_reader *r)
{   return (sfnts_reader_rbyte_inline(r) << 24) + (sfnts_reader_rbyte_inline(r) << 16) + 
           (sfnts_reader_rbyte_inline(r) << 8) + sfnts_reader_rbyte_inline(r);
}

static void sfnts_reader_rstring(sfnts_reader *r, byte *v, int length)
{   if (length < 0)
        return;
    while (!r->error) {
        int l = min(length, r->length - r->offset);
        memcpy(v, r->p + r->offset, l);
        r->p += l;
        length -= l;
        if (length <= 0)
            return;
        sfnts_next_elem(r);
    }
}

static void sfnts_reader_seek(sfnts_reader *r, ulong pos)
{   /* fixme : optimize */
    ulong skipped = 0;

    r->index = -1;
    sfnts_next_elem(r);
    while (skipped + r->length < pos && !r->error) {
        skipped += r->length;
        sfnts_next_elem(r);
    }
    r->offset = pos - skipped;
}

static void sfnts_reader_init(sfnts_reader *r, ref *pdr)
{   r->rbyte = sfnts_reader_rbyte;
    r->rword = sfnts_reader_rword;
    r->rlong = sfnts_reader_rlong;
    r->rstring = sfnts_reader_rstring;
    r->seek = sfnts_reader_seek;
    r->index = -1;
    r->error = false;
    if (r_type(pdr) != t_dictionary ||
        dict_find_string(pdr, "sfnts", &r->sfnts) <= 0)
        r->error = true;
    sfnts_next_elem(r);
}

/* -------------------------------------------------------- */

typedef struct sfnts_writer_s sfnts_writer;
struct sfnts_writer_s {
    byte *buf, *p;
    int buf_size;
    void (*wbyte)(sfnts_writer *w, byte v);
    void (*wword)(sfnts_writer *w, ushort v);
    void (*wlong)(sfnts_writer *w, ulong v);
    void (*wstring)(sfnts_writer *w, byte *v, int length);
};

static void sfnts_writer_wbyte(sfnts_writer *w, byte v)
{   if (w->buf + w->buf_size < w->p + 1)
        return; /* safety */
    w->p[0] = v;
    w->p++;
}

static void sfnts_writer_wword(sfnts_writer *w, ushort v)
{   if (w->buf + w->buf_size < w->p + 2)
        return; /* safety */
    w->p[0] = v / 256;
    w->p[1] = v % 256;
    w->p += 2;
}

static void sfnts_writer_wlong(sfnts_writer *w, ulong v)
{   if (w->buf + w->buf_size < w->p + 4)
        return; /* safety */
    w->p[0] = v >> 24;
    w->p[1] = (v >> 16) & 0xFF;
    w->p[2] = (v >>  8) & 0xFF;
    w->p[3] = v & 0xFF;
    w->p += 4;
}

static void sfnts_writer_wstring(sfnts_writer *w, byte *v, int length)
{   if (w->buf + w->buf_size < w->p + length)
        return; /* safety */
    memcpy(w->p, v, length);
    w->p += length;
}

static const sfnts_writer sfnts_writer_stub = {
    0, 0, 0,
    sfnts_writer_wbyte,
    sfnts_writer_wword,
    sfnts_writer_wlong,
    sfnts_writer_wstring
};

/* -------------------------------------------------------- */

static inline bool sfnts_need_copy_table(byte *tag)
{ return memcmp(tag, "glyf", 4) &&
         memcmp(tag, "glyx", 4) && /* Presents in files created by AdobePS5.dll Version 5.1.2 */
         memcmp(tag, "loca", 4) &&
         memcmp(tag, "locx", 4) && /* Presents in files created by AdobePS5.dll Version 5.1.2 */
         memcmp(tag, "cmap", 4);
}

static void sfnt_copy_table(sfnts_reader *r, sfnts_writer *w, int length)
{   byte buf[1024];

    while (length > 0 && !r->error) {
        int l = min(length, sizeof(buf));
        r->rstring(r, buf, l);
        w->wstring(w, buf, l);
        length -= l;
    }
}

static ulong sfnts_copy_except_glyf(sfnts_reader *r, sfnts_writer *w)
{   /* Note : TTC is not supported and probably is unuseful for Type 42. */
    /* This skips glyf, loca and cmap from copying. */
    struct {
        byte tag[4];
        ulong checkSum, offset, offset_new, length;
    } tables[30];
    const ushort alignment = 4; /* Not sure, maybe 2 */
    ulong version = r->rlong(r);
    ushort num_tables = r->rword(r);
    ushort i, num_tables_new = 0;
    ushort searchRange, entrySelector = 0, rangeShift, v;
    ulong size_new = 12;

    r->rword(r); /* searchRange */
    r->rword(r); /* entrySelector */
    r->rword(r); /* rangeShift */
    for (i = 0; i < num_tables; i++) {
        if (r->error)
            return 0;
        r->rstring(r, tables[i].tag, 4);
        tables[i].checkSum = r->rlong(r);
        tables[i].offset = r->rlong(r);
        tables[i].length = r->rlong(r);
        tables[i].offset_new = size_new;
        if (sfnts_need_copy_table(tables[i].tag)) {
            num_tables_new ++;
            size_new += (tables[i].length + alignment - 1) / alignment * alignment;
        }
    }
    size_new += num_tables_new * 16;
    if (w == 0)
        return size_new;

    searchRange = v = num_tables_new * 16;
    for (i = 0; v; i++) {
        v >>= 1;
        searchRange |= v;
        entrySelector++;
    }
    searchRange -= searchRange >> 1;
    rangeShift = num_tables_new * 16 - searchRange;

    w->wlong(w, version);
    w->wword(w, num_tables_new);
    w->wword(w, searchRange);
    w->wword(w, entrySelector);
    w->wword(w, rangeShift);
    for (i = 0; i < num_tables; i++)
        if (sfnts_need_copy_table(tables[i].tag)) {
            w->wstring(w, tables[i].tag, 4);
            w->wlong(w, tables[i].checkSum);
            w->wlong(w, tables[i].offset_new + num_tables_new * 16);
            w->wlong(w, tables[i].length);
        }
    for (i = 0; i < num_tables; i++)
        if (sfnts_need_copy_table(tables[i].tag)) {
            int k = tables[i].length;
            r->seek(r, tables[i].offset);
            if (r->error)
                return 0;
            if (w->p - w->buf != tables[i].offset_new + num_tables_new * 16)
                return 0; /* the algorithm consistency check */
            sfnt_copy_table(r, w, tables[i].length);
            for (; k & (alignment - 1); k++)
                w->wbyte(w, 0);
        }
    return size_new;
}

static ulong true_type_size(ref *pdr)
{   sfnts_reader r;

    sfnts_reader_init(&r, pdr);
    return sfnts_copy_except_glyf(&r, 0);
}

static ushort FAPI_FF_serialize_tt_font(FAPI_font *ff, void *buf, int buf_size)
{   ref *pdr = (ref *)ff->client_font_data2;
    sfnts_reader r;
    sfnts_writer w = sfnts_writer_stub;

    w.buf_size = buf_size;
    w.buf = w.p = buf;
    sfnts_reader_init(&r, pdr);
    if(!sfnts_copy_except_glyf(&r, &w))
        return 1;
    return r.error;
}

static inline ushort float_to_ushort(float v)
{   return (ushort)(v * 16); /* fixme : the scale may depend on renderer */
}

static ushort FAPI_FF_get_word(FAPI_font *ff, fapi_font_feature var_id, int index)
{   gs_font_type1 *pfont = (gs_font_type1 *)ff->client_font_data;
    ref *pdr = (ref *)ff->client_font_data2;

    switch((int)var_id) {
        case FAPI_FONT_FEATURE_Weight: return 0; /* wrong */
        case FAPI_FONT_FEATURE_ItalicAngle: return 0; /* wrong */
        case FAPI_FONT_FEATURE_IsFixedPitch: return 0; /* wrong */
        case FAPI_FONT_FEATURE_UnderLinePosition: return 0; /* wrong */
        case FAPI_FONT_FEATURE_UnderlineThickness: return 0; /* wrong */
        case FAPI_FONT_FEATURE_FontType: return (pfont->FontType == 2 ? 2 : 1);
        case FAPI_FONT_FEATURE_FontBBox: 
            switch (index) {
                case 0 : return (ushort)pfont->FontBBox.p.x;
                case 1 : return (ushort)pfont->FontBBox.p.y;
                case 2 : return (ushort)pfont->FontBBox.q.x;
                case 3 : return (ushort)pfont->FontBBox.q.y;
            }
            return 0;
        case FAPI_FONT_FEATURE_BlueValues_count: return pfont->data.BlueValues.count;
        case FAPI_FONT_FEATURE_BlueValues: return float_to_ushort(pfont->data.BlueValues.values[index]);
        case FAPI_FONT_FEATURE_OtherBlues_count: return pfont->data.OtherBlues.count;
        case FAPI_FONT_FEATURE_OtherBlues: return float_to_ushort(pfont->data.OtherBlues.values[index]);
        case FAPI_FONT_FEATURE_FamilyBlues_count: return pfont->data.FamilyBlues.count;
        case FAPI_FONT_FEATURE_FamilyBlues: return float_to_ushort(pfont->data.FamilyBlues.values[index]);
        case FAPI_FONT_FEATURE_FamilyOtherBlues_count: return pfont->data.FamilyOtherBlues.count;
        case FAPI_FONT_FEATURE_FamilyOtherBlues: return float_to_ushort(pfont->data.FamilyOtherBlues.values[index]);
        case FAPI_FONT_FEATURE_BlueShift: return float_to_ushort(pfont->data.BlueShift);
        case FAPI_FONT_FEATURE_BlueFuzz: return float_to_ushort(pfont->data.BlueShift);
        case FAPI_FONT_FEATURE_StdHW: return (pfont->data.StdHW.count == 0 ? 0 : float_to_ushort(pfont->data.StdHW.values[0])); /* UFST bug ? */
        case FAPI_FONT_FEATURE_StdVW: return (pfont->data.StdVW.count == 0 ? 0 : float_to_ushort(pfont->data.StdVW.values[0])); /* UFST bug ? */
        case FAPI_FONT_FEATURE_StemSnapH_count: return pfont->data.StemSnapH.count;
        case FAPI_FONT_FEATURE_StemSnapH: return float_to_ushort(pfont->data.StemSnapH.values[index]);
        case FAPI_FONT_FEATURE_StemSnapV_count: return pfont->data.StemSnapV.count;
        case FAPI_FONT_FEATURE_StemSnapV: return float_to_ushort(pfont->data.StemSnapV.values[index]);
        case FAPI_FONT_FEATURE_ForceBold: return pfont->data.ForceBold;
        case FAPI_FONT_FEATURE_LanguageGroup: return pfont->data.LanguageGroup;
        case FAPI_FONT_FEATURE_lenIV: return (ff->need_decrypt ? 0 : pfont->data.lenIV);
        case FAPI_FONT_FEATURE_GlobalSubrs_count: 
            {   ref *Private, *GlobalSubrs;
                if (dict_find_string(pdr, "Private", &Private) <= 0)
                    return 0;
                if (dict_find_string(Private, "GlobalSubrs", &GlobalSubrs) <= 0)
                    return 0;;
                return r_size(GlobalSubrs);
            }
        case FAPI_FONT_FEATURE_Subrs_count: 
            {   ref *Private, *Subrs;
                if (dict_find_string(pdr, "Private", &Private) <= 0)
                    return 0;
                if (dict_find_string(Private, "Subrs", &Subrs) <= 0)
                    return 0;
                return r_size(Subrs);
            }
	    /* Multiple Master specific */
        case FAPI_FONT_FEATURE_DollarBlend: 
	    {   ref *DBlend; 
		if (dict_find_string(pdr, "$Blend", &DBlend) <= 0)
		    return 0;
		return 1;
	    }
        case FAPI_FONT_FEATURE_BlendAxisTypes_count: 
	    {   ref *Info, *Axes; 
		if (dict_find_string(pdr, "FontInfo", &Info) <= 0)
		    return 0;
		if (dict_find_string(Info, "BlendAxisTypes", &Axes) <= 0)
		    return 0;
		return r_size(Axes);
	    }
        case FAPI_FONT_FEATURE_BlendFontInfo_count: 
	    {   ref *Info, *FontInfo; 
		if (dict_find_string(pdr, "Blend", &Info) <= 0)
		    return 0;
		if (dict_find_string(Info, "FontInfo", &FontInfo) <= 0)
		    return 0;
		return dict_length(FontInfo);
	    }
        case FAPI_FONT_FEATURE_BlendPrivate_count: 
	    {   ref *Info, *Private; 
		if (dict_find_string(pdr, "Blend", &Info) <= 0)
		    return 0;
		if (dict_find_string(Info, "Private", &Private) <= 0)
		    return 0;
		return dict_length(Private);
	    }
        case FAPI_FONT_FEATURE_WeightVector_count: 
	    {   ref *Array; 
		if (dict_find_string(pdr, "WeightVector", &Array) <= 0)
		    return 0;
		return r_size(Array);
	    }
        case FAPI_FONT_FEATURE_BlendDesignPositionsArrays_count: 
	    {   ref *Info, *Array; 
		if (dict_find_string(pdr, "FontInfo", &Info) <= 0)
		    return 0;
		if (dict_find_string(Info, "BlendDesignPositions", &Array) <= 0)
		    return 0;
		return r_size(Array);
	    }
        case FAPI_FONT_FEATURE_BlendDesignMapArrays_count: 
	    {   ref *Info, *Array; 
		if (dict_find_string(pdr, "FontInfo", &Info) <= 0)
		    return 0;
		if (dict_find_string(Info, "BlendDesignMap", &Array) <= 0)
		    return 0;
		return r_size(Array);
	    }
        case FAPI_FONT_FEATURE_BlendDesignMapSubArrays_count: 
	    {   ref *Info, *Array, SubArray; 
		if (dict_find_string(pdr, "FontInfo", &Info) <= 0)
		    return 0;
		if (dict_find_string(Info, "BlendDesignMap", &Array) <= 0)
		    return 0;
		if (array_get(ff->memory, Array, index, &SubArray) < 0)
		    return 0;
		return r_size(&SubArray);
	    }
	case FAPI_FONT_FEATURE_DollarBlend_length:
	    {   ref *DBlend, Element, name, string; 
		int i, length = 0;
		char Buffer[32];
		if (dict_find_string(pdr, "$Blend", &DBlend) <= 0)
		    return 0;
		for (i = 0;i < r_size(DBlend);i++) {
		    if (array_get(ff->memory, DBlend, i, &Element) < 0)
			return 0;
		    switch (r_btype(&Element)) {
			case t_name:
			    name_string_ref(ff->memory, &name, &string);
			    length += r_size(&string) + 1;
			    break;
			case t_real:
			    sprintf(Buffer, "%f", Element.value.realval);
			    length += strlen(Buffer) + 1;
			    break;
			case t_integer:
			    sprintf(Buffer, "%d", Element.value.intval);
			    length += strlen(Buffer) + 1;
			    break;
			case t_operator:
			    { op_def const *op;

			    op = op_index_def(r_size(&Element));
			    length += strlen(op->oname + 1) + 1;
			    }
			    break;
			default:
			    break;
		    }
		}
		return length;
	    }
	    /* End MM specifics */
    }
    return 0;
}

static ulong FAPI_FF_get_long(FAPI_font *ff, fapi_font_feature var_id, int index)
{   gs_font_type1 *pfont = (gs_font_type1 *)ff->client_font_data;
    ref *pdr = (ref *)ff->client_font_data2;

    switch((int)var_id) {
        case FAPI_FONT_FEATURE_UniqueID: return pfont->UID.id;
        case FAPI_FONT_FEATURE_BlueScale: return (ulong)(pfont->data.BlueScale * 65536); 
        case FAPI_FONT_FEATURE_Subrs_total_size :
            {   ref *Private, *Subrs, v;
                int lenIV = max(pfont->data.lenIV, 0), k;
                ulong size = 0;
                long i;
                const char *name[2] = {"Subrs", "GlobalSubrs"};
                if (dict_find_string(pdr, "Private", &Private) <= 0)
                    return 0;
                for (k = 0; k < 2; k++) {
                    if (dict_find_string(Private, name[k], &Subrs) > 0)
                        for (i = r_size(Subrs) - 1; i >= 0; i--) {
                            array_get(pfont->memory, Subrs, i, &v);
                            size += r_size(&v) - (ff->need_decrypt ? 0 : lenIV);
                        }
                }
                return size;
            }
        case FAPI_FONT_FEATURE_TT_size:
            return true_type_size(pdr);
    }
    return 0;
}

static float FAPI_FF_get_float(FAPI_font *ff, fapi_font_feature var_id, int index)
{   gs_font_base *pbfont = (gs_font_base *)ff->client_font_data;
    ref *pdr = (ref *)ff->client_font_data2;

    switch((int)var_id) {
        case FAPI_FONT_FEATURE_FontMatrix:
            {   double FontMatrix_div = (ff->is_cid && !IsCIDFont(pbfont) ? 1000 : 1);
                switch(index) {
                    case 0 : return pbfont->base->FontMatrix.xx / FontMatrix_div;
                    case 1 : return pbfont->base->FontMatrix.xy / FontMatrix_div;
                    case 2 : return pbfont->base->FontMatrix.yx / FontMatrix_div;
                    case 3 : return pbfont->base->FontMatrix.yy / FontMatrix_div;
                    case 4 : return pbfont->base->FontMatrix.tx / FontMatrix_div;
                    case 5 : return pbfont->base->FontMatrix.ty / FontMatrix_div;
                }
            } 
        case FAPI_FONT_FEATURE_WeightVector: 
	    {   ref *Array, value; 

		if (dict_find_string(pdr, "WeightVector", &Array) <= 0)
		    return 0;
		if (array_get(ff->memory, Array, index, &value) < 0)
		    return 0;
		if (!r_has_type(&value, t_integer)) {
		    if (r_has_type(&value, t_real)) {
			return value.value.realval;
		    } else
			return 0;
		}
		else
		    return (float)value.value.intval;
	    }
	case FAPI_FONT_FEATURE_BlendDesignPositionsArrayValue:
	    {	ref *Info, *Array, SubArray, value;
		int array_index = index / 8;
		index %= 8;
		if (dict_find_string(pdr, "FontInfo", &Info) <= 0)
		    return 0;
		if (dict_find_string(Info, "BlendDesignPositions", &Array) <= 0)
		    return 0;
		if (array_get(ff->memory, Array, array_index, &SubArray) < 0)
		    return 0;
		if (array_get(ff->memory, &SubArray, index, &value) < 0)
		    return 0;
		if (!r_has_type(&value, t_integer)) {
		    if (r_has_type(&value, t_real)) {
			return value.value.realval;
		    } else
			return 0;
		}
		else
		    return (float)value.value.intval;
	    }
	case FAPI_FONT_FEATURE_BlendDesignMapArrayValue:
	    {	ref *Info, *Array, SubArray, SubSubArray, value;
		int array_index = index / 64, subarray_index = (index %64) / 8;
		index %= 8;
		if (dict_find_string(pdr, "FontInfo", &Info) <= 0)
		    return 0;
		if (dict_find_string(Info, "BlendDesignMap", &Array) <= 0)
		    return 0;
		if (array_get(ff->memory, Array, array_index, &SubArray) < 0)
		    return 0;
		if (array_get(ff->memory, &SubArray, index, &SubSubArray) < 0)
		    return 0;
		if (array_get(ff->memory, &SubSubArray, index, &value) < 0)
		    return 0;
		if (!r_has_type(&value, t_integer)) {
		    if (r_has_type(&value, t_real)) {
			return value.value.realval;
		    } else
			return 0;
		}
		else
		    return (float)value.value.intval;
	    }
    }
    return 0;
}

static int FAPI_FF_get_name(FAPI_font *ff, fapi_font_feature var_id, int index, char *Buffer, int len)
{
    ref name, string, *pdr = (ref *)ff->client_font_data2;

    switch((int)var_id) {
	case FAPI_FONT_FEATURE_BlendAxisTypes:
	    {   ref *Info, *Axes; 
		if (dict_find_string(pdr, "FontInfo", &Info) <= 0)
		    return 0;
		if (dict_find_string(Info, "BlendAxisTypes", &Axes) <= 0)
		    return 0;
		if(!r_has_type(Axes, t_array))
		    return 0;
		if (array_get(ff->memory, Axes, index, &name) < 0)
		    return 0;
	    }
    }
    name_string_ref(ff->memory, &name, &string);
    if(r_size(&string) >= len)
	return 0;
    memcpy(Buffer, string.value.const_bytes, r_size(&string));
    Buffer[r_size(&string)] = 0x00;
    return 1;
}

static int FAPI_FF_get_proc(FAPI_font *ff, fapi_font_feature var_id, int index, char *Buffer)
{
    ref *pdr = (ref *)ff->client_font_data2;
    char *ptr = Buffer;

    if (!Buffer)
	return 0;

    switch((int)var_id) {
	case FAPI_FONT_FEATURE_DollarBlend:
	    {   ref *DBlend, Element; 
		int i;
		char Buf[32];
		if (dict_find_string(pdr, "$Blend", &DBlend) <= 0)
		    return 0;
		for (i = 0;i < r_size(DBlend);i++) {
		    *ptr++ = 0x20;
		    if (array_get(ff->memory, DBlend, i, &Element) < 0)
			return 0;
		    switch (r_btype(&Element)) {
			case t_real:
			    sprintf(Buf, "%f", Element.value.realval);
			    strcpy(ptr, Buf);
			    ptr += strlen(Buf);
			    break;
			case t_integer:
			    sprintf(Buf, "%d", Element.value.intval);
			    strcpy(ptr, Buf);
			    ptr += strlen(Buf);
			    break;
			case t_operator:
			    { op_def const *op;

			    op = op_index_def(r_size(&Element));
			    strcpy(ptr, op->oname + 1);
			    ptr += strlen(op->oname + 1);
			    }
			    break;
			default:
			    break;
		    }
		}
	    }
    }
    return ptr - Buffer;
}
static inline void decode_bytes(byte *p, const byte *s, int l, int lenIV)
{   ushort state = 4330;

    for (; l; s++, l--) {
        uchar c = (*s ^ (state >> 8));
        state = (*s + state) * crypt_c1 + crypt_c2;
        if (lenIV > 0) 
            lenIV--;
        else {
            *p = c;
            p++;
        }
    }
}

static ushort get_type1_data(FAPI_font *ff, const ref *type1string,
			      byte *buf, ushort buf_length)
{   gs_font_type1 *pfont = (gs_font_type1 *)ff->client_font_data;
    int lenIV = max(pfont->data.lenIV, 0);
    int length = r_size(type1string) - (ff->need_decrypt ? lenIV : 0);

    if (buf != 0) {
        int l = min(length, buf_length); /*safety */
        if (ff->need_decrypt && pfont->data.lenIV >= 0)
            decode_bytes(buf, type1string->value.const_bytes, l + lenIV, lenIV);
        else
            memcpy(buf, type1string->value.const_bytes, l);
    }
    return length;
}

static ushort FAPI_FF_get_gsubr(FAPI_font *ff, int index, byte *buf, ushort buf_length)
{   ref *pdr = (ref *)ff->client_font_data2;
    ref *Private, *GlobalSubrs, subr;

    if (dict_find_string(pdr, "Private", &Private) <= 0)
        return 0;
    if (dict_find_string(Private, "GlobalSubrs", &GlobalSubrs) <= 0)
	return 0;
    if (array_get(ff->memory, 
	      GlobalSubrs, index, &subr) < 0 || r_type(&subr) != t_string)
        return 0;
    return get_type1_data(ff, &subr, buf, buf_length);
}

static ushort FAPI_FF_get_subr(FAPI_font *ff, int index, byte *buf, ushort buf_length)
{   ref *pdr = (ref *)ff->client_font_data2;
    ref *Private, *Subrs, subr;

    if (dict_find_string(pdr, "Private", &Private) <= 0)
        return 0;
    if (dict_find_string(Private, "Subrs", &Subrs) <= 0)
	return 0;
    if (array_get(ff->memory, Subrs, index, &subr) < 0 || r_type(&subr) != t_string)
        return 0;
    return get_type1_data(ff, &subr, buf, buf_length);
}

static bool sfnt_get_glyph_offset(ref *pdr, gs_font_type42 *pfont42, int index, ulong *offset0, ulong *offset1)
{   /* Note : TTC is not supported and probably is unuseful for Type 42. */
    sfnts_reader r;
    int glyf_elem_size = (2 << pfont42->data.indexToLocFormat);

    sfnts_reader_init(&r, pdr);
    r.seek(&r, pfont42->data.loca + index * glyf_elem_size);
    *offset0 = pfont42->data.glyf + (glyf_elem_size == 2 ? r.rword(&r) * 2 : r.rlong(&r));
    *offset1 = pfont42->data.glyf + (glyf_elem_size == 2 ? r.rword(&r) * 2 : r.rlong(&r));
    return r.error;
}

static int get_GlyphDirectory_data_ptr(const gs_memory_t *mem,
					ref *pdr, int char_code, const byte **ptr)
{
    ref *GlyphDirectory, glyph0, *glyph = &glyph0, glyph_index;
    if (dict_find_string(pdr, "GlyphDirectory", &GlyphDirectory) > 0) {
	if (((r_type(GlyphDirectory) == t_dictionary &&
		(make_int(&glyph_index, char_code),
		    dict_find(GlyphDirectory, &glyph_index, &glyph) > 0)) ||
	     (r_type(GlyphDirectory) == t_array &&
		array_get(mem, GlyphDirectory, char_code, &glyph0) >= 0)
            )
	    && r_type(glyph) == t_string) {
        *ptr = glyph->value.const_bytes;
	return r_size(glyph);
    } else
	/* We have a GlyphDirectory, but couldn't find the glyph. If we 
	 * return -1 then we will attempt to use glyf and loca which 
	 * will fail. Instead return 0, so we execute an 'empty' glyph.
	 */
	return 0;
    }
    return -1;
}

static bool get_MetricsCount(FAPI_font *ff)
{   if (!ff->is_type1 && ff->is_cid) {
	gs_font_cid2 *pfcid = (gs_font_cid2 *)ff->client_font_data;

	return pfcid->cidata.MetricsCount;
    }
    return 0;
}



static ushort FAPI_FF_get_glyph(FAPI_font *ff, int char_code, byte *buf, ushort buf_length)
{   /* 
     * We assume that renderer requests glyph data with multiple
     * consecutive calls to this function.
     *
     * For a simple glyph it calls this function exactly twice: first
     * with buf == NULL for requesting the necessary buffer length, and
     * second with buf != NULL for requesting the data (the second call
     * may be skipped if the renderer discontinues the rendering).
     *
     * For a composite glyph it calls this function 2 * (N + 1)
     * times: 2 calls for the main glyph (same as above) followed with
     * 2 * N calls for subglyphs, where N is less or equal to the number
     * of subglyphs (N may be less if the renderer caches glyph data,
     * or discontinues rendering on an exception).
     */
    ref *pdr = (ref *)ff->client_font_data2;
    ushort glyph_length;

    if (ff->is_type1) {
        if (ff->is_cid) {
            const ref *glyph = ff->char_data;

            glyph_length = get_type1_data(ff, glyph, buf, buf_length);
        } else {
            ref *CharStrings, char_name, *glyph;
            if (ff->char_data != NULL) {
		/*
		 * Can't use char_code in this case because hooked Type 1 fonts
		 * with 'glyphshow' may render a character which has no
		 * Encoding entry.
		 */
                if (name_ref(ff->memory, ff->char_data, 
			     ff->char_data_len, &char_name, -1) < 0)
		    return -1;
		if (buf != NULL) {
		    /* 
		     * Trigger the next call to the 'seac' case below. 
		     * Here we use the assumption about call sequence
		     * being documented above.
		     */
		    ff->char_data = NULL; 
		}
	    }  else { /* seac */
		i_ctx_t *i_ctx_p = (i_ctx_t *)ff->client_ctx_p;
		ref *StandardEncoding;

		if (dict_find_string(systemdict, "StandardEncoding", &StandardEncoding) <= 0 ||
		    array_get(ff->memory, StandardEncoding, char_code, &char_name) < 0)
		    if (name_ref(ff->memory, (const byte *)".notdef", 7, &char_name, -1) < 0)
			return -1;
	    }
            if (dict_find_string(pdr, "CharStrings", &CharStrings) <= 0)
                return -1;
            if (dict_find(CharStrings, &char_name, &glyph) <= 0)
                return -1;
            glyph_length = get_type1_data(ff, glyph, buf, buf_length);
        }
    } else { /* type 42 */
	const byte *data_ptr;
	int l = get_GlyphDirectory_data_ptr(ff->memory, pdr, char_code, &data_ptr);

	if (l >= 0) {
	    int MetricsCount = get_MetricsCount(ff), mc = MetricsCount << 1;

            glyph_length = max((ushort)(l - mc), 0); /* safety */
            if (buf != 0 && glyph_length > 0)
                memcpy(buf, data_ptr + mc, min(glyph_length, buf_length)/* safety */);
        } else {
            gs_font_type42 *pfont42 = (gs_font_type42 *)ff->client_font_data;
            ulong offset0, offset1;
	    bool error = sfnt_get_glyph_offset(pdr, pfont42, char_code, &offset0, &offset1);

            glyph_length = (error ? -1 : offset1 - offset0);
            if (buf != 0 && !error) {
                sfnts_reader r;
                sfnts_reader_init(&r, pdr);

                r.seek(&r, offset0);
                r.rstring(&r, buf, min(glyph_length, buf_length)/* safety */);
                if (r.error)
                    glyph_length = -1;
            }
        }
    }
    return glyph_length;
}

static const FAPI_font ff_stub = {
    0, /* server_font_data */
    0, /* need_decrypt */
    NULL, /* const gs_memory_t */
    0, /* font_file_path */
    0, /* subfont */
    false, /* is_type1 */
    false, /* is_cid */
    false, /* is_outline_font */
    false, /* is_mtx_skipped */
    false, /* is_vertical */
    0, /* client_ctx_p */
    0, /* client_font_data */
    0, /* client_font_data2 */
    0, /* char_data */
    0, /* char_data_len */
    FAPI_FF_get_word,
    FAPI_FF_get_long,
    FAPI_FF_get_float,
    FAPI_FF_get_name,
    FAPI_FF_get_proc,
    FAPI_FF_get_gsubr,
    FAPI_FF_get_subr,
    FAPI_FF_get_glyph,
    FAPI_FF_serialize_tt_font
};

static int FAPI_get_xlatmap(i_ctx_t *i_ctx_p, char **xlatmap)
{   ref *pref;
    int code;

    if ((code = dict_find_string(systemdict, ".xlatmap", &pref)) < 0)
	return code;
    if (r_type(pref) != t_string)
        return_error(e_typecheck);
    *xlatmap = (char *)pref->value.bytes;
    /*  Note : this supposes that xlatmap doesn't move in virtual memory.
        Garbager must not be called while plugin executes get_scaled_font, get_decodingID.
        Fix some day with making copy of xlatmap in system memory.
    */
    return 0;
}

static int renderer_retcode(i_ctx_t *i_ctx_p, FAPI_server *I, FAPI_retcode rc)
{   if (rc == 0)
	return 0;
    eprintf2("Error: Font Renderer Plugin ( %s ) return code = %d\n", I->ig.d->subtype, rc);
    return rc < 0 ? rc : e_invalidfont;
}

static int zFAPIavailable(i_ctx_t *i_ctx_p)
{   i_plugin_holder *h = i_plugin_get_list(i_ctx_p);
    bool available = true;
    os_ptr op = osp;

    for (; h != 0; h = h->next)
       	if (!strcmp(h->I->d->type,"FAPI"))
            goto found;
    available = false;
    found :
    push(1);
    make_bool(op, available);
    return 0;
} 

static void get_server_param(i_ctx_t *i_ctx_p, const char *subtype, const byte **server_param, int *server_param_size)
{   ref *FAPIconfig, *options, *server_options;

    if (dict_find_string(systemdict, ".FAPIconfig", &FAPIconfig) >= 0 && r_has_type(FAPIconfig, t_dictionary)) {
	if (dict_find_string(FAPIconfig, "ServerOptions", &options) >= 0 && r_has_type(options, t_dictionary)) {
	    if (dict_find_string(options, subtype, &server_options) >= 0 && r_has_type(server_options, t_string)) {
		*server_param = server_options->value.const_bytes;
		*server_param_size = r_size(server_options);
	    }
	}
    }
}

static int FAPI_find_plugin(i_ctx_t *i_ctx_p, const char *subtype, FAPI_server **pI)
{   i_plugin_holder *h = i_plugin_get_list(i_ctx_p);
    int code;

    for (; h != 0; h = h->next)
       	if (!strcmp(h->I->d->type,"FAPI"))
	    if (!strcmp(h->I->d->subtype, subtype)) {
	        FAPI_server *I = *pI = (FAPI_server *)h->I;
		const byte *server_param = NULL;
		int server_param_size = 0;

		get_server_param(i_ctx_p, subtype, &server_param, &server_param_size);
	        if ((code = renderer_retcode(i_ctx_p, I, I->ensure_open(I, server_param, server_param_size))) < 0)
		    return code;
	        return 0;
	    }
    return_error(e_invalidfont);
}

static inline void release_typeface(FAPI_server *I, void **server_font_data)
{
    I->release_typeface(I, *server_font_data);
    I->face.font_id = gs_no_id;
    if (I->ff.server_font_data == *server_font_data)
	I->ff.server_font_data = 0;
    *server_font_data = 0;
}

static int FAPI_prepare_font(i_ctx_t *i_ctx_p, FAPI_server *I, ref *pdr, gs_font_base *pbfont, 
                              const char *font_file_path, const FAPI_font_scale *font_scale, 
			      const char *xlatmap, int BBox[4], const char **decodingID)
{   /* Returns 1 iff BBox is set. */
    /* Cleans up server's font data if failed. */

    /* A renderer may need to access the top level font's data of 
     * a CIDFontType 0 (FontType 9) font while preparing its subfonts,
     * and/or perform a completion action with the top level font after
     * its descendants are prepared. Therefore with such fonts
     * we first call get_scaled_font(..., FAPI_TOPLEVEL_BEGIN), then	
     * get_scaled_font(..., i) with eash descendant font index i,
     * and then get_scaled_font(..., FAPI_TOPLEVEL_COMPLETE).
     * For other fonts we don't call with 'i'.
     *
     * Actually server's data for top level CIDFontTYpe 0 non-disk fonts should not be important, 
     * because with non-disk fonts FAPI_do_char never deals with the top-level font, 
     * but does with its descendants individually.
     * Therefore a recommendation for the renderer is don't build any special
     * data for the top-level non-disk font of CIDFontType 0, but return immediately
     * with success code and NULL data pointer.
     *
     * get_scaled_font may be called for same font at second time,
     * so the renderen must check whether the data is already built.
     */
    int code, bbox_set = 0;
    ref *SubfontId;
    int subfont;

    I->ff = ff_stub;
    if (dict_find_string(pdr, "SubfontId", &SubfontId) >= 0 && r_has_type(SubfontId, t_integer))
        subfont = SubfontId->value.intval;
    else
	subfont = 0;
    I->ff.subfont = subfont;
    I->ff.font_file_path = font_file_path;
    I->ff.is_type1 = IsType1GlyphData(pbfont);
    I->ff.is_vertical = (pbfont->WMode != 0);
    I->ff.memory = imemory;
    I->ff.client_ctx_p = i_ctx_p;
    I->ff.client_font_data = pbfont;
    I->ff.client_font_data2 = pdr;
    I->ff.server_font_data = pbfont->FAPI_font_data; /* Possibly pass it from zFAPIpassfont. */
    I->ff.is_cid = IsCIDFont(pbfont);
    I->ff.is_outline_font = pbfont->PaintType != 0;
    I->ff.is_mtx_skipped = (get_MetricsCount(&I->ff) != 0);
    if ((code = renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &I->ff,
                                         font_scale, xlatmap, FAPI_TOPLEVEL_BEGIN))) < 0)
	return code;
    pbfont->FAPI_font_data = I->ff.server_font_data; /* Save it back to GS font. */
    if (I->ff.server_font_data != 0) {
	if ((code = renderer_retcode(i_ctx_p, I, I->get_font_bbox(I, &I->ff, BBox))) < 0) {
	    release_typeface(I, &pbfont->FAPI_font_data);
	    return code;
	}
	bbox_set = 1;
    }
    if (xlatmap != NULL && pbfont->FAPI_font_data != NULL)
	if ((code = renderer_retcode(i_ctx_p, I, I->get_decodingID(I, &I->ff, decodingID))) < 0) {
	    release_typeface(I, &pbfont->FAPI_font_data);
	    return code;
	}
    /* Prepare descendant fonts : */
    if (font_file_path == NULL && I->ff.is_type1 && I->ff.is_cid) { /* Renderers should expect same condition. */
        gs_font_cid0 *pfcid = (gs_font_cid0 *)pbfont;
        gs_font_type1 **FDArray = pfcid->cidata.FDArray;
        int i, n = pfcid->cidata.FDArray_size;
        ref *rFDArray, f;

        if (dict_find_string(pdr, "FDArray", &rFDArray) <= 0 || r_type(rFDArray) != t_array)
            return_error(e_invalidfont);
        I->ff = ff_stub;
        I->ff.is_type1 = true;
	I->ff.is_vertical = false; /* A subfont may be shared with another fonts. */
	I->ff.memory = imemory;
	I->ff.client_ctx_p = i_ctx_p;
        for (i = 0; i < n; i++) {
            gs_font_type1 *pbfont1 = FDArray[i];
	    int BBox_temp[4];

            pbfont1->FontBBox = pbfont->FontBBox; /* Inherit FontBBox from the type 9 font. */
            if(array_get(imemory, rFDArray, i, &f) < 0 || r_type(&f) != t_dictionary)
                return_error(e_invalidfont);
            I->ff.client_font_data = pbfont1;
            pbfont1->FAPI = pbfont->FAPI;
            I->ff.client_font_data2 = &f;
            I->ff.server_font_data = pbfont1->FAPI_font_data;
            I->ff.is_cid = true;
	    I->ff.is_outline_font = pbfont1->PaintType != 0;
	    I->ff.is_mtx_skipped = (get_MetricsCount(&I->ff) != 0);
	    I->ff.subfont = 0;
            if ((code = renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &I->ff,
	                                 font_scale, NULL, i))) < 0)
		break;
            pbfont1->FAPI_font_data = I->ff.server_font_data; /* Save it back to GS font. */
	    /* Try to do something with the descendant font to ensure that it's working : */
	    if ((code = renderer_retcode(i_ctx_p, I, I->get_font_bbox(I, &I->ff, BBox_temp))) < 0)
		break;
        }
	if (i == n) {
	    code = renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &I->ff,
	                                 font_scale, NULL, FAPI_TOPLEVEL_COMPLETE));
	    if (code >= 0)
		return bbox_set; /* Full success. */
	}
	/* Fail, release server's font data : */
        for (i = 0; i < n; i++) {
            gs_font_type1 *pbfont1 = FDArray[i];

	    if (pbfont1->FAPI_font_data != NULL)
		release_typeface(I, &pbfont1->FAPI_font_data);
	}
	if (pbfont->FAPI_font_data != NULL)
	    release_typeface(I, &pbfont->FAPI_font_data);
	return_error(e_invalidfont);
    } else {
	code = renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &I->ff,
	                                 font_scale, xlatmap, FAPI_TOPLEVEL_COMPLETE));
        if (code < 0) {
	    release_typeface(I, &pbfont->FAPI_font_data);
	    return code;
        }
	return bbox_set;
    }
}

static int FAPI_refine_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font_base *pbfont, const char *font_file_path)
{   ref *pdr = op;  /* font dict */
    double size, size1;
    int BBox[4], scale;
    const char *decodingID = NULL;
    char *xlatmap = NULL;
    FAPI_server *I = pbfont->FAPI;
    FAPI_font_scale font_scale = {{1, 0, 0, 1, 0, 0}, {0, 0}, {1, 1}, true};
    ref *Decoding_old;
    int code;

    if (font_file_path != NULL && pbfont->FAPI_font_data == NULL)
        if ((code = FAPI_get_xlatmap(i_ctx_p, &xlatmap)) < 0)
	    return code;
    scale = 1 << I->frac_shift;
    size1 = size = 1 / hypot(pbfont->FontMatrix.xx, pbfont->FontMatrix.xy);
    if (size < 1000)
	size = 1000;
    if (size1 > 100)
        size1 = (int)(size1 + 0.5);
    font_scale.matrix[0] = font_scale.matrix[3] = (int)(size * scale + 0.5);
    font_scale.HWResolution[0] = (FracInt)(72 * scale);
    font_scale.HWResolution[1] = (FracInt)(72 * scale);

    code = FAPI_prepare_font(i_ctx_p, I, pdr, pbfont, font_file_path, &font_scale, xlatmap, BBox, &decodingID);
    if (code < 0)
	return code;

    if (code > 0) {
	/* Refine FontBBox : */
	ref *v, mat[4], arr;

	pbfont->FontBBox.p.x = (double)BBox[0] * size1 / size;
	pbfont->FontBBox.p.y = (double)BBox[1] * size1 / size;
	pbfont->FontBBox.q.x = (double)BBox[2] * size1 / size;
	pbfont->FontBBox.q.y = (double)BBox[3] * size1 / size;
	if (dict_find_string(op, "FontBBox", &v) > 0) {
	    if(!r_has_type(v, t_array) && !r_has_type(v, t_shortarray))
		return_error(e_invalidfont);
	    if (r_size(v) < 4)
		return_error(e_invalidfont);

	    make_real(&mat[0], pbfont->FontBBox.p.x);
	    make_real(&mat[1], pbfont->FontBBox.p.y);
	    make_real(&mat[2], pbfont->FontBBox.q.x);
	    make_real(&mat[3], pbfont->FontBBox.q.y);
	    if(r_has_type(v, t_shortarray)) {
		/* Create a new full blown array in case the values are reals */
		code = ialloc_ref_array(&arr, a_all, 4, "array");
		if (code < 0)
		    return code;
		v = &arr;
		code = idict_put_string(op, "FontBBox", &arr);
		if (code < 0)
		    return code;
	    }
	    ref_assign_old(v, v->value.refs + 0, &mat[0], "FAPI_refine_font_BBox");
	    ref_assign_old(v, v->value.refs + 1, &mat[1], "FAPI_refine_font_BBox");
	    ref_assign_old(v, v->value.refs + 2, &mat[2], "FAPI_refine_font_BBox");
	    ref_assign_old(v, v->value.refs + 3, &mat[3], "FAPI_refine_font_BBox");
	}
    }

    /* Assign a Decoding : */
    if (decodingID != 0 && *decodingID && dict_find_string(pdr, "Decoding", &Decoding_old) <= 0) {
       ref Decoding;

       if (IsCIDFont(pbfont)) {
            ref *CIDSystemInfo, *Ordering, SubstNWP;
            byte buf[30];
            int ordering_length, decodingID_length = min(strlen(decodingID), sizeof(buf) - 2);

            if (dict_find_string(pdr, "CIDSystemInfo", &CIDSystemInfo) <= 0 || !r_has_type(CIDSystemInfo, t_dictionary))
                return_error(e_invalidfont);
            if (dict_find_string(CIDSystemInfo, "Ordering", &Ordering) <= 0 || !r_has_type(Ordering, t_string))
                return_error(e_invalidfont);
            ordering_length = min(r_size(Ordering), sizeof(buf) - 2 - decodingID_length);
            memcpy(buf, Ordering->value.const_bytes, ordering_length);
            if ((code = name_ref(imemory, buf, ordering_length, &SubstNWP, 0)) < 0)
		return code;
            if ((code = dict_put_string(pdr, "SubstNWP", &SubstNWP, NULL)) < 0)
		return code;
            buf[ordering_length] = '.';
            memcpy(buf + ordering_length + 1, decodingID, decodingID_length);
            buf[decodingID_length + 1 + ordering_length] = 0; /* Debug purpose only */
            if ((code = name_ref(imemory, buf, 
				 decodingID_length + 1 + ordering_length, &Decoding, 0)) < 0)
		return code;
        } else
            if ((code = name_ref(imemory, (const byte *)decodingID, 
				 strlen(decodingID), &Decoding, 0)) < 0)
		return code;
        if ((code = dict_put_string(pdr, "Decoding", &Decoding, NULL)) < 0)
	    return code;
    }
    return 0;
}

static int notify_remove_font(void *proc_data, void *event_data)
{   /* gs_font_finalize passes event_data == NULL, so check it here. */
    if (event_data == NULL) {
        gs_font_base *pbfont = proc_data;
	FAPI_server *I = pbfont->FAPI;

	if (pbfont->FAPI_font_data != 0) {
	    release_typeface(I, &pbfont->FAPI_font_data);
	}
    }
    return 0;
}

/*  <string|name> <font> <is_disk_font> .rebuildfontFAPI <string|name> <font> */
/*  Rebuild a font for handling it with an external renderer. 
    
    The font was built as a native GS font to allow easy access
    to font features. Then zFAPIrebuildfont sets FAPI entry
    into gx_font_base and replaces BuildGlyph and BuildChar 
    to enforce the FAPI handling.

    This operator must not be called with devices which embed fonts.

*/
static int zFAPIrebuildfont(i_ctx_t *i_ctx_p)
{   os_ptr op = osp;
    build_proc_refs build;
    gs_font *pfont;
    int code = font_param(op - 1, &pfont), code1;
    gs_font_base *pbfont = (gs_font_base *)pfont;
    ref *v;
    char *font_file_path = NULL, FAPI_ID[20];
    const byte *pchars;
    uint len;
    font_data *pdata;

    if (code < 0)
	return code;
    check_type(*op, t_boolean);
    if (pbfont->FAPI != 0) {
        /*  If the font was processed with zFAPIpassfont,
            it already has an attached FAPI and server_font_data.
            Don't change them here.
        */
    } else {
        if (dict_find_string(op - 1, "FAPI", &v) <= 0 || !r_has_type(v, t_name))
	    return_error(e_invalidfont);
        obj_string_data(imemory, v, &pchars, &len);
        len = min(len, sizeof(FAPI_ID) - 1);
        strncpy(FAPI_ID, (const char *)pchars, len);
        FAPI_ID[len] = 0;
        if ((code = FAPI_find_plugin(i_ctx_p, FAPI_ID, &pbfont->FAPI)) < 0)
	    return code;
    }
    pdata = (font_data *)pfont->client_data;
    if (dict_find_string(op - 1, "Path", &v) <= 0 || !r_has_type(v, t_string))
        v = NULL;
    if (pfont->FontType == ft_CID_encrypted && v == NULL) {
        if ((code = build_proc_name_refs(imemory, &build, ".FAPIBuildGlyph9", ".FAPIBuildGlyph9")) < 0)
	    return code;
	pop(1);
	return 0;
    } else
        if ((code = build_proc_name_refs(imemory, &build, ".FAPIBuildChar", ".FAPIBuildGlyph")) < 0)
	    return code;
    if (pdata->BuildChar.value.pname && build.BuildChar.value.pname &&
	name_index(imemory, &pdata->BuildChar) == name_index(imemory, &build.BuildChar)) {
        /* Already rebuilt - maybe a substituted font. */
    } else {
        ref_assign_new(&pdata->BuildChar, &build.BuildChar);
        ref_assign_new(&pdata->BuildGlyph, &build.BuildGlyph);
        if (v != NULL)
            font_file_path = ref_to_string(v, imemory_global, "font file path");
        code = FAPI_refine_font(i_ctx_p, op - 1, pbfont, font_file_path);
        if (font_file_path != NULL)
            gs_free_string(imemory_global, (byte *)font_file_path, r_size(v) + 1, "font file path");
        code1 = gs_notify_register(&pfont->notify_list, notify_remove_font, pbfont);
        (void)code1;  /* Recover possible error just ignoring it. */
    }
    pop(1);
    return code;
}

static ulong array_find(const gs_memory_t *mem, ref *Encoding, ref *char_name) {
    ulong n = r_size(Encoding), i;
    ref v;
    for (i = 0; i < n; i++)
        if (array_get(mem, Encoding, i, &v) < 0)
            break;
        else if(r_type(char_name) == r_type(&v) && char_name->value.const_pname == v.value.const_pname)
            return i;
    return 0;
}

static int outline_char(i_ctx_t *i_ctx_p, FAPI_server *I, int import_shift_v, gs_show_enum *penum_s, struct gx_path_s *path, bool close_path)
{   FAPI_path path_interface = path_interface_stub;
    FAPI_outline_handler olh;
    int code;

    olh.path = path;
    olh.x0 = penum_s->pgs->ctm.tx_fixed;
    olh.y0 = penum_s->pgs->ctm.ty_fixed;
    olh.close_path = close_path;
    olh.need_close = false;
    path_interface.olh = &olh;
    path_interface.shift = import_shift_v;
    if ((code = renderer_retcode(i_ctx_p, I, I->get_char_outline(I, &path_interface))) < 0)
	return code;
    if (olh.need_close && olh.close_path)
        if ((code = add_closepath(&path_interface)) < 0)
	    return code;
    return 0;
}

static void compute_em_scale(const gs_font_base *pbfont, FAPI_metrics *metrics, double FontMatrix_div, double *em_scale_x, double *em_scale_y)
{   /* optimize : move this stuff to FAPI_refine_font */
    gs_matrix *m = &pbfont->base->orig_FontMatrix;
    int rounding_x, rounding_y; /* Striking out the 'float' representation error in FontMatrix. */
    double sx, sy;

    if (m->xx == 0 && m->xy == 0 && m->yx == 0 && m->yy == 0)
	m = &pbfont->base->FontMatrix;
    sx = hypot(m->xx, m->xy) * metrics->em_x / FontMatrix_div;
    sy = hypot(m->yx, m->yy) * metrics->em_y / FontMatrix_div;
    rounding_x = (int)(0x00800000 / sx);
    rounding_y = (int)(0x00800000 / sy);
    *em_scale_x = (int)(sx * rounding_x + 0.5) / (double)rounding_x;
    *em_scale_y = (int)(sy * rounding_y + 0.5) / (double)rounding_y;
}

static int fapi_copy_mono(gx_device *dev1, FAPI_raster *rast, int dx, int dy)
{   if ((rast->line_step & (align_bitmap_mod - 1)) == 0)
        return dev_proc(dev1, copy_mono)(dev1, rast->p, 0, rast->line_step, 0, dx, dy, rast->width, rast->height, 0, 1);
    else { /* bitmap data needs to be aligned, make the aligned copy : */
        int line_step = bitmap_raster(rast->width), code;
        byte *p = gs_alloc_byte_array(dev1->memory, rast->height, line_step, "fapi_copy_mono");
        byte *q = p, *r = rast->p, *pe;
        if (p == NULL)
            return_error(e_VMerror);
        pe = p + rast->height * line_step;
        for (; q < pe; q+=line_step, r += rast->line_step)
            memcpy(q, r, rast->line_step);
        code = dev_proc(dev1, copy_mono)(dev1, p, 0, line_step, 0, dx, dy, rast->width, rast->height, 0, 1);
        gs_free_object(dev1->memory, p, "fapi_copy_mono");
        return code;
    }
}

static const int frac_pixel_shift = 4;

static int fapi_finish_render_aux(i_ctx_t *i_ctx_p, gs_font_base *pbfont, FAPI_server *I)
{   gs_text_enum_t *penum = op_show_find(i_ctx_p);
    gs_show_enum *penum_s = (gs_show_enum *)penum;
    gs_state *pgs;
    gx_device *dev1;
    gx_device *dev;
    const int import_shift_v = _fixed_shift - I->frac_shift;
    FAPI_raster rast;
    int code;
    extern_st(st_gs_show_enum);
    extern_st(st_gs_state);

    if(penum == NULL) {
	return_error(e_undefined);
    }
    dev = penum_s->dev;

    /* Ensure that pis points to a st_gs_gstate (graphics state) structure */
    if (gs_object_type(penum->memory, penum->pis) != &st_gs_state) {
	/* If pis is not a graphics state, see if the text enumerator is a 
	 * show enumerator, in which case we have a pointer to the graphics state there
	 */
	if (gs_object_type(penum->memory, penum) == &st_gs_show_enum) {
	    pgs = penum_s->pgs;
	} else
	    /* No graphics state, give up... */
	    return_error(e_undefined);
    } else 
	pgs = (gs_state *)penum->pis;

    dev1 = gs_currentdevice_inline(pgs); /* Possibly changed by zchar_set_cache. */

    if (SHOW_IS(penum, TEXT_DO_NONE)) {
        /* do nothing */
    } else if (igs->in_charpath) {
        if ((code = outline_char(i_ctx_p, I, import_shift_v, penum_s, pgs->show_gstate->path, !pbfont->PaintType)) < 0)
	    return code;
    } else {
        int code = I->get_char_raster(I, &rast);
        if (code == e_limitcheck || pbfont->PaintType) {
            /* The server provides an outline instead the raster. */
            gs_imager_state *pis = (gs_imager_state *)pgs->show_gstate;
            gs_point pt;

            if ((code = gs_currentpoint(pgs, &pt)) < 0)
		return code;
            if ((code = outline_char(i_ctx_p, I, import_shift_v, penum_s, pgs->path, !pbfont->PaintType)) < 0)
		return code;
            if ((code = gs_imager_setflat((gs_imager_state *)pgs, gs_char_flatness(pis, 1.0))) < 0)
		return code;
            if (pbfont->PaintType) {
		float lw = gs_currentlinewidth(pgs);

		gs_setlinewidth(pgs, pbfont->StrokeWidth);
                code = gs_stroke(pgs);
		gs_setlinewidth(pgs, lw);
		if (code < 0)
		    return code;
            } else
                if ((code = gs_fill(pgs)) < 0)
		    return code;
            if ((code = gs_moveto(pgs, pt.x, pt.y)) < 0)
		return code;
        } else {
	    int rast_orig_x =   rast.orig_x;
	    int rast_orig_y = - rast.orig_y;

            if (pgs->in_cachedevice == CACHE_DEVICE_CACHING) { /* Using GS cache */
		/*  GS and renderer may transform coordinates few differently.
		    The best way is to make set_cache_device to take the renderer's bitmap metrics immediately,
		    but we need to account CDevProc, which may truncate the bitmap.
		    Perhaps GS overestimates the bitmap size,
		    so now we only add a compensating shift - the dx and dy.
		*/              
		if (rast.width != 0) {
		    int shift_rd = _fixed_shift  - frac_pixel_shift;
		    int rounding = 1 << (frac_pixel_shift - 1);
		    int dx = arith_rshift_slow((pgs->ctm.tx_fixed >> shift_rd) + rast_orig_x + rounding, frac_pixel_shift);
		    int dy = arith_rshift_slow((pgs->ctm.ty_fixed >> shift_rd) + rast_orig_y + rounding, frac_pixel_shift);

		    if (dx + rast.left_indent < 0 || dx + rast.left_indent + rast.black_width > dev1->width)
			eprintf2("Warning : Cropping a FAPI glyph while caching : dx=%d,%d.\n", 
				dx + rast.left_indent, dx + rast.left_indent + rast.black_width - dev1->width);
		    if (dy + rast.top_indent < 0 || dy + rast.top_indent + rast.black_height > dev1->height)
			eprintf2("Warning : Cropping a FAPI glyph while caching : dx=%d,%d.\n", 
				dy + rast.top_indent, dy + rast.top_indent + rast.black_height - dev1->height);
		    if ((code = fapi_copy_mono(dev1, &rast, dx, dy)) < 0)
			return code;

		    if (gs_object_type(penum->memory, penum) == &st_gs_show_enum) {
			penum_s->cc->offset.x += float2fixed(penum_s->fapi_glyph_shift.x);
			penum_s->cc->offset.y += float2fixed(penum_s->fapi_glyph_shift.y);
		    }
		}
            } else { /* Not using GS cache */
	        const gx_clip_path * pcpath = i_ctx_p->pgs->clip_path; 
                const gx_drawing_color * pdcolor = penum->pdcolor;

		if (gs_object_type(penum->memory, penum) == &st_gs_show_enum) {
		    if ((code = gx_image_fill_masked(dev, rast.p, 0, rast.line_step, 0,
			          (int)(pgs->ctm.tx + (double)rast_orig_x / (1 << frac_pixel_shift) + penum_s->fapi_glyph_shift.x + 0.5), 
			          (int)(pgs->ctm.ty + (double)rast_orig_y / (1 << frac_pixel_shift) + penum_s->fapi_glyph_shift.y + 0.5), 
			          rast.width, rast.height,
			          pdcolor, 1, rop3_default, pcpath)) < 0)
				    return code;
		} else {
		    if ((code = gx_image_fill_masked(dev, rast.p, 0, rast.line_step, 0,
			          (int)(pgs->ctm.tx + (double)rast_orig_x / (1 << frac_pixel_shift) + 0.5), 
			          (int)(pgs->ctm.ty + (double)rast_orig_y / (1 << frac_pixel_shift) + 0.5), 
			          rast.width, rast.height,
			          pdcolor, 1, rop3_default, pcpath)) < 0)
				    return code;
		}
            }
        }
    }
    pop(2);
    return 0;
}

static int fapi_finish_render(i_ctx_t *i_ctx_p)
{   os_ptr op = osp;
    gs_font *pfont;
    int code = font_param(op - 1, &pfont);

    if (code == 0) {
        gs_font_base *pbfont = (gs_font_base *) pfont;
        FAPI_server *I = pbfont->FAPI;
        code = fapi_finish_render_aux(i_ctx_p, pbfont, I);
        I->release_char_data(I);
    }
    return code;
}

static const byte *
find_substring(const byte *where, int length, const char *what)
{
    int l = strlen(what);
    int n = length - l;
    const byte *p = where;

    for (; n >= 0; n--, p++)
	if (!memcmp(p, what, l))
	    return p;
    return NULL;
}

#define GET_U16_MSB(p) (((uint)((p)[0]) << 8) + (p)[1])
#define GET_S16_MSB(p) (int)((GET_U16_MSB(p) ^ 0x8000) - 0x8000)

static int FAPI_do_char(i_ctx_t *i_ctx_p, gs_font_base *pbfont, gx_device *dev, char *font_file_path, bool bBuildGlyph, ref *charstring)
{   /* Stack : <font> <code|name> --> - */
    os_ptr op = osp;
    ref *pdr = op - 1;
    gs_text_enum_t *penum = op_show_find(i_ctx_p);
    gs_show_enum *penum_s = (gs_show_enum *)penum;
    /* 
        fixme: the following code needs to optimize with a maintainence of scaled font objects 
        in graphics library and in interpreter. Now we suppose that the renderer
        uses font cache, so redundant font opening isn't a big expense.
    */
    FAPI_char_ref cr = {0, 0, {0}, 0, false, NULL, 0, 0, 0, 0, 0, FAPI_METRICS_NOTDEF};
    const gs_matrix * ctm = &ctm_only(igs);
    int scale;
    FAPI_metrics metrics;
    FAPI_server *I = pbfont->FAPI;
    int client_char_code = 0;
    ref char_name, *SubfontId;
    bool is_TT_from_type42 = (pbfont->FontType == ft_TrueType && font_file_path == NULL);
    bool is_embedded_type1 = ((pbfont->FontType == ft_encrypted ||
			       pbfont->FontType == ft_encrypted2) && 
			      font_file_path == NULL);
    bool bCID = (IsCIDFont(pbfont) || charstring != NULL);
    bool bIsType1GlyphData = IsType1GlyphData(pbfont);
    gs_log2_scale_point log2_scale = {0, 0};
    int alpha_bits = (*dev_proc(dev, get_alpha_bits)) (dev, go_text);
    double FontMatrix_div = (bCID && bIsType1GlyphData && font_file_path == NULL ? 1000 : 1);
    bool bVertical = (gs_rootfont(igs)->WMode != 0), bVertical0 = bVertical;
    double sbw[4] = {0, 0, 0, 0};
    double em_scale_x, em_scale_y;
    gs_rect char_bbox;
    op_proc_t exec_cont = 0;
    int code;
    bool align_to_pixels = gs_currentaligntopixels(pbfont->dir);
    enum {
	SBW_DONE,
	SBW_SCALE,
	SBW_FROM_RENDERER
    } sbw_state = SBW_SCALE;

    I->ff = ff_stub;
    if(bBuildGlyph && !bCID) {
        check_type(*op, t_name);
    } else
        check_type(*op, t_integer);

    if (penum == 0)
        return_error(e_undefined);
    /* Compute the scale : */
    if (!SHOW_IS(penum, TEXT_DO_NONE) && !igs->in_charpath) {
        gs_currentcharmatrix(igs, NULL, 1); /* make char_tm valid */
        penum_s->fapi_log2_scale.x = -1;
        gx_compute_text_oversampling(penum_s, (gs_font *)pbfont, alpha_bits, &log2_scale);
        penum_s->fapi_log2_scale = log2_scale;
    }
    scale = 1 << I->frac_shift;
retry_oversampling:
    if (I->face.font_id != pbfont->id ||
	memcmp(&I->face.ctm, &ctm, sizeof(&I->face.ctm)) ||
	I->face.log2_scale.x != log2_scale.x ||
	I->face.log2_scale.y != log2_scale.y ||
	I->face.align_to_pixels != align_to_pixels ||
	I->face.HWResolution[0] != dev->HWResolution[0] ||
	I->face.HWResolution[1] != dev->HWResolution[1]
       ) {
	FAPI_font_scale font_scale = {{1, 0, 0, 1, 0, 0}, {0, 0}, {1, 1}, true};
        gs_matrix *base_font_matrix = &pbfont->base->orig_FontMatrix;
        double dx, dy;

	I->face.font_id = pbfont->id;
	I->face.ctm = *ctm;
	I->face.log2_scale = log2_scale;
	I->face.align_to_pixels = align_to_pixels;
	I->face.HWResolution[0] = dev->HWResolution[0];
	I->face.HWResolution[1] = dev->HWResolution[1];

	font_scale.subpixels[0] = 1 << log2_scale.x;
	font_scale.subpixels[1] = 1 << log2_scale.y;
	font_scale.align_to_pixels = align_to_pixels;

	if (base_font_matrix->xx == 0 && base_font_matrix->xy == 0 &&
	    base_font_matrix->yx == 0 && base_font_matrix->yy == 0)
	    base_font_matrix = &pbfont->base->FontMatrix;
        dx = hypot(base_font_matrix->xx, base_font_matrix->xy);
        dy = hypot(base_font_matrix->yx, base_font_matrix->yy);
        /*  Trick : we need to restore the font scale from ctm, pbfont->FontMatrix,
            and base_font_matrix. We assume that base_font_matrix is
            a multiple of pbfont->FontMatrix with a constant from scalefont.
            But we cannot devide ctm by pbfont->FontMatrix for getting
            a proper result: the base_font_matrix may be XY transposition,
            but we must not cut out the transposition from ctm.
            Therefore we use the norm of base_font_matrix columns as the divisors
            for X and Y. It is not clear what to do when base_font_matrix is anisotropic
            (i.e. dx != dy), but we did not meet such fonts before now.
        */
        font_scale.matrix[0] =  (FracInt)(ctm->xx * FontMatrix_div / dx * 72 / dev->HWResolution[0] * scale + 0.5);
        font_scale.matrix[1] = -(FracInt)(ctm->xy * FontMatrix_div / dy * 72 / dev->HWResolution[0] * scale + 0.5);
        font_scale.matrix[2] =  (FracInt)(ctm->yx * FontMatrix_div / dx * 72 / dev->HWResolution[1] * scale + 0.5);
        font_scale.matrix[3] = -(FracInt)(ctm->yy * FontMatrix_div / dy * 72 / dev->HWResolution[1] * scale + 0.5);
        font_scale.matrix[4] =  (FracInt)(ctm->tx * FontMatrix_div / dx * 72 / dev->HWResolution[0] * scale + 0.5);
        font_scale.matrix[5] =  (FracInt)(ctm->ty * FontMatrix_div / dy * 72 / dev->HWResolution[1] * scale + 0.5);
        /* Note: the ctm mapping here is upside down. */
	font_scale.HWResolution[0] = (FracInt)((double)dev->HWResolution[0] * font_scale.subpixels[0] * scale);
	font_scale.HWResolution[1] = (FracInt)((double)dev->HWResolution[1] * font_scale.subpixels[1] * scale);

	/* Prepare font data : */
	if (dict_find_string(pdr, "SubfontId", &SubfontId) > 0 && r_has_type(SubfontId, t_integer))
	    I->ff.subfont = SubfontId->value.intval;
	else
	    I->ff.subfont = 0;
	I->ff.memory = pbfont->memory;
	I->ff.font_file_path = font_file_path;
	I->ff.client_font_data = pbfont;
	I->ff.client_font_data2 = pdr;
	I->ff.server_font_data = pbfont->FAPI_font_data;
	I->ff.is_type1 = bIsType1GlyphData;
	I->ff.is_cid = bCID;
	I->ff.is_outline_font = pbfont->PaintType != 0;
	I->ff.is_mtx_skipped = (get_MetricsCount(&I->ff) != 0);
	I->ff.is_vertical = bVertical;
	I->ff.client_ctx_p = i_ctx_p;
	if ((code = renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &I->ff, &font_scale, 
				 NULL,  
				 (!bCID || (pbfont->FontType != ft_encrypted  &&
				            pbfont->FontType != ft_encrypted2)
				        ? FAPI_TOPLEVEL_PREPARED : FAPI_DESCENDANT_PREPARED)))) < 0)
	    return code;
    }

    /* Obtain the character name : */
    if (bCID) {
	int_param(op, 0xFFFF, &client_char_code);
        make_null(&char_name);
    } else if (r_has_type(op, t_integer)) {
	/* Translate from PS encoding to char name : */
	ref *Encoding;
	int_param(op, 0xFF, &client_char_code);
        if (dict_find_string(pdr, "Encoding", &Encoding) > 0 && 
            (r_has_type(Encoding, t_array) || 
	    r_has_type(Encoding, t_shortarray) || r_has_type(Encoding, t_mixedarray))) {
	    if (array_get(imemory, Encoding, client_char_code, &char_name) < 0)
	        if ((code = name_ref(imemory, (const byte *)".notdef", 7, &char_name, -1)) < 0)
		    return code;
	} else
            return_error(e_invalidfont);
    } else 
	char_name = *op;

    /* Obtain the character code or glyph index : */
    cr.char_codes_count = 1;
    if (bCID) {
        if (font_file_path != NULL) {
            ref *Decoding, *TT_cmap, *SubstNWP;
            ref src_type, dst_type;
	    bool is_glyph_index = true;
	    uint c;

            if (dict_find_string(pdr, "Decoding", &Decoding) <= 0 || !r_has_type(Decoding, t_dictionary))
                return_error(e_invalidfont);
            if (dict_find_string(pdr, "SubstNWP", &SubstNWP) <= 0 || !r_has_type(SubstNWP, t_array))
                return_error(e_invalidfont);
	    if (dict_find_string(pdr, "TT_cmap", &TT_cmap) <= 0 || !r_has_type(TT_cmap, t_dictionary)) {
		ref *DecodingArray, char_code, char_code1, ih;
		int i = client_char_code % 256, n;

		make_int(&ih, client_char_code / 256);
		/* Check the Decoding array for this block of CIDs */
		if (dict_find(Decoding, &ih, &DecodingArray) <= 0 || 
			!r_has_type(DecodingArray, t_array) ||
			array_get(imemory, DecodingArray, i, &char_code) < 0)
		    return_error(e_invalidfont);
		
		/* Check the Decoding entry */
		if (r_has_type(&char_code, t_integer))
		    n = 1; 
		else if (r_has_type(&char_code, t_array)) {
		    DecodingArray = &char_code;
		    i = 0;
		    n = r_size(DecodingArray);
		} else
		    return_error(e_invalidfont);

		for (;n--; i++) {
		    if (array_get(imemory, DecodingArray, i, &char_code1) < 0 ||
			!r_has_type(&char_code1, t_integer))
			return_error(e_invalidfont);

		    c = char_code1.value.intval;
		    I->check_cmap_for_GID(I, &c);
		    if (c != 0)
		        break;
		}
	    } else {
		code = cid_to_TT_charcode(imemory, Decoding, TT_cmap, SubstNWP,
				      client_char_code, &c, &src_type, &dst_type);
		if (code < 0)
		    return code;

		/* cid_to_TT_charcode() returns 1 if it found a 
                 * matching character code. Otherwise it returns
                 * zero after setting c to zero (.notdef glyph id)
                 * or a negative value on error. */
		if (code > 0)
		    is_glyph_index = false;
	    }
	    cr.char_codes[0] = c;
	    cr.is_glyph_index = is_glyph_index;
            /* fixme : process the narrow/wide/proportional mapping type,
	       using src_type, dst_type. Should adjust the 'matrix' above.
               Call get_font_proportional_feature for proper choice.
            */
        } else
            cr.char_codes[0] = client_char_code;
    } else if (is_TT_from_type42) {
        /* This font must not use 'cmap', so compute glyph index from CharStrings : */
	ref *CharStrings, *glyph_index;
        if (dict_find_string(pdr, "CharStrings", &CharStrings) <= 0 || !r_has_type(CharStrings, t_dictionary))
            return_error(e_invalidfont);
        if (dict_find(CharStrings, &char_name, &glyph_index) < 0) {
            cr.char_codes[0] = 0; /* .notdef */
            if ((code = name_ref(imemory, (const byte *)".notdef", 7, &char_name, -1)) < 0)
		return code;
        } else if (r_has_type(glyph_index, t_integer))
            cr.char_codes[0] = glyph_index->value.intval;
        else
            return_error(e_invalidfont);
        cr.is_glyph_index = true;
    } else if (is_embedded_type1) {
        /*  Since the client passes charstring by callback using I->ff.char_data,
            the client doesn't need to provide a good cr here.
            Perhaps since UFST uses char codes as glyph cache keys (UFST 4.2 cannot use names),
            we provide font char codes equal to document's char codes.
            This trick assumes that Encoding can't point different glyphs
            for same char code. The last should be true due to 
            PLRM3, "5.9.4 Subsetting and Incremental Definition of Glyphs".
        */
        if (r_has_type(op, t_integer))
            cr.char_codes[0] = client_char_code;
        else {
            /* 
	     * Reverse Encoding here, because it can be an incremental one. 
	     * Note that this can cause problems with UFST (see the comment above),
	     * if the encoding doesn't contain the glyph name rendered with glyphshow.
	     */
            ref *Encoding;
            if (dict_find_string(osp - 1, "Encoding", &Encoding) > 0)
                cr.char_codes[0] = (uint)array_find(imemory, Encoding, op);
            else
                return_error(e_invalidfont);
        }
    } else { /* a non-embedded font, i.e. a disk font */
        bool can_retrieve_char_by_name = false;
	const byte *p;

        obj_string_data(imemory, &char_name, &cr.char_name, &cr.char_name_length);
	p = find_substring(cr.char_name, cr.char_name_length, gx_extendeg_glyph_name_separator);
	if (p != NULL) {
	    cr.char_name_length = p - cr.char_name;
	    name_ref(pbfont->memory, cr.char_name, cr.char_name_length, &char_name, true);
	}
        if ((code = renderer_retcode(i_ctx_p, I, I->can_retrieve_char_by_name(I, &I->ff, &cr, &can_retrieve_char_by_name))) < 0)
	    return code;
        if (!can_retrieve_char_by_name) {
	    /* Translate from char name to encoding used with 3d party font technology : */
	    ref *Decoding, *char_code;
            if (dict_find_string(osp - 1, "Decoding", &Decoding) > 0 && r_has_type(Decoding, t_dictionary)) {
		if (dict_find(Decoding, &char_name, &char_code) > 0) {
		    code = 0;
		    if (r_has_type(char_code, t_integer)) {
			int_param(char_code, 0xFFFF, &cr.char_codes[0]);
		    } else if (r_has_type(char_code, t_array) || r_has_type(char_code, t_shortarray)) {
			int i;
			ref v;

			cr.char_codes_count = r_size(char_code);
			if (cr.char_codes_count > count_of(cr.char_codes))
			    code = gs_note_error(e_rangecheck);
			if (code >= 0) {
			    for (i = 0; i < cr.char_codes_count; i++) {
				code = array_get(imemory, char_code, i, &v);
				if (code < 0)
				    break;
				if (!r_has_type(char_code, t_integer)) {
				    code = gs_note_error(e_rangecheck);
				    break;
				}
				cr.char_codes[i] = v.value.intval;
			    }
			}
		    } else
			code = gs_note_error(e_rangecheck);
		    if (code < 0) {
			char buf[16];
			int l = cr.char_name_length;

			if (l > sizeof(buf) - 1)
			    l = sizeof(buf) - 1;
			memcpy(buf, cr.char_name, l);
			buf[l] = 0;
			eprintf1("Wrong decoding entry for the character '%s'.\n", buf);
			return_error(e_rangecheck);
		    }
		}
	    }
        }
    }
    cr.char_code = cr.char_codes[0];
    cr.client_char_code = client_char_code;
#if 0 /* Debug purpose only: search chars in UFST fonts. */
    cr.char_code = client_char_code; /* remove for release !!!!!!!!!!!!!!!! */
#endif
    /* Provide glyph data for renderer : */
    if (!I->ff.is_cid) {
	ref sname;
	name_string_ref(imemory, &char_name, &sname);
        I->ff.char_data = sname.value.const_bytes;
	I->ff.char_data_len = r_size(&sname);
    } else if (I->ff.is_type1)
        I->ff.char_data = charstring;

    /* Compute the metrics replacement : */

    if(bCID && !bIsType1GlyphData) {
	gs_font_cid2 *pfcid = (gs_font_cid2 *)pbfont;
	int MetricsCount = pfcid->cidata.MetricsCount;

	if (MetricsCount > 0) {
	    const byte *data_ptr;
	    int l = get_GlyphDirectory_data_ptr(imemory, pdr, cr.char_code, &data_ptr);

	    if (MetricsCount == 2 && l >= 4) {
		if (!bVertical0) {
		    cr.sb_x = GET_S16_MSB(data_ptr + 2) * scale;
		    cr.aw_x = GET_U16_MSB(data_ptr + 0) * scale;
		    cr.metrics_type = FAPI_METRICS_REPLACE;
		}
	    } else if (l >= 8){
		cr.sb_y = GET_S16_MSB(data_ptr + 2) * scale;
		cr.aw_y = GET_U16_MSB(data_ptr + 0) * scale;
		cr.sb_x = GET_S16_MSB(data_ptr + 6) * scale;
		cr.aw_x = GET_U16_MSB(data_ptr + 4) * scale;
		cr.metrics_type = FAPI_METRICS_REPLACE;
	    }
	}
    }
    if (cr.metrics_type != FAPI_METRICS_REPLACE && bVertical) {
	double pwv[4];
	code = zchar_get_metrics2(pbfont, &char_name, pwv);
	if (code < 0)
	    return code;
	if (code == metricsNone) {
	    if (bCID) {
		cr.sb_x = fapi_round(sbw[2] / 2 * scale);
		cr.sb_y = fapi_round(pbfont->FontBBox.q.y * scale);
		cr.aw_y = fapi_round(- pbfont->FontBBox.q.x * scale); /* Sic ! */
		cr.metrics_scale = (bIsType1GlyphData ? 1000 : 1);
		cr.metrics_type = FAPI_METRICS_REPLACE;
		sbw[0] = sbw[2] / 2;
		sbw[1] = pbfont->FontBBox.q.y;
		sbw[2] = 0;
		sbw[3] = - pbfont->FontBBox.q.x; /* Sic ! */
		sbw_state = SBW_DONE;
	    } else
		bVertical = false;
	} else {
	    cr.sb_x = fapi_round(pwv[2] * scale);
	    cr.sb_y = fapi_round(pwv[3] * scale);
	    cr.aw_x = fapi_round(pwv[0] * scale);
	    cr.aw_y = fapi_round(pwv[1] * scale);
	    cr.metrics_scale = (bIsType1GlyphData ? 1000 : 1);
	    cr.metrics_type = (code == metricsSideBearingAndWidth ? 
				FAPI_METRICS_REPLACE : FAPI_METRICS_REPLACE_WIDTH);
	    sbw[0] = pwv[2];
	    sbw[1] = pwv[3];
	    sbw[2] = pwv[0];
	    sbw[3] = pwv[1];
	    sbw_state = SBW_DONE;
	}
    }
    if (cr.metrics_type == FAPI_METRICS_NOTDEF && !bVertical) {
	code = zchar_get_metrics(pbfont, &char_name, sbw);
	if (code < 0)
	    return code;
	if (code == metricsNone) {
	    sbw_state = SBW_FROM_RENDERER;
	    if (pbfont->FontType == 2) {
		gs_font_type1 *pfont1 = (gs_font_type1 *)pbfont;

		cr.aw_x = export_shift(pfont1->data.defaultWidthX, _fixed_shift - I->frac_shift);
		cr.metrics_scale = 1000;
		cr.metrics_type = FAPI_METRICS_ADD;
	    }
	} else {
	    cr.sb_x = fapi_round(sbw[2] * scale);
	    cr.sb_y = fapi_round(sbw[3] * scale);
	    cr.aw_x = fapi_round(sbw[0] * scale);
	    cr.aw_y = fapi_round(sbw[1] * scale);
	    cr.metrics_scale = (bIsType1GlyphData ? 1000 : 1);
	    cr.metrics_type = (code == metricsSideBearingAndWidth ? 
				FAPI_METRICS_REPLACE : FAPI_METRICS_REPLACE_WIDTH);
	    sbw_state = SBW_DONE;
	}
    }
    
    /* Take metrics from font : */
    if (SHOW_IS(penum, TEXT_DO_NONE)) {
	if ((code = renderer_retcode(i_ctx_p, I, I->get_char_width(I, &I->ff, &cr, &metrics))) < 0)
	    return code;
    } else if (igs->in_charpath || I->ff.is_outline_font) {
        if ((code = renderer_retcode(i_ctx_p, I, I->get_char_outline_metrics(I, &I->ff, &cr, &metrics))) < 0)
	    return code;
    } else {
#if 0 /* Debug purpose only. */
        code = e_limitcheck; 
#else
	code = I->get_char_raster_metrics(I, &I->ff, &cr, &metrics);
#endif
        if (code == e_limitcheck) {
            if(log2_scale.x > 0 || log2_scale.y > 0) {
                penum_s->fapi_log2_scale.x = log2_scale.x = penum_s->fapi_log2_scale.y = log2_scale.y = 0;
                I->release_char_data(I);
                goto retry_oversampling;
            }
            if ((code = renderer_retcode(i_ctx_p, I, I->get_char_outline_metrics(I, &I->ff, &cr, &metrics))) < 0)
		return code;
        } else {
            if ((code = renderer_retcode(i_ctx_p, I, code)) < 0)
		return code;
        }
    }
    compute_em_scale(pbfont, &metrics, FontMatrix_div, &em_scale_x, &em_scale_y);
    char_bbox.p.x = metrics.bbox_x0 / em_scale_x;
    char_bbox.p.y = metrics.bbox_y0 / em_scale_y;
    char_bbox.q.x = metrics.bbox_x1 / em_scale_x;
    char_bbox.q.y = metrics.bbox_y1 / em_scale_y;
    if (pbfont->PaintType != 0) {
	float w = pbfont->StrokeWidth / 2;

	char_bbox.p.x -= w;
	char_bbox.p.y -= w;
	char_bbox.q.x += w;
	char_bbox.q.y += w;
    }
    penum_s->fapi_glyph_shift.x = penum_s->fapi_glyph_shift.y = 0;
    if (sbw_state == SBW_FROM_RENDERER) {
	sbw[2] = metrics.escapement / em_scale_x;
	if (pbfont->FontType == 2) {
	    gs_font_type1 *pfont1 = (gs_font_type1 *)pbfont;

	    sbw[2] += fixed2float(pfont1->data.defaultWidthX);
	}
    } else if (sbw_state == SBW_SCALE) {
	sbw[0] = (double)cr.sb_x / scale / em_scale_x;
	sbw[1] = (double)cr.sb_y / scale / em_scale_y;
	sbw[2] = (double)cr.aw_x / scale / em_scale_x;
	sbw[3] = (double)cr.aw_y / scale / em_scale_y;
    }

    /* Setup cache and render : */
    if (cr.metrics_type == FAPI_METRICS_REPLACE) {
	/*
	 * Here we don't take care of replaced advance width 
	 * because gs_text_setcachedevice handles it.
	 */
	int can_replace_metrics;

        if ((code = renderer_retcode(i_ctx_p, I, I->can_replace_metrics(I, &I->ff, &cr, &can_replace_metrics))) < 0)
	    return code;
	if (!can_replace_metrics) {
	    /* 
	     * The renderer should replace the lsb, but it can't.
	     * To work around we compute a displacement in integral pixels
	     * and later shift the bitmap to it. The raster will be inprecise
	     * with non-integral pixels shift.
	     */
	    char_bbox.q.x -= char_bbox.p.x;
	    char_bbox.p.x = 0;
	    gs_distance_transform((metrics.bbox_x0 / em_scale_x - sbw[0]), 
				  0, ctm, &penum_s->fapi_glyph_shift);
	    penum_s->fapi_glyph_shift.x *= 1 << log2_scale.x;
	    penum_s->fapi_glyph_shift.y *= 1 << log2_scale.y;
	}
    }
    /*
     * We assume that if bMetricsFromGlyphDirectory is true,
     * the font does not specify Metrics[2] and/or CDevProc
     * If someday we meet a font contradicting this assumption,
     * zchar_set_cache to be improved with additional flag,
     * to ignore Metrics[2] and CDevProc.
     *
     * Note that for best quality the result of CDevProc
     * to be passed to I->get_char_raster_metrics, because
     * both raster and metrics depend on replaced lsb.
     * Perhaps in many cases the metrics from font is
     * used as an argument for CDevProc. Only way to resolve
     * is to call I->get_char_raster_metrics twice (before
     * and after CDevProc), or better to split it into
     * smaller functions. Unfortunately UFST cannot retrieve metrics 
     * quickly and separately from raster. Only way to resolve is 
     * to devide the replaced lsb into 2 parts, which correspond to
     * integral and fractinal pixels, then pass the fractional shift
     * to renderer and apply the integer shift after it.
     *
     * Besides that, we are not sure what to do if a font
     * contains both Metrics[2] and CDevProc. Should
     * CDevProc to be applied to Metrics[2] or to the metrics
     * from glyph code ? Currently we keep a compatibility
     * to the native GS font renderer without a deep analyzis.
     */
    code = zchar_set_cache(i_ctx_p, pbfont, &char_name,
		           NULL, sbw + 2, &char_bbox,
			   fapi_finish_render, &exec_cont, sbw);
    if (code >= 0 && exec_cont != 0)
	code = (*exec_cont)(i_ctx_p);
    if (code != 0) {
        if (code < 0) {
            /* An error */
            I->release_char_data(I);
        } else { 
            /* Callout to CDevProc, zsetcachedevice2, fapi_finish_render. */
        }
    }
    return code;
}

static int FAPI_char(i_ctx_t *i_ctx_p, bool bBuildGlyph, ref *charstring)
{   /* Stack : <font> <code|name> --> - */
    ref *v;
    char *font_file_path = NULL;
    gx_device *dev = gs_currentdevice_inline(igs);
    gs_font *pfont;
    int code = font_param(osp - 1, &pfont);

    if (code == 0) {
        gs_font_base *pbfont = (gs_font_base *) pfont;
        if (dict_find_string(osp - 1, "Path", &v) > 0 && r_has_type(v, t_string))
            font_file_path = ref_to_string(v, imemory, "font file path");
        code = FAPI_do_char(i_ctx_p, pbfont, dev, font_file_path, bBuildGlyph, charstring);
        if (font_file_path != NULL)
            gs_free_string(imemory, (byte *)font_file_path, r_size(v) + 1, "font file path");
    }
    return code;
}

static int FAPIBuildGlyph9aux(i_ctx_t *i_ctx_p)
{   os_ptr op = osp;                  /* <font0> <cid> <font9> <cid> */
    ref font9 = *pfont_dict(gs_currentfont(igs));
    ref *rFDArray, f;
    int font_index;
    int code;

    if ((code = ztype9mapcid(i_ctx_p)) < 0)
	return code;  /* <font0> <cid> <charstring> <font_index> */
    /* fixme: what happens if the charstring is absent ? 
       Can FDArray contain 'null' (see %Type9BuildGlyph in gs_cidfn.ps)? */
    font_index = op[0].value.intval;
    if (dict_find_string(&font9, "FDArray", &rFDArray) <= 0 || r_type(rFDArray) != t_array)
        return_error(e_invalidfont);
    if(array_get(imemory, rFDArray, font_index, &f) < 0 || r_type(&f) != t_dictionary)
        return_error(e_invalidfont);
    op[0] = op[-2];                   
    op[-2] = op[-1]; /* Keep the charstring on ostack for the garbager. */
    op[-1] = f;                       /* <font0> <charstring> <subfont> <cid> */
    if ((code = FAPI_char(i_ctx_p, true, op - 2)) < 0)
	return code;
                                      /* <font0> <charstring> */
    return 0;
}

/* <font> <code> .FAPIBuildChar - */
static int zFAPIBuildChar(i_ctx_t *i_ctx_p)
{   return FAPI_char(i_ctx_p, false, NULL);
}

/* non-CID : <font> <code> .FAPIBuildGlyph - */
/*     CID : <font> <name> .FAPIBuildGlyph - */
static int zFAPIBuildGlyph(i_ctx_t *i_ctx_p)
{   return FAPI_char(i_ctx_p, true, NULL);
}

/* <font> <cid> .FAPIBuildGlyph9 - */
static int zFAPIBuildGlyph9(i_ctx_t *i_ctx_p)
{   /*  The alghorithm is taken from %Type9BuildGlyph - see gs_cidfn.ps .  */
    os_ptr op = osp;
    int cid, code;
    avm_space s = ialloc_space(idmemory);

    check_type(op[ 0], t_integer);
    check_type(op[-1], t_dictionary);
    cid = op[0].value.intval;
    push(2);
    op[-1] = *pfont_dict(gs_currentfont(igs));
    op[0] = op[-2];                   /* <font0> <cid> <font9> <cid> */
    ialloc_set_space(idmemory, (r_is_local(op - 3) ? avm_global : avm_local)); /* for ztype9mapcid */
    code = FAPIBuildGlyph9aux(i_ctx_p);
    if (code != 0) {                  /* <font0> <dirty> <dirty> <dirty> */
        /* Adjust ostack for the correct error handling : */
        make_int(op - 2, cid);        
        pop(2);                       /* <font0> <cid> */
    } else {                          /* <font0> <dirty> */
        pop(2);                       /* */
        /*  Note that this releases the charstring, and it may be garbage-collected
            before the interpreter calls fapi_finish_render. This requires the server
            to keep glyph raster internally between calls to get_char_raster_metrics
            and get_char_raster. Perhaps UFST cannot provide metrics without 
            building a raster, so this constraint actually goes from UFST.
        */
    }
    ialloc_set_space(idmemory, s);
    return code;
}

static int do_FAPIpassfont(i_ctx_t *i_ctx_p, char *font_file_path, bool *success)
{   ref *pdr = osp;  /* font dict */
    gs_font *pfont;
    int code = font_param(osp, &pfont);
    gs_font_base *pbfont;
    int BBox[4];
    i_plugin_holder *h = i_plugin_get_list(i_ctx_p);
    char *xlatmap = NULL;
    FAPI_font_scale font_scale = {{1, 0, 0, 1, 0, 0}, {0, 0}, {1, 1}, true};
    const char *decodingID = NULL;


    if (code < 0)
	return code;
    code = FAPI_get_xlatmap(i_ctx_p, &xlatmap);	/* Useful for emulated fonts hooked with FAPI. */
    if (code < 0)
	return code;
    pbfont = (gs_font_base *)pfont;
    *success = false;
    for (; h != 0; h = h->next) {
	ref FAPI_ID;
        FAPI_server *I;
	const byte *server_param = NULL;
	int server_param_size = 0;

	if (strcmp(h->I->d->type, "FAPI"))
            continue;
        I = (FAPI_server *)h->I;
	get_server_param(i_ctx_p, I->ig.d->subtype, &server_param, &server_param_size);
        if ((code = renderer_retcode(i_ctx_p, I, I->ensure_open(I, server_param, server_param_size))) < 0)
	    return code;
	font_scale.HWResolution[0] = font_scale.HWResolution[1] = 72 << I->frac_shift;
	font_scale.matrix[0] = font_scale.matrix[3] = 1 << I->frac_shift;

	code = FAPI_prepare_font(i_ctx_p, I, pdr, pbfont, font_file_path, &font_scale, xlatmap, BBox, &decodingID);
	if (code < 0) {
            /* Failed, skip this renderer : */
	    continue;
	}
        pbfont->FAPI = I; /* We found a good renderer, so go with it */
        if ((code = name_ref(imemory, (const byte *)I->ig.d->subtype, strlen(I->ig.d->subtype), &FAPI_ID, false)) < 0)
	    return code;
	if ((code = dict_put_string(pdr, "FAPI", &FAPI_ID, NULL)) < 0)
	    return code; /* Insert FAPI entry to font dictionary. */
	*success = true;
	return 0;
    }
    /* Could not find renderer, return with false success. */
    return 0;
}

/* <font_dict> .FAPIpassfont bool <font_dict> */
/* must insert /FAPI to font dictionary */
/* This operator must not be called with devices which embed fonts. */
static int zFAPIpassfont(i_ctx_t *i_ctx_p)
{   os_ptr op = osp;
    int code;
    bool found = false;
    char *font_file_path = NULL;
    ref *v;

    /* Normally embedded fonts have no Path, but if a CID font is
     * emulated with a TT font, and it is hooked with FAPI,
     * the path presents and is neccessary to access the full font data.
     */
    check_type(*op, t_dictionary);
    if (dict_find_string(op, "Path", &v) > 0 && r_has_type(v, t_string))
        font_file_path = ref_to_string(v, imemory_global, "font file path");
    code = do_FAPIpassfont(i_ctx_p, font_file_path, &found);
    if (font_file_path != NULL)
        gs_free_string(imemory_global, (byte *)font_file_path, r_size(v) + 1, "font file path");
    if(code != 0)
	return code;
    push(1);
    make_bool(op, found);
    return 0;
}

const op_def zfapi_op_defs[] =
{   {"2.FAPIavailable",   zFAPIavailable},
    {"2.FAPIpassfont",    zFAPIpassfont},
    {"2.FAPIrebuildfont", zFAPIrebuildfont},
    {"2.FAPIBuildChar",   zFAPIBuildChar},
    {"2.FAPIBuildGlyph",  zFAPIBuildGlyph},
    {"2.FAPIBuildGlyph9", zFAPIBuildGlyph9},
    op_def_end(0)
};

