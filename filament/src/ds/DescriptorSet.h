/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef TNT_FILAMENT_DETAILS_DESCRIPTORSET_H
#define TNT_FILAMENT_DETAILS_DESCRIPTORSET_H

#include "DescriptorSetLayout.h"

#include <private/filament/EngineEnums.h>

#include <backend/DescriptorSetOffsetArray.h>
#include <backend/DriverApiForward.h>
#include <backend/DriverEnums.h>
#include <backend/Handle.h>

#include <utils/compiler.h>
#include <utils/bitset.h>
#include <utils/FixedCapacityVector.h>
#include <utils/StaticString.h>

#include <stdint.h>

namespace filament {

class DescriptorSet {
public:
    DescriptorSet() noexcept;
    explicit DescriptorSet(utils::StaticString name, DescriptorSetLayout const& descriptorSetLayout) noexcept;
    DescriptorSet(DescriptorSet const&) = delete;
    DescriptorSet(DescriptorSet&& rhs) noexcept;
    DescriptorSet& operator=(DescriptorSet const&) = delete;
    DescriptorSet& operator=(DescriptorSet&& rhs) noexcept;
    ~DescriptorSet() noexcept;

    void terminate(backend::DriverApi& driver) noexcept;

    // update the descriptors if needed
    void commit(DescriptorSetLayout const& layout, backend::DriverApi& driver) noexcept {
        if (UTILS_UNLIKELY(mDirty.any())) {
            commitSlow(layout, driver);
        }
    }

    void commitSlow(DescriptorSetLayout const& layout, backend::DriverApi& driver) noexcept;

    // bind the descriptor set
    void bind(backend::DriverApi& driver, DescriptorSetBindingPoints set) const noexcept;

    void bind(backend::DriverApi& driver, DescriptorSetBindingPoints set,
            backend::DescriptorSetOffsetArray dynamicOffsets) const noexcept;

    // unbind the descriptor set
    static void unbind(backend::DriverApi& driver, DescriptorSetBindingPoints set) noexcept;

    // sets a ubo/ssbo descriptor
    void setBuffer(DescriptorSetLayout const& layout,
            backend::descriptor_binding_t binding,
            backend::Handle<backend::HwBufferObject> boh,
            uint32_t offset, uint32_t size);

    // sets a sampler descriptor
    void setSampler(DescriptorSetLayout const& layout,
            backend::descriptor_binding_t binding,
            backend::Handle<backend::HwTexture> th,
            backend::SamplerParams params);

    // Used for duplicating material
    DescriptorSet duplicate(utils::StaticString name, DescriptorSetLayout const& layout) const noexcept;

    backend::DescriptorSetHandle getHandle() const noexcept {
        return mDescriptorSetHandle;
    }

    utils::bitset64 getValidDescriptors() const noexcept {
        return mValid;
    }

    static bool isTextureCompatibleWithDescriptor(
            backend::TextureType t, backend::SamplerType s,
            backend::DescriptorType d) noexcept;

private:
    struct Desc {
        Desc() noexcept { }
        union {
            struct {
                backend::Handle<backend::HwBufferObject> boh;
                uint32_t offset;
                uint32_t size;
            } buffer{};
            struct {
                backend::Handle<backend::HwTexture> th;
                backend::SamplerParams params;
                uint32_t padding;
            } texture;
        };
    };

    utils::FixedCapacityVector<Desc> mDescriptors;          // 16
    mutable utils::bitset64 mDirty;                         //  8
    mutable utils::bitset64 mValid;                         //  8
    backend::DescriptorSetHandle mDescriptorSetHandle;      //  4
    mutable bool mSetAfterCommitWarning = false;            //  1
    mutable bool mSetUndefinedParameterWarning = false;     //  1
    utils::StaticString mName;                              // 16
};

} // namespace filament

#endif //TNT_FILAMENT_DETAILS_DESCRIPTORSET_H
