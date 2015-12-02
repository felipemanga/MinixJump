
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include <random>
#include <algorithm>
#include <png.h>

#include "XGame.h"


#ifndef WIN32
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include "XEventNames.h"
#include "PNGLoader.h"

char Sprite::tmpbuf[256];

int XGame::loadSpritesheet( const std::string &name ){
	int id = this->loadTexture(name + ".png");
	if( !id ) return 0;

	XGameTexture *t = this->getTexture(id);
	std::ifstream inp(name+".txt");
	if( inp.is_open() ){
		while( !inp.eof() ){
			std::string name;
			int x, y, w, h;
			inp >> name >> x >> y >> w >> h;
			// std::cout << name << std::endl;
			t->frames[name] = {x, y, w, h};
		}
		inp.close();
	}

	return id;
}

class Texture : public XGameTexture {

public:
	XImage *img;
	int depthPad;
	int depth;

	Texture( char *data ) : XGameTexture(){
		this->data = data;
		this->alpha = NULL;
		img = NULL;
		depthPad = 32;
		depth = 32;
	}

	~Texture(){
		if( alpha ) free(alpha);
		if( img ) XDestroyImage( img );
		else if( data ) free( data );
	}

	void create( Display *display, Visual *vis ){
		img = XCreateImage( 
			display, 
			vis, 
			depthPad, 
			ZPixmap, 
			0, 
			data, width, height, depth, 0 
			);
	}

	void to16BPP(){
		depthPad = depth = 16;

		if( alpha ) free(alpha);

		int BPP = 2;
		char *ndata = (char *) malloc(width*height*BPP);
		alpha = (char *) malloc(width*height);
		unsigned char *d = (unsigned char *) data;
		char *ap = alpha;
		bool hasAlpha = false;

		for( int y=0; y<height; ++y ){
			for( int x=0; x<width; ++x ){
				unsigned short int &s = *((unsigned short int *)&ndata[(y*width+x)*BPP]);
				unsigned int r = *d++;
				unsigned int g = *d++;
				unsigned int b = *d++;
				/* */
				r ^= ((x+y)&1)<<2;
				g ^= ((x+y)&1)<<1;
				b ^= ((x+y)&1)<<2;

				r = (unsigned int)( r/255.0f*0x1F+0.5f ) << 3;
				g = (unsigned int)( g/255.0f*0x3F+0.5f ) << 2;
				b = (unsigned int)( b/255.0f*0x1F+0.5f ) << 3;
				/* */

				if( *d != 0xFF )
					hasAlpha=true;

				*ap++ = *d++;


				s = (
				    ((r<<8)&0xF800)
				  | ((g<<3)&0x07E0)
				  | ((b>>3)&0x001F)
				  );
			}
		}

		if( !hasAlpha ){
			free(alpha);
			alpha=NULL;
		}

		free(data);
		data = ndata;
	}
};

static bool sortSprite( Sprite *a, Sprite *b ){
	return a->z > b->z;
}


class XGameImpl : public XGame {
public:
	int depth;
	int depthPad;
	int depthBytes;

	Display *display;
	int screen;
	Window window;
	GC gc;
	void *bufferPixels;

	GLXContext glc;

	std::unordered_map<std::string, int> textureLookup;

	XGameImpl(){
		display = NULL;
		bufferPixels = NULL;
		depth = 16;
		depthPad = 16;
		depthBytes = depth/8;
	}

	virtual void clearScene(){
		while( sprites.size() ){
			Sprite *s = sprites.back();
			sprites.pop_back();
			delete s;
		}
	}

	virtual ~XGameImpl(){
		clearScene();

		while( textures.size() ){
			XGameTexture *t = textures.back();
			textures.pop_back();
			delete t;
		}

		if( display ){
			XFreeGC(display, gc);
			XDestroyWindow(display, window);
			XCloseDisplay(display);
		}

		if( bufferPixels )
			free( bufferPixels );
	}

	int loadTexture( const std::string &name ){
		PNGLoader *ldr = PNGLoader::load(name.c_str());
		if( !ldr ) return 0;

		auto it = textureLookup.find(name);
		if( it != textureLookup.end() )
			return it->second;

		Texture *t = new Texture( ldr->data );
		t->cropWidth = t->width = ldr->width;
		t->cropHeight = t->height = ldr->height;
		textures.push_back(t);

		if( this->depth == 16 ) t->to16BPP();

		ldr->forget();
		delete ldr;

		return textures.size()-1;
	}

