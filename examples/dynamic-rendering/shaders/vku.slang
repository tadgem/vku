module vku;

public float4x4 _inverse(float4x4 m)
{
    __target_switch
    {
    case glsl: __intrinsic_asm "inverse";
    case spirv: return spirv_asm {
        OpExtInst $$float4x4 result glsl450 MatrixInverse $m
    };
    }
}