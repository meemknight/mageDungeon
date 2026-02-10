// Vertex shader for the cosmetic dynamic light mask geometry pass.
#version 450

layout(location = 0) in vec2 inClipPos;
layout(location = 1) in vec2 inWorldPos;
layout(location = 2) in vec2 inLightCenter;
layout(location = 3) in float inLightRadius;
layout(location = 4) in float inFalloffPower;
layout(location = 5) in vec3 inLightColor;
layout(location = 6) in float inTransmission;
layout(location = 7) in float inTransmissionStartDistance;

layout(location = 0) out vec2 outWorldPos;
layout(location = 1) out vec2 outLightCenter;
layout(location = 2) out float outLightRadius;
layout(location = 3) out float outFalloffPower;
layout(location = 4) out vec3 outLightColor;
layout(location = 5) out float outTransmission;
layout(location = 6) out float outTransmissionStartDistance;

void main()
{
	gl_Position = vec4(inClipPos, 0.0, 1.0);
	outWorldPos = inWorldPos;
	outLightCenter = inLightCenter;
	outLightRadius = inLightRadius;
	outFalloffPower = inFalloffPower;
	outLightColor = inLightColor;
	outTransmission = inTransmission;
	outTransmissionStartDistance = inTransmissionStartDistance;
}
