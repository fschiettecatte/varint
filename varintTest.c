/*****************************************************************************
*       Copyright (C) 2009-2014, FS Consulting Inc. All rights reserved		 *
*																			 *
*  This notice is intended as a precaution against inadvertent publication	 *
*  and does not constitute an admission or acknowledgement that publication	 *
*  has occurred or constitute a waiver of confidentiality.					 *
*																			 *
*  This software is the proprietary and confidential property				 *
*  of FS Consulting, Inc.													 *
*****************************************************************************/


/*{

	Module:	varintTest.c

	Author:	Francois Schiettecatte (fschiettecatte@gmail.com)

	Creation Date:	January 7th, 2009

	Purpose:	Test integer encoding schemes
	
	Compile and run as follows:
	
		gcc -g -o varintTest varintTest.c && ./varintTest

		gcc -O3 -o varintTest varintTest.c && ./varintTest

}*/


/*---------------------------------------------------------------------------*/

/*
** C includes (POSIX)
*/

/* Enable gnu source */
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif	/* !defined(_GNU_SOURCE) */

/* Enable reentrant functions */
#if !defined(_REENTRANT)
#define _REENTRANT
#endif	/* !defined(_REENTRANT) */

/* Enable large file functions */
#if !defined(_LARGEFILE_SOURCE)
#define _LARGEFILE_SOURCE
#endif	/* !defined(_LARGEFILE_SOURCE) */

/* Set large file bits */
#if !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS		(64)
#endif	/* !defined(_FILE_OFFSET_BITS) */

/* #include <assert.h> */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <semaphore.h>
#include <pthread.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <grp.h>
#include <limits.h>
#include <math.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <locale.h>
#include <iconv.h>
#include <wchar.h>
#include <wctype.h>


/*---------------------------------------------------------------------------*/

/*
** Define the kind of C compiler we are using
**
*/

/*
** Check for the STDC define although the ANSI
** standard wants us to look for __STDC__
*/
#if defined(STDC)
#if !defined(__STDC__)
#define __STDC__
#endif	/* !defined(__STDC__) */
#endif	/* defined(STDC) */

/* Check for the __GNUC__ define */
#if defined(__GNUC__)
#if !defined(__STDC__)
#define __STDC__
#endif	/* !defined(__STDC__) */
#endif	/* defined(__GNUC__) */


/*---------------------------------------------------------------------------*/

/*
** Check for various compiler defined symbols
*/

#if !defined(__FILE__)
#define __FILE__		"Undefined __FILE__"
#endif	/* defined(__FILE__) */

#if !defined(__LINE__)
#define __LINE__		(-1)
#endif	/* defined(__LINE__) */


/*---------------------------------------------------------------------------*/

/*
** Additional types
*/

#if !defined(boolean)
typedef unsigned short		boolean;
#endif	/* !defined(boolean) */ 


/*---------------------------------------------------------------------------*/

/*
** Additional contants
*/

#if !defined(NULL)
#define NULL				(0)
#endif	/* !defined(NULL) */


#if !defined(true)
#define true 				(1)
#endif	/* !defined(true) */

#if !defined(false)
#define false 				(0)
#endif	/* !defined(false) */


/*---------------------------------------------------------------------------*/


/* #define DEBUG	(1) */

/*
** Assertion macro, use this to wrap around a condition that must 
** always be true to test that it is true. Compiles out when DEBUG
** is not defined.
*/
#if defined(DEBUG)
/* #define ASSERT(f)	 { if (!(f)) { printf("Assertion failed, file: '%s', line: %d\n", __FILE__, __LINE__); fflush(NULL); abort(); } } */
#define ASSERT(f)	 {}
#else
#define ASSERT(f)	 {}
#endif	/* defined(DEBUG) */


/*---------------------------------------------------------------------------*/


/*
** ========================================== 
** === Number storage macros (compressed) ===
** ==========================================
*/


/* Masks for compressed numbers storage */
#define NUM_COMPRESSED_CONTINUE_BIT				(0x80)		/* 1000 0000 */
#define NUM_COMPRESSED_DATA_MASK				(0x7F)		/* 0111 1111 */
#define NUM_COMPRESSED_DATA_BITS				(7)



/* Macro to get the number of bytes occupied by a compressed 32 bit integer.
** The integer to evaluate should be set in uiMacroValue and the number of 
** bytes is placed in uiMacroSize
*/
#define NUM_GET_COMPRESSED_UINT_SIZE(uiMacroValue, uiMacroSize) \
	{	\
		if ( uiMacroValue < (1UL << 7) )	\
			uiMacroSize = 1;	\
		else if ( uiMacroValue < (1UL << 14) )	\
			uiMacroSize = 2;	\
		else if ( uiMacroValue < (1UL << 21) )	\
			uiMacroSize = 3;	\
		else if ( uiMacroValue < (1UL << 28) )	\
			uiMacroSize = 4;	\
		else	\
			uiMacroSize = 5;	\
	}


/* Macro for the maximum number of bytes occupied by a compressed 32 bit integer  */
#define NUM_COMPRESSED_UINT_MAX_SIZE			(5)


/* Macro for skipping over a compressed integer in memory  */
#define NUM_SKIP_COMPRESSED_UINT(pucMacroPtr) \
	{	\
		ASSERT(pucMacroPtr != NULL);	\
		for ( ; *pucMacroPtr & NUM_COMPRESSED_CONTINUE_BIT; pucMacroPtr++ );	\
		pucMacroPtr++;	\
	}


/* Macro for reading a compressed integer from memory. The integer is 
** read starting at pucMacroPtr and is stored in uiMacroValue
*/
/* Newer and faster */
#define NUM_READ_COMPRESSED_UINT(uiMacroValue, pucMacroPtr) \
	{	\
		ASSERT(pucMacroPtr != NULL);	\
		for ( uiMacroValue = 0; uiMacroValue <<= NUM_COMPRESSED_DATA_BITS, uiMacroValue += (*pucMacroPtr & NUM_COMPRESSED_DATA_MASK), *pucMacroPtr & NUM_COMPRESSED_CONTINUE_BIT; pucMacroPtr++ );	\
		pucMacroPtr++;	\
	}

/* Older and slower */
/* #define NUM_READ_COMPRESSED_UINT(uiMacroValue, pucMacroPtr) \ */
/* 	{	unsigned char ucMacroByte = '\0';	\ */
/* 		ASSERT(pucMacroPtr != NULL);	\ */
/* 		uiMacroValue = 0;	\ */
/* 		do { 	\ */
/* 			uiMacroValue = uiMacroValue << NUM_COMPRESSED_DATA_BITS;	\ */
/* 			ucMacroByte = *(pucMacroPtr++);	\ */
/* 			uiMacroValue += (ucMacroByte & NUM_COMPRESSED_DATA_MASK); 	\ */
/* 		}	\ */
/* 		while ( ucMacroByte & NUM_COMPRESSED_CONTINUE_BIT );	\ */
/* 	} */


/* Macro for compressing an unsigned integer and writing it to memory */
#define NUM_WRITE_COMPRESSED_UINT(uiMacroValue, pucMacroPtr) \
	{	\
		unsigned char ucMacroByte = '\0';	\
		unsigned int uiMacroLocalValue = uiMacroValue;	\
		unsigned int uiMacroSize = 0; 	\
		unsigned int uiMacroBytesLeft = 0;	\
		ASSERT(uiMacroValue >= 0);	\
		ASSERT(pucMacroPtr != NULL);	\
		NUM_GET_COMPRESSED_UINT_SIZE(uiMacroLocalValue, uiMacroSize); \
		for ( uiMacroBytesLeft = uiMacroSize; uiMacroBytesLeft > 0; uiMacroBytesLeft-- ) { 	\
			ucMacroByte = (unsigned char)(uiMacroLocalValue & NUM_COMPRESSED_DATA_MASK);	\
			if ( uiMacroBytesLeft != uiMacroSize )	{ \
				ucMacroByte |= NUM_COMPRESSED_CONTINUE_BIT;	\
			}	\
			pucMacroPtr[uiMacroBytesLeft - 1] = ucMacroByte;	\
			uiMacroLocalValue >>= NUM_COMPRESSED_DATA_BITS;	\
		}	\
		pucMacroPtr += uiMacroSize; 	\
		ASSERT(uiMacroLocalValue == 0);	\
	}


/*---------------------------------------------------------------------------*/


/*
** ====================================== 
** === Number storage macros (varint) ===
** ======================================
*/


/* Masks used to mask out a varint in memory */
static unsigned int	uiVarintMaskGlobal[] = {0x0, 0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF};


/* Structure to store the byte size of each varint */
struct varintSize {
	unsigned char	ucSize1;
	unsigned char	ucSize2;
	unsigned char	ucSize3;
	unsigned char	ucSize4;
};


