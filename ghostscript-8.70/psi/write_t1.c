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

/* $Id: write_t1.c 9742 2009-05-15 12:13:25Z ken $ */

/*
Functions to serialize a type 1 font as PostScript code that can then be
passed to FreeType via the FAPI FreeType bridge.
Started by Graham Asher, 26th July 2002.
*/

#include "wrfont.h"
#include "write_t1.h"

/*
Public structures and functions in this file are prefixed with FF_ because they are part of
the FAPI FreeType implementation.
*/

static void write_word_entry(FAPI_font* a_fapi_font,WRF_output* a_output,const char* a_name,int a_index,int a_divisor)
	{
	short x;
	WRF_wbyte(a_output,'/');
	WRF_wstring(a_output,a_name);
	WRF_wbyte(a_output,' ');
	/* Get the value and convert it from unsigned to signed by assigning it to a short. */
	x = a_fapi_font->get_word(a_fapi_font,a_index,0);
	/* Divide by the divisor to bring it back to font units. */
	x = (short)(x / a_divisor);
	WRF_wint(a_output,x);
	WRF_wstring(a_output," def\n");
	}

static void write_array_entry_with_count(FAPI_font* a_fapi_font,WRF_output* a_output,const char* a_name,int a_index,int a_count,int a_divisor)
	{
	int i;

	if (a_count <= 0)
		return;

	WRF_wbyte(a_output,'/');
	WRF_wstring(a_output,a_name);
	WRF_wstring(a_output," [");
	for (i = 0; i < a_count; i++)
		{
		/* Get the value and convert it from unsigned to signed by assigning it to a short. */
		short x = a_fapi_font->get_word(a_fapi_font,a_index,i);
		/* Divide by the divisor to bring it back to font units. */
		x = (short)(x / a_divisor);
		WRF_wint(a_output,x);
		WRF_wbyte(a_output,(byte)(i == a_count - 1 ? ']' : ' '));
		}
	WRF_wstring(a_output," def\n");
	}


static void write_array_entry(FAPI_font* a_fapi_font,WRF_output* a_output,const char* a_name,int a_index,int a_divisor)
	{
	/* NOTE that the feature index must be preceded by the count index for this to work. */
	int count = a_fapi_font->get_word(a_fapi_font,a_index - 1,0);
	write_array_entry_with_count(a_fapi_font,a_output,a_name,a_index,count,a_divisor);
	}

static void write_subrs(FAPI_font* a_fapi_font,WRF_output* a_output)
	{
	int i;
	int count = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_Subrs_count,0);
	if (count <= 0)
		return;

	WRF_wstring(a_output,"/Subrs ");
	WRF_wint(a_output,count);
	WRF_wstring(a_output," array\n");

	for (i = 0; i < count; i++)
		{
		long length = a_fapi_font->get_subr(a_fapi_font,i,0,0);
		long buffer_size;
		WRF_wstring(a_output,"dup ");
		WRF_wint(a_output,i);
		WRF_wbyte(a_output,' ');
		WRF_wint(a_output,length);
		WRF_wstring(a_output," RD ");
		
		/* Get the subroutine into the buffer and encrypt it in place. */
		buffer_size = a_output->m_limit - a_output->m_count;
		if (buffer_size >= length)
			{
			a_fapi_font->get_subr(a_fapi_font,i,a_output->m_pos,(ushort)length);
			WRF_wtext(a_output,a_output->m_pos,length);
			}
		else
			a_output->m_count += length;

		WRF_wstring(a_output," NP\n");
		}

	WRF_wstring(a_output,"ND\n");
	}

static int is_MM_font(FAPI_font *a_fapi_font)
{
    return a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_DollarBlend,0);
}

