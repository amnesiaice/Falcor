__import Shading;
__import DefaultVS;

cbuffer PerFrameCB : register(b0)
{
    LightData gDirLight;
    LightData gPointLight;
    bool gConstColor;
    float3 gAmbient;
};

float4 main(VertexOut vOut) : SV_TARGET
{
    if(gConstColor)
    {
        return float4(0, 1, 0, 1);
    }
    else
    {
        ShadingData sd = prepareShadingData(vOut, gMaterial, gCamera.posW);
        float4 finalColor;
        finalColor.a = 1;
        finalColor.rgb = evalMaterial(sd, gDirLight, 1).color.rgb;
        finalColor.rgb += evalMaterial(sd, gPointLight, 1).color.rgb;
        return finalColor;
    }
}
