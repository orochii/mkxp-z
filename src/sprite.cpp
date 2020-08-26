/*
** sprite.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 Jonas Kulla <Nyocurio@gmail.com>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sprite.h"

#include "sharedstate.h"
#include "bitmap.h"
#include "etc.h"
#include "etc-internal.h"
#include "util.h"

#include "gl-util.h"
#include "quad.h"
#include "transform.h"
#include "shader.h"
#include "glstate.h"
#include "quadarray.h"

#include <math.h>
#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#include <SDL_rect.h>

#include <sigc++/connection.h>

struct SpritePrivate
{
	Bitmap *bitmap;

	Quad quad;
	Transform trans;

	Rect *srcRect;
	sigc::connection srcRectCon;

	bool mirrored;
	int bushDepth;
	float efBushDepth;
	NormValue bushOpacity;
	NormValue opacity;
	BlendType blendType;

	IntRect sceneRect;
	Vec2i sceneOrig;

	/* Would this sprite be visible on
	 * the screen if drawn? */
	bool isVisible;

	Color *color;
	Tone *tone;

	struct
	{
		int amp;
		int length;
		float speed;
		float phase;
		int mode;
		int size;

		/* Wave effect is active (amp != 0) */
		bool active;
		/* qArray needs updating */
		bool dirty;
		SimpleQuadArray qArray;
	} wave;

	EtcTemps tmp;

	sigc::connection prepareCon;

	SpritePrivate()
	    : bitmap(0),
	      srcRect(&tmp.rect),
	      mirrored(false),
	      bushDepth(0),
	      efBushDepth(0),
	      bushOpacity(128),
	      opacity(255),
	      blendType(BlendNormal),
	      isVisible(false),
	      color(&tmp.color),
	      tone(&tmp.tone)

	{
		sceneRect.x = sceneRect.y = 0;

		updateSrcRectCon();

		prepareCon = shState->prepareDraw.connect
		        (sigc::mem_fun(this, &SpritePrivate::prepare));

		wave.amp = 0;
		wave.length = 180;
		wave.speed = 360;
		wave.phase = 0.0f;
		wave.dirty = false;
		wave.mode = 0;
		wave.size = 8;
	}

	~SpritePrivate()
	{
		srcRectCon.disconnect();
		prepareCon.disconnect();
	}

	void recomputeBushDepth()
	{
		if (nullOrDisposed(bitmap))
			return;

		/* Calculate effective (normalized) bush depth */
		float texBushDepth = (bushDepth / trans.getScale().y) -
		                     (srcRect->y + srcRect->height) +
		                     bitmap->height();

		efBushDepth = 1.0f - texBushDepth / bitmap->height();
	}

	void onSrcRectChange()
	{
		FloatRect rect = srcRect->toFloatRect();
		Vec2i bmSize;

		if (!nullOrDisposed(bitmap))
			bmSize = Vec2i(bitmap->width(), bitmap->height());

		/* Clamp the rectangle so it doesn't reach outside
		 * the bitmap bounds */
		rect.w = clamp<int>(rect.w, 0, bmSize.x-rect.x);
		rect.h = clamp<int>(rect.h, 0, bmSize.y-rect.y);

		quad.setTexRect(mirrored ? rect.hFlipped() : rect);

		quad.setPosRect(FloatRect(0, 0, rect.w, rect.h));
		recomputeBushDepth();

		wave.dirty = true;
	}

	void updateSrcRectCon()
	{
		/* Cut old connection */
		srcRectCon.disconnect();
		/* Create new one */
		srcRectCon = srcRect->valueChanged.connect
				(sigc::mem_fun(this, &SpritePrivate::onSrcRectChange));
	}

	void updateVisibility()
	{
		isVisible = false;

		if (nullOrDisposed(bitmap))
			return;

		if (!opacity)
			return;

		if (wave.active)
		{
			/* Don't do expensive wave bounding box
			 * calculations */
			isVisible = true;
			return;
		}

		/* Compare sprite bounding box against the scene */

		/* If sprite is zoomed/rotated, just opt out for now
		 * for simplicity's sake */
		const Vec2 &scale = trans.getScale();
		if (scale.x != 1 || scale.y != 1 || trans.getRotation() != 0)
		{
			isVisible = true;
			return;
		}

		IntRect self;
		self.setPos(trans.getPositionI() - (trans.getOriginI() + sceneOrig));
		self.w = bitmap->width();
		self.h = bitmap->height();

		isVisible = SDL_HasIntersection(&self, &sceneRect);
	}
	/*	
	*	Horizontal wave chunk emission
	*	Used by "traditional" wave effects.
	*/	
	void emitHorzWaveChunk(SVertex *&vert, float phase, int width,
	                   float zoomY, int chunkY, int chunkLength, int index)
	{
		float wavePos = phase + (chunkY / (float) wave.length) * (float) (M_PI * 2);
		float chunkX = sin(wavePos) * wave.amp;

		FloatRect tex(0, chunkY / zoomY, width, chunkLength / zoomY);
		FloatRect pos = tex;
		switch(wave.mode) {
			case 1: // Vertical move, normal
				tex.y += chunkX;
				break;
			case 2: // Horizontal move, interlaced
				if (index % 2 == 0){
					pos.x = chunkX;
				} else {
					pos.x = -chunkX;
				}
				break;
			case 3: // Vertical move, interlaced
				if (index % 2 == 0){
					tex.y += chunkX;
				} else {
					tex.y -= chunkX;
				}
				break;
			default: // Horizontal move, normal
				pos.x = chunkX;
		}
		// quad.setTexRect(mirrored ? rect.hFlipped() : rect);
		Quad::setTexPosRect(vert, mirrored ? tex.hFlipped() : tex, pos);
		vert += 4;
	}
	/*	
	*	Vertical wave chunk emission
	*	Variation of the traditional wave, made because I can.
	*/	
	void emitVertWaveChunk(SVertex *&vert, float phase, int height,
	                   float zoomX, int chunkX, int chunkWidth, int index)
	{
		float wavePos = phase + (chunkX / (float) wave.length) * (float) (M_PI * 2);
		float chunkY = sin(wavePos) * wave.amp;

		FloatRect tex(chunkX / zoomX, 0, chunkWidth / zoomX, height);
		FloatRect pos = tex;
		switch(wave.mode) {
			case 5: // Horizontal move, normal
				tex.x += chunkY;
				break;
			case 6: // Vertical move, interlaced
				if (index % 2 == 0){
					pos.y = chunkY;
				} else {
					pos.y = -chunkY;
				}
				break;
			case 7: // Horizontal move, interlaced
				if (index % 2 == 0){
					tex.x += chunkY;
				} else {
					tex.x -= chunkY;
				}
				break;
			default: // Vertical move, normal
				pos.y = chunkY;
		}
		// quad.setTexRect(mirrored ? rect.hFlipped() : rect);
		Quad::setTexPosRect(vert, mirrored ? tex.hFlipped() : tex, pos);
		vert += 4;
	}
	/*	
	*	Effect chunk emission
	*	Square-ish chunks for visual effects.
	*/	
	void emitEffectChunk(SVertex *&vert, float phase, int width, int height, 
						float zoomX, float zoomY, int chunkX, int chunkY, int ix, int iy, int tX, int tY)
	{
		FloatRect tex(chunkX / zoomX, chunkY / zoomY, width / zoomX, height / zoomY);
		FloatRect pos = tex;
		if (mirrored) pos.x = (tX - ix) * (width/zoomX);
		switch(wave.mode) {
			case 9:
				{
					float idx = (ix + (tX/4) * iy); // (tX * tY) - 
					float dsp = (phase*wave.length - idx);
					float xDsp = sin(phase + ix) * wave.amp;
					if (dsp < 0) {
						dsp = 0;
						xDsp = 0;
					}
					pos.x += xDsp;
					pos.y -= dsp;
				}
				break;
			default: // Explode
				{
					float dst = (wave.amp * phase) + (wave.length * phase * phase)/2;
					float idx = (tX * tY) - (ix + tX * iy);
					float dsp = idx * phase;
					float midX = ((float)ix - (tX/2)) / tX;
					float midY = ((float)tY - iy - 1) / tY;
					pos.x += dsp * midX * dst * (mirrored ? -1 : 1); //  * wave.amp / 180.0f
					pos.y -= dsp * midY * dst; //  * wave.length / 180.0f
				}
		}
		// quad.setTexRect(mirrored ? rect.hFlipped() : rect);
		Quad::setTexPosRect(vert, mirrored ? tex.hFlipped() : tex, pos);
		vert += 4;
	}
	// wave.mode >3: sprite effects (explode, etc)
	void updateWave()
	{
		if (nullOrDisposed(bitmap))
			return;

		if (wave.amp == 0)
		{
			wave.active = false;
			return;
		}
		/* Enable wave */
		wave.active = true;
		/* Get general use properties */
		int width = srcRect->width;
		int height = srcRect->height;
		float zoomX = trans.getScale().x;
		float zoomY = trans.getScale().y;
		if (wave.amp < -(width / 2))
		{
			wave.qArray.resize(0);
			wave.qArray.commit();

			return;
		}

		/* RMVX does this, and I have no fucking clue why */
		if (wave.amp < 0)
		{
			wave.qArray.resize(1);

			int x = -wave.amp;
			int w = width - x * 2;

			FloatRect tex(x, srcRect->y, w, srcRect->height);

			Quad::setTexPosRect(&wave.qArray.vertices[0], tex, tex);
			wave.qArray.commit();

			return;
		}
		/* CASE: HORIZONTAL WAVES */
		if (wave.mode < 4) {
			/* Vertical chunks */
			int visibleLength = height * zoomY; 							/* The length of the sprite as it appears on screen */
			int firstLength = ((int) trans.getPosition().y) % wave.size; 	/* First chunk length (aligned to wave.size pixel boundary) */
			int vchunks = (visibleLength - firstLength) / wave.size; 		/* Amount of full wave.size pixel vchunks in the middle */
			int lastLength = (visibleLength - firstLength) % wave.size; 	/* Final chunk length */
			int vertChunks = !!firstLength + vchunks + !!lastLength;
			/* Get total chunks. */
			int totalChunks = vertChunks;
			/* Resize array */
			wave.qArray.resize(totalChunks);
			/* Create reusable vert reference */
			SVertex *vert = &wave.qArray.vertices[0];
			/* Calculate phase value (radians) */
			float phase = (wave.phase * (float) M_PI) / 180.0f;
			/* Emit first chunk */
			if (firstLength > 0)
				emitHorzWaveChunk(vert, phase, width, zoomY, 0, firstLength, 0);
			/* Emit middle chunks */
			for (int i = 0; i < vchunks; ++i)
				emitHorzWaveChunk(vert, phase, width, zoomY, firstLength + i * wave.size, wave.size, i+1);
			/* Emit last chunk */
			if (lastLength > 0)
				emitHorzWaveChunk(vert, phase, width, zoomY, firstLength + vchunks * wave.size, lastLength, vchunks);
			/* Commit changes to quad array */
			wave.qArray.commit();
		} 
		/* CASE: VERTICAL WAVES */
		else if (wave.mode < 8) {
			/* Horizontal chunks */
			int visibleWidth = width * zoomX;
			int firstWidth = ((int) trans.getPosition().x) % wave.size;		/* The width of the sprite as it appears on screen */
			int hchunks = (visibleWidth - firstWidth) / wave.size;			/* First chunk width (aligned to wave.size pixel boundary) */
			int lastWidth = (visibleWidth - firstWidth) % wave.size;		/* Amount of full wave.size pixel hchunks in the middle */
			int horzChunks = !!firstWidth + hchunks + !!lastWidth;			/* Final chunk width */
			/* Get total chunks. */
			int totalChunks = horzChunks;
			/* Resize array */
			wave.qArray.resize(totalChunks);
			/* Create reusable vert reference */
			SVertex *vert = &wave.qArray.vertices[0];
			/* Calculate phase value (radians) */
			float phase = (wave.phase * (float) M_PI) / 180.0f;
			/* Emit first chunk */
			if (firstWidth > 0)
				emitVertWaveChunk(vert, phase, height, zoomX, 0, firstWidth, 0);
			/* Emit middle chunks */
			for (int i = 0; i < hchunks; ++i)
				emitVertWaveChunk(vert, phase, height, zoomX, firstWidth + i * wave.size, wave.size, i+1);
			/* Emit last chunk */
			if (lastWidth > 0)
				emitVertWaveChunk(vert, phase, height, zoomX, firstWidth + hchunks * wave.size, lastWidth, hchunks);
			/* Commit changes to quad array */
			wave.qArray.commit();
		} 
		/* CASE: SPRITE EFFECTS */
		else {
			/* Vertical chunks */
			int visibleLength = height * zoomY; 							/* The length of the sprite as it appears on screen */
			int firstLength = ((int) trans.getPosition().y) % wave.size; 	/* First chunk length (aligned to wave.size pixel boundary) */
			int vchunks = (visibleLength - firstLength) / wave.size; 		/* Amount of full wave.size pixel vchunks in the middle */
			int lastLength = (visibleLength - firstLength) % wave.size; 	/* Final chunk length */
			int vertChunks = !!firstLength + vchunks + !!lastLength;
			/* Horizontal chunks */
			int visibleWidth = width * zoomX;
			int firstWidth = ((int) trans.getPosition().x) % wave.size;		/* The width of the sprite as it appears on screen */
			int hchunks = (visibleWidth - firstWidth) / wave.size;			/* First chunk width (aligned to wave.size pixel boundary) */
			int lastWidth = (visibleWidth - firstWidth) % wave.size;		/* Amount of full wave.size pixel hchunks in the middle */
			int horzChunks = !!firstWidth + hchunks + !!lastWidth;			/* Final chunk width */
			/* Get total chunks. */
			int totalChunks = horzChunks * vertChunks;
			/* Resize array */
			wave.qArray.resize(totalChunks);
			/* Create reusable vert reference */
			SVertex *vert = &wave.qArray.vertices[0];
			/* Get fraction of phase */
			float phase = wave.phase / 180.0f;
			/* Emit first line of chunks */
			if (firstLength > 0) {
				/* Emit first chunk */
				// emitEffectChunk(vert, phase, width, height, zoomX, zoomY, chunkX, chunkY, index)
				// emitEffectChunk(vert, phase, height, zoomX, chunkX, chunkWidth, index)
				if (firstWidth > 0)
					emitEffectChunk(vert, phase, firstWidth, firstLength, zoomX, zoomY, 0, 0, 0, 0, horzChunks, vertChunks);
				/* Emit middle chunks */
				for (int i = 0; i < hchunks; ++i)
					emitEffectChunk(vert, phase, wave.size, firstLength, zoomX, zoomY, firstWidth + i * wave.size, 0, i+1, 0, horzChunks, vertChunks);
				/* Emit last chunk */
				if (lastWidth > 0)
					emitEffectChunk(vert, phase, lastWidth, firstLength, zoomX, zoomY, firstWidth + hchunks * wave.size, 0, hchunks, 0, horzChunks, vertChunks);
			}
			/* Emit middle lines of chunks */
			for (int j = 0; j < vchunks; ++j) {
				float currChunkY = firstLength + j * wave.size;
				/* Emit first chunk */
				// emitEffectChunk(vert, phase, width, height, zoomX, zoomY, chunkX, chunkY, index)
				// emitEffectChunk(vert, phase, height, zoomX, chunkX, chunkWidth, index)
				if (firstWidth > 0)
					emitEffectChunk(vert, phase, firstWidth, wave.size, zoomX, zoomY, 
									0, 									currChunkY, 0, 			j+1, horzChunks, vertChunks);
				/* Emit middle chunks */
				for (int i = 0; i < hchunks; ++i)
					emitEffectChunk(vert, phase, wave.size, wave.size, zoomX, zoomY, 
									firstWidth + i * wave.size, 		currChunkY, i+1, 		j+1, horzChunks, vertChunks);
				/* Emit last chunk */
				if (lastWidth > 0)
					emitEffectChunk(vert, phase, lastWidth, wave.size, zoomX, zoomY, 
									firstWidth + hchunks * wave.size, 	currChunkY, hchunks, 	j+1, horzChunks, vertChunks);
			}
			/* Emit last line of chunks */
			float lastChunkY = firstLength + vchunks * wave.size;
			if (lastLength > 0) {
				/* Emit first chunk */
				// emitEffectChunk(vert, phase, width, height, zoomX, zoomY, chunkX, chunkY, index)
				// emitEffectChunk(vert, phase, height, zoomX, chunkX, chunkWidth, index)
				if (firstWidth > 0)
					emitEffectChunk(vert, phase, firstWidth, wave.size, zoomX, zoomY, 
									0, 									lastChunkY, 0, 			vchunks, horzChunks, vertChunks);
				/* Emit middle chunks */
				for (int i = 0; i < hchunks; ++i)
					emitEffectChunk(vert, phase, wave.size, wave.size, zoomX, zoomY, 
									firstWidth + i * wave.size, 		lastChunkY, i+1, 		vchunks, horzChunks, vertChunks);
				/* Emit last chunk */
				if (lastWidth > 0)
					emitEffectChunk(vert, phase, lastWidth, wave.size, zoomX, zoomY, 
									firstWidth + hchunks * wave.size, 	lastChunkY, hchunks, 	vchunks, horzChunks, vertChunks);
			}
			/* Commit changes to quad array */
			wave.qArray.commit();
		}
	}

	void prepare()
	{
		if (wave.dirty)
		{
			updateWave();
			wave.dirty = false;
		}

		updateVisibility();
	}
};

