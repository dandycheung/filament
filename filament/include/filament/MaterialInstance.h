/*
 * Copyright (C) 2015 The Android Open Source Project
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
 */

#ifndef TNT_FILAMENT_MATERIALINSTANCE_H
#define TNT_FILAMENT_MATERIALINSTANCE_H

#include <filament/FilamentAPI.h>
#include <filament/Color.h>
#include <filament/Engine.h>
#include <filament/MaterialEnums.h>

#include <backend/DriverEnums.h>

#include <utils/compiler.h>

#include <math/mathfwd.h>

#include <type_traits>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace filament {

class Material;
class Texture;
class TextureSampler;
class UniformBuffer;
class BufferInterfaceBlock;

class UTILS_PUBLIC MaterialInstance : public FilamentAPI {
    template<size_t N>
    using StringLiteralHelper = const char[N];

    struct StringLiteral {
        const char* UTILS_NONNULL data;
        size_t size;
        template<size_t N>
        StringLiteral(StringLiteralHelper<N> const& s) noexcept // NOLINT(google-explicit-constructor)
                : data(s), size(N - 1) {
        }
    };

public:
    using CullingMode = backend::CullingMode;
    // ReSharper disable once CppRedundantQualifier
    using TransparencyMode = filament::TransparencyMode;
    using DepthFunc = backend::SamplerCompareFunc;
    using StencilCompareFunc = backend::SamplerCompareFunc;
    using StencilOperation = backend::StencilOperation;
    using StencilFace = backend::StencilFace;

    template<typename T>
    using is_supported_parameter_t = std::enable_if_t<
            std::is_same_v<float, T> ||
            std::is_same_v<int32_t, T> ||
            std::is_same_v<uint32_t, T> ||
            std::is_same_v<math::int2, T> ||
            std::is_same_v<math::int3, T> ||
            std::is_same_v<math::int4, T> ||
            std::is_same_v<math::uint2, T> ||
            std::is_same_v<math::uint3, T> ||
            std::is_same_v<math::uint4, T> ||
            std::is_same_v<math::float2, T> ||
            std::is_same_v<math::float3, T> ||
            std::is_same_v<math::float4, T> ||
            std::is_same_v<math::mat4f, T> ||
            // these types are slower as they need a layout conversion
            std::is_same_v<bool, T> ||
            std::is_same_v<math::bool2, T> ||
            std::is_same_v<math::bool3, T> ||
            std::is_same_v<math::bool4, T> ||
            std::is_same_v<math::mat3f, T>
    >;

    /**
     * Creates a new MaterialInstance using another MaterialInstance as a template for initialization.
     * The new MaterialInstance is an instance of the same Material of the template instance and
     * must be destroyed just like any other MaterialInstance.
     *
     * @param other A MaterialInstance to use as a template for initializing a new instance
     * @param name  A name for the new MaterialInstance or nullptr to use the template's name
     * @return      A new MaterialInstance
     */
    static MaterialInstance* UTILS_NONNULL duplicate(MaterialInstance const* UTILS_NONNULL other,
            const char* UTILS_NULLABLE name = nullptr) noexcept;

    /**
     * @return the Material associated with this instance
     */
    Material const* UTILS_NONNULL getMaterial() const noexcept;

    /**
     * @return the name associated with this instance
     */
    const char* UTILS_NONNULL getName() const noexcept;

    /**
     * Set a uniform by name
     *
     * @param name          Name of the parameter as defined by Material. Cannot be nullptr.
     * @param nameLength    Length in `char` of the name parameter.
     * @param value         Value of the parameter to set.
     * @throws utils::PreConditionPanic if name doesn't exist or no-op if exceptions are disabled.
     */
    template<typename T, typename = is_supported_parameter_t<T>>
    void setParameter(const char* UTILS_NONNULL name, size_t nameLength, T const& value);

    /** inline helper to provide the name as a null-terminated string literal */
    template<typename T, typename = is_supported_parameter_t<T>>
    void setParameter(StringLiteral const name, T const& value) {
        setParameter<T>(name.data, name.size, value);
    }

    /** inline helper to provide the name as a null-terminated C string */
    template<typename T, typename = is_supported_parameter_t<T>>
    void setParameter(const char* UTILS_NONNULL name, T const& value) {
        setParameter<T>(name, strlen(name), value);
    }


    /**
     * Set a uniform array by name
     *
     * @param name          Name of the parameter array as defined by Material. Cannot be nullptr.
     * @param nameLength    Length in `char` of the name parameter.
     * @param values        Array of values to set to the named parameter array.
     * @param count         Size of the array to set.
     * @throws utils::PreConditionPanic if name doesn't exist or no-op if exceptions are disabled.
     * @see Material::hasParameter
     */
    template<typename T, typename = is_supported_parameter_t<T>>
    void setParameter(const char* UTILS_NONNULL name, size_t nameLength,
            const T* UTILS_NONNULL values, size_t count);

