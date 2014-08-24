/*
Copyright (c) 2014, Paul Houx - All rights reserved.
This code is intended for use with the Cinder C++ library: http://libcinder.org

FXAA:
Copyright (c) 2010, 2011 NVIDIA CORPORATION. ALL RIGHTS RESERVED.

SMAA:
Copyright (c) 2013 Jorge Jimenez (jorge@iryoku.com)
Copyright (c) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
Copyright (c) 2013 Belen Masia (bmasia@unizar.es)
Copyright (c) 2013 Fernando Navarro (fernandn@microsoft.com)
Copyright (c) 2013 Diego Gutierrez (diegog@unizar.es)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Context.h"
#include "cinder/gl/Fbo.h"
#include "cinder/Camera.h"
#include "cinder/Timer.h"

#include "fxaa/FXAA.h"
#include "smaa/SMAA.h"
#include "Pistons.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class PostProcessingAAApp : public AppNative
{
public:
	void prepareSettings( Settings* settings ) override;

	void setup() override;
	void update() override;
	void draw() override;

	void render();

	void resize() override;
	void mouseMove( MouseEvent event ) override;
	void mouseDrag( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;

private:
	CameraPersp         mCamera;         // 3D camera to render our scene.
	Pistons             mPistons;        // Moving, colored pistons.

	Timer               mTimer;          // We can pause and resume our timer.
	double              mTime;
	double              mTimeOffset;

	gl::FboRef          mFboScene;       // Non-anti-aliased frame buffer to render our scene to.
	gl::FboRef          mFboFinal;       // Frame buffer that will contain the anti-aliased result.

	FXAA                mFXAA;           // Takes care of applying FXAA anti-aliasing to our scene.
	SMAA                mSMAA;           // Takes care of applying SMAA anti-aliasing to our scene.

	gl::TextureRef      mInfoFXAA;       // Info texture.
	gl::TextureRef      mInfoSMAA;       // Info texture.
	gl::TextureRef      mInfoOriginal;   // Info texture.

	Vec2i               mDivider;        // Determines which part of our scene is anti-aliased.
	int                 mDividerWidth;
	uint8_t             mPixelSize;      // Allows us to zoom in on the scene.
};

void PostProcessingAAApp::prepareSettings( Settings* settings )
{
	settings->disableFrameRate();
	settings->setWindowSize( 1280, 720 );
}

void PostProcessingAAApp::setup()
{
	try {
		mInfoFXAA = gl::Texture::create( loadImage( loadAsset( "fxaa.png" ) ) );
		mInfoSMAA = gl::Texture::create( loadImage( loadAsset( "smaa.png" ) ) );
		mInfoOriginal = gl::Texture::create( loadImage( loadAsset( "original.png" ) ) );
	}
	catch( const std::exception& ) {}

	mPistons.setup();
	mFXAA.setup();
	mSMAA.setup();

	mTimeOffset = 1.0;
	mTimer.start();

	mDivider = getWindowSize() / 2;
	mDividerWidth = getWindowWidth() / 4;
	mPixelSize = 1;
}

void PostProcessingAAApp::update()
{
	// Keep track of time.
	mTime = mTimer.getSeconds() + mTimeOffset;

	// Animate our camera.
	double t = mTime / 10.0;

	float phi = (float) t;
	float theta = 3.14159265f * ( 0.25f + 0.2f * math<float>::sin( phi * 0.9f ) );
	float x = 150.0f * math<float>::cos( phi ) * math<float>::cos( theta );
	float y = 150.0f * math<float>::sin( theta );
	float z = 150.0f * math<float>::sin( phi ) * math<float>::cos( theta );

	mCamera.setEyePoint( Vec3f( x, y, z ) );
	mCamera.setCenterOfInterestPoint( Vec3f( 1, 50, 0 ) );
	mCamera.setFov( 40.0f );

	// Update the pistons.
	mPistons.update( mCamera );
}

void PostProcessingAAApp::draw()
{
	// Render our scene to an Fbo.
	render();

	// Clear the main buffer.
	gl::clear();
	gl::color( Color::white() );

	// Draw non-anti-aliased scene.
	gl::draw( mFboScene->getColorTexture(), getWindowBounds() );

	// Draw FXAA-anti-aliased scene.
	mFXAA.apply( mFboFinal, mFboScene );

	gl::pushMatrices();
	gl::setMatricesWindow( mDividerWidth, getWindowHeight() );
	gl::pushViewport( mDivider.x - mDividerWidth, 0, mDividerWidth, getWindowHeight() );
	gl::draw( mFboFinal->getColorTexture(), getWindowBounds().getMoveULTo( Vec2f( -( mDivider.x - mDividerWidth ), 0 ) ) );
	gl::popViewport();
	gl::popMatrices();

	// Draw SMAA-anti-aliased scene.
	mSMAA.apply( mFboFinal, mFboScene );

	gl::pushMatrices();
	gl::setMatricesWindow( mDividerWidth, getWindowHeight() );
	gl::pushViewport( mDivider.x, 0, mDividerWidth, getWindowHeight() );
	gl::draw( mFboFinal->getColorTexture(), getWindowBounds().getMoveULTo( Vec2f( -mDivider.x, 0 ) ) );
	gl::popViewport();
	gl::popMatrices();

	// Draw divider.
	gl::drawLine( Vec2f( (float) mDivider.x, 0 ), Vec2f( (float) mDivider.x, (float) getWindowHeight() ) );
	gl::drawLine( Vec2f( (float) ( mDivider.x - mDividerWidth ), 0 ), Vec2f( (float) ( mDivider.x - mDividerWidth ), (float) getWindowHeight() ) );
	gl::drawLine( Vec2f( (float) ( mDivider.x + mDividerWidth ), 0 ), Vec2f( (float) ( mDivider.x + mDividerWidth ), (float) getWindowHeight() ) );
	
	// Draw info.
	gl::enableAlphaBlending();
	gl::draw( mInfoOriginal, Vec2f( mDivider.x - mDividerWidth * 3 / 2 - 128, 32 ) );
	gl::draw( mInfoFXAA, Vec2f( mDivider.x - mDividerWidth * 1 / 2 - 128, 32 ) );
	gl::draw( mInfoSMAA, Vec2f( mDivider.x + mDividerWidth * 1 / 2 - 128, 32 ) );
	gl::draw( mInfoOriginal, Vec2f( mDivider.x + mDividerWidth * 3 / 2 - 128, 32 ) );
	gl::disableAlphaBlending();
}

void PostProcessingAAApp::render()
{
	// Bind the Fbo. Automatically unbinds it at the end of this function.
	gl::ScopedFramebuffer fbo( mFboScene );

	// Clear the buffer.
	gl::clear( ColorA( 0, 0, 0, 0 ) );
	gl::color( Color::white() );

	// Render our scene.
	gl::pushViewport( 0, 0, mFboScene->getWidth(), mFboScene->getHeight() );
	mPistons.draw( mCamera, float( mTime ) );
	gl::popViewport();
}

void PostProcessingAAApp::resize()
{
	gl::Texture2d::Format tfmt;
	tfmt.setMinFilter( GL_NEAREST );
	tfmt.setMagFilter( GL_NEAREST );

	gl::Fbo::Format fmt;
	fmt.setColorTextureFormat( tfmt );

	// We want to store luminance in the alpha channel, 
	// as this yields the best FXAA results.
	// So make sure we have one.
	fmt.setColorBufferInternalFormat( GL_RGBA8 );

	mFboScene = gl::Fbo::create( getWindowWidth() / mPixelSize, getWindowHeight() / mPixelSize, fmt );
	mFboFinal = gl::Fbo::create( getWindowWidth() / mPixelSize, getWindowHeight() / mPixelSize, fmt );

	// Update the camera's aspect ratio.
	mCamera.setAspectRatio( getWindowAspectRatio() );

	//
	mDividerWidth = getWindowWidth() / 4;
}

void PostProcessingAAApp::mouseMove( MouseEvent event )
{
	mDivider = event.getPos();
}

void PostProcessingAAApp::mouseDrag( MouseEvent event )
{
	mDivider = event.getPos();
}

void PostProcessingAAApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_ESCAPE:
		quit();
		break;
	case KeyEvent::KEY_SPACE:
		// Start/stop the animation
		if( mTimer.isStopped() ) {
			mTimeOffset += mTimer.getSeconds();
			mTimer.start();
		}
		else
			mTimer.stop();
		break;
		/*case KeyEvent::KEY_1:
			mMode = Mode::EDGE_DETECTION;
			break;
			case KeyEvent::KEY_2:
			mMode = Mode::BLEND_WEIGHTS;
			break;
			case KeyEvent::KEY_3:
			mMode = Mode::BLEND_NEIGHBORS;
			break;*/
	case KeyEvent::KEY_v:
		if( gl::isVerticalSyncEnabled() )
			gl::enableVerticalSync( false );
		else
			gl::enableVerticalSync();
		break;
	case KeyEvent::KEY_UP:
		if( mPixelSize < 8 ) {
			mPixelSize <<= 1; resize();
		}
		break;
	case KeyEvent::KEY_DOWN:
		if( mPixelSize > 1 ) {
			mPixelSize >>= 1; resize();
		}
		break;

	}
}

CINDER_APP_NATIVE( PostProcessingAAApp, RendererGl )
