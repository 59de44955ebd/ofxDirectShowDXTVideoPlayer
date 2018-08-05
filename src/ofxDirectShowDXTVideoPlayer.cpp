// based on code written by Jeremy Rotsztain and Philippe Laurheret for Second Story, 2015

#include "ofMain.h"

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <stdint.h>

#include "ofxDirectShowDXTVideoPlayer.h"
#include "DirectShowDXTVideo.h"

#define STRINGIFY(x) #x

ofxDirectShowDXTVideoPlayer::ofxDirectShowDXTVideoPlayer(){
	m_player = NULL;
	m_bShaderInitialized = false;
	m_width = 0;
	m_height = 0;
}

ofxDirectShowDXTVideoPlayer::~ofxDirectShowDXTVideoPlayer(){
	close();
}

bool ofxDirectShowDXTVideoPlayer::load(string path) {

	path = ofToDataPath(path);

	ofTextureData texData;

	close();
	m_player = new DirectShowDXTVideo();
	bool bOK = m_player->loadMovieManualGraph(path);
	if (!bOK) {
		ofLogError("ofxDirectShowDXTVideoPlayer") << "Could not load video file";
		goto error;
	}

	m_width = m_player->getWidth();
	m_height = m_player->getHeight();

	if (m_width == 0 || m_height == 0) {
		ofLogError("ofxDirectShowDXTVideoPlayer") << "Failed to create OpenGL texture: width and/or height are zero";
		goto error;
	}


	texData.width = m_width;
	texData.height = m_height;
	texData.textureTarget = GL_TEXTURE_2D;

	m_textureFormat = m_player->getTextureFormat();

	if (m_textureFormat == TextureFormat_RGB_DXT1){
		texData.glInternalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		m_pix.allocate(m_width, m_height, OF_IMAGE_COLOR);
	}
	else if (m_textureFormat == TextureFormat_RGBA_DXT5){
		texData.glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		m_pix.allocate(m_width, m_height, OF_IMAGE_COLOR_ALPHA);
	}
	else if (m_textureFormat == TextureFormat_YCoCg_DXT5) {
		texData.glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		m_pix.allocate(m_width, m_height, OF_IMAGE_COLOR);
	}
	else {
		ofLogError("ofxDirectShowDXTVideoPlayer") << "Unknown texture format";
		goto error;
	}

	m_tex.allocate(texData, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);

//	GLenum err = glGetError();
//	if (err != GL_NO_ERROR) {
//		ofLogError("ofxDirectShowDXTVideoPlayer") << gluErrorString(err);
//		goto error;
//	}

	if (!m_bShaderInitialized){

         string vertexShader;
         if (ofIsGLProgrammableRenderer())
         {
            vertexShader = "#version 400\n" STRINGIFY(
                uniform mat4 modelViewMatrix;
                uniform mat4 modelViewProjectionMatrix;
                layout (location = 0) in vec4 position;
                layout (location = 3) in vec2 texcoord;
                out vec2 v_texcoord;

                void main() {
                    v_texcoord = texcoord;
                    gl_Position = modelViewProjectionMatrix * position;
                }
            );
         }
         else
         {
		    vertexShader = STRINGIFY(
				void main(void)
				{
                    gl_Position = ftransform();
                    gl_TexCoord[0] = gl_MultiTexCoord0;
				});
         }

         string fragmentShader;
         if (ofIsGLProgrammableRenderer())
         {
             fragmentShader = "#version 400\n" STRINGIFY(
				uniform sampler2D src_tex0;
                in vec2 v_texcoord;
				out vec4 v_fragColor;
                uniform float width;
                uniform float height;
			    const vec4 offsets = vec4(-0.50196078431373, -0.50196078431373, 0.0, 0.0);
				void main()
				{
					vec4 CoCgSY = texture2D(src_tex0, v_texcoord);
					CoCgSY += offsets;
					float scale = ( CoCgSY.z * ( 255.0 / 8.0 ) ) + 1.0;
					float Co = CoCgSY.x / scale;
					float Cg = CoCgSY.y / scale;
					float Y = CoCgSY.w;
					vec4 rgba = vec4(Y + Co - Cg, Y + Cg, Y - Co - Cg, 1.0);
					v_fragColor = rgba;
				});
         }
         else
         {
		    fragmentShader = STRINGIFY(
                uniform sampler2D cocgsy_src;
			    const vec4 offsets = vec4(-0.50196078431373, -0.50196078431373, 0.0, 0.0);
				void main()
				{
					vec4 CoCgSY = texture2D(cocgsy_src, gl_TexCoord[0].xy);
					CoCgSY += offsets;
					float scale = ( CoCgSY.z * ( 255.0 / 8.0 ) ) + 1.0;
					float Co = CoCgSY.x / scale;
					float Cg = CoCgSY.y / scale;
					float Y = CoCgSY.w;
					vec4 rgba = vec4(Y + Co - Cg, Y + Cg, Y - Co - Cg, 1.0);
					gl_FragColor = rgba;
				});
         }

		 bool bShaderOK = m_shader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);

		 if (bShaderOK){
			 bShaderOK = m_shader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragmentShader);
		 }
		 if (bShaderOK){
			 bShaderOK = m_shader.linkProgram();
		 }
		 if (bShaderOK){
			 m_bShaderInitialized = true;
		 }
		 else {
			 ofLogError("ofxDirectShowDXTVideoPlayer") << "Failed to setup shader";
			 goto error;
		 }

         if (ofIsGLProgrammableRenderer())
         {
			 m_shader.begin();
			 m_shader.setUniform1f("width", (float)m_width);
			 m_shader.setUniform1f("height", (float)m_height);
			 m_shader.end();
         }

	 }

	return true;

