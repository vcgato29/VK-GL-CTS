/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Texture test utilities.
 *//*--------------------------------------------------------------------*/

#include "glsTextureTestUtil.hpp"
#include "gluDefs.hpp"
#include "gluDrawUtil.hpp"
#include "gluRenderContext.hpp"
#include "deRandom.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuTexCompareVerifier.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "qpWatchDog.h"
#include "deStringUtil.hpp"

using tcu::TestLog;
using std::vector;
using std::string;
using std::map;

namespace deqp
{
namespace gls
{
namespace TextureTestUtil
{

enum
{
	MIN_SUBPIXEL_BITS	= 4
};

SamplerType getSamplerType (tcu::TextureFormat format)
{
	using tcu::TextureFormat;

	switch (format.type)
	{
		case TextureFormat::SIGNED_INT8:
		case TextureFormat::SIGNED_INT16:
		case TextureFormat::SIGNED_INT32:
			return SAMPLERTYPE_INT;

		case TextureFormat::UNSIGNED_INT8:
		case TextureFormat::UNSIGNED_INT32:
		case TextureFormat::UNSIGNED_INT_1010102_REV:
			return SAMPLERTYPE_UINT;

		// Texture formats used in depth/stencil textures.
		case TextureFormat::UNSIGNED_INT16:
		case TextureFormat::UNSIGNED_INT_24_8:
			return (format.order == TextureFormat::D || format.order == TextureFormat::DS) ? SAMPLERTYPE_FLOAT : SAMPLERTYPE_UINT;

		default:
			return SAMPLERTYPE_FLOAT;
	}
}

SamplerType getFetchSamplerType (tcu::TextureFormat format)
{
	using tcu::TextureFormat;

	switch (format.type)
	{
		case TextureFormat::SIGNED_INT8:
		case TextureFormat::SIGNED_INT16:
		case TextureFormat::SIGNED_INT32:
			return SAMPLERTYPE_FETCH_INT;

		case TextureFormat::UNSIGNED_INT8:
		case TextureFormat::UNSIGNED_INT32:
		case TextureFormat::UNSIGNED_INT_1010102_REV:
			return SAMPLERTYPE_FETCH_UINT;

		// Texture formats used in depth/stencil textures.
		case TextureFormat::UNSIGNED_INT16:
		case TextureFormat::UNSIGNED_INT_24_8:
			return (format.order == TextureFormat::D || format.order == TextureFormat::DS) ? SAMPLERTYPE_FETCH_FLOAT : SAMPLERTYPE_FETCH_UINT;

		default:
			return SAMPLERTYPE_FETCH_FLOAT;
	}
}

static tcu::Texture1DView getSubView (const tcu::Texture1DView& view, int baseLevel, int maxLevel)
{
	const int	clampedBase	= de::clamp(baseLevel, 0, view.getNumLevels()-1);
	const int	clampedMax	= de::clamp(maxLevel, clampedBase, view.getNumLevels()-1);
	const int	numLevels	= clampedMax-clampedBase+1;
	return tcu::Texture1DView(numLevels, view.getLevels()+clampedBase);
}

static tcu::Texture2DView getSubView (const tcu::Texture2DView& view, int baseLevel, int maxLevel)
{
	const int	clampedBase	= de::clamp(baseLevel, 0, view.getNumLevels()-1);
	const int	clampedMax	= de::clamp(maxLevel, clampedBase, view.getNumLevels()-1);
	const int	numLevels	= clampedMax-clampedBase+1;
	return tcu::Texture2DView(numLevels, view.getLevels()+clampedBase);
}

static tcu::TextureCubeView getSubView (const tcu::TextureCubeView& view, int baseLevel, int maxLevel)
{
	const int							clampedBase	= de::clamp(baseLevel, 0, view.getNumLevels()-1);
	const int							clampedMax	= de::clamp(maxLevel, clampedBase, view.getNumLevels()-1);
	const int							numLevels	= clampedMax-clampedBase+1;
	const tcu::ConstPixelBufferAccess*	levels[tcu::CUBEFACE_LAST];

	for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
		levels[face] = view.getFaceLevels((tcu::CubeFace)face) + clampedBase;

	return tcu::TextureCubeView(numLevels, levels);
}

static tcu::Texture3DView getSubView (const tcu::Texture3DView& view, int baseLevel, int maxLevel)
{
	const int	clampedBase	= de::clamp(baseLevel, 0, view.getNumLevels()-1);
	const int	clampedMax	= de::clamp(maxLevel, clampedBase, view.getNumLevels()-1);
	const int	numLevels	= clampedMax-clampedBase+1;
	return tcu::Texture3DView(numLevels, view.getLevels()+clampedBase);
}

static tcu::TextureCubeArrayView getSubView (const tcu::TextureCubeArrayView& view, int baseLevel, int maxLevel)
{
	const int	clampedBase	= de::clamp(baseLevel, 0, view.getNumLevels()-1);
	const int	clampedMax	= de::clamp(maxLevel, clampedBase, view.getNumLevels()-1);
	const int	numLevels	= clampedMax-clampedBase+1;
	return tcu::TextureCubeArrayView(numLevels, view.getLevels()+clampedBase);
}

inline float linearInterpolate (float t, float minVal, float maxVal)
{
	return minVal + (maxVal - minVal) * t;
}

inline tcu::Vec4 linearInterpolate (float t, const tcu::Vec4& a, const tcu::Vec4& b)
{
	return a + (b - a) * t;
}

inline float bilinearInterpolate (float x, float y, const tcu::Vec4& quad)
{
	float w00 = (1.0f-x)*(1.0f-y);
	float w01 = (1.0f-x)*y;
	float w10 = x*(1.0f-y);
	float w11 = x*y;
	return quad.x()*w00 + quad.y()*w10 + quad.z()*w01 + quad.w()*w11;
}

inline float triangleInterpolate (float v0, float v1, float v2, float x, float y)
{
	return v0 + (v2-v0)*x + (v1-v0)*y;
}

inline float triangleInterpolate (const tcu::Vec3& v, float x, float y)
{
	return triangleInterpolate(v.x(), v.y(), v.z(), x, y);
}

SurfaceAccess::SurfaceAccess (tcu::Surface& surface, const tcu::PixelFormat& colorFmt, int x, int y, int width, int height)
	: m_surface		(&surface)
	, m_colorMask	(getColorMask(colorFmt))
	, m_x			(x)
	, m_y			(y)
	, m_width		(width)
	, m_height		(height)
{
}

SurfaceAccess::SurfaceAccess (tcu::Surface& surface, const tcu::PixelFormat& colorFmt)
	: m_surface		(&surface)
	, m_colorMask	(getColorMask(colorFmt))
	, m_x			(0)
	, m_y			(0)
	, m_width		(surface.getWidth())
	, m_height		(surface.getHeight())
{
}

SurfaceAccess::SurfaceAccess (const SurfaceAccess& parent, int x, int y, int width, int height)
	: m_surface			(parent.m_surface)
	, m_colorMask		(parent.m_colorMask)
	, m_x				(parent.m_x + x)
	, m_y				(parent.m_y + y)
	, m_width			(width)
	, m_height			(height)
{
}

// 1D lookup LOD computation.

float computeLodFromDerivates (LodMode mode, float dudx, float dudy)
{
	float p = 0.0f;
	switch (mode)
	{
		// \note [mika] Min and max bounds equal to exact with 1D textures
		case LODMODE_EXACT:
		case LODMODE_MIN_BOUND:
		case LODMODE_MAX_BOUND:
			p = de::max(deFloatAbs(dudx), deFloatAbs(dudy));
			break;

		default:
			DE_ASSERT(DE_FALSE);
	}

	return deFloatLog2(p);
}

static float computeNonProjectedTriLod (LodMode mode, const tcu::IVec2& dstSize, deInt32 srcSize, const tcu::Vec3& sq)
{
	float dux	= (sq.z() - sq.x()) * (float)srcSize;
	float duy	= (sq.y() - sq.x()) * (float)srcSize;
	float dx	= (float)dstSize.x();
	float dy	= (float)dstSize.y();

	return computeLodFromDerivates(mode, dux/dx, duy/dy);
}

// 2D lookup LOD computation.

float computeLodFromDerivates (LodMode mode, float dudx, float dvdx, float dudy, float dvdy)
{
	float p = 0.0f;
	switch (mode)
	{
		case LODMODE_EXACT:
			p = de::max(deFloatSqrt(dudx*dudx + dvdx*dvdx), deFloatSqrt(dudy*dudy + dvdy*dvdy));
			break;

		case LODMODE_MIN_BOUND:
		case LODMODE_MAX_BOUND:
		{
			float mu = de::max(deFloatAbs(dudx), deFloatAbs(dudy));
			float mv = de::max(deFloatAbs(dvdx), deFloatAbs(dvdy));

			p = mode == LODMODE_MIN_BOUND ? de::max(mu, mv) : mu + mv;
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
	}

	return deFloatLog2(p);
}

static float computeNonProjectedTriLod (LodMode mode, const tcu::IVec2& dstSize, const tcu::IVec2& srcSize, const tcu::Vec3& sq, const tcu::Vec3& tq)
{
	float dux	= (sq.z() - sq.x()) * (float)srcSize.x();
	float duy	= (sq.y() - sq.x()) * (float)srcSize.x();
	float dvx	= (tq.z() - tq.x()) * (float)srcSize.y();
	float dvy	= (tq.y() - tq.x()) * (float)srcSize.y();
	float dx	= (float)dstSize.x();
	float dy	= (float)dstSize.y();

	return computeLodFromDerivates(mode, dux/dx, dvx/dx, duy/dy, dvy/dy);
}

// 3D lookup LOD computation.

float computeLodFromDerivates (LodMode mode, float dudx, float dvdx, float dwdx, float dudy, float dvdy, float dwdy)
{
	float p = 0.0f;
	switch (mode)
	{
		case LODMODE_EXACT:
			p = de::max(deFloatSqrt(dudx*dudx + dvdx*dvdx + dwdx*dwdx), deFloatSqrt(dudy*dudy + dvdy*dvdy + dwdy*dwdy));
			break;

		case LODMODE_MIN_BOUND:
		case LODMODE_MAX_BOUND:
		{
			float mu = de::max(deFloatAbs(dudx), deFloatAbs(dudy));
			float mv = de::max(deFloatAbs(dvdx), deFloatAbs(dvdy));
			float mw = de::max(deFloatAbs(dwdx), deFloatAbs(dwdy));

			p = mode == LODMODE_MIN_BOUND ? de::max(de::max(mu, mv), mw) : (mu + mv + mw);
			break;
		}

		default:
			DE_ASSERT(DE_FALSE);
	}

	return deFloatLog2(p);
}

static float computeNonProjectedTriLod (LodMode mode, const tcu::IVec2& dstSize, const tcu::IVec3& srcSize, const tcu::Vec3& sq, const tcu::Vec3& tq, const tcu::Vec3& rq)
{
	float dux	= (sq.z() - sq.x()) * (float)srcSize.x();
	float duy	= (sq.y() - sq.x()) * (float)srcSize.x();
	float dvx	= (tq.z() - tq.x()) * (float)srcSize.y();
	float dvy	= (tq.y() - tq.x()) * (float)srcSize.y();
	float dwx	= (rq.z() - rq.x()) * (float)srcSize.z();
	float dwy	= (rq.y() - rq.x()) * (float)srcSize.z();
	float dx	= (float)dstSize.x();
	float dy	= (float)dstSize.y();

	return computeLodFromDerivates(mode, dux/dx, dvx/dx, dwx/dx, duy/dy, dvy/dy, dwy/dy);
}

static inline float projectedTriInterpolate (const tcu::Vec3& s, const tcu::Vec3& w, float nx, float ny)
{
	return (s[0]*(1.0f-nx-ny)/w[0] + s[1]*ny/w[1] + s[2]*nx/w[2]) / ((1.0f-nx-ny)/w[0] + ny/w[1] + nx/w[2]);
}

static inline float triDerivateX (const tcu::Vec3& s, const tcu::Vec3& w, float wx, float width, float ny)
{
	float d = w[1]*w[2]*(width*(ny - 1.0f) + wx) - w[0]*(w[2]*width*ny + w[1]*wx);
	return (w[0]*w[1]*w[2]*width * (w[1]*(s[0] - s[2])*(ny - 1.0f) + ny*(w[2]*(s[1] - s[0]) + w[0]*(s[2] - s[1])))) / (d*d);
}

static inline float triDerivateY (const tcu::Vec3& s, const tcu::Vec3& w, float wy, float height, float nx)
{
	float d = w[1]*w[2]*(height*(nx - 1.0f) + wy) - w[0]*(w[1]*height*nx + w[2]*wy);
	return (w[0]*w[1]*w[2]*height * (w[2]*(s[0] - s[1])*(nx - 1.0f) + nx*(w[0]*(s[1] - s[2]) + w[1]*(s[2] - s[0])))) / (d*d);
}

// 1D lookup LOD.
static float computeProjectedTriLod (LodMode mode, const tcu::Vec3& u, const tcu::Vec3& projection, float wx, float wy, float width, float height)
{
	// Exact derivatives.
	float dudx	= triDerivateX(u, projection, wx, width, wy/height);
	float dudy	= triDerivateY(u, projection, wy, height, wx/width);

	return computeLodFromDerivates(mode, dudx, dudy);
}

// 2D lookup LOD.
static float computeProjectedTriLod (LodMode mode, const tcu::Vec3& u, const tcu::Vec3& v, const tcu::Vec3& projection, float wx, float wy, float width, float height)
{
	// Exact derivatives.
	float dudx	= triDerivateX(u, projection, wx, width, wy/height);
	float dvdx	= triDerivateX(v, projection, wx, width, wy/height);
	float dudy	= triDerivateY(u, projection, wy, height, wx/width);
	float dvdy	= triDerivateY(v, projection, wy, height, wx/width);

	return computeLodFromDerivates(mode, dudx, dvdx, dudy, dvdy);
}

// 3D lookup LOD.
static float computeProjectedTriLod (LodMode mode, const tcu::Vec3& u, const tcu::Vec3& v, const tcu::Vec3& w, const tcu::Vec3& projection, float wx, float wy, float width, float height)
{
	// Exact derivatives.
	float dudx	= triDerivateX(u, projection, wx, width, wy/height);
	float dvdx	= triDerivateX(v, projection, wx, width, wy/height);
	float dwdx	= triDerivateX(w, projection, wx, width, wy/height);
	float dudy	= triDerivateY(u, projection, wy, height, wx/width);
	float dvdy	= triDerivateY(v, projection, wy, height, wx/width);
	float dwdy	= triDerivateY(w, projection, wy, height, wx/width);

	return computeLodFromDerivates(mode, dudx, dvdx, dwdx, dudy, dvdy, dwdy);
}

static inline tcu::Vec4 execSample (const tcu::Texture1DView& src, const ReferenceParams& params, float s, float lod)
{
	if (params.samplerType == SAMPLERTYPE_SHADOW)
		return tcu::Vec4(src.sampleCompare(params.sampler, params.ref, s, lod), 0.0, 0.0, 1.0f);
	else
		return src.sample(params.sampler, s, lod);
}

static inline tcu::Vec4 execSample (const tcu::Texture2DView& src, const ReferenceParams& params, float s, float t, float lod)
{
	if (params.samplerType == SAMPLERTYPE_SHADOW)
		return tcu::Vec4(src.sampleCompare(params.sampler, params.ref, s, t, lod), 0.0, 0.0, 1.0f);
	else
		return src.sample(params.sampler, s, t, lod);
}

static inline tcu::Vec4 execSample (const tcu::TextureCubeView& src, const ReferenceParams& params, float s, float t, float r, float lod)
{
	if (params.samplerType == SAMPLERTYPE_SHADOW)
		return tcu::Vec4(src.sampleCompare(params.sampler, params.ref, s, t, r, lod), 0.0, 0.0, 1.0f);
	else
		return src.sample(params.sampler, s, t, r, lod);
}

static inline tcu::Vec4 execSample (const tcu::Texture2DArrayView& src, const ReferenceParams& params, float s, float t, float r, float lod)
{
	if (params.samplerType == SAMPLERTYPE_SHADOW)
		return tcu::Vec4(src.sampleCompare(params.sampler, params.ref, s, t, r, lod), 0.0, 0.0, 1.0f);
	else
		return src.sample(params.sampler, s, t, r, lod);
}

static inline tcu::Vec4 execSample (const tcu::TextureCubeArrayView& src, const ReferenceParams& params, float s, float t, float r, float q, float lod)
{
	if (params.samplerType == SAMPLERTYPE_SHADOW)
		return tcu::Vec4(src.sampleCompare(params.sampler, params.ref, s, t, r, q, lod), 0.0, 0.0, 1.0f);
	else
		return src.sample(params.sampler, s, t, r, q, lod);
}

static inline tcu::Vec4 execSample (const tcu::Texture1DArrayView& src, const ReferenceParams& params, float s, float t, float lod)
{
	if (params.samplerType == SAMPLERTYPE_SHADOW)
		return tcu::Vec4(src.sampleCompare(params.sampler, params.ref, s, t, lod), 0.0, 0.0, 1.0f);
	else
		return src.sample(params.sampler, s, t, lod);
}

static void sampleTextureNonProjected (const SurfaceAccess& dst, const tcu::Texture1DView& rawSrc, const tcu::Vec4& sq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture1DView					src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	float										lodBias				= (params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f;

	tcu::IVec2									dstSize				= tcu::IVec2(dst.getWidth(), dst.getHeight());
	int											srcSize				= src.getWidth();

	// Coordinates and lod per triangle.
	tcu::Vec3									triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	float										triLod[2]			= { de::clamp(computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[0]) + lodBias, params.minLod, params.maxLod),
																		de::clamp(computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[1]) + lodBias, params.minLod, params.maxLod) };

	for (int y = 0; y < dst.getHeight(); y++)
	{
		for (int x = 0; x < dst.getWidth(); x++)
		{
			float	yf		= ((float)y + 0.5f) / (float)dst.getHeight();
			float	xf		= ((float)x + 0.5f) / (float)dst.getWidth();

			int		triNdx	= xf + yf >= 1.0f ? 1 : 0; // Top left fill rule.
			float	triX	= triNdx ? 1.0f-xf : xf;
			float	triY	= triNdx ? 1.0f-yf : yf;

			float	s		= triangleInterpolate(triS[triNdx].x(), triS[triNdx].y(), triS[triNdx].z(), triX, triY);
			float	lod		= triLod[triNdx];

			dst.setPixel(execSample(src, params, s, lod) * params.colorScale + params.colorBias, x, y);
		}
	}
}

static void sampleTextureNonProjected (const SurfaceAccess& dst, const tcu::Texture2DView& rawSrc, const tcu::Vec4& sq, const tcu::Vec4& tq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture2DView					src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	float										lodBias				= (params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f;

	tcu::IVec2									dstSize				= tcu::IVec2(dst.getWidth(), dst.getHeight());
	tcu::IVec2									srcSize				= tcu::IVec2(src.getWidth(), src.getHeight());

	// Coordinates and lod per triangle.
	tcu::Vec3									triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	tcu::Vec3									triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	float										triLod[2]			= { de::clamp(computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[0], triT[0]) + lodBias, params.minLod, params.maxLod),
																		de::clamp(computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[1], triT[1]) + lodBias, params.minLod, params.maxLod) };