    /** inline helper to provide the name as a null-terminated string literal */
    template<typename T, typename = is_supported_parameter_t<T>>
    void setParameter(StringLiteral const name, const T* UTILS_NONNULL values, size_t const count) {
        setParameter<T>(name.data, name.size, values, count);
    }

    /** inline helper to provide the name as a null-terminated C string */
    template<typename T, typename = is_supported_parameter_t<T>>
    void setParameter(const char* UTILS_NONNULL name,
                      const T* UTILS_NONNULL values, size_t const count) {
        setParameter<T>(name, strlen(name), values, count);
    }


    /**
     * Set a texture as the named parameter
     *
     * Note: Depth textures can't be sampled with a linear filter unless the comparison mode is set
     *       to COMPARE_TO_TEXTURE.
     *
     * @param name          Name of the parameter as defined by Material. Cannot be nullptr.
     * @param nameLength    Length in `char` of the name parameter.
     * @param texture       Non nullptr Texture object pointer.
     * @param sampler       Sampler parameters.
     * @throws utils::PreConditionPanic if name doesn't exist or no-op if exceptions are disabled.
     */
    void setParameter(const char* UTILS_NONNULL name, size_t nameLength,
            Texture const* UTILS_NULLABLE texture, TextureSampler const& sampler);

    /** inline helper to provide the name as a null-terminated string literal */
    void setParameter(StringLiteral const name,
                      Texture const* UTILS_NULLABLE texture, TextureSampler const& sampler) {
        setParameter(name.data, name.size, texture, sampler);
    }

    /** inline helper to provide the name as a null-terminated C string */
    void setParameter(const char* UTILS_NONNULL name,
                      Texture const* UTILS_NULLABLE texture, TextureSampler const& sampler) {
        setParameter(name, strlen(name), texture, sampler);
    }


    /**
     * Set an RGB color as the named parameter.
     * A conversion might occur depending on the specified type
     *
     * @param name          Name of the parameter as defined by Material. Cannot be nullptr.
     * @param nameLength    Length in `char` of the name parameter.
     * @param type          Whether the color value is encoded as Linear or sRGB.
     * @param color         Array of read, green, blue channels values.
     * @throws utils::PreConditionPanic if name doesn't exist or no-op if exceptions are disabled.
     */
    void setParameter(const char* UTILS_NONNULL name, size_t nameLength,
            RgbType type, math::float3 color);

    /** inline helper to provide the name as a null-terminated string literal */
    void setParameter(StringLiteral const name, RgbType const type, math::float3 const color) {
        setParameter(name.data, name.size, type, color);
    }

    /** inline helper to provide the name as a null-terminated C string */
    void setParameter(const char* UTILS_NONNULL name, RgbType const type, math::float3 const color) {
        setParameter(name, strlen(name), type, color);
    }


    /**
     * Set an RGBA color as the named parameter.
     * A conversion might occur depending on the specified type
     *
     * @param name          Name of the parameter as defined by Material. Cannot be nullptr.
     * @param nameLength    Length in `char` of the name parameter.
     * @param type          Whether the color value is encoded as Linear or sRGB/A.
     * @param color         Array of read, green, blue and alpha channels values.
     * @throws utils::PreConditionPanic if name doesn't exist or no-op if exceptions are disabled.
     */
    void setParameter(const char* UTILS_NONNULL name, size_t nameLength,
            RgbaType type, math::float4 color);

    /** inline helper to provide the name as a null-terminated string literal */
    void setParameter(StringLiteral const name, RgbaType const type, math::float4 const color) {
        setParameter(name.data, name.size, type, color);
    }

    /** inline helper to provide the name as a null-terminated C string */
    void setParameter(const char* UTILS_NONNULL name, RgbaType const type, math::float4 const color) {
        setParameter(name, strlen(name), type, color);
    }

    /**
     * Gets the value of a parameter by name.
     * 
     * Note: Only supports non-texture parameters such as numeric and math types.
     * 
     * @param name          Name of the parameter as defined by Material. Cannot be nullptr.
     * @param nameLength    Length in `char` of the name parameter.
     * @throws utils::PreConditionPanic if name doesn't exist or no-op if exceptions are disabled.
     * 
     * @see Material::hasParameter
     */
    template<typename T>
    T getParameter(const char* UTILS_NONNULL name, size_t nameLength) const;

    /** inline helper to provide the name as a null-terminated C string */
    template<typename T, typename = is_supported_parameter_t<T>>
    T getParameter(StringLiteral const name) const {
        return getParameter<T>(name.data, name.size);
    }

    /** inline helper to provide the name as a null-terminated C string */
    template<typename T, typename = is_supported_parameter_t<T>>
    T getParameter(const char* UTILS_NONNULL name) const {
        return getParameter<T>(name, strlen(name));
    }

