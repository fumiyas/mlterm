/*
 *	$Id$
 */

#include  "mkf_ucs4_map.h"

#include  <string.h>
#include  <kiklib/kik_debug.h>

#include  "mkf_ucs4_usascii.h"
#include  "mkf_ucs4_iso8859.h"
#include  "mkf_ucs4_viscii.h"
#include  "mkf_ucs4_koi8.h"
#include  "mkf_ucs4_jisx0201.h"
#include  "mkf_ucs4_jisx0208.h"
#include  "mkf_ucs4_jisx0212.h"
#include  "mkf_ucs4_jisx0213.h"
#include  "mkf_ucs4_ksc5601.h"
#include  "mkf_ucs4_uhc.h"
#include  "mkf_ucs4_johab.h"
#include  "mkf_ucs4_gb2312.h"
#include  "mkf_ucs4_gbk.h"
#include  "mkf_ucs4_big5.h"
#include  "mkf_ucs4_cns11643.h"


typedef struct  map
{
	mkf_charset_t  cs ;
	int  (*map_ucs4_to)( mkf_char_t * , u_int32_t) ;
	int  (*map_to_ucs4)( mkf_char_t * , u_int16_t) ;
	
} map_t ;


/* --- static variables --- */

static map_t  map_table[] =
{
	/* 35 converters are registed. */
	
	{ US_ASCII , mkf_map_ucs4_to_us_ascii , mkf_map_us_ascii_to_ucs4 } ,
	{ ISO8859_1_R , mkf_map_ucs4_to_iso8859_1_r , mkf_map_iso8859_1_r_to_ucs4 } ,
	{ ISO8859_2_R , mkf_map_ucs4_to_iso8859_2_r , mkf_map_iso8859_2_r_to_ucs4 } ,
	{ ISO8859_3_R , mkf_map_ucs4_to_iso8859_3_r , mkf_map_iso8859_3_r_to_ucs4 } ,
	{ ISO8859_4_R , mkf_map_ucs4_to_iso8859_4_r , mkf_map_iso8859_4_r_to_ucs4 } ,
	{ ISO8859_5_R , mkf_map_ucs4_to_iso8859_5_r , mkf_map_iso8859_5_r_to_ucs4 } ,
	{ ISO8859_6_R , mkf_map_ucs4_to_iso8859_6_r , mkf_map_iso8859_6_r_to_ucs4 } ,
	{ ISO8859_7_R , mkf_map_ucs4_to_iso8859_7_r , mkf_map_iso8859_7_r_to_ucs4 } ,
	{ ISO8859_8_R , mkf_map_ucs4_to_iso8859_8_r , mkf_map_iso8859_8_r_to_ucs4 } ,
	{ ISO8859_9_R , mkf_map_ucs4_to_iso8859_9_r , mkf_map_iso8859_9_r_to_ucs4 } ,
	{ ISO8859_10_R , mkf_map_ucs4_to_iso8859_10_r , mkf_map_iso8859_10_r_to_ucs4 } ,
	{ TIS620_2533 , mkf_map_ucs4_to_tis620_2533 , mkf_map_tis620_2533_to_ucs4 } ,
	{ ISO8859_13_R , mkf_map_ucs4_to_iso8859_13_r , mkf_map_iso8859_13_r_to_ucs4 } ,
	{ ISO8859_14_R , mkf_map_ucs4_to_iso8859_14_r , mkf_map_iso8859_14_r_to_ucs4 } ,
	{ ISO8859_15_R , mkf_map_ucs4_to_iso8859_15_r , mkf_map_iso8859_15_r_to_ucs4 } ,
	{ ISO8859_16_R , mkf_map_ucs4_to_iso8859_16_r , mkf_map_iso8859_16_r_to_ucs4 } ,
	{ TCVN5712_3_1993 , mkf_map_ucs4_to_tcvn5712_3_1993 , mkf_map_tcvn5712_3_1993_to_ucs4 } ,
	
	{ VISCII , mkf_map_ucs4_to_viscii , mkf_map_viscii_to_ucs4 } ,
	{ KOI8_R , mkf_map_ucs4_to_koi8_r , mkf_map_koi8_r_to_ucs4 } ,
	{ KOI8_U , mkf_map_ucs4_to_koi8_u , mkf_map_koi8_u_to_ucs4 } ,
	
	{ JISX0201_ROMAN , mkf_map_ucs4_to_jisx0201_roman , mkf_map_jisx0201_roman_to_ucs4 } ,
	{ JISX0201_KATA , mkf_map_ucs4_to_jisx0201_kata , mkf_map_jisx0201_kata_to_ucs4 } ,
	{ JISX0208_1983 , mkf_map_ucs4_to_jisx0208_1983 , mkf_map_jisx0208_1983_to_ucs4 } ,
	{ JISX0212_1990 , mkf_map_ucs4_to_jisx0212_1990 , mkf_map_jisx0212_1990_to_ucs4 } ,
	{ JISX0213_2000_1 , mkf_map_ucs4_to_jisx0213_2000_1 , mkf_map_jisx0213_2000_1_to_ucs4 } ,
	{ JISX0213_2000_2 , mkf_map_ucs4_to_jisx0213_2000_2 , mkf_map_jisx0213_2000_2_to_ucs4 } ,
	{ JISC6226_1978_NEC_EXT , mkf_map_ucs4_to_nec_ext , mkf_map_nec_ext_to_ucs4 } ,
	
	{ GB2312_80 , mkf_map_ucs4_to_gb2312_80 , mkf_map_gb2312_80_to_ucs4 } ,
	{ GBK , mkf_map_ucs4_to_gbk , mkf_map_gbk_to_ucs4 } ,
	
	{ CNS11643_1992_1 , mkf_map_ucs4_to_cns11643_1992_1 , mkf_map_cns11643_1992_1_to_ucs4 } ,
	{ CNS11643_1992_2 , mkf_map_ucs4_to_cns11643_1992_2 , mkf_map_cns11643_1992_2_to_ucs4 } ,
	{ BIG5 , mkf_map_ucs4_to_big5 , mkf_map_big5_to_ucs4 } ,

	{ KSC5601_1987 , mkf_map_ucs4_to_ksc5601_1987 , mkf_map_ksc5601_1987_to_ucs4 } ,
	{ UHC , mkf_map_ucs4_to_uhc , mkf_map_uhc_to_ucs4 } ,
	{ JOHAB , mkf_map_ucs4_to_johab , mkf_map_johab_to_ucs4 } ,
	
} ;