Sprite::Sprite(Viewport *viewport)
    : ViewportElement(viewport)
{
	p = new SpritePrivate;
	onGeometryChange(scene->getGeometry());
}

Sprite::~Sprite()
{
	dispose();
}

DEF_ATTR_RD_SIMPLE(Sprite, Bitmap,     Bitmap*, p->bitmap)
DEF_ATTR_RD_SIMPLE(Sprite, X,          int,     p->trans.getPosition().x)
DEF_ATTR_RD_SIMPLE(Sprite, Y,          int,     p->trans.getPosition().y)
DEF_ATTR_RD_SIMPLE(Sprite, OX,         int,     p->trans.getOrigin().x)
DEF_ATTR_RD_SIMPLE(Sprite, OY,         int,     p->trans.getOrigin().y)
DEF_ATTR_RD_SIMPLE(Sprite, ZoomX,      float,   p->trans.getScale().x)
DEF_ATTR_RD_SIMPLE(Sprite, ZoomY,      float,   p->trans.getScale().y)
DEF_ATTR_RD_SIMPLE(Sprite, Angle,      float,   p->trans.getRotation())
DEF_ATTR_RD_SIMPLE(Sprite, Mirror,     bool,    p->mirrored)
DEF_ATTR_RD_SIMPLE(Sprite, BushDepth,  int,     p->bushDepth)
DEF_ATTR_RD_SIMPLE(Sprite, BlendType,  int,     p->blendType)
DEF_ATTR_RD_SIMPLE(Sprite, Width,      int,     p->srcRect->width)
DEF_ATTR_RD_SIMPLE(Sprite, Height,     int,     p->srcRect->height)
DEF_ATTR_RD_SIMPLE(Sprite, WaveAmp,    int,     p->wave.amp)
DEF_ATTR_RD_SIMPLE(Sprite, WaveLength, int,     p->wave.length)
DEF_ATTR_RD_SIMPLE(Sprite, WaveSpeed,  float,   p->wave.speed)
DEF_ATTR_RD_SIMPLE(Sprite, WavePhase,  float,   p->wave.phase)
DEF_ATTR_RD_SIMPLE(Sprite, WaveMode,   int,     p->wave.mode)
DEF_ATTR_RD_SIMPLE(Sprite, WaveSize,   int  ,   p->wave.size)