/* Structure which tells us the byte size of each varint based on the offset */
static struct varintSize pvsVarintSizesGlobal[] = 
{
	/*   0 - 00 00 00 00 */	{1,	1,	1,	1},		/*   1 - 00 00 00 01 */	{1,	1,	1,	2},		/*   2 - 00 00 00 10 */	{1,	1,	1,	3},		/*   3 - 00 00 00 11 */	{1,	1,	1,	4},
	/*   4 - 00 00 01 00 */	{1,	1,	2,	1},		/*   5 - 00 00 01 01 */	{1,	1,	2,	2},		/*   6 - 00 00 01 10 */	{1,	1,	2,	3},		/*   7 - 00 00 01 11 */	{1,	1,	2,	4},
	/*   8 - 00 00 10 00 */	{1,	1,	3,	1},		/*   9 - 00 00 10 01 */	{1,	1,	3,	2},		/*  10 - 00 00 10 10 */	{1,	1,	3,	3},		/*  11 - 00 00 10 11 */	{1,	1,	3,	4},
	/*  12 - 00 00 11 00 */	{1,	1,	4,	1},		/*  13 - 00 00 11 01 */	{1,	1,	4,	2},		/*  14 - 00 00 11 10 */	{1,	1,	4,	3},		/*  15 - 00 00 11 11 */	{1,	1,	4,	4},
	/*  16 - 00 01 00 00 */	{1,	2,	1,	1},		/*  17 - 00 01 00 01 */	{1,	2,	1,	2},		/*  18 - 00 01 00 10 */	{1,	2,	1,	3},		/*  19 - 00 01 00 11 */	{1,	2,	1,	4},
	/*  20 - 00 01 01 00 */	{1,	2,	2,	1},		/*  21 - 00 01 01 01 */	{1,	2,	2,	2},		/*  22 - 00 01 01 10 */	{1,	2,	2,	3},		/*  23 - 00 01 01 11 */	{1,	2,	2,	4},
	/*  24 - 00 01 10 00 */	{1,	2,	3,	1},		/*  25 - 00 01 10 01 */	{1,	2,	3,	2},		/*  26 - 00 01 10 10 */	{1,	2,	3,	3},		/*  27 - 00 01 10 11 */	{1,	2,	3,	4},
	/*  28 - 00 01 11 00 */	{1,	2,	4,	1},		/*  29 - 00 01 11 01 */	{1,	2,	4,	2},		/*  30 - 00 01 11 10 */	{1,	2,	4,	3},		/*  31 - 00 01 11 11 */	{1,	2,	4,	4},
	/*  32 - 00 10 00 00 */	{1,	3,	1,	1},		/*  33 - 00 10 00 01 */	{1,	3,	1,	2},		/*  34 - 00 10 00 10 */	{1,	3,	1,	3},		/*  35 - 00 10 00 11 */	{1,	3,	1,	4},
	/*  36 - 00 10 01 00 */	{1,	3,	2,	1},		/*  37 - 00 10 01 01 */	{1,	3,	2,	2},		/*  38 - 00 10 01 10 */	{1,	3,	2,	3},		/*  39 - 00 10 01 11 */	{1,	3,	2,	4},
	/*  40 - 00 10 10 00 */	{1,	3,	3,	1},		/*  41 - 00 10 10 01 */	{1,	3,	3,	2},		/*  42 - 00 10 10 10 */	{1,	3,	3,	3},		/*  43 - 00 10 10 11 */	{1,	3,	3,	4},
	/*  44 - 00 10 11 00 */	{1,	3,	4,	1},		/*  45 - 00 10 11 01 */	{1,	3,	4,	2},		/*  46 - 00 10 11 10 */	{1,	3,	4,	3},		/*  47 - 00 10 11 11 */	{1,	3,	4,	4},
	/*  48 - 00 11 00 00 */	{1,	4,	1,	1},		/*  49 - 00 11 00 01 */	{1,	4,	1,	2},		/*  50 - 00 11 00 10 */	{1,	4,	1,	3},		/*  51 - 00 11 00 11 */	{1,	4,	1,	4},
	/*  52 - 00 11 01 00 */	{1,	4,	2,	1},		/*  53 - 00 11 01 01 */	{1,	4,	2,	2},		/*  54 - 00 11 01 10 */	{1,	4,	2,	3},		/*  55 - 00 11 01 11 */	{1,	4,	2,	4},
	/*  56 - 00 11 10 00 */	{1,	4,	3,	1},		/*  57 - 00 11 10 01 */	{1,	4,	3,	2},		/*  58 - 00 11 10 10 */	{1,	4,	3,	3},		/*  59 - 00 11 10 11 */	{1,	4,	3,	4},
	/*  60 - 00 11 11 00 */	{1,	4,	4,	1},		/*  61 - 00 11 11 01 */	{1,	4,	4,	2},		/*  62 - 00 11 11 10 */	{1,	4,	4,	3},		/*  63 - 00 11 11 11 */	{1,	4,	4,	4},
	/*  64 - 01 00 00 00 */	{2,	1,	1,	1},		/*  65 - 01 00 00 01 */	{2,	1,	1,	2},		/*  66 - 01 00 00 10 */	{2,	1,	1,	3},		/*  67 - 01 00 00 11 */	{2,	1,	1,	4},
	/*  68 - 01 00 01 00 */	{2,	1,	2,	1},		/*  69 - 01 00 01 01 */	{2,	1,	2,	2},		/*  70 - 01 00 01 10 */	{2,	1,	2,	3},		/*  71 - 01 00 01 11 */	{2,	1,	2,	4},
	/*  72 - 01 00 10 00 */	{2,	1,	3,	1},		/*  73 - 01 00 10 01 */	{2,	1,	3,	2},		/*  74 - 01 00 10 10 */	{2,	1,	3,	3},		/*  75 - 01 00 10 11 */	{2,	1,	3,	4},
	/*  76 - 01 00 11 00 */	{2,	1,	4,	1},		/*  77 - 01 00 11 01 */	{2,	1,	4,	2},		/*  78 - 01 00 11 10 */	{2,	1,	4,	3},		/*  79 - 01 00 11 11 */	{2,	1,	4,	4},
	/*  80 - 01 01 00 00 */	{2,	2,	1,	1},		/*  81 - 01 01 00 01 */	{2,	2,	1,	2},		/*  82 - 01 01 00 10 */	{2,	2,	1,	3},		/*  83 - 01 01 00 11 */	{2,	2,	1,	4},
	/*  84 - 01 01 01 00 */	{2,	2,	2,	1},		/*  85 - 01 01 01 01 */	{2,	2,	2,	2},		/*  86 - 01 01 01 10 */	{2,	2,	2,	3},		/*  87 - 01 01 01 11 */	{2,	2,	2,	4},
	/*  88 - 01 01 10 00 */	{2,	2,	3,	1},		/*  89 - 01 01 10 01 */	{2,	2,	3,	2},		/*  90 - 01 01 10 10 */	{2,	2,	3,	3},		/*  91 - 01 01 10 11 */	{2,	2,	3,	4},
	/*  92 - 01 01 11 00 */	{2,	2,	4,	1},		/*  93 - 01 01 11 01 */	{2,	2,	4,	2},		/*  94 - 01 01 11 10 */	{2,	2,	4,	3},		/*  95 - 01 01 11 11 */	{2,	2,	4,	4},
	/*  96 - 01 10 00 00 */	{2,	3,	1,	1},		/*  97 - 01 10 00 01 */	{2,	3,	1,	2},		/*  98 - 01 10 00 10 */	{2,	3,	1,	3},		/*  99 - 01 10 00 11 */	{2,	3,	1,	4},
	/* 100 - 01 10 01 00 */	{2,	3,	2,	1},		/* 101 - 01 10 01 01 */	{2,	3,	2,	2},		/* 102 - 01 10 01 10 */	{2,	3,	2,	3},		/* 103 - 01 10 01 11 */	{2,	3,	2,	4},
	/* 104 - 01 10 10 00 */	{2,	3,	3,	1},		/* 105 - 01 10 10 01 */	{2,	3,	3,	2},		/* 106 - 01 10 10 10 */	{2,	3,	3,	3},		/* 107 - 01 10 10 11 */	{2,	3,	3,	4},
	/* 108 - 01 10 11 00 */	{2,	3,	4,	1},		/* 109 - 01 10 11 01 */	{2,	3,	4,	2},		/* 110 - 01 10 11 10 */	{2,	3,	4,	3},		/* 111 - 01 10 11 11 */	{2,	3,	4,	4},
	/* 112 - 01 11 00 00 */	{2,	4,	1,	1},		/* 113 - 01 11 00 01 */	{2,	4,	1,	2},		/* 114 - 01 11 00 10 */	{2,	4,	1,	3},		/* 115 - 01 11 00 11 */	{2,	4,	1,	4},
	/* 116 - 01 11 01 00 */	{2,	4,	2,	1},		/* 117 - 01 11 01 01 */	{2,	4,	2,	2},		/* 118 - 01 11 01 10 */	{2,	4,	2,	3},		/* 119 - 01 11 01 11 */	{2,	4,	2,	4},
	/* 120 - 01 11 10 00 */	{2,	4,	3,	1},		/* 121 - 01 11 10 01 */	{2,	4,	3,	2},		/* 122 - 01 11 10 10 */	{2,	4,	3,	3},		/* 123 - 01 11 10 11 */	{2,	4,	3,	4},
	/* 124 - 01 11 11 00 */	{2,	4,	4,	1},		/* 125 - 01 11 11 01 */	{2,	4,	4,	2},		/* 126 - 01 11 11 10 */	{2,	4,	4,	3},		/* 127 - 01 11 11 11 */	{2,	4,	4,	4},
	/* 128 - 10 00 00 00 */	{3,	1,	1,	1},		/* 129 - 10 00 00 01 */	{3,	1,	1,	2},		/* 130 - 10 00 00 10 */	{3,	1,	1,	3},		/* 131 - 10 00 00 11 */	{3,	1,	1,	4},
	/* 132 - 10 00 01 00 */	{3,	1,	2,	1},		/* 133 - 10 00 01 01 */	{3,	1,	2,	2},		/* 134 - 10 00 01 10 */	{3,	1,	2,	3},		/* 135 - 10 00 01 11 */	{3,	1,	2,	4},
	/* 136 - 10 00 10 00 */	{3,	1,	3,	1},		/* 137 - 10 00 10 01 */	{3,	1,	3,	2},		/* 138 - 10 00 10 10 */	{3,	1,	3,	3},		/* 139 - 10 00 10 11 */	{3,	1,	3,	4},
	/* 140 - 10 00 11 00 */	{3,	1,	4,	1},		/* 141 - 10 00 11 01 */	{3,	1,	4,	2},		/* 142 - 10 00 11 10 */	{3,	1,	4,	3},		/* 143 - 10 00 11 11 */	{3,	1,	4,	4},
	/* 144 - 10 01 00 00 */	{3,	2,	1,	1},		/* 145 - 10 01 00 01 */	{3,	2,	1,	2},		/* 146 - 10 01 00 10 */	{3,	2,	1,	3},		/* 147 - 10 01 00 11 */	{3,	2,	1,	4},
	/* 148 - 10 01 01 00 */	{3,	2,	2,	1},		/* 149 - 10 01 01 01 */	{3,	2,	2,	2},		/* 150 - 10 01 01 10 */	{3,	2,	2,	3},		/* 151 - 10 01 01 11 */	{3,	2,	2,	4},
	/* 152 - 10 01 10 00 */	{3,	2,	3,	1},		/* 153 - 10 01 10 01 */	{3,	2,	3,	2},		/* 154 - 10 01 10 10 */	{3,	2,	3,	3},		/* 155 - 10 01 10 11 */	{3,	2,	3,	4},
	/* 156 - 10 01 11 00 */	{3,	2,	4,	1},		/* 157 - 10 01 11 01 */	{3,	2,	4,	2},		/* 158 - 10 01 11 10 */	{3,	2,	4,	3},		/* 159 - 10 01 11 11 */	{3,	2,	4,	4},
	/* 160 - 10 10 00 00 */	{3,	3,	1,	1},		/* 161 - 10 10 00 01 */	{3,	3,	1,	2},		/* 162 - 10 10 00 10 */	{3,	3,	1,	3},		/* 163 - 10 10 00 11 */	{3,	3,	1,	4},
	/* 164 - 10 10 01 00 */	{3,	3,	2,	1},		/* 165 - 10 10 01 01 */	{3,	3,	2,	2},		/* 166 - 10 10 01 10 */	{3,	3,	2,	3},		/* 167 - 10 10 01 11 */	{3,	3,	2,	4},
	/* 168 - 10 10 10 00 */	{3,	3,	3,	1},		/* 169 - 10 10 10 01 */	{3,	3,	3,	2},		/* 170 - 10 10 10 10 */	{3,	3,	3,	3},		/* 171 - 10 10 10 11 */	{3,	3,	3,	4},
	/* 172 - 10 10 11 00 */	{3,	3,	4,	1},		/* 173 - 10 10 11 01 */	{3,	3,	4,	2},		/* 174 - 10 10 11 10 */	{3,	3,	4,	3},		/* 175 - 10 10 11 11 */	{3,	3,	4,	4},
	/* 176 - 10 11 00 00 */	{3,	4,	1,	1},		/* 177 - 10 11 00 01 */	{3,	4,	1,	2},		/* 178 - 10 11 00 10 */	{3,	4,	1,	3},		/* 179 - 10 11 00 11 */	{3,	4,	1,	4},
	/* 180 - 10 11 01 00 */	{3,	4,	2,	1},		/* 181 - 10 11 01 01 */	{3,	4,	2,	2},		/* 182 - 10 11 01 10 */	{3,	4,	2,	3},		/* 183 - 10 11 01 11 */	{3,	4,	2,	4},
	/* 184 - 10 11 10 00 */	{3,	4,	3,	1},		/* 185 - 10 11 10 01 */	{3,	4,	3,	2},		/* 186 - 10 11 10 10 */	{3,	4,	3,	3},		/* 187 - 10 11 10 11 */	{3,	4,	3,	4},
	/* 188 - 10 11 11 00 */	{3,	4,	4,	1},		/* 189 - 10 11 11 01 */	{3,	4,	4,	2},		/* 190 - 10 11 11 10 */	{3,	4,	4,	3},		/* 191 - 10 11 11 11 */	{3,	4,	4,	4},
	/* 192 - 11 00 00 00 */	{4,	1,	1,	1},		/* 193 - 11 00 00 01 */	{4,	1,	1,	2},		/* 194 - 11 00 00 10 */	{4,	1,	1,	3},		/* 195 - 11 00 00 11 */	{4,	1,	1,	4},
	/* 196 - 11 00 01 00 */	{4,	1,	2,	1},		/* 197 - 11 00 01 01 */	{4,	1,	2,	2},		/* 198 - 11 00 01 10 */	{4,	1,	2,	3},		/* 199 - 11 00 01 11 */	{4,	1,	2,	4},
	/* 200 - 11 00 10 00 */	{4,	1,	3,	1},		/* 201 - 11 00 10 01 */	{4,	1,	3,	2},		/* 202 - 11 00 10 10 */	{4,	1,	3,	3},		/* 203 - 11 00 10 11 */	{4,	1,	3,	4},
	/* 204 - 11 00 11 00 */	{4,	1,	4,	1},		/* 205 - 11 00 11 01 */	{4,	1,	4,	2},		/* 206 - 11 00 11 10 */	{4,	1,	4,	3},		/* 207 - 11 00 11 11 */	{4,	1,	4,	4},
	/* 208 - 11 01 00 00 */	{4,	2,	1,	1},		/* 209 - 11 01 00 01 */	{4,	2,	1,	2},		/* 210 - 11 01 00 10 */	{4,	2,	1,	3},		/* 211 - 11 01 00 11 */	{4,	2,	1,	4},
	/* 212 - 11 01 01 00 */	{4,	2,	2,	1},		/* 213 - 11 01 01 01 */	{4,	2,	2,	2},		/* 214 - 11 01 01 10 */	{4,	2,	2,	3},		/* 215 - 11 01 01 11 */	{4,	2,	2,	4},
	/* 216 - 11 01 10 00 */	{4,	2,	3,	1},		/* 217 - 11 01 10 01 */	{4,	2,	3,	2},		/* 218 - 11 01 10 10 */	{4,	2,	3,	3},		/* 219 - 11 01 10 11 */	{4,	2,	3,	4},
	/* 220 - 11 01 11 00 */	{4,	2,	4,	1},		/* 221 - 11 01 11 01 */	{4,	2,	4,	2},		/* 222 - 11 01 11 10 */	{4,	2,	4,	3},		/* 223 - 11 01 11 11 */	{4,	2,	4,	4},
	/* 224 - 11 10 00 00 */	{4,	3,	1,	1},		/* 225 - 11 10 00 01 */	{4,	3,	1,	2},		/* 226 - 11 10 00 10 */	{4,	3,	1,	3},		/* 227 - 11 10 00 11 */	{4,	3,	1,	4},
	/* 228 - 11 10 01 00 */	{4,	3,	2,	1},		/* 229 - 11 10 01 01 */	{4,	3,	2,	2},		/* 230 - 11 10 01 10 */	{4,	3,	2,	3},		/* 231 - 11 10 01 11 */	{4,	3,	2,	4},
	/* 232 - 11 10 10 00 */	{4,	3,	3,	1},		/* 233 - 11 10 10 01 */	{4,	3,	3,	2},		/* 234 - 11 10 10 10 */	{4,	3,	3,	3},		/* 235 - 11 10 10 11 */	{4,	3,	3,	4},
	/* 236 - 11 10 11 00 */	{4,	3,	4,	1},		/* 237 - 11 10 11 01 */	{4,	3,	4,	2},		/* 238 - 11 10 11 10 */	{4,	3,	4,	3},		/* 239 - 11 10 11 11 */	{4,	3,	4,	4},
	/* 240 - 11 11 00 00 */	{4,	4,	1,	1},		/* 241 - 11 11 00 01 */	{4,	4,	1,	2},		/* 242 - 11 11 00 10 */	{4,	4,	1,	3},		/* 243 - 11 11 00 11 */	{4,	4,	1,	4},
	/* 244 - 11 11 01 00 */	{4,	4,	2,	1},		/* 245 - 11 11 01 01 */	{4,	4,	2,	2},		/* 246 - 11 11 01 10 */	{4,	4,	2,	3},		/* 247 - 11 11 01 11 */	{4,	4,	2,	4},
	/* 248 - 11 11 10 00 */	{4,	4,	3,	1},		/* 249 - 11 11 10 01 */	{4,	4,	3,	2},		/* 250 - 11 11 10 10 */	{4,	4,	3,	3},		/* 251 - 11 11 10 11 */	{4,	4,	3,	4},
	/* 252 - 11 11 11 00 */	{4,	4,	4,	1},		/* 253 - 11 11 11 01 */	{4,	4,	4,	2},		/* 254 - 11 11 11 10 */	{4,	4,	4,	3},		/* 255 - 11 11 11 11 */	{4,	4,	4,	4},
};


