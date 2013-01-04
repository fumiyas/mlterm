/** @file
 *  @brief image handling functions using gdk-pixbuf
 *	$Id$
 */

#include  "../x_imagelib.h"

/*
 * <Xutil.h> might not include <Xlib.h> internally in some environments
 * (e.g. open window in Solaris 2.6), so <X11/Xlib.h> is necessary.
 * SF Bug 350944.
 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>		/* XInternAtom */
#include <X11/Xutil.h>
#include <string.h>		/* memcpy */
#include <stdio.h>		/* sscanf */
#ifdef  BUILTIN_IMAGELIB
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif	/* BUILTIN_IMAGELIB */
#ifdef  DLOPEN_LIBM
#include <kiklib/kik_dlfcn.h>	/* dynamically loading pow */
#else
#include <math.h>		/* pow */
#endif

#include <kiklib/kik_debug.h>
#include <kiklib/kik_types.h>	/* u_int32_t/u_int16_t */
#include <kiklib/kik_def.h>	/* SSIZE_MAX */
#include <kiklib/kik_unistd.h>
#include <kiklib/kik_str.h>	/* strdup */
#include <kiklib/kik_util.h>	/* DIGIT_STR_LEN */

#include "x_display.h"		/* x_display_get_visual_info */


/*
 * 'data' which is malloc'ed for XCreateImage() in pixbuf_to_ximage_truecolor()
 * is free'ed in XDestroyImage().
 * If malloc is replaced kik_mem_malloc in kik_mem.h, kik_mem_free_all() will
 * free 'data' which is already free'ed in XDestroyImage() and
 * segmentation fault error can happen.
 */
#if  defined(KIK_DEBUG) && defined(malloc)
#undef malloc
#endif

#if  1
#define  USE_FS
#endif

#if  0
#define  ENABLE_CARD2PIXBUF
#endif

#if  (GDK_PIXBUF_MAJOR < 2)
#define  g_object_ref( pixbuf) gdk_pixbuf_ref( pixbuf)
#define  g_object_unref( pixbuf) gdk_pixbuf_unref( pixbuf)
#endif

/* Trailing "/" is appended in value_table_refresh(). */
#ifndef  LIBMDIR
#define  LIBMDIR  "/lib"
#endif

#ifndef  LIBEXECDIR
#define  LIBEXECDIR  "/usr/local/libexec"
#endif

#if  0
#define  __DEBUG
#endif

#define  PIXEL_RED(pixel,rgbinfo) \
	((((pixel) & (rgbinfo).r_mask) >> (rgbinfo).r_offset) << (rgbinfo).r_limit)
#define  PIXEL_BLUE(pixel,rgbinfo) \
	((((pixel) & (rgbinfo).b_mask) >> (rgbinfo).b_offset) << (rgbinfo).b_limit)
#define  PIXEL_GREEN(pixel,rgbinfo) \
	((((pixel) & (rgbinfo).g_mask) >> (rgbinfo).g_offset) << (rgbinfo).g_limit)
#define  RGB_TO_PIXEL(r,g,b,rgbinfo) \
	(((((r) >> (rgbinfo).r_limit) << (rgbinfo).r_offset) & (rgbinfo).r_mask) | \
	 ((((g) >> (rgbinfo).g_limit) << (rgbinfo).g_offset) & (rgbinfo).g_mask) | \
	 ((((b) >> (rgbinfo).b_limit) << (rgbinfo).b_offset) & (rgbinfo).b_mask) )

#if  GDK_PIXBUF_MAJOR >= 2 && GDK_PIXBUF_MINOR >= 14
#ifndef  G_PLATFORM_WIN32
GInputStream * g_unix_input_stream_new( gint fd , gboolean close_fd) ;
#endif
#endif


typedef struct  rgb_info
{
	u_long  r_mask ;
	u_long  g_mask ;
	u_long  b_mask ;
	u_int  r_limit ;
	u_int  g_limit ;
	u_int  b_limit ;
	u_int  r_offset ;
	u_int  g_offset ;
	u_int  b_offset ;

} rgb_info_t ;


/* --- static variables --- */

static int  display_count = 0 ;


/* --- static functions --- */

#define  USE_X11	/* Necessary to use closest_color_index(), lsb() and msb() */
#include  "../../common/c_imagelib.c"

static Status
get_drawable_size(
	Display *  display ,
	Drawable  drawable ,
	u_int *  width ,
	u_int *  height
	)
{
	Window  root ;
	int  x ;
	int  y ;
	u_int  border ;
	u_int  depth ;
	
	return  XGetGeometry( display , drawable , &root , &x , &y , width , height ,
			&border , &depth) ;
}

/* returned cmap shuold be freed by the caller */
static int
fetch_colormap(
	x_display_t *  disp ,
	XColor **  color_list
	)
{
	int  num_cells , i ;

	num_cells = disp->visual->map_entries ;

	if( ( *color_list = calloc( num_cells , sizeof(XColor))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf(KIK_DEBUG_TAG "couldn't allocate color table\n") ;
	#endif
		return  0 ;
	}

	for( i = 0 ; i < num_cells ; i ++)
	{
		((*color_list)[i]).pixel = i ;
	}

	XQueryColors( disp->display , disp->colormap , *color_list , num_cells) ;

	return  num_cells ;
}

/* Get an background pixmap from _XROOTMAP_ID */
static Pixmap
root_pixmap(
	x_display_t *  disp
	)
{
	Atom  id ;
	int  act_format ;
	u_long  nitems ;
	u_long  bytes_after ;
	u_char *  prop ;
	Pixmap  pixmap ;

	if( ! ( id = XInternAtom( disp->display , "_XROOTPMAP_ID" , False)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " _XROOTPMAP_ID atom is not available.\n") ;
	#endif

		return  None ;
	}

	if( XGetWindowProperty( disp->display , disp->my_window , id , 0 , 1 , False , XA_PIXMAP ,
		&id , &act_format , &nitems , &bytes_after , &prop) != Success || ! prop)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " Failed to read prop\n") ;
	#endif

		return  None ;
	}

	pixmap = *((Drawable *)prop) ;
	XFree( prop) ;

	return  pixmap ;
}

static void
rgb_info_init(
	XVisualInfo *  vinfo ,
	rgb_info_t *  rgb
	)
{
	rgb->r_mask = vinfo->red_mask ;
	rgb->g_mask = vinfo->green_mask ;
	rgb->b_mask = vinfo->blue_mask ;

	rgb->r_offset = lsb( rgb->r_mask) ;
	rgb->g_offset = lsb( rgb->g_mask) ;
	rgb->b_offset = lsb( rgb->b_mask) ;

	rgb->r_limit = 8 + rgb->r_offset - msb( rgb->r_mask) ;
	rgb->g_limit = 8 + rgb->g_offset - msb( rgb->g_mask) ;
	rgb->b_limit = 8 + rgb->b_offset - msb( rgb->b_mask) ;
}

#include  <dlfcn.h>