DEF_ATTR_SIMPLE(Sprite, BushOpacity, int,     p->bushOpacity)
DEF_ATTR_SIMPLE(Sprite, Opacity,     int,     p->opacity)
DEF_ATTR_SIMPLE(Sprite, SrcRect,     Rect&,  *p->srcRect)
DEF_ATTR_SIMPLE(Sprite, Color,       Color&, *p->color)
DEF_ATTR_SIMPLE(Sprite, Tone,        Tone&,  *p->tone)

void Sprite::setBitmap(Bitmap *bitmap)
{
	guardDisposed();

	if (p->bitmap == bitmap)
		return;

	p->bitmap = bitmap;

	if (nullOrDisposed(bitmap))
		return;

	bitmap->ensureNonMega();

	*p->srcRect = bitmap->rect();
	p->onSrcRectChange();
	p->quad.setPosRect(p->srcRect->toFloatRect());

	p->wave.dirty = true;
}

void Sprite::setX(int value)
{
	guardDisposed();

	if (p->trans.getPosition().x == value)
		return;

	p->trans.setPosition(Vec2(value, getY()));
}

void Sprite::setY(int value)
{
	guardDisposed();

	if (p->trans.getPosition().y == value)
		return;

	p->trans.setPosition(Vec2(getX(), value));

	if (rgssVer >= 2)
	{
		p->wave.dirty = true;
		setSpriteY(value);
	}
}