error:
	 if (m_player) {
		 delete m_player;
		 m_player = NULL;
	 }

	return false;
}

void ofxDirectShowDXTVideoPlayer::close(){
	stop();
	if (m_player){
		delete m_player;
		m_player = NULL;
	}
}

ofTexture * ofxDirectShowDXTVideoPlayer::getTexture() {
    return &this->m_tex;
}

void ofxDirectShowDXTVideoPlayer::update(){
	if(m_player && m_player->isLoaded() ){
        this->writeToTexture(this->m_tex);
		m_player->update();
	}
}

void ofxDirectShowDXTVideoPlayer::writeToTexture(ofTexture &texture) {
	m_player->getPixels(m_pix.getPixels());

    ofTextureData texData = texture.getTextureData();

    if (!ofIsGLProgrammableRenderer())
    {
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        glEnable(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, texData.textureID);

    GLint dataLength = 0;

    if (m_textureFormat == TextureFormat_RGB_DXT1){
        dataLength = m_width * m_height / 2;
    }
    else {
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &dataLength);
    }

    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, texData.glInternalFormat, dataLength, m_pix.getPixels());

//    GLenum err = glGetError();
//    if (err != GL_NO_ERROR){
//		ofLogError("ofxDirectShowDXTVideoPlayer") << gluErrorString(err);
//    }

    if (!ofIsGLProgrammableRenderer())
    {
        glPopClientAttrib();
        glDisable(GL_TEXTURE_2D);
    }
};

void ofxDirectShowDXTVideoPlayer::draw(int x, int y, int w, int h){
	if (m_textureFormat == TextureFormat_YCoCg_DXT5) m_shader.begin();
	m_tex.draw(x, y, w, h);
	if (m_textureFormat == TextureFormat_YCoCg_DXT5) m_shader.end();
}

void ofxDirectShowDXTVideoPlayer::play(){
	if(m_player && m_player->isLoaded() ){
        if (m_player->isPaused())
        {
			m_player->setPaused(false);
        }
        else
        {
			m_player->play();
        }
	}
}

void ofxDirectShowDXTVideoPlayer::pause(){
	if(m_player && m_player->isLoaded() && m_player->isPlaying()){
		m_player->setPaused(true);
	}
}

void ofxDirectShowDXTVideoPlayer::stop(){
	if(m_player && m_player->isLoaded() ){
		m_player->stop();
	}
}

bool ofxDirectShowDXTVideoPlayer::isFrameNew() const {
	return (m_player && m_player->isFrameNew() );
}

ofPixels & ofxDirectShowDXTVideoPlayer::getPixels(){
	return m_pix;
}

const ofPixels & ofxDirectShowDXTVideoPlayer::getPixels() const {
	return m_pix;
}

/*
unsigned char* ofxDirectShowDXTVideoPlayer::getPixels(){
	return pix.getPixels();
}
*/

