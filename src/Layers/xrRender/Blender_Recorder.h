// Blender_Recorder.h: interface for the CBlender_Recorder class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BLENDER_RECORDER_H__1F549674_8674_4EB2_95E6_E6BC19218A6C__INCLUDED_)
#define AFX_BLENDER_RECORDER_H__1F549674_8674_4EB2_95E6_E6BC19218A6C__INCLUDED_
#pragma once

#include "Layers/xrRender/tss.h"

#pragma pack(push, 4)

class CBlender_Compile
{
public:
    static constexpr auto InvalidStage = std::numeric_limits<u32>::max();

    sh_list L_textures;
    sh_list L_constants;
    sh_list L_matrices;

    LPCSTR detail_texture;
    R_constant_setup* detail_scaler;

    bool bFFP;
    bool bDetail;
    bool bDetail_Diffuse;
    bool bDetail_Bump;
    BOOL bUseSteepParallax;
    int iElement;

public:
    CSimulator RS;
    IBlender* BT;
    ShaderElement* SH;
#ifdef USE_DX11
    enum
    {
        NO_TESS = 0,
        TESS_PN = 1,
        TESS_HM = 2,
        TESS_PN_HM = 3
    };
    u32 TessMethod;
#endif

private:
    SPass dest;
    R_constant_table ctable;

    STextureList passTextures;
    SMatrixList passMatrices;
    SConstantList passConstants;
    u32 dwStage;

private:
    inline u32 BC(BOOL v) const { return v ? 1 : 0; }
    void SetupSampler(u32 stage, pcstr sampler);

public:
    u32 SampledImage(pcstr sampler, pcstr image, shared_str texture);

    CSimulator& R() { return RS; }
    void SetParams(int iPriority, bool bStrictB2F);
    void SetMapping();

    // R1-compiler
    void PassBegin();
    u32 Pass() { return SH->passes.size(); }
    void PassSET_ZB(BOOL bZTest, BOOL bZWrite, BOOL bInvertZTest = FALSE);
    void PassSET_ablend_mode(BOOL bABlend, u32 abSRC, u32 abDST);
    void PassSET_ablend_aref(BOOL aTest, u32 aRef);
    void PassSET_Blend(BOOL bABlend, u32 abSRC, u32 abDST, BOOL aTest, u32 aRef);
    void PassSET_Blend_BLEND(BOOL bAref = FALSE, u32 ref = 0)
    {
        PassSET_Blend(TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA, bAref, ref);
    }
    void PassSET_Blend_SET(BOOL bAref = FALSE, u32 ref = 0)
    {
        PassSET_Blend(FALSE, D3DBLEND_ONE, D3DBLEND_ZERO, bAref, ref);
    }
    void PassSET_Blend_ADD(BOOL bAref = FALSE, u32 ref = 0)
    {
        PassSET_Blend(TRUE, D3DBLEND_ONE, D3DBLEND_ONE, bAref, ref);
    }
    void PassSET_Blend_MUL(BOOL bAref = FALSE, u32 ref = 0)
    {
        PassSET_Blend(TRUE, D3DBLEND_DESTCOLOR, D3DBLEND_ZERO, bAref, ref);
    }
    void PassSET_Blend_MUL2X(BOOL bAref = FALSE, u32 ref = 0)
    {
        PassSET_Blend(TRUE, D3DBLEND_DESTCOLOR, D3DBLEND_SRCCOLOR, bAref, ref);
    }
    void PassSET_LightFog(BOOL bLight, BOOL bFog);
    void PassSET_Shaders(pcstr _vs, pcstr _ps, pcstr _gs = "null", pcstr _hs = "null", pcstr _ds = "null");
    void PassEnd();

    void StageBegin();
    u32 Stage() { return dwStage; }
    void StageSET_Address(u32 adr);
    void StageSET_XForm(u32 tf, u32 tc);
    void StageSET_Color(u32 a1, u32 op, u32 a2);
    void StageSET_Color3(u32 a1, u32 op, u32 a2, u32 a3);
    void StageSET_Alpha(u32 a1, u32 op, u32 a2);
    void StageSET_TMC(LPCSTR T, LPCSTR M, LPCSTR C, int UVW_channel);
    void Stage_Texture(LPCSTR name, u32 address = D3DTADDRESS_WRAP, u32 fmin = D3DTEXF_LINEAR,
        u32 fmip = D3DTEXF_LINEAR, u32 fmag = D3DTEXF_LINEAR);
    void StageTemplate_LMAP0();
    void Stage_Matrix(LPCSTR name, int UVW_channel);
    void Stage_Constant(LPCSTR name);
    void StageEnd();