static void
value_table_refresh(
	u_char *  value_table ,		/* 256 bytes */
	x_picture_modifier_t *  mod
	)
{
	int i , tmp ;
	double real_gamma , real_brightness , real_contrast ;
	static double (*pow_func)( double , double) ;

	real_gamma = (double)(mod->gamma) / 100 ;
	real_contrast = (double)(mod->contrast) / 100 ;
	real_brightness = (double)(mod->brightness) / 100 ;

	if( ! pow_func)
	{
	#ifdef  DLOPEN_LIBM
		kik_dl_handle_t  handle ;

		if( ( ! ( handle = kik_dl_open( LIBMDIR "/" , "m")) &&
		      ! ( handle = kik_dl_open( "" , "m"))) ||
		    ! ( pow_func = kik_dl_func_symbol( handle , "pow")))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Failed to load pow in "
				LIBMDIR "/libm.so\n") ;
		#endif

			if( handle)
			{
				kik_dl_close( handle) ;
			}

			/*
			 * gamma, contrast and brightness options are ignored.
			 * (alpha option still survives.)
			 */
			for( i = 0 ; i < 256 ; i++)
			{
				value_table[i] = i ;
			}

			return ;
		}
	#else  /* DLOPEN_LIBM */
		pow_func = pow ;
	#endif /* BUILTIN_IMAGELIB */
	}
	
	for( i = 0 ; i < 256 ; i++)
	{
		tmp = real_contrast * (255 * (*pow_func)(((double)i + 0.5)/ 255, real_gamma) -128)
			+ 128 *  real_brightness ;
		if( tmp >= 255)
		{
			break;
		}
		else if( tmp < 0)
		{
			value_table[i] = 0 ;
		}
		else
		{
			value_table[i] = tmp ;
		}
	}

	for( ; i < 256 ; i++)
	{
		value_table[i] = 255 ;
	}
}

static int
modify_pixmap(
	x_display_t *  disp ,
	Pixmap  src_pixmap ,
	Pixmap  dst_pixmap ,		/* Can be same as src_pixmap */
	x_picture_modifier_t *  pic_mod	/* Mustn't be normal */
	)
{
	u_char  value_table[256] ;
	u_int  i , j ;
	u_int  width , height ;
	XImage *  image ;
	u_char  r , g , b ;
	u_long  pixel ;

	get_drawable_size( disp->display , src_pixmap , &width , &height) ;
	if( ( image = XGetImage( disp->display , src_pixmap , 0 , 0 , width , height ,
			AllPlanes , ZPixmap)) == NULL)
	{
		return  0 ;
	}

	value_table_refresh( value_table , pic_mod) ;

	if( disp->visual->class == TrueColor)
	{
		XVisualInfo *  vinfo ;
		rgb_info_t  rgbinfo ;

		if( ! ( vinfo = x_display_get_visual_info( disp)))
		{
			XDestroyImage( image) ;

			return  0 ;
		}

		rgb_info_init( vinfo , &rgbinfo) ;
		XFree( vinfo) ;

		for( i = 0 ; i < height ; i++)
		{
			for( j = 0 ; j < width ; j++)
			{
				pixel = XGetPixel( image , j , i) ;

				r = PIXEL_RED(pixel,rgbinfo) ;
				g = PIXEL_GREEN(pixel,rgbinfo) ;
				b = PIXEL_BLUE(pixel,rgbinfo) ;

				r = (value_table[r] * (255 - pic_mod->alpha) +
					pic_mod->blend_red * pic_mod->alpha) / 255 ;
				g = (value_table[g] * (255 - pic_mod->alpha) +
					pic_mod->blend_green * pic_mod->alpha) / 255 ;
				b = (value_table[b] * (255 - pic_mod->alpha) +
					pic_mod->blend_blue * pic_mod->alpha) / 255 ;

				XPutPixel( image , j , i ,
					RGB_TO_PIXEL(r,g,b,rgbinfo) |
					(disp->depth == 32 ? 0xff000000 : 0)) ;
			}
		}
	}
	else /* if( disp->visual->class == PseudoColor) */
	{
		XColor *  color_list ;
		int  num_cells ;

		if( ( num_cells = fetch_colormap( disp , &color_list)) == 0)
		{
			XDestroyImage( image) ;

			return  0 ;
		}

		for( i = 0 ; i < height ; i++)
		{
			for( j = 0 ; j < width ; j++)
			{
				if( ( pixel = XGetPixel( image, j, i)) >= num_cells)
				{
				#ifdef  DEBUG
					kik_debug_printf( KIK_DEBUG_TAG " Pixel %x is illegal.\n" ,
						pixel) ;
				#endif
					continue ;
				}

				r = color_list[pixel].red >> 8 ;
				g = color_list[pixel].green >> 8 ;
				b = color_list[pixel].blue >> 8 ;

				r = (value_table[r] * (255 - pic_mod->alpha) +
					pic_mod->blend_red * pic_mod->alpha) / 255 ;
				g = (value_table[g] * (255 - pic_mod->alpha) +
					pic_mod->blend_green * pic_mod->alpha) / 255 ;
				b = (value_table[b] * (255 - pic_mod->alpha) +
					pic_mod->blend_blue * pic_mod->alpha) / 255 ;

				XPutPixel( image , j , i ,
					closest_color_index( color_list , num_cells , r , g , b)) ;
			}
		}

		free( color_list) ;
	}

	XPutImage( disp->display , dst_pixmap , disp->gc->gc , image ,
		0 , 0 , 0 , 0 , width , height) ;

	XDestroyImage( image) ;

	return  1 ;
}


#ifdef  BUILTIN_IMAGELIB

/* create GdkPixbuf from the specified file path.
 *
 * The returned pixbuf should be unrefed by the caller.
 * Don't modify returned pixbuf since the pixbuf
 * is stored in the cache and may be reused.
 * This function is not reentrant.
 */