/* --- global functions --- */

int
mkf_map_ucs4_to_cs(
	mkf_char_t *  non_ucs ,
	mkf_char_t *  ucs4 ,
	mkf_charset_t  cs
	)
{
	int  counter ;
	u_int32_t  ucs4_code ;

	if( ucs4->cs != ISO10646_UCS4_1)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( ucs4) ;
	
	for( counter = 0 ; counter < sizeof( map_table) / sizeof( map_t) ; counter ++)
	{
		if( map_table[counter].cs == cs)
		{
			if( (*map_table[counter].map_ucs4_to)( non_ucs , ucs4_code))
			{
				return  1 ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" UCS4 char(0x%.2x%.2x%.2x%.2x) is not supported.\n" ,
					ucs4->ch[0] , ucs4->ch[1] , ucs4->ch[2] , ucs4->ch[3]) ;
			#endif

				return  0 ;
			}
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " %x cs is not supported to map to ucs4.\n" , cs) ;
#endif
	
	return  0 ;
}

int
mkf_map_ucs4_to_with_funcs(
	mkf_char_t *  non_ucs ,
	mkf_char_t *  ucs4 ,
	mkf_map_ucs4_to_func_t *  map_ucs4_to_funcs ,
	size_t  list_size
	)
{
	int  counter ;
	u_int32_t  ucs4_code ;

	if( ucs4->cs != ISO10646_UCS4_1)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( ucs4) ;
	
	for( counter = 0 ; counter < list_size ; counter ++)
	{
		if( (*map_ucs4_to_funcs[counter])( non_ucs , ucs4_code))
		{
			return  1 ;
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " UCS4 char(0x%.2x%.2x%.2x%.2x) is not supported.\n" ,
		ucs4->ch[0] , ucs4->ch[1] , ucs4->ch[2] , ucs4->ch[3]) ;
#endif

	return  0 ;
}

/*
 * using the default order of the mapping table.
 */
int
mkf_map_ucs4_to(
	mkf_char_t *  non_ucs ,
	mkf_char_t *  ucs4
	)
{
	int  counter ;
	u_int32_t  ucs4_code ;

	if( ucs4->cs != ISO10646_UCS4_1)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( ucs4) ;

	for( counter = 0 ; counter < sizeof( map_table) / sizeof( map_table[0]) ;
		counter ++)
	{
		if( (*map_table[counter].map_ucs4_to)( non_ucs , ucs4_code))
		{
			return  1 ;
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " UCS4 char(0x%.2x%.2x%.2x%.2x) is not supported.\n" ,
		ucs4->ch[0] , ucs4->ch[1] , ucs4->ch[2] , ucs4->ch[3]) ;
#endif

	return  0 ;
}

/*
 * using the default order of the mapping table.
 */
int
mkf_map_ucs4_to_iso2022cs(
	mkf_char_t *  non_ucs ,
	mkf_char_t *  ucs4
	)
{
	int  counter ;
	u_int32_t  ucs4_code ;

	if( ucs4->cs != ISO10646_UCS4_1)
	{
		return  0 ;
	}

	ucs4_code = mkf_char_to_int( ucs4) ;

	for( counter = 0 ; counter < sizeof( map_table) / sizeof( map_table[0]) ;
		counter ++)
	{
		if( IS_CS_BASED_ON_ISO2022(map_table[counter].cs))
		{
			if( (*map_table[counter].map_ucs4_to)( non_ucs , ucs4_code))
			{
				return  1 ;
			}
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " UCS4 char(0x%.2x%.2x%.2x%.2x) is not supported.\n" ,
		ucs4->ch[0] , ucs4->ch[1] , ucs4->ch[2] , ucs4->ch[3]) ;
#endif

	return  0 ;
}

int
mkf_map_to_ucs4(
	mkf_char_t *  ucs4 ,
	mkf_char_t *  non_ucs
	)
{
	int  counter ;
	u_int32_t  code ;
	
	code = mkf_char_to_int( non_ucs) ;
	
	for( counter = 0 ; counter < sizeof( map_table) / sizeof( map_t) ; counter ++)
	{
		if( map_table[counter].cs == non_ucs->cs)
		{
			if( (*map_table[counter].map_to_ucs4)( ucs4 , code))
			{
				return  1 ;
			}
		}
	}
	
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " this cs(%x) (code %x) cannot be mapped to UCS4.\n" ,
		non_ucs->cs , code) ;
#endif

	return  0 ;
}

