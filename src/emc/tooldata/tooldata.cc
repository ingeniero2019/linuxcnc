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
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "rtapi_mutex.h"
#include "tooldata.hh"

#define TOOL_MMAP_FILENAME "/tmp/tool.mmap"
#define TOOL_MMAP_MODE     0600
#define TOOL_MMAP_CREATOR_OPEN_FLAGS  O_RDWR | O_CREAT | O_TRUNC
#define TOOL_MMAP_USER_OPEN_FLAGS     O_RDWR

static int           creator_fd;
static char*         tool_mmap_base = 0;
static unsigned int  x_last_index; // for typeof() assignments only
static EMC_TOOL_STAT *toolstat; //used for _creator

/* mmap region:
**   1) the_mutex   (typeof(rtapi_mutex_t))
**   2) last_index  (typeof(x_last_index) )
**   3) CANON_TOOL_TABLE items (howmany=CANON_POCKETS_MAX)
**
** Note: the CANON_TOOL_TABLE items are stored sequentialy
**       (structs not used in the mmapped region)
*/

#define TOOL_MMAP_MUTEX_OFFSET      0
#define TOOL_MMAP_LAST_INDEX_OFFSET ( sizeof(rtapi_mutex_t) )

#define TOOL_MMAP_MUTEX      ( tool_mmap_base \
                             + TOOL_MMAP_MUTEX_OFFSET \
                             )

#define TOOL_MMAP_LAST_INDEX ( tool_mmap_base \
                             + TOOL_MMAP_LAST_INDEX_OFFSET \
                             )
//---------------------------------------------------------------------
#define TOOL_MMAP_HEADER_SIZE ( sizeof(rtapi_mutex_t) \
                              + sizeof(x_last_index) \
                              )
#define TOOL_MMAP_SIZE TOOL_MMAP_HEADER_SIZE + \
                       CANON_POCKETS_MAX * sizeof(struct CANON_TOOL_TABLE)

#define TOOL_MMAP_POSE_SIZE ( sizeof(EmcPose::tran.x) \
                            + sizeof(EmcPose::tran.y) \
                            + sizeof(EmcPose::tran.z) \
                            + sizeof(EmcPose::a) \
                            + sizeof(EmcPose::b) \
                            + sizeof(EmcPose::c) \
                            + sizeof(EmcPose::u) \
                            + sizeof(EmcPose::v) \
                            + sizeof(EmcPose::w) \
                            )

#define TOOL_MMAP_STRIDE    ( sizeof(CANON_TOOL_TABLE::toolno) \
                            + sizeof(CANON_TOOL_TABLE::pocketno) \
                            + sizeof(CANON_TOOL_TABLE::diameter) \
                            + sizeof(CANON_TOOL_TABLE::frontangle) \
                            + sizeof(CANON_TOOL_TABLE::backangle) \
                            + sizeof(CANON_TOOL_TABLE::orientation) \
                            + TOOL_MMAP_POSE_SIZE \
                            )
//---------------------------------------------------------------------

static int tool_tbl_mutex_get() {
    rtapi_mutex_t the_mutex = *(rtapi_mutex_t*)(TOOL_MMAP_MUTEX);
    useconds_t waited_us  =    0;
    useconds_t delta_us   =   10;
    useconds_t maxwait_us = 1000;
    while ( rtapi_mutex_try(&the_mutex) ) { //true==failed
        usleep(delta_us); waited_us += delta_us;
        fprintf(stderr,"!!!%8d UNEXPECTED tool_tbl_mutex_get(): waited_us=%d\n"
               ,getpid(),waited_us);
        if (waited_us > maxwait_us) break;
    }
    if (waited_us > maxwait_us) {
        fprintf(stderr,"\n!!! %8d UNEXPECTED tool_tbl_mutex_get(): FAIL\n",getpid());
        fprintf(stderr,"waited_us=%d delta_us=%d maxwait_us=%d\n\n",
                waited_us,delta_us,maxwait_us);
        rtapi_mutex_give(&the_mutex); // continue without mutex
        return -1;
    }
    return 0;
} // tool_tbl_mutex_get()