static GdkPixbuf *
load_file(
	char *  path ,		/* If NULL is specified, cache is cleared. */
	u_int  width ,		/* 0 == image width */
	u_int  height ,		/* 0 == image height */
	GdkInterpType  scale_type
	)
{
	static char *  name = NULL ;
	static GdkPixbuf *  orig_cache = NULL ;
	static GdkPixbuf *  scaled_cache = NULL ;
	GdkPixbuf *  pixbuf ;

	if( ! path)
	{
		/* free caches */
		if( orig_cache)
		{
			g_object_unref( orig_cache) ;
			orig_cache = NULL ;
		}
		
		if( scaled_cache)
		{
			g_object_unref( scaled_cache) ;
			scaled_cache = NULL ;
		}
		
		return  NULL ;
	}

	if( name == NULL || strcmp( name , path) != 0)
	{
		/* create new pixbuf */

	#ifdef  ENABLE_SIXEL
		if( ! strstr( path , ".six") || ! ( pixbuf = gdk_pixbuf_new_from_sixel( path)))
	#endif
		{
		#if GDK_PIXBUF_MAJOR >= 2
		#if GDK_PIXBUF_MINOR >= 14
			if( strstr( path , "://"))
			{
				GFile *  file ;
				GInputStream *  in ;

				if( ( in = (GInputStream*)g_file_read(
						( file = g_vfs_get_file_for_uri(
								g_vfs_get_default() , path)) ,
						NULL , NULL)))
				{
					pixbuf = gdk_pixbuf_new_from_stream( in , NULL , NULL) ;
					g_object_unref( in) ;
				}
				else
				{
				#ifndef  G_PLATFORM_WIN32
					char *  cmd ;
				#endif

					pixbuf = NULL ;

					/* g_unix_input_stream_new doesn't exists on win32. */
				#ifndef  G_PLATFORM_WIN32
					if( ( cmd = alloca( 11 + strlen( path) + 1)))
					{
						FILE *  fp ;

						sprintf( cmd , "curl -k -s %s" , path) ;
						if( ( fp = popen( cmd , "r")))
						{
							in = g_unix_input_stream_new(
								fileno(fp) , FALSE) ;
							pixbuf = gdk_pixbuf_new_from_stream(
									in , NULL , NULL) ;
							fclose( fp) ;
						}
					}
				#endif
				}

				g_object_unref( file) ;
			}
			else
		#endif
			{
				pixbuf = gdk_pixbuf_new_from_file( path , NULL) ;
			}
		#else
			pixbuf = gdk_pixbuf_new_from_file( path) ;
		#endif /*GDK_PIXBUF_MAJOR*/
		}

		if( pixbuf == NULL)
		{
			return  NULL ;
		}

		/* XXX Don't cache ~/.mlterm/[pty name].six. */
		if( ! strstr( path , "mlterm/"))
		{
		#ifdef  __DEBUG
			kik_warn_printf(KIK_DEBUG_TAG " adding a pixbuf to cache(%s)\n" , path) ;
		#endif

			/* Replace cache */
			free( name) ;
			name = strdup( path) ;

			if( orig_cache)
			{
				g_object_unref( orig_cache) ;
			}
			orig_cache = pixbuf ;

			if( scaled_cache) /* scaled_cache one is not vaild now */
			{
				g_object_unref( scaled_cache) ;
				scaled_cache = NULL ;
			}
		}
	}
	else
	{
	#ifdef __DEBUG
		kik_warn_printf(KIK_DEBUG_TAG " using the pixbuf from cache\n") ;
	#endif
		pixbuf = orig_cache ;
	}
	/* loading from file/cache ends here */

	if( width == 0)
	{
		width = gdk_pixbuf_get_width( pixbuf) ;
	}
	if( height == 0)
	{
		height = gdk_pixbuf_get_height( pixbuf) ;
	}
	
	/* It is necessary to scale orig_cache if width/height don't correspond. */
	if( ( width != gdk_pixbuf_get_width( pixbuf)) ||
	    ( height != gdk_pixbuf_get_height( pixbuf)))
	{
		if( pixbuf != orig_cache)
		{
			/* Non-cached image */

			GdkPixbuf *  scaled_pixbuf ;

			scaled_pixbuf = gdk_pixbuf_scale_simple( pixbuf ,
					width , height , scale_type) ;
			g_object_unref( pixbuf) ;

			return  scaled_pixbuf ;
		}
		/* Old cached scaled_cache pixbuf became obsolete if width/height is changed */
		else if( scaled_cache &&
		         gdk_pixbuf_get_width( scaled_cache) == width &&
		         gdk_pixbuf_get_height( scaled_cache) == height)
		{
		#ifdef __DEBUG
			kik_warn_printf(KIK_DEBUG_TAG
				" using the scaled_cache pixbuf(%u x %u) from cache\n" ,
				width , height) ;
		#endif
		
			pixbuf = scaled_cache ;
		}
		else
		{
			if( ! ( pixbuf = gdk_pixbuf_scale_simple( pixbuf ,
						width , height , scale_type)))
			{
				return  NULL ;
			}

		#ifdef __DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" adding a scaled pixbuf to cache(%u x %u)\n" , width , height) ;
		#endif

			if( scaled_cache)
			{
				g_object_unref( scaled_cache) ;
			}

			scaled_cache = pixbuf ;
		}
	}
	/* scaling ends here */

	if( pixbuf == scaled_cache || pixbuf == orig_cache)
	{
		/* Add reference count of the cache. */
		g_object_ref( pixbuf) ;
	}

	return  pixbuf ;
}


#ifdef  ENABLE_CARD2PIXBUF

/* create a pixbuf from an array of cardinals */
static GdkPixbuf *
create_pixbuf_from_cardinals(
	u_int32_t *  cardinal,
	int  req_width,
	int  req_height
	)
{
	GdkPixbuf *  pixbuf ;
	GdkPixbuf *  scaled ;
	int  rowstride ;
	u_char *  line ;
	u_char *  pixel ;
	int  width , height ;
	int  i , j ;

	width = cardinal[0] ;
	height = cardinal[1] ;
	
	if( ( pixbuf = gdk_pixbuf_new( GDK_COLORSPACE_RGB , TRUE , 8 , width , height)) == NULL)
	{
		return  NULL ;
	}

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;
	cardinal += 2 ;

	for( i = 0 ; i < width ; i++)
	{
		pixel = line ;
		for( j = 0 ; j < height ; j++)
		{
			/* ARGB -> RGBA conversion */
			pixel[2] = (*cardinal) & 0xff ;
			pixel[1] = ((*cardinal) >> 8) & 0xff ;
			pixel[0] = ((*cardinal) >> 16) & 0xff ;
			pixel[3] = ((*cardinal) >> 24) & 0xff ;

			cardinal++ ;
			pixel += 4;
		}
		line += rowstride ;
	}

	if( req_width == 0)
	{
		req_width = width ;
	}
	if( req_height == 0)
	{
		req_height = height ;
	}

	if( (req_width != width) || (req_height != height))
	{
		scaled = gdk_pixbuf_scale_simple( pixbuf , req_width , req_height ,
						 GDK_INTERP_TILES) ;
	}
	else
	{
		scaled = NULL ;
	}

	if( scaled)
	{
		g_object_unref( pixbuf) ;

		return  scaled ;
	}
	else
	{
		return  pixbuf ;
	}
}

#endif	/* ENABLE_CARD2PIXBUF */