void Sprite::setOX(int value)
{
	guardDisposed();

	if (p->trans.getOrigin().x == value)
		return;

	p->trans.setOrigin(Vec2(value, getOY()));
}

void Sprite::setOY(int value)
{
	guardDisposed();

	if (p->trans.getOrigin().y == value)
		return;

	p->trans.setOrigin(Vec2(getOX(), value));
}

void Sprite::setZoomX(float value)
{
	guardDisposed();

	if (p->trans.getScale().x == value)
		return;

	p->trans.setScale(Vec2(value, getZoomY()));
}

void Sprite::setZoomY(float value)
{
	guardDisposed();

	if (p->trans.getScale().y == value)
		return;

	p->trans.setScale(Vec2(getZoomX(), value));
	p->recomputeBushDepth();

	if (rgssVer >= 2)
		p->wave.dirty = true;
}

void Sprite::setAngle(float value)
{
	guardDisposed();

	if (p->trans.getRotation() == value)
		return;

	p->trans.setRotation(value);
}

void Sprite::setMirror(bool mirrored)
{
	guardDisposed();

	if (p->mirrored == mirrored)
		return;

	p->mirrored = mirrored;
	p->onSrcRectChange();
}

void Sprite::setBushDepth(int value)
{
	guardDisposed();

	if (p->bushDepth == value)
		return;

	p->bushDepth = value;
	p->recomputeBushDepth();
}

