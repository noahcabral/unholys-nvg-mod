#ifndef ROMEO_NVG_FACE_GLOW_H
#define ROMEO_NVG_FACE_GLOW_H

#if defined(SHADER_BUS) && SHADER_BUS >= 7
uniform float4 bus_romeo_eye_glow;
uniform float4 bus_romeo_eye_left;
uniform float4 bus_romeo_eye_right;
uniform float4 bus_romeo_face_side;
uniform float4 bus_romeo_face_up;
uniform float4 bus_romeo_face_forward;

Texture2D<float4> s_romeo_face_glow_mask;
#ifdef USE_MSAA
Texture2DMS<float4> s_romeo_face_glow_position;
#else
Texture2D<float4> s_romeo_face_glow_position;
#endif

float RomeoNvgEyeSpill(float3 surfacePoint, float3 center, float3 side, float3 up,
    float3 forward, uint eye, uint profile, float physicalRadius)
{
    float3 offset = surfacePoint - center;
    if (dot(offset, offset) > 0.18 * 0.18)
        return 0.0;

    float2 patch = float2(dot(side, offset), dot(up, offset));
    float radius = length(patch) / physicalRadius;
    if (radius >= 1.0)
        return 0.0;

    int2 tile = int2(eye, profile) * 128;
    int2 coord = clamp(int2((patch / 0.030 + 0.5) * 128), 0, 127);
    float2 stored = s_romeo_face_glow_mask.Load(int3(tile + coord, 0)).rg;
    if (stored.x <= 0.0)
        return 0.0;

    // The depth atlas contains only exposed skin from the fitted visual.
    // Test against the visible surface so goggles and masks block the spill.
    float2 skinDepth = 0.08 + (stored * 255.0 - 1.0) * (0.08 / 254.0);
    float depth = dot(forward, offset);
    // Oblique views see skin farther back than the frontal depth sample.
    // Keep the near cutoff tight so the eyepiece in front stays out of the light.
    float front = 1.0 - smoothstep(0.0035, 0.007, depth - skinDepth.x);
    float back = 1.0 - smoothstep(0.010, 0.021, skinDepth.y - depth);
    float coverage = front * back;
    return coverage * (1.0 - smoothstep(0.74, 1.0, radius)) * (1.0 - 0.35 * radius);
}

float3 RomeoNvgFaceGlow(float2 pixel, float3 image)
{
    uint mode = uint(round(bus_romeo_eye_glow.w));
    if (mode == 0u || mode > 3u || ssfx_issvp.x > 0.5)
        return image;

#ifdef USE_MSAA
    float z = s_romeo_face_glow_position.Load(int2(pixel), 0).z;
#else
    float z = s_romeo_face_glow_position.Load(int3(int2(pixel), 0)).z;
#endif
    if (z <= 0.0)
        return image;

    float3 surfacePoint = float3(z * (pixel * pos_decompression_params.zw - pos_decompression_params.xy), z);
    float3 side = mul((float3x3)m_V, bus_romeo_face_side.xyz);
    float3 up = mul((float3x3)m_V, bus_romeo_face_up.xyz);
    float3 forward = mul((float3x3)m_V, bus_romeo_face_forward.xyz);
    float radius = mode == 3u ? 0.01332 : 0.015;
    uint profile = uint(round(bus_romeo_eye_left.w));

    float3 right = mul(m_V, float4(bus_romeo_eye_right.xyz, 1.0)).xyz;
    float amount = RomeoNvgEyeSpill(surfacePoint, right, side, up, forward, 1u, profile, radius);
    if (mode != 2u)
    {
        float3 left = mul(m_V, float4(bus_romeo_eye_left.xyz, 1.0)).xyz;
        amount = max(amount, RomeoNvgEyeSpill(surfacePoint, left, side, up, forward, 0u, profile, radius));
    }
    return image + bus_romeo_eye_glow.rgb * (amount * 0.32);
}
#endif

#endif