    /**
     * Set-up a custom scissor rectangle; by default it is disabled.
     *
     * The scissor rectangle gets clipped by the View's viewport, in other words, the scissor
     * cannot affect fragments outside of the View's Viewport.
     *
     * Currently the scissor is not compatible with dynamic resolution and should always be
     * disabled when dynamic resolution is used.
     *
     * @param left      left coordinate of the scissor box relative to the viewport
     * @param bottom    bottom coordinate of the scissor box relative to the viewport
     * @param width     width of the scissor box
     * @param height    height of the scissor box
     *
     * @see unsetScissor
     * @see View::setViewport
     * @see View::setDynamicResolutionOptions
     */
    void setScissor(uint32_t left, uint32_t bottom, uint32_t width, uint32_t height) noexcept;

    /**
     * Returns the scissor rectangle to its default disabled setting.
     *
     * Currently the scissor is not compatible with dynamic resolution and should always be
     * disabled when dynamic resolution is used.
     *
     * @see View::setDynamicResolutionOptions
     */
    void unsetScissor() noexcept;

    /**
     * Sets a polygon offset that will be applied to all renderables drawn with this material
     * instance.
     *
     *  The value of the offset is scale * dz + r * constant, where dz is the change in depth
     *  relative to the screen area of the triangle, and r is the smallest value that is guaranteed
     *  to produce a resolvable offset for a given implementation. This offset is added before the
     *  depth test.
     *
     *  @warning using a polygon offset other than zero has a significant negative performance
     *  impact, as most implementations have to disable early depth culling. DO NOT USE unless
     *  absolutely necessary.
     *
     * @param scale scale factor used to create a variable depth offset for each triangle
     * @param constant scale factor used to create a constant depth offset for each triangle
     */
    void setPolygonOffset(float scale, float constant) noexcept;

    /**
     * Overrides the minimum alpha value a fragment must have to not be discarded when the blend
     * mode is MASKED. Defaults to 0.4 if it has not been set in the parent Material. The specified
     * value should be between 0 and 1 and will be clamped if necessary.
     */
    void setMaskThreshold(float threshold) noexcept;

    /**
     * Gets the minimum alpha value a fragment must have to not be discarded when the blend
     * mode is MASKED
     */
    float getMaskThreshold() const noexcept;

    /**
     * Sets the screen space variance of the filter kernel used when applying specular
     * anti-aliasing. The default value is set to 0.15. The specified value should be between
     * 0 and 1 and will be clamped if necessary.
     */
    void setSpecularAntiAliasingVariance(float variance) noexcept;

    /**
     * Gets the screen space variance of the filter kernel used when applying specular
     * anti-aliasing.
     */
    float getSpecularAntiAliasingVariance() const noexcept;

    /**
     * Sets the clamping threshold used to suppress estimation errors when applying specular
     * anti-aliasing. The default value is set to 0.2. The specified value should be between 0
     * and 1 and will be clamped if necessary.
     */
    void setSpecularAntiAliasingThreshold(float threshold) noexcept;

    /**
     * Gets the clamping threshold used to suppress estimation errors when applying specular
     * anti-aliasing.
     */
    float getSpecularAntiAliasingThreshold() const noexcept;

    /**
     * Enables or disables double-sided lighting if the parent Material has double-sided capability,
     * otherwise prints a warning. If double-sided lighting is enabled, backface culling is
     * automatically disabled.
     */
    void setDoubleSided(bool doubleSided) noexcept;

    /**
     * Returns whether double-sided lighting is enabled when the parent Material has double-sided
     * capability.
     */
    bool isDoubleSided() const noexcept;

    /**
     * Specifies how transparent objects should be rendered (default is DEFAULT).
     */
    void setTransparencyMode(TransparencyMode mode) noexcept;

    /**
     * Returns the transparency mode.
     */
    TransparencyMode getTransparencyMode() const noexcept;

    /**
     * Overrides the default triangle culling state that was set on the material.
     */
    void setCullingMode(CullingMode culling) noexcept;

    /**
     * Overrides the default triangle culling state that was set on the material separately for the
     * color and shadow passes
     */
    void setCullingMode(CullingMode colorPassCullingMode, CullingMode shadowPassCullingMode) noexcept;

    /**
     * Returns the face culling mode.
     */
    CullingMode getCullingMode() const noexcept;

    /**
     * Returns the face culling mode for the shadow passes.
     */
    CullingMode getShadowCullingMode() const noexcept;

    /**
     * Overrides the default color-buffer write state that was set on the material.
     */
    void setColorWrite(bool enable) noexcept;

    /**
     * Returns whether color write is enabled.
     */
    bool isColorWriteEnabled() const noexcept;

