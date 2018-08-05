#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){

	// setup the GL context

	// window mode
	ofSetupOpenGL(1280, 720, OF_WINDOW);

	// fullscreen mode
	//ofSetupOpenGL(1920, 1080, OF_FULLSCREEN);

	ofRunApp(new ofApp());
}
