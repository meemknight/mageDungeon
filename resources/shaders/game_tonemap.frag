// Fullscreen tone mapping pass for HDR gameplay framebuffer.
#version 450

layout(set = 2, binding = 0) uniform sampler2D uHdrTexture;
layout(set = 2, binding = 1) uniform sampler2D uLightMaskTexture;
layout(std140, set = 3, binding = 0) uniform ToneMapUniform
{
	// x: tonemapper index, y: exposure, z: saturation, w: vibrance
	vec4 uToneMapData;
	// x: gamma, y: shadowBoost, z: highlightBoost, w: vignette
	vec4 uGradingData;
	// xyz: lift
	vec4 uLift;
	// xyz: gain
	vec4 uGain;
	// x: hasCosmeticLightMask
	vec4 uExtraData;
};

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

#define AGX_LOOK 2

int getTonemapper()
{
	return int(clamp(uToneMapData.x, 0.0, 4.0) + 0.5);
}

float getExposure()
{
	return max(uToneMapData.y, 0.0);
}

float getSaturation()
{
	return uToneMapData.z;
}

float getVibrance()
{
	return uToneMapData.w;
}

float getGradingGamma()
{
	return max(uGradingData.x, 0.001);
}

float getShadowBoost()
{
	return uGradingData.y;
}

float getHighlightBoost()
{
	return uGradingData.z;
}

float getVignette()
{
	return clamp(uGradingData.w, 0.0, 1.0);
}

float hasCosmeticLightMask()
{
	return uExtraData.x;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ACES
/////////////////////////////////////////////////////////////////////////////////////////

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3(
	0.59719, 0.35458, 0.04823,
	0.07600, 0.90834, 0.01566,
	0.02840, 0.13383, 0.83777);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
	1.60475, -0.53108, -0.07367,
	-0.10208, 1.10813, -0.00605,
	-0.00327, -0.07276, 1.07602);

vec3 RRTAndODTFit(vec3 v)
{
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return a / b;
}