/* Macro to get the size of a varint */
#define NUM_GET_VARINT_SIZE(uiMacroValue, uiMacroSize) \
	{	\
		ASSERT((uiMacroValue >= 0) && (uiMacroValue <= 0xFFFFFFFF));	\
\
		if ( uiMacroValue <= 0xFF ) {	\
			uiMacroSize = 1;	\
		}	\
		else if ( uiMacroValue <= 0xFFFF ) {	\
			uiMacroSize = 2;	\
		}	\
		else if ( uiMacroValue <= 0xFFFFFF ) {	\
			uiMacroSize = 3;	\
		}	\
		else if ( uiMacroValue <= 0xFFFFFFFF ) {	\
			uiMacroSize = 4;	\
		}	\
		else {	\
/* 			iUtlLogPanic(UTL_LOG_CONTEXT, "Value exceeds maximum size for a compact varint, value: %u", uiMacroValue); */	\
		}	\
	}



/* Macro to write a varint */
#define NUM_WRITE_VARINT(uiMacroValue, uiMacroSize, pucMacroPtr) \
	{	\
		ASSERT((uiMacroValue >= 0) && (uiMacroValue <= 0xFFFFFFFF));	\
		ASSERT(uiMacroValue >= 0);	\
		ASSERT((uiMacroSize >= 1) && (uiMacroSize <= 4));	\
		ASSERT(pucMacroPtr != NULL);	\
\
		unsigned char	*pucMacroValuePtr = (unsigned char *)&uiMacroValue;	\
\
		switch ( uiMacroSize ) {	\
			case 4: *pucMacroPtr = *pucMacroValuePtr; pucMacroPtr++; pucMacroValuePtr++;	\
			case 3: *pucMacroPtr = *pucMacroValuePtr; pucMacroPtr++; pucMacroValuePtr++;	\
			case 2: *pucMacroPtr = *pucMacroValuePtr; pucMacroPtr++; pucMacroValuePtr++;	\
			case 1: *pucMacroPtr = *pucMacroValuePtr; pucMacroPtr++; pucMacroValuePtr++;	\
		}	\
	}


/* Macro to read a varint */
#define NUM_READ_VARINT(uiMacroValue, uiMacroSize, pucMacroPtr) \
	{	\
		ASSERT((uiMacroSize >= 1) && (uiMacroSize <= 4));	\
		ASSERT(pucMacroPtr != NULL);	\
\
		uiMacroValue = (unsigned int)*((unsigned int *)(pucMacroPtr));	\
		uiMacroValue &= uiVarintMaskGlobal[uiMacroSize];	\
		pucMacroPtr += uiMacroSize;	\
	}



/* Header size for a varint quad or trio */
#define NUM_VARINT_HEADER_SIZE					(1)

/* Number of bits used to encode the number of bytes in the varint */
#define NUM_VARINT_HEADER_SIZE_BITS				(2)


/* Macro to get the size of a varint quad */
#define NUM_GET_VARINT_QUAD_SIZE(uiMacroValue1, uiMacroValue2, uiMacroValue3, uiMacroValue4, uiMacroSize) \
	{	\
		ASSERT((uiMacroValue1 >= 0) && (uiMacroValue1 <= 0xFFFFFFFF));	\
		ASSERT((uiMacroValue2 >= 0) && (uiMacroValue2 <= 0xFFFFFFFF));	\
		ASSERT((uiMacroValue3 >= 0) && (uiMacroValue3 <= 0xFFFFFFFF));	\
		ASSERT((uiMacroValue4 >= 0) && (uiMacroValue4 <= 0xFFFFFFFF));	\
\
		unsigned int	uiMacroLocalSize = 0;	\
\
		uiMacroSize = NUM_VARINT_HEADER_SIZE;	\
\
		NUM_GET_VARINT_SIZE(uiMacroValue1, uiMacroLocalSize);	\
		uiMacroSize += uiMacroLocalSize;	\
\
		NUM_GET_VARINT_SIZE(uiMacroValue2, uiMacroLocalSize);	\
		uiMacroSize += uiMacroLocalSize;	\
\
		NUM_GET_VARINT_SIZE(uiMacroValue3, uiMacroLocalSize);	\
		uiMacroSize += uiMacroLocalSize;	\
\
		NUM_GET_VARINT_SIZE(uiMacroValue4, uiMacroLocalSize);	\
		uiMacroSize += uiMacroLocalSize;	\
	}


/* Macro to write a varint quad */
#define NUM_WRITE_VARINT_QUAD(uiMacroValue1, uiMacroValue2, uiMacroValue3, uiMacroValue4, pucMacroPtr) \
	{	\
		ASSERT((uiMacroValue1 >= 0) && (uiMacroValue1 <= 0xFFFFFFFF));	\
		ASSERT((uiMacroValue2 >= 0) && (uiMacroValue2 <= 0xFFFFFFFF));	\
		ASSERT((uiMacroValue3 >= 0) && (uiMacroValue3 <= 0xFFFFFFFF));	\
		ASSERT((uiMacroValue4 >= 0) && (uiMacroValue4 <= 0xFFFFFFFF));	\
		ASSERT(pucMacroPtr != NULL);	\
\
		unsigned char	*pucMacroStartPtr = pucMacroPtr;	\
		unsigned char	ucMacroQuartetHeader = '\0';	\
		unsigned int	uiMacroSize = 0;	\
\
		pucMacroPtr += NUM_VARINT_HEADER_SIZE;	\
\
		NUM_GET_VARINT_SIZE(uiMacroValue1, uiMacroSize);	\
		ucMacroQuartetHeader |= (uiMacroSize - 1);	\
		NUM_WRITE_VARINT(uiMacroValue1, uiMacroSize, pucMacroPtr);	\
\
		NUM_GET_VARINT_SIZE(uiMacroValue2, uiMacroSize);	\
		ucMacroQuartetHeader <<= NUM_VARINT_HEADER_SIZE_BITS;	\
		ucMacroQuartetHeader |= (uiMacroSize - 1);	\
		NUM_WRITE_VARINT(uiMacroValue2, uiMacroSize, pucMacroPtr);	\
\
		NUM_GET_VARINT_SIZE(uiMacroValue3, uiMacroSize);	\
		ucMacroQuartetHeader <<= NUM_VARINT_HEADER_SIZE_BITS;	\
		ucMacroQuartetHeader |= (uiMacroSize - 1);	\
		NUM_WRITE_VARINT(uiMacroValue3, uiMacroSize, pucMacroPtr);	\
\
		NUM_GET_VARINT_SIZE(uiMacroValue4, uiMacroSize);	\
		ucMacroQuartetHeader <<= NUM_VARINT_HEADER_SIZE_BITS;	\
		ucMacroQuartetHeader |= (uiMacroSize - 1);	\
		NUM_WRITE_VARINT(uiMacroValue4, uiMacroSize, pucMacroPtr);	\
\
		*pucMacroStartPtr = ucMacroQuartetHeader;	\
	}


/* Macro to read a varint quad */
#define NUM_READ_VARINT_QUAD(uiMacroValue1, uiMacroValue2, uiMacroValue3, uiMacroValue4, pucMacroPtr) \
	{	\
		ASSERT(pucMacroPtr != NULL);	\
\
		struct varintSize	*pvsVarintSizesGlobalPtr = pvsVarintSizesGlobal + pucMacroPtr[0];	\
\
		pucMacroPtr += NUM_VARINT_HEADER_SIZE;	\
\
		NUM_READ_VARINT(uiMacroValue1, pvsVarintSizesGlobalPtr->ucSize1, pucMacroPtr) \
		NUM_READ_VARINT(uiMacroValue2, pvsVarintSizesGlobalPtr->ucSize2, pucMacroPtr) \
		NUM_READ_VARINT(uiMacroValue3, pvsVarintSizesGlobalPtr->ucSize3, pucMacroPtr) \
		NUM_READ_VARINT(uiMacroValue4, pvsVarintSizesGlobalPtr->ucSize4, pucMacroPtr) \
	}


/*---------------------------------------------------------------------------*/


/*
** ============================================== 
** === Number storage macros (compact varint) ===
** ==============================================
*/


/* Masks used to mask out a compact varint in memory */
static unsigned int	uiCompactVarintMaskGlobal[] = {0x0, 0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF};


/* Structure to store the byte size of each compact varint */
struct compactVarintSize {
	unsigned char	ucSize1;
	unsigned char	ucSize2;
	unsigned char	ucSize3;
	unsigned char	ucSize4;
};


