#include <metal_stdlib>
using namespace metal;

struct v2f
{
    float4 position [[position]];
    float3 normal;
    float2 uv;
    float4 pos;
    float3 normalW;
    float3 tangentW;
    float3 bitangentW;
};

struct FrameData
{
    float angle;
};

struct CameraData
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 normal;
    float3 dir;
};
struct MaterialData
{
    float4 baseColor;
    float roughnessFactor;
    float metallicFactor;
};

float pow5(float value) {
    float sq = value*value;
    return sq*sq*value;
}
float diffuseBRDF_Burley(float NdotL, float NdotV, float VdotH, float roughness) {
    float EPSILON = 1e-6;
    float diffuseFresnelNV = pow5(metal::saturate(1.0-NdotL)+EPSILON);
    float diffuseFresnelNL = pow5(metal::saturate(1.0-NdotV)+EPSILON);
    float diffuseFresnel90 = 0.5+2.0*VdotH*VdotH*roughness;
    float fresnel = (1.0+(diffuseFresnel90-1.0)*diffuseFresnelNL) *
    (1.0+(diffuseFresnel90-1.0)*diffuseFresnelNV);
    return fresnel/M_PI_F;
}

static half3 SchlickFresnel(half3 F0, float LdotH)
{
    return F0 + (1 - F0) * pow(1.0 - LdotH, 5.0);
}

static float SmithGeometric(float NdotL, float NdotV, float roughness)
{
    float k = roughness * 0.5;
    float Gl = NdotL / ((NdotL * (1 - k)) + k);
    float Gv = NdotV / ((NdotV * (1 - k)) + k);
    return Gl * Gv;
}

static float TrowbridgeReitzNDF(float NdotH, float roughness)
{
    float roughnessSq = roughness * roughness;
    float f = NdotH * (NdotH * roughnessSq - NdotH) + 1;
    return roughnessSq / (M_PI_F * f * f);
}

v2f vertex vertexMain( device const packed_float3* positions [[buffer(0)]],
                        device const packed_float3* normals [[buffer(1)]], 
                        device const packed_float2* uvs [[buffer(4)]], 
                        device const packed_float4* tangents [[buffer(5)]], 
                        constant FrameData* frameData [[buffer(2)]], 
                        device const CameraData& cameraData [[buffer(3)]],
                        uint vertexId [[vertex_id]] )
{
    v2f o;
    o.position = cameraData.projection * cameraData.view * cameraData.model * float4( positions[ vertexId ], 1.0 );
    o.normal = normals[ vertexId ];
    o.uv = uvs[ vertexId ];
    o.pos = cameraData.model * float4( positions[ vertexId ], 1.0 );

    float4 inTangent = tangents[ vertexId ];
    o.normalW = normalize(float3(cameraData.model * float4(o.normal.xyz, 0.0)));
    o.tangentW = normalize(float3(cameraData.model * float4(inTangent.xyz, 0.0)));
    o.bitangentW = cross(o.normalW, o.tangentW) * inTangent.w;

    return o;
}

half4 fragment fragmentMain( v2f in [[stage_in]],
                            constant CameraData &uniforms [[buffer(0)]],
                            constant MaterialData &material [[buffer(1)]],
                            texture2d< half, access::sample > baseColorTexture [[texture(0)]],
                            texture2d< half, access::sample > normalTexture [[texture(1)]],
                            texture2d< half, access::sample > pbrTexture [[texture(2)]],
                            texture2d< half, access::sample > emissiveTexture [[texture(3)]],
                            texture2d< half, access::sample > occlusionTexture [[texture(4)]]
) {
    float roughness = material.roughnessFactor;
    float metallic = material.metallicFactor;
    half3 baseColor = half3(material.baseColor.rgb);
    half alpha = material.baseColor.a;

    constexpr sampler s( address::repeat, filter::linear );
    half3 emissive = emissiveTexture.sample( s, in.uv ).rgb;
    half ao = occlusionTexture.sample( s, in.uv ).r;
    baseColor *= baseColorTexture.sample( s, in.uv ).rgb;
    alpha *= baseColorTexture.sample( s, in.uv ).a;
    float3 N = float3(normalTexture.sample( s, in.uv ).rgb);
    roughness *= pbrTexture.sample( s, in.uv ).g;
    metallic *= pbrTexture.sample( s, in.uv ).b;
    N = normalize(float3x3(in.tangentW, in.bitangentW, in.normalW) * (2.0 * N - 1.0));
    roughness = fmax(roughness, 0.01);
    if (isnan(length(in.tangentW))) {
        N = normalize((uniforms.normal * float4(in.normal, 0.0)).xyz);
    } else {
        N = normalize((uniforms.model * float4(in.normal, 0.0)).xyz);
    }

    half3 lightColor = half3(1.0);
    float3 lightPos = uniforms.dir;
    float3 viewPos = uniforms.dir;
    float3 V = normalize(viewPos - in.pos.xyz);
    float3 L = normalize(lightPos - in.pos.xyz);
    float3 H = normalize(L + V);
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float NdotV = saturate(dot(N, V));
    float VdotH = saturate(dot(V, H));

    half3 f0(0.04);
    half3 specularColor = mix(f0, baseColor.rgb, metallic);
    half3 F0 = specularColor.rgb;

    half3 ambient = 0.01 * lightColor;
    float diff = diffuseBRDF_Burley(NdotL, NdotV, VdotH, roughness);
    half3 diffuse = diff * lightColor;

    half3 F = SchlickFresnel(F0, VdotH);
    float G = SmithGeometric(NdotL, NdotV, roughness);
    float D = TrowbridgeReitzNDF(NdotH, roughness);
    half3 specular = NdotL * D * F * G / (4.0 * NdotL * NdotV);

    return half4(ao * (emissive + baseColor * diffuse + specular * 5.0), alpha);
}