    /**
     * Overrides the default depth-buffer write state that was set on the material.
     */
    void setDepthWrite(bool enable) noexcept;

    /**
     * Returns whether depth write is enabled.
     */
    bool isDepthWriteEnabled() const noexcept;

    /**
     * Overrides the default depth testing state that was set on the material.
     */
    void setDepthCulling(bool enable) noexcept;

    /**
     * Overrides the default depth function state that was set on the material.
     */
    void setDepthFunc(DepthFunc depthFunc) noexcept;

    /**
     * Returns the depth function state.
     */
    DepthFunc getDepthFunc() const noexcept;

    /**
     * Returns whether depth culling is enabled.
     */
    bool isDepthCullingEnabled() const noexcept;

    /**
     * Overrides the default stencil-buffer write state that was set on the material.
     */
    void setStencilWrite(bool enable) noexcept;

    /**
     * Returns whether stencil write is enabled.
     */
    bool isStencilWriteEnabled() const noexcept;

    /**
     * Sets the stencil comparison function (default is StencilCompareFunc::A).
     *
     * It's possible to set separate stencil comparison functions; one for front-facing polygons,
     * and one for back-facing polygons. The face parameter determines the comparison function(s)
     * updated by this call.
     */
    void setStencilCompareFunction(StencilCompareFunc func,
            StencilFace face = StencilFace::FRONT_AND_BACK) noexcept;

    /**
     * Sets the stencil fail operation (default is StencilOperation::KEEP).
     *
     * The stencil fail operation is performed to update values in the stencil buffer when the
     * stencil test fails.
     *
     * It's possible to set separate stencil fail operations; one for front-facing polygons, and one
     * for back-facing polygons. The face parameter determines the stencil fail operation(s) updated
     * by this call.
     */
    void setStencilOpStencilFail(StencilOperation op,
            StencilFace face = StencilFace::FRONT_AND_BACK) noexcept;

    /**
     * Sets the depth fail operation (default is StencilOperation::KEEP).
     *
     * The depth fail operation is performed to update values in the stencil buffer when the depth
     * test fails.
     *
     * It's possible to set separate depth fail operations; one for front-facing polygons, and one
     * for back-facing polygons. The face parameter determines the depth fail operation(s) updated
     * by this call.
     */
    void setStencilOpDepthFail(StencilOperation op,
            StencilFace face = StencilFace::FRONT_AND_BACK) noexcept;

    /**
     * Sets the depth-stencil pass operation (default is StencilOperation::KEEP).
     *
     * The depth-stencil pass operation is performed to update values in the stencil buffer when
     * both the stencil test and depth test pass.
     *
     * It's possible to set separate depth-stencil pass operations; one for front-facing polygons,
     * and one for back-facing polygons. The face parameter determines the depth-stencil pass
     * operation(s) updated by this call.
     */
    void setStencilOpDepthStencilPass(StencilOperation op,
            StencilFace face = StencilFace::FRONT_AND_BACK) noexcept;

    /**
     * Sets the stencil reference value (default is 0).
     *
     * The stencil reference value is the left-hand side for stencil comparison tests. It's also
     * used as the replacement stencil value when StencilOperation is REPLACE.
     *
     * It's possible to set separate stencil reference values; one for front-facing polygons, and
     * one for back-facing polygons. The face parameter determines the reference value(s) updated by
     * this call.
     */
    void setStencilReferenceValue(uint8_t value,
            StencilFace face = StencilFace::FRONT_AND_BACK) noexcept;

    /**
     * Sets the stencil read mask (default is 0xFF).
     *
     * The stencil read mask masks the bits of the values participating in the stencil comparison
     * test- both the value read from the stencil buffer and the reference value.
     *
     * It's possible to set separate stencil read masks; one for front-facing polygons, and one for
     * back-facing polygons. The face parameter determines the stencil read mask(s) updated by this
     * call.
     */
    void setStencilReadMask(uint8_t readMask,
            StencilFace face = StencilFace::FRONT_AND_BACK) noexcept;

    /**
     * Sets the stencil write mask (default is 0xFF).
     *
     * The stencil write mask masks the bits in the stencil buffer updated by stencil operations.
     *
     * It's possible to set separate stencil write masks; one for front-facing polygons, and one for
     * back-facing polygons. The face parameter determines the stencil write mask(s) updated by this
     * call.
     */
    void setStencilWriteMask(uint8_t writeMask,
            StencilFace face = StencilFace::FRONT_AND_BACK) noexcept;

    /**
     * PostProcess and compute domain material instance must be commited manually. This call has
     * no effect on surface domain materials.
     * @param engine Filament engine
     */
    void commit(Engine& engine) const;

protected:
    // prevent heap allocation
    ~MaterialInstance() = default;
};

} // namespace filament

#endif // TNT_FILAMENT_MATERIALINSTANCE_H