static int
pixbuf_to_pixmap_pseudocolor(
	x_display_t *  disp,
	GdkPixbuf *  pixbuf,
	Pixmap pixmap
	)
{
	int  width , height , rowstride ;
	u_int  bytes_per_pixel ;
	int  x , y ;
	int  num_cells ;
#ifdef USE_FS
	char *  diff_next ;
	char *  diff_cur ;
	char *  temp ;
#endif /* USE_FS */
	u_char *  line ;
	u_char *  pixel ;
	XColor *  color_list ;
	int  closest ;
	int  diff_r , diff_g , diff_b ;
	int  ret_val = 0 ;

	if( ( num_cells = fetch_colormap( disp , &color_list)) == 0)
	{
		return  0 ;
	}

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

#ifdef USE_FS
	if( ( diff_cur = calloc( 1 , width * 3)) == NULL)
	{
		goto  error1 ;
	}
	if( ( diff_next = calloc( 1 , width * 3)) == NULL)
	{
		goto  error2 ;
	}
#endif /* USE_FS */

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4 : 3 ;
	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;

	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( y = 0 ; y < height ; y++)
	{
		pixel = line ;
#ifdef USE_FS
		closest = closest_color_index( color_list , num_cells ,
					       pixel[0] - diff_cur[0] ,
					       pixel[1] - diff_cur[1] ,
					       pixel[2] - diff_cur[2]) ;
		diff_r = (color_list[closest].red   >>8) - pixel[0] ;
		diff_g = (color_list[closest].green >>8) - pixel[1] ;
		diff_b = (color_list[closest].blue  >>8) - pixel[2] ;

		diff_cur[3*1 + 0 ] += diff_r /2 ;
		diff_cur[3*1 + 1 ] += diff_g /2 ;
		diff_cur[3*1 + 2 ] += diff_b /2 ;

		/* initialize next line */
		diff_next[3*0 +0] = diff_r /4 ;
		diff_next[3*0 +1] = diff_g /4 ;
		diff_next[3*0 +2] = diff_b /4 ;

		diff_next[3*1 +0] = diff_r /4 ;
		diff_next[3*1 +1] = diff_g /4 ;
		diff_next[3*1 +2] = diff_b /4 ;
#else
		closest = closest_color_index( color_list , num_cells ,
					       pixel[0] , pixel[1] , pixel[2]) ;
#endif /* USE_FS */

		XSetForeground( disp->display , disp->gc->gc , closest) ;
		XDrawPoint( disp->display , pixmap , disp->gc->gc , 0 , y) ;
		pixel += bytes_per_pixel ;

		for( x = 1 ; x < width -2 ; x++)
		{
#ifdef USE_FS
			closest = closest_color_index( color_list , num_cells ,
						       pixel[0] - diff_cur[3*x +0] ,
						       pixel[1] - diff_cur[3*x +1] ,
						       pixel[2] - diff_cur[3*x +2]) ;
			diff_r = (color_list[closest].red   >>8) - pixel[0] ;
			diff_g = (color_list[closest].green >>8) - pixel[1] ;
			diff_b = (color_list[closest].blue  >>8) - pixel[2] ;

			diff_cur[3*(x+1) + 0 ] += diff_r /2 ;
			diff_cur[3*(x+1) + 1 ] += diff_g /2 ;
			diff_cur[3*(x+1) + 2 ] += diff_b /2 ;

			diff_next[3*(x-1) +0] += diff_r /8 ;
			diff_next[3*(x-1) +1] += diff_g /8 ;
			diff_next[3*(x-1) +2] += diff_b /8 ;

			diff_next[3*(x+0) +0] += diff_r /8 ;
			diff_next[3*(x+0) +1] += diff_g /8 ;
			diff_next[3*(x+0) +2] += diff_b /8 ;
			/* initialize next line */
			diff_next[3*(x+1) +0] = diff_r /4 ;
			diff_next[3*(x+1) +1] = diff_g /4 ;
			diff_next[3*(x+1) +2] = diff_b /4 ;
#else
			closest = closest_color_index( color_list , num_cells ,
						       pixel[0] , pixel[1] , pixel[2]) ;
#endif /* USE_FS */

			XSetForeground( disp->display , disp->gc->gc , closest) ;
			XDrawPoint( disp->display , pixmap , disp->gc->gc , x , y) ;

			pixel += bytes_per_pixel ;
		}
#ifdef USE_FS
		closest = closest_color_index( color_list , num_cells ,
					       pixel[0] - diff_cur[3*x +0] ,
					       pixel[1] - diff_cur[3*x +1] ,
					       pixel[2] - diff_cur[3*x +2]) ;
		diff_r = (color_list[closest].red   >>8) - pixel[0] ;
		diff_g = (color_list[closest].green >>8) - pixel[1] ;
		diff_b = (color_list[closest].blue  >>8) - pixel[2] ;

		diff_next[3*(x-1) +0] += diff_r /4 ;
		diff_next[3*(x-1) +1] += diff_g /4 ;
		diff_next[3*(x-1) +2] += diff_b /4 ;

		diff_next[3*(x+0) +0] += diff_r /4 ;
		diff_next[3*(x+0) +1] += diff_g /4 ;
		diff_next[3*(x+0) +2] += diff_b /4 ;

		temp = diff_cur ;
		diff_cur = diff_next ;
		diff_next = temp ;
#else
		closest = closest_color_index( color_list , num_cells ,
					       pixel[0] , pixel[1] , pixel[2]) ;
#endif /* USE_FS */

		XSetForeground( disp->display , disp->gc->gc , closest) ;
		XDrawPoint( disp->display , pixmap , disp->gc->gc , x , y) ;
		line += rowstride ;
	}

	ret_val = 1 ;

#ifdef USE_FS
error2:
	free( diff_cur) ;
	free( diff_next) ;
#endif /* USE_FS */

error1:
	free( color_list) ;

	return  ret_val ;
}

static XImage *
pixbuf_to_ximage_truecolor(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf
	)
{
	XVisualInfo *  vinfo ;
	rgb_info_t  rgbinfo ;
	u_int  i , j ;
	u_int  width , height , rowstride , bytes_per_pixel ;
	u_char *  line ;
	XImage *  image ;
	char *  data ;

	if( ! ( vinfo = x_display_get_visual_info( disp)))
	{
		return  NULL ;
	}

	rgb_info_init( vinfo , &rgbinfo) ;
	XFree( vinfo) ;

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;
	/* Set num of bytes per pixel of display (necessarily 4 or 2 in TrueColor). */
	bytes_per_pixel = disp->depth > 16 ? 4 : 2 ;

	if( width > SSIZE_MAX / bytes_per_pixel / height ||	/* integer overflow */
	    ! ( data = malloc( width * height * bytes_per_pixel)))
	{
		return  NULL ;
	}

	if( ! ( image = XCreateImage( disp->display , disp->visual , disp->depth ,
				ZPixmap , 0 , data , width , height ,
				/* in case depth isn't multiple of 8 */
				bytes_per_pixel * 8 ,
				width * bytes_per_pixel)))
	{
		free( data) ;

		return  NULL ;
	}

	/* set num of bytes per pixel of pixbuf */
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4 : 3 ;
	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( i = 0 ; i < height ; i++)
	{
		u_char *  pixel ;

		pixel = line ;
		for( j = 0 ; j < width ; j++)
		{
			XPutPixel( image , j , i ,
				RGB_TO_PIXEL(pixel[0],pixel[1],pixel[2],rgbinfo) |
				(disp->depth == 32 ? 0xff000000 : 0)) ;
			pixel += bytes_per_pixel ;
		}
		line += rowstride ;
	}

	return  image ;
}

static int
pixbuf_to_pixmap(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap
	)
{
	if( disp->visual->class == TrueColor)
	{
		XImage *  image ;

		if( ( image = pixbuf_to_ximage_truecolor( disp , pixbuf)))
		{
			XPutImage( disp->display , pixmap , disp->gc->gc , image ,
				0 , 0 , 0 , 0 ,
				gdk_pixbuf_get_width( pixbuf) ,
				gdk_pixbuf_get_height( pixbuf)) ;
			XDestroyImage( image) ;

			return  1 ;
		}
		else
		{
			return  0 ;
		}
	}
	else /* if( disp->visual->class == PseudoColor) */
	{
		return  pixbuf_to_pixmap_pseudocolor( disp , pixbuf , pixmap) ;
	}
}

