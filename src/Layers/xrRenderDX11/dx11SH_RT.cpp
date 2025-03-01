#include "stdafx.h"
#pragma hdrstop

#include "Layers/xrRender/ResourceManager.h"
#include "dx11TextureUtils.h"

CRT::CRT()
{
    pSurface = NULL;
    pRT = NULL;
    ZeroMemory(pZRT, sizeof(pZRT));
#ifdef USE_DX11
    pUAView = NULL;
#endif
    dwWidth = 0;
    dwHeight = 0;
    fmt = D3DFMT_UNKNOWN;
}
CRT::~CRT()
{
    destroy();

    // release external reference
    RImplementation.Resources->_DeleteRT(this);
}

bool CRT::used_as_depth() const
{
    switch (fmt)
    {
    case D3DFMT_D15S1:
    case D3DFMT_D24X8:
    case D3DFMT_D32S8X24:
    case MAKEFOURCC('D', 'F', '2', '4'):
        return true;
    default:
        return false;
    }
}

void CRT::create(LPCSTR Name, u32 w, u32 h, D3DFORMAT f, u32 SampleCount /*= 1*/, u32 slices_num /*=1*/, Flags32 flags /*= {}*/)
{
    if (pSurface)
        return;

    R_ASSERT(HW.pDevice && Name && Name[0] && w && h);
    _order = CPU::QPC(); // Device.GetTimerGlobal()->GetElapsed_clk();

    dwWidth = w;
    dwHeight = h;
    fmt = f;
    sampleCount = SampleCount;
    n_slices = slices_num;

    const bool createBaseTarget = flags.test(CreateBase);
    if (createBaseTarget)
    {
        dwFlags |= CreateBase;
        if (!used_as_depth())
        {
            u32 idx;
            char const* str = strrchr(Name, '_');
            sscanf(++str, "%d", &idx);
            R_CHK(HW.m_pSwapChain->GetBuffer(idx, __uuidof(ID3DTexture2D), (LPVOID*)&pSurface));
        }
    }

    //	DirectX 10 supports non-power of two textures
    // Pow2
    // if (!btwIsPow2(w) || !btwIsPow2(h))
    //{
    //	if (!HW.Caps.raster.bNonPow2)	return;
    //}

    // Check width-and-height of render target surface
    if (w > D3D_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return;
    if (h > D3D_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return;

    // Select usage
    u32 usage = D3DUSAGE_RENDERTARGET;
    if (used_as_depth())
        usage = D3DUSAGE_DEPTHSTENCIL;

    DXGI_FORMAT dx11FMT;

    switch (fmt)
    {
    case D3DFMT_D32S8X24:
        dx11FMT = DXGI_FORMAT_R32G8X24_TYPELESS;
        usage = D3DUSAGE_DEPTHSTENCIL;
        break;

    case D3DFMT_D24S8:
        dx11FMT = DXGI_FORMAT_R24G8_TYPELESS;
        usage = D3DUSAGE_DEPTHSTENCIL;
        break;

    case D3DFMT_D32F_LOCKABLE:
        dx11FMT = DXGI_FORMAT_R32_TYPELESS;
        usage = D3DUSAGE_DEPTHSTENCIL;
        break;

    case D3DFMT_D16_LOCKABLE:
        dx11FMT = DXGI_FORMAT_R16_TYPELESS;
        usage = D3DUSAGE_DEPTHSTENCIL;
        break;

    default:
        dx11FMT = dx11TextureUtils::ConvertTextureFormat(fmt);
        break;
    }
    if (createBaseTarget) // just override
        dx11FMT = dx11TextureUtils::ConvertTextureFormat(fmt);

    const bool useAsDepth = usage != D3DUSAGE_RENDERTARGET;

    // Validate render-target usage
    u32 required = D3D_FORMAT_SUPPORT_TEXTURE2D;

    if (useAsDepth)
        required |= D3D_FORMAT_SUPPORT_DEPTH_STENCIL;
    else
        required |= D3D_FORMAT_SUPPORT_RENDER_TARGET;

    if (!HW.CheckFormatSupport(dx11FMT, required))
        return;

    // Try to create texture/surface
    RImplementation.Resources->Evict();

    // Create the render target texture
    D3D_TEXTURE2D_DESC desc;
    if (pSurface)
        pSurface->GetDesc(&desc);
    else
    {
        const u32 initialBindFlag = createBaseTarget ? 0 : D3D_BIND_SHADER_RESOURCE;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = dwWidth;
        desc.Height = dwHeight;
        desc.MipLevels = 1;
        desc.ArraySize = n_slices;
        desc.Format = dx11FMT;
        desc.SampleDesc.Count = SampleCount;
        desc.Usage = D3D_USAGE_DEFAULT;
        if (SampleCount <= 1)
            desc.BindFlags = initialBindFlag | (useAsDepth ? D3D_BIND_DEPTH_STENCIL : D3D_BIND_RENDER_TARGET);
        else
        {
            desc.BindFlags = (useAsDepth ? D3D_BIND_DEPTH_STENCIL : (initialBindFlag | D3D_BIND_RENDER_TARGET));
            if (RImplementation.o.msaa_opt)
            {
                desc.SampleDesc.Quality = u32(D3D_STANDARD_MULTISAMPLE_PATTERN);
            }
        }

#ifdef USE_DX11
        if (flags.test(CreateUAV))
        {
            dwFlags |= CreateUAV;

            if (HW.FeatureLevel >= D3D_FEATURE_LEVEL_11_0 && !useAsDepth && SampleCount == 1)
                desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }
#else
        UNUSED(flags);
#endif

        CHK_DX(HW.pDevice->CreateTexture2D(&desc, NULL, &pSurface));
        // R_CHK		(pSurface->GetSurfaceLevel	(0,&pRT)); // TODO: DX11: check if texture is created?
#ifdef DEBUG
        Msg("* created RT(%s), %dx%d, format = %d samples = %d", Name, w, h, dx11FMT, SampleCount);
#endif // DEBUG
    }
    HW.stats_manager.increment_stats_rtarget(pSurface);

#ifdef DEBUG
    if (pSurface)
    {
        pSurface->SetPrivateData(WKPDID_D3DDebugObjectName, cName.size(), cName.c_str());
    }
#endif

    // OK
    if (useAsDepth)
    {
        D3D_DEPTH_STENCIL_VIEW_DESC ViewDesc;
        ZeroMemory(&ViewDesc, sizeof(ViewDesc));

        ViewDesc.Format = DXGI_FORMAT_UNKNOWN;
        if (SampleCount <= 1)
        {
            ViewDesc.ViewDimension = n_slices > 1 ? D3D_DSV_DIMENSION_TEXTURE2DARRAY : D3D_DSV_DIMENSION_TEXTURE2D;;
        }
        else
        {
            VERIFY(n_slices == 1);
            ViewDesc.ViewDimension = D3D_DSV_DIMENSION_TEXTURE2DMS;
            ViewDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
        }

        ViewDesc.Texture2D.MipSlice = 0;

        switch (desc.Format)
        {
        case DXGI_FORMAT_R16_TYPELESS:
            ViewDesc.Format = DXGI_FORMAT_D16_UNORM;
            break;

        case DXGI_FORMAT_R24G8_TYPELESS:
            ViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            break;

        case DXGI_FORMAT_R32_TYPELESS:
            ViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
            break;

        case DXGI_FORMAT_R32G8X24_TYPELESS:
            ViewDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            break;

        default:
            ViewDesc.Format = desc.Format;
        }

        if (n_slices > 1)
        {
            ViewDesc.Texture2DArray.ArraySize = n_slices;
        }

        CHK_DX(HW.pDevice->CreateDepthStencilView(pSurface, &ViewDesc, &dsv_all));
#if defined(DEBUG)
        {
            char name[128];
            xr_sprintf(name, "%s:all", Name);
            dsv_all->SetPrivateData(WKPDID_D3DDebugObjectName, xr_strlen(name), name);
        }
#endif

        dsv_per_slice.resize(n_slices);
        for (int idx = 0; idx < n_slices; ++idx)
        {
            ViewDesc.Texture2DArray.ArraySize = 1;
            ViewDesc.Texture2DArray.FirstArraySlice = idx;
            CHK_DX(HW.pDevice->CreateDepthStencilView(pSurface, &ViewDesc, &dsv_per_slice[idx]));
#if DEBUG
            {
                char name[128];
                xr_sprintf(name, "%s:s%d", Name, idx);
                dsv_per_slice[idx]->SetPrivateData(WKPDID_D3DDebugObjectName, xr_strlen(name), name);
            }
#endif
        }
        for (int id = 0; id < R__NUM_CONTEXTS; ++id)
        {
            set_slice_write(id, -1);
        }
    }
    else
        CHK_DX(HW.pDevice->CreateRenderTargetView(pSurface, 0, &pRT));

#ifdef USE_DX11
    if (flags.test(CreateUAV) && HW.FeatureLevel >= D3D_FEATURE_LEVEL_11_0 && !useAsDepth && SampleCount == 1)
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
        ZeroMemory(&UAVDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
        UAVDesc.Format = dx11FMT;
        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = dwWidth * dwHeight;
        CHK_DX(HW.pDevice->CreateUnorderedAccessView(pSurface, &UAVDesc, &pUAView));
    }
#endif
    if (createBaseTarget)
    {
        // pTexture->surface_set(pSurface) creates shader resource view
        // which requires D3D_BIND_SHADER_RESOURCE flag to be set,
        // but it isn't set for Base target.
        return;
    }

    pTexture = RImplementation.Resources->_CreateTexture(Name);
    pTexture->surface_set(pSurface);
}

void CRT::destroy()
{
    if (pTexture._get())
    {
        pTexture->surface_set(0);
        pTexture = NULL;
    }
    _RELEASE(pRT);
    for (auto& dsv : dsv_per_slice)
    {
        _RELEASE(dsv);
    }
    _RELEASE(dsv_all);

    HW.stats_manager.decrement_stats_rtarget(pSurface);
    _RELEASE(pSurface);
#ifdef USE_DX11
    _RELEASE(pUAView);
#endif
}

void CRT::set_slice_read(int slice)
{
    VERIFY(slice <= n_slices || slice == -1);
    pTexture->set_slice(slice);
}

void CRT::set_slice_write(u32 context_id, int slice)
{ 
    VERIFY(slice <= n_slices || slice == -1);
    pZRT[context_id] = (slice < 0) ? dsv_all : dsv_per_slice[slice];
}


void CRT::reset_begin() { destroy(); }
void CRT::reset_end() { create(*cName, dwWidth, dwHeight, fmt, sampleCount, { dwFlags }); }

void CRT::resolve_into(CRT& destination) const // TODO: this should be moved into backend
{
    VERIFY(fmt == destination.fmt); // only RTs with same format supported
    auto srcSurf = pTexture->surface_get();
    auto destSurf = destination.pTexture->surface_get();
    HW.get_context(CHW::IMM_CTX_ID)->ResolveSubresource(destSurf, 0,
        srcSurf, 0, dx11TextureUtils::ConvertTextureFormat(fmt));
    _RELEASE(srcSurf);
    _RELEASE(destSurf);
}

void resptrcode_crt::create(LPCSTR Name, u32 w, u32 h, D3DFORMAT f, u32 SampleCount /*= 1*/, u32 slices_num /*=1*/, Flags32 flags /*= 0*/)
{
    _set(RImplementation.Resources->_CreateRT(Name, w, h, f, SampleCount, slices_num, flags));
}

//////////////////////////////////////////////////////////////////////////
/*	DX10 cut
CRTC::CRTC			()
{
    if (pSurface)	return;

    pSurface									= NULL;
    pRT[0]=pRT[1]=pRT[2]=pRT[3]=pRT[4]=pRT[5]	= NULL;
    dwSize										= 0;
    fmt											= D3DFMT_UNKNOWN;
}
CRTC::~CRTC			()
{
    destroy			();

    // release external reference
    DEV->_DeleteRTC	(this);
}

void CRTC::create	(LPCSTR Name, u32 size,	D3DFORMAT f)
{
    R_ASSERT	(HW.pDevice && Name && Name[0] && size && btwIsPow2(size));
    _order		= CPU::QPC();	//Device.GetTimerGlobal()->GetElapsed_clk();

    HRESULT		_hr;

    dwSize		= size;
    fmt			= f;

    // Get caps
    //D3DCAPS9	caps;
    //R_CHK		(HW.pDevice->GetDeviceCaps(&caps));

    //	DirectX 10 supports non-power of two textures
    // Pow2
    //if (!btwIsPow2(size))
    //{
    //	if (!HW.Caps.raster.bNonPow2)	return;
    //}

    // Check width-and-height of render target surface
    if (size>D3Dxx_REQ_TEXTURECUBE_DIMENSION)		return;

    //	TODO: DX11: Validate cube texture format
    // Validate render-target usage
    //_hr = HW.pD3D->CheckDeviceFormat(
    //	HW.DevAdapter,
    //	HW.m_DriverType,
    //	HW.Caps.fTarget,
    //	D3DUSAGE_RENDERTARGET,
    //	D3DRTYPE_CUBETEXTURE,
    //	f
    //	);
    //if (FAILED(_hr))					return;

    // Try to create texture/surface
    DEV->Evict					();
    _hr = HW.pDevice->CreateCubeTexture	(size, 1, D3DUSAGE_RENDERTARGET, f, D3DPOOL_DEFAULT, &pSurface,NULL);
    if (FAILED(_hr) || (0==pSurface))	return;

    // OK
    Msg			("* created RTc(%s), 6(%d)",Name,size);
    for (u32 face=0; face<6; face++)
        R_CHK	(pSurface->GetCubeMapSurface	((D3DCUBEMAP_FACES)face, 0, pRT+face));
    pTexture	= DEV->_CreateTexture	(Name);
    pTexture->surface_set						(pSurface);
}

void CRTC::destroy		()
{
    pTexture->surface_set	(0);
    pTexture				= NULL;
    for (u32 face=0; face<6; face++)
        _RELEASE	(pRT[face]	);
    _RELEASE	(pSurface	);
}
void CRTC::reset_begin	()
{
    destroy		();
}
void CRTC::reset_end	()
{
    create		(*cName,dwSize,fmt);
}

void resptrcode_crtc::create(LPCSTR Name, u32 size, D3DFORMAT f)
{
    _set		(DEV->_CreateRTC(Name,size,f));
}
*/