static void tool_mmap_mutex_give()
{
    rtapi_mutex_t the_mutex = *(rtapi_mutex_t*)(TOOL_MMAP_MUTEX);
    rtapi_mutex_give(&the_mutex);
} // tool_mmap_mutex_give()

//typ creator: emc/ioControl.cc, sai/driver.cc
//    (first applicable process started in linuxcnc script)
int tool_mmap_creator(EMC_TOOL_STAT * ptr)
{
    static int inited=0;

    if (inited) {
        fprintf(stderr,"Error: tool_mmap_creator already called BYE\n");
        exit(EXIT_FAILURE);
    }

    toolstat = ptr; //note NULL for sai

    creator_fd = open(TOOL_MMAP_FILENAME,
                     TOOL_MMAP_CREATOR_OPEN_FLAGS,TOOL_MMAP_MODE);
    if (!creator_fd) {
        perror("tool_mmap_creator(): file open fail");
        exit(EXIT_FAILURE);
    }
    if (lseek(creator_fd, TOOL_MMAP_SIZE, SEEK_SET) == -1) {
        close(creator_fd);
        perror("tool_mmap_creator() lseek fail");
        exit(EXIT_FAILURE);
    }
    if (write(creator_fd, "\0", 1) < 0) {
        close(creator_fd);
        perror("tool_mmap_creator(): file tail write fail");
        exit(EXIT_FAILURE);
    }
    tool_mmap_base = (char*)mmap(0, TOOL_MMAP_SIZE, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, creator_fd, 0);
    if (tool_mmap_base == MAP_FAILED) {
        close(creator_fd);
        perror("tool_mmap_creator(): mmap fail");
        exit(EXIT_FAILURE);
    }
    inited = 1;
    rtapi_mutex_t the_mutex = *(rtapi_mutex_t*)(TOOL_MMAP_MUTEX);
    rtapi_mutex_give(&the_mutex);

    return 0;
} // tool_mmap_creator();

//typ: milltask, guis (emcmodule,emcsh,...), halui
int tool_mmap_user()
{
    int fd = open(TOOL_MMAP_FILENAME,
                  TOOL_MMAP_USER_OPEN_FLAGS, TOOL_MMAP_MODE);

    if (fd == -1) {
        perror("tool_mmap_user(): file open fail");
        exit(EXIT_FAILURE);
    }
    tool_mmap_base = (char*)mmap(0, TOOL_MMAP_SIZE, PROT_READ|PROT_WRITE,
                                 MAP_SHARED, fd, 0);

    if (tool_mmap_base == MAP_FAILED) {
        close(fd);
        perror("tool_mmap_user(): mmap fail");
        exit(EXIT_FAILURE);
    }
    return 0;
} //tool_mmap_user()

void tool_mmap_close()
{
    // mapped file is not deleted
    // flush mmapped file to filesystem
    if (msync(tool_mmap_base, TOOL_MMAP_SIZE, MS_SYNC) == -1) {
        perror("tool_mmap_close(): msync fail");
    }
    if (munmap(tool_mmap_base, TOOL_MMAP_SIZE) < 0) {
        close(creator_fd);
        perror("tool_mmap_close(): munmapfail");
        exit(EXIT_FAILURE);
    }
    close(creator_fd);
} //tool_mmap_close()

void tool_tbl_last_index_set(int idx)  //force last_index
{
    *(typeof(x_last_index)*)(TOOL_MMAP_LAST_INDEX) = idx;
} //tool_tbl_last_index_set()

int tool_tbl_last_index_get(void)
{
    if (tool_mmap_base) {
        return *(typeof(x_last_index)*)(TOOL_MMAP_LAST_INDEX);
    } else {
        return -1;
    }
} // tool_tbl_last_index_get()

