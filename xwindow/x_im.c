/*
 *	$Id$
 */

#include  <kiklib/kik_dlfcn.h>
#include  <kiklib/kik_str.h>	/* kik_str_sep, kik_snprintf */

#include  "x_im.h"
#include  "x_term_manager.h"

#ifndef  LIBDIR
#define  IM_DIR  "/usr/local/lib/mlterm/"
#else
#define  IM_DIR  LIBDIR "/mlterm/"
#endif


typedef  x_im_t * (*x_im_new_func_t)( u_int64_t  magic ,
				      ml_char_encoding_t  term_encoding ,
				      x_im_export_syms_t *  syms ,
				      char *  engine) ;


/* --- static variables --- */

static  x_im_export_syms_t  im_export_syms =
{
	ml_str_init ,
	ml_str_delete ,
	ml_char_combine ,
	ml_char_set ,
	ml_get_char_encoding ,
	ml_is_msb_set ,
	ml_parser_new ,
	ml_conv_new ,
	x_im_candidate_screen_new ,
	x_term_manager_add_fd ,
	x_term_manager_remove_fd

} ;

/* --- static functions --- */

static x_im_new_func_t
dlsym_im_new_func(
	char *  im_name
	)
{
	kik_dl_handle_t  handle ;
	char * libname ;

	if( ! im_name)
	{
		return  NULL ;
	}

	if( ! ( libname = alloca( sizeof(char) * (strlen( im_name) + 4))))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  NULL ;
	}

	sprintf( libname , "im-%s" , im_name) ;

	if( ! ( handle = kik_dl_open( IM_DIR , libname)))
	{
		return  NULL ;
	}

	return  (x_im_new_func_t) kik_dl_func_symbol( handle , "im_new") ;
}


/* --- global functions --- */

x_im_t *
x_im_new(
	ml_char_encoding_t  term_encoding ,
	x_im_event_listener_t *  im_listener ,
	char *  input_method
	)
{
	x_im_t *  im ;
	x_im_new_func_t  func ;
	char *  im_name ;
	char *  im_attr ;

	if( strcmp( input_method , "none") == 0)
	{
		return  NULL ;
	}

	if( strchr( input_method , ':'))
	{
		im_attr = strdup( input_method) ;

		if( ( im_name = kik_str_sep( &im_attr , ":")) == NULL)
		{
		#ifdef  DEBUG
			kik_error_printf( "%s is illegal input method.\n" , input_method) ;
		#endif

			return  NULL ;
		}
	}
	else
	{
		im_name = strdup( input_method) ;
		im_attr = NULL ;
	}

	if ( ! ( func = dlsym_im_new_func( im_name)))
	{
		free( im_name) ;

		return  NULL ;
	}

	if( ( im = (*func)( IM_API_COMPAT_CHECK_MAGIC , term_encoding ,
			    &im_export_syms , im_attr)))
	{
		/*
		 * initializations for x_im_t
		 */
		im->listener = im_listener ;
		im->cand_screen = NULL ;
		im->preedit.chars = NULL ;
		im->preedit.num_of_chars = 0 ;
		im->preedit.filled_len = 0 ;
		im->preedit.segment_offset = 0 ;
		im->preedit.cursor_offset = X_IM_PREEDIT_NOCURSOR ;

	}
	else
	{
		kik_error_printf( "Cound not open specified "
				  "input method(%s).\n" , im_name) ;
	}

	free( im_name) ;

	return  im ;
}

void
x_im_redraw_preedit(
	x_im_t *  im ,
	int  is_focused
	)
{
	(*im->listener->draw_preedit_str)( im->listener->self ,
					   im->preedit.chars ,
					   im->preedit.filled_len ,
					   im->preedit.cursor_offset) ;

	if( ! im->cand_screen)
	{
		return ;
	}

	if( is_focused)
	{
		int  x ;
		int  y ;

		if( (*im->listener->get_spot)( im->listener->self ,
					       im->preedit.chars ,
					       im->preedit.segment_offset ,
					       &x , &y))
		{
			(*im->cand_screen->show)( im->cand_screen) ;
			(*im->cand_screen->set_spot)( im->cand_screen , x , y) ;
		}
	}
	else
	{
		(*im->cand_screen->hide)( im->cand_screen) ;
	}
}

