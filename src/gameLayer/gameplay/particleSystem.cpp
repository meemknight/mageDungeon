#include "particleSystem.h"
#include <gameplay/Physics.h>
#include <iostream>
#include <gameLayer.h>

void ParticleSystem::emitParticles(const ParticleSettings &particle, glm::vec2 pos, std::ranlux24_base &rng
	, glm::vec2 parentPos)
{

	auto rand = [&](glm::vec2 v)
	{
		if (v.x > v.y) { std::swap(v.x, v.y); }
		return getRandomFloat(rng, v.x, v.y);
	};

	for (int i = 0; i < particle.onCreateCount; i++)
	{

		if (particles.size() >= maxCount) { break; }

		ParticleInstance instance;

		glm::vec2 newPos = pos;
		newPos.x += rand(particle.positionX);
		newPos.y += rand(particle.positionY);

		instance.pos = newPos;
		instance.parentPos = parentPos;

		instance.durationTotal = rand(particle.particleLifeTime);
		instance.durationRemaining = instance.durationTotal;
		instance.rotation = rand(particle.rotation);
		instance.rotationSpeed = rand(particle.rotationSpeed);
		instance.rotationDrag = rand(particle.rotationDrag);

		instance.animationType = particle.animationType;
		instance.animationSpeed = rand(particle.animationSpeed);
		instance.animationAcceleration = rand(particle.animationAcceleration);
		instance.animationRotation = rand(particle.animationRotation);
		instance.animationTime = rand(particle.animationPhase);
		instance.animationScale = {rand(particle.animationScaleX), rand(particle.animationScaleY)};
		instance.animationOffset = {};

		instance.drag = {rand(particle.dragX), rand(particle.dragY)};

		instance.velocity = {rand(particle.velocityX), rand(particle.velocityY)};

		instance.tranzitionType = particle.tranzitionType;

		instance.colorStart.r = rand({particle.createApearence.color1.r, particle.createApearence.color2.r});
		instance.colorStart.g = rand({particle.createApearence.color1.g, particle.createApearence.color2.g});
		instance.colorStart.b = rand({particle.createApearence.color1.b, particle.createApearence.color2.b});
		instance.colorStart.a = rand({particle.createApearence.color1.a, particle.createApearence.color2.a});

		instance.colorEnd.r = rand({particle.endApearence.color1.r, particle.endApearence.color2.r});
		instance.colorEnd.g = rand({particle.endApearence.color1.g, particle.endApearence.color2.g});
		instance.colorEnd.b = rand({particle.endApearence.color1.b, particle.endApearence.color2.b});
		instance.colorEnd.a = rand({particle.endApearence.color1.a, particle.endApearence.color2.a});

		instance.sizeStart = rand(particle.createApearence.size);
		instance.sizeEnd = rand(particle.endApearence.size);

		instance.texture = particle.texture;

		instance.followParent = particle.folowParent;

		particles.push_back(instance);

	}

}


float interpolate(float a, float b, float perc)
{
	return a * (1-perc) + b * perc;
}

glm::vec4 interpolate(glm::vec4 a, glm::vec4 b, float perc)
{
	return a * (1-perc) + b * perc;
}

float calculateLifePercent(ParticleInstance &particleInstance)
{
	float lifePerc = particleInstance.durationRemaining / particleInstance.durationTotal; //close to 0 when gone, 1 when full

	switch (particleInstance.tranzitionType)
	{
		case ParticleSettings::TRANZITION_TYPES::none:
		lifePerc = 1;
		break;
		case ParticleSettings::TRANZITION_TYPES::linear:

		break;
		case ParticleSettings::TRANZITION_TYPES::curbe:
		lifePerc *= lifePerc;
		break;
		case ParticleSettings::TRANZITION_TYPES::abruptCurbe:
		lifePerc *= lifePerc * lifePerc;
		break;
		case ParticleSettings::TRANZITION_TYPES::wave:
		lifePerc = (std::cos(lifePerc * 5 * 3.141592) * lifePerc + lifePerc) / 2.f;
		break;
		case ParticleSettings::TRANZITION_TYPES::wave2:
		lifePerc = std::cos(lifePerc * 5 * 3.141592) * std::sqrt(lifePerc) * 0.9f + 0.1f;
		break;
		case ParticleSettings::TRANZITION_TYPES::delay:
		lifePerc = (std::cos(lifePerc * 3.141592 * 2) * std::sin(lifePerc * lifePerc)) / 2.f;
		break;
		case ParticleSettings::TRANZITION_TYPES::delay2:
		lifePerc = (std::atan(2 * lifePerc * lifePerc * lifePerc * 3.141592)) / 2.f;
		break;
		default:
		break;
	}

	return lifePerc;
}


float dampExp(float value, float dragPerSecond, float dt)
{
	// dragPerSecond = 0 => factor = 1
	// higher drag => stronger decay
	const float factor = std::exp(-dragPerSecond * dt);
	return value * factor;
}