void Sprite::setBlendType(int type)
{
	guardDisposed();

	switch (type)
	{
	default :
	case BlendNormal :
		p->blendType = BlendNormal;
		return;
	case BlendAddition :
		p->blendType = BlendAddition;
		return;
	case BlendSubstraction :
		p->blendType = BlendSubstraction;
		return;
	}
}

#define DEF_WAVE_SETTER(Name, name, type) \
	void Sprite::setWave##Name(type value) \
	{ \
		guardDisposed(); \
		if (p->wave.name == value) \
			return; \
		p->wave.name = value; \
		p->wave.dirty = true; \
	}

DEF_WAVE_SETTER(Amp,    amp,    int)
DEF_WAVE_SETTER(Length, length, int)
DEF_WAVE_SETTER(Speed,  speed,  float)
DEF_WAVE_SETTER(Phase,  phase,  float)
DEF_WAVE_SETTER(Mode,  mode,  int)
DEF_WAVE_SETTER(Size,  size,  int)

#undef DEF_WAVE_SETTER

void Sprite::initDynAttribs()
{
	p->srcRect = new Rect;
	p->color = new Color;
	p->tone = new Tone;

	p->updateSrcRectCon();
}

/* Flashable */
void Sprite::update()
{
	guardDisposed();

	Flashable::update();

	p->wave.phase += p->wave.speed / 180;
	p->wave.dirty = true;
}