/* Structure which tells us the byte size of each compact varint based on the offset */
static struct compactVarintSize pvsCompactVarintSizesGlobal[] = 
{
	/*   0 - 00 00 00 00 */	{0,	0,	0,	0},		/*   1 - 00 00 00 01 */	{0,	0,	0,	1},		/*   2 - 00 00 00 10 */	{0,	0,	0,	2},		/*   3 - 00 00 00 11 */	{0,	0,	0,	3},
	/*   4 - 00 00 01 00 */	{0,	0,	1,	0},		/*   5 - 00 00 01 01 */	{0,	0,	1,	1},		/*   6 - 00 00 01 10 */	{0,	0,	1,	2},		/*   7 - 00 00 01 11 */	{0,	0,	1,	3},
	/*   8 - 00 00 10 00 */	{0,	0,	2,	0},		/*   9 - 00 00 10 01 */	{0,	0,	2,	1},		/*  10 - 00 00 10 10 */	{0,	0,	2,	2},		/*  11 - 00 00 10 11 */	{0,	0,	2,	3},
	/*  12 - 00 00 11 00 */	{0,	0,	3,	0},		/*  13 - 00 00 11 01 */	{0,	0,	3,	1},		/*  14 - 00 00 11 10 */	{0,	0,	3,	2},		/*  15 - 00 00 11 11 */	{0,	0,	3,	3},
	/*  16 - 00 01 00 00 */	{0,	1,	0,	0},		/*  17 - 00 01 00 01 */	{0,	1,	0,	1},		/*  18 - 00 01 00 10 */	{0,	1,	0,	2},		/*  19 - 00 01 00 11 */	{0,	1,	0,	3},
	/*  20 - 00 01 01 00 */	{0,	1,	1,	0},		/*  21 - 00 01 01 01 */	{0,	1,	1,	1},		/*  22 - 00 01 01 10 */	{0,	1,	1,	2},		/*  23 - 00 01 01 11 */	{0,	1,	1,	3},
	/*  24 - 00 01 10 00 */	{0,	1,	2,	0},		/*  25 - 00 01 10 01 */	{0,	1,	2,	1},		/*  26 - 00 01 10 10 */	{0,	1,	2,	2},		/*  27 - 00 01 10 11 */	{0,	1,	2,	3},
	/*  28 - 00 01 11 00 */	{0,	1,	3,	0},		/*  29 - 00 01 11 01 */	{0,	1,	3,	1},		/*  30 - 00 01 11 10 */	{0,	1,	3,	2},		/*  31 - 00 01 11 11 */	{0,	1,	3,	3},
	/*  32 - 00 10 00 00 */	{0,	2,	0,	0},		/*  33 - 00 10 00 01 */	{0,	2,	0,	1},		/*  34 - 00 10 00 10 */	{0,	2,	0,	2},		/*  35 - 00 10 00 11 */	{0,	2,	0,	3},
	/*  36 - 00 10 01 00 */	{0,	2,	1,	0},		/*  37 - 00 10 01 01 */	{0,	2,	1,	1},		/*  38 - 00 10 01 10 */	{0,	2,	1,	2},		/*  39 - 00 10 01 11 */	{0,	2,	1,	3},
	/*  40 - 00 10 10 00 */	{0,	2,	2,	0},		/*  41 - 00 10 10 01 */	{0,	2,	2,	1},		/*  42 - 00 10 10 10 */	{0,	2,	2,	2},		/*  43 - 00 10 10 11 */	{0,	2,	2,	3},
	/*  44 - 00 10 11 00 */	{0,	2,	3,	0},		/*  45 - 00 10 11 01 */	{0,	2,	3,	1},		/*  46 - 00 10 11 10 */	{0,	2,	3,	2},		/*  47 - 00 10 11 11 */	{0,	2,	3,	3},
	/*  48 - 00 11 00 00 */	{0,	3,	0,	0},		/*  49 - 00 11 00 01 */	{0,	3,	0,	1},		/*  50 - 00 11 00 10 */	{0,	3,	0,	2},		/*  51 - 00 11 00 11 */	{0,	3,	0,	3},
	/*  52 - 00 11 01 00 */	{0,	3,	1,	0},		/*  53 - 00 11 01 01 */	{0,	3,	1,	1},		/*  54 - 00 11 01 10 */	{0,	3,	1,	2},		/*  55 - 00 11 01 11 */	{0,	3,	1,	3},
	/*  56 - 00 11 10 00 */	{0,	3,	2,	0},		/*  57 - 00 11 10 01 */	{0,	3,	2,	1},		/*  58 - 00 11 10 10 */	{0,	3,	2,	2},		/*  59 - 00 11 10 11 */	{0,	3,	2,	3},
	/*  60 - 00 11 11 00 */	{0,	3,	3,	0},		/*  61 - 00 11 11 01 */	{0,	3,	3,	1},		/*  62 - 00 11 11 10 */	{0,	3,	3,	2},		/*  63 - 00 11 11 11 */	{0,	3,	3,	3},
	/*  64 - 01 00 00 00 */	{1,	0,	0,	0},		/*  65 - 01 00 00 01 */	{1,	0,	0,	1},		/*  66 - 01 00 00 10 */	{1,	0,	0,	2},		/*  67 - 01 00 00 11 */	{1,	0,	0,	3},
	/*  68 - 01 00 01 00 */	{1,	0,	1,	0},		/*  69 - 01 00 01 01 */	{1,	0,	1,	1},		/*  70 - 01 00 01 10 */	{1,	0,	1,	2},		/*  71 - 01 00 01 11 */	{1,	0,	1,	3},
	/*  72 - 01 00 10 00 */	{1,	0,	2,	0},		/*  73 - 01 00 10 01 */	{1,	0,	2,	1},		/*  74 - 01 00 10 10 */	{1,	0,	2,	2},		/*  75 - 01 00 10 11 */	{1,	0,	2,	3},
	/*  76 - 01 00 11 00 */	{1,	0,	3,	0},		/*  77 - 01 00 11 01 */	{1,	0,	3,	1},		/*  78 - 01 00 11 10 */	{1,	0,	3,	2},		/*  79 - 01 00 11 11 */	{1,	0,	3,	3},
	/*  80 - 01 01 00 00 */	{1,	1,	0,	0},		/*  81 - 01 01 00 01 */	{1,	1,	0,	1},		/*  82 - 01 01 00 10 */	{1,	1,	0,	2},		/*  83 - 01 01 00 11 */	{1,	1,	0,	3},
	/*  84 - 01 01 01 00 */	{1,	1,	1,	0},		/*  85 - 01 01 01 01 */	{1,	1,	1,	1},		/*  86 - 01 01 01 10 */	{1,	1,	1,	2},		/*  87 - 01 01 01 11 */	{1,	1,	1,	3},
	/*  88 - 01 01 10 00 */	{1,	1,	2,	0},		/*  89 - 01 01 10 01 */	{1,	1,	2,	1},		/*  90 - 01 01 10 10 */	{1,	1,	2,	2},		/*  91 - 01 01 10 11 */	{1,	1,	2,	3},
	/*  92 - 01 01 11 00 */	{1,	1,	3,	0},		/*  93 - 01 01 11 01 */	{1,	1,	3,	1},		/*  94 - 01 01 11 10 */	{1,	1,	3,	2},		/*  95 - 01 01 11 11 */	{1,	1,	3,	3},
	/*  96 - 01 10 00 00 */	{1,	2,	0,	0},		/*  97 - 01 10 00 01 */	{1,	2,	0,	1},		/*  98 - 01 10 00 10 */	{1,	2,	0,	2},		/*  99 - 01 10 00 11 */	{1,	2,	0,	3},
	/* 100 - 01 10 01 00 */	{1,	2,	1,	0},		/* 101 - 01 10 01 01 */	{1,	2,	1,	1},		/* 102 - 01 10 01 10 */	{1,	2,	1,	2},		/* 103 - 01 10 01 11 */	{1,	2,	1,	3},
	/* 104 - 01 10 10 00 */	{1,	2,	2,	0},		/* 105 - 01 10 10 01 */	{1,	2,	2,	1},		/* 106 - 01 10 10 10 */	{1,	2,	2,	2},		/* 107 - 01 10 10 11 */	{1,	2,	2,	3},
	/* 108 - 01 10 11 00 */	{1,	2,	3,	0},		/* 109 - 01 10 11 01 */	{1,	2,	3,	1},		/* 110 - 01 10 11 10 */	{1,	2,	3,	2},		/* 111 - 01 10 11 11 */	{1,	2,	3,	3},
	/* 112 - 01 11 00 00 */	{1,	3,	0,	0},		/* 113 - 01 11 00 01 */	{1,	3,	0,	1},		/* 114 - 01 11 00 10 */	{1,	3,	0,	2},		/* 115 - 01 11 00 11 */	{1,	3,	0,	3},
	/* 116 - 01 11 01 00 */	{1,	3,	1,	0},		/* 117 - 01 11 01 01 */	{1,	3,	1,	1},		/* 118 - 01 11 01 10 */	{1,	3,	1,	2},		/* 119 - 01 11 01 11 */	{1,	3,	1,	3},
	/* 120 - 01 11 10 00 */	{1,	3,	2,	0},		/* 121 - 01 11 10 01 */	{1,	3,	2,	1},		/* 122 - 01 11 10 10 */	{1,	3,	2,	2},		/* 123 - 01 11 10 11 */	{1,	3,	2,	3},
	/* 124 - 01 11 11 00 */	{1,	3,	3,	0},		/* 125 - 01 11 11 01 */	{1,	3,	3,	1},		/* 126 - 01 11 11 10 */	{1,	3,	3,	2},		/* 127 - 01 11 11 11 */	{1,	3,	3,	3},
	/* 128 - 10 00 00 00 */	{2,	0,	0,	0},		/* 129 - 10 00 00 01 */	{2,	0,	0,	1},		/* 130 - 10 00 00 10 */	{2,	0,	0,	2},		/* 131 - 10 00 00 11 */	{2,	0,	0,	3},
	/* 132 - 10 00 01 00 */	{2,	0,	1,	0},		/* 133 - 10 00 01 01 */	{2,	0,	1,	1},		/* 134 - 10 00 01 10 */	{2,	0,	1,	2},		/* 135 - 10 00 01 11 */	{2,	0,	1,	3},
	/* 136 - 10 00 10 00 */	{2,	0,	2,	0},		/* 137 - 10 00 10 01 */	{2,	0,	2,	1},		/* 138 - 10 00 10 10 */	{2,	0,	2,	2},		/* 139 - 10 00 10 11 */	{2,	0,	2,	3},
	/* 140 - 10 00 11 00 */	{2,	0,	3,	0},		/* 141 - 10 00 11 01 */	{2,	0,	3,	1},		/* 142 - 10 00 11 10 */	{2,	0,	3,	2},		/* 143 - 10 00 11 11 */	{2,	0,	3,	3},
	/* 144 - 10 01 00 00 */	{2,	1,	0,	0},		/* 145 - 10 01 00 01 */	{2,	1,	0,	1},		/* 146 - 10 01 00 10 */	{2,	1,	0,	2},		/* 147 - 10 01 00 11 */	{2,	1,	0,	3},
	/* 148 - 10 01 01 00 */	{2,	1,	1,	0},		/* 149 - 10 01 01 01 */	{2,	1,	1,	1},		/* 150 - 10 01 01 10 */	{2,	1,	1,	2},		/* 151 - 10 01 01 11 */	{2,	1,	1,	3},
	/* 152 - 10 01 10 00 */	{2,	1,	2,	0},		/* 153 - 10 01 10 01 */	{2,	1,	2,	1},		/* 154 - 10 01 10 10 */	{2,	1,	2,	2},		/* 155 - 10 01 10 11 */	{2,	1,	2,	3},
	/* 156 - 10 01 11 00 */	{2,	1,	3,	0},		/* 157 - 10 01 11 01 */	{2,	1,	3,	1},		/* 158 - 10 01 11 10 */	{2,	1,	3,	2},		/* 159 - 10 01 11 11 */	{2,	1,	3,	3},
	/* 160 - 10 10 00 00 */	{2,	2,	0,	0},		/* 161 - 10 10 00 01 */	{2,	2,	0,	1},		/* 162 - 10 10 00 10 */	{2,	2,	0,	2},		/* 163 - 10 10 00 11 */	{2,	2,	0,	3},
	/* 164 - 10 10 01 00 */	{2,	2,	1,	0},		/* 165 - 10 10 01 01 */	{2,	2,	1,	1},		/* 166 - 10 10 01 10 */	{2,	2,	1,	2},		/* 167 - 10 10 01 11 */	{2,	2,	1,	3},
	/* 168 - 10 10 10 00 */	{2,	2,	2,	0},		/* 169 - 10 10 10 01 */	{2,	2,	2,	1},		/* 170 - 10 10 10 10 */	{2,	2,	2,	2},		/* 171 - 10 10 10 11 */	{2,	2,	2,	3},
	/* 172 - 10 10 11 00 */	{2,	2,	3,	0},		/* 173 - 10 10 11 01 */	{2,	2,	3,	1},		/* 174 - 10 10 11 10 */	{2,	2,	3,	2},		/* 175 - 10 10 11 11 */	{2,	2,	3,	3},
	/* 176 - 10 11 00 00 */	{2,	3,	0,	0},		/* 177 - 10 11 00 01 */	{2,	3,	0,	1},		/* 178 - 10 11 00 10 */	{2,	3,	0,	2},		/* 179 - 10 11 00 11 */	{2,	3,	0,	3},
	/* 180 - 10 11 01 00 */	{2,	3,	1,	0},		/* 181 - 10 11 01 01 */	{2,	3,	1,	1},		/* 182 - 10 11 01 10 */	{2,	3,	1,	2},		/* 183 - 10 11 01 11 */	{2,	3,	1,	3},
	/* 184 - 10 11 10 00 */	{2,	3,	2,	0},		/* 185 - 10 11 10 01 */	{2,	3,	2,	1},		/* 186 - 10 11 10 10 */	{2,	3,	2,	2},		/* 187 - 10 11 10 11 */	{2,	3,	2,	3},
	/* 188 - 10 11 11 00 */	{2,	3,	3,	0},		/* 189 - 10 11 11 01 */	{2,	3,	3,	1},		/* 190 - 10 11 11 10 */	{2,	3,	3,	2},		/* 191 - 10 11 11 11 */	{2,	3,	3,	3},
	/* 192 - 11 00 00 00 */	{3,	0,	0,	0},		/* 193 - 11 00 00 01 */	{3,	0,	0,	1},		/* 194 - 11 00 00 10 */	{3,	0,	0,	2},		/* 195 - 11 00 00 11 */	{3,	0,	0,	3},
	/* 196 - 11 00 01 00 */	{3,	0,	1,	0},		/* 197 - 11 00 01 01 */	{3,	0,	1,	1},		/* 198 - 11 00 01 10 */	{3,	0,	1,	2},		/* 199 - 11 00 01 11 */	{3,	0,	1,	3},
	/* 200 - 11 00 10 00 */	{3,	0,	2,	0},		/* 201 - 11 00 10 01 */	{3,	0,	2,	1},		/* 202 - 11 00 10 10 */	{3,	0,	2,	2},		/* 203 - 11 00 10 11 */	{3,	0,	2,	3},
	/* 204 - 11 00 11 00 */	{3,	0,	3,	0},		/* 205 - 11 00 11 01 */	{3,	0,	3,	1},		/* 206 - 11 00 11 10 */	{3,	0,	3,	2},		/* 207 - 11 00 11 11 */	{3,	0,	3,	3},
	/* 208 - 11 01 00 00 */	{3,	1,	0,	0},		/* 209 - 11 01 00 01 */	{3,	1,	0,	1},		/* 210 - 11 01 00 10 */	{3,	1,	0,	2},		/* 211 - 11 01 00 11 */	{3,	1,	0,	3},
	/* 212 - 11 01 01 00 */	{3,	1,	1,	0},		/* 213 - 11 01 01 01 */	{3,	1,	1,	1},		/* 214 - 11 01 01 10 */	{3,	1,	1,	2},		/* 215 - 11 01 01 11 */	{3,	1,	1,	3},
	/* 216 - 11 01 10 00 */	{3,	1,	2,	0},		/* 217 - 11 01 10 01 */	{3,	1,	2,	1},		/* 218 - 11 01 10 10 */	{3,	1,	2,	2},		/* 219 - 11 01 10 11 */	{3,	1,	2,	3},
	/* 220 - 11 01 11 00 */	{3,	1,	3,	0},		/* 221 - 11 01 11 01 */	{3,	1,	3,	1},		/* 222 - 11 01 11 10 */	{3,	1,	3,	2},		/* 223 - 11 01 11 11 */	{3,	1,	3,	3},
	/* 224 - 11 10 00 00 */	{3,	2,	0,	0},		/* 225 - 11 10 00 01 */	{3,	2,	0,	1},		/* 226 - 11 10 00 10 */	{3,	2,	0,	2},		/* 227 - 11 10 00 11 */	{3,	2,	0,	3},
	/* 228 - 11 10 01 00 */	{3,	2,	1,	0},		/* 229 - 11 10 01 01 */	{3,	2,	1,	1},		/* 230 - 11 10 01 10 */	{3,	2,	1,	2},		/* 231 - 11 10 01 11 */	{3,	2,	1,	3},
	/* 232 - 11 10 10 00 */	{3,	2,	2,	0},		/* 233 - 11 10 10 01 */	{3,	2,	2,	1},		/* 234 - 11 10 10 10 */	{3,	2,	2,	2},		/* 235 - 11 10 10 11 */	{3,	2,	2,	3},
	/* 236 - 11 10 11 00 */	{3,	2,	3,	0},		/* 237 - 11 10 11 01 */	{3,	2,	3,	1},		/* 238 - 11 10 11 10 */	{3,	2,	3,	2},		/* 239 - 11 10 11 11 */	{3,	2,	3,	3},
	/* 240 - 11 11 00 00 */	{3,	3,	0,	0},		/* 241 - 11 11 00 01 */	{3,	3,	0,	1},		/* 242 - 11 11 00 10 */	{3,	3,	0,	2},		/* 243 - 11 11 00 11 */	{3,	3,	0,	3},
	/* 244 - 11 11 01 00 */	{3,	3,	1,	0},		/* 245 - 11 11 01 01 */	{3,	3,	1,	1},		/* 246 - 11 11 01 10 */	{3,	3,	1,	2},		/* 247 - 11 11 01 11 */	{3,	3,	1,	3},
	/* 248 - 11 11 10 00 */	{3,	3,	2,	0},		/* 249 - 11 11 10 01 */	{3,	3,	2,	1},		/* 250 - 11 11 10 10 */	{3,	3,	2,	2},		/* 251 - 11 11 10 11 */	{3,	3,	2,	3},
	/* 252 - 11 11 11 00 */	{3,	3,	3,	0},		/* 253 - 11 11 11 01 */	{3,	3,	3,	1},		/* 254 - 11 11 11 10 */	{3,	3,	3,	2},		/* 255 - 11 11 11 11 */	{3,	3,	3,	3},
};