static int
pixbuf_to_pixmap_and_mask(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap ,
	Pixmap *  mask		/* Created in this function. */
	)
{
	if( ! pixbuf_to_pixmap( disp, pixbuf, pixmap))
	{
		return  0 ;
	}

	if( gdk_pixbuf_get_has_alpha( pixbuf))
	{
		int  i , j ;
		int  width , height , rowstride ;
		u_char *  line ;
		u_char *  pixel ;
		GC  mask_gc ;
		XGCValues  gcv ;
		int  has_tp ;

		width = gdk_pixbuf_get_width( pixbuf) ;
		height = gdk_pixbuf_get_height( pixbuf) ;

		/*
		 * DefaultRootWindow should not be used because depth and visual
		 * of DefaultRootWindow don't always match those of mlterm window.
		 * Use x_display_get_group_leader instead.
		 */
		*mask = XCreatePixmap( disp->display ,
				       x_display_get_group_leader( disp) ,
				       width, height, 1) ;
		mask_gc = XCreateGC( disp->display , *mask , 0 , &gcv) ;

		XSetForeground( disp->display , mask_gc , 0) ;
		XFillRectangle( disp->display , *mask , mask_gc , 0 , 0 , width , height) ;
		XSetForeground( disp->display , mask_gc , 1) ;

		line = gdk_pixbuf_get_pixels( pixbuf) ;
		rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;
		has_tp = 0 ;

		for( i = 0 ; i < height ; i++)
		{
			pixel = line + 3 ;
			for( j = 0 ; j < width ; j++)
			{
				if( *pixel > 127)
				{
					XDrawPoint( disp->display , *mask , mask_gc , j , i) ;
				}
				else
				{
					has_tp = 1 ;
				}

				pixel += 4 ;
			}
			line += rowstride ;
		}

		XFreeGC( disp->display , mask_gc) ;

		if( ! has_tp)
		{
			/* mask is not necessary. */
			XFreePixmap( disp->display , *mask) ;
			*mask = None ;
		}
	}
	else
	{
		/* no mask */
		*mask = None ;
	}

	return  1 ;
}

static XImage *
compose_truecolor(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap
	)
{
	XVisualInfo *  vinfo ;
	rgb_info_t  rgbinfo ;
	XImage *  image ;
	int  i , j ;
	int  width , height , rowstride ;
	u_char *  line ;
	u_char *  pixel ;
	u_char  r , g , b ;
	u_long  pixel2 ;

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

	if( ! ( vinfo = x_display_get_visual_info( disp)))
	{
		return  NULL ;
	}

	rgb_info_init( vinfo , &rgbinfo) ;
	XFree( vinfo) ;

	if( ! ( image = XGetImage( disp->display , pixmap , 0 , 0 , width , height ,
				AllPlanes , ZPixmap)))
	{
		return  NULL ;
	}

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( i = 0; i < height; i++)
	{
		pixel = line ;
		for( j = 0 ; j < width ; j++)
		{
			pixel2 = XGetPixel( image , j , i) ;
			
			r = PIXEL_RED(pixel2,rgbinfo) ;
			g = PIXEL_BLUE(pixel2,rgbinfo) ;
			b = PIXEL_GREEN(pixel2,rgbinfo) ;

			r = (r*(256 - pixel[3]) + pixel[0] * pixel[3])>>8 ;
			g = (g*(256 - pixel[3]) + pixel[1] * pixel[3])>>8 ;
			b = (b*(256 - pixel[3]) + pixel[2] * pixel[3])>>8 ;

			XPutPixel( image , j , i ,
				RGB_TO_PIXEL(r,g,b,rgbinfo) |
				(disp->depth == 32 ? 0xff000000 : 0)) ;
			pixel += 4 ;
		}
		line += rowstride ;
	}

	return  image ;
}

static XImage *
compose_pseudocolor(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap
	)
{
	XImage *  image ;
	int  i , j , num_cells ;
	int  width , height , rowstride ;
	u_int  r , g , b ;
	u_char *  line ;
	u_char *  pixel ;
	u_long  pixel2 ;
	XColor *  color_list ;

	if( ( num_cells = fetch_colormap( disp , &color_list)) == 0)
	{
		return  NULL ;
	}

	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;

	if( ! ( image = XGetImage( disp->display , pixmap , 0 , 0 , width , height ,
				AllPlanes, ZPixmap)))
	{
		free( color_list) ;
		return  NULL ;
	}

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( i = 0 ; i < height ; i++)
	{
		pixel = line ;
		for( j = 0 ; j < width ; j++)
		{
			if( ( pixel2 = XGetPixel( image , j , i)) >= num_cells)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Pixel %x is illegal.\n" ,
					pixel2) ;
			#endif
				continue ;
			}

			r = color_list[pixel2].red >>8 ;
			g = color_list[pixel2].green >>8 ;
			b = color_list[pixel2].blue >>8 ;

			r = (r*(256 - pixel[3]) + pixel[0] *  pixel[3])>>8 ;
			g = (g*(256 - pixel[3]) + pixel[1] *  pixel[3])>>8 ;
			b = (b*(256 - pixel[3]) + pixel[2] *  pixel[3])>>8 ;

			XPutPixel( image , j , i ,
				closest_color_index( color_list , num_cells , r , g , b)) ;
			pixel += 4 ;
		}
		line += rowstride ;
	}

	free( color_list) ;

	return  image ;
}

static int
compose_to_pixmap(
	x_display_t *  disp ,
	GdkPixbuf *  pixbuf ,
	Pixmap  pixmap
	)
{
	XImage *  image ;

	if( disp->visual->class == TrueColor)
	{
		image = compose_truecolor( disp , pixbuf , pixmap) ;
	}
	else /* if( disp->visual->class == PseudoColor) */
	{
		image = compose_pseudocolor( disp , pixbuf , pixmap) ;
	}

	if( ! image)
	{
		return  0 ;
	}

	XPutImage( disp->display , pixmap , disp->gc->gc , image , 0 , 0 , 0 , 0 ,
			gdk_pixbuf_get_width( pixbuf) ,
			gdk_pixbuf_get_height( pixbuf)) ;
	XDestroyImage( image) ;

	return  1 ;
}