	for (int y = 0; y < dst.getHeight(); y++)
	{
		for (int x = 0; x < dst.getWidth(); x++)
		{
			float	yf		= ((float)y + 0.5f) / (float)dst.getHeight();
			float	xf		= ((float)x + 0.5f) / (float)dst.getWidth();

			int		triNdx	= xf + yf >= 1.0f ? 1 : 0; // Top left fill rule.
			float	triX	= triNdx ? 1.0f-xf : xf;
			float	triY	= triNdx ? 1.0f-yf : yf;

			float	s		= triangleInterpolate(triS[triNdx].x(), triS[triNdx].y(), triS[triNdx].z(), triX, triY);
			float	t		= triangleInterpolate(triT[triNdx].x(), triT[triNdx].y(), triT[triNdx].z(), triX, triY);
			float	lod		= triLod[triNdx];

			dst.setPixel(execSample(src, params, s, t, lod) * params.colorScale + params.colorBias, x, y);
		}
	}
}

static void sampleTextureProjected (const SurfaceAccess& dst, const tcu::Texture1DView& rawSrc, const tcu::Vec4& sq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture1DView					src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	float										lodBias				= (params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f;
	float										dstW				= (float)dst.getWidth();
	float										dstH				= (float)dst.getHeight();

	tcu::Vec4									uq					= sq * (float)src.getWidth();

	tcu::Vec3									triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	tcu::Vec3									triU[2]				= { uq.swizzle(0, 1, 2), uq.swizzle(3, 2, 1) };
	tcu::Vec3									triW[2]				= { params.w.swizzle(0, 1, 2), params.w.swizzle(3, 2, 1) };

	for (int py = 0; py < dst.getHeight(); py++)
	{
		for (int px = 0; px < dst.getWidth(); px++)
		{
			float	wx		= (float)px + 0.5f;
			float	wy		= (float)py + 0.5f;
			float	nx		= wx / dstW;
			float	ny		= wy / dstH;

			int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
			float	triWx	= triNdx ? dstW - wx : wx;
			float	triWy	= triNdx ? dstH - wy : wy;
			float	triNx	= triNdx ? 1.0f - nx : nx;
			float	triNy	= triNdx ? 1.0f - ny : ny;

			float	s		= projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy);
			float	lod		= computeProjectedTriLod(params.lodMode, triU[triNdx], triW[triNdx], triWx, triWy, (float)dst.getWidth(), (float)dst.getHeight())
							+ lodBias;

			dst.setPixel(execSample(src, params, s, lod) * params.colorScale + params.colorBias, px, py);
		}
	}
}

static void sampleTextureProjected (const SurfaceAccess& dst, const tcu::Texture2DView& rawSrc, const tcu::Vec4& sq, const tcu::Vec4& tq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture2DView					src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	float										lodBias				= (params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f;
	float										dstW				= (float)dst.getWidth();
	float										dstH				= (float)dst.getHeight();

	tcu::Vec4									uq					= sq * (float)src.getWidth();
	tcu::Vec4									vq					= tq * (float)src.getHeight();

	tcu::Vec3									triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	tcu::Vec3									triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	tcu::Vec3									triU[2]				= { uq.swizzle(0, 1, 2), uq.swizzle(3, 2, 1) };
	tcu::Vec3									triV[2]				= { vq.swizzle(0, 1, 2), vq.swizzle(3, 2, 1) };
	tcu::Vec3									triW[2]				= { params.w.swizzle(0, 1, 2), params.w.swizzle(3, 2, 1) };

	for (int py = 0; py < dst.getHeight(); py++)
	{
		for (int px = 0; px < dst.getWidth(); px++)
		{
			float	wx		= (float)px + 0.5f;
			float	wy		= (float)py + 0.5f;
			float	nx		= wx / dstW;
			float	ny		= wy / dstH;

			int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
			float	triWx	= triNdx ? dstW - wx : wx;
			float	triWy	= triNdx ? dstH - wy : wy;
			float	triNx	= triNdx ? 1.0f - nx : nx;
			float	triNy	= triNdx ? 1.0f - ny : ny;

			float	s		= projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy);
			float	t		= projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy);
			float	lod		= computeProjectedTriLod(params.lodMode, triU[triNdx], triV[triNdx], triW[triNdx], triWx, triWy, (float)dst.getWidth(), (float)dst.getHeight())
							+ lodBias;

			dst.setPixel(execSample(src, params, s, t, lod) * params.colorScale + params.colorBias, px, py);
		}
	}
}

void sampleTexture (const SurfaceAccess& dst, const tcu::Texture2DView& src, const float* texCoord, const ReferenceParams& params)
{
	const tcu::Texture2DView	view	= getSubView(src, params.baseLevel, params.maxLevel);
	const tcu::Vec4				sq		= tcu::Vec4(texCoord[0+0], texCoord[2+0], texCoord[4+0], texCoord[6+0]);
	const tcu::Vec4				tq		= tcu::Vec4(texCoord[0+1], texCoord[2+1], texCoord[4+1], texCoord[6+1]);

	if (params.flags & ReferenceParams::PROJECTED)
		sampleTextureProjected(dst, view, sq, tq, params);
	else
		sampleTextureNonProjected(dst, view, sq, tq, params);
}

void sampleTexture (const SurfaceAccess& dst, const tcu::Texture1DView& src, const float* texCoord, const ReferenceParams& params)
{
	const tcu::Texture1DView	view	= getSubView(src, params.baseLevel, params.maxLevel);
	const tcu::Vec4				sq		= tcu::Vec4(texCoord[0], texCoord[1], texCoord[2], texCoord[3]);

	if (params.flags & ReferenceParams::PROJECTED)
		sampleTextureProjected(dst, view, sq, params);
	else
		sampleTextureNonProjected(dst, view, sq, params);
}

static float computeCubeLodFromDerivates (LodMode lodMode, const tcu::Vec3& coord, const tcu::Vec3& coordDx, const tcu::Vec3& coordDy, const int faceSize)
{
	const tcu::CubeFace	face	= tcu::selectCubeFace(coord);
	int					maNdx	= 0;
	int					sNdx	= 0;
	int					tNdx	= 0;

	// \note Derivate signs don't matter when computing lod
	switch (face)
	{
		case tcu::CUBEFACE_NEGATIVE_X:
		case tcu::CUBEFACE_POSITIVE_X: maNdx = 0; sNdx = 2; tNdx = 1; break;
		case tcu::CUBEFACE_NEGATIVE_Y:
		case tcu::CUBEFACE_POSITIVE_Y: maNdx = 1; sNdx = 0; tNdx = 2; break;
		case tcu::CUBEFACE_NEGATIVE_Z:
		case tcu::CUBEFACE_POSITIVE_Z: maNdx = 2; sNdx = 0; tNdx = 1; break;
		default:
			DE_ASSERT(DE_FALSE);
	}

	{
		const float		sc		= coord[sNdx];
		const float		tc		= coord[tNdx];
		const float		ma		= de::abs(coord[maNdx]);
		const float		scdx	= coordDx[sNdx];
		const float		tcdx	= coordDx[tNdx];
		const float		madx	= de::abs(coordDx[maNdx]);
		const float		scdy	= coordDy[sNdx];
		const float		tcdy	= coordDy[tNdx];
		const float		mady	= de::abs(coordDy[maNdx]);
		const float		dudx	= float(faceSize) * 0.5f * (scdx*ma - sc*madx) / (ma*ma);
		const float		dvdx	= float(faceSize) * 0.5f * (tcdx*ma - tc*madx) / (ma*ma);
		const float		dudy	= float(faceSize) * 0.5f * (scdy*ma - sc*mady) / (ma*ma);
		const float		dvdy	= float(faceSize) * 0.5f * (tcdy*ma - tc*mady) / (ma*ma);

		return computeLodFromDerivates(lodMode, dudx, dvdx, dudy, dvdy);
	}
}

static void sampleTextureCube (const SurfaceAccess& dst, const tcu::TextureCubeView& rawSrc, const tcu::Vec4& sq, const tcu::Vec4& tq, const tcu::Vec4& rq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::TextureCubeView					src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	const tcu::IVec2							dstSize				= tcu::IVec2(dst.getWidth(), dst.getHeight());
	const float									dstW				= float(dstSize.x());
	const float									dstH				= float(dstSize.y());
	const int									srcSize				= src.getSize();

	// Coordinates per triangle.
	const tcu::Vec3								triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3								triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3								triR[2]				= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	const tcu::Vec3								triW[2]				= { params.w.swizzle(0, 1, 2), params.w.swizzle(3, 2, 1) };

	const float									lodBias				((params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f);

	for (int py = 0; py < dst.getHeight(); py++)
	{
		for (int px = 0; px < dst.getWidth(); px++)
		{
			const float		wx		= (float)px + 0.5f;
			const float		wy		= (float)py + 0.5f;
			const float		nx		= wx / dstW;
			const float		ny		= wy / dstH;

			const int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
			const float		triNx	= triNdx ? 1.0f - nx : nx;
			const float		triNy	= triNdx ? 1.0f - ny : ny;

			const tcu::Vec3	coord		(triangleInterpolate(triS[triNdx], triNx, triNy),
										 triangleInterpolate(triT[triNdx], triNx, triNy),
										 triangleInterpolate(triR[triNdx], triNx, triNy));
			const tcu::Vec3	coordDx		(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
										 triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy),
										 triDerivateX(triR[triNdx], triW[triNdx], wx, dstW, triNy));
			const tcu::Vec3	coordDy		(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
										 triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx),
										 triDerivateY(triR[triNdx], triW[triNdx], wy, dstH, triNx));

			const float		lod			= de::clamp(computeCubeLodFromDerivates(params.lodMode, coord, coordDx, coordDy, srcSize) + lodBias, params.minLod, params.maxLod);

			dst.setPixel(execSample(src, params, coord.x(), coord.y(), coord.z(), lod) * params.colorScale + params.colorBias, px, py);
		}
	}
}

void sampleTexture (const SurfaceAccess& dst, const tcu::TextureCubeView& src, const float* texCoord, const ReferenceParams& params)
{
	const tcu::TextureCubeView	view	= getSubView(src, params.baseLevel, params.maxLevel);
	const tcu::Vec4				sq		= tcu::Vec4(texCoord[0+0], texCoord[3+0], texCoord[6+0], texCoord[9+0]);
	const tcu::Vec4				tq		= tcu::Vec4(texCoord[0+1], texCoord[3+1], texCoord[6+1], texCoord[9+1]);
	const tcu::Vec4				rq		= tcu::Vec4(texCoord[0+2], texCoord[3+2], texCoord[6+2], texCoord[9+2]);

	return sampleTextureCube(dst, view, sq, tq, rq, params);
}

static void sampleTextureNonProjected (const SurfaceAccess& dst, const tcu::Texture2DArrayView& rawSrc, const tcu::Vec4& sq, const tcu::Vec4& tq, const tcu::Vec4& rq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture2DArrayView				src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	float										lodBias				= (params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f;

	tcu::IVec2									dstSize				= tcu::IVec2(dst.getWidth(), dst.getHeight());
	tcu::IVec2									srcSize				= tcu::IVec2(src.getWidth(), src.getHeight());

	// Coordinates and lod per triangle.
	tcu::Vec3									triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	tcu::Vec3									triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	tcu::Vec3									triR[2]				= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	float										triLod[2]			= { computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[0], triT[0]) + lodBias,
																		computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[1], triT[1]) + lodBias};

	for (int y = 0; y < dst.getHeight(); y++)
	{
		for (int x = 0; x < dst.getWidth(); x++)
		{
			float	yf		= ((float)y + 0.5f) / (float)dst.getHeight();
			float	xf		= ((float)x + 0.5f) / (float)dst.getWidth();

			int		triNdx	= xf + yf >= 1.0f ? 1 : 0; // Top left fill rule.
			float	triX	= triNdx ? 1.0f-xf : xf;
			float	triY	= triNdx ? 1.0f-yf : yf;

			float	s		= triangleInterpolate(triS[triNdx].x(), triS[triNdx].y(), triS[triNdx].z(), triX, triY);
			float	t		= triangleInterpolate(triT[triNdx].x(), triT[triNdx].y(), triT[triNdx].z(), triX, triY);
			float	r		= triangleInterpolate(triR[triNdx].x(), triR[triNdx].y(), triR[triNdx].z(), triX, triY);
			float	lod		= triLod[triNdx];

			dst.setPixel(execSample(src, params, s, t, r, lod) * params.colorScale + params.colorBias, x, y);
		}
	}
}

void sampleTexture (const SurfaceAccess& dst, const tcu::Texture2DArrayView& src, const float* texCoord, const ReferenceParams& params)
{
	tcu::Vec4 sq = tcu::Vec4(texCoord[0+0], texCoord[3+0], texCoord[6+0], texCoord[9+0]);
	tcu::Vec4 tq = tcu::Vec4(texCoord[0+1], texCoord[3+1], texCoord[6+1], texCoord[9+1]);
	tcu::Vec4 rq = tcu::Vec4(texCoord[0+2], texCoord[3+2], texCoord[6+2], texCoord[9+2]);

	DE_ASSERT(!(params.flags & ReferenceParams::PROJECTED)); // \todo [2012-02-17 pyry] Support projected lookups.
	sampleTextureNonProjected(dst, src, sq, tq, rq, params);
}

static void sampleTextureNonProjected (const SurfaceAccess& dst, const tcu::Texture1DArrayView& rawSrc, const tcu::Vec4& sq, const tcu::Vec4& tq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture1DArrayView				src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	float										lodBias				= (params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f;

	tcu::IVec2									dstSize				= tcu::IVec2(dst.getWidth(), dst.getHeight());
	deInt32										srcSize				= src.getWidth();

	// Coordinates and lod per triangle.
	tcu::Vec3									triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	tcu::Vec3									triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	float										triLod[2]			= { computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[0]) + lodBias,
																		computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[1]) + lodBias};

	for (int y = 0; y < dst.getHeight(); y++)
	{
		for (int x = 0; x < dst.getWidth(); x++)
		{
			float	yf		= ((float)y + 0.5f) / (float)dst.getHeight();
			float	xf		= ((float)x + 0.5f) / (float)dst.getWidth();

			int		triNdx	= xf + yf >= 1.0f ? 1 : 0; // Top left fill rule.
			float	triX	= triNdx ? 1.0f-xf : xf;
			float	triY	= triNdx ? 1.0f-yf : yf;

			float	s		= triangleInterpolate(triS[triNdx].x(), triS[triNdx].y(), triS[triNdx].z(), triX, triY);
			float	t		= triangleInterpolate(triT[triNdx].x(), triT[triNdx].y(), triT[triNdx].z(), triX, triY);
			float	lod		= triLod[triNdx];

			dst.setPixel(execSample(src, params, s, t, lod) * params.colorScale + params.colorBias, x, y);
		}
	}
}

void sampleTexture (const SurfaceAccess& dst, const tcu::Texture1DArrayView& src, const float* texCoord, const ReferenceParams& params)
{
	tcu::Vec4 sq = tcu::Vec4(texCoord[0+0], texCoord[2+0], texCoord[4+0], texCoord[6+0]);
	tcu::Vec4 tq = tcu::Vec4(texCoord[0+1], texCoord[2+1], texCoord[4+1], texCoord[6+1]);

	DE_ASSERT(!(params.flags & ReferenceParams::PROJECTED)); // \todo [2014-06-09 mika] Support projected lookups.
	sampleTextureNonProjected(dst, src, sq, tq, params);
}

static void sampleTextureNonProjected (const SurfaceAccess& dst, const tcu::Texture3DView& rawSrc, const tcu::Vec4& sq, const tcu::Vec4& tq, const tcu::Vec4& rq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture3DView					src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	float										lodBias				= (params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f;

	tcu::IVec2									dstSize				= tcu::IVec2(dst.getWidth(), dst.getHeight());
	tcu::IVec3									srcSize				= tcu::IVec3(src.getWidth(), src.getHeight(), src.getDepth());

	// Coordinates and lod per triangle.
	tcu::Vec3									triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	tcu::Vec3									triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	tcu::Vec3									triR[2]				= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	float										triLod[2]			= { de::clamp(computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[0], triT[0], triR[0]) + lodBias, params.minLod, params.maxLod),
																		de::clamp(computeNonProjectedTriLod(params.lodMode, dstSize, srcSize, triS[1], triT[1], triR[1]) + lodBias, params.minLod, params.maxLod) };

	for (int y = 0; y < dst.getHeight(); y++)
	{
		for (int x = 0; x < dst.getWidth(); x++)
		{
			float	yf		= ((float)y + 0.5f) / (float)dst.getHeight();
			float	xf		= ((float)x + 0.5f) / (float)dst.getWidth();

			int		triNdx	= xf + yf >= 1.0f ? 1 : 0; // Top left fill rule.
			float	triX	= triNdx ? 1.0f-xf : xf;
			float	triY	= triNdx ? 1.0f-yf : yf;

			float	s		= triangleInterpolate(triS[triNdx].x(), triS[triNdx].y(), triS[triNdx].z(), triX, triY);
			float	t		= triangleInterpolate(triT[triNdx].x(), triT[triNdx].y(), triT[triNdx].z(), triX, triY);
			float	r		= triangleInterpolate(triR[triNdx].x(), triR[triNdx].y(), triR[triNdx].z(), triX, triY);
			float	lod		= triLod[triNdx];

			dst.setPixel(src.sample(params.sampler, s, t, r, lod) * params.colorScale + params.colorBias, x, y);
		}
	}
}