/* Macro to get the size of a compact varint */
#define NUM_GET_COMPACT_VARINT_SIZE(uiMacroValue, uiMacroSize) \
	{	\
		ASSERT((uiMacroValue >= 0) && (uiMacroValue <= 0xFFFFFF));	\
\
		if ( uiMacroValue == 0 ) {	\
			uiMacroSize = 0;	\
		}	\
		else if ( uiMacroValue <= 0xFF ) {	\
			uiMacroSize = 1;	\
		}	\
		else if ( uiMacroValue <= 0xFFFF ) {	\
			uiMacroSize = 2;	\
		}	\
		else if ( uiMacroValue <= 0xFFFFFF ) {	\
			uiMacroSize = 3;	\
		}	\
		else {	\
/* 			iUtlLogPanic(UTL_LOG_CONTEXT, "Value exceeds maximum size for a compact varint, value: %u", uiMacroValue); */	\
		}	\
	}



/* Macro to write a compact varint */
#define NUM_WRITE_COMPACT_VARINT(uiMacroValue, uiMacroSize, pucMacroPtr) \
	{	\
		ASSERT((uiMacroValue >= 0) && (uiMacroValue <= 0xFFFFFF));	\
		ASSERT((uiMacroSize >= 0) && (uiMacroSize <= 3));	\
		ASSERT(pucMacroPtr != NULL);	\
\
		unsigned char	*pucMacroValuePtr = (unsigned char *)&uiMacroValue;	\
\
		switch ( uiMacroSize ) {	\
			case 3: *pucMacroPtr = *pucMacroValuePtr; pucMacroPtr++; pucMacroValuePtr++;	\
			case 2: *pucMacroPtr = *pucMacroValuePtr; pucMacroPtr++; pucMacroValuePtr++;	\
			case 1: *pucMacroPtr = *pucMacroValuePtr; pucMacroPtr++; pucMacroValuePtr++;	\
			case 0:	; \
		}	\
	}


/* Macro to read a compact varint */
#define NUM_READ_COMPACT_VARINT(uiMacroValue, uiMacroSize, pucMacroPtr) \
	{	\
		ASSERT((uiMacroSize >= 0) && (uiMacroSize <= 3));	\
		ASSERT(pucMacroPtr != NULL);	\
\
		uiMacroValue = (unsigned int)*((unsigned int *)(pucMacroPtr));	\
		uiMacroValue &= uiCompactVarintMaskGlobal[uiMacroSize];	\
		pucMacroPtr += uiMacroSize;	\
	}



/* Header size for a varint quad or trio */
#define NUM_COMPACT_VARINT_HEADER_SIZE						(1)

/* Number of bits used to encode the number of bytes in the varint */
#define NUM_COMPACT_VARINT_HEADER_BYTE_COUNT_BITS			(2)


/* Macro to get the size of a compact varint quad */
#define NUM_GET_COMPACT_VARINT_QUAD_SIZE(uiMacroValue1, uiMacroValue2, uiMacroValue3, uiMacroValue4, uiMacroSize) \
	{	\
		ASSERT((uiMacroValue1 >= 0) && (uiMacroValue1 <= 0xFFFFFF));	\
		ASSERT((uiMacroValue2 >= 0) && (uiMacroValue2 <= 0xFFFFFF));	\
		ASSERT((uiMacroValue3 >= 0) && (uiMacroValue3 <= 0xFFFFFF));	\
		ASSERT((uiMacroValue4 >= 0) && (uiMacroValue4 <= 0xFFFFFF));	\
\
		unsigned int	uiMacroLocalSize = 0;	\
\
		uiMacroSize = NUM_COMPACT_VARINT_HEADER_SIZE;	\
\
		NUM_GET_COMPACT_VARINT_SIZE(uiMacroValue1, uiMacroLocalSize);	\
		uiMacroSize += uiMacroLocalSize;	\
\
		NUM_GET_COMPACT_VARINT_SIZE(uiMacroValue2, uiMacroLocalSize);	\
		uiMacroSize += uiMacroLocalSize;	\
\
		NUM_GET_COMPACT_VARINT_SIZE(uiMacroValue3, uiMacroLocalSize);	\
		uiMacroSize += uiMacroLocalSize;	\
\
		NUM_GET_COMPACT_VARINT_SIZE(uiMacroValue4, uiMacroLocalSize);	\
		uiMacroSize += uiMacroLocalSize;	\
	}