static void write_private_dictionary(FAPI_font* a_fapi_font,WRF_output* a_output)
	{
	a_output->m_encrypt = true;

	/* Write 4 bytes that must encrypt to at least one character that cannot be a valid hexadecimal character. */
	WRF_wstring(a_output,"XXXX");

	/*+ to do: correct size of dictionary from 8. */	
	WRF_wstring(a_output,"dup /Private 8 dict dup begin\n");

	WRF_wstring(a_output,"/MinFeature {16 16} def\n");
	WRF_wstring(a_output,"/password 5839 def\n");
	WRF_wstring(a_output,"/lenIV -1 def\n"); /* indicate that /subrs are not encoded. */
	write_word_entry(a_fapi_font,a_output,"BlueFuzz",FAPI_FONT_FEATURE_BlueFuzz,16);

	WRF_wstring(a_output,"/BlueScale ");
	WRF_wfloat(a_output,a_fapi_font->get_long(a_fapi_font,FAPI_FONT_FEATURE_BlueScale,0) / 65536.0);
	WRF_wstring(a_output," def\n");

	write_word_entry(a_fapi_font,a_output,"BlueShift",FAPI_FONT_FEATURE_BlueShift,16);
	write_array_entry(a_fapi_font,a_output,"BlueValues",FAPI_FONT_FEATURE_BlueValues,16);
	write_array_entry(a_fapi_font,a_output,"OtherBlues",FAPI_FONT_FEATURE_OtherBlues,16);
	write_array_entry(a_fapi_font,a_output,"FamilyBlues",FAPI_FONT_FEATURE_FamilyBlues,16);
	write_array_entry(a_fapi_font,a_output,"FamilyOtherBlues",FAPI_FONT_FEATURE_FamilyOtherBlues,16);
	write_word_entry(a_fapi_font,a_output,"ForceBold",FAPI_FONT_FEATURE_ForceBold,1);
	write_array_entry_with_count(a_fapi_font,a_output,"StdHW",FAPI_FONT_FEATURE_StdHW,1,16);
	write_array_entry_with_count(a_fapi_font,a_output,"StdVW",FAPI_FONT_FEATURE_StdVW,1,16);
	write_array_entry(a_fapi_font,a_output,"StemSnapH",FAPI_FONT_FEATURE_StemSnapH,16);
	write_array_entry(a_fapi_font,a_output,"StemSnapV",FAPI_FONT_FEATURE_StemSnapV,16);

	if (is_MM_font(a_fapi_font)) {
	    WRF_wstring(a_output,"3 index /Blend get /Private get begin\n");
	    WRF_wstring(a_output,"|-\n");
	}
	write_subrs(a_fapi_font,a_output);
	}

static void write_blend_dictionary(FAPI_font* a_fapi_font,WRF_output* a_output)
{
}

