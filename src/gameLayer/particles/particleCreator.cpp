#include "particleCreator.h"
#include <gameplay/Physics.h>
#include <gameplay/elements.h>


glm::vec3 RGBtoHSV(glm::vec3 c)
{
	c = glm::clamp(c, glm::vec3(0), glm::vec3(1));
	glm::vec4 K = glm::vec4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
	glm::vec4 p = glm::mix(glm::vec4(c.b, c.g, K.w, K.z),
		glm::vec4(c.g, c.b, K.x, K.y),
		glm::step(c.b, c.g));
	glm::vec4 q = glm::mix(glm::vec4(p.x, p.y, p.w, c.r),
		glm::vec4(c.r, p.y, p.z, p.x),
		glm::step(p.x, c.r));

	float d = q.x - glm::min(q.w, q.y);
	float e = 1e-10f;

	return glm::vec3(
		glm::abs(q.z + (q.w - q.y) / (6.0f * d + e)), // H [0..1]
		d / (q.x + e),                               // S
		q.x                                          // V
	);
}

glm::vec3 HSVtoRGB(glm::vec3 c)
{
	//c = glm::clamp(c, glm::vec3(0), glm::vec3(1));
	glm::vec3 p = glm::abs(glm::fract(c.x + glm::vec3(0.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f);
	return c.z * glm::mix(glm::vec3(1.0f), glm::clamp(p - 1.0f, 0.0f, 1.0f), c.y);
}

glm::vec4 RGBtoHSV(glm::vec4 c)
{
	return glm::vec4(RGBtoHSV(glm::vec3(c)), c.a);
}

glm::vec4 HSVtoRGB(glm::vec4 c)
{
	return glm::vec4(HSVtoRGB(glm::vec3(c)), c.a);
}

// b in [-1, 1] recommended (but will work for any value; it clamps).
// b > 0  : increase brightness, slightly increase saturation, hue shifts one way
// b < 0  : decrease brightness, slightly decrease saturation, hue shifts the other way
glm::vec3 changeColorBrightness(glm::vec3 colorIn, float b)
{
	colorIn = glm::clamp(colorIn, 0.0f, 1.0f);

	glm::vec3 hsv = RGBtoHSV(colorIn);

	float t = glm::clamp(glm::abs(b), 0.0f, 1.0f);
	float sgn = (b >= 0.0f) ? 1.0f : -1.0f;

	// Brightness: push V up/down. This is linear; swap to a curve if you want.
	hsv.z = glm::clamp(hsv.z + b, 0.0f, 1.0f);

	// Saturation: slight boost when brightening, slight reduction when darkening.
	// Keep it subtle so it doesn't look like a filter.
	const float satGain = 0.22f; // tweak to taste
	hsv.y = glm::clamp(hsv.y * (1.0f + sgn * satGain * t), 0.0f, 1.0f);

	// Hue shift: small shift that scales with |b|, opposite direction when darkening.
	// Hue is [0,1], so wrap with fract.
	const float hueShift = 0.02f; // ~= 7.2 degrees
	hsv.x = glm::fract(hsv.x + sgn * hueShift * t);

	return HSVtoRGB(hsv);
}

glm::vec4 changeColorBrightness(glm::vec4 colorIn, float b)
{
	glm::vec3 rgb = changeColorBrightness(glm::vec3(colorIn), b);
	return glm::vec4(rgb, colorIn.a);
}

ParticleSettings getBasicMagicMissleParticle(glm::vec4 startColor, glm::vec4 endColor)
{

	ParticleSettings fireParticle;

	fireParticle.onCreateCount = 1;
	fireParticle.particleLifeTime = {0.3, 0.6};
	fireParticle.velocityX = glm::vec2{-8,8} *PIXEL_SIZE;
	fireParticle.velocityY = glm::vec2{-8,-12} *PIXEL_SIZE;
	fireParticle.createApearence.size = glm::vec2{4, 7} *PIXEL_SIZE;
	fireParticle.endApearence.size = glm::vec2{5, 7} *PIXEL_SIZE;

	fireParticle.dragX = glm::vec2{-5,5} *PIXEL_SIZE;
	fireParticle.dragY = glm::vec2{-50,-80} *PIXEL_SIZE;
	fireParticle.rotation = {0, 360};
	fireParticle.rotationSpeed = {0, 10};
	fireParticle.rotationDrag = {0, 100};
	fireParticle.createApearence.color1 = changeColorBrightness(startColor, 0.1);
	fireParticle.createApearence.color2 = changeColorBrightness(startColor, -0.1);
	fireParticle.endApearence.color1 = changeColorBrightness(endColor, 0.1);
	fireParticle.endApearence.color2 = changeColorBrightness(endColor, -0.1);

	fireParticle.tranzitionType = ParticleSettings::TRANZITION_TYPES::abruptCurbe;
	fireParticle.positionX = glm::vec2{-2,2} *PIXEL_SIZE;
	fireParticle.positionY = glm::vec2{-2,2} *PIXEL_SIZE;

	return fireParticle;
}


// ------------------------------------------------------------
// 1) Arcane trail (good for magic missile / wand shots)
// ------------------------------------------------------------
ParticleSettings getArcaneTrailParticle(glm::vec4 startColor, glm::vec4 endColor)
{
	ParticleSettings p;

	p.onCreateCount = 2;
	p.particleLifeTime = {0.18f, 0.35f};

	p.velocityX = glm::vec2{-10, 10} *PIXEL_SIZE;
	p.velocityY = glm::vec2{-10, 10} *PIXEL_SIZE;

	p.createApearence.size = glm::vec2{4, 6} *PIXEL_SIZE;
	p.endApearence.size = glm::vec2{1, 2} *PIXEL_SIZE;

	p.dragX = glm::vec2{-35, -55} *PIXEL_SIZE; // pulls inward / damps quickly
	p.dragY = glm::vec2{-35, -55} *PIXEL_SIZE;

	p.rotation = {0, 360};
	p.rotationSpeed = {40, 160};
	p.rotationDrag = {50, 120};

	p.createApearence.color1 = changeColorBrightness(startColor, 0.15f);
	p.createApearence.color2 = changeColorBrightness(startColor, -0.10f);
	p.endApearence.color1 = changeColorBrightness(endColor, 0.10f);
	p.endApearence.color2 = changeColorBrightness(endColor, -0.15f);

	p.tranzitionType = ParticleSettings::TRANZITION_TYPES::curbe;

	p.positionX = glm::vec2{-2, 2} *PIXEL_SIZE;
	p.positionY = glm::vec2{-2, 2} *PIXEL_SIZE;

	return p;
}

// ------------------------------------------------------------
// 2) Arcane sparks (impact / hit flash)
// ------------------------------------------------------------
ParticleSettings getArcaneSparkBurstParticle(glm::vec4 startColor, glm::vec4 endColor)
{
	ParticleSettings p;

	p.onCreateCount = 6;
	p.particleLifeTime = {0.12f, 0.22f};

	p.velocityX = glm::vec2{-50, 50} *PIXEL_SIZE;
	p.velocityY = glm::vec2{-50, 50} *PIXEL_SIZE;

	p.createApearence.size = glm::vec2{2, 3} *PIXEL_SIZE;
	p.endApearence.size = glm::vec2{0.5f, 1.5f} *PIXEL_SIZE;

	p.dragX = glm::vec2{-120, -200} *PIXEL_SIZE;
	p.dragY = glm::vec2{-120, -200} *PIXEL_SIZE;

	p.rotation = {0, 360};
	p.rotationSpeed = {120, 380};
	p.rotationDrag = {200, 500};

	p.createApearence.color1 = changeColorBrightness(startColor, 0.30f);
	p.createApearence.color2 = changeColorBrightness(startColor, 0.05f);
	p.endApearence.color1 = changeColorBrightness(endColor, -0.05f);
	p.endApearence.color2 = changeColorBrightness(endColor, -0.25f);

	p.tranzitionType = ParticleSettings::TRANZITION_TYPES::abruptCurbe;

	p.positionX = glm::vec2{-1, 1} *PIXEL_SIZE;
	p.positionY = glm::vec2{-1, 1} *PIXEL_SIZE;

	return p;
}

// ------------------------------------------------------------
// 3) Healing motes (gentle upward float)
// ------------------------------------------------------------
ParticleSettings getHealingMoteParticle(glm::vec4 startColor, glm::vec4 endColor)
{
	ParticleSettings p;

	p.onCreateCount = 1;
	p.particleLifeTime = {0.55f, 0.95f};

	p.velocityX = glm::vec2{-6, 6} *PIXEL_SIZE;
	p.velocityY = glm::vec2{-18, -28} *PIXEL_SIZE;

	p.createApearence.size = glm::vec2{3, 4} *PIXEL_SIZE;
	p.endApearence.size = glm::vec2{2, 3} *PIXEL_SIZE;

	p.dragX = glm::vec2{-8, -14} *PIXEL_SIZE;
	p.dragY = glm::vec2{-18, -28} *PIXEL_SIZE;

	p.rotation = {0, 360};
	p.rotationSpeed = {10, 40};
	p.rotationDrag = {10, 40};

	p.createApearence.color1 = changeColorBrightness(startColor, 0.10f);
	p.createApearence.color2 = changeColorBrightness(startColor, -0.05f);
	p.endApearence.color1 = changeColorBrightness(endColor, 0.10f);
	p.endApearence.color2 = changeColorBrightness(endColor, -0.10f);

	p.tranzitionType = ParticleSettings::TRANZITION_TYPES::wave;

	p.positionX = glm::vec2{-5, 5} *PIXEL_SIZE;
	p.positionY = glm::vec2{-2, 2} *PIXEL_SIZE;

	return p;
}

// ------------------------------------------------------------
// 4) Frost shards (fast, sharp, slightly downward / outward)
// ------------------------------------------------------------
ParticleSettings getFrostShardParticle(glm::vec4 startColor, glm::vec4 endColor)
{
	ParticleSettings p;

	p.onCreateCount = 2;
	p.particleLifeTime = {0.20f, 0.40f};

	p.velocityX = glm::vec2{-35, 35} *PIXEL_SIZE;
	p.velocityY = glm::vec2{-10, 15} *PIXEL_SIZE;

	p.createApearence.size = glm::vec2{2, 5} *PIXEL_SIZE;
	p.endApearence.size = glm::vec2{1, 3} *PIXEL_SIZE;

	p.dragX = glm::vec2{-55, -90} *PIXEL_SIZE;
	p.dragY = glm::vec2{-30, -60} *PIXEL_SIZE;

	p.rotation = {0, 360};
	p.rotationSpeed = {60, 220};
	p.rotationDrag = {80, 220};

	p.createApearence.color1 = changeColorBrightness(startColor, 0.05f);
	p.createApearence.color2 = changeColorBrightness(startColor, -0.15f);
	p.endApearence.color1 = changeColorBrightness(endColor, 0.00f);
	p.endApearence.color2 = changeColorBrightness(endColor, -0.20f);

	p.tranzitionType = ParticleSettings::TRANZITION_TYPES::linear;

	p.positionX = glm::vec2{-2, 2} *PIXEL_SIZE;
	p.positionY = glm::vec2{-2, 2} *PIXEL_SIZE;

	return p;
}

// ------------------------------------------------------------
// 5) Poison mist (slow drifting cloud)
// ------------------------------------------------------------
ParticleSettings getPoisonMistParticle(glm::vec4 startColor, glm::vec4 endColor)
{
	ParticleSettings p;

	p.onCreateCount = 2;
	p.particleLifeTime = {0.70f, 1.20f};

	p.velocityX = glm::vec2{-10, 10} *PIXEL_SIZE;
	p.velocityY = glm::vec2{-4,  -10} *PIXEL_SIZE; // mostly rises slowly

	p.createApearence.size = glm::vec2{6, 10} *PIXEL_SIZE;
	p.endApearence.size = glm::vec2{10, 16} *PIXEL_SIZE;

	p.dragX = glm::vec2{-6, -12} *PIXEL_SIZE;
	p.dragY = glm::vec2{-6, -12} *PIXEL_SIZE;

	p.rotation = {0, 360};
	p.rotationSpeed = {0, 30};
	p.rotationDrag = {0, 40};

	p.createApearence.color1 = changeColorBrightness(startColor, -0.05f);
	p.createApearence.color2 = changeColorBrightness(startColor, -0.20f);
	p.endApearence.color1 = changeColorBrightness(endColor, -0.10f);
	p.endApearence.color2 = changeColorBrightness(endColor, -0.25f);

	p.tranzitionType = ParticleSettings::TRANZITION_TYPES::delay2;

	p.positionX = glm::vec2{-6, 6} *PIXEL_SIZE;
	p.positionY = glm::vec2{-4, 4} *PIXEL_SIZE;

	return p;
}

// ------------------------------------------------------------
// 6) Lightning zap (snappy streak, short lifetime)
// ------------------------------------------------------------
ParticleSettings getLightningZapParticle(glm::vec4 startColor, glm::vec4 endColor)
{
	ParticleSettings p;

	p.onCreateCount = 3;
	p.particleLifeTime = {0.06f, 0.12f};

	p.velocityX = glm::vec2{-90, 90} *PIXEL_SIZE;
	p.velocityY = glm::vec2{-30, 30} *PIXEL_SIZE;

	p.createApearence.size = glm::vec2{1, 6} *PIXEL_SIZE;
	p.endApearence.size = glm::vec2{0.5f, 2.0f} *PIXEL_SIZE;

	p.dragX = glm::vec2{-300, -420} *PIXEL_SIZE;
	p.dragY = glm::vec2{-200, -320} *PIXEL_SIZE;

	p.rotation = {0, 360};
	p.rotationSpeed = {300, 720};
	p.rotationDrag = {600, 1000};

	p.createApearence.color1 = changeColorBrightness(startColor, 0.35f);
	p.createApearence.color2 = changeColorBrightness(startColor, 0.15f);
	p.endApearence.color1 = changeColorBrightness(endColor, 0.10f);
	p.endApearence.color2 = changeColorBrightness(endColor, -0.10f);

	p.tranzitionType = ParticleSettings::TRANZITION_TYPES::abruptCurbe;

	p.positionX = glm::vec2{-1, 1} *PIXEL_SIZE;
	p.positionY = glm::vec2{-1, 1} *PIXEL_SIZE;

	return p;
}

// ------------------------------------------------------------
// 7) Dark curse embers (sinky + wavy, “evil”)
// ------------------------------------------------------------
ParticleSettings getDarkCurseEmberParticle(glm::vec4 startColor, glm::vec4 endColor)
{
	ParticleSettings p;

	p.onCreateCount = 2;
	p.particleLifeTime = {0.35f, 0.70f};

	p.velocityX = glm::vec2{-10, 10} *PIXEL_SIZE;
	p.velocityY = glm::vec2{10,  26} *PIXEL_SIZE; // sinks

	p.createApearence.size = glm::vec2{3, 5} *PIXEL_SIZE;
	p.endApearence.size = glm::vec2{1, 2} *PIXEL_SIZE;

	p.dragX = glm::vec2{-10, -18} *PIXEL_SIZE;
	p.dragY = glm::vec2{-20, -35} *PIXEL_SIZE;

	p.rotation = {0, 360};
	p.rotationSpeed = {20, 90};
	p.rotationDrag = {30, 120};

	p.createApearence.color1 = changeColorBrightness(startColor, -0.10f);
	p.createApearence.color2 = changeColorBrightness(startColor, -0.25f);
	p.endApearence.color1 = changeColorBrightness(endColor, -0.10f);
	p.endApearence.color2 = changeColorBrightness(endColor, -0.30f);

	p.tranzitionType = ParticleSettings::TRANZITION_TYPES::wave2;

	p.positionX = glm::vec2{-4, 4} *PIXEL_SIZE;
	p.positionY = glm::vec2{-4, 4} *PIXEL_SIZE;

	return p;
}

// ------------------------------------------------------------
// 8) Teleport puff (expanding swirl, nice for blink/warp)
// ------------------------------------------------------------
ParticleSettings getTeleportPuffParticle(glm::vec4 startColor, glm::vec4 endColor)
{
	ParticleSettings p;

	p.onCreateCount = 3;
	p.particleLifeTime = {0.25f, 0.45f};

	p.velocityX = glm::vec2{-18, 18} *PIXEL_SIZE;
	p.velocityY = glm::vec2{-18, 18} *PIXEL_SIZE;

	p.createApearence.size = glm::vec2{4, 6} *PIXEL_SIZE;
	p.endApearence.size = glm::vec2{10, 14} *PIXEL_SIZE;

	p.dragX = glm::vec2{-25, -45} *PIXEL_SIZE;
	p.dragY = glm::vec2{-25, -45} *PIXEL_SIZE;

	p.rotation = {0, 360};
	p.rotationSpeed = {140, 420};
	p.rotationDrag = {200, 500};

	p.createApearence.color1 = changeColorBrightness(startColor, 0.12f);
	p.createApearence.color2 = changeColorBrightness(startColor, -0.08f);
	p.endApearence.color1 = changeColorBrightness(endColor, 0.05f);
	p.endApearence.color2 = changeColorBrightness(endColor, -0.20f);

	p.tranzitionType = ParticleSettings::TRANZITION_TYPES::delay;

	p.positionX = glm::vec2{-3, 3} *PIXEL_SIZE;
	p.positionY = glm::vec2{-3, 3} *PIXEL_SIZE;

	return p;
}

ParticleEmissionSettings getBasicMagicMissleParticleEmision(int element)
{

	ParticleEmissionSettings ret;

	glm::vec4 color1 = elementToColor(element); color1.a = 0.3;
	glm::vec4 color2 = RGBtoHSV(color1); color2.a = 0.3;

	color2.x += 0.15;
	color2.y -= 0.1;
	color2.z += 0.25;

	color2 = HSVtoRGB(color2);

	auto x = color1;
	color1 = color2;
	color2 = x;


	//glm::vec4 color1Create = RGBtoHSV(color1);
	//glm::vec4 color2Create = RGBtoHSV(color2);
	//color1.y -= 0.2;
	//color2.y -= 0.2;
	//color1.z += 0.1;
	//color2.z += 0.1;
	//color1Create = HSVtoRGB(color1Create);
	//color2Create = HSVtoRGB(color2Create);
	//
	//
	//glm::vec4 color1Release = RGBtoHSV(color1);
	//glm::vec4 color2Release = RGBtoHSV(color2);
	//color1.y -= 0.2;
	//color2.y -= 0.2;
	//color1.x -= 0.1;
	//color2.x -= 0.1;
	//color1.z -= 0.1;
	//color2.z -= 0.1;
	//color1Release = HSVtoRGB(color1Release);
	//color2Release = HSVtoRGB(color2Release);


	//fire
	ret.sustain = getBasicMagicMissleParticle(color1, color2);

	//ret.create = getBasicMagicMissleParticle(color1Create, color2Create);
	//ret.create.folowParent = false;
	//
	//ret.release = getBasicMagicMissleParticle(color1Release, color2Release);
	//ret.release.folowParent = false;


	return ret;
}