/* Macro to write a compact varint quad */
#define NUM_WRITE_COMPACT_VARINT_QUAD(uiMacroValue1, uiMacroValue2, uiMacroValue3, uiMacroValue4, pucMacroPtr) \
	{	\
		ASSERT((uiMacroValue1 >= 0) && (uiMacroValue1 <= 0xFFFFFF));	\
		ASSERT((uiMacroValue2 >= 0) && (uiMacroValue2 <= 0xFFFFFF));	\
		ASSERT((uiMacroValue3 >= 0) && (uiMacroValue3 <= 0xFFFFFF));	\
		ASSERT((uiMacroValue4 >= 0) && (uiMacroValue4 <= 0xFFFFFF));	\
		ASSERT(pucMacroPtr != NULL);	\
\
		unsigned char	*pucMacroStartPtr = pucMacroPtr;	\
		unsigned char	ucMacroQuartetHeader = '\0';	\
		unsigned int	uiMacroSize = 0;	\
\
		pucMacroPtr += NUM_COMPACT_VARINT_HEADER_SIZE;	\
\
		NUM_GET_COMPACT_VARINT_SIZE(uiMacroValue1, uiMacroSize);	\
		ucMacroQuartetHeader |= uiMacroSize;	\
		NUM_WRITE_COMPACT_VARINT(uiMacroValue1, uiMacroSize, pucMacroPtr);	\
\
		NUM_GET_COMPACT_VARINT_SIZE(uiMacroValue2, uiMacroSize);	\
		ucMacroQuartetHeader <<= NUM_COMPACT_VARINT_HEADER_BYTE_COUNT_BITS;	\
		ucMacroQuartetHeader |= uiMacroSize;	\
		NUM_WRITE_COMPACT_VARINT(uiMacroValue2, uiMacroSize, pucMacroPtr);	\
\
		NUM_GET_COMPACT_VARINT_SIZE(uiMacroValue3, uiMacroSize);	\
		ucMacroQuartetHeader <<= NUM_COMPACT_VARINT_HEADER_BYTE_COUNT_BITS;	\
		ucMacroQuartetHeader |= uiMacroSize;	\
		NUM_WRITE_COMPACT_VARINT(uiMacroValue3, uiMacroSize, pucMacroPtr);	\
\
		NUM_GET_COMPACT_VARINT_SIZE(uiMacroValue4, uiMacroSize);	\
		ucMacroQuartetHeader <<= NUM_COMPACT_VARINT_HEADER_BYTE_COUNT_BITS;	\
		ucMacroQuartetHeader |= uiMacroSize;	\
		NUM_WRITE_COMPACT_VARINT(uiMacroValue4, uiMacroSize, pucMacroPtr);	\
\
		*pucMacroStartPtr = ucMacroQuartetHeader;	\
	}


/* Macro to read a compact varint quad */
#define NUM_READ_COMPACT_VARINT_QUAD(uiMacroValue1, uiMacroValue2, uiMacroValue3, uiMacroValue4, pucMacroPtr) \
	{	\
		ASSERT(pucMacroPtr != NULL);	\
\
		struct compactVarintSize	*pvsVarintSizesGlobalPtr = pvsCompactVarintSizesGlobal + pucMacroPtr[0];	\
\
		pucMacroPtr += NUM_COMPACT_VARINT_HEADER_SIZE;	\
\
		NUM_READ_COMPACT_VARINT(uiMacroValue1, pvsVarintSizesGlobalPtr->ucSize1, pucMacroPtr) \
		NUM_READ_COMPACT_VARINT(uiMacroValue2, pvsVarintSizesGlobalPtr->ucSize2, pucMacroPtr) \
		NUM_READ_COMPACT_VARINT(uiMacroValue3, pvsVarintSizesGlobalPtr->ucSize3, pucMacroPtr) \
		NUM_READ_COMPACT_VARINT(uiMacroValue4, pvsVarintSizesGlobalPtr->ucSize4, pucMacroPtr) \
	}


/*---------------------------------------------------------------------------*/


/* Macro to get the diff between two timeval structures */
#define UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal) \
{ \
	tvDiffTimeVal.tv_sec = tvEndTimeVal.tv_sec - tvStartTimeVal.tv_sec; \
	tvDiffTimeVal.tv_usec = tvEndTimeVal.tv_usec - tvStartTimeVal.tv_usec; \
	if ( tvDiffTimeVal.tv_usec < 0 ) { \
		tvDiffTimeVal.tv_sec--; \
		tvDiffTimeVal.tv_usec += 1000000; \
	} \
}


/* Macro to convert a timeval structure to microseconds */
#define UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvTimeVal, dDouble) \
{ \
	dDouble = ((double)tvTimeVal.tv_sec * 1000000) + (double)tvTimeVal.tv_usec; \
}


/*---------------------------------------------------------------------------*/


/*

	Function:	pucUtlStringsFormatIntegerString()

	Purpose:	Returns a pointer to a nicely formated number, currently we
				only deal with US formats.

				We also assume that there is enough space in the return string

	Parameters:	pucNumberString		number string to format
				pucString			return pointer for the formated string
				uiStringLength		size of the return pointer

	Global Variables:	none

	Returns:	A pointer to a formated string, null on error

*/
unsigned char *pucUtlStringsFormatIntegerString
(
	unsigned char *pucNumberString,
	unsigned char *pucString,
	unsigned int uiStringLength
)
{

	unsigned char			*pucStringPtr = NULL;
	unsigned char			*pucNumberStringPtr = NULL;
	boolean					bFlag = false;


	/* Start off the pointers */
	pucNumberStringPtr = pucNumberString;
	pucStringPtr = pucString;


	/* Loop over all the characters */
	while ( *pucNumberStringPtr != '\0' ) {
		
		/* Do we want to put a comma down? */
		if ( ((strlen(pucNumberStringPtr) % 3) == 0) && (bFlag == true) ) {

			/* Check that we are not going to burst the buffer */
			if ( (pucStringPtr - pucString) == uiStringLength ) {
				*pucStringPtr = '\0';
				return (pucString);
			}
	
			*pucStringPtr = ',';
			pucStringPtr++;
		}
		
		/* Copy the character over */
		*pucStringPtr = *pucNumberStringPtr;

		/* Set the flag when we can start putting commas down */
		if ( (pucNumberStringPtr != pucNumberString) || (*pucNumberStringPtr != '-') ) {
			bFlag = true;
		}

		/* Check that we are not going to burst the buffer */
		if ( (pucStringPtr - pucString) == uiStringLength ) {
			*pucStringPtr = '\0';
			return (pucString);
		}

		pucStringPtr++;
		pucNumberStringPtr++;
	}

	/* Terminate the destination string */
	*pucStringPtr = '\0';


	return (pucString);

}


/*---------------------------------------------------------------------------*/


/* Performance test */
#define COMPRESSED_UINT_IN_PLACE					(1)
#define VARINT_IN_PLACE								(1)
#define COMPACT_VARINT_IN_PLACE						(1)

#define COMPRESSED_UINT_ACROSS_MEMORY				(1)
#define VARINT_ACROSS_MEMORY						(1)
#define COMPACT_VARINT_ACROSS_MEMORY				(1)


/* Checks the values read, disabling this causes the 
** optimizer to optimize away the VARINT and COMPACT_VARINT
** loops which messes up the stats.
*/
#define CHECK_READ									(1)


/* Defines which control the nature of the performance test */
#define DATA_LENGTH									(1000)
#define STRING_LENGTH								(1000)

#define NUMBER_1									(1235)
#define NUMBER_2									(123456)
#define NUMBER_3									(1234567)
#define NUMBER_4									(12345678)

#define REPETITIONS									(60)
#define ITERATIONS									(16000000)



/* Integrity tests */
/* #define TEST_VARINT_1								(1) */
/* #define TEST_VARINT_2								(1) */
/* #define TEST_COMPACT_VARINT_1						(1) */
/* #define TEST_COMPACT_VARINT_2						(1) */


/*---------------------------------------------------------------------------*/


/*{

	Function:	main()

	Purpose:	Main.

	Called by:	main()

	Parameters:	void

	Global Variables:	none

	Returns:	int

}*/
int main
(
	int argc,
	char *argv[]
)
{


	printf("\n");


#if defined(COMPRESSED_UINT_IN_PLACE)
	/* Testing compressed uint in place */
	{
	
		unsigned char		pucData[DATA_LENGTH];
		unsigned char		*pucDataPtr = NULL;
	
		unsigned int		uiValueWritten1 = NUMBER_1;
		unsigned int		uiValueWritten2 = NUMBER_2;
		unsigned int		uiValueWritten3 = NUMBER_3;
		unsigned int		uiValueWritten4 = NUMBER_4;
		unsigned int		uiValueRead1 = 0;
		unsigned int		uiValueRead2 = 0;
		unsigned int		uiValueRead3 = 0;
		unsigned int		uiValueRead4 = 0;
	
		unsigned int		uiI = 0;
		unsigned int		uiJ = 0;
	
		unsigned long long	ullTotalIterations = 0;

		struct timeval		tvStartTimeVal;
		struct timeval		tvEndTimeVal;
		struct timeval		tvDiffTimeVal;
		double				dMicroSeconds = 0;
		unsigned char		pucNumberString[STRING_LENGTH];
		unsigned char		pucString[STRING_LENGTH];
	
		
		memset(pucData, DATA_LENGTH, 0);
	
		pucDataPtr = pucData;
		NUM_WRITE_COMPRESSED_UINT(uiValueWritten1, pucDataPtr);
		NUM_WRITE_COMPRESSED_UINT(uiValueWritten2, pucDataPtr);
		NUM_WRITE_COMPRESSED_UINT(uiValueWritten3, pucDataPtr);
		NUM_WRITE_COMPRESSED_UINT(uiValueWritten4, pucDataPtr);
	
	
		gettimeofday(&tvStartTimeVal, NULL);
	
		for ( uiI = 0, ullTotalIterations = 0; uiI < REPETITIONS; uiI++ ) {
	
			for ( uiJ = 0, pucDataPtr = pucData; uiJ < ITERATIONS; uiJ++, ullTotalIterations++ ) {

				pucDataPtr = pucData;
				NUM_READ_COMPRESSED_UINT(uiValueRead1, pucDataPtr);
				NUM_READ_COMPRESSED_UINT(uiValueRead2, pucDataPtr);
				NUM_READ_COMPRESSED_UINT(uiValueRead3, pucDataPtr);
				NUM_READ_COMPRESSED_UINT(uiValueRead4, pucDataPtr);
	
#if defined(CHECK_READ)
				if ( uiValueRead1 != uiValueWritten1 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead1, uiValueWritten1);
					exit (-1);
				}
		
				if ( uiValueRead2 != uiValueWritten2 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead2, uiValueWritten2);
					exit (-1);
				}
		
				if ( uiValueRead3 != uiValueWritten3 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead3, uiValueWritten3);
					exit (-1);
				}
		
				if ( uiValueRead4 != uiValueWritten4 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead4, uiValueWritten4);
					exit (-1);
				}
#endif	/* defined(CHECK_READ) */
			}
		}
	
		gettimeofday(&tvEndTimeVal, NULL);
	
		UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal);
		UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvDiffTimeVal, dMicroSeconds);
	
		snprintf(pucNumberString, STRING_LENGTH, "%llu", (ullTotalIterations * 4));
		printf("Reading compressed uint in place, numbers read: %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%.0f", dMicroSeconds);
		printf("\tMicroseconds : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", (double)(ullTotalIterations * 4) * ((double)1000000 / dMicroSeconds));
		printf("\tIterations/second : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		printf("\n\n");
	
	}
#endif	/* defined(COMPRESSED_UINT_IN_PLACE) */



#if defined(VARINT_IN_PLACE)
	/* Testing varint in place */
	{
	
		unsigned char		pucData[DATA_LENGTH];
		unsigned char		*pucDataPtr = NULL;
	
		unsigned int		uiValueWritten1 = NUMBER_1;
		unsigned int		uiValueWritten2 = NUMBER_2;
		unsigned int		uiValueWritten3 = NUMBER_3;
		unsigned int		uiValueWritten4 = NUMBER_4;
		unsigned int		uiValueRead1 = 0;
		unsigned int		uiValueRead2 = 0;
		unsigned int		uiValueRead3 = 0;
		unsigned int		uiValueRead4 = 0;
		
		unsigned int		uiI = 0;
		unsigned int		uiJ = 0;
	
		unsigned long long	ullTotalIterations = 0;
	
		struct timeval		tvStartTimeVal;
		struct timeval		tvEndTimeVal;
		struct timeval		tvDiffTimeVal;
		double				dMicroSeconds = 0;
		unsigned char		pucNumberString[STRING_LENGTH];
		unsigned char		pucString[STRING_LENGTH];

		
		memset(pucData, DATA_LENGTH, 0);
	
		pucDataPtr = pucData;
		NUM_WRITE_VARINT_QUAD(uiValueWritten1, uiValueWritten2, uiValueWritten3, uiValueWritten4, pucDataPtr);
	
		gettimeofday(&tvStartTimeVal, NULL);
	
		for ( uiI = 0, ullTotalIterations = 0; uiI < REPETITIONS; uiI++ ) {
	
			for ( uiJ = 0, pucDataPtr = pucData; uiJ < ITERATIONS; uiJ++, ullTotalIterations++ ) {
	
				pucDataPtr = pucData;
				NUM_READ_VARINT_QUAD(uiValueRead1, uiValueRead2, uiValueRead3, uiValueRead4, pucDataPtr);
		
#if defined(CHECK_READ)
				if ( uiValueRead1 != uiValueWritten1 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead1, uiValueWritten1);
					exit (-1);
				}
		
				if ( uiValueRead2 != uiValueWritten2 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead2, uiValueWritten2);
					exit (-1);
				}
		
				if ( uiValueRead3 != uiValueWritten3 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead3, uiValueWritten3);
					exit (-1);
				}
		
				if ( uiValueRead4 != uiValueWritten4 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead4, uiValueWritten4);
					exit (-1);
				}
