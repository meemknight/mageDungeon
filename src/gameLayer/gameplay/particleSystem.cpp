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

		//update

		p.velocity = dampExp(p.velocity, glm::max(p.drag, glm::vec2(0.0f)), deltaTime);

		p.pos += p.velocity * deltaTime;

		p.rotationSpeed = dampExp(p.rotationSpeed, std::max(p.rotationDrag, 0.0f), deltaTime);
		p.rotation += p.rotationSpeed * deltaTime;

		// optional: keep rotation in [0,360)
		p.rotation = wrapDegrees(p.rotation);


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

		glm::vec4 aabb = {p.pos - glm::vec2(finalSize / 2.f) - p.parentPos + parentPos, finalSize, finalSize};

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
