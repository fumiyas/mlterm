/*
 *	$Id$
 */

#include  "mkf_jisx0213_2000_property.h"


/* --- static variables --- */

static mkf_property_t  jisx0213_2000_1_property_table[] =
{
	/* 0x2b52 */
	MKF_COMBINING ,	/* COMBINING DOUBLE INVERTED BREVE */
	0 ,
	0 ,
	0 ,
	0 ,
	MKF_COMBINING ,	/* COMBINING BREVE */
	0 ,
	MKF_COMBINING , /* COMBINING DOUBLE ACUTE ACCENT */
	MKF_COMBINING , /* COMBINING ACUTE ACCENT */
	MKF_COMBINING , /* COMBINING MACRON */
	MKF_COMBINING , /* COMBINING GRAVE ACCENT */
	MKF_COMBINING , /* COMBINING DOUBLE GRAVE ACCENT */
	MKF_COMBINING , /* COMBINING CARON */
	MKF_COMBINING , /* COMBINING CIRCUMFLEX ACCENT */

	/* 0x2b60 */
	0 ,
	0 ,
	0 ,
	0 ,
	0 ,
	0 ,
	0 ,
	MKF_COMBINING , /* COMBINING RING BELOW */
	MKF_COMBINING , /* COMBINING CARON BELOW */
	MKF_COMBINING , /* COMBINING RIGHT HALF RING BELOW */
	MKF_COMBINING , /* COMBINING LEFT HALF RING BELOW */
	MKF_COMBINING , /* COMBINING PLUS SIGN BELOW */
	MKF_COMBINING , /* COMBINING MINUS SIGN BELOW */
	MKF_COMBINING , /* COMBINING DIAERESIS */
	MKF_COMBINING , /* COMBINING X ABOVE */
	MKF_COMBINING , /* COMBINING VERTICAL LINE BELOW */

	/* 0x2b70 */
	MKF_COMBINING , /* COMBINING INVERTED BREVE BELOW */
	0 ,
	MKF_COMBINING , /* COMBINING DIAERESIS BELOW */
	MKF_COMBINING , /* COMBINING TILDE BELOW */
	MKF_COMBINING , /* COMBINING SEAGULL BELOW */
	MKF_COMBINING , /* COMBINING TILDE OVERLAY */
	MKF_COMBINING , /* COMBINING UP TACK BELOW */
	MKF_COMBINING , /* COMBINING DOWN TACK BELOW */
	MKF_COMBINING , /* COMBINING LEFT TACK BELOW */
	MKF_COMBINING , /* COMBINING RIGHT TACK BELOW */
	MKF_COMBINING , /* COMBINING BRIDGE BELOW */
	MKF_COMBINING , /* COMBINING INVERTED BRIDGE BELOW */
	MKF_COMBINING , /* COMBINING SQUARE BELOW */
	MKF_COMBINING , /* COMBINING TILDE */
	MKF_COMBINING , /* COMBINING LEFT ANGLE ABOVE */
} ;


/* --- global functions --- */

mkf_property_t
mkf_get_jisx0213_2000_1_property(
	u_char *  ch ,
	size_t  size
	)
{
	if( ch[0] == 0x2b)
	{
		if( 0x52 <= ch[1] && ch[1] <= 0x7e)
		{
			return  jisx0213_2000_1_property_table[ ch[1] - 0x52] ;
		}
	}

	return  0 ;
}