#endif	/* defined(CHECK_READ) */
			}
		}


		gettimeofday(&tvEndTimeVal, NULL);
	
		UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal);
		UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvDiffTimeVal, dMicroSeconds);
	
		snprintf(pucNumberString, STRING_LENGTH, "%llu", (ullTotalIterations * 4));
		printf("Reading varint quad in place, numbers read: %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", dMicroSeconds);
		printf("\tMicroseconds : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", (double)(ullTotalIterations * 4) * ((double)1000000 / dMicroSeconds));
		printf("\tIterations/second : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		printf("\n\n");

	}
#endif	/* defined(VARINT_IN_PLACE) */



#if defined(COMPACT_VARINT_IN_PLACE)
	/* Testing compact varint in place */
	{
	
		unsigned char		pucData[DATA_LENGTH];
		unsigned char		*pucDataPtr = NULL;
	
		unsigned int		uiValueWritten1 = NUMBER_1;
		unsigned int		uiValueWritten2 = NUMBER_2;
		unsigned int		uiValueWritten3 = NUMBER_3;
		unsigned int		uiValueWritten4 = NUMBER_4;
		unsigned int		uiValueRead1 = 0;
		unsigned int		uiValueRead2 = 0;
		unsigned int		uiValueRead3 = 0;
		unsigned int		uiValueRead4 = 0;
		
		unsigned int		uiI = 0;
		unsigned int		uiJ = 0;
	
		unsigned long long	ullTotalIterations = 0;
	
		struct timeval		tvStartTimeVal;
		struct timeval		tvEndTimeVal;
		struct timeval		tvDiffTimeVal;
		double				dMicroSeconds = 0;
		unsigned char		pucNumberString[STRING_LENGTH];
		unsigned char		pucString[STRING_LENGTH];

		
		memset(pucData, DATA_LENGTH, 0);
	
		pucDataPtr = pucData;
		NUM_WRITE_COMPACT_VARINT_QUAD(uiValueWritten1, uiValueWritten2, uiValueWritten3, uiValueWritten4, pucDataPtr);
	
		gettimeofday(&tvStartTimeVal, NULL);
	
		for ( uiI = 0, ullTotalIterations = 0; uiI < REPETITIONS; uiI++ ) {
	
			for ( uiJ = 0, pucDataPtr = pucData; uiJ < ITERATIONS; uiJ++, ullTotalIterations++ ) {
	
				pucDataPtr = pucData;
				NUM_READ_COMPACT_VARINT_QUAD(uiValueRead1, uiValueRead2, uiValueRead3, uiValueRead4, pucDataPtr);
		
#if defined(CHECK_READ)
				if ( uiValueRead1 != uiValueWritten1 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead1, uiValueWritten1);
					exit (-1);
				}
		
				if ( uiValueRead2 != uiValueWritten2 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead2, uiValueWritten2);
					exit (-1);
				}
		
				if ( uiValueRead3 != uiValueWritten3 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead3, uiValueWritten3);
					exit (-1);
				}
		
				if ( uiValueRead4 != uiValueWritten4 ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead4, uiValueWritten4);
					exit (-1);
				}
#endif	/* defined(CHECK_READ) */
			}
		}


		gettimeofday(&tvEndTimeVal, NULL);
	
		UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal);
		UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvDiffTimeVal, dMicroSeconds);
	
		snprintf(pucNumberString, STRING_LENGTH, "%llu", (ullTotalIterations * 4));
		printf("Reading compact varint quad in place, numbers read: %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", dMicroSeconds);
		printf("\tMicroseconds : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", (double)(ullTotalIterations * 4) * ((double)1000000 / dMicroSeconds));
		printf("\tIterations/second : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		printf("\n\n");

	}
#endif	/* defined(COMPACT_VARINT_IN_PLACE) */



#if defined(COMPRESSED_UINT_ACROSS_MEMORY)
	/* Testing compressed uint across memory */
	{
	
	
		unsigned char		*pucData = NULL;
		unsigned char		*pucDataPtr = NULL;

		unsigned int		uiDataLength = ITERATIONS * 16;

		unsigned int		uiI = 0;
		unsigned int		uiJ = 0;

		unsigned long long	ullTotalIterations = 0;
	
		unsigned int		uiValueWritten1 = NUMBER_1;
		unsigned int		uiValueWritten2 = NUMBER_2;
		unsigned int		uiValueWritten3 = NUMBER_3;
		unsigned int		uiValueWritten4 = NUMBER_4;

		unsigned int		uiValueRead1 = 0;
		unsigned int		uiValueRead2 = 0;
		unsigned int		uiValueRead3 = 0;
		unsigned int		uiValueRead4 = 0;

		struct timeval		tvStartTimeVal;
		struct timeval		tvEndTimeVal;
		struct timeval		tvDiffTimeVal;
		double				dMicroSeconds = 0;
		unsigned char		pucNumberString[STRING_LENGTH];
		unsigned char		pucString[STRING_LENGTH];


		if ( (pucData = malloc(uiDataLength)) == NULL ) {
			printf("Failed to allocate memory\n");
			exit (-1);
		}
		
		memset(pucData, 0, uiDataLength);


		gettimeofday(&tvStartTimeVal, NULL);

		for ( uiI = 0, pucDataPtr = pucData; uiI < ITERATIONS; uiI++ ) {
	
			uiValueWritten1 = uiI;
			uiValueWritten2 = uiValueWritten1 + 1;
			uiValueWritten3 = uiValueWritten2 + 1;
			uiValueWritten4 = uiValueWritten3 + 1;

			NUM_WRITE_COMPRESSED_UINT(uiValueWritten1, pucDataPtr);
			NUM_WRITE_COMPRESSED_UINT(uiValueWritten2, pucDataPtr);
			NUM_WRITE_COMPRESSED_UINT(uiValueWritten3, pucDataPtr);
			NUM_WRITE_COMPRESSED_UINT(uiValueWritten4, pucDataPtr);
		
		}
	
		gettimeofday(&tvEndTimeVal, NULL);

		UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal);
		UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvDiffTimeVal, dMicroSeconds);

		snprintf(pucNumberString, STRING_LENGTH, "%llu", (ITERATIONS * 4));
		printf("Writing compressed uint across memory, numbers written: %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%.0f", dMicroSeconds);
		printf("\tMicroseconds : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%.0f", (double)(ITERATIONS * 4) * ((double)1000000 / dMicroSeconds));
		printf("\tIterations/second : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%u", uiDataLength);
		printf("\tBytes, allocated: %s", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
		snprintf(pucNumberString, STRING_LENGTH, "%ld", pucDataPtr - pucData);
		printf(", used %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		printf("\n");
		

		gettimeofday(&tvStartTimeVal, NULL);
	
		for ( uiI = 0, ullTotalIterations = 0; uiI < REPETITIONS; uiI++ ) {

			for ( uiJ = 0, pucDataPtr = pucData; uiJ < ITERATIONS; uiJ++, ullTotalIterations++ ) {

				NUM_READ_COMPRESSED_UINT(uiValueRead1, pucDataPtr);
				NUM_READ_COMPRESSED_UINT(uiValueRead2, pucDataPtr);
				NUM_READ_COMPRESSED_UINT(uiValueRead3, pucDataPtr);
				NUM_READ_COMPRESSED_UINT(uiValueRead4, pucDataPtr);

#if defined(CHECK_READ)
				if ( uiValueRead1 != uiJ ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead1, uiJ);
					exit (-1);
				}
	
				if ( uiValueRead2 != (uiJ + 1) ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead2, uiJ + 1);
					exit (-1);
				}
	
				if ( uiValueRead3 != (uiJ + 2) ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead3, uiJ + 2);
					exit (-1);
				}
	
				if ( uiValueRead4 != (uiJ + 3) ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead4, uiJ + 3);
					exit (-1);
				}
#endif	/* defined(CHECK_READ) */
			}
		}
	
		gettimeofday(&tvEndTimeVal, NULL);
	
		UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal);
		UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvDiffTimeVal, dMicroSeconds);
	
		snprintf(pucNumberString, STRING_LENGTH, "%llu", (ullTotalIterations * 4));
		printf("Reading compressed uint across memory, numbers read: %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", dMicroSeconds);
		printf("\tMicroseconds : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", (double)(ullTotalIterations * 4) * ((double)1000000 / dMicroSeconds));
		printf("\tIterations/second : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		printf("\n\n");

	}
#endif	/* defined(COMPRESSED_UINT_IN_PLACE) */



#if defined(VARINT_ACROSS_MEMORY)
	/* Testing varint across memory */
	{
	
		unsigned char		*pucData = NULL;
		unsigned char		*pucDataPtr = NULL;

		unsigned int		uiDataLength = ITERATIONS * 16;

		unsigned int		uiI = 0;
		unsigned int		uiJ = 0;

		unsigned long long	ullTotalIterations = 0;
	
		unsigned int		uiValueWritten1 = NUMBER_1;
		unsigned int		uiValueWritten2 = NUMBER_2;
		unsigned int		uiValueWritten3 = NUMBER_3;
		unsigned int		uiValueWritten4 = NUMBER_4;

		unsigned int		uiValueRead1 = 0;
		unsigned int		uiValueRead2 = 0;
		unsigned int		uiValueRead3 = 0;
		unsigned int		uiValueRead4 = 0;

		unsigned int		uiValueRead = 0;

		struct timeval		tvStartTimeVal;
		struct timeval		tvEndTimeVal;
		struct timeval		tvDiffTimeVal;
		double				dMicroSeconds = 0;
		unsigned char		pucNumberString[STRING_LENGTH];
		unsigned char		pucString[STRING_LENGTH];


		if ( (pucData = malloc(uiDataLength)) == NULL ) {
			printf("Failed to allocate memory\n");
			exit (-1);
		}

		memset(pucData, 0, uiDataLength);


		gettimeofday(&tvStartTimeVal, NULL);

		for ( uiI = 0, pucDataPtr = pucData; uiI < ITERATIONS; uiI++ ) {
	
			uiValueWritten1 = uiI;
			uiValueWritten2 = uiValueWritten1 + 1;
			uiValueWritten3 = uiValueWritten2 + 1;
			uiValueWritten4 = uiValueWritten3 + 1;

			NUM_WRITE_VARINT_QUAD(uiValueWritten1, uiValueWritten2, uiValueWritten3, uiValueWritten4, pucDataPtr);

		}
	
		gettimeofday(&tvEndTimeVal, NULL);

		UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal);
		UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvDiffTimeVal, dMicroSeconds);

		snprintf(pucNumberString, STRING_LENGTH, "%llu", (ITERATIONS * 4));
		printf("Writing varint across memory, numbers written: %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%.0f", dMicroSeconds);
		printf("\tMicroseconds : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%.0f", (double)(ITERATIONS * 4) * ((double)1000000 / dMicroSeconds));
		printf("\tIterations/second : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%u", uiDataLength);
		printf("\tBytes, allocated: %s", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
		snprintf(pucNumberString, STRING_LENGTH, "%ld", pucDataPtr - pucData);
		printf(", used %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		printf("\n");


		gettimeofday(&tvStartTimeVal, NULL);
	
		for ( uiI = 0, ullTotalIterations = 0; uiI < REPETITIONS; uiI++ ) {
	
			for ( uiJ = 0, pucDataPtr = pucData; uiJ < ITERATIONS; uiJ++, ullTotalIterations++ ) {
	
				NUM_READ_VARINT_QUAD(uiValueRead1, uiValueRead2, uiValueRead3, uiValueRead4, pucDataPtr);
	
#if defined(CHECK_READ)
				if ( uiValueRead1 != uiJ ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead1, uiJ);
					exit (-1);
				}
	
				if ( uiValueRead2 != (uiJ + 1) ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead2, uiJ + 1);
					exit (-1);
				}
	
				if ( uiValueRead3 != (uiJ + 2) ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead3, uiJ + 2);
					exit (-1);
				}
	
				if ( uiValueRead4 != (uiJ + 3) ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead4, uiJ + 3);
					exit (-1);
				}