/* SceneElement */
void Sprite::draw()
{
	if (!p->isVisible)
		return;

	if (emptyFlashFlag)
		return;

	ShaderBase *base;

	bool renderEffect = p->color->hasEffect() ||
	                    p->tone->hasEffect()  ||
	                    flashing              ||
	                    p->bushDepth != 0;

	if (renderEffect)
	{
		SpriteShader &shader = shState->shaders().sprite;

		shader.bind();
		shader.applyViewportProj();
		shader.setSpriteMat(p->trans.getMatrix());

		shader.setTone(p->tone->norm);
		shader.setOpacity(p->opacity.norm);
		shader.setBushDepth(p->efBushDepth);
		shader.setBushOpacity(p->bushOpacity.norm);

		/* When both flashing and effective color are set,
		 * the one with higher alpha will be blended */
		const Vec4 *blend = (flashing && flashColor.w > p->color->norm.w) ?
			                 &flashColor : &p->color->norm;

		shader.setColor(*blend);

		base = &shader;
	}
	else if (p->opacity != 255)
	{
		AlphaSpriteShader &shader = shState->shaders().alphaSprite;
		shader.bind();

		shader.setSpriteMat(p->trans.getMatrix());
		shader.setAlpha(p->opacity.norm);
		shader.applyViewportProj();
		base = &shader;
	}
	else
	{
		SimpleSpriteShader &shader = shState->shaders().simpleSprite;
		shader.bind();

		shader.setSpriteMat(p->trans.getMatrix());
		shader.applyViewportProj();
		base = &shader;
	}

	glState.blendMode.pushSet(p->blendType);

	p->bitmap->bindTex(*base);

	if (p->wave.active)
		p->wave.qArray.draw();
	else
		p->quad.draw();

	glState.blendMode.pop();
}

void Sprite::onGeometryChange(const Scene::Geometry &geo)
{
	/* Offset at which the sprite will be drawn
	 * relative to screen origin */
	p->trans.setGlobalOffset(geo.offset());

	p->sceneRect.setSize(geo.rect.size());
	p->sceneOrig = geo.orig;
}

void Sprite::releaseResources()
{
	unlink();

	delete p;
}
