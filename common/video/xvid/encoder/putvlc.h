/***********************************************************HeaderBegin*******

 *                                                                         

 * File:	putvlc.h

 *

 * Description: Header file to include prototypes for vlc functions

 *

 ***********************************************************HeaderEnd*********/



#ifndef _ENCORE_PUTVLC_H

#define _ENCORE_PUTVLC_H

#include "bitwriter.h"





// stuff from max_level.h



static int intra_max_level[2][64] = {

    {

     27, 10, 5, 4, 3, 3, 3, 3,

     2, 2, 1, 1, 1, 1, 1, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     },



    {

     8, 3, 2, 2, 2, 2, 2, 1,

     1, 1, 1, 1, 1, 1, 1, 1,

     1, 1, 1, 1, 1, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0}

};





static int inter_max_level[2][64] = {

    {

     12, 6, 4, 3, 3, 3, 3, 2,

     2, 2, 2, 1, 1, 1, 1, 1,

     1, 1, 1, 1, 1, 1, 1, 1,

     1, 1, 1, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0},



    {

     3, 2, 1, 1, 1, 1, 1, 1,

     1, 1, 1, 1, 1, 1, 1, 1,

     1, 1, 1, 1, 1, 1, 1, 1,

     1, 1, 1, 1, 1, 1, 1, 1,

     1, 1, 1, 1, 1, 1, 1, 1,

     1, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0,

     0, 0, 0, 0, 0, 0, 0, 0}

};





static int intra_max_run0[28] = {

    999, 14, 9, 7, 3, 2, 1,

    1, 1, 1, 1, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0

};





static int intra_max_run1[9] = {

    999, 20, 6,

    1, 0, 0,

    0, 0, 0

};





static int inter_max_run0[13] = {

    999,

    26, 10, 6, 2, 1, 1,

    0, 0, 0, 0, 0, 0

};





static int inter_max_run1[4] = {

    999, 40, 1, 0

};







#define MARKER_BIT 1

/**

    Public interface of VLC coding module

    Methods defined in putvlc.c

    Referenced from block.c

**/

int PutCoeff(Bitstream * bitstream, int run, int level, int last, int Mode);

int PutIntraDC(Bitstream * bitstream, int val, bool lum);

int PutMV(Bitstream * bitstream, int mvint);

int PutMCBPC_intra(Bitstream * bitstream, int cbpc, int mode);

int PutMCBPC_inter(Bitstream * bitstream, int cbpc, int mode);

int PutCBPY(Bitstream * bitstream, int cbpy, bool intra);



/**

    Internal vlc methods, defined in putvlc.c 

**/



int PutDCsize_lum(Bitstream * bitstream, int size);

int PutDCsize_chrom(Bitstream * bitstream, int size);

int PutCoeff_inter(Bitstream * bitstream, int run, int level, int last);

int PutCoeff_intra(Bitstream * bitstream, int run, int level, int last);

int PutRunCoeff_inter(Bitstream * bitstream, int run, int level, int last);

int PutRunCoeff_intra(Bitstream * bitstream, int run, int level, int last);

int PutLevelCoeff_inter(Bitstream * bitstream, int run, int level, int last);

int PutLevelCoeff_intra(Bitstream * bitstream, int run, int level, int last);



/** Currently unused **/

int PutMCBPC_Sprite(Bitstream * bitstream, int cbpc, int mode);

int PutCoeff_inter_RVLC(Bitstream * bitstream, int run, int level, int last);

int PutCoeff_intra_RVLC(Bitstream * bitstream, int run, int level, int last);



#endif
