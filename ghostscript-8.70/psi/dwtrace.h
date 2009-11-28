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

/* $Id: dwtrace.h 9043 2008-08-28 22:48:19Z giles $ */
/* The interface of Graphical trace server for Windows */

#ifndef dwtrace_INCLUDED
#  define dwtrace_INCLUDED

extern struct vd_trace_interface_s visual_tracer;
struct vd_trace_interface_s *visual_tracer_init(void);
void visual_tracer_close(void);

#endif /* dwtrace_INCLUDED */