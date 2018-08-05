#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){

	//dxtPlayer.load("sample-1080p30-HapA.avi");
	//dxtPlayer.load("sample-1080p30-HapQ.avi");

	if (!dxtPlayer.load("sample-1080p30-Hap.avi")) {
		ofLogError("ofApp") << "Error loading sample-1080p30-Hap.avi";
		ofExit(1);
	}

	dxtPlayer.play();
}

//--------------------------------------------------------------
void ofApp::update(){
	dxtPlayer.update();
}

//--------------------------------------------------------------
void ofApp::draw(){
	ofSetColor(255);
	dxtPlayer.draw(0, 0, m_width, m_height);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
	m_width = w;
	m_height = h;
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){

}