glm::vec2 dampExp(glm::vec2 value, glm::vec2 dragPerSecond, float dt)
{
	return {
		value.x * std::exp(-dragPerSecond.x * dt),
		value.y * std::exp(-dragPerSecond.y * dt)
	};
}

float wrapDegrees(float deg)
{
	deg = std::fmod(deg, 360.0f);
	if (deg < 0.0f) deg += 360.0f;
	return deg;
}

constexpr float ANIMATION_PI = 3.14159265f;

float triangleWave(float t)
{
	float phase = std::fmod(t, 2.0f * ANIMATION_PI);
	if (phase < 0.0f) { phase += 2.0f * ANIMATION_PI; }
	float norm = phase / (2.0f * ANIMATION_PI);
	return 4.0f * std::abs(norm - 0.5f) - 1.0f;
}

glm::vec2 rotateVec(glm::vec2 v, float degrees)
{
	if (std::abs(degrees) <= 0.0001f) { return v; }
	float rad = degrees * (ANIMATION_PI / 180.0f);
	float cs = std::cos(rad);
	float sn = std::sin(rad);
	return {v.x * cs - v.y * sn, v.x * sn + v.y * cs};
}

glm::vec2 getAnimationOffset(const ParticleInstance &p)
{
	if (p.animationType == ParticleSettings::ANIMATION_TYPES::animationNone)
	{
		return {};
	}

	float t = p.animationTime;
	glm::vec2 scale = p.animationScale;
	glm::vec2 offset = {};

	// animation patterns are purely visual offsets layered on top of physics

	switch (p.animationType)
	{
		case ParticleSettings::ANIMATION_TYPES::animationCircle:
		{
			offset = {std::cos(t) * scale.x, std::sin(t) * scale.y};
		}
		break;
		case ParticleSettings::ANIMATION_TYPES::animationAtom:
		{
			float radius = 0.5f + 0.5f * std::sin(t * 2.0f);
			offset = {std::cos(t) * scale.x * radius, std::sin(t) * scale.y * radius};
		}
		break;
		case ParticleSettings::ANIMATION_TYPES::animationZigZag:
		{
			float tri = triangleWave(t);
			offset = {tri * scale.x, std::sin(t * 0.5f) * scale.y};
		}
		break;
		case ParticleSettings::ANIMATION_TYPES::animationSpiral:
		{
			float radius = std::fmod(std::abs(t), 2.0f * ANIMATION_PI) / (2.0f * ANIMATION_PI);
			offset = {std::cos(t) * scale.x * radius, std::sin(t) * scale.y * radius};
		}
		break;
		case ParticleSettings::ANIMATION_TYPES::animationWave:
		{
			offset = {std::sin(t) * scale.x, std::cos(t * 0.5f) * scale.y};
		}
		break;
		case ParticleSettings::ANIMATION_TYPES::animationFigure8:
		{
			offset = {std::sin(t) * scale.x, std::sin(t * 2.0f) * scale.y};
		}
		break;
		case ParticleSettings::ANIMATION_TYPES::animationBob:
		{
			offset = {0.0f, std::sin(t) * scale.y};
		}
		break;
		default:
			break;
	}

	return rotateVec(offset, p.animationRotation);
}

void ParticleSystem::update(float deltaTime)
{
	if (particles.size() >= maxCount)
	{
		particles.resize(maxCount);
	}

	
	for (int i = 0; i < particles.size(); i++)
	{
		auto &p = particles[i];

		p.durationRemaining -= deltaTime;

		if (p.durationRemaining <= 0.0f)
		{
			particles[i] = particles.back();
			particles.pop_back();
			--i;
			continue;
		}

		// update physics-driven motion (velocity + drag)

		p.velocity = dampExp(p.velocity, glm::max(p.drag, glm::vec2(0.0f)), deltaTime);

		p.pos += p.velocity * deltaTime;

		p.rotationSpeed = dampExp(p.rotationSpeed, std::max(p.rotationDrag, 0.0f), deltaTime);
		p.rotation += p.rotationSpeed * deltaTime;

		// optional: keep rotation in [0,360)
		p.rotation = wrapDegrees(p.rotation);

		// animation phase is applied on top of the physics position
		p.animationSpeed += p.animationAcceleration * deltaTime;
		p.animationTime += p.animationSpeed * deltaTime;
		p.animationOffset = getAnimationOffset(p);


	}


}

