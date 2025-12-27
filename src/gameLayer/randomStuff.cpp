#include "randomStuff.h"



// Returns a float in [min, max)
float getRandomFloat(std::ranlux24_base &rng, float min, float max)
{
	std::uniform_real_distribution<float> dist(min, max);
	return dist(rng);
}

// Returns an int in [min, max]
int getRandomInt(std::ranlux24_base &rng, int min, int max)
{
	std::uniform_int_distribution<int> dist(min, max);
	return dist(rng);
}

bool getRandomChance(std::ranlux24_base &rng, float chance)
{
	float dice = getRandomFloat(rng, 0.0, 1.0);
	return dice <= chance;
}

