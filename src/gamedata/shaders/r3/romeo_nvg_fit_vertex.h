#include "romeo_nvg_fit.h"

#if defined(SHADER_BUS) && SHADER_BUS >= 7 && ROMEO_NVG_FIT_PART != 0
uniform float4 bus_romeo_nvg_fit;
#endif

float4 RomeoNvgFitHudVertex(float4 clipPosition, float4 modelPosition)
{
#if ROMEO_NVG_FIT_QUAD
    // Use the corrected first-person eye position before applying the pod fit.
    clipPosition += mul(m_P, float4(0.0, 0.0, 0.118805097, 0.0));
    clipPosition.xy *= 4.703894794;
#endif

#if defined(SHADER_BUS) && SHADER_BUS >= 7 && ROMEO_NVG_FIT_PART != 0
    const bool quad = ROMEO_NVG_FIT_QUAD != 0;
    float weight = saturate(bus_romeo_nvg_fit.x);
#if ROMEO_NVG_FIT_PART == 2
    // Taper across the bridge to the adjusted pod positions, with the shoe
    // fixed in the centre and each contact moving with its own pod.
    float x = mul(m_WV, modelPosition).x - (quad ? 0.0 : 0.0007);
    bool right = x >= 0.0;
    weight *= quad ? smoothstep(0.017, 0.045, abs(x)) : smoothstep(0.007, 0.024, abs(x));
#else
    const bool right = ROMEO_NVG_FIT_PART == 1;
#endif
    float2 viewport = float2(0.5 * screen_res.x / screen_res.y, -0.5);
#if ROMEO_NVG_FIT_DIGI
    float2 adjustment = RomeoNvgDigitalFitDelta(clipPosition.xy * viewport, clipPosition.w, right);
#else
    float2 adjustment = RomeoNvgFitDelta(clipPosition.xy * viewport, clipPosition.w, quad, right);
#endif
    clipPosition.xy += weight * adjustment / viewport;
#if ROMEO_NVG_FIT_GLASS && !ROMEO_NVG_FIT_DIGI
    // The deployed glass sits in front of the sharp casing. The remaining
    // housing still depth-tests normally around the clear apertures.
    if (weight >= 0.9999)
        clipPosition.z = 0.00005 * clipPosition.w;
#endif
#endif
    return clipPosition;
}