#endif	/* defined(CHECK_READ) */
			}
		}

	
		gettimeofday(&tvEndTimeVal, NULL);

		UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal);
		UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvDiffTimeVal, dMicroSeconds);
	
		snprintf(pucNumberString, STRING_LENGTH, "%llu", (ullTotalIterations * 4));
		printf("Reading varint across memory, numbers read: %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", dMicroSeconds);
		printf("\tMicroseconds : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", (double)(ullTotalIterations * 4) * ((double)1000000 / dMicroSeconds));
		printf("\tIterations/second : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		printf("\n\n");
	
	}
#endif	/* defined(VARINT_ACROSS_MEMORY) */



#if defined(COMPACT_VARINT_ACROSS_MEMORY)
	/* Testing compact varint across memory */
	{
	
		unsigned char		*pucData = NULL;
		unsigned char		*pucDataPtr = NULL;

		unsigned int		uiDataLength = ITERATIONS * 16;

		unsigned int		uiI = 0;
		unsigned int		uiJ = 0;

		unsigned long long	ullTotalIterations = 0;
	
		unsigned int		uiValueWritten1 = NUMBER_1;
		unsigned int		uiValueWritten2 = NUMBER_2;
		unsigned int		uiValueWritten3 = NUMBER_3;
		unsigned int		uiValueWritten4 = NUMBER_4;

		unsigned int		uiValueRead1 = 0;
		unsigned int		uiValueRead2 = 0;
		unsigned int		uiValueRead3 = 0;
		unsigned int		uiValueRead4 = 0;

		unsigned int		uiValueRead = 0;

		struct timeval		tvStartTimeVal;
		struct timeval		tvEndTimeVal;
		struct timeval		tvDiffTimeVal;
		double				dMicroSeconds = 0;
		unsigned char		pucNumberString[STRING_LENGTH];
		unsigned char		pucString[STRING_LENGTH];


		if ( (pucData = malloc(uiDataLength)) == NULL ) {
			printf("Failed to allocate memory\n");
			exit (-1);
		}

		memset(pucData, 0, uiDataLength);


		gettimeofday(&tvStartTimeVal, NULL);

		for ( uiI = 0, pucDataPtr = pucData; uiI < ITERATIONS; uiI++ ) {
	
			uiValueWritten1 = uiI;
			uiValueWritten2 = uiValueWritten1 + 1;
			uiValueWritten3 = uiValueWritten2 + 1;
			uiValueWritten4 = uiValueWritten3 + 1;

			NUM_WRITE_COMPACT_VARINT_QUAD(uiValueWritten1, uiValueWritten2, uiValueWritten3, uiValueWritten4, pucDataPtr);

		}
	
		gettimeofday(&tvEndTimeVal, NULL);

		UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal);
		UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvDiffTimeVal, dMicroSeconds);

		snprintf(pucNumberString, STRING_LENGTH, "%llu", (ITERATIONS * 4));
		printf("Writing compact varint across memory, numbers written: %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%.0f", dMicroSeconds);
		printf("\tMicroseconds : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%.0f", (double)(ITERATIONS * 4) * ((double)1000000 / dMicroSeconds));
		printf("\tIterations/second : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		snprintf(pucNumberString, STRING_LENGTH, "%u", uiDataLength);
		printf("\tBytes, allocated: %s", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
		snprintf(pucNumberString, STRING_LENGTH, "%ld", pucDataPtr - pucData);
		printf(", used %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));

		printf("\n");


		gettimeofday(&tvStartTimeVal, NULL);
	
		for ( uiI = 0, ullTotalIterations = 0; uiI < REPETITIONS; uiI++ ) {
	
			for ( uiJ = 0, pucDataPtr = pucData; uiJ < ITERATIONS; uiJ++, ullTotalIterations++ ) {
	
				NUM_READ_COMPACT_VARINT_QUAD(uiValueRead1, uiValueRead2, uiValueRead3, uiValueRead4, pucDataPtr);
	
#if defined(CHECK_READ)
				if ( uiValueRead1 != uiJ ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead1, uiJ);
					exit (-1);
				}
	
				if ( uiValueRead2 != (uiJ + 1) ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead2, uiJ + 1);
					exit (-1);
				}
	
				if ( uiValueRead3 != (uiJ + 2) ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead3, uiJ + 2);
					exit (-1);
				}
	
				if ( uiValueRead4 != (uiJ + 3) ) {
					printf("Failed, read: %u, expected: %u.\n", uiValueRead4, uiJ + 3);
					exit (-1);
				}
#endif	/* defined(CHECK_READ) */
			}
		}

	
		gettimeofday(&tvEndTimeVal, NULL);

		UTL_DATE_DIFF_TIMEVAL(tvStartTimeVal, tvEndTimeVal, tvDiffTimeVal);
		UTL_DATE_TIMEVAL_TO_MICROSECONDS(tvDiffTimeVal, dMicroSeconds);
	
		snprintf(pucNumberString, STRING_LENGTH, "%llu", (ullTotalIterations * 4));
		printf("Reading compact varint across memory, numbers read: %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", dMicroSeconds);
		printf("\tMicroseconds : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		snprintf(pucNumberString, STRING_LENGTH, "%.0f", (double)(ullTotalIterations * 4) * ((double)1000000 / dMicroSeconds));
		printf("\tIterations/second : %s\n", pucUtlStringsFormatIntegerString(pucNumberString, pucString, STRING_LENGTH));
	
		printf("\n\n");
	
	}
#endif	/* defined(COMPACT_VARINT_ACROSS_MEMORY) */



#if defined(TEST_VARINT_1)
	/* Sanity test 1 */
	{

		unsigned char	pucData[DATA_LENGTH];
		unsigned char	*pucDataPtr = NULL;

		unsigned int	uiI = 0;

		unsigned int	uiValueWritten = 0;
		unsigned int	uiValueRead = 0;

		unsigned int	uiSize = 0;
		

		memset(pucData, DATA_LENGTH, 0);

		for ( uiI = 0; uiI < ITERATIONS; uiI++ ) {

			uiValueWritten = uiI;
	
			pucDataPtr = pucData;
			NUM_GET_VARINT_SIZE(uiValueWritten, uiSize);
			NUM_WRITE_VARINT(uiValueWritten, uiSize, pucDataPtr);

			pucDataPtr = pucData;
			NUM_READ_VARINT(uiValueRead, uiSize, pucDataPtr);

			if ( uiValueRead != uiValueWritten ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead, uiValueWritten);
				exit (-1);
			}
		}

		printf("Sanity test 1 complete\n\n");

	}
#endif	/* defined(TEST_VARINT_1) */



#if defined(TEST_VARINT_2)
	/* Sanity test 2 */
	{

		unsigned char	pucData[DATA_LENGTH];
		unsigned char	*pucDataPtr = NULL;

		unsigned int	uiI = 0;

		unsigned int	uiValueWritten1 = 0;
		unsigned int	uiValueWritten2 = 0;
		unsigned int	uiValueWritten3 = 0;
		unsigned int	uiValueWritten4 = 0;

		unsigned int	uiValueRead1 = 0;
		unsigned int	uiValueRead2 = 0;
		unsigned int	uiValueRead3 = 0;
		unsigned int	uiValueRead4 = 0;

		
		memset(pucData, DATA_LENGTH, 0);

		for ( uiI = 0; uiI < ITERATIONS; uiI++ ) {

			uiValueWritten1 = uiI;
			uiValueWritten2 = uiValueWritten1 + 1;
			uiValueWritten3 = uiValueWritten2 + 1;
			uiValueWritten4 = uiValueWritten3 + 1;

			pucDataPtr = pucData;
			NUM_WRITE_VARINT_QUAD(uiValueWritten1, uiValueWritten2, uiValueWritten3, uiValueWritten4, pucDataPtr);

			pucDataPtr = pucData;
			NUM_READ_VARINT_QUAD(uiValueRead1, uiValueRead2, uiValueRead3, uiValueRead4, pucDataPtr);

			if ( uiValueRead1 != uiValueWritten1 ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead1, uiValueWritten1);
				exit (-1);
			}

			if ( uiValueRead2 != uiValueWritten2 ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead2, uiValueWritten2);
				exit (-1);
			}

			if ( uiValueRead3 != uiValueWritten3 ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead3, uiValueWritten3);
				exit (-1);
			}

			if ( uiValueRead4 != uiValueWritten4 ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead4, uiValueWritten4);
				exit (-1);
			}
		}

		printf("Sanity test 2 complete\n\n");

	}
#endif	/* defined(TEST_VARINT_2) */



#if defined(TEST_COMPACT_VARINT_1)
	/* Sanity test 1 */
	{

		unsigned char	pucData[DATA_LENGTH];
		unsigned char	*pucDataPtr = NULL;

		unsigned int	uiI = 0;

		unsigned int	uiValueWritten = 0;
		unsigned int	uiValueRead = 0;

		unsigned int	uiSize = 0;
		

		printf("Sanity test 1\n\n");

		memset(pucData, DATA_LENGTH, 0);

		for ( uiI = 0; uiI < ITERATIONS; uiI++ ) {

			uiValueWritten = uiI;
	
			pucDataPtr = pucData;
			NUM_GET_COMPACT_VARINT_SIZE(uiValueWritten, uiSize);
			NUM_WRITE_COMPACT_VARINT(uiValueWritten, uiSize, pucDataPtr);

			pucDataPtr = pucData;
			NUM_READ_COMPACT_VARINT(uiValueRead, uiSize, pucDataPtr);

			if ( uiValueRead != uiValueWritten ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead, uiValueWritten);
				exit (-1);
			}
		}

		printf("Sanity test 1 complete\n\n");

	}
#endif	/* defined(TEST_COMPACT_VARINT_1) */



#if defined(TEST_COMPACT_VARINT_2)
	/* Sanity test 2 */
	{

		unsigned char	pucData[DATA_LENGTH];
		unsigned char	*pucDataPtr = NULL;

		unsigned int	uiI = 0;

		unsigned int	uiValueWritten1 = 0;
		unsigned int	uiValueWritten2 = 0;
		unsigned int	uiValueWritten3 = 0;
		unsigned int	uiValueWritten4 = 0;

		unsigned int	uiValueRead1 = 0;
		unsigned int	uiValueRead2 = 0;
		unsigned int	uiValueRead3 = 0;
		unsigned int	uiValueRead4 = 0;

		
		printf("Sanity test 2\n\n");

		memset(pucData, DATA_LENGTH, 0);

		for ( uiI = 0; uiI < ITERATIONS; uiI++ ) {

			uiValueWritten1 = uiI;
			uiValueWritten2 = uiValueWritten1 + 1;
			uiValueWritten3 = uiValueWritten2 + 1;
			uiValueWritten4 = uiValueWritten3 + 1;

			pucDataPtr = pucData;
			NUM_WRITE_COMPACT_VARINT_QUAD(uiValueWritten1, uiValueWritten2, uiValueWritten3, uiValueWritten4, pucDataPtr);

			pucDataPtr = pucData;
			NUM_READ_COMPACT_VARINT_QUAD(uiValueRead1, uiValueRead2, uiValueRead3, uiValueRead4, pucDataPtr);

			if ( uiValueRead1 != uiValueWritten1 ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead1, uiValueWritten1);
				exit (-1);
			}

			if ( uiValueRead2 != uiValueWritten2 ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead2, uiValueWritten2);
				exit (-1);
			}

			if ( uiValueRead3 != uiValueWritten3 ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead3, uiValueWritten3);
				exit (-1);
			}

			if ( uiValueRead4 != uiValueWritten4 ) {
				printf("Failed, read: %u, expected: %u.\n", uiValueRead4, uiValueWritten4);
				exit (-1);
			}
		}

		printf("Sanity test 2 complete\n\n");

	}
#endif	/* defined(TEST_COMPACT_VARINT_2) */


	printf("\n\n");
	exit(0);

}


/*---------------------------------------------------------------------------*/