	void blit( int texid, int x, int y, bool mirror ){
		Texture *tex = (Texture *) textures[texid];
		// std::cout << tex->offX << tex->offY << x << y << " " << tex->cropWidth << " " << tex->cropHeight << std::endl;
		if( tex->img ){
			XPutImage(
				display, window, gc, 
				tex->img, tex->offX, tex->offY, x, y, 
				tex->cropWidth, 
				tex->cropHeight 
			);
		}else{
			Texture *fb = (Texture *) textures[0];

			if( x >= fb->width || x+tex->cropWidth < 0 ||
			    y >= fb->height || y+tex->cropHeight < 0 )
				return;

			int oxoff=x, oyoff=y;
			int ixoff=tex->offX, iyoff=tex->offY;

			int sx=0, sy=0;
			int ex=tex->cropWidth, ey=tex->cropHeight;

			if( oxoff<0 ){ 
				ex += oxoff; 
				if( mirror ) ixoff += oxoff; 
				else ixoff -= oxoff; 
				oxoff = 0;
			}
			if( oyoff<0 ){ ey += oyoff; iyoff -= oyoff; oyoff = 0; }

			if( oxoff+ex > fb->cropWidth ) ex -= ((oxoff+ex) - fb->cropWidth);
			if( oyoff+ey > fb->cropHeight ) ey -= ((oyoff+ey) - fb->cropHeight);

			auto od = (unsigned short *) fb->getData();
			int odstride = fb->width;
			auto id = (unsigned short *) tex->getData();
			int idstride = tex->width;

			int m=1;
			if( mirror ){
				m=-1;
				ixoff += tex->cropWidth-1;
			}
			
			unsigned char *alpha = (unsigned char *) tex->getAlpha();
			if( !alpha ){
				for( int iy=sy; iy<ey; ++iy ){
					for( int ix=sx; ix<ex; ++ix ){
						unsigned short v=id[(iy+iyoff)*idstride+ixoff+ix*m];
						od[(iy+oyoff)*odstride+ix+oxoff] = v;
					}
				}
			}else{
				for( int iy=sy; iy<ey; ++iy ){
					for( int ix=sx; ix<ex; ++ix ){
						int i=(iy+iyoff)*idstride+ixoff+ix*m;
						auto ia = alpha[i];
						if( ia < 0x10 ) continue;
						float fa = ia/255.0f;
						float fb = 1-fa;
						unsigned short v=id[i];
						unsigned short &o=od[(iy+oyoff)*odstride+ix+oxoff];
						unsigned short r = 0x1F & (unsigned short)( (float((v&0xF800)>>11)*fa + float((o&0xF800)>>11)*fb) );
						unsigned short g = 0x3F & (unsigned short)( (float((v&0x07E0)>>5 )*fa + float((o&0x07E0)>>5 )*fb) );
						unsigned short b = 0x1F & (unsigned short)( (float( v&0x001F     )*fa + float( o&0x001F     )*fb) );
						o = (r<<11) | (g<<5) | b;
					}
				}			
			}
		}
	}

	void blit4X( int texid, int x, int y, bool mirror ){
		Texture *tex = (Texture *) textures[texid];
		if( tex->img ){
			XPutImage(
				display, window, gc, 
				tex->img, tex->offX, tex->offY, x, y, 
				tex->cropWidth, 
				tex->cropHeight 
			);
		}else{
			Texture *fb = (Texture *) textures[0];

			if( x >= fb->width || x+tex->cropWidth*2 < 0 ||
			    y >= fb->height || y+tex->cropHeight*2 < 0 )
				return;

			int oxoff=x, oyoff=y;
			int ixoff=tex->offX, iyoff=tex->offY;

			int sx=0, sy=0;
			int ex=tex->cropWidth, ey=tex->cropHeight;

			if( oxoff<0 ){ 
				ex += oxoff/2; 
				if( mirror ) ixoff += oxoff/2; 
				else ixoff -= oxoff/2; 
				oxoff = 0;
			}
			if( oyoff<0 ){ ey += oyoff/2; iyoff -= oyoff/2; oyoff = 0; }

			if( oxoff+ex*2 > fb->cropWidth ) ex -= ((oxoff+ex*2) - fb->cropWidth)/2;
			if( oyoff+ey*2 > fb->cropHeight ) ey -= ((oyoff+ey*2) - fb->cropHeight)/2;

			auto od = (unsigned short *) fb->getData();
			int odstride = fb->width;
			auto id = (unsigned short *) tex->getData();
			int idstride = tex->width;

			int m=1;
			if( mirror ){
				m=-1;
				ixoff += tex->cropWidth-1;
			}

			unsigned char *alpha = (unsigned char *) tex->getAlpha();
			if( !alpha ){
				std::cout << " no alpha" << std::endl;
				for( int iy=sy; iy<ey; ++iy ){
					for( int ix=sx; ix<ex; ++ix ){
						unsigned short v=id[(iy+iyoff)*idstride+ixoff+ix*m];
						od[(iy*2+0+oyoff)*odstride+ix*2+0+oxoff] = v;
						od[(iy*2+0+oyoff)*odstride+ix*2+1+oxoff] = v;
						od[(iy*2+1+oyoff)*odstride+ix*2+0+oxoff] = v;
						od[(iy*2+1+oyoff)*odstride+ix*2+1+oxoff] = v;
					}
				}
			}else{
				typedef unsigned int uint;
				for( int iy=sy; iy<ey; ++iy ){
					for( int ix=sx; ix<ex; ++ix ){
						int i=(iy+iyoff)*idstride+ixoff+ix*m;
						unsigned char a = alpha[i];
						if( a < 0x10 ) continue;
						unsigned short v=id[i];
						unsigned short o=od[(iy*2+0+oyoff)*odstride+ix*2+0+oxoff];
						unsigned short r = ((uint(v&0xF800) + uint(o&0xF800)))>>1;
						unsigned short g = ((uint(v&0x07E0) + uint(o&0x07E0)))>>1;
						unsigned short b = ((uint(v&0x001F) + uint(o&0x001F)))>>1;
						v = r | g | b;
						od[(iy*2+0+oyoff)*odstride+ix*2+0+oxoff] = v;
						od[(iy*2+0+oyoff)*odstride+ix*2+1+oxoff] = v;
						od[(iy*2+1+oyoff)*odstride+ix*2+0+oxoff] = v;
						od[(iy*2+1+oyoff)*odstride+ix*2+1+oxoff] = v;
					}
				}			
			}
		}
	}

