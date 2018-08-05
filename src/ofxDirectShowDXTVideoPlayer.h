#pragma once

#include "ofMain.h"
#include "DXTShared.h"

class DirectShowDXTVideo;

class ofxDirectShowDXTVideoPlayer : public ofBaseVideoPlayer {

	public:

		ofxDirectShowDXTVideoPlayer();
		~ofxDirectShowDXTVideoPlayer();

		bool load(string path);
		void update();
		void writeToTexture(ofTexture& texture);
		void draw(int x, int y, int w, int h);
		void draw(int x, int y) { draw(x, y, getWidth(), getHeight()); }
		void close();
		void play();
		void pause();
		void stop();

		bool isFrameNew() const ;
		ofPixels & getPixels(); // @NOTE: return uncompressed pixels
		const ofPixels & getPixels() const;

		float getWidth() const;
		float getHeight() const;

		bool isPaused() const;
		bool isLoaded() const;
		bool isPlaying() const;

		bool setPixelFormat(ofPixelFormat pixelFormat);
		ofPixelFormat getPixelFormat() const;
		ofShader getShader();
		ofTexture * getTexture();

		float getPosition();
		float getSpeed();
		float getDuration();
		bool getIsMovieDone();

		void setPaused(bool bPause);
		void setPosition(float pct);
		void setVolume(float volume); // 0..1
		void setLoopState(ofLoopType state);
		void setSpeed(float speed);
		void setFrame(int frame);  // frame 0 = first frame...

		int getCurrentFrame() const;
		int getTotalFrames() const;
		ofLoopType getLoopState() const;

		void firstFrame();
		void nextFrame();
		void previousFrame();

	protected:

		int	m_height;
		int	m_width;
		DirectShowDXTVideo * m_player;
		ofShader m_shader;
		bool m_bShaderInitialized;
		ofPixels m_pix; // copy of compressed pixels
		ofTexture m_tex; // texture for pix
		DXTTextureFormat m_textureFormat;
};