static int
modify_image(
	GdkPixbuf *  pixbuf ,
	x_picture_modifier_t *  pic_mod		/* Mustn't be normal */
	)
{
	int  i , j ;
	int  width , height , rowstride , bytes_per_pixel ;
	u_char *  line ;
	u_char *  pixel ;
	u_char  value_table[256] ;

	value_table_refresh( value_table , pic_mod) ;

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4 : 3 ;
	width = gdk_pixbuf_get_width (pixbuf) ;
	height = gdk_pixbuf_get_height (pixbuf) ;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf) ;

	line = gdk_pixbuf_get_pixels( pixbuf) ;

	for( i = 0 ; i < height ; i++)
	{
		pixel = line ;
		line += rowstride ;

		for( j = 0 ; j < width ; j++)
		{
			/*
			 * XXX
			 * keeps neither hue nor saturation.
			 * MUST be replaced by another better color model(CIE Yxy? lab?)
			 */
			pixel[0] = (value_table[pixel[0]] * (255 - pic_mod->alpha) +
					pic_mod->blend_red * pic_mod->alpha) / 255 ;
			pixel[1] = (value_table[pixel[1]] * (255 - pic_mod->alpha) +
					pic_mod->blend_green * pic_mod->alpha) / 255 ;
			pixel[2] = (value_table[pixel[2]] * (255 - pic_mod->alpha) +
					pic_mod->blend_blue * pic_mod->alpha) / 255 ;
			/* alpha plane is not changed */
			pixel += bytes_per_pixel ;
		}
	}

	return  1 ;
}

#else  /* BUILTIN_IMAGELIB */

#ifdef  NO_TOOLS

#define  load_file( disp , width , height , path , pic_mod , pixmap , mask)  (0)
#define  create_cardinals_from_file( path , width , height)  (NULL)

#else	/* NO_TOOLS */

static pid_t
exec_mlimgloader(
	int *  read_fd ,
	int *  write_fd ,
	Window  window ,
	u_int  width ,
	u_int  height ,
	char *  path ,
	char *  cardinal_opt
	)
{
	int  fds1[2] ;
	int  fds2[2] ;
	pid_t  pid ;

	if( ! path || ! *path ||
	    pipe( fds1) == -1)
	{
		return  -1 ;
	}

	if( pipe( fds2) == -1)
	{
		goto  error1 ;
	}

	if( ( pid = fork()) == -1)
	{
		goto  error2 ;
	}

	if( pid == 0)
	{
		/* child process */

		char *  args[7] ;
		char  win_str[DIGIT_STR_LEN(Window) + 1] ;
		char  width_str[DIGIT_STR_LEN(u_int) + 1] ;
		char  height_str[DIGIT_STR_LEN(u_int) + 1] ;

		args[0] = LIBEXECDIR "/mlterm/mlimgloader" ;
		sprintf( win_str , "%lu" , window) ;
		args[1] = win_str ;
		sprintf( width_str , "%u" , width) ;
		args[2] = width_str ;
		sprintf( height_str , "%u" , height) ;
		args[3] = height_str ;
		args[4] = path ;
		args[5] = cardinal_opt ;
		args[6] = NULL ;

		close( fds1[1]) ;
		close( fds2[0]) ;

		if( dup2( fds1[0] , STDIN_FILENO) != -1 && dup2( fds2[1] , STDOUT_FILENO) != -1)
		{
			execv( args[0] , args) ;
		}

		kik_msg_printf( "Failed to exec %s.\n" , args[0]) ;

		exit(1) ;
	}

	close( fds1[0]) ;
	close( fds2[1]) ;

	*write_fd = fds1[1] ;
	*read_fd = fds2[0] ;

	return  pid ;

error2:
	close( fds2[0]) ;
	close( fds2[1]) ;
error1:
	close( fds1[0]) ;
	close( fds1[1]) ;

	return  -1 ;
}

static int
load_file(
	x_display_t *  disp ,
	u_int  width ,
	u_int  height ,
	char *  path ,
	x_picture_modifier_t *  pic_mod ,
	Pixmap *  pixmap ,
	Pixmap *  mask		/* Can be NULL */
	)
{
	int  read_fd ;
	int  write_fd ;
	char  pix_str[DIGIT_STR_LEN(Pixmap) + 1 + DIGIT_STR_LEN(Pixmap) + 1] ;
	Pixmap  pixmap_tmp ;
	Pixmap  mask_tmp ;
	ssize_t  size ;

	if( exec_mlimgloader( &read_fd , &write_fd ,
			x_display_get_group_leader( disp) ,
			width , height , path , NULL) == -1)
	{
		return  0 ;
	}

	if( ( size = read( read_fd , pix_str , sizeof(pix_str) - 1)) <= 0)
	{
		goto  error ;
	}

	pix_str[size] = '\0' ;

	if( sscanf( pix_str , "%lu %lu" , &pixmap_tmp , &mask_tmp) != 2)
	{
		goto  error ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Receiving pixmap %lu %lu\n" , pixmap_tmp , mask_tmp) ;
#endif

	if( width == 0 || height == 0)
	{
		get_drawable_size( disp->display , pixmap_tmp , &width , &height) ;
	}

	*pixmap = XCreatePixmap( disp->display , x_display_get_group_leader( disp) ,
				width , height , disp->depth) ;

	if( ! x_picture_modifier_is_normal( pic_mod))
	{
		modify_pixmap( disp , pixmap_tmp , *pixmap , pic_mod) ;
	}
	else
	{
		XCopyArea( disp->display , pixmap_tmp , *pixmap , disp->gc->gc ,
			0 , 0 , width , height , 0 , 0) ;
	}

	if( mask)
	{
		if( mask_tmp)
		{
			GC  mask_gc ;
			XGCValues  gcv ;

			*mask = XCreatePixmap( disp->display ,
					x_display_get_group_leader( disp) , width , height , 1) ;
			mask_gc = XCreateGC( disp->display , *mask , 0 , &gcv) ;
			XCopyArea( disp->display , mask_tmp , *mask , mask_gc ,
				0 , 0 , width , height , 0 , 0) ;

			XFreeGC( disp->display , mask_gc) ;
		}
		else
		{
			*mask = None ;
		}
	}

	XSync( disp->display , False) ;

	close( read_fd) ;
	close( write_fd) ; /* child process exited by this. pixmap_tmp is alive until here. */

	return  1 ;

error:
	close( read_fd) ;
	close( write_fd) ;

	return  0 ;
}

static u_int32_t *
create_cardinals_from_file(
	char *  path ,
	u_int32_t  width ,
	u_int32_t  height
	)
{
	int  read_fd ;
	int  write_fd ;
	u_int32_t *  cardinal ;
	ssize_t  size ;

	if( exec_mlimgloader( &read_fd , &write_fd ,
			None , width , height , path , "-c") == -1)
	{
		return  0 ;
	}

	if( read( read_fd , &width , sizeof(u_int32_t)) != sizeof(u_int32_t) ||
	    read( read_fd , &height , sizeof(u_int32_t)) != sizeof(u_int32_t))
	{
		cardinal = NULL ;
	}
	else if( ( cardinal = malloc( ( size = (width * height + 2) * sizeof(u_int32_t)))))
	{
		u_char *  p ;
		ssize_t  n_rd ;

		cardinal[0] = width ;
		cardinal[1] = height ;

		size -= (sizeof(u_int32_t) * 2) ;
		p = &cardinal[2] ;
		while( ( n_rd = read( read_fd , p , size)) > 0)
		{
			p += n_rd ;
			size -= n_rd ;
		}

		if( size > 0)
		{
			free( cardinal) ;
			cardinal = NULL ;
		}
	}

	close( read_fd) ;
	close( write_fd) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s(w %d h %d) is loaded.\n" ,
			path , width , height) ;
#endif

	return  cardinal ;
}