    // R1/R2-compiler	[programmable]
    u32 i_Sampler(LPCSTR name) const;
    void i_Texture(u32 s, LPCSTR name);
    void i_Projective(u32 s, bool b);
    void i_Address(u32 s, u32 address);
    void i_Filter_Min(u32 s, u32 f);
    void i_Filter_Mip(u32 s, u32 f);
    void i_Filter_Mag(u32 s, u32 f);
    void i_Filter_Aniso(u32 s, u32 f);
#if defined(USE_DX11)
    void i_dx11FilterAnizo(u32 s, BOOL value);
#endif
    void i_Filter(u32 s, u32 _min, u32 _mip, u32 _mag);
    void i_BorderColor(u32 s, u32 color);

    // R1/R2-compiler	[programmable]		- templates
    void r_Pass(LPCSTR vs, LPCSTR ps, bool bFog, BOOL bZtest = TRUE, BOOL bZwrite = TRUE, BOOL bABlend = FALSE,
        D3DBLEND abSRC = D3DBLEND_ONE, D3DBLEND abDST = D3DBLEND_ZERO, BOOL aTest = FALSE, u32 aRef = 0);

    void r_Constant(LPCSTR name, R_constant_setup* s);
#if defined(USE_DX11) || defined(USE_OGL)
    void r_Pass(LPCSTR vs, LPCSTR gs, LPCSTR ps, bool bFog, BOOL bZtest = TRUE, BOOL bZwrite = TRUE,
        BOOL bABlend = FALSE, D3DBLEND abSRC = D3DBLEND_ONE, D3DBLEND abDST = D3DBLEND_ZERO, BOOL aTest = FALSE,
        u32 aRef = 0);
#ifdef USE_DX11
    void r_TessPass(LPCSTR vs, LPCSTR hs, LPCSTR ds, LPCSTR gs, LPCSTR ps, bool bFog, BOOL bZtest = TRUE,
        BOOL bZwrite = TRUE, BOOL bABlend = FALSE, D3DBLEND abSRC = D3DBLEND_ONE, D3DBLEND abDST = D3DBLEND_ZERO,
        BOOL aTest = FALSE, u32 aRef = 0);
    void r_ComputePass(LPCSTR cs);
#endif
    void r_Stencil(BOOL Enable, u32 Func = D3DCMP_ALWAYS, u32 Mask = 0x00, u32 WriteMask = 0x00,
        u32 Fail = D3DSTENCILOP_KEEP, u32 Pass = D3DSTENCILOP_KEEP, u32 ZFail = D3DSTENCILOP_KEEP);
    void r_StencilRef(u32 Ref);
    void r_CullMode(D3DCULL Mode);
#endif // !USE_DX9

#if defined(USE_DX11)
    void r_dx11Texture(LPCSTR ResourceName, LPCSTR texture, bool recursive = false);
    void r_dx11Texture(LPCSTR ResourceName, shared_str texture, bool recursive = false)
    {
        return r_dx11Texture(ResourceName, texture.c_str(), recursive);
    };
    u32 r_dx11Sampler(LPCSTR ResourceName);
#endif // USE_DX11

    u32 r_Sampler(LPCSTR name, LPCSTR texture, bool b_ps1x_ProjectiveDivide = false, u32 address = D3DTADDRESS_WRAP,
        u32 fmin = D3DTEXF_LINEAR, u32 fmip = D3DTEXF_LINEAR, u32 fmag = D3DTEXF_LINEAR);
    u32 r_Sampler(LPCSTR name, shared_str texture, bool b_ps1x_ProjectiveDivide = false, u32 address = D3DTADDRESS_WRAP,
        u32 fmin = D3DTEXF_LINEAR, u32 fmip = D3DTEXF_LINEAR, u32 fmag = D3DTEXF_LINEAR)
    {
        return r_Sampler(name, texture.c_str(), b_ps1x_ProjectiveDivide, address, fmin, fmip, fmag);
    }
    void r_Sampler_rtf(LPCSTR name, LPCSTR texture, bool b_ps1x_ProjectiveDivide = false);
    void r_Sampler_clf(LPCSTR name, LPCSTR texture, bool b_ps1x_ProjectiveDivide = false);
    void r_Sampler_clw(LPCSTR name, LPCSTR texture, bool b_ps1x_ProjectiveDivide = false);

#ifdef USE_OGL
    void i_Comparison(u32 s, u32 func);
    void r_Sampler_cmp(pcstr name, pcstr texture, bool b_ps1x_ProjectiveDivide = false);
#endif // USE_OGL

    void r_ColorWriteEnable(bool cR = true, bool cG = true, bool cB = true, bool cA = true);
    void r_End();

    //

    CBlender_Compile();
    ~CBlender_Compile();

    void _cpp_Compile(ShaderElement* _SH);
    ShaderElement* _lua_Compile(LPCSTR namesp, LPCSTR name);
};
#pragma pack(pop)

#endif // !defined(AFX_BLENDER_RECORDER_H__1F549674_8674_4EB2_95E6_E6BC19218A6C__INCLUDED_)