static void sampleTextureProjected (const SurfaceAccess& dst, const tcu::Texture3DView& rawSrc, const tcu::Vec4& sq, const tcu::Vec4& tq, const tcu::Vec4& rq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture3DView					src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	float										lodBias				= (params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f;
	float										dstW				= (float)dst.getWidth();
	float										dstH				= (float)dst.getHeight();

	tcu::Vec4									uq					= sq * (float)src.getWidth();
	tcu::Vec4									vq					= tq * (float)src.getHeight();
	tcu::Vec4									wq					= rq * (float)src.getDepth();

	tcu::Vec3									triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	tcu::Vec3									triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	tcu::Vec3									triR[2]				= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	tcu::Vec3									triU[2]				= { uq.swizzle(0, 1, 2), uq.swizzle(3, 2, 1) };
	tcu::Vec3									triV[2]				= { vq.swizzle(0, 1, 2), vq.swizzle(3, 2, 1) };
	tcu::Vec3									triW[2]				= { wq.swizzle(0, 1, 2), wq.swizzle(3, 2, 1) };
	tcu::Vec3									triP[2]				= { params.w.swizzle(0, 1, 2), params.w.swizzle(3, 2, 1) };

	for (int py = 0; py < dst.getHeight(); py++)
	{
		for (int px = 0; px < dst.getWidth(); px++)
		{
			float	wx		= (float)px + 0.5f;
			float	wy		= (float)py + 0.5f;
			float	nx		= wx / dstW;
			float	ny		= wy / dstH;

			int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
			float	triWx	= triNdx ? dstW - wx : wx;
			float	triWy	= triNdx ? dstH - wy : wy;
			float	triNx	= triNdx ? 1.0f - nx : nx;
			float	triNy	= triNdx ? 1.0f - ny : ny;

			float	s		= projectedTriInterpolate(triS[triNdx], triP[triNdx], triNx, triNy);
			float	t		= projectedTriInterpolate(triT[triNdx], triP[triNdx], triNx, triNy);
			float	r		= projectedTriInterpolate(triR[triNdx], triP[triNdx], triNx, triNy);
			float	lod		= computeProjectedTriLod(params.lodMode, triU[triNdx], triV[triNdx], triW[triNdx], triP[triNdx], triWx, triWy, (float)dst.getWidth(), (float)dst.getHeight())
							+ lodBias;

			dst.setPixel(src.sample(params.sampler, s, t, r, lod) * params.colorScale + params.colorBias, px, py);
		}
	}
}

void sampleTexture (const SurfaceAccess& dst, const tcu::Texture3DView& src, const float* texCoord, const ReferenceParams& params)
{
	const tcu::Texture3DView	view	= getSubView(src, params.baseLevel, params.maxLevel);
	const tcu::Vec4				sq		= tcu::Vec4(texCoord[0+0], texCoord[3+0], texCoord[6+0], texCoord[9+0]);
	const tcu::Vec4				tq		= tcu::Vec4(texCoord[0+1], texCoord[3+1], texCoord[6+1], texCoord[9+1]);
	const tcu::Vec4				rq		= tcu::Vec4(texCoord[0+2], texCoord[3+2], texCoord[6+2], texCoord[9+2]);

	if (params.flags & ReferenceParams::PROJECTED)
		sampleTextureProjected(dst, view, sq, tq, rq, params);
	else
		sampleTextureNonProjected(dst, view, sq, tq, rq, params);
}

static void sampleTextureCubeArray (const SurfaceAccess& dst, const tcu::TextureCubeArrayView& rawSrc, const tcu::Vec4& sq, const tcu::Vec4& tq, const tcu::Vec4& rq, const tcu::Vec4& qq, const ReferenceParams& params)
{
	// Separate combined DS formats
	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::TextureCubeArrayView				src					= getEffectiveTextureView(rawSrc, srcLevelStorage, params.sampler);

	const float									dstW				= (float)dst.getWidth();
	const float									dstH				= (float)dst.getHeight();

	// Coordinates per triangle.
	tcu::Vec3									triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	tcu::Vec3									triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	tcu::Vec3									triR[2]				= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	tcu::Vec3									triQ[2]				= { qq.swizzle(0, 1, 2), qq.swizzle(3, 2, 1) };
	const tcu::Vec3								triW[2]				= { params.w.swizzle(0, 1, 2), params.w.swizzle(3, 2, 1) };

	const float									lodBias				= (params.flags & ReferenceParams::USE_BIAS) ? params.bias : 0.0f;

	for (int py = 0; py < dst.getHeight(); py++)
	{
		for (int px = 0; px < dst.getWidth(); px++)
		{
			const float		wx		= (float)px + 0.5f;
			const float		wy		= (float)py + 0.5f;
			const float		nx		= wx / dstW;
			const float		ny		= wy / dstH;

			const int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
			const float		triNx	= triNdx ? 1.0f - nx : nx;
			const float		triNy	= triNdx ? 1.0f - ny : ny;

			const tcu::Vec3	coord	(triangleInterpolate(triS[triNdx], triNx, triNy),
									 triangleInterpolate(triT[triNdx], triNx, triNy),
									 triangleInterpolate(triR[triNdx], triNx, triNy));

			const float		coordQ	= triangleInterpolate(triQ[triNdx], triNx, triNy);

			const tcu::Vec3	coordDx	(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
									 triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy),
									 triDerivateX(triR[triNdx], triW[triNdx], wx, dstW, triNy));
			const tcu::Vec3	coordDy	(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
									 triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx),
									 triDerivateY(triR[triNdx], triW[triNdx], wy, dstH, triNx));

			const float		lod		= de::clamp(computeCubeLodFromDerivates(params.lodMode, coord, coordDx, coordDy, src.getSize()) + lodBias, params.minLod, params.maxLod);

			dst.setPixel(execSample(src, params, coord.x(), coord.y(), coord.z(), coordQ, lod) * params.colorScale + params.colorBias, px, py);
		}
	}
}

void sampleTexture (const SurfaceAccess& dst, const tcu::TextureCubeArrayView& src, const float* texCoord, const ReferenceParams& params)
{
	tcu::Vec4 sq = tcu::Vec4(texCoord[0+0], texCoord[4+0], texCoord[8+0], texCoord[12+0]);
	tcu::Vec4 tq = tcu::Vec4(texCoord[0+1], texCoord[4+1], texCoord[8+1], texCoord[12+1]);
	tcu::Vec4 rq = tcu::Vec4(texCoord[0+2], texCoord[4+2], texCoord[8+2], texCoord[12+2]);
	tcu::Vec4 qq = tcu::Vec4(texCoord[0+3], texCoord[4+3], texCoord[8+3], texCoord[12+3]);

	sampleTextureCubeArray(dst, src, sq, tq, rq, qq, params);
}

void fetchTexture (const SurfaceAccess& dst, const tcu::ConstPixelBufferAccess& src, const float* texCoord, const tcu::Vec4& colorScale, const tcu::Vec4& colorBias)
{
	const tcu::Vec4		sq			= tcu::Vec4(texCoord[0], texCoord[1], texCoord[2], texCoord[3]);
	const tcu::Vec3		triS[2]		= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };

	for (int y = 0; y < dst.getHeight(); y++)
	{
		for (int x = 0; x < dst.getWidth(); x++)
		{
			const float	yf		= ((float)y + 0.5f) / (float)dst.getHeight();
			const float	xf		= ((float)x + 0.5f) / (float)dst.getWidth();

			const int	triNdx	= xf + yf >= 1.0f ? 1 : 0; // Top left fill rule.
			const float	triX	= triNdx ? 1.0f-xf : xf;
			const float	triY	= triNdx ? 1.0f-yf : yf;

			const float	s		= triangleInterpolate(triS[triNdx].x(), triS[triNdx].y(), triS[triNdx].z(), triX, triY);

			dst.setPixel(src.getPixel((int)s, 0) * colorScale + colorBias, x, y);
		}
	}
}

bool compareImages (TestLog& log, const tcu::Surface& reference, const tcu::Surface& rendered, tcu::RGBA threshold)
{
	return tcu::pixelThresholdCompare(log, "Result", "Image comparison result", reference, rendered, threshold, tcu::COMPARE_LOG_RESULT);
}

bool compareImages (TestLog& log, const char* name, const char* desc, const tcu::Surface& reference, const tcu::Surface& rendered, tcu::RGBA threshold)
{
	return tcu::pixelThresholdCompare(log, name, desc, reference, rendered, threshold, tcu::COMPARE_LOG_RESULT);
}

int measureAccuracy (tcu::TestLog& log, const tcu::Surface& reference, const tcu::Surface& rendered, int bestScoreDiff, int worstScoreDiff)
{
	return tcu::measurePixelDiffAccuracy(log, "Result", "Image comparison result", reference, rendered, bestScoreDiff, worstScoreDiff, tcu::COMPARE_LOG_EVERYTHING);
}

inline int rangeDiff (int x, int a, int b)
{
	if (x < a)
		return a-x;
	else if (x > b)
		return x-b;
	else
		return 0;
}

inline tcu::RGBA rangeDiff (tcu::RGBA p, tcu::RGBA a, tcu::RGBA b)
{
	int rMin = de::min(a.getRed(),		b.getRed());
	int rMax = de::max(a.getRed(),		b.getRed());
	int gMin = de::min(a.getGreen(),	b.getGreen());
	int gMax = de::max(a.getGreen(),	b.getGreen());
	int bMin = de::min(a.getBlue(),		b.getBlue());
	int bMax = de::max(a.getBlue(),		b.getBlue());
	int aMin = de::min(a.getAlpha(),	b.getAlpha());
	int aMax = de::max(a.getAlpha(),	b.getAlpha());

	return tcu::RGBA(rangeDiff(p.getRed(),		rMin, rMax),
					 rangeDiff(p.getGreen(),	gMin, gMax),
					 rangeDiff(p.getBlue(),		bMin, bMax),
					 rangeDiff(p.getAlpha(),	aMin, aMax));
}

inline bool rangeCompare (tcu::RGBA p, tcu::RGBA a, tcu::RGBA b, tcu::RGBA threshold)
{
	tcu::RGBA diff = rangeDiff(p, a, b);
	return diff.getRed()	<= threshold.getRed() &&
		   diff.getGreen()	<= threshold.getGreen() &&
		   diff.getBlue()	<= threshold.getBlue() &&
		   diff.getAlpha()	<= threshold.getAlpha();
}

RandomViewport::RandomViewport (const tcu::RenderTarget& renderTarget, int preferredWidth, int preferredHeight, deUint32 seed)
	: x			(0)
	, y			(0)
	, width		(deMin32(preferredWidth, renderTarget.getWidth()))
	, height	(deMin32(preferredHeight, renderTarget.getHeight()))
{
	de::Random rnd(seed);
	x = rnd.getInt(0, renderTarget.getWidth()	- width);
	y = rnd.getInt(0, renderTarget.getHeight()	- height);
}

ProgramLibrary::ProgramLibrary (const glu::RenderContext& context, tcu::TestLog& log, glu::GLSLVersion glslVersion, glu::Precision texCoordPrecision)
	: m_context				(context)
	, m_log					(log)
	, m_glslVersion			(glslVersion)
	, m_texCoordPrecision	(texCoordPrecision)
{
}

ProgramLibrary::~ProgramLibrary (void)
{
	clear();
}

void ProgramLibrary::clear (void)
{
	for (map<Program, glu::ShaderProgram*>::iterator i = m_programs.begin(); i != m_programs.end(); i++)
	{
		delete i->second;
		i->second = DE_NULL;
	}
	m_programs.clear();
}

glu::ShaderProgram* ProgramLibrary::getProgram (Program program)
{
	if (m_programs.find(program) != m_programs.end())
		return m_programs[program]; // Return from cache.

	static const char* vertShaderTemplate =
		"${VTX_HEADER}"
		"${VTX_IN} highp vec4 a_position;\n"
		"${VTX_IN} ${PRECISION} ${TEXCOORD_TYPE} a_texCoord;\n"
		"${VTX_OUT} ${PRECISION} ${TEXCOORD_TYPE} v_texCoord;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = a_position;\n"
		"	v_texCoord = a_texCoord;\n"
		"}\n";
	static const char* fragShaderTemplate =
		"${FRAG_HEADER}"
		"${FRAG_IN} ${PRECISION} ${TEXCOORD_TYPE} v_texCoord;\n"
		"uniform ${PRECISION} float u_bias;\n"
		"uniform ${PRECISION} float u_ref;\n"
		"uniform ${PRECISION} vec4 u_colorScale;\n"
		"uniform ${PRECISION} vec4 u_colorBias;\n"
		"uniform ${PRECISION} ${SAMPLER_TYPE} u_sampler;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"	${FRAG_COLOR} = ${LOOKUP} * u_colorScale + u_colorBias;\n"
		"}\n";

	map<string, string> params;

	bool	isCube		= de::inRange<int>(program, PROGRAM_CUBE_FLOAT, PROGRAM_CUBE_SHADOW_BIAS);
	bool	isArray		= de::inRange<int>(program, PROGRAM_2D_ARRAY_FLOAT, PROGRAM_2D_ARRAY_SHADOW)
							|| de::inRange<int>(program, PROGRAM_1D_ARRAY_FLOAT, PROGRAM_1D_ARRAY_SHADOW);

	bool	is1D		= de::inRange<int>(program, PROGRAM_1D_FLOAT, PROGRAM_1D_UINT_BIAS)
							|| de::inRange<int>(program, PROGRAM_1D_ARRAY_FLOAT, PROGRAM_1D_ARRAY_SHADOW)
							|| de::inRange<int>(program, PROGRAM_BUFFER_FLOAT, PROGRAM_BUFFER_UINT);

	bool	is2D		= de::inRange<int>(program, PROGRAM_2D_FLOAT, PROGRAM_2D_UINT_BIAS)
							|| de::inRange<int>(program, PROGRAM_2D_ARRAY_FLOAT, PROGRAM_2D_ARRAY_SHADOW);

	bool	is3D		= de::inRange<int>(program, PROGRAM_3D_FLOAT, PROGRAM_3D_UINT_BIAS);
	bool	isCubeArray	= de::inRange<int>(program, PROGRAM_CUBE_ARRAY_FLOAT, PROGRAM_CUBE_ARRAY_SHADOW);
	bool	isBuffer	= de::inRange<int>(program, PROGRAM_BUFFER_FLOAT, PROGRAM_BUFFER_UINT);

	if (m_glslVersion == glu::GLSL_VERSION_100_ES)
	{
		params["FRAG_HEADER"]	= "";
		params["VTX_HEADER"]	= "";
		params["VTX_IN"]		= "attribute";
		params["VTX_OUT"]		= "varying";
		params["FRAG_IN"]		= "varying";
		params["FRAG_COLOR"]	= "gl_FragColor";
	}
	else if (m_glslVersion == glu::GLSL_VERSION_300_ES || m_glslVersion == glu::GLSL_VERSION_310_ES || m_glslVersion == glu::GLSL_VERSION_320_ES || m_glslVersion == glu::GLSL_VERSION_330)
	{
		const string	version	= glu::getGLSLVersionDeclaration(m_glslVersion);
		const char*		ext		= DE_NULL;

		if (glu::glslVersionIsES(m_glslVersion) && m_glslVersion != glu::GLSL_VERSION_320_ES) {
			if (isCubeArray)
				ext = "GL_EXT_texture_cube_map_array";
			else if (isBuffer)
				ext = "GL_EXT_texture_buffer";
		}

		params["FRAG_HEADER"]	= version + (ext ? string("\n#extension ") + ext + " : require" : string()) + "\nlayout(location = 0) out mediump vec4 dEQP_FragColor;\n";
		params["VTX_HEADER"]	= version + "\n";
		params["VTX_IN"]		= "in";
		params["VTX_OUT"]		= "out";
		params["FRAG_IN"]		= "in";
		params["FRAG_COLOR"]	= "dEQP_FragColor";
	}
	else
		DE_FATAL("Unsupported version");

	params["PRECISION"]		= glu::getPrecisionName(m_texCoordPrecision);

	if (isCubeArray)
		params["TEXCOORD_TYPE"]	= "vec4";
	else if (isCube || (is2D && isArray) || is3D)
		params["TEXCOORD_TYPE"]	= "vec3";
	else if ((is1D && isArray) || is2D)
		params["TEXCOORD_TYPE"]	= "vec2";
	else if (is1D)
		params["TEXCOORD_TYPE"]	= "float";
	else
		DE_ASSERT(DE_FALSE);

	const char*	sampler	= DE_NULL;
	const char*	lookup	= DE_NULL;

	if (m_glslVersion == glu::GLSL_VERSION_300_ES || m_glslVersion == glu::GLSL_VERSION_310_ES || m_glslVersion == glu::GLSL_VERSION_320_ES || m_glslVersion == glu::GLSL_VERSION_330)
	{
		switch (program)
		{
			case PROGRAM_2D_FLOAT:			sampler = "sampler2D";				lookup = "texture(u_sampler, v_texCoord)";												break;
			case PROGRAM_2D_INT:			sampler = "isampler2D";				lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_2D_UINT:			sampler = "usampler2D";				lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_2D_SHADOW:			sampler = "sampler2DShadow";		lookup = "vec4(texture(u_sampler, vec3(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";			break;
			case PROGRAM_2D_FLOAT_BIAS:		sampler = "sampler2D";				lookup = "texture(u_sampler, v_texCoord, u_bias)";										break;
			case PROGRAM_2D_INT_BIAS:		sampler = "isampler2D";				lookup = "vec4(texture(u_sampler, v_texCoord, u_bias))";								break;
			case PROGRAM_2D_UINT_BIAS:		sampler = "usampler2D";				lookup = "vec4(texture(u_sampler, v_texCoord, u_bias))";								break;
			case PROGRAM_2D_SHADOW_BIAS:	sampler = "sampler2DShadow";		lookup = "vec4(texture(u_sampler, vec3(v_texCoord, u_ref), u_bias), 0.0, 0.0, 1.0)";	break;
			case PROGRAM_1D_FLOAT:			sampler = "sampler1D";				lookup = "texture(u_sampler, v_texCoord)";												break;
			case PROGRAM_1D_INT:			sampler = "isampler1D";				lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_1D_UINT:			sampler = "usampler1D";				lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_1D_SHADOW:			sampler = "sampler1DShadow";		lookup = "vec4(texture(u_sampler, vec3(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";			break;
			case PROGRAM_1D_FLOAT_BIAS:		sampler = "sampler1D";				lookup = "texture(u_sampler, v_texCoord, u_bias)";										break;
			case PROGRAM_1D_INT_BIAS:		sampler = "isampler1D";				lookup = "vec4(texture(u_sampler, v_texCoord, u_bias))";								break;
			case PROGRAM_1D_UINT_BIAS:		sampler = "usampler1D";				lookup = "vec4(texture(u_sampler, v_texCoord, u_bias))";								break;
			case PROGRAM_1D_SHADOW_BIAS:	sampler = "sampler1DShadow";		lookup = "vec4(texture(u_sampler, vec3(v_texCoord, u_ref), u_bias), 0.0, 0.0, 1.0)";	break;
			case PROGRAM_CUBE_FLOAT:		sampler = "samplerCube";			lookup = "texture(u_sampler, v_texCoord)";												break;
			case PROGRAM_CUBE_INT:			sampler = "isamplerCube";			lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_CUBE_UINT:			sampler = "usamplerCube";			lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_CUBE_SHADOW:		sampler = "samplerCubeShadow";		lookup = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";			break;
			case PROGRAM_CUBE_FLOAT_BIAS:	sampler = "samplerCube";			lookup = "texture(u_sampler, v_texCoord, u_bias)";										break;
			case PROGRAM_CUBE_INT_BIAS:		sampler = "isamplerCube";			lookup = "vec4(texture(u_sampler, v_texCoord, u_bias))";								break;
			case PROGRAM_CUBE_UINT_BIAS:	sampler = "usamplerCube";			lookup = "vec4(texture(u_sampler, v_texCoord, u_bias))";								break;
			case PROGRAM_CUBE_SHADOW_BIAS:	sampler = "samplerCubeShadow";		lookup = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref), u_bias), 0.0, 0.0, 1.0)";	break;
			case PROGRAM_2D_ARRAY_FLOAT:	sampler = "sampler2DArray";			lookup = "texture(u_sampler, v_texCoord)";												break;
			case PROGRAM_2D_ARRAY_INT:		sampler = "isampler2DArray";		lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_2D_ARRAY_UINT:		sampler = "usampler2DArray";		lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_2D_ARRAY_SHADOW:	sampler = "sampler2DArrayShadow";	lookup = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";			break;
			case PROGRAM_3D_FLOAT:			sampler = "sampler3D";				lookup = "texture(u_sampler, v_texCoord)";												break;
			case PROGRAM_3D_INT:			sampler = "isampler3D";				lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_3D_UINT:			sampler = "usampler3D";				lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_3D_FLOAT_BIAS:		sampler = "sampler3D";				lookup = "texture(u_sampler, v_texCoord, u_bias)";										break;
			case PROGRAM_3D_INT_BIAS:		sampler = "isampler3D";				lookup = "vec4(texture(u_sampler, v_texCoord, u_bias))";								break;
			case PROGRAM_3D_UINT_BIAS:		sampler = "usampler3D";				lookup = "vec4(texture(u_sampler, v_texCoord, u_bias))";								break;
			case PROGRAM_CUBE_ARRAY_FLOAT:	sampler = "samplerCubeArray";		lookup = "texture(u_sampler, v_texCoord)";												break;
			case PROGRAM_CUBE_ARRAY_INT:	sampler = "isamplerCubeArray";		lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_CUBE_ARRAY_UINT:	sampler = "usamplerCubeArray";		lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_CUBE_ARRAY_SHADOW:	sampler = "samplerCubeArrayShadow";	lookup = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";			break;
			case PROGRAM_1D_ARRAY_FLOAT:	sampler = "sampler1DArray";			lookup = "texture(u_sampler, v_texCoord)";												break;
			case PROGRAM_1D_ARRAY_INT:		sampler = "isampler1DArray";		lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_1D_ARRAY_UINT:		sampler = "usampler1DArray";		lookup = "vec4(texture(u_sampler, v_texCoord))";										break;
			case PROGRAM_1D_ARRAY_SHADOW:	sampler = "sampler1DArrayShadow";	lookup = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";			break;
			case PROGRAM_BUFFER_FLOAT:		sampler = "samplerBuffer";			lookup = "texelFetch(u_sampler, int(v_texCoord))";										break;
			case PROGRAM_BUFFER_INT:		sampler = "isamplerBuffer";			lookup = "vec4(texelFetch(u_sampler, int(v_texCoord)))";								break;
			case PROGRAM_BUFFER_UINT:		sampler = "usamplerBuffer";			lookup = "vec4(texelFetch(u_sampler, int(v_texCoord)))";								break;
			default:
				DE_ASSERT(false);
		}
	}
	else if (m_glslVersion == glu::GLSL_VERSION_100_ES)
	{
		sampler = isCube ? "samplerCube" : "sampler2D";

		switch (program)
		{
			case PROGRAM_2D_FLOAT:			lookup = "texture2D(u_sampler, v_texCoord)";			break;
			case PROGRAM_2D_FLOAT_BIAS:		lookup = "texture2D(u_sampler, v_texCoord, u_bias)";	break;
			case PROGRAM_CUBE_FLOAT:		lookup = "textureCube(u_sampler, v_texCoord)";			break;
			case PROGRAM_CUBE_FLOAT_BIAS:	lookup = "textureCube(u_sampler, v_texCoord, u_bias)";	break;
			default:
				DE_ASSERT(false);
		}
	}
	else
		DE_FATAL("Unsupported version");

	params["SAMPLER_TYPE"]	= sampler;
	params["LOOKUP"]		= lookup;

	std::string vertSrc = tcu::StringTemplate(vertShaderTemplate).specialize(params);
	std::string fragSrc = tcu::StringTemplate(fragShaderTemplate).specialize(params);

	glu::ShaderProgram* progObj = new glu::ShaderProgram(m_context, glu::makeVtxFragSources(vertSrc, fragSrc));
	if (!progObj->isOk())
	{
		m_log << *progObj;
		delete progObj;
		TCU_FAIL("Failed to compile shader program");
	}

	try
	{
		m_programs[program] = progObj;
	}
	catch (...)
	{
		delete progObj;
		throw;
	}

	return progObj;
}

