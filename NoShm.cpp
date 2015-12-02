#include <X11/Xlib.h>
#include <X11/Xfuncproto.h>
#include <X11/extensions/XShm.h>

_XFUNCPROTOBEGIN

Bool XShmQueryVersion(Display *d, int *major, int *minor, Bool *pixmaps ){
	return false;
}

Status XShmAttach( Display *d, XShmSegmentInfo *shminfo ){ return false; }
Status XShmDetach( Display *d, XShmSegmentInfo *shminfo ){ return false; }
XImage *XShmCreateImage( Display *, Visual *, unsigned int, int, char *, XShmSegmentInfo *, unsigned 
int, unsigned int ){
	return NULL;
}
Status XShmPutImage( Display *dpy, Drawable d, GC gc, XImage *image, int sx, int sy, int dx, int dy, 
unsigned int w, unsigned int h, Bool evt ){
	return false;
}

_XFUNCPROTOEND