#endif	/* NO_TOOLS */

#endif	/* BUILTIN_IMAGELIB */


/* --- global functions --- */

int
x_imagelib_display_opened(
	Display *  display
	)
{
#if GDK_PIXBUF_MAJOR >= 2
	if( display_count == 0)
	{
		g_type_init() ;
	}
#endif /*GDK_PIXBUF_MAJOR*/

	/* Want _XROOTPIAMP_ID changed events. */
	XSelectInput( display , DefaultRootWindow( display) , PropertyChangeMask) ;

	display_count ++ ;

	return  1 ;
}

int
x_imagelib_display_closed(
	Display *  display
	)
{
	display_count -- ;

	if( display_count == 0)
	{
	#ifdef  BUILTIN_IMAGELIB
		/* drop pixbuf cache */
		load_file( NULL , 0 , 0 , 0) ;
	#endif
	}

	return  1 ;
}

/** Load an image from the specified file.
 *\param win mlterm window.
 *\param path File full path.
 *\param pic_mod picture modifier.
 *
 *\return  Pixmap to be used as a window's background.
 */
Pixmap
x_imagelib_load_file_for_background(
	x_window_t *  win,
	char *  path,
	x_picture_modifier_t *  pic_mod
	)
{
#ifdef  BUILTIN_IMAGELIB
	GdkPixbuf *  pixbuf ;
#endif
	Pixmap pixmap ;

	if( ! path || ! *path)
	{
		return  None ;
	}

	if( strncmp( path , "pixmap:" , 7) == 0 &&
		sscanf( path + 7 , "%lu" , &pixmap) == 1)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " pixmap:%lu is used.\n" , pixmap) ;
	#endif

		return  pixmap ;
	}

#ifdef  BUILTIN_IMAGELIB

	if( ! ( pixbuf = load_file( path , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				   GDK_INTERP_BILINEAR)))
	{
		return  None ;
	}

	if( ! x_picture_modifier_is_normal( pic_mod))
	{
		/* pixbuf which load_file() returned is cached, so don't modify it. */
		GdkPixbuf *  p ;

		p = gdk_pixbuf_copy( pixbuf) ;
		g_object_unref( pixbuf) ;

		if( ( pixbuf = p) == NULL)
		{
			return  None ;
		}
		
		if( ! modify_image( pixbuf , pic_mod))
		{
			g_object_unref( pixbuf) ;

			return  None ;
		}
	}

	if( gdk_pixbuf_get_has_alpha( pixbuf) &&
		(pixmap = x_imagelib_get_transparent_background( win , NULL)))
	{
		if( ! compose_to_pixmap( win->disp , pixbuf , pixmap))
		{
			goto  error ;
		}
	}
	else
	{
		pixmap = XCreatePixmap( win->disp->display , win->my_window ,
					ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
					win->disp->depth) ;
		
		if( ! pixbuf_to_pixmap( win->disp, pixbuf, pixmap))
		{
			goto  error ;
		}
	}

	g_object_unref( pixbuf) ;

	return  pixmap ;
	
error:
	XFreePixmap( win->disp->display , pixmap) ;
	g_object_unref( pixbuf) ;

	return  None ;

#else	/* BUILTIN_IMAGELIB */

	if( load_file( win->disp , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				path , pic_mod , &pixmap , NULL))
	{
		return  pixmap ;
	}
	else
	{
		return  None ;
	}

#endif	/* BUILTIN_IMAGELIB */
}

/** Create an pixmap from root window
 *\param win window structure
 *\param pic_mod picture modifier
 *
 *\return  Newly allocated Pixmap (or None in the case of failure)
 */
Pixmap
x_imagelib_get_transparent_background(
	x_window_t *  win,
	x_picture_modifier_t *  pic_mod
	)
{
	int  x ;
	int  y ;
	int  pix_x ;
	int  pix_y ;
	u_int  width ;
	u_int  height ;
	Pixmap  root ;
	Pixmap  pixmap ;
	u_int  root_width ;
	u_int  root_height ;

	if( ! ( root = root_pixmap( win->disp)))
	{
		return  None ;
	}

	if( ! x_window_get_visible_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " x_window_get_visible_geometry failed.\n") ;
	#endif

		return  None ;
	}

	/* The pixmap to be returned */
	pixmap = XCreatePixmap( win->disp->display , win->my_window ,
				ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				win->disp->depth) ;

	get_drawable_size( win->disp->display , root , &root_width , &root_height) ;

	if( win->disp->depth != DefaultDepth( win->disp->display ,
					DefaultScreen( win->disp->display)))
	{
		u_int  bytes_per_pixel ;
		XImage *  image = NULL ;
		char *  data = NULL ;
		XImage *  image2 ;
		XVisualInfo  vinfo_template ;
		int  nitems ;
		XVisualInfo *  vinfo ;
		rgb_info_t  rgbinfo ;
		rgb_info_t  rgbinfo2 ;
		u_int  _x ;
		u_int  _y ;

		/* Set num of bytes per pixel of display (necessarily 4 or 2 in TrueColor). */
		bytes_per_pixel = win->disp->depth > 16 ? 4 : 2 ;

		if( win->disp->visual->class != TrueColor ||
		    ! ( image = XGetImage( win->disp->display , root , x , y , width , height ,
					AllPlanes , ZPixmap)) ||
		    width > SSIZE_MAX / bytes_per_pixel / height ||
		    ! ( data = malloc( width * height * bytes_per_pixel)) ||
		    ! ( image2 = XCreateImage( win->disp->display , win->disp->visual ,
					win->disp->depth , ZPixmap , 0 , data , width , height ,
					/* in case depth isn't multiple of 8 */
					bytes_per_pixel * 8 ,
					width * bytes_per_pixel)))
		{
			XFreePixmap( win->disp->display , pixmap) ;
			if( image)
			{
				XDestroyImage( image) ;
			}
			if( data)
			{
				free( data) ;
			}

			return  None ;
		}

		vinfo_template.visualid = XVisualIDFromVisual(
						DefaultVisual( win->disp->display ,
							DefaultScreen( win->disp->display))) ;
		vinfo = XGetVisualInfo( win->disp->display , VisualIDMask ,
					&vinfo_template , &nitems) ;
		rgb_info_init( vinfo , &rgbinfo) ;
		XFree( vinfo) ;

		vinfo = x_display_get_visual_info( win->disp) ;
		rgb_info_init( vinfo , &rgbinfo2) ;
		XFree( vinfo) ;

		for( _y = 0 ; _y < height ; _y++)
		{
			for( _x = 0 ; _x < width ; _x++)
			{
				u_long  pixel ;

				pixel = XGetPixel( image , _x , _y) ;
				XPutPixel( image2 , _x , _y ,
					(win->disp->depth == 32 ? 0xff000000 : 0) |
					RGB_TO_PIXEL( PIXEL_RED(pixel,rgbinfo) ,
					              PIXEL_GREEN(pixel,rgbinfo) ,
					              PIXEL_BLUE(pixel,rgbinfo) , rgbinfo2)) ;
			}
		}

		XPutImage( win->disp->display , pixmap , win->disp->gc->gc , image2 ,
			0 , 0 , 0 , 0 , width , height) ;

		XDestroyImage( image) ;
		XDestroyImage( image2) ;
	}
	else if( root_width < win->disp->width || root_height < win->disp->height)
	{
		GC  gc ;

		gc = XCreateGC( win->disp->display , win->my_window , 0 , NULL) ;
		
		x %= root_width ;
		y %= root_height ;
		
		/* Some WM (WindowMaker etc) need tiling... sigh.*/
		XSetTile( win->disp->display , gc , root) ;
		XSetTSOrigin( win->disp->display , gc , -x , -y) ;
		XSetFillStyle( win->disp->display , gc , FillTiled) ;
		/* XXX not correct with virtual desktop?  */
		XFillRectangle( win->disp->display , pixmap , gc ,
			pix_x , pix_y , width , height) ;
		
		XFreeGC( win->disp->display, gc) ;
	}
	else
	{
		XCopyArea( win->disp->display , root , pixmap , win->gc->gc ,
				x , y , width , height , pix_x , pix_y) ;
	}

	if( ! x_picture_modifier_is_normal( pic_mod))
	{
		if( ! modify_pixmap( win->disp , pixmap , pixmap , pic_mod))
		{
			XFreePixmap( win->disp->display , pixmap) ;

			return  None ;
		}
	}

	return  pixmap ;
}