TextureRenderer::TextureRenderer (const glu::RenderContext& context, tcu::TestLog& log, glu::GLSLVersion glslVersion, glu::Precision texCoordPrecision)
	: m_renderCtx		(context)
	, m_log				(log)
	, m_programLibrary	(context, log, glslVersion, texCoordPrecision)
{
}

TextureRenderer::~TextureRenderer (void)
{
	clear();
}

void TextureRenderer::clear (void)
{
	m_programLibrary.clear();
}

void TextureRenderer::renderQuad (int texUnit, const float* texCoord, TextureType texType)
{
	renderQuad(texUnit, texCoord, RenderParams(texType));
}

void TextureRenderer::renderQuad (int texUnit, const float* texCoord, const RenderParams& params)
{
	const glw::Functions&	gl			= m_renderCtx.getFunctions();
	tcu::Vec4				wCoord		= params.flags & RenderParams::PROJECTED ? params.w : tcu::Vec4(1.0f);
	bool					useBias		= !!(params.flags & RenderParams::USE_BIAS);
	bool					logUniforms	= !!(params.flags & RenderParams::LOG_UNIFORMS);

	// Render quad with texture.
	float position[] =
	{
		-1.0f*wCoord.x(), -1.0f*wCoord.x(), 0.0f, wCoord.x(),
		-1.0f*wCoord.y(), +1.0f*wCoord.y(), 0.0f, wCoord.y(),
		+1.0f*wCoord.z(), -1.0f*wCoord.z(), 0.0f, wCoord.z(),
		+1.0f*wCoord.w(), +1.0f*wCoord.w(), 0.0f, wCoord.w()
	};
	static const deUint16 indices[] = { 0, 1, 2, 2, 1, 3 };

	Program progSpec	= PROGRAM_LAST;
	int		numComps	= 0;
	if (params.texType == TEXTURETYPE_2D)
	{
		numComps = 2;

		switch (params.samplerType)
		{
			case SAMPLERTYPE_FLOAT:		progSpec = useBias ? PROGRAM_2D_FLOAT_BIAS	: PROGRAM_2D_FLOAT;		break;
			case SAMPLERTYPE_INT:		progSpec = useBias ? PROGRAM_2D_INT_BIAS	: PROGRAM_2D_INT;		break;
			case SAMPLERTYPE_UINT:		progSpec = useBias ? PROGRAM_2D_UINT_BIAS	: PROGRAM_2D_UINT;		break;
			case SAMPLERTYPE_SHADOW:	progSpec = useBias ? PROGRAM_2D_SHADOW_BIAS	: PROGRAM_2D_SHADOW;	break;
			default:					DE_ASSERT(false);
		}
	}
	else if (params.texType == TEXTURETYPE_1D)
	{
		numComps = 1;

		switch (params.samplerType)
		{
			case SAMPLERTYPE_FLOAT:		progSpec = useBias ? PROGRAM_1D_FLOAT_BIAS	: PROGRAM_1D_FLOAT;		break;
			case SAMPLERTYPE_INT:		progSpec = useBias ? PROGRAM_1D_INT_BIAS	: PROGRAM_1D_INT;		break;
			case SAMPLERTYPE_UINT:		progSpec = useBias ? PROGRAM_1D_UINT_BIAS	: PROGRAM_1D_UINT;		break;
			case SAMPLERTYPE_SHADOW:	progSpec = useBias ? PROGRAM_1D_SHADOW_BIAS	: PROGRAM_1D_SHADOW;	break;
			default:					DE_ASSERT(false);
		}
	}
	else if (params.texType == TEXTURETYPE_CUBE)
	{
		numComps = 3;

		switch (params.samplerType)
		{
			case SAMPLERTYPE_FLOAT:		progSpec = useBias ? PROGRAM_CUBE_FLOAT_BIAS	: PROGRAM_CUBE_FLOAT;	break;
			case SAMPLERTYPE_INT:		progSpec = useBias ? PROGRAM_CUBE_INT_BIAS		: PROGRAM_CUBE_INT;		break;
			case SAMPLERTYPE_UINT:		progSpec = useBias ? PROGRAM_CUBE_UINT_BIAS		: PROGRAM_CUBE_UINT;	break;
			case SAMPLERTYPE_SHADOW:	progSpec = useBias ? PROGRAM_CUBE_SHADOW_BIAS	: PROGRAM_CUBE_SHADOW;	break;
			default:					DE_ASSERT(false);
		}
	}
	else if (params.texType == TEXTURETYPE_3D)
	{
		numComps = 3;

		switch (params.samplerType)
		{
			case SAMPLERTYPE_FLOAT:		progSpec = useBias ? PROGRAM_3D_FLOAT_BIAS	: PROGRAM_3D_FLOAT;		break;
			case SAMPLERTYPE_INT:		progSpec = useBias ? PROGRAM_3D_INT_BIAS	: PROGRAM_3D_INT;		break;
			case SAMPLERTYPE_UINT:		progSpec = useBias ? PROGRAM_3D_UINT_BIAS	: PROGRAM_3D_UINT;		break;
			default:					DE_ASSERT(false);
		}
	}
	else if (params.texType == TEXTURETYPE_2D_ARRAY)
	{
		DE_ASSERT(!useBias); // \todo [2012-02-17 pyry] Support bias.

		numComps = 3;

		switch (params.samplerType)
		{
			case SAMPLERTYPE_FLOAT:		progSpec = PROGRAM_2D_ARRAY_FLOAT;	break;
			case SAMPLERTYPE_INT:		progSpec = PROGRAM_2D_ARRAY_INT;	break;
			case SAMPLERTYPE_UINT:		progSpec = PROGRAM_2D_ARRAY_UINT;	break;
			case SAMPLERTYPE_SHADOW:	progSpec = PROGRAM_2D_ARRAY_SHADOW;	break;
			default:					DE_ASSERT(false);
		}
	}
	else if (params.texType == TEXTURETYPE_CUBE_ARRAY)
	{
		DE_ASSERT(!useBias);

		numComps = 4;

		switch (params.samplerType)
		{
			case SAMPLERTYPE_FLOAT:		progSpec = PROGRAM_CUBE_ARRAY_FLOAT;	break;
			case SAMPLERTYPE_INT:		progSpec = PROGRAM_CUBE_ARRAY_INT;		break;
			case SAMPLERTYPE_UINT:		progSpec = PROGRAM_CUBE_ARRAY_UINT;		break;
			case SAMPLERTYPE_SHADOW:	progSpec = PROGRAM_CUBE_ARRAY_SHADOW;	break;
			default:					DE_ASSERT(false);
		}
	}
	else if (params.texType == TEXTURETYPE_1D_ARRAY)
	{
		DE_ASSERT(!useBias); // \todo [2012-02-17 pyry] Support bias.

		numComps = 2;

		switch (params.samplerType)
		{
			case SAMPLERTYPE_FLOAT:		progSpec = PROGRAM_1D_ARRAY_FLOAT;	break;
			case SAMPLERTYPE_INT:		progSpec = PROGRAM_1D_ARRAY_INT;	break;
			case SAMPLERTYPE_UINT:		progSpec = PROGRAM_1D_ARRAY_UINT;	break;
			case SAMPLERTYPE_SHADOW:	progSpec = PROGRAM_1D_ARRAY_SHADOW;	break;
			default:					DE_ASSERT(false);
		}
	}
	else if (params.texType == TEXTURETYPE_BUFFER)
	{
		numComps = 1;

		switch (params.samplerType)
		{
			case SAMPLERTYPE_FETCH_FLOAT:	progSpec = PROGRAM_BUFFER_FLOAT;	break;
			case SAMPLERTYPE_FETCH_INT:		progSpec = PROGRAM_BUFFER_INT;		break;
			case SAMPLERTYPE_FETCH_UINT:	progSpec = PROGRAM_BUFFER_UINT;		break;
			default:						DE_ASSERT(false);
		}
	}
	else
		DE_ASSERT(DE_FALSE);

	glu::ShaderProgram* program = m_programLibrary.getProgram(progSpec);

	// \todo [2012-09-26 pyry] Move to ProgramLibrary and log unique programs only(?)
	if (params.flags & RenderParams::LOG_PROGRAMS)
		m_log << *program;

	GLU_EXPECT_NO_ERROR(gl.getError(), "Set vertex attributes");

	// Program and uniforms.
	deUint32 prog = program->getProgram();
	gl.useProgram(prog);

	gl.uniform1i(gl.getUniformLocation(prog, "u_sampler"), texUnit);
	if (logUniforms)
		m_log << TestLog::Message << "u_sampler = " << texUnit << TestLog::EndMessage;

	if (useBias)
	{
		gl.uniform1f(gl.getUniformLocation(prog, "u_bias"), params.bias);
		if (logUniforms)
			m_log << TestLog::Message << "u_bias = " << params.bias << TestLog::EndMessage;
	}

	if (params.samplerType == SAMPLERTYPE_SHADOW)
	{
		gl.uniform1f(gl.getUniformLocation(prog, "u_ref"), params.ref);
		if (logUniforms)
			m_log << TestLog::Message << "u_ref = " << params.ref << TestLog::EndMessage;
	}

	gl.uniform4fv(gl.getUniformLocation(prog, "u_colorScale"),	1, params.colorScale.getPtr());
	gl.uniform4fv(gl.getUniformLocation(prog, "u_colorBias"),	1, params.colorBias.getPtr());

	if (logUniforms)
	{
		m_log << TestLog::Message << "u_colorScale = " << params.colorScale << TestLog::EndMessage;
		m_log << TestLog::Message << "u_colorBias = " << params.colorBias << TestLog::EndMessage;
	}

	GLU_EXPECT_NO_ERROR(gl.getError(), "Set program state");

	{
		const glu::VertexArrayBinding vertexArrays[] =
		{
			glu::va::Float("a_position",	4,			4, 0, &position[0]),
			glu::va::Float("a_texCoord",	numComps,	4, 0, texCoord)
		};
		glu::draw(m_renderCtx, prog, DE_LENGTH_OF_ARRAY(vertexArrays), &vertexArrays[0],
				  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
	}
}

void computeQuadTexCoord1D (std::vector<float>& dst, float left, float right)
{
	dst.resize(4);

	dst[0] = left;
	dst[1] = left;
	dst[2] = right;
	dst[3] = right;
}

void computeQuadTexCoord1DArray (std::vector<float>& dst, int layerNdx, float left, float right)
{
	dst.resize(4*2);

	dst[0] = left;	dst[1] = (float)layerNdx;
	dst[2] = left;	dst[3] = (float)layerNdx;
	dst[4] = right;	dst[5] = (float)layerNdx;
	dst[6] = right;	dst[7] = (float)layerNdx;
}

void computeQuadTexCoord2D (std::vector<float>& dst, const tcu::Vec2& bottomLeft, const tcu::Vec2& topRight)
{
	dst.resize(4*2);

	dst[0] = bottomLeft.x();	dst[1] = bottomLeft.y();
	dst[2] = bottomLeft.x();	dst[3] = topRight.y();
	dst[4] = topRight.x();		dst[5] = bottomLeft.y();
	dst[6] = topRight.x();		dst[7] = topRight.y();
}

void computeQuadTexCoord2DArray (std::vector<float>& dst, int layerNdx, const tcu::Vec2& bottomLeft, const tcu::Vec2& topRight)
{
	dst.resize(4*3);

	dst[0] = bottomLeft.x();	dst[ 1] = bottomLeft.y();	dst[ 2] = (float)layerNdx;
	dst[3] = bottomLeft.x();	dst[ 4] = topRight.y();		dst[ 5] = (float)layerNdx;
	dst[6] = topRight.x();		dst[ 7] = bottomLeft.y();	dst[ 8] = (float)layerNdx;
	dst[9] = topRight.x();		dst[10] = topRight.y();		dst[11] = (float)layerNdx;
}

void computeQuadTexCoord3D (std::vector<float>& dst, const tcu::Vec3& p0, const tcu::Vec3& p1, const tcu::IVec3& dirSwz)
{
	tcu::Vec3 f0 = tcu::Vec3(0.0f, 0.0f, 0.0f).swizzle(dirSwz[0], dirSwz[1], dirSwz[2]);
	tcu::Vec3 f1 = tcu::Vec3(0.0f, 1.0f, 0.0f).swizzle(dirSwz[0], dirSwz[1], dirSwz[2]);
	tcu::Vec3 f2 = tcu::Vec3(1.0f, 0.0f, 0.0f).swizzle(dirSwz[0], dirSwz[1], dirSwz[2]);
	tcu::Vec3 f3 = tcu::Vec3(1.0f, 1.0f, 0.0f).swizzle(dirSwz[0], dirSwz[1], dirSwz[2]);

	tcu::Vec3 v0 = p0 + (p1-p0)*f0;
	tcu::Vec3 v1 = p0 + (p1-p0)*f1;
	tcu::Vec3 v2 = p0 + (p1-p0)*f2;
	tcu::Vec3 v3 = p0 + (p1-p0)*f3;

	dst.resize(4*3);

	dst[0] = v0.x(); dst[ 1] = v0.y(); dst[ 2] = v0.z();
	dst[3] = v1.x(); dst[ 4] = v1.y(); dst[ 5] = v1.z();
	dst[6] = v2.x(); dst[ 7] = v2.y(); dst[ 8] = v2.z();
	dst[9] = v3.x(); dst[10] = v3.y(); dst[11] = v3.z();
}

void computeQuadTexCoordCube (std::vector<float>& dst, tcu::CubeFace face)
{
	static const float texCoordNegX[] =
	{
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f
	};
	static const float texCoordPosX[] =
	{
		+1.0f,  1.0f,  1.0f,
		+1.0f, -1.0f,  1.0f,
		+1.0f,  1.0f, -1.0f,
		+1.0f, -1.0f, -1.0f
	};
	static const float texCoordNegY[] =
	{
		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f
	};
	static const float texCoordPosY[] =
	{
		-1.0f, +1.0f, -1.0f,
		-1.0f, +1.0f,  1.0f,
		 1.0f, +1.0f, -1.0f,
		 1.0f, +1.0f,  1.0f
	};
	static const float texCoordNegZ[] =
	{
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f
	};
	static const float texCoordPosZ[] =
	{
		-1.0f,  1.0f, +1.0f,
		-1.0f, -1.0f, +1.0f,
		 1.0f,  1.0f, +1.0f,
		 1.0f, -1.0f, +1.0f
	};

	const float*	texCoord		= DE_NULL;
	int				texCoordSize	= DE_LENGTH_OF_ARRAY(texCoordNegX);

	switch (face)
	{
		case tcu::CUBEFACE_NEGATIVE_X: texCoord = texCoordNegX; break;
		case tcu::CUBEFACE_POSITIVE_X: texCoord = texCoordPosX; break;
		case tcu::CUBEFACE_NEGATIVE_Y: texCoord = texCoordNegY; break;
		case tcu::CUBEFACE_POSITIVE_Y: texCoord = texCoordPosY; break;
		case tcu::CUBEFACE_NEGATIVE_Z: texCoord = texCoordNegZ; break;
		case tcu::CUBEFACE_POSITIVE_Z: texCoord = texCoordPosZ; break;
		default:
			DE_ASSERT(DE_FALSE);
			return;
	}

	dst.resize(texCoordSize);
	std::copy(texCoord, texCoord+texCoordSize, dst.begin());
}

void computeQuadTexCoordCube (std::vector<float>& dst, tcu::CubeFace face, const tcu::Vec2& bottomLeft, const tcu::Vec2& topRight)
{
	int		sRow		= 0;
	int		tRow		= 0;
	int		mRow		= 0;
	float	sSign		= 1.0f;
	float	tSign		= 1.0f;
	float	mSign		= 1.0f;

	switch (face)
	{
		case tcu::CUBEFACE_NEGATIVE_X: mRow = 0; sRow = 2; tRow = 1; mSign = -1.0f;				   tSign = -1.0f;	break;
		case tcu::CUBEFACE_POSITIVE_X: mRow = 0; sRow = 2; tRow = 1;				sSign = -1.0f; tSign = -1.0f;	break;
		case tcu::CUBEFACE_NEGATIVE_Y: mRow = 1; sRow = 0; tRow = 2; mSign = -1.0f;				   tSign = -1.0f;	break;
		case tcu::CUBEFACE_POSITIVE_Y: mRow = 1; sRow = 0; tRow = 2;												break;
		case tcu::CUBEFACE_NEGATIVE_Z: mRow = 2; sRow = 0; tRow = 1; mSign = -1.0f; sSign = -1.0f; tSign = -1.0f;	break;
		case tcu::CUBEFACE_POSITIVE_Z: mRow = 2; sRow = 0; tRow = 1;							   tSign = -1.0f;	break;
		default:
			DE_ASSERT(DE_FALSE);
			return;
	}

	dst.resize(3*4);

	dst[0+mRow] = mSign;
	dst[3+mRow] = mSign;
	dst[6+mRow] = mSign;
	dst[9+mRow] = mSign;

	dst[0+sRow] = sSign * bottomLeft.x();
	dst[3+sRow] = sSign * bottomLeft.x();
	dst[6+sRow] = sSign * topRight.x();
	dst[9+sRow] = sSign * topRight.x();

	dst[0+tRow] = tSign * bottomLeft.y();
	dst[3+tRow] = tSign * topRight.y();
	dst[6+tRow] = tSign * bottomLeft.y();
	dst[9+tRow] = tSign * topRight.y();
}

void computeQuadTexCoordCubeArray (std::vector<float>& dst, tcu::CubeFace face, const tcu::Vec2& bottomLeft, const tcu::Vec2& topRight, const tcu::Vec2& layerRange)
{
	int			sRow	= 0;
	int			tRow	= 0;
	int			mRow	= 0;
	const int	qRow	= 3;
	float		sSign	= 1.0f;
	float		tSign	= 1.0f;
	float		mSign	= 1.0f;
	const float	l0		= layerRange.x();
	const float	l1		= layerRange.y();

	switch (face)
	{
		case tcu::CUBEFACE_NEGATIVE_X: mRow = 0; sRow = 2; tRow = 1; mSign = -1.0f;				   tSign = -1.0f;	break;
		case tcu::CUBEFACE_POSITIVE_X: mRow = 0; sRow = 2; tRow = 1;				sSign = -1.0f; tSign = -1.0f;	break;
		case tcu::CUBEFACE_NEGATIVE_Y: mRow = 1; sRow = 0; tRow = 2; mSign = -1.0f;				   tSign = -1.0f;	break;
		case tcu::CUBEFACE_POSITIVE_Y: mRow = 1; sRow = 0; tRow = 2;												break;
		case tcu::CUBEFACE_NEGATIVE_Z: mRow = 2; sRow = 0; tRow = 1; mSign = -1.0f; sSign = -1.0f; tSign = -1.0f;	break;
		case tcu::CUBEFACE_POSITIVE_Z: mRow = 2; sRow = 0; tRow = 1;							   tSign = -1.0f;	break;
		default:
			DE_ASSERT(DE_FALSE);
			return;
	}

	dst.resize(4*4);

	dst[ 0+mRow] = mSign;
	dst[ 4+mRow] = mSign;
	dst[ 8+mRow] = mSign;
	dst[12+mRow] = mSign;

	dst[ 0+sRow] = sSign * bottomLeft.x();
	dst[ 4+sRow] = sSign * bottomLeft.x();
	dst[ 8+sRow] = sSign * topRight.x();
	dst[12+sRow] = sSign * topRight.x();

	dst[ 0+tRow] = tSign * bottomLeft.y();
	dst[ 4+tRow] = tSign * topRight.y();
	dst[ 8+tRow] = tSign * bottomLeft.y();
	dst[12+tRow] = tSign * topRight.y();

	if (l0 != l1)
	{
		dst[ 0+qRow] = l0;
		dst[ 4+qRow] = l0*0.5f + l1*0.5f;
		dst[ 8+qRow] = l0*0.5f + l1*0.5f;
		dst[12+qRow] = l1;
	}
	else
	{
		dst[ 0+qRow] = l0;
		dst[ 4+qRow] = l0;
		dst[ 8+qRow] = l0;
		dst[12+qRow] = l0;
	}
}

// Texture result verification

//! Verifies texture lookup results and returns number of failed pixels.
int computeTextureLookupDiff (const tcu::ConstPixelBufferAccess&	result,
							  const tcu::ConstPixelBufferAccess&	reference,
							  const tcu::PixelBufferAccess&			errorMask,
							  const tcu::Texture1DView&				baseView,
							  const float*							texCoord,
							  const ReferenceParams&				sampleParams,
							  const tcu::LookupPrecision&			lookupPrec,
							  const tcu::LodPrecision&				lodPrec,
							  qpWatchDog*							watchDog)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture1DView					src					= getEffectiveTextureView(getSubView(baseView, sampleParams.baseLevel, sampleParams.maxLevel), srcLevelStorage, sampleParams.sampler);

	const tcu::Vec4								sq					= tcu::Vec4(texCoord[0], texCoord[1], texCoord[2], texCoord[3]);

	const tcu::IVec2							dstSize				= tcu::IVec2(result.getWidth(), result.getHeight());
	const float									dstW				= float(dstSize.x());
	const float									dstH				= float(dstSize.y());
	const int									srcSize				= src.getWidth();

	// Coordinates and lod per triangle.
	const tcu::Vec3								triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3								triW[2]				= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2								lodBias				((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	int											numFailed			= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		// Ugly hack, validation can take way too long at the moment.
		if (watchDog)
			qpWatchDog_touch(watchDog);

		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= (result.getPixel(px, py)		- sampleParams.colorBias) / sampleParams.colorScale;
			const tcu::Vec4	refPix	= (reference.getPixel(px, py)	- sampleParams.colorBias) / sampleParams.colorScale;

			// Try comparison to ideal reference first, and if that fails use slower verificator.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(resPix - refPix), lookupPrec.colorThreshold)))
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
				const float		triWx	= triNdx ? dstW - wx : wx;
				const float		triWy	= triNdx ? dstH - wy : wy;
				const float		triNx	= triNdx ? 1.0f - nx : nx;
				const float		triNy	= triNdx ? 1.0f - ny : ny;

				const float		coord		= projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy);
				const float		coordDx		= triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy) * float(srcSize);
				const float 	coordDy		= triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx) * float(srcSize);

				tcu::Vec2		lodBounds	= tcu::computeLodBoundsFromDerivates(coordDx, coordDy, lodPrec);

				// Compute lod bounds across lodOffsets range.
				for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
				{
					const float		wxo		= triWx + lodOffsets[lodOffsNdx].x();
					const float		wyo		= triWy + lodOffsets[lodOffsNdx].y();
					const float		nxo		= wxo/dstW;
					const float		nyo		= wyo/dstH;

					const float	coordDxo	= triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo) * float(srcSize);
					const float	coordDyo	= triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo) * float(srcSize);
					const tcu::Vec2	lodO	= tcu::computeLodBoundsFromDerivates(coordDxo, coordDyo, lodPrec);

					lodBounds.x() = de::min(lodBounds.x(), lodO.x());
					lodBounds.y() = de::max(lodBounds.y(), lodO.y());
				}

				const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);
				const bool		isOk		= tcu::isLookupResultValid(src, sampleParams.sampler, lookupPrec, coord, clampedLod, resPix);

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

