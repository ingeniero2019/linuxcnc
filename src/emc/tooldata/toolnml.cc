/*
** Copyright: 2020
** Author:    Dewey Garrett <dgarrett@panix.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "tooldata.hh"

static CANON_TOOL_TABLE *thetable;

int tool_nml_register(CANON_TOOL_TABLE *basetable)
{
   thetable = basetable;
   return 0;
} //tool_nml_register

void tool_tbl_last_index_set(int idx)  //force last_index
{
    return;  //noop for nml
} //tool_tbl_last_index_set()

int tool_tbl_last_index_get(void)
{
    return CANON_POCKETS_MAX -1;
} // tool_tbl_last_index_get()


int tool_tbl_put(CANON_TOOL_TABLE from,int idx)
{
    if (!thetable) {fprintf(stderr,"tool_nml_put NOT INITIALIZED\n");}
    if (idx < 0 || idx >= CANON_POCKETS_MAX) {
        fprintf(stderr,"tool_tbl_put() bad idx = %d\n",idx);
    }
    *(thetable + idx) = from;

    return 0;
} // tool_tbl_put()

struct CANON_TOOL_TABLE tool_tbl_get(int idx)
{
    if (idx < 0 || idx >= CANON_POCKETS_MAX) {
        fprintf(stderr,"tool_tbl_get() bad idx = %d\n",idx);
    }

if (!thetable) {
   fprintf(stderr,"%8d PROBLEM GET idx=%d thetable=%p\n",getpid(),idx,thetable);
  return (*(struct CANON_TOOL_TABLE*)(NULL));
}
    
    return  *(struct CANON_TOOL_TABLE*)(thetable + idx);
} // tool_tbl_get()
