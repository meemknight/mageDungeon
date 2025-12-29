#pragma once

#include <gameplay/particleSystem.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>    


glm::vec3 RGBtoHSV(glm::vec3 c);

glm::vec3 HSVtoRGB(glm::vec3 c);

glm::vec4 RGBtoHSV(glm::vec4 c);

glm::vec4 HSVtoRGB(glm::vec4 c);


// b in [-1, 1] recommended (but will work for any value; it clamps).
// b > 0  : increase brightness, slightly increase saturation, hue shifts one way
// b < 0  : decrease brightness, slightly decrease saturation, hue shifts the other way
glm::vec3 changeColorBrightness(glm::vec3 colorIn, float b);

glm::vec4 changeColorBrightness(glm::vec4 colorIn, float b);

ParticleSettings getBasicMagicMissleParticle(glm::vec4 startColor, glm::vec4 endColor);

ParticleSettings getArcaneTrailParticle(glm::vec4 startColor, glm::vec4 endColor);
ParticleSettings getSparkBurstParticle(glm::vec4 startColor, glm::vec4 endColor);
ParticleSettings getHealingMoteParticle(glm::vec4 startColor, glm::vec4 endColor);
ParticleSettings getFrostShardParticle(glm::vec4 startColor, glm::vec4 endColor);
ParticleSettings getPoisonMistParticle(glm::vec4 startColor, glm::vec4 endColor);
ParticleSettings getLightningZapParticle(glm::vec4 startColor, glm::vec4 endColor);
ParticleSettings getDarkCurseEmberParticle(glm::vec4 startColor, glm::vec4 endColor);
ParticleSettings getTeleportPuffParticle(glm::vec4 startColor, glm::vec4 endColor);



ParticleEmissionSettings getBasicMagicMissleParticleEmision(int element);