vec3 ACESFitted(vec3 color)
{
	color = transpose(ACESInputMat) * color;
	color = RRTAndODTFit(color);
	color = transpose(ACESOutputMat) * color;
	return clamp(color, 0.0, 1.0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// AGX
/////////////////////////////////////////////////////////////////////////////////////////

float toLinearAXG(float sRGB)
{
	bool cutoff = (sRGB < 0.04045);
	float higher = pow((sRGB + 0.055) / 1.055, 2.4);
	float lower = sRGB / 12.92;
	return mix(higher, lower, cutoff);
}

vec3 AGX(vec3 col)
{
	col = mat3(0.842, 0.0423, 0.0424,
		0.0784, 0.878, 0.0784,
		0.0792, 0.0792, 0.879) * col;

	col = clamp((log2(col) + 12.47393) / 16.5, vec3(0.0), vec3(1.0));
	col = 0.5 + 0.5 * sin(((-3.11 * col + 6.42) * col - 0.378) * col - 1.44);

	#if AGX_LOOK == 1
	col = mix(vec3(dot(col, vec3(0.216, 0.7152, 0.0722))), col * vec3(1.0, 0.9, 0.5), 0.8);
	#elif AGX_LOOK == 2
	col = mix(vec3(dot(col, vec3(0.216, 0.7152, 0.0722))), pow(col, vec3(1.35)), 1.4);
	#endif

	return col;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ZCAM
/////////////////////////////////////////////////////////////////////////////////////////

const float Zcam_Lp = 10000.0;
const float Zcam_m1 = 2610.0 / 16384.0;
const float Zcam_m2 = 1.7 * 2523.0 / 32.0;
const float Zcam_c1 = 107.0 / 128.0;
const float Zcam_c2 = 2413.0 / 128.0;
const float Zcam_c3 = 2392.0 / 128.0;

vec3 eotf_pq(vec3 x)
{
	x = sign(x) * pow(abs(x), vec3(1.0 / Zcam_m2));
	x = sign(x) * pow((abs(x) - Zcam_c1) / (Zcam_c2 - Zcam_c3 * abs(x)), vec3(1.0 / Zcam_m1)) * Zcam_Lp;
	return x;
}

vec3 eotf_pq_inverse(vec3 x)
{
	x /= Zcam_Lp;
	x = sign(x) * pow(abs(x), vec3(Zcam_m1));
	x = sign(x) * pow((Zcam_c1 + Zcam_c2 * abs(x)) / (1.0 + Zcam_c3 * abs(x)), vec3(Zcam_m2));
	return x;
}

const float Zcam_W = 140.0;
const float Zcam_b = 1.15;
const float Zcam_g = 0.66;

vec3 XYZ_to_ICh(vec3 XYZ)
{
	XYZ *= Zcam_W;
	XYZ.xy = vec2(Zcam_b, Zcam_g) * XYZ.xy - (vec2(Zcam_b, Zcam_g) - 1.0) * XYZ.zx;

	const mat3 XYZ_to_LMS = transpose(mat3(
		0.41479, 0.579999, 0.014648,
		-0.20151, 1.12065, 0.0531008,
		-0.0166008, 0.2648, 0.66848));

	vec3 LMS = XYZ_to_LMS * XYZ;
	LMS = eotf_pq_inverse(LMS);

	const mat3 LMS_to_Iab = transpose(mat3(
		0.0, 1.0, 0.0,
		3.524, -4.06671, 0.542708,
		0.199076, 1.0968, -1.29588));

	vec3 Iab = LMS_to_Iab * LMS;

	float I = eotf_pq(vec3(Iab.x)).x / Zcam_W;
	float C = length(Iab.yz);
	float h = atan(Iab.z, Iab.y);
	return vec3(I, C, h);
}

vec3 ICh_to_XYZ(vec3 ICh)
{
	vec3 Iab;
	Iab.x = eotf_pq_inverse(vec3(ICh.x * Zcam_W)).x;
	Iab.y = ICh.y * cos(ICh.z);
	Iab.z = ICh.y * sin(ICh.z);

	const mat3 Iab_to_LMS = transpose(mat3(
		1.0, 0.2772, 0.1161,
		1.0, 0.0, 0.0,
		1.0, 0.0426, -0.7538));

	vec3 LMS = Iab_to_LMS * Iab;
	LMS = eotf_pq(LMS);

	const mat3 LMS_to_XYZ = transpose(mat3(
		1.92423, -1.00479, 0.03765,
		0.35032, 0.72648, -0.06538,
		-0.09098, -0.31273, 1.52277));

	vec3 XYZ = LMS_to_XYZ * LMS;
	XYZ.x = (XYZ.x + (Zcam_b - 1.0) * XYZ.z) / Zcam_b;
	XYZ.y = (XYZ.y + (Zcam_g - 1.0) * XYZ.x) / Zcam_g;
	return XYZ / Zcam_W;
}

const mat3 XYZ_to_sRGB = mat3(
	3.2404542, -0.9692660, 0.0556434,
	-1.5371385, 1.8760108, -0.2040259,
	-0.4985314, 0.0415560, 1.0572252);

const mat3 sRGB_to_XYZ = mat3(
	0.4124564, 0.2126729, 0.0193339,
	0.3575761, 0.7151522, 0.1191920,
	0.1804375, 0.0721750, 0.9503041);

bool in_sRGB_gamut(vec3 ICh)
{
	vec3 sRGB = XYZ_to_sRGB * ICh_to_XYZ(ICh);
	return all(greaterThanEqual(sRGB, vec3(0.0))) && all(lessThanEqual(sRGB, vec3(1.0)));
}

vec3 Zcam_tonemap(vec3 sRGB)
{
	vec3 ICh = XYZ_to_ICh(sRGB_to_XYZ * sRGB);

	const float s0 = 0.71;
	const float s1 = 1.04;
	const float p = 1.40;
	const float t0 = 0.01;
	float n = s1 * pow(ICh.x / (ICh.x + s0), p);
	ICh.x = clamp(n * n / (n + t0), 0.0, 1.0);

	if (!in_sRGB_gamut(ICh))
	{
		float C = ICh.y;
		ICh.y -= 0.5 * C;

		for (float i = 0.25; i >= 1.0 / 256.0; i *= 0.5)
		{
			ICh.y += (in_sRGB_gamut(ICh) ? i : -i) * C;
		}
	}

	return XYZ_to_sRGB * ICh_to_XYZ(ICh);
}

/////////////////////////////////////////////////////////////////////////////////////////
// sRGB conversion helpers
/////////////////////////////////////////////////////////////////////////////////////////

vec3 fromLinearSRGB(vec3 linearRGB)
{
	bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
	vec3 higher = vec3(1.055) * pow(linearRGB, vec3(1.0 / 2.4)) - vec3(0.055);
	vec3 lower = linearRGB * vec3(12.92);
	return mix(higher, lower, cutoff);
}

float fromLinearSRGB(float linearRGB)
{
	bool cutoff = linearRGB < 0.0031308;
	float higher = 1.055 * pow(linearRGB, 1.0 / 2.4) - 0.055;
	float lower = linearRGB * 12.92;
	return mix(higher, lower, cutoff);
}

vec3 toLinearSRGB(vec3 sRGB)
{
	bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
	vec3 higher = pow((sRGB + vec3(0.055)) / vec3(1.055), vec3(2.4));
	vec3 lower = sRGB / vec3(12.92);
	return mix(higher, lower, cutoff);
}

float toLinearSRGB(float sRGB)
{
	bool cutoff = sRGB < 0.04045;
	float higher = pow((sRGB + 0.055) / 1.055, 2.4);
	float lower = sRGB / 12.92;
	return mix(higher, lower, cutoff);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Uncharted2
/////////////////////////////////////////////////////////////////////////////////////////

vec3 Uncharted2Tonemap(vec3 x)
{
	float Brightness = 0.28;
	x *= Brightness;
	float A = 0.28;
	float B = 0.29;
	float C = 0.10;
	float D = 0.2;
	float E = 0.025;
	float F = 0.35;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 unchartedTonemapping(vec3 color)
{
	vec3 curr = Uncharted2Tonemap(color * 4.7);
	return curr / Uncharted2Tonemap(vec3(15.2));
}

/////////////////////////////////////////////////////////////////////////////////////////
// PBR Neutral
/////////////////////////////////////////////////////////////////////////////////////////

vec3 PBRNeutralToneMapping(vec3 color)
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression) { return color; }

	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
	return mix(color, newPeak * vec3(1.0, 1.0, 1.0), g);
}

// Extra grading controls applied in linear space before the selected tonemapper.
vec3 adjustColor(vec3 color, float saturation, float vibrance, float gamma,
	float shadowBoost, float highlightBoost, vec3 lift, vec3 gain)
{
	float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));

	color = mix(vec3(luma), color, saturation);

	float satStrength = 1.0 - smoothstep(0.0, 1.0, max(color.r, max(color.g, color.b)));
	color = mix(color, mix(vec3(luma), color, vibrance), satStrength);

	float shadowMask = smoothstep(0.0, 0.5, luma);
	float highlightMask = smoothstep(0.5, 1.0, luma);
	color *= mix(vec3(1.0), vec3(1.0 + shadowBoost), shadowMask);
	color *= mix(vec3(1.0), vec3(1.0 + highlightBoost), highlightMask);

	color = (color + lift) * gain;

	color = pow(max(color, 0.0), vec3(gamma));

	return color;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Tonemapper routing and gamma management
/////////////////////////////////////////////////////////////////////////////////////////

float toLinear(float a)
{
	int tonemapper = getTonemapper();
	if (tonemapper == 1)
	{
		return toLinearAXG(a);
	}
	return toLinearSRGB(a);
}

vec3 toLinear(vec3 a)
{
	return vec3(toLinear(a.r), toLinear(a.g), toLinear(a.b));
}

vec3 tonemapFunction(vec3 c)
{
	int tonemapper = getTonemapper();
	if (tonemapper == 0)
	{
		return ACESFitted(c);
	}
	if (tonemapper == 1)
	{
		return AGX(c);
	}
	if (tonemapper == 2)
	{
		return Zcam_tonemap(c);
	}
	if (tonemapper == 3)
	{
		return unchartedTonemapping(c);
	}
	if (tonemapper == 4)
	{
		return PBRNeutralToneMapping(c);
	}

	return ACESFitted(c);
}

vec3 toGammaSpace(vec3 a)
{
	int tonemapper = getTonemapper();
	if (tonemapper == 1)
	{
		// AgX already outputs display-referred color.
		return a;
	}

	return fromLinearSRGB(a);
}

//https://www.shadertoy.com/view/lsKSWR
vec3 applyVignette(vec3 color, float intensity, vec2 uv)
{
	uv *= 1.0 - uv.yx;

	float vig = uv.x * uv.y * 15.0;
	vig = pow(vig, mix(0.25, 1.0, intensity));

	return mix(color, color * vig, intensity);
}

/* Gradient noise from Jorge Jimenez's presentation: */
/* http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare */
float gradientNoise(in vec2 uv)
{
	return fract(52.9829189 * fract(dot(uv, vec2(0.06711056, 0.00583715))));
}

void main()
{
	vec2 clampedUV = clamp(inUV, vec2(0.0), vec2(1.0));

	vec3 hdrColor = max(texture(uHdrTexture, clampedUV).rgb, vec3(0.0));
	hdrColor *= getExposure();

	vec3 linearColor = toLinear(hdrColor);
	if (hasCosmeticLightMask() > 0.5)
	{
		float lightMask = max(texture(uLightMaskTexture, clampedUV).r, 0.0);
		linearColor *= (1.0 + lightMask);
	}
	linearColor = adjustColor(linearColor,
		getSaturation(),
		getVibrance(),
		getGradingGamma(),
		getShadowBoost(),
		getHighlightBoost(),
		uLift.xyz,
		uGain.xyz);
	vec3 toneMapped = tonemapFunction(linearColor);
	vec3 outRgb = toGammaSpace(max(toneMapped, vec3(0.0)));

	float strength = 1;
	outRgb.rgb += (strength * 1.0 / 255.0) * gradientNoise(gl_FragCoord.xy) - (strength * 0.5 / 255.0);

	outRgb.rgb = applyVignette(outRgb.rgb, getVignette(), clampedUV);

	outColor = vec4(clamp(outRgb, vec3(0.0), vec3(1.0)), 1.0);
}