	XGameTexture *getTexture( int texid ){
		return textures[texid];
	}

	void setupFramebuffer(){
		char *data = (char *) malloc(this->width*this->height*depthBytes);
		Texture *t = new Texture( data );
		t->cropWidth = t->width = this->width;
		t->cropHeight = t->height = this->height;
		t->depthPad = depthPad;
		t->depth = depth;
		t->create( display, CopyFromParent );
		textures.push_back( t );
	}

	virtual void init(){
		display = XOpenDisplay(NULL);

		if( !display ) 
		    throw XGameError("Could not create game window.");

		XVisualInfo vinfo;

		screen = DefaultScreen(display);

		/* */
		XMatchVisualInfo(display, screen, depth, TrueColor, &vinfo);
		XSetWindowAttributes attrs;
		attrs.colormap = XCreateColormap( display, DefaultRootWindow(display), vinfo.visual, AllocNone);
		attrs.background_pixel = 0;
		attrs.border_pixel = 0;

		window = XCreateWindow(
			display,
			DefaultRootWindow(display),
			10, 10, width, height,
			0,
			depth, InputOutput,
			vinfo.visual,
			CWBackPixel | CWColormap | CWBorderPixel, &attrs
		);
		/*/
		window = XCreateSimpleWindow( 
			display,
			DefaultRootWindow(display), 
			0, 0, width, height,
			0, 
			BlackPixel(display, screen),
			WhitePixel(display, screen)
		);
		/* */

		XSetStandardProperties(
			display, window,
			title.c_str(), title.c_str(),
			None, NULL, 0, NULL
		);

		XSelectInput( display, window, 0xFFFFFFFF>>(32-24) );

		gc=XCreateGC( display, window, 0, 0 );

		Atom wmDelete=XInternAtom(display, "WM_DELETE_WINDOW", True);
		XSetWMProtocols(display, window, &wmDelete, 1);

		XClearWindow( display, window );
		XMapRaised( display, window );

		glXCreateContext(display, &vinfo, NULL, GL_FALSE);

		setupFramebuffer();
	}

	void update(){
		if( gameOver ){
			gameOver = false;
			clearScene();
			initScene(this);
		}

		for( auto s=sprites.begin(); s!=sprites.end(); ++s )
			(*s)->update();

		std::sort( sprites.begin(), sprites.end(), sortSprite );

		for( auto s=sprites.begin(); s!=sprites.end(); ++s )
			(*s)->blit();

		draw();
	}

	void draw(){
		blit(0,0,0,false);
		XSync( display, False );
	}

	virtual void run(){
		XEvent event;
		KeySym key;
		char keyText[255];
		bool canDraw = false;

		timespec sleepTime, remTime;

		sleepTime.tv_sec = 0;
		sleepTime.tv_nsec = 1000000000 / fps;

		while(running){
			while(XPending(display) > 0){
				XNextEvent( display, &event );
				switch( event.type ){
				case ClientMessage:
					return;
				case MapNotify:
					canDraw = true;
					continue;
				case KeyPress:
					memset( keyText, 0, 255);
					XLookupString(&event.xkey, keyText, 255, &key, 0);
					if( onKeyDown ) onKeyDown( key );
					break;
				case KeyRelease:
					memset( keyText, 0, 255);
					XLookupString(&event.xkey, keyText, 255, &key, 0);
					if( onKeyUp ) onKeyUp( key );
					break;
				default:
					break;
					// std::cout << "Unknown Event " << X_EVENT[event.type] << std::endl;				
				}
			}

			// std::this_thread::sleep_for(std::chrono::milliseconds(12)); // old gcc has no this_thread.
			// usleep(100000);
			nanosleep(&sleepTime, &remTime);

			if( canDraw ) update();

		}
	}
};

#endif

XGame *XGame::create(){
	return new XGameImpl();
}

