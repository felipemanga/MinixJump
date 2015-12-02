#ifndef XGAME_H
#define XGAME_H

#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

#include <png.h>

class Rect {
public:
	int x, y, w, h;
};

class XGameTexture {
protected:
	char *data;
	char *alpha;
public:
	XGameTexture(){ data=NULL; offX=offY=0; }
	~XGameTexture(){}
	char *getData(){ return data; }
	char *getAlpha(){ return alpha; }
	int width;
	int height;
	int offX, offY, cropWidth, cropHeight;
	std::unordered_map<std::string, Rect> frames;

	bool setTextureFrame( const std::string &name ){
		auto it = frames.find(name);
		if( it == frames.end() ) return false;
		auto &f = it->second;
		offX=f.x;
		offY=f.y;
		cropWidth=f.w;
		cropHeight=f.h;
		return true;
	}

	void fill16(unsigned short c){
		auto b = (unsigned short *) data;
		auto e = b+width*height;
		while(b!=e) *b++=c;
	}
};

class XGameError{
public:
	std::string message;
	XGameError(const std::string &msg ){
		this->message=msg;
	}
};

class Sprite;

class XGame {
protected:
	std::vector<XGameTexture *> textures;
public:
	int width;
	int height;
	int fps;
	std::string title;
	bool running, gameOver;

	std::vector<Sprite *> sprites;

	std::function<void(int)> onKeyDown;
	std::function<void(int)> onKeyUp;
	std::function<void(XGame *)> initScene;

	int cameraX, cameraY;

	XGame(){
		gameOver = true;
		cameraX = cameraY = 0;
		width = 800;
		height = 600;
		fps = 24;
		title = "XGame";
		running = true;
		onKeyUp = onKeyDown = NULL;
	}


	virtual ~XGame(){}

	virtual void init() = 0;
	virtual void run() = 0;
	virtual int loadTexture(const std::string &) = 0;
	virtual XGameTexture *getTexture(int) = 0;
	virtual void blit(int id, int x, int y, bool mirror) = 0;
	virtual void blit4X(int id, int x, int y, bool mirror) = 0;

	static XGame *create();
	
	virtual int loadSpritesheet(const std::string &name);
	bool setTextureFrame( int texid, const std::string &name ){
		return textures[texid]->setTextureFrame(name);
	}
};


class Sprite {
public:
	int texture;
	int z;
	int frame, x, y, velX, velY, velZ;
	int frameTime, frameTimeLeft;
	int loopStart, loopEnd;
	bool stretch, mirror, fixed;
	std::string animation;
	XGame *game;

	static char tmpbuf[256];


	Sprite( XGame *game ){
		this->game = game;
		game->sprites.push_back(this);
		loopStart = loopEnd = frame = x = y = z = velX = velY = velZ = 0;
		fixed = mirror = stretch = false;
		frameTimeLeft = frameTime = 0;
	}

	virtual void update(){
		z += velZ;
		y += velY;
		x += velX;
		if( !frameTimeLeft ){
			frameTimeLeft = frameTime;
			frame++;
			if( frame == loopEnd ) frame = loopStart;
		}else{
			frameTimeLeft--;
		}

		applyAnimation();
	}

	void applyAnimation(){
		if( !animation.empty() ){
			sprintf( tmpbuf, "%s%i", animation.c_str(), frame );
			if( !game->setTextureFrame(texture, tmpbuf) && !game->setTextureFrame(texture, animation) )
				std::cout << "Invalid animation " << animation << std::endl;
		}
	}

	void blit(){
		if( !texture ) return;
		
		int ox = x;
		int oy = y;
		if( !fixed ){
			ox -= game->cameraX;
			oy -= game->cameraY;
		}

		// std::cout << "blit " << ox << " " << oy << std::endl;
		if( !stretch ) game->blit(texture, ox, oy, mirror);
		else game->blit4X(texture, ox, oy, mirror);
	}
};

#endif
