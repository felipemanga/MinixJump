#ifndef PNGLOADER_H
#define PNGLOADER_H

class PNGLoader {
public:
	char *data;
	const char *error;
	int width, height;
	static PNGLoader* load(const char *file);
	virtual ~PNGLoader(){}
	void forget(){ data=NULL; }
};

#endif