int computeTextureLookupDiff (const tcu::ConstPixelBufferAccess&	result,
							  const tcu::ConstPixelBufferAccess&	reference,
							  const tcu::PixelBufferAccess&			errorMask,
							  const tcu::Texture2DView&				baseView,
							  const float*							texCoord,
							  const ReferenceParams&				sampleParams,
							  const tcu::LookupPrecision&			lookupPrec,
							  const tcu::LodPrecision&				lodPrec,
							  qpWatchDog*							watchDog)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture2DView					src					= getEffectiveTextureView(getSubView(baseView, sampleParams.baseLevel, sampleParams.maxLevel), srcLevelStorage, sampleParams.sampler);

	const tcu::Vec4								sq					= tcu::Vec4(texCoord[0+0], texCoord[2+0], texCoord[4+0], texCoord[6+0]);
	const tcu::Vec4								tq					= tcu::Vec4(texCoord[0+1], texCoord[2+1], texCoord[4+1], texCoord[6+1]);

	const tcu::IVec2							dstSize				= tcu::IVec2(result.getWidth(), result.getHeight());
	const float									dstW				= float(dstSize.x());
	const float									dstH				= float(dstSize.y());
	const tcu::IVec2							srcSize				= tcu::IVec2(src.getWidth(), src.getHeight());

	// Coordinates and lod per triangle.
	const tcu::Vec3								triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3								triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3								triW[2]				= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2								lodBias				((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	int											numFailed			= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		// Ugly hack, validation can take way too long at the moment.
		if (watchDog)
			qpWatchDog_touch(watchDog);

		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= (result.getPixel(px, py)		- sampleParams.colorBias) / sampleParams.colorScale;
			const tcu::Vec4	refPix	= (reference.getPixel(px, py)	- sampleParams.colorBias) / sampleParams.colorScale;

			// Try comparison to ideal reference first, and if that fails use slower verificator.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(resPix - refPix), lookupPrec.colorThreshold)))
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
				const float		triWx	= triNdx ? dstW - wx : wx;
				const float		triWy	= triNdx ? dstH - wy : wy;
				const float		triNx	= triNdx ? 1.0f - nx : nx;
				const float		triNy	= triNdx ? 1.0f - ny : ny;

				const tcu::Vec2	coord		(projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy),
											 projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy));
				const tcu::Vec2	coordDx		= tcu::Vec2(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
														triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy)) * srcSize.asFloat();
				const tcu::Vec2	coordDy		= tcu::Vec2(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
														triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx)) * srcSize.asFloat();

				tcu::Vec2		lodBounds	= tcu::computeLodBoundsFromDerivates(coordDx.x(), coordDx.y(), coordDy.x(), coordDy.y(), lodPrec);

				// Compute lod bounds across lodOffsets range.
				for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
				{
					const float		wxo		= triWx + lodOffsets[lodOffsNdx].x();
					const float		wyo		= triWy + lodOffsets[lodOffsNdx].y();
					const float		nxo		= wxo/dstW;
					const float		nyo		= wyo/dstH;

					const tcu::Vec2	coordDxo	= tcu::Vec2(triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo),
															triDerivateX(triT[triNdx], triW[triNdx], wxo, dstW, nyo)) * srcSize.asFloat();
					const tcu::Vec2	coordDyo	= tcu::Vec2(triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo),
															triDerivateY(triT[triNdx], triW[triNdx], wyo, dstH, nxo)) * srcSize.asFloat();
					const tcu::Vec2	lodO		= tcu::computeLodBoundsFromDerivates(coordDxo.x(), coordDxo.y(), coordDyo.x(), coordDyo.y(), lodPrec);

					lodBounds.x() = de::min(lodBounds.x(), lodO.x());
					lodBounds.y() = de::max(lodBounds.y(), lodO.y());
				}

				const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);
				const bool		isOk		= tcu::isLookupResultValid(src, sampleParams.sampler, lookupPrec, coord, clampedLod, resPix);

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

bool verifyTextureResult (tcu::TestContext&						testCtx,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::Texture1DView&				src,
						  const float*							texCoord,
						  const ReferenceParams&				sampleParams,
						  const tcu::LookupPrecision&			lookupPrec,
						  const tcu::LodPrecision&				lodPrec,
						  const tcu::PixelFormat&				pixelFormat)
{
	tcu::TestLog&	log				= testCtx.getLog();
	tcu::Surface	reference		(result.getWidth(), result.getHeight());
	tcu::Surface	errorMask		(result.getWidth(), result.getHeight());
	int				numFailedPixels;

	DE_ASSERT(getCompareMask(pixelFormat) == lookupPrec.colorMask);

	sampleTexture(SurfaceAccess(reference, pixelFormat), src, texCoord, sampleParams);
	numFailedPixels = computeTextureLookupDiff(result, reference.getAccess(), errorMask.getAccess(), src, texCoord, sampleParams, lookupPrec, lodPrec, testCtx.getWatchDog());

	if (numFailedPixels > 0)
		log << TestLog::Message << "ERROR: Result verification failed, got " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

	log << TestLog::ImageSet("VerifyResult", "Verification result")
		<< TestLog::Image("Rendered", "Rendered image", result);

	if (numFailedPixels > 0)
	{
		log << TestLog::Image("Reference", "Ideal reference image", reference)
			<< TestLog::Image("ErrorMask", "Error mask", errorMask);
	}

	log << TestLog::EndImageSet;

	return numFailedPixels == 0;
}

bool verifyTextureResult (tcu::TestContext&						testCtx,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::Texture2DView&				src,
						  const float*							texCoord,
						  const ReferenceParams&				sampleParams,
						  const tcu::LookupPrecision&			lookupPrec,
						  const tcu::LodPrecision&				lodPrec,
						  const tcu::PixelFormat&				pixelFormat)
{
	tcu::TestLog&	log				= testCtx.getLog();
	tcu::Surface	reference		(result.getWidth(), result.getHeight());
	tcu::Surface	errorMask		(result.getWidth(), result.getHeight());
	int				numFailedPixels;

	DE_ASSERT(getCompareMask(pixelFormat) == lookupPrec.colorMask);

	sampleTexture(SurfaceAccess(reference, pixelFormat), src, texCoord, sampleParams);
	numFailedPixels = computeTextureLookupDiff(result, reference.getAccess(), errorMask.getAccess(), src, texCoord, sampleParams, lookupPrec, lodPrec, testCtx.getWatchDog());

	if (numFailedPixels > 0)
		log << TestLog::Message << "ERROR: Result verification failed, got " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

	log << TestLog::ImageSet("VerifyResult", "Verification result")
		<< TestLog::Image("Rendered", "Rendered image", result);

	if (numFailedPixels > 0)
	{
		log << TestLog::Image("Reference", "Ideal reference image", reference)
			<< TestLog::Image("ErrorMask", "Error mask", errorMask);
	}

	log << TestLog::EndImageSet;

	return numFailedPixels == 0;
}

//! Verifies texture lookup results and returns number of failed pixels.
int computeTextureLookupDiff (const tcu::ConstPixelBufferAccess&	result,
							  const tcu::ConstPixelBufferAccess&	reference,
							  const tcu::PixelBufferAccess&			errorMask,
							  const tcu::TextureCubeView&			baseView,
							  const float*							texCoord,
							  const ReferenceParams&				sampleParams,
							  const tcu::LookupPrecision&			lookupPrec,
							  const tcu::LodPrecision&				lodPrec,
							  qpWatchDog*							watchDog)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::TextureCubeView					src					= getEffectiveTextureView(getSubView(baseView, sampleParams.baseLevel, sampleParams.maxLevel), srcLevelStorage, sampleParams.sampler);

	const tcu::Vec4								sq					= tcu::Vec4(texCoord[0+0], texCoord[3+0], texCoord[6+0], texCoord[9+0]);
	const tcu::Vec4								tq					= tcu::Vec4(texCoord[0+1], texCoord[3+1], texCoord[6+1], texCoord[9+1]);
	const tcu::Vec4								rq					= tcu::Vec4(texCoord[0+2], texCoord[3+2], texCoord[6+2], texCoord[9+2]);

	const tcu::IVec2							dstSize				= tcu::IVec2(result.getWidth(), result.getHeight());
	const float									dstW				= float(dstSize.x());
	const float									dstH				= float(dstSize.y());
	const int									srcSize				= src.getSize();

	// Coordinates per triangle.
	const tcu::Vec3								triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3								triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3								triR[2]				= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	const tcu::Vec3								triW[2]				= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2								lodBias				((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	const float									posEps				= 1.0f / float(1<<MIN_SUBPIXEL_BITS);

	int											numFailed			= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),

		// \note Not strictly allowed by spec, but implementations do this in practice.
		tcu::Vec2(-1, -1),
		tcu::Vec2(-1, +1),
		tcu::Vec2(+1, -1),
		tcu::Vec2(+1, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		// Ugly hack, validation can take way too long at the moment.
		if (watchDog)
			qpWatchDog_touch(watchDog);

		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= (result.getPixel(px, py)		- sampleParams.colorBias) / sampleParams.colorScale;
			const tcu::Vec4	refPix	= (reference.getPixel(px, py)	- sampleParams.colorBias) / sampleParams.colorScale;

			// Try comparison to ideal reference first, and if that fails use slower verificator.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(resPix - refPix), lookupPrec.colorThreshold)))
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const bool		tri0	= (wx-posEps)/dstW + (wy-posEps)/dstH <= 1.0f;
				const bool		tri1	= (wx+posEps)/dstW + (wy+posEps)/dstH >= 1.0f;

				bool			isOk	= false;

				DE_ASSERT(tri0 || tri1);

				// Pixel can belong to either of the triangles if it lies close enough to the edge.
				for (int triNdx = (tri0?0:1); triNdx <= (tri1?1:0); triNdx++)
				{
					const float		triWx	= triNdx ? dstW - wx : wx;
					const float		triWy	= triNdx ? dstH - wy : wy;
					const float		triNx	= triNdx ? 1.0f - nx : nx;
					const float		triNy	= triNdx ? 1.0f - ny : ny;

					const tcu::Vec3	coord		(projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy),
												 projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy),
												 projectedTriInterpolate(triR[triNdx], triW[triNdx], triNx, triNy));
					const tcu::Vec3	coordDx		(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
												 triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy),
												 triDerivateX(triR[triNdx], triW[triNdx], wx, dstW, triNy));
					const tcu::Vec3	coordDy		(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
												 triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx),
												 triDerivateY(triR[triNdx], triW[triNdx], wy, dstH, triNx));

					tcu::Vec2		lodBounds	= tcu::computeCubeLodBoundsFromDerivates(coord, coordDx, coordDy, srcSize, lodPrec);

					// Compute lod bounds across lodOffsets range.
					for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
					{
						const float		wxo		= triWx + lodOffsets[lodOffsNdx].x();
						const float		wyo		= triWy + lodOffsets[lodOffsNdx].y();
						const float		nxo		= wxo/dstW;
						const float		nyo		= wyo/dstH;

						const tcu::Vec3	coordO		(projectedTriInterpolate(triS[triNdx], triW[triNdx], nxo, nyo),
													 projectedTriInterpolate(triT[triNdx], triW[triNdx], nxo, nyo),
													 projectedTriInterpolate(triR[triNdx], triW[triNdx], nxo, nyo));
						const tcu::Vec3	coordDxo	(triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo),
													 triDerivateX(triT[triNdx], triW[triNdx], wxo, dstW, nyo),
													 triDerivateX(triR[triNdx], triW[triNdx], wxo, dstW, nyo));
						const tcu::Vec3	coordDyo	(triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo),
													 triDerivateY(triT[triNdx], triW[triNdx], wyo, dstH, nxo),
													 triDerivateY(triR[triNdx], triW[triNdx], wyo, dstH, nxo));
						const tcu::Vec2	lodO		= tcu::computeCubeLodBoundsFromDerivates(coordO, coordDxo, coordDyo, srcSize, lodPrec);

						lodBounds.x() = de::min(lodBounds.x(), lodO.x());
						lodBounds.y() = de::max(lodBounds.y(), lodO.y());
					}

					const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);

					if (tcu::isLookupResultValid(src, sampleParams.sampler, lookupPrec, coord, clampedLod, resPix))
					{
						isOk = true;
						break;
					}
				}

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