void ParticleSystem::render(gl2d::Renderer2D &renderer, 
	ParticlePostProcessRenderer &postProcessRenderer,
	glm::vec2 parentPos)
{
	if (particles.empty()) { return; }

#pragma region post process stuff
	const bool postProcessEffec = 1;
	//int pixelateFactor = 5;	

	int pixelateFactor = (PIXEL_SIZE * renderer.currentCamera.zoom);
	pixelateFactor = std::max(pixelateFactor, 2);

	glm::ivec2 oldSize = {renderer.windowW, renderer.windowH};

	int fbW = 1, fbH = 1;
	float scaleX = 1.0f, scaleY = 1.0f;
	fbW = std::max(1, (oldSize.x + pixelateFactor - 1) / pixelateFactor); // ceil div
	fbH = std::max(1, (oldSize.y + pixelateFactor - 1) / pixelateFactor); // ceil div
	scaleX = (float)oldSize.x / (float)fbW;
	scaleY = (float)oldSize.y / (float)fbH;

	if (postProcessEffec)
	{
		postProcessRenderer.fbo.bind();
		renderer.updateWindowMetrics(postProcessRenderer.fbo.w, postProcessRenderer.fbo.h);

	};
	auto cam = renderer.currentCamera;
#pragma endregion


	for (auto &p : particles)
	{

		//[0, 1] used to interpolate
		float perc = calculateLifePercent(p);

		glm::vec4 finalColor = interpolate(p.colorEnd, p.colorStart, perc);
		float finalSize = interpolate(p.sizeEnd, p.sizeStart, perc);

		// animation offset is visual-only and layered on top of physics
		glm::vec2 renderPos = p.pos + p.animationOffset;
		glm::vec4 aabb = {renderPos - glm::vec2(finalSize / 2.f), finalSize, finalSize};

		if (p.followParent)
		{
			aabb += glm::vec4(-p.parentPos + parentPos, glm::vec2(0,0));
		}

		if (postProcessEffec)
		{
			renderer.currentCamera = cam;

			// --- FIX (divide by real scale, not pixelateFactor) ---
			aabb.x /= scaleX; aabb.y /= scaleY;
			aabb.z /= scaleX; aabb.w /= scaleY;

			aabb.x -= renderer.currentCamera.position.x / scaleX;
			aabb.y -= renderer.currentCamera.position.y / scaleY;
			// ------------------------------------------------------

			renderer.currentCamera.position = {};
		}

		if (p.texture.isValid())
		{
			renderer.renderRectangle(aabb, p.texture, finalColor, {}, p.rotation);
		}
		else
		{
			renderer.renderRectangle(aabb, finalColor, {}, p.rotation);
		}

	}

	if (postProcessEffec)
	{
		renderer.currentCamera = cam;
		renderer.updateWindowMetrics(oldSize.x, oldSize.y);
		postProcessRenderer.fbo.unbind();
	};


}

void ParticlePostProcessRenderer::init()
{

	fbo.create(1, 1, true);

}

void ParticlePostProcessRenderer::updateWindowMetrics(gl2d::Renderer2D &renderer)
{
	//int pixelateFactor = 5;	

	int pixelateFactor = (PIXEL_SIZE * renderer.currentCamera.zoom);
	pixelateFactor = std::max(pixelateFactor, 2);

	glm::ivec2 oldSize = {renderer.windowW, renderer.windowH};

	// --- FIX (use actual FBO size + real scale) ---
	int fbW = 1, fbH = 1;
	float scaleX = 1.0f, scaleY = 1.0f;
	// ---------------------------------------------

	// --- FIX ---
	fbW = std::max(1, (oldSize.x + pixelateFactor - 1) / pixelateFactor); // ceil div
	fbH = std::max(1, (oldSize.y + pixelateFactor - 1) / pixelateFactor); // ceil div
	scaleX = (float)oldSize.x / (float)fbW;
	scaleY = (float)oldSize.y / (float)fbH;
	// -----------

	fbo.resize(fbW, fbH);
	fbo.clear();


}

void ParticlePostProcessRenderer::finalRender(gl2d::Renderer2D &renderer)
{

	//SDL_SetTextureAlphaMod(fbo.texture.tex, 255);
	//SDL_SetTextureBlendMode(fbo.texture.tex, SDL_BLENDMODE_ADD);

	renderer.pushCamera();
	renderer.renderRectangle({0,0, renderer.windowW, renderer.windowH}, fbo.texture, {1,1,1,2}, {}, {}, {0,0,1,1});
	renderer.popCamera();

}

void ParticleSystem::killParticlesColliding(Map &map, glm::vec2 parentPos)
{
	for (int i = 0; i < particles.size(); i++)
	{
		auto &p = particles[i];
		glm::vec2 worldPos = p.pos;
		if (p.followParent)
		{
			worldPos += -p.parentPos + parentPos;
		}

		int tx = (int)std::floor(worldPos.x);
		int ty = (int)std::floor(worldPos.y);

		if (tx < 0 || ty < 0 || tx >= map.size.x || ty >= map.size.y ||
			map.isCollidableAtPosSafe(tx, ty))
		{
			particles[i] = particles.back();
			particles.pop_back();
			--i;
		}
	}
}
