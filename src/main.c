/* main.c

Copyright (C) 1999-2003 Tom Gilbert.
Copyright (C) 2010-2018 Daniel Friesel.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies of the Software and its documentation and acknowledgment shall be
given in the documentation and software packages that this Software was
used.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "feh.h"
#include "filelist.h"
#include "winwidget.h"
#include "timers.h"
#include "options.h"
#include "events.h"
#include "signals.h"
#include "wallpaper.h"
#include <termios.h>

char **cmdargv = NULL;
int cmdargc = 0;
char *mode = NULL;

int main(int argc, char **argv)
{
	atexit(feh_clean_exit);

	srandom(getpid() * time(NULL) % ((unsigned int) -1));

	setup_signal_handlers();
	init_parse_options(argc, argv);

	init_imlib_fonts();

	if (opt.display) {
		init_x_and_imlib();
		init_keyevents();
		init_buttonbindings();
	}

	feh_event_init();

	imlib_set_cache_size(2048 * 1024);
	//imlib_set_cache_size(51200 * 1024);

	
	if (opt.index)
		init_index_mode();
	else if (opt.multiwindow)
		init_multiwindow_mode();
	else if (opt.list || opt.customlist)
		init_list_mode();
	else if (opt.loadables)
		init_loadables_mode();
	else if (opt.unloadables)
		init_unloadables_mode();
	else if (opt.thumbs)
		init_thumbnail_mode();
	else if (opt.bgmode) {
		feh_wm_set_bg_filelist(opt.bgmode);
		exit(0);
	}
	else if (opt.display){
	//  Slideshow mode is the default. Because it's spiffy 
	        opt.slideshow = 1;
	  	init_slideshow_mode();
	}
	else {
		eprintf("Invalid option combination");
	}
	
	/* main event loop */
	while (feh_main_iteration());

	return(sig_exit);
}

/* Return 0 to stop iterating, 1 if ok to continue. */
int feh_main_iteration()
{
	static int first = 1;
	static int xfd = 0;
	//static int fdsize = 0;
	XEvent ev;
	fd_set fdset;
	static int currentIndex = -1;
	static int prevIndex = -1;

	if (window_num == 0 || sig_exit != 0)
		return(0);

	if (first) {
		/* Only need to set these up the first time */
		xfd = ConnectionNumber(disp);
		//fdsize = xfd + 1;
		prevIndex = opt.initial_index;
		first = 0;
		/*
		 * Only accept commands from stdin if
		 * - stdin is a terminal (otherwise it's probably used as an image / filelist)
		 * - we aren't running in multiwindow mode (cause it's not clear which
		 *   window commands should be applied to in that case)
		 * - we're in the same process group as stdin, AKA we're not running
		 *   in the background. Background processes are stopped with SIGTTOU
		 *   if they try to write to stdout or change terminal attributes. They
		 *   also don't get input from stdin anyway.
		 */
		if (isatty(STDIN_FILENO) && !opt.multiwindow && getpgrp() == (tcgetpgrp(STDIN_FILENO))) {
			setup_stdin();
		}
	}

	currentIndex = feh_get_pic_index(opt.interval,opt.pic_count);

	if (currentIndex != prevIndex) {
		slideshow_change_image_by_index(opt.w_data, currentIndex);
	}

	prevIndex = currentIndex;

	while (XPending(disp)) {
		XNextEvent(disp, &ev);
		if (ev_handler[ev.type])
			(*(ev_handler[ev.type])) (&ev);

		if (window_num == 0 || sig_exit != 0)
			return(0);
	}
	XFlush(disp);

	feh_redraw_menus();

	FD_ZERO(&fdset);
	FD_SET(xfd, &fdset);
	if (control_via_stdin)
		FD_SET(STDIN_FILENO, &fdset);

	if (window_num == 0 || sig_exit != 0)
		return(0);
	
	return(1);
}

void feh_clean_exit(void)
{
	delete_rm_files();

	free(opt.menu_font);

	if(disp)
		XCloseDisplay(disp);

	/*
	 * Only restore the old terminal settings if
	 * - we changed them in the first place
	 * - stdin still is a terminal (it might have been closed)
	 * - stdin still belongs to us (we might have been detached from the
	 *   controlling terminal, in that case we probably shouldn't be messing
	 *   around with it) <https://github.com/derf/feh/issues/324>
	 */
	if (control_via_stdin && isatty(STDIN_FILENO) && getpgrp() == (tcgetpgrp(STDIN_FILENO)))
		restore_stdin();

	if (opt.filelistfile)
		feh_write_filelist(filelist, opt.filelistfile);

	return;
}