bool verifyTextureResult (tcu::TestContext&						testCtx,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::TextureCubeView&			src,
						  const float*							texCoord,
						  const ReferenceParams&				sampleParams,
						  const tcu::LookupPrecision&			lookupPrec,
						  const tcu::LodPrecision&				lodPrec,
						  const tcu::PixelFormat&				pixelFormat)
{
	tcu::TestLog&	log				= testCtx.getLog();
	tcu::Surface	reference		(result.getWidth(), result.getHeight());
	tcu::Surface	errorMask		(result.getWidth(), result.getHeight());
	int				numFailedPixels;

	DE_ASSERT(getCompareMask(pixelFormat) == lookupPrec.colorMask);

	sampleTexture(SurfaceAccess(reference, pixelFormat), src, texCoord, sampleParams);
	numFailedPixels = computeTextureLookupDiff(result, reference.getAccess(), errorMask.getAccess(), src, texCoord, sampleParams, lookupPrec, lodPrec, testCtx.getWatchDog());

	if (numFailedPixels > 0)
		log << TestLog::Message << "ERROR: Result verification failed, got " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

	log << TestLog::ImageSet("VerifyResult", "Verification result")
		<< TestLog::Image("Rendered", "Rendered image", result);

	if (numFailedPixels > 0)
	{
		log << TestLog::Image("Reference", "Ideal reference image", reference)
			<< TestLog::Image("ErrorMask", "Error mask", errorMask);
	}

	log << TestLog::EndImageSet;

	return numFailedPixels == 0;
}

//! Verifies texture lookup results and returns number of failed pixels.
int computeTextureLookupDiff (const tcu::ConstPixelBufferAccess&	result,
							  const tcu::ConstPixelBufferAccess&	reference,
							  const tcu::PixelBufferAccess&			errorMask,
							  const tcu::Texture3DView&				baseView,
							  const float*							texCoord,
							  const ReferenceParams&				sampleParams,
							  const tcu::LookupPrecision&			lookupPrec,
							  const tcu::LodPrecision&				lodPrec,
							  qpWatchDog*							watchDog)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture3DView					src					= getEffectiveTextureView(getSubView(baseView, sampleParams.baseLevel, sampleParams.maxLevel), srcLevelStorage, sampleParams.sampler);

	const tcu::Vec4								sq					= tcu::Vec4(texCoord[0+0], texCoord[3+0], texCoord[6+0], texCoord[9+0]);
	const tcu::Vec4								tq					= tcu::Vec4(texCoord[0+1], texCoord[3+1], texCoord[6+1], texCoord[9+1]);
	const tcu::Vec4								rq					= tcu::Vec4(texCoord[0+2], texCoord[3+2], texCoord[6+2], texCoord[9+2]);

	const tcu::IVec2							dstSize				= tcu::IVec2(result.getWidth(), result.getHeight());
	const float									dstW				= float(dstSize.x());
	const float									dstH				= float(dstSize.y());
	const tcu::IVec3							srcSize				= tcu::IVec3(src.getWidth(), src.getHeight(), src.getDepth());

	// Coordinates and lod per triangle.
	const tcu::Vec3								triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3								triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3								triR[2]				= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	const tcu::Vec3								triW[2]				= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2								lodBias				((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	const float									posEps				= 1.0f / float(1<<MIN_SUBPIXEL_BITS);

	int											numFailed			= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		// Ugly hack, validation can take way too long at the moment.
		if (watchDog)
			qpWatchDog_touch(watchDog);

		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= (result.getPixel(px, py)		- sampleParams.colorBias) / sampleParams.colorScale;
			const tcu::Vec4	refPix	= (reference.getPixel(px, py)	- sampleParams.colorBias) / sampleParams.colorScale;

			// Try comparison to ideal reference first, and if that fails use slower verificator.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(resPix - refPix), lookupPrec.colorThreshold)))
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const bool		tri0	= (wx-posEps)/dstW + (wy-posEps)/dstH <= 1.0f;
				const bool		tri1	= (wx+posEps)/dstW + (wy+posEps)/dstH >= 1.0f;

				bool			isOk	= false;

				DE_ASSERT(tri0 || tri1);

				// Pixel can belong to either of the triangles if it lies close enough to the edge.
				for (int triNdx = (tri0?0:1); triNdx <= (tri1?1:0); triNdx++)
				{
					const float		triWx	= triNdx ? dstW - wx : wx;
					const float		triWy	= triNdx ? dstH - wy : wy;
					const float		triNx	= triNdx ? 1.0f - nx : nx;
					const float		triNy	= triNdx ? 1.0f - ny : ny;

					const tcu::Vec3	coord		(projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy),
												 projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy),
												 projectedTriInterpolate(triR[triNdx], triW[triNdx], triNx, triNy));
					const tcu::Vec3	coordDx		= tcu::Vec3(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
															triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy),
															triDerivateX(triR[triNdx], triW[triNdx], wx, dstW, triNy)) * srcSize.asFloat();
					const tcu::Vec3	coordDy		= tcu::Vec3(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
															triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx),
															triDerivateY(triR[triNdx], triW[triNdx], wy, dstH, triNx)) * srcSize.asFloat();

					tcu::Vec2		lodBounds	= tcu::computeLodBoundsFromDerivates(coordDx.x(), coordDx.y(), coordDx.z(), coordDy.x(), coordDy.y(), coordDy.z(), lodPrec);

					// Compute lod bounds across lodOffsets range.
					for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
					{
						const float		wxo		= triWx + lodOffsets[lodOffsNdx].x();
						const float		wyo		= triWy + lodOffsets[lodOffsNdx].y();
						const float		nxo		= wxo/dstW;
						const float		nyo		= wyo/dstH;

						const tcu::Vec3	coordDxo	= tcu::Vec3(triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo),
																triDerivateX(triT[triNdx], triW[triNdx], wxo, dstW, nyo),
																triDerivateX(triR[triNdx], triW[triNdx], wxo, dstW, nyo)) * srcSize.asFloat();
						const tcu::Vec3	coordDyo	= tcu::Vec3(triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo),
																triDerivateY(triT[triNdx], triW[triNdx], wyo, dstH, nxo),
																triDerivateY(triR[triNdx], triW[triNdx], wyo, dstH, nxo)) * srcSize.asFloat();
						const tcu::Vec2	lodO		= tcu::computeLodBoundsFromDerivates(coordDxo.x(), coordDxo.y(), coordDxo.z(), coordDyo.x(), coordDyo.y(), coordDyo.z(), lodPrec);

						lodBounds.x() = de::min(lodBounds.x(), lodO.x());
						lodBounds.y() = de::max(lodBounds.y(), lodO.y());
					}

					const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);

					if (tcu::isLookupResultValid(src, sampleParams.sampler, lookupPrec, coord, clampedLod, resPix))
					{
						isOk = true;
						break;
					}
				}

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

bool verifyTextureResult (tcu::TestContext&						testCtx,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::Texture3DView&				src,
						  const float*							texCoord,
						  const ReferenceParams&				sampleParams,
						  const tcu::LookupPrecision&			lookupPrec,
						  const tcu::LodPrecision&				lodPrec,
						  const tcu::PixelFormat&				pixelFormat)
{
	tcu::TestLog&	log				= testCtx.getLog();
	tcu::Surface	reference		(result.getWidth(), result.getHeight());
	tcu::Surface	errorMask		(result.getWidth(), result.getHeight());
	int				numFailedPixels;

	DE_ASSERT(getCompareMask(pixelFormat) == lookupPrec.colorMask);

	sampleTexture(SurfaceAccess(reference, pixelFormat), src, texCoord, sampleParams);
	numFailedPixels = computeTextureLookupDiff(result, reference.getAccess(), errorMask.getAccess(), src, texCoord, sampleParams, lookupPrec, lodPrec, testCtx.getWatchDog());

	if (numFailedPixels > 0)
		log << TestLog::Message << "ERROR: Result verification failed, got " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

	log << TestLog::ImageSet("VerifyResult", "Verification result")
		<< TestLog::Image("Rendered", "Rendered image", result);

	if (numFailedPixels > 0)
	{
		log << TestLog::Image("Reference", "Ideal reference image", reference)
			<< TestLog::Image("ErrorMask", "Error mask", errorMask);
	}

	log << TestLog::EndImageSet;

	return numFailedPixels == 0;
}

//! Verifies texture lookup results and returns number of failed pixels.
int computeTextureLookupDiff (const tcu::ConstPixelBufferAccess&	result,
							  const tcu::ConstPixelBufferAccess&	reference,
							  const tcu::PixelBufferAccess&			errorMask,
							  const tcu::Texture1DArrayView&		baseView,
							  const float*							texCoord,
							  const ReferenceParams&				sampleParams,
							  const tcu::LookupPrecision&			lookupPrec,
							  const tcu::LodPrecision&				lodPrec,
							  qpWatchDog*							watchDog)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture1DArrayView				src					= getEffectiveTextureView(baseView, srcLevelStorage, sampleParams.sampler);

	const tcu::Vec4								sq					= tcu::Vec4(texCoord[0+0], texCoord[2+0], texCoord[4+0], texCoord[6+0]);
	const tcu::Vec4								tq					= tcu::Vec4(texCoord[0+1], texCoord[2+1], texCoord[4+1], texCoord[6+1]);

	const tcu::IVec2							dstSize				= tcu::IVec2(result.getWidth(), result.getHeight());
	const float									dstW				= float(dstSize.x());
	const float									dstH				= float(dstSize.y());
	const float									srcSize				= float(src.getWidth()); // For lod computation, thus #layers is ignored.

	// Coordinates and lod per triangle.
	const tcu::Vec3								triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3								triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3								triW[2]				= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2								lodBias				((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	int											numFailed			= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		// Ugly hack, validation can take way too long at the moment.
		if (watchDog)
			qpWatchDog_touch(watchDog);

		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= (result.getPixel(px, py)		- sampleParams.colorBias) / sampleParams.colorScale;
			const tcu::Vec4	refPix	= (reference.getPixel(px, py)	- sampleParams.colorBias) / sampleParams.colorScale;

			// Try comparison to ideal reference first, and if that fails use slower verificator.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(resPix - refPix), lookupPrec.colorThreshold)))
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
				const float		triWx	= triNdx ? dstW - wx : wx;
				const float		triWy	= triNdx ? dstH - wy : wy;
				const float		triNx	= triNdx ? 1.0f - nx : nx;
				const float		triNy	= triNdx ? 1.0f - ny : ny;

				const tcu::Vec2	coord	(projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy),
										 projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy));
				const float	coordDx		= triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy) * srcSize;
				const float	coordDy		= triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx) * srcSize;

				tcu::Vec2		lodBounds	= tcu::computeLodBoundsFromDerivates(coordDx, coordDy, lodPrec);

				// Compute lod bounds across lodOffsets range.
				for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
				{
					const float		wxo		= triWx + lodOffsets[lodOffsNdx].x();
					const float		wyo		= triWy + lodOffsets[lodOffsNdx].y();
					const float		nxo		= wxo/dstW;
					const float		nyo		= wyo/dstH;

					const float	coordDxo		= triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo) * srcSize;
					const float	coordDyo		= triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo) * srcSize;
					const tcu::Vec2	lodO		= tcu::computeLodBoundsFromDerivates(coordDxo, coordDyo, lodPrec);

					lodBounds.x() = de::min(lodBounds.x(), lodO.x());
					lodBounds.y() = de::max(lodBounds.y(), lodO.y());
				}

				const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);
				const bool		isOk		= tcu::isLookupResultValid(src, sampleParams.sampler, lookupPrec, coord, clampedLod, resPix);

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

//! Verifies texture lookup results and returns number of failed pixels.
int computeTextureLookupDiff (const tcu::ConstPixelBufferAccess&	result,
							  const tcu::ConstPixelBufferAccess&	reference,
							  const tcu::PixelBufferAccess&			errorMask,
							  const tcu::Texture2DArrayView&		baseView,
							  const float*							texCoord,
							  const ReferenceParams&				sampleParams,
							  const tcu::LookupPrecision&			lookupPrec,
							  const tcu::LodPrecision&				lodPrec,
							  qpWatchDog*							watchDog)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::Texture2DArrayView				src					= getEffectiveTextureView(baseView, srcLevelStorage, sampleParams.sampler);

	const tcu::Vec4								sq					= tcu::Vec4(texCoord[0+0], texCoord[3+0], texCoord[6+0], texCoord[9+0]);
	const tcu::Vec4								tq					= tcu::Vec4(texCoord[0+1], texCoord[3+1], texCoord[6+1], texCoord[9+1]);
	const tcu::Vec4								rq					= tcu::Vec4(texCoord[0+2], texCoord[3+2], texCoord[6+2], texCoord[9+2]);

	const tcu::IVec2							dstSize				= tcu::IVec2(result.getWidth(), result.getHeight());
	const float									dstW				= float(dstSize.x());
	const float									dstH				= float(dstSize.y());
	const tcu::Vec2								srcSize				= tcu::IVec2(src.getWidth(), src.getHeight()).asFloat(); // For lod computation, thus #layers is ignored.

	// Coordinates and lod per triangle.
	const tcu::Vec3								triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3								triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3								triR[2]				= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	const tcu::Vec3								triW[2]				= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2								lodBias				((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	int											numFailed			= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		// Ugly hack, validation can take way too long at the moment.
		if (watchDog)
			qpWatchDog_touch(watchDog);

		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= (result.getPixel(px, py)		- sampleParams.colorBias) / sampleParams.colorScale;
			const tcu::Vec4	refPix	= (reference.getPixel(px, py)	- sampleParams.colorBias) / sampleParams.colorScale;

			// Try comparison to ideal reference first, and if that fails use slower verificator.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(resPix - refPix), lookupPrec.colorThreshold)))
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
				const float		triWx	= triNdx ? dstW - wx : wx;
				const float		triWy	= triNdx ? dstH - wy : wy;
				const float		triNx	= triNdx ? 1.0f - nx : nx;
				const float		triNy	= triNdx ? 1.0f - ny : ny;

				const tcu::Vec3	coord		(projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy),
											 projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy),
											 projectedTriInterpolate(triR[triNdx], triW[triNdx], triNx, triNy));
				const tcu::Vec2	coordDx		= tcu::Vec2(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
														triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy)) * srcSize;
				const tcu::Vec2	coordDy		= tcu::Vec2(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
														triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx)) * srcSize;

				tcu::Vec2		lodBounds	= tcu::computeLodBoundsFromDerivates(coordDx.x(), coordDx.y(), coordDy.x(), coordDy.y(), lodPrec);

				// Compute lod bounds across lodOffsets range.
				for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
				{
					const float		wxo		= triWx + lodOffsets[lodOffsNdx].x();
					const float		wyo		= triWy + lodOffsets[lodOffsNdx].y();
					const float		nxo		= wxo/dstW;
					const float		nyo		= wyo/dstH;

					const tcu::Vec2	coordDxo	= tcu::Vec2(triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo),
															triDerivateX(triT[triNdx], triW[triNdx], wxo, dstW, nyo)) * srcSize;
					const tcu::Vec2	coordDyo	= tcu::Vec2(triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo),
															triDerivateY(triT[triNdx], triW[triNdx], wyo, dstH, nxo)) * srcSize;
					const tcu::Vec2	lodO		= tcu::computeLodBoundsFromDerivates(coordDxo.x(), coordDxo.y(), coordDyo.x(), coordDyo.y(), lodPrec);

					lodBounds.x() = de::min(lodBounds.x(), lodO.x());
					lodBounds.y() = de::max(lodBounds.y(), lodO.y());
				}

				const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);
				const bool		isOk		= tcu::isLookupResultValid(src, sampleParams.sampler, lookupPrec, coord, clampedLod, resPix);

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