static void write_main_dictionary(FAPI_font* a_fapi_font,WRF_output* a_output)
	{
	int i;
	WRF_wstring(a_output,"5 dict begin\n");

    WRF_wstring(a_output,"/FontType 1 def\n");

	WRF_wstring(a_output,"/FontMatrix [");
	for (i = 0; i < 6; i++)
		{
		WRF_wfloat(a_output,a_fapi_font->get_float(a_fapi_font,FAPI_FONT_FEATURE_FontMatrix,i));
		WRF_wbyte(a_output,(byte)(i == 5 ? ']' : ' '));
		}
	WRF_wbyte(a_output,'\n');

	/* For now, specify standard encoding - I think GS will pass glyph indices so doesn't matter. */
	WRF_wstring(a_output,"/Encoding StandardEncoding def\n");

	WRF_wstring(a_output,"/FontBBox {");
	for (i = 0; i < 4; i++)
		{
		short x = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_FontBBox,i);
		WRF_wint(a_output,x);
		WRF_wbyte(a_output,(byte)(i == 3 ? '}' : ' '));
		}
	WRF_wbyte(a_output,'\n');
	if (is_MM_font(a_fapi_font)) {
	    short x,x2;
	    float x1;
	    uint i, j, entries;
	    char Buffer[255];

	    entries = 0;
    	    x = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_BlendAxisTypes_count,0);
	    if (x)
		entries++;
	    x = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_BlendDesignPositionsArrays_count,0);
	    if (x)
		entries++;
	    x = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_BlendDesignMapArrays_count,0);
	    if (x)
		entries++;

	    sprintf(Buffer, "/FontInfo %d dict dup begin\n", entries);
	    WRF_wstring(a_output, Buffer);
	    x = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_BlendAxisTypes_count,0);
	    if (x) {
		WRF_wstring(a_output, "/BlendAxisTypes [");
		for (i = 0;i < x;i++) {
		    WRF_wstring(a_output," /");
		    a_fapi_font->get_name(a_fapi_font,FAPI_FONT_FEATURE_BlendAxisTypes,i, (char *)&Buffer,255);
		    WRF_wstring(a_output,Buffer);
		}
		WRF_wstring(a_output,"] def\n");
	    }
	    x = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_BlendDesignPositionsArrays_count,0);
	    if (x) {
		WRF_wstring(a_output, "/BlendDesignPositions [");
		x2 = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_BlendAxisTypes_count,0);
		for (i = 0;i < x; i++) {
		    WRF_wstring(a_output,"[");
		    for (j = 0; j < x2; j++) {
			x1 = a_fapi_font->get_float(a_fapi_font,FAPI_FONT_FEATURE_BlendDesignPositionsArrayValue,i*8+j);
			sprintf(Buffer, "%f ", x1);
			WRF_wstring(a_output,Buffer);
		    }
		    WRF_wstring(a_output,"]");
		}
	    WRF_wstring(a_output, "] def\n");
	    }
	    x = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_BlendDesignMapArrays_count,0);
	    if (x) {
		WRF_wstring(a_output, "/BlendDesignMap [");
		for (i = 0;i < x;i++) {
		    x2 = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_BlendDesignMapSubArrays_count,i);
		    WRF_wstring(a_output,"[");
		    for (j = 0; j < x2; j++) {
			WRF_wstring(a_output,"[");
			x1 = a_fapi_font->get_float(a_fapi_font,FAPI_FONT_FEATURE_BlendDesignPositionsArrayValue,i*64+j*64);
			sprintf(Buffer, "%f ", x1);
			WRF_wstring(a_output,Buffer);
			x1 = a_fapi_font->get_float(a_fapi_font,FAPI_FONT_FEATURE_BlendDesignPositionsArrayValue,i*64+j*64 + 1);
			sprintf(Buffer, "%f ", x1);
			WRF_wstring(a_output,Buffer);
			WRF_wstring(a_output,"]");
		    }
		    WRF_wstring(a_output,"]");
		}
		WRF_wstring(a_output, "] def\n");
	    }
	    WRF_wstring(a_output,"end readonly def\n");
	    WRF_wstring(a_output,"/$Blend {");
	    x = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_DollarBlend_length,0);
	    if(a_output->m_count)
		a_output->m_count += x;
	    x = a_fapi_font->get_proc(a_fapi_font,FAPI_FONT_FEATURE_DollarBlend,0,(char *)a_output->m_pos);
	    if(a_output->m_pos)
		a_output->m_pos += x;
	    WRF_wstring(a_output,"} def\n");
	    WRF_wstring(a_output,"/$Blend {0.1 mul exch 0.45 mul add exch 0.17 mul add add} def\n");
	    WRF_wstring(a_output,"/WeightVector [");
	    x = a_fapi_font->get_word(a_fapi_font,FAPI_FONT_FEATURE_WeightVector_count,0);
	    for (i = 0;i < x;i++) {
		x1 = a_fapi_font->get_float(a_fapi_font,FAPI_FONT_FEATURE_WeightVector,i);
		sprintf(Buffer, "%f ", x1);
		WRF_wstring(a_output,Buffer);
	    }
	    WRF_wstring(a_output,"] def\n");
	}
	WRF_wstring(a_output,"currentdict end\ncurrentfile eexec\n");
	write_private_dictionary(a_fapi_font,a_output);
	if (is_MM_font(a_fapi_font)) {
	    write_blend_dictionary(a_fapi_font, a_output);
	}
	}
	
/**
Write a Type 1 font in textual format and return its length in bytes.
If a_buffer_size is less than the total length, only a_buffer_size bytes are written, but the total
length is returned correctly.

The PostScript is non-standard. The main dictionary contains no /Charstrings dictionary. This
is supplied to FreeType using the incremental interface, There is also no /PaintType entry. This is required
by PostScript but FreeType doesn't use it.
*/
long FF_serialize_type1_font(FAPI_font* a_fapi_font,unsigned char* a_buffer,long a_buffer_size)
	{
	WRF_output output;
	WRF_init(&output,a_buffer,a_buffer_size);
	
	/* Leading comment identifying a Type 1 font. */
	WRF_wstring(&output,"%!PS-AdobeFont-1\n");

	write_main_dictionary(a_fapi_font,&output);
	return output.m_count;
	}