int tool_tbl_put(struct CANON_TOOL_TABLE t,int idx)
{
#define PUT(ptr,T,item) *(typeof(T.item)*)(ptr) = T.item; \
                                           ptr += sizeof(T.item)
    // Example: item==toolno (int type)
    //          *(int*)(ptr) = T.toolno; ptr += sizeof(int)

    if (!tool_mmap_base) {
        fprintf(stderr,"%8d tool_tbl_base not mapped BYE\n",getpid());
        exit(EXIT_FAILURE);
    }

#if 0 //{
    // for debugging, breaks runtests
    if (idx <4) {
        fprintf(stderr,"___put %3d mutex=%ld\n",
               idx,tool_mmap_mutex);
    }
#endif //}

    if (idx >= CANON_POCKETS_MAX) {
       fprintf(stderr,"UNEXPECTED: tool_tbl_put(): idx > maxallowed <%d,%d> %s\n",
              idx,CANON_POCKETS_MAX-1,__FILE__);
    }

    if (tool_tbl_mutex_get()) {
        fprintf(stderr,"!!!%8d UNEXPECTED tool_tbl_mutex_get() fail\n",getpid());
        fprintf(stderr,"!!!continuing without mutex\n");
    }

    int last_index = *(typeof(x_last_index)*)(TOOL_MMAP_LAST_INDEX);
    if (idx > last_index) {  // extend know indices
        *(typeof(x_last_index)*)(TOOL_MMAP_LAST_INDEX) = idx;
    }

    char* p = tool_mmap_base + TOOL_MMAP_HEADER_SIZE + idx*TOOL_MMAP_STRIDE;
    PUT(p, t, toolno);
    PUT(p, t, pocketno);
    PUT(p, t, diameter);
    PUT(p, t, frontangle);
    PUT(p, t, backangle);
    PUT(p, t, orientation);
    PUT(p, t, offset.tran.x);
    PUT(p, t, offset.tran.y);
    PUT(p, t, offset.tran.z);
    PUT(p, t, offset.a);
    PUT(p, t, offset.b);
    PUT(p, t, offset.c);
    PUT(p, t, offset.u);
    PUT(p, t, offset.v);
    PUT(p, t, offset.w);

    if (toolstat && p == 0) { //note sai does not use
       *(struct CANON_TOOL_TABLE*)(&toolstat->toolTableCurrent) = t;
    }
    tool_mmap_mutex_give();
    return 0;
#undef PUT
} // tool_tbl_put()

struct CANON_TOOL_TABLE tool_tbl_get(int idx)
{
#define GET(ptr,T,item) T.item  = *(typeof(T.item)*)(ptr); \
                           ptr += sizeof(T.item)

    // Example: item==diameter (double type)
    //          T.diameter = *(double*)(ptr); ptr += sizeof(double)

    struct CANON_TOOL_TABLE ret_table;

    if (!tool_mmap_base) {
        fprintf(stderr,"tool_tbl_get() not mmapped BYE %s\n", __FILE__);
        exit(EXIT_FAILURE);
    }
    if (idx >= CANON_POCKETS_MAX) {
       fprintf(stderr,"PROBLEM tool_tbl_get(): idx > maxallowed <%d,%d>\n",
              idx,CANON_POCKETS_MAX-1);
    }

#if 0 //{
    // for debugging (should only occur when initializing)
    if (idx > last_index ) {
       //print first 10 only
       if (idx < 10 ) {
           fprintf(stderr,"UNEXPECTED: tool_tbl_get() idx=%d>last_index=%d\n",
                  idx,last_index);
       }
    }
#endif //}

    if (tool_tbl_mutex_get()) {
        fprintf(stderr,"!!!%8d UNEXPECTED tool_tbl_mutex_get() fail\n",getpid());
        fprintf(stderr,"!!!continuing without mutex\n");
    }

    char* p = tool_mmap_base + TOOL_MMAP_HEADER_SIZE + idx*TOOL_MMAP_STRIDE;
    GET(p, ret_table, toolno);
    GET(p, ret_table, pocketno);
    GET(p, ret_table, diameter);
    GET(p, ret_table, frontangle);
    GET(p, ret_table, backangle);
    GET(p, ret_table, orientation);
    GET(p, ret_table, offset.tran.x);
    GET(p, ret_table, offset.tran.y);
    GET(p, ret_table, offset.tran.z);
    GET(p, ret_table, offset.a);
    GET(p, ret_table, offset.b);
    GET(p, ret_table, offset.c);
    GET(p, ret_table, offset.u);
    GET(p, ret_table, offset.v);
    GET(p, ret_table, offset.w);

    tool_mmap_mutex_give();
    return ret_table;
#undef GET
} // tool_tbl_get()