bool verifyTextureResult (tcu::TestContext&						testCtx,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::Texture1DArrayView&		src,
						  const float*							texCoord,
						  const ReferenceParams&				sampleParams,
						  const tcu::LookupPrecision&			lookupPrec,
						  const tcu::LodPrecision&				lodPrec,
						  const tcu::PixelFormat&				pixelFormat)
{
	tcu::TestLog&	log				= testCtx.getLog();
	tcu::Surface	reference		(result.getWidth(), result.getHeight());
	tcu::Surface	errorMask		(result.getWidth(), result.getHeight());
	int				numFailedPixels;

	DE_ASSERT(getCompareMask(pixelFormat) == lookupPrec.colorMask);

	sampleTexture(SurfaceAccess(reference, pixelFormat), src, texCoord, sampleParams);
	numFailedPixels = computeTextureLookupDiff(result, reference.getAccess(), errorMask.getAccess(), src, texCoord, sampleParams, lookupPrec, lodPrec, testCtx.getWatchDog());

	if (numFailedPixels > 0)
		log << TestLog::Message << "ERROR: Result verification failed, got " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

	log << TestLog::ImageSet("VerifyResult", "Verification result")
		<< TestLog::Image("Rendered", "Rendered image", result);

	if (numFailedPixels > 0)
	{
		log << TestLog::Image("Reference", "Ideal reference image", reference)
			<< TestLog::Image("ErrorMask", "Error mask", errorMask);
	}

	log << TestLog::EndImageSet;

	return numFailedPixels == 0;
}

bool verifyTextureResult (tcu::TestContext&						testCtx,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::Texture2DArrayView&		src,
						  const float*							texCoord,
						  const ReferenceParams&				sampleParams,
						  const tcu::LookupPrecision&			lookupPrec,
						  const tcu::LodPrecision&				lodPrec,
						  const tcu::PixelFormat&				pixelFormat)
{
	tcu::TestLog&	log				= testCtx.getLog();
	tcu::Surface	reference		(result.getWidth(), result.getHeight());
	tcu::Surface	errorMask		(result.getWidth(), result.getHeight());
	int				numFailedPixels;

	DE_ASSERT(getCompareMask(pixelFormat) == lookupPrec.colorMask);

	sampleTexture(SurfaceAccess(reference, pixelFormat), src, texCoord, sampleParams);
	numFailedPixels = computeTextureLookupDiff(result, reference.getAccess(), errorMask.getAccess(), src, texCoord, sampleParams, lookupPrec, lodPrec, testCtx.getWatchDog());

	if (numFailedPixels > 0)
		log << TestLog::Message << "ERROR: Result verification failed, got " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

	log << TestLog::ImageSet("VerifyResult", "Verification result")
		<< TestLog::Image("Rendered", "Rendered image", result);

	if (numFailedPixels > 0)
	{
		log << TestLog::Image("Reference", "Ideal reference image", reference)
			<< TestLog::Image("ErrorMask", "Error mask", errorMask);
	}

	log << TestLog::EndImageSet;

	return numFailedPixels == 0;
}

//! Verifies texture lookup results and returns number of failed pixels.
int computeTextureLookupDiff (const tcu::ConstPixelBufferAccess&	result,
							  const tcu::ConstPixelBufferAccess&	reference,
							  const tcu::PixelBufferAccess&			errorMask,
							  const tcu::TextureCubeArrayView&		baseView,
							  const float*							texCoord,
							  const ReferenceParams&				sampleParams,
							  const tcu::LookupPrecision&			lookupPrec,
							  const tcu::IVec4&						coordBits,
							  const tcu::LodPrecision&				lodPrec,
							  qpWatchDog*							watchDog)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	std::vector<tcu::ConstPixelBufferAccess>	srcLevelStorage;
	const tcu::TextureCubeArrayView				src					= getEffectiveTextureView(getSubView(baseView, sampleParams.baseLevel, sampleParams.maxLevel), srcLevelStorage, sampleParams.sampler);

	const tcu::Vec4								sq					= tcu::Vec4(texCoord[0+0], texCoord[4+0], texCoord[8+0], texCoord[12+0]);
	const tcu::Vec4								tq					= tcu::Vec4(texCoord[0+1], texCoord[4+1], texCoord[8+1], texCoord[12+1]);
	const tcu::Vec4								rq					= tcu::Vec4(texCoord[0+2], texCoord[4+2], texCoord[8+2], texCoord[12+2]);
	const tcu::Vec4								qq					= tcu::Vec4(texCoord[0+3], texCoord[4+3], texCoord[8+3], texCoord[12+3]);

	const tcu::IVec2							dstSize				= tcu::IVec2(result.getWidth(), result.getHeight());
	const float									dstW				= float(dstSize.x());
	const float									dstH				= float(dstSize.y());
	const int									srcSize				= src.getSize();

	// Coordinates per triangle.
	const tcu::Vec3								triS[2]				= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3								triT[2]				= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3								triR[2]				= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	const tcu::Vec3								triQ[2]				= { qq.swizzle(0, 1, 2), qq.swizzle(3, 2, 1) };
	const tcu::Vec3								triW[2]				= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2								lodBias				((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	const float									posEps				= 1.0f / float((1<<4) + 1); // ES3 requires at least 4 subpixel bits.

	int											numFailed			= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),

		// \note Not strictly allowed by spec, but implementations do this in practice.
		tcu::Vec2(-1, -1),
		tcu::Vec2(-1, +1),
		tcu::Vec2(+1, -1),
		tcu::Vec2(+1, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		// Ugly hack, validation can take way too long at the moment.
		if (watchDog)
			qpWatchDog_touch(watchDog);

		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= (result.getPixel(px, py)		- sampleParams.colorBias) / sampleParams.colorScale;
			const tcu::Vec4	refPix	= (reference.getPixel(px, py)	- sampleParams.colorBias) / sampleParams.colorScale;

			// Try comparison to ideal reference first, and if that fails use slower verificator.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(resPix - refPix), lookupPrec.colorThreshold)))
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const bool		tri0	= nx + ny - posEps <= 1.0f;
				const bool		tri1	= nx + ny + posEps >= 1.0f;

				bool			isOk	= false;

				DE_ASSERT(tri0 || tri1);

				// Pixel can belong to either of the triangles if it lies close enough to the edge.
				for (int triNdx = (tri0?0:1); triNdx <= (tri1?1:0); triNdx++)
				{
					const float		triWx		= triNdx ? dstW - wx : wx;
					const float		triWy		= triNdx ? dstH - wy : wy;
					const float		triNx		= triNdx ? 1.0f - nx : nx;
					const float		triNy		= triNdx ? 1.0f - ny : ny;

					const tcu::Vec4	coord		(projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy),
												 projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy),
												 projectedTriInterpolate(triR[triNdx], triW[triNdx], triNx, triNy),
												 projectedTriInterpolate(triQ[triNdx], triW[triNdx], triNx, triNy));
					const tcu::Vec3	coordDx		(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
												 triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy),
												 triDerivateX(triR[triNdx], triW[triNdx], wx, dstW, triNy));
					const tcu::Vec3	coordDy		(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
												 triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx),
												 triDerivateY(triR[triNdx], triW[triNdx], wy, dstH, triNx));

					tcu::Vec2		lodBounds	= tcu::computeCubeLodBoundsFromDerivates(coord.toWidth<3>(), coordDx, coordDy, srcSize, lodPrec);

					// Compute lod bounds across lodOffsets range.
					for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
					{
						const float		wxo			= triWx + lodOffsets[lodOffsNdx].x();
						const float		wyo			= triWy + lodOffsets[lodOffsNdx].y();
						const float		nxo			= wxo/dstW;
						const float		nyo			= wyo/dstH;

						const tcu::Vec3	coordO		(projectedTriInterpolate(triS[triNdx], triW[triNdx], nxo, nyo),
													 projectedTriInterpolate(triT[triNdx], triW[triNdx], nxo, nyo),
													 projectedTriInterpolate(triR[triNdx], triW[triNdx], nxo, nyo));
						const tcu::Vec3	coordDxo	(triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo),
													 triDerivateX(triT[triNdx], triW[triNdx], wxo, dstW, nyo),
													 triDerivateX(triR[triNdx], triW[triNdx], wxo, dstW, nyo));
						const tcu::Vec3	coordDyo	(triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo),
													 triDerivateY(triT[triNdx], triW[triNdx], wyo, dstH, nxo),
													 triDerivateY(triR[triNdx], triW[triNdx], wyo, dstH, nxo));
						const tcu::Vec2	lodO		= tcu::computeCubeLodBoundsFromDerivates(coordO, coordDxo, coordDyo, srcSize, lodPrec);

						lodBounds.x() = de::min(lodBounds.x(), lodO.x());
						lodBounds.y() = de::max(lodBounds.y(), lodO.y());
					}

					const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);

					if (tcu::isLookupResultValid(src, sampleParams.sampler, lookupPrec, coordBits, coord, clampedLod, resPix))
					{
						isOk = true;
						break;
					}
				}

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

bool verifyTextureResult (tcu::TestContext&						testCtx,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::TextureCubeArrayView&		src,
						  const float*							texCoord,
						  const ReferenceParams&				sampleParams,
						  const tcu::LookupPrecision&			lookupPrec,
						  const tcu::IVec4&						coordBits,
						  const tcu::LodPrecision&				lodPrec,
						  const tcu::PixelFormat&				pixelFormat)
{
	tcu::TestLog&	log				= testCtx.getLog();
	tcu::Surface	reference		(result.getWidth(), result.getHeight());
	tcu::Surface	errorMask		(result.getWidth(), result.getHeight());
	int				numFailedPixels;

	DE_ASSERT(getCompareMask(pixelFormat) == lookupPrec.colorMask);

	sampleTexture(SurfaceAccess(reference, pixelFormat), src, texCoord, sampleParams);
	numFailedPixels = computeTextureLookupDiff(result, reference.getAccess(), errorMask.getAccess(), src, texCoord, sampleParams, lookupPrec, coordBits, lodPrec, testCtx.getWatchDog());

	if (numFailedPixels > 0)
		log << TestLog::Message << "ERROR: Result verification failed, got " << numFailedPixels << " invalid pixels!" << TestLog::EndMessage;

	log << TestLog::ImageSet("VerifyResult", "Verification result")
		<< TestLog::Image("Rendered", "Rendered image", result);

	if (numFailedPixels > 0)
	{
		log << TestLog::Image("Reference", "Ideal reference image", reference)
			<< TestLog::Image("ErrorMask", "Error mask", errorMask);
	}

	log << TestLog::EndImageSet;

	return numFailedPixels == 0;
}

// Shadow lookup verification

int computeTextureCompareDiff (const tcu::ConstPixelBufferAccess&	result,
							   const tcu::ConstPixelBufferAccess&	reference,
							   const tcu::PixelBufferAccess&		errorMask,
							   const tcu::Texture2DView&			src,
							   const float*							texCoord,
							   const ReferenceParams&				sampleParams,
							   const tcu::TexComparePrecision&		comparePrec,
							   const tcu::LodPrecision&				lodPrec,
							   const tcu::Vec3&						nonShadowThreshold)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	const tcu::Vec4		sq				= tcu::Vec4(texCoord[0+0], texCoord[2+0], texCoord[4+0], texCoord[6+0]);
	const tcu::Vec4		tq				= tcu::Vec4(texCoord[0+1], texCoord[2+1], texCoord[4+1], texCoord[6+1]);

	const tcu::IVec2	dstSize			= tcu::IVec2(result.getWidth(), result.getHeight());
	const float			dstW			= float(dstSize.x());
	const float			dstH			= float(dstSize.y());
	const tcu::IVec2	srcSize			= tcu::IVec2(src.getWidth(), src.getHeight());

	// Coordinates and lod per triangle.
	const tcu::Vec3		triS[2]			= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3		triT[2]			= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3		triW[2]			= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2		lodBias			((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	int					numFailed		= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= result.getPixel(px, py);
			const tcu::Vec4	refPix	= reference.getPixel(px, py);

			// Other channels should trivially match to reference.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(refPix.swizzle(1,2,3) - resPix.swizzle(1,2,3)), nonShadowThreshold)))
			{
				errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
				numFailed += 1;
				continue;
			}

			// Reference result is known to be a valid result, we can
			// skip verification if thes results are equal
			if (resPix.x() != refPix.x())
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
				const float		triWx	= triNdx ? dstW - wx : wx;
				const float		triWy	= triNdx ? dstH - wy : wy;
				const float		triNx	= triNdx ? 1.0f - nx : nx;
				const float		triNy	= triNdx ? 1.0f - ny : ny;

				const tcu::Vec2	coord		(projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy),
											 projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy));
				const tcu::Vec2	coordDx		= tcu::Vec2(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
														triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy)) * srcSize.asFloat();
				const tcu::Vec2	coordDy		= tcu::Vec2(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
														triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx)) * srcSize.asFloat();

				tcu::Vec2		lodBounds	= tcu::computeLodBoundsFromDerivates(coordDx.x(), coordDx.y(), coordDy.x(), coordDy.y(), lodPrec);

				// Compute lod bounds across lodOffsets range.
				for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
				{
					const float		wxo		= triWx + lodOffsets[lodOffsNdx].x();
					const float		wyo		= triWy + lodOffsets[lodOffsNdx].y();
					const float		nxo		= wxo/dstW;
					const float		nyo		= wyo/dstH;

					const tcu::Vec2	coordDxo	= tcu::Vec2(triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo),
															triDerivateX(triT[triNdx], triW[triNdx], wxo, dstW, nyo)) * srcSize.asFloat();
					const tcu::Vec2	coordDyo	= tcu::Vec2(triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo),
															triDerivateY(triT[triNdx], triW[triNdx], wyo, dstH, nxo)) * srcSize.asFloat();
					const tcu::Vec2	lodO		= tcu::computeLodBoundsFromDerivates(coordDxo.x(), coordDxo.y(), coordDyo.x(), coordDyo.y(), lodPrec);

					lodBounds.x() = de::min(lodBounds.x(), lodO.x());
					lodBounds.y() = de::max(lodBounds.y(), lodO.y());
				}

				const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);
				const bool		isOk		= tcu::isTexCompareResultValid(src, sampleParams.sampler, comparePrec, coord, clampedLod, sampleParams.ref, resPix.x());

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

int computeTextureCompareDiff (const tcu::ConstPixelBufferAccess&	result,
							   const tcu::ConstPixelBufferAccess&	reference,
							   const tcu::PixelBufferAccess&		errorMask,
							   const tcu::TextureCubeView&			src,
							   const float*							texCoord,
							   const ReferenceParams&				sampleParams,
							   const tcu::TexComparePrecision&		comparePrec,
							   const tcu::LodPrecision&				lodPrec,
							   const tcu::Vec3&						nonShadowThreshold)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	const tcu::Vec4		sq				= tcu::Vec4(texCoord[0+0], texCoord[3+0], texCoord[6+0], texCoord[9+0]);
	const tcu::Vec4		tq				= tcu::Vec4(texCoord[0+1], texCoord[3+1], texCoord[6+1], texCoord[9+1]);
	const tcu::Vec4		rq				= tcu::Vec4(texCoord[0+2], texCoord[3+2], texCoord[6+2], texCoord[9+2]);

	const tcu::IVec2	dstSize			= tcu::IVec2(result.getWidth(), result.getHeight());
	const float			dstW			= float(dstSize.x());
	const float			dstH			= float(dstSize.y());
	const int			srcSize			= src.getSize();

	// Coordinates per triangle.
	const tcu::Vec3		triS[2]			= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3		triT[2]			= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3		triR[2]			= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	const tcu::Vec3		triW[2]			= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2		lodBias			((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	int					numFailed		= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= result.getPixel(px, py);
			const tcu::Vec4	refPix	= reference.getPixel(px, py);

			// Other channels should trivially match to reference.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(refPix.swizzle(1,2,3) - resPix.swizzle(1,2,3)), nonShadowThreshold)))
			{
				errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
				numFailed += 1;
				continue;
			}

			// Reference result is known to be a valid result, we can
			// skip verification if thes results are equal
			if (resPix.x() != refPix.x())
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
				const float		triWx	= triNdx ? dstW - wx : wx;
				const float		triWy	= triNdx ? dstH - wy : wy;
				const float		triNx	= triNdx ? 1.0f - nx : nx;
				const float		triNy	= triNdx ? 1.0f - ny : ny;

				const tcu::Vec3	coord		(projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy),
											 projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy),
											 projectedTriInterpolate(triR[triNdx], triW[triNdx], triNx, triNy));
				const tcu::Vec3	coordDx		(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
											 triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy),
											 triDerivateX(triR[triNdx], triW[triNdx], wx, dstW, triNy));
				const tcu::Vec3	coordDy		(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
											 triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx),
											 triDerivateY(triR[triNdx], triW[triNdx], wy, dstH, triNx));

				tcu::Vec2		lodBounds	= tcu::computeCubeLodBoundsFromDerivates(coord, coordDx, coordDy, srcSize, lodPrec);

				// Compute lod bounds across lodOffsets range.
				for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
				{
					const float		wxo		= triWx + lodOffsets[lodOffsNdx].x();
					const float		wyo		= triWy + lodOffsets[lodOffsNdx].y();
					const float		nxo		= wxo/dstW;
					const float		nyo		= wyo/dstH;

					const tcu::Vec3	coordO		(projectedTriInterpolate(triS[triNdx], triW[triNdx], nxo, nyo),
												 projectedTriInterpolate(triT[triNdx], triW[triNdx], nxo, nyo),
												 projectedTriInterpolate(triR[triNdx], triW[triNdx], nxo, nyo));
					const tcu::Vec3	coordDxo	(triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo),
												 triDerivateX(triT[triNdx], triW[triNdx], wxo, dstW, nyo),
												 triDerivateX(triR[triNdx], triW[triNdx], wxo, dstW, nyo));
					const tcu::Vec3	coordDyo	(triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo),
												 triDerivateY(triT[triNdx], triW[triNdx], wyo, dstH, nxo),
												 triDerivateY(triR[triNdx], triW[triNdx], wyo, dstH, nxo));
					const tcu::Vec2	lodO		= tcu::computeCubeLodBoundsFromDerivates(coordO, coordDxo, coordDyo, srcSize, lodPrec);

					lodBounds.x() = de::min(lodBounds.x(), lodO.x());
					lodBounds.y() = de::max(lodBounds.y(), lodO.y());
				}

				const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);
				const bool		isOk		= tcu::isTexCompareResultValid(src, sampleParams.sampler, comparePrec, coord, clampedLod, sampleParams.ref, resPix.x());

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

