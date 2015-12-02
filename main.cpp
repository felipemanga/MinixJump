#include <stdio.h>
#include <iostream>
#include <memory>
#include <stdlib.h>
#include "XGame.h"
#include <X11/keysym.h>
#include <list>
#include <vector>
#include <algorithm>
#include <functional>

std::vector<std::string> alienColors = {"Yellow", "Blue", "Beige", "Green", "Pink"};

class Block;

std::list<Block *> blocks;
int cameraYTarget, maxPoint = 0xFFFFFF, maxZ=0;

class Block : public Sprite {
public:
	int ty;
	int tx;

	Block( XGame *g ) : Sprite(g){
		blocks.push_back(this);
		z = maxZ--;
	}

	virtual void update(){
		if( ty - cameraYTarget > 600 ){
			ty -= 600;
			tx = rand()%10*70;
			z = maxZ--;
		}

		y -= (y - ty) * 0.3;
		x -= (x - tx) * 0.3;

		Sprite::update();
	}

	virtual ~Block(){
		blocks.remove(this);
	}
};


class Player : public Sprite {
public:
	int walkSpeed;
	bool jumping, alive;
	int jumpStartSpeed;
	bool keys[0xFFFF];
	std::string color;
	
	int h, w;

	Player( XGame *g ) : Sprite(g){
		frameTime = 3;
		color = alienColors[rand()%alienColors.size()];

		walkSpeed = 18;
		jumpStartSpeed = 60;
		alive = true;
		stretch = false;
		jumping = false;
		y = 400;
		x = 70*6;

		for( int i=0; i<0xFFFF; ++i ) keys[i]=false;

		texture = game->loadSpritesheet("aliens");
		stateIdle();

		XGameTexture *t = game->getTexture(texture);
		applyAnimation();
		h = t->cropHeight;
		w = t->cropWidth;

		g->onKeyDown = std::bind(&Player::onKeyDown, this, std::placeholders::_1);
		g->onKeyUp = std::bind(&Player::onKeyUp, this, std::placeholders::_1);
	}

	void stateIdle(){
		animation = color + "_idle";
	}

	void stateRun(bool mirror){
		frame = loopStart = 1;
		loopEnd = 3;
		this->mirror = mirror;
		animation = color + "_walk";
	}

	void stateJump(){
		frame = loopStart = 0;
		loopEnd = 1;
		velY = -jumpStartSpeed;
		animation = color + "_jump";
		jumping = true;
	}

	virtual void update(){
		if( velX < 0 && x < -w ) x = 800;
		if( velX > 0 && x > 800 ) x = -w;
		velY += 10;
		if( alive ){
			if( velY > 0 ){
				jumping = true;
				for( auto it=blocks.begin(); it != blocks.end(); ++it ){
					auto block=*it;
					if( 
					block->y >= y+h-4 && block->y < y+velY+h
					&& block->x+w*0.7f >= x && block->x <= x + w*0.7f
					){
						velY = 0;
						jumping = false;
						y = block->y - h;
						// std::cout << "Points: " << maxPoint << " / " << y << std::endl;
						if( maxPoint > y-398 ) maxPoint = y-398;
						applyInput();
						break;
					}
				}

				if( jumping && y - cameraYTarget > 700 )
					alive = false;

				if( !jumping && y - cameraYTarget < 100 )
					cameraYTarget -= 400;
			}
		} else {
			cameraYTarget = y;
			if( game->cameraY > 800 ) game->gameOver = true;
		}
		
		game->cameraY -= (game->cameraY - cameraYTarget) * 0.3;

		z = maxZ-1;
		Sprite::update();
	}

	void applyInput(){
		if( keys[XK_Left] ){
			velX = -walkSpeed;
			mirror = true;
			if( !jumping ) stateRun(true);
		} else if( keys[XK_Right] ){
			velX = walkSpeed;
			mirror = false;
			if( !jumping ) stateRun(false);
		} else {
			velX *= 0.5;
		}
		if( !jumping ){
			if(velX==0 && velY==0){
				stateIdle();
			}
			if( keys[XK_space] ){
				stateJump();
			}
		}
	}

	void onKeyDown( int key ){
		if( key == XK_Escape ) game->running = false;
		if( !alive ) return;
		keys[key] = true;
		applyInput();
	}

	void onKeyUp( int key ){
		if( !alive ) return;

		keys[key] = false;
		if( !jumping ) applyInput();
	}

};

void create(XGame *game){
	Sprite *bg = new Sprite(game);
	bg->texture = game->loadTexture("bg.png");
	bg->z = 0x7FFFFFFF;
	bg->fixed = true;

	cameraYTarget = 0;
	game->cameraY = game->cameraX = 0;

	Block *b = new Block(game);
	int blockTex = b->texture = game->loadTexture("castle.png");
	b->tx = b->x = 70*6;
	b->ty = b->y = 70*7;

	for( int i=0; i<15; ++i ){
		b = new Block(game);
		b->texture = blockTex;
		b->tx = b->x = rand()%10*70;
		b->ty = rand()%8*70;
		b->y = - 70 - rand()%10*70;
	}

	new Player(game);
}

int main(){
	srand(time(NULL));
	try{
		auto game = std::unique_ptr<XGame>( XGame::create() );

		game->init();
		game->fps = 24;
		game->initScene = create;

		game->run();

	}catch( XGameError xge ){
		std::cout << "ERROR: " << xge.message << std::endl;
		return -1;
	}

	std::cout << "Pontuacao maxima: " << -maxPoint << std::endl;

	return 0;
}
