// Separable bloom pass. Horizontal/vertical behavior is selected by uniform.
#version 450

layout(set = 2, binding = 0) uniform sampler2D uBloomInputTexture;
layout(std140, set = 3, binding = 0) uniform BloomPassUniform
{
	// x: pass mode (0 = horizontal extract, 1 = vertical blur)
	vec4 uPassData;
};

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

// Artistic tweak constants.
const int BLOOM_RADIUS = 4;
const float BLOOM_THRESHOLD = 0.28;
const float BLOOM_SOFT_KNEE = 0.08;
const float BLOOM_INTENSITY = 1.20;
const float BLOOM_SATURATION = 1.12;

const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);

float brightnessMask(vec3 color)
{
	float luminance = dot(color, LUMA);
	return smoothstep(BLOOM_THRESHOLD - BLOOM_SOFT_KNEE,
		BLOOM_THRESHOLD + BLOOM_SOFT_KNEE,
		luminance);
}

vec3 saturateColor(vec3 color, float saturation)
{
	float gray = dot(color, LUMA);
	return mix(vec3(gray), color, saturation);
}

void main()
{
	bool verticalPass = uPassData.x > 0.5;

	vec2 textureSizePixels = vec2(textureSize(uBloomInputTexture, 0));
	vec2 texel = 1.0 / max(textureSizePixels, vec2(1.0));

	vec2 clampedUV = clamp(inUV, vec2(0.0), vec2(1.0));
	if (!verticalPass)
	{
		// Source particle target is Y-flipped in horizontal extract pass.
		clampedUV.y = 1.0 - clampedUV.y;
	}

	vec2 snappedUV = (floor(clampedUV * textureSizePixels) + 0.5) * texel;
	vec2 axis = verticalPass ? vec2(0.0, 1.0) : vec2(1.0, 0.0);

	vec3 bloomAccum = vec3(0.0);
	float bloomWeight = 0.0;

	for (int i = -BLOOM_RADIUS; i <= BLOOM_RADIUS; i++)
	{
		float distance = abs(float(i));
		float weight = 1.0 - distance / (float(BLOOM_RADIUS) + 1.0);
		if (weight <= 0.0) { continue; }

		vec4 sampleValue = texture(uBloomInputTexture, snappedUV + axis * float(i) * texel);

		if (verticalPass)
		{
			bloomAccum += sampleValue.rgb * weight;
		}
		else
		{
			float mask = brightnessMask(sampleValue.rgb) * sampleValue.a;
			bloomAccum += sampleValue.rgb * mask * weight;
		}

		bloomWeight += weight;
	}

	vec3 bloomColor = vec3(0.0);
	if (bloomWeight > 0.0)
	{
		bloomColor = bloomAccum / bloomWeight;
	}

	if (verticalPass)
	{
		bloomColor = saturateColor(bloomColor, BLOOM_SATURATION) * BLOOM_INTENSITY;
	}

	// Bloom-only overlay, composited additively over the screen with fixed alpha.
	outColor = vec4(bloomColor, 1.0);
}
