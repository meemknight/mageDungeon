// Fragment shader for additive cosmetic dynamic light falloff.
#version 450

layout(location = 0) in vec2 inWorldPos;
layout(location = 1) in vec2 inLightCenter;
layout(location = 2) in float inLightRadius;
layout(location = 3) in float inFalloffPower;
layout(location = 4) in vec3 inLightColor;

layout(location = 0) out vec4 outColor;

void main()
{
	float radius = max(inLightRadius, 0.0001);
	float distanceToCenter = length(inWorldPos - inLightCenter);
	float normalized = clamp(1.0 - distanceToCenter / radius, 0.0, 1.0);
	if (normalized <= 0.0)
	{
		discard;
	}

	float attenuation = pow(normalized, max(inFalloffPower, 0.05));
	outColor = vec4(inLightColor * attenuation, 1.0);
}