/** Load an image from the specified file with alpha plane. A pixmap and a mask are returned.
 *\param display connection to the X server.
 *\param path File full path.
 *\param cardinal Returns pointer to a data structure for the extended WM hint spec.
 *\param pixmap Returns an image pixmap for the old WM hint.
 *\param mask Returns a mask bitmap for the old WM hint.
 *\param width Pointer to the desired width. If *width is 0, the returned image would not be scaled and *width would be overwritten by its width. "width" can be NULL and the image would not be scaled and nothing would be returned in this case.
 *\param height Pointer to the desired height. *height can be 0 and height can be NULL(see "width" 's description)
 *
 *\return  Success => 1, Failure => 0
 */
int
x_imagelib_load_file(
	x_display_t *  disp,
	char *  path,
	u_int32_t **  cardinal,
	Pixmap *  pixmap,
	PixmapMask *  mask,
	u_int *  width,
	u_int *  height
	)
{
	u_int  dst_height, dst_width ;
#ifdef  BUILTIN_IMAGELIB
	GdkPixbuf *  pixbuf ;
#endif

	if( ! width)
	{
		dst_width = 0 ;
	}
	else
	{
		dst_width = *width ;
	}
	if( ! height)
	{
		dst_height = 0 ;
	}
	else
	{
		dst_height = *height ;
	}

#ifdef  BUILTIN_IMAGELIB

	if( path)
	{
		/* create a pixbuf from the file and create a cardinal array */
		if( !( pixbuf = load_file( path , dst_width , dst_height , GDK_INTERP_BILINEAR)))
		{
		#ifdef DEBUG
			kik_warn_printf(KIK_DEBUG_TAG "couldn't load pixbuf\n") ;
		#endif
			return  0 ;
		}

		if( cardinal)
		{
			if( ! ( *cardinal = create_cardinals_from_pixbuf( pixbuf)))
			{
				g_object_unref( pixbuf) ;

				return  0 ;
			}
		}
	}
	else
	{
	#ifdef  ENABLE_CARD2PIXBUF
		if( ! cardinal || ! *cardinal)
		{
			return  0 ;
		}

		/* create a pixbuf from the cardinal array */
		if( ! ( pixbuf = create_pixbuf_from_cardinals( *cardinal ,
					dst_width , dst_height)))
	#endif
		{
			return  0 ;
		}
	}

	dst_width = gdk_pixbuf_get_width( pixbuf) ;
	dst_height = gdk_pixbuf_get_height( pixbuf) ;

	/*
	 * Create the Icon pixmap & mask to be used in WMHints.
	 * Note that none as a result is acceptable.
	 * Pixmaps can't be cached since the last pixmap may be freed by someone...
	 */

	if( pixmap)
	{
		/*
		 * DefaultRootWindow should not be used because depth and visual
		 * of DefaultRootWindow don't always match those of mlterm window.
		 * Use x_display_get_group_leader instead.
		 */
		*pixmap = XCreatePixmap( disp->display , x_display_get_group_leader( disp) ,
					 dst_width , dst_height , disp->depth) ;
		if( mask)
		{
			if( ! pixbuf_to_pixmap_and_mask( disp , pixbuf , *pixmap , mask))
			{
				g_object_unref( pixbuf) ;

				goto  error ;
			}
		}
		else
		{
			if( ! pixbuf_to_pixmap( disp , pixbuf , *pixmap))
			{
				g_object_unref( pixbuf) ;

				goto  error ;
			}
		}
	}

	g_object_unref( pixbuf) ;

#else	/* BUILTIN_IMAGELIB */

	if( ! path)
	{
		/* cardinals => pixbuf is not supported. */
		return  0 ;
	}

	if( ! load_file( disp , dst_width , dst_height , path , NULL , pixmap , mask))
	{
		return  0 ;
	}

	/* XXX Duplicated in load_file */
	if( dst_width == 0 || dst_height == 0)
	{
		get_drawable_size( disp->display , *pixmap , &dst_width , &dst_height) ;
	}

	if( cardinal)
	{
		if( ! (*cardinal = create_cardinals_from_file( path , dst_width , dst_height)))
		{
			goto  error ;
		}
	}

#endif	/* BUILTIN_IMAGELIB */

	if( width && *width == 0)
	{
		*width = dst_width ;
	}

	if( height && *height == 0)
	{
		*height = dst_height ;
	}

	return  1 ;

error:
	XFreePixmap( disp->display , *pixmap) ;

	return  0 ;
}

Pixmap
x_imagelib_pixbuf_to_pixmap(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod ,
	GdkPixbufPtr  pixbuf
	)
{
#ifdef  BUILTIN_IMAGELIB

	Pixmap  pixmap ;
	GdkPixbuf *  target ;

	if( ! x_picture_modifier_is_normal( pic_mod))
	{
		if( ( target = gdk_pixbuf_copy( pixbuf)) == NULL)
		{
			return  None ;
		}

		modify_image( target , pic_mod) ;
	}
	else
	{
		target = pixbuf ;
	}
	
	pixmap = XCreatePixmap( win->disp->display , win->my_window ,
				ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				win->disp->depth) ;
	
	if( pixbuf_to_pixmap( win->disp , target , pixmap))
	{
		return  pixmap ;
	}

	if( target != pixbuf)
	{
		g_object_unref( target) ;
	}
	
	XFreePixmap( win->disp->display, pixmap) ;

#endif	/* BUILTIN_IMAGELIB */

	return  None ;
}

int
x_delete_image(
	Display *  display ,
	Pixmap  pixmap
	)
{
	XFreePixmap( display , pixmap) ;

	return  1 ;
}