int computeTextureCompareDiff (const tcu::ConstPixelBufferAccess&	result,
							   const tcu::ConstPixelBufferAccess&	reference,
							   const tcu::PixelBufferAccess&		errorMask,
							   const tcu::Texture2DArrayView&		src,
							   const float*							texCoord,
							   const ReferenceParams&				sampleParams,
							   const tcu::TexComparePrecision&		comparePrec,
							   const tcu::LodPrecision&				lodPrec,
							   const tcu::Vec3&						nonShadowThreshold)
{
	DE_ASSERT(result.getWidth() == reference.getWidth() && result.getHeight() == reference.getHeight());
	DE_ASSERT(result.getWidth() == errorMask.getWidth() && result.getHeight() == errorMask.getHeight());

	const tcu::Vec4		sq				= tcu::Vec4(texCoord[0+0], texCoord[3+0], texCoord[6+0], texCoord[9+0]);
	const tcu::Vec4		tq				= tcu::Vec4(texCoord[0+1], texCoord[3+1], texCoord[6+1], texCoord[9+1]);
	const tcu::Vec4		rq				= tcu::Vec4(texCoord[0+2], texCoord[3+2], texCoord[6+2], texCoord[9+2]);

	const tcu::IVec2	dstSize			= tcu::IVec2(result.getWidth(), result.getHeight());
	const float			dstW			= float(dstSize.x());
	const float			dstH			= float(dstSize.y());
	const tcu::IVec2	srcSize			= tcu::IVec2(src.getWidth(), src.getHeight());

	// Coordinates and lod per triangle.
	const tcu::Vec3		triS[2]			= { sq.swizzle(0, 1, 2), sq.swizzle(3, 2, 1) };
	const tcu::Vec3		triT[2]			= { tq.swizzle(0, 1, 2), tq.swizzle(3, 2, 1) };
	const tcu::Vec3		triR[2]			= { rq.swizzle(0, 1, 2), rq.swizzle(3, 2, 1) };
	const tcu::Vec3		triW[2]			= { sampleParams.w.swizzle(0, 1, 2), sampleParams.w.swizzle(3, 2, 1) };

	const tcu::Vec2		lodBias			((sampleParams.flags & ReferenceParams::USE_BIAS) ? sampleParams.bias : 0.0f);

	int					numFailed		= 0;

	const tcu::Vec2 lodOffsets[] =
	{
		tcu::Vec2(-1,  0),
		tcu::Vec2(+1,  0),
		tcu::Vec2( 0, -1),
		tcu::Vec2( 0, +1),
	};

	tcu::clear(errorMask, tcu::RGBA::green().toVec());

	for (int py = 0; py < result.getHeight(); py++)
	{
		for (int px = 0; px < result.getWidth(); px++)
		{
			const tcu::Vec4	resPix	= result.getPixel(px, py);
			const tcu::Vec4	refPix	= reference.getPixel(px, py);

			// Other channels should trivially match to reference.
			if (!tcu::boolAll(tcu::lessThanEqual(tcu::abs(refPix.swizzle(1,2,3) - resPix.swizzle(1,2,3)), nonShadowThreshold)))
			{
				errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
				numFailed += 1;
				continue;
			}

			// Reference result is known to be a valid result, we can
			// skip verification if thes results are equal
			if (resPix.x() != refPix.x())
			{
				const float		wx		= (float)px + 0.5f;
				const float		wy		= (float)py + 0.5f;
				const float		nx		= wx / dstW;
				const float		ny		= wy / dstH;

				const int		triNdx	= nx + ny >= 1.0f ? 1 : 0;
				const float		triWx	= triNdx ? dstW - wx : wx;
				const float		triWy	= triNdx ? dstH - wy : wy;
				const float		triNx	= triNdx ? 1.0f - nx : nx;
				const float		triNy	= triNdx ? 1.0f - ny : ny;

				const tcu::Vec3	coord		(projectedTriInterpolate(triS[triNdx], triW[triNdx], triNx, triNy),
											 projectedTriInterpolate(triT[triNdx], triW[triNdx], triNx, triNy),
											 projectedTriInterpolate(triR[triNdx], triW[triNdx], triNx, triNy));
				const tcu::Vec2	coordDx		= tcu::Vec2(triDerivateX(triS[triNdx], triW[triNdx], wx, dstW, triNy),
														triDerivateX(triT[triNdx], triW[triNdx], wx, dstW, triNy)) * srcSize.asFloat();
				const tcu::Vec2	coordDy		= tcu::Vec2(triDerivateY(triS[triNdx], triW[triNdx], wy, dstH, triNx),
														triDerivateY(triT[triNdx], triW[triNdx], wy, dstH, triNx)) * srcSize.asFloat();

				tcu::Vec2		lodBounds	= tcu::computeLodBoundsFromDerivates(coordDx.x(), coordDx.y(), coordDy.x(), coordDy.y(), lodPrec);

				// Compute lod bounds across lodOffsets range.
				for (int lodOffsNdx = 0; lodOffsNdx < DE_LENGTH_OF_ARRAY(lodOffsets); lodOffsNdx++)
				{
					const float		wxo		= triWx + lodOffsets[lodOffsNdx].x();
					const float		wyo		= triWy + lodOffsets[lodOffsNdx].y();
					const float		nxo		= wxo/dstW;
					const float		nyo		= wyo/dstH;

					const tcu::Vec2	coordDxo	= tcu::Vec2(triDerivateX(triS[triNdx], triW[triNdx], wxo, dstW, nyo),
															triDerivateX(triT[triNdx], triW[triNdx], wxo, dstW, nyo)) * srcSize.asFloat();
					const tcu::Vec2	coordDyo	= tcu::Vec2(triDerivateY(triS[triNdx], triW[triNdx], wyo, dstH, nxo),
															triDerivateY(triT[triNdx], triW[triNdx], wyo, dstH, nxo)) * srcSize.asFloat();
					const tcu::Vec2	lodO		= tcu::computeLodBoundsFromDerivates(coordDxo.x(), coordDxo.y(), coordDyo.x(), coordDyo.y(), lodPrec);

					lodBounds.x() = de::min(lodBounds.x(), lodO.x());
					lodBounds.y() = de::max(lodBounds.y(), lodO.y());
				}

				const tcu::Vec2	clampedLod	= tcu::clampLodBounds(lodBounds + lodBias, tcu::Vec2(sampleParams.minLod, sampleParams.maxLod), lodPrec);
				const bool		isOk		= tcu::isTexCompareResultValid(src, sampleParams.sampler, comparePrec, coord, clampedLod, sampleParams.ref, resPix.x());

				if (!isOk)
				{
					errorMask.setPixel(tcu::RGBA::red().toVec(), px, py);
					numFailed += 1;
				}
			}
		}
	}

	return numFailed;
}

// Mipmap generation comparison.

static int compareGenMipmapBilinear (const tcu::ConstPixelBufferAccess& dst, const tcu::ConstPixelBufferAccess& src, const tcu::PixelBufferAccess& errorMask, const GenMipmapPrecision& precision)
{
	DE_ASSERT(dst.getDepth() == 1 && src.getDepth() == 1); // \todo [2013-10-29 pyry] 3D textures.

	const float		dstW		= float(dst.getWidth());
	const float		dstH		= float(dst.getHeight());
	const float		srcW		= float(src.getWidth());
	const float		srcH		= float(src.getHeight());
	int				numFailed	= 0;

	// Translation to lookup verification parameters.
	const tcu::Sampler		sampler		(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
										 tcu::Sampler::LINEAR, tcu::Sampler::LINEAR, 0.0f, false /* non-normalized coords */);
	tcu::LookupPrecision	lookupPrec;

	lookupPrec.colorThreshold	= precision.colorThreshold;
	lookupPrec.colorMask		= precision.colorMask;
	lookupPrec.coordBits		= tcu::IVec3(22);
	lookupPrec.uvwBits			= precision.filterBits;

	for (int y = 0; y < dst.getHeight(); y++)
	for (int x = 0; x < dst.getWidth(); x++)
	{
		const tcu::Vec4	result	= dst.getPixel(x, y);
		const float		cx		= (float(x)+0.5f) / dstW * srcW;
		const float		cy		= (float(y)+0.5f) / dstH * srcH;
		const bool		isOk	= tcu::isLinearSampleResultValid(src, sampler, lookupPrec, tcu::Vec2(cx, cy), 0, result);

		errorMask.setPixel(isOk ? tcu::RGBA::green().toVec() : tcu::RGBA::red().toVec(), x, y);
		if (!isOk)
			numFailed += 1;
	}

	return numFailed;
}

static int compareGenMipmapBox (const tcu::ConstPixelBufferAccess& dst, const tcu::ConstPixelBufferAccess& src, const tcu::PixelBufferAccess& errorMask, const GenMipmapPrecision& precision)
{
	DE_ASSERT(dst.getDepth() == 1 && src.getDepth() == 1); // \todo [2013-10-29 pyry] 3D textures.

	const float		dstW		= float(dst.getWidth());
	const float		dstH		= float(dst.getHeight());
	const float		srcW		= float(src.getWidth());
	const float		srcH		= float(src.getHeight());
	int				numFailed	= 0;

	// Translation to lookup verification parameters.
	const tcu::Sampler		sampler		(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
										 tcu::Sampler::LINEAR, tcu::Sampler::LINEAR, 0.0f, false /* non-normalized coords */);
	tcu::LookupPrecision	lookupPrec;

	lookupPrec.colorThreshold	= precision.colorThreshold;
	lookupPrec.colorMask		= precision.colorMask;
	lookupPrec.coordBits		= tcu::IVec3(22);
	lookupPrec.uvwBits			= precision.filterBits;

	for (int y = 0; y < dst.getHeight(); y++)
	for (int x = 0; x < dst.getWidth(); x++)
	{
		const tcu::Vec4	result	= dst.getPixel(x, y);
		const float		cx		= deFloatFloor(float(x) / dstW * srcW) + 1.0f;
		const float		cy		= deFloatFloor(float(y) / dstH * srcH) + 1.0f;
		const bool		isOk	= tcu::isLinearSampleResultValid(src, sampler, lookupPrec, tcu::Vec2(cx, cy), 0, result);

		errorMask.setPixel(isOk ? tcu::RGBA::green().toVec() : tcu::RGBA::red().toVec(), x, y);
		if (!isOk)
			numFailed += 1;
	}

	return numFailed;
}

static int compareGenMipmapVeryLenient (const tcu::ConstPixelBufferAccess& dst, const tcu::ConstPixelBufferAccess& src, const tcu::PixelBufferAccess& errorMask, const GenMipmapPrecision& precision)
{
	DE_ASSERT(dst.getDepth() == 1 && src.getDepth() == 1); // \todo [2013-10-29 pyry] 3D textures.
	DE_UNREF(precision);

	const float		dstW		= float(dst.getWidth());
	const float		dstH		= float(dst.getHeight());
	const float		srcW		= float(src.getWidth());
	const float		srcH		= float(src.getHeight());
	int				numFailed	= 0;

	for (int y = 0; y < dst.getHeight(); y++)
	for (int x = 0; x < dst.getWidth(); x++)
	{
		const tcu::Vec4	result	= dst.getPixel(x, y);
		const int		minX		= deFloorFloatToInt32(((float)x-0.5f) / dstW * srcW);
		const int		minY		= deFloorFloatToInt32(((float)y-0.5f) / dstH * srcH);
		const int		maxX		= deCeilFloatToInt32(((float)x+1.5f) / dstW * srcW);
		const int		maxY		= deCeilFloatToInt32(((float)y+1.5f) / dstH * srcH);
		tcu::Vec4		minVal, maxVal;
		bool			isOk;

		DE_ASSERT(minX < maxX && minY < maxY);

		for (int ky = minY; ky <= maxY; ky++)
		{
			for (int kx = minX; kx <= maxX; kx++)
			{
				const int		sx		= de::clamp(kx, 0, src.getWidth()-1);
				const int		sy		= de::clamp(ky, 0, src.getHeight()-1);
				const tcu::Vec4	sample	= src.getPixel(sx, sy);

				if (ky == minY && kx == minX)
				{
					minVal = sample;
					maxVal = sample;
				}
				else
				{
					minVal = min(sample, minVal);
					maxVal = max(sample, maxVal);
				}
			}
		}

		isOk = boolAll(logicalAnd(lessThanEqual(minVal, result), lessThanEqual(result, maxVal)));

		errorMask.setPixel(isOk ? tcu::RGBA::green().toVec() : tcu::RGBA::red().toVec(), x, y);
		if (!isOk)
			numFailed += 1;
	}

	return numFailed;
}

qpTestResult compareGenMipmapResult (tcu::TestLog& log, const tcu::Texture2D& resultTexture, const tcu::Texture2D& level0Reference, const GenMipmapPrecision& precision)
{
	qpTestResult result = QP_TEST_RESULT_PASS;

	// Special comparison for level 0.
	{
		const tcu::Vec4		threshold	= select(precision.colorThreshold, tcu::Vec4(1.0f), precision.colorMask);
		const bool			level0Ok	= tcu::floatThresholdCompare(log, "Level0", "Level 0", level0Reference.getLevel(0), resultTexture.getLevel(0), threshold, tcu::COMPARE_LOG_RESULT);

		if (!level0Ok)
		{
			log << TestLog::Message << "ERROR: Level 0 comparison failed!" << TestLog::EndMessage;
			result = QP_TEST_RESULT_FAIL;
		}
	}

	for (int levelNdx = 1; levelNdx < resultTexture.getNumLevels(); levelNdx++)
	{
		const tcu::ConstPixelBufferAccess	src			= resultTexture.getLevel(levelNdx-1);
		const tcu::ConstPixelBufferAccess	dst			= resultTexture.getLevel(levelNdx);
		tcu::Surface						errorMask	(dst.getWidth(), dst.getHeight());
		bool								levelOk		= false;

		// Try different comparisons in quality order.

		if (!levelOk)
		{
			const int numFailed = compareGenMipmapBilinear(dst, src, errorMask.getAccess(), precision);
			if (numFailed == 0)
				levelOk = true;
			else
				log << TestLog::Message << "WARNING: Level " << levelNdx << " comparison to bilinear method failed, found " << numFailed << " invalid pixels." << TestLog::EndMessage;
		}

		if (!levelOk)
		{
			const int numFailed = compareGenMipmapBox(dst, src, errorMask.getAccess(), precision);
			if (numFailed == 0)
				levelOk = true;
			else
				log << TestLog::Message << "WARNING: Level " << levelNdx << " comparison to box method failed, found " << numFailed << " invalid pixels." << TestLog::EndMessage;
		}

		// At this point all high-quality methods have been used.
		if (!levelOk && result == QP_TEST_RESULT_PASS)
			result = QP_TEST_RESULT_QUALITY_WARNING;

		if (!levelOk)
		{
			const int numFailed = compareGenMipmapVeryLenient(dst, src, errorMask.getAccess(), precision);
			if (numFailed == 0)
				levelOk = true;
			else
				log << TestLog::Message << "ERROR: Level " << levelNdx << " appears to contain " << numFailed << " completely wrong pixels, failing case!" << TestLog::EndMessage;
		}

		if (!levelOk)
			result = QP_TEST_RESULT_FAIL;

		log << TestLog::ImageSet(string("Level") + de::toString(levelNdx), string("Level ") + de::toString(levelNdx) + " result")
			<< TestLog::Image("Result", "Result", dst);

		if (!levelOk)
			log << TestLog::Image("ErrorMask", "Error mask", errorMask);

		log << TestLog::EndImageSet;
	}

	return result;
}

qpTestResult compareGenMipmapResult (tcu::TestLog& log, const tcu::TextureCube& resultTexture, const tcu::TextureCube& level0Reference, const GenMipmapPrecision& precision)
{
	qpTestResult result = QP_TEST_RESULT_PASS;

	static const char* s_faceNames[] = { "-X", "+X", "-Y", "+Y", "-Z", "+Z" };
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_faceNames) == tcu::CUBEFACE_LAST);

	// Special comparison for level 0.
	for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; faceNdx++)
	{
		const tcu::CubeFace	face		= tcu::CubeFace(faceNdx);
		const tcu::Vec4		threshold	= select(precision.colorThreshold, tcu::Vec4(1.0f), precision.colorMask);
		const bool			level0Ok	= tcu::floatThresholdCompare(log,
																	 ("Level0Face" + de::toString(faceNdx)).c_str(),
																	 (string("Level 0, face ") + s_faceNames[face]).c_str(),
																	 level0Reference.getLevelFace(0, face),
																	 resultTexture.getLevelFace(0, face),
																	 threshold, tcu::COMPARE_LOG_RESULT);

		if (!level0Ok)
		{
			log << TestLog::Message << "ERROR: Level 0, face " << s_faceNames[face] << " comparison failed!" << TestLog::EndMessage;
			result = QP_TEST_RESULT_FAIL;
		}
	}

	for (int levelNdx = 1; levelNdx < resultTexture.getNumLevels(); levelNdx++)
	{
		for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; faceNdx++)
		{
			const tcu::CubeFace					face		= tcu::CubeFace(faceNdx);
			const char*							faceName	= s_faceNames[face];
			const tcu::ConstPixelBufferAccess	src			= resultTexture.getLevelFace(levelNdx-1,	face);
			const tcu::ConstPixelBufferAccess	dst			= resultTexture.getLevelFace(levelNdx,		face);
			tcu::Surface						errorMask	(dst.getWidth(), dst.getHeight());
			bool								levelOk		= false;

			// Try different comparisons in quality order.

			if (!levelOk)
			{
				const int numFailed = compareGenMipmapBilinear(dst, src, errorMask.getAccess(), precision);
				if (numFailed == 0)
					levelOk = true;
				else
					log << TestLog::Message << "WARNING: Level " << levelNdx << ", face " << faceName << " comparison to bilinear method failed, found " << numFailed << " invalid pixels." << TestLog::EndMessage;
			}

			if (!levelOk)
			{
				const int numFailed = compareGenMipmapBox(dst, src, errorMask.getAccess(), precision);
				if (numFailed == 0)
					levelOk = true;
				else
					log << TestLog::Message << "WARNING: Level " << levelNdx << ", face " << faceName <<" comparison to box method failed, found " << numFailed << " invalid pixels." << TestLog::EndMessage;
			}

			// At this point all high-quality methods have been used.
			if (!levelOk && result == QP_TEST_RESULT_PASS)
				result = QP_TEST_RESULT_QUALITY_WARNING;

			if (!levelOk)
			{
				const int numFailed = compareGenMipmapVeryLenient(dst, src, errorMask.getAccess(), precision);
				if (numFailed == 0)
					levelOk = true;
				else
					log << TestLog::Message << "ERROR: Level " << levelNdx << ", face " << faceName << " appears to contain " << numFailed << " completely wrong pixels, failing case!" << TestLog::EndMessage;
			}

			if (!levelOk)
				result = QP_TEST_RESULT_FAIL;

			log << TestLog::ImageSet(string("Level") + de::toString(levelNdx) + "Face" + de::toString(faceNdx), string("Level ") + de::toString(levelNdx) + ", face " + string(faceName) + " result")
				<< TestLog::Image("Result", "Result", dst);

			if (!levelOk)
				log << TestLog::Image("ErrorMask", "Error mask", errorMask);

			log << TestLog::EndImageSet;
		}
	}

	return result;
}

// Logging utilities.

std::ostream& operator<< (std::ostream& str, const LogGradientFmt& fmt)
{
	return str << "(R: " << fmt.valueMin->x() << " -> " << fmt.valueMax->x() << ", "
			   <<  "G: " << fmt.valueMin->y() << " -> " << fmt.valueMax->y() << ", "
			   <<  "B: " << fmt.valueMin->z() << " -> " << fmt.valueMax->z() << ", "
			   <<  "A: " << fmt.valueMin->w() << " -> " << fmt.valueMax->w() << ")";
}

} // TextureTestUtil
} // gls
} // deqp