float ofxDirectShowDXTVideoPlayer::getWidth() const  {
	if(m_player && m_player->isLoaded() ){
		return m_player->getWidth();
	}
	return 0.0;
}

float ofxDirectShowDXTVideoPlayer::getHeight() const {
	if(m_player && m_player->isLoaded() ){
		return m_player->getHeight();
	}
	return 0.0;
}

ofShader ofxDirectShowDXTVideoPlayer::getShader() {
    return this->m_shader;
}

bool ofxDirectShowDXTVideoPlayer::isPaused() const {
	return (m_player && m_player->isPaused() );
}

bool ofxDirectShowDXTVideoPlayer::isLoaded() const  {
	return (m_player && m_player->isLoaded() );
}

bool ofxDirectShowDXTVideoPlayer::isPlaying() const  {
	return (m_player && m_player->isPlaying() );
}

bool ofxDirectShowDXTVideoPlayer::setPixelFormat(ofPixelFormat pixelFormat){
	return (pixelFormat == OF_PIXELS_RGBA);
}

ofPixelFormat ofxDirectShowDXTVideoPlayer::getPixelFormat() const  {
	return OF_PIXELS_RGBA;
}

//should implement!
float ofxDirectShowDXTVideoPlayer::getPosition(){
	if(m_player && m_player->isLoaded() ){
		return m_player->getPosition();
	}
	return 0.0;
}

float ofxDirectShowDXTVideoPlayer::getSpeed(){
	if(m_player && m_player->isLoaded() ){
		return m_player->getSpeed();
	}
	return 0.0;
}

float ofxDirectShowDXTVideoPlayer::getDuration(){
	if(m_player && m_player->isLoaded() ){
		return m_player->getDurationInSeconds();
	}
	return 0.0;
}

bool ofxDirectShowDXTVideoPlayer::getIsMovieDone(){
	return (m_player && m_player->isMovieDone() );
}

void ofxDirectShowDXTVideoPlayer::setPaused(bool bPause){
	if(m_player && m_player->isLoaded() ){
		m_player->setPaused(bPause);
	}
}

void ofxDirectShowDXTVideoPlayer::setPosition(float pct){
	if(m_player && m_player->isLoaded() ) {
		m_player->setPosition(pct);
	}
}

void ofxDirectShowDXTVideoPlayer::setVolume(float volume){
	if(m_player && m_player->isLoaded() ){
		m_player->setVolume(volume);
	}
}

void ofxDirectShowDXTVideoPlayer::setLoopState(ofLoopType state){
	if(m_player){
		if( state == OF_LOOP_NONE ){
			m_player->setLoop(false);
		}
		else if( state == OF_LOOP_NORMAL ){
			m_player->setLoop(true);
		}else{
			ofLogError("ofDirectShowPlayer") << " cannot set loop of type palindrome " << endl;
		}
	}
}

void ofxDirectShowDXTVideoPlayer::setSpeed(float speed){
	if(m_player && m_player->isLoaded() ){
		m_player->setSpeed(speed);
	}
}

int	ofxDirectShowDXTVideoPlayer::getCurrentFrame() const {
	if(m_player && m_player->isLoaded() ){
		return m_player->getCurrentFrame();
	}
	return 0;
}

int	ofxDirectShowDXTVideoPlayer::getTotalFrames() const {
	if(m_player && m_player->isLoaded() ){
		return m_player->getTotalFrames();
	}
	return 0;
}

ofLoopType ofxDirectShowDXTVideoPlayer::getLoopState() const {
	if(m_player){
		if(m_player->isLooping() ){
			return OF_LOOP_NORMAL;
		}

	}
	return OF_LOOP_NONE;
}

void ofxDirectShowDXTVideoPlayer::setFrame(int frame){
	if(m_player && m_player->isLoaded() ){
		frame = ofClamp(frame, 0, getTotalFrames());
		return m_player->setFrame(frame);
	}
}  // frame 0 = first frame...

void ofxDirectShowDXTVideoPlayer::firstFrame(){
	setPosition(0.0);
}

void ofxDirectShowDXTVideoPlayer::nextFrame(){
	if(m_player && m_player->isLoaded() ){
		m_player->nextFrame();
	}
}

void ofxDirectShowDXTVideoPlayer::previousFrame(){
	if(m_player && m_player->isLoaded() ){
		m_player->previousFrame();
	}
}

