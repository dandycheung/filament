/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "VulkanPlatformSwapChainImpl.h"

#include "vulkan/VulkanConstants.h"
#include "vulkan/utils/Definitions.h"
#include "vulkan/utils/Helper.h"
#include "vulkan/utils/Image.h"

#include <backend/DriverEnums.h>

using namespace bluevk;
using namespace utils;

namespace filament::backend {

namespace {

std::tuple<VkImage, VkDeviceMemory> createImageAndMemory(VulkanContext const& context,
        VkDevice device, VkExtent2D extent, VkFormat format, bool isProtected) {
    bool const isDepth = fvkutils::isVkDepthFormat(format);
    // Filament expects blit() to work with any texture, so we almost always set these usage flags
    // (see copyFrame() and readPixels()).
    VkImageUsageFlags const blittable =
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkImageCreateInfo imageInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = isProtected ? VK_IMAGE_CREATE_PROTECTED_BIT : VkImageCreateFlags(0),
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent.width, extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = blittable | (isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
    };
    VkImage image;
    VkResult result = vkCreateImage(device, &imageInfo, VKALLOC, &image);
    FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS)
            << "Unable to create image: " << static_cast<int32_t>(result);

    // Allocate memory for the VkImage and bind it.
    VkDeviceMemory imageMemory;
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);

    const VkFlags requiredMemoryFlags =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        (isProtected ? VK_MEMORY_PROPERTY_PROTECTED_BIT : 0U);
    uint32_t memoryTypeIndex
            = context.selectMemoryType(memReqs.memoryTypeBits, requiredMemoryFlags);

    FILAMENT_CHECK_POSTCONDITION(memoryTypeIndex < VK_MAX_MEMORY_TYPES)
            << "VulkanPlatformSwapChainImpl: unable to find a memory type that meets requirements.";

    VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = memoryTypeIndex,
    };
    result = vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory);
    FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS) << "Unable to allocate image memory."
                                                       << " error=" << static_cast<int32_t>(result);
    result = vkBindImageMemory(device, image, imageMemory, 0);
    FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS) << "Unable to bind image."
                                                       << " error=" << static_cast<int32_t>(result);
    return std::tuple(image, imageMemory);
}

VkFormat selectDepthFormat(fvkutils::VkFormatList const& depthFormats, bool hasStencil) {
    auto const formatItr = std::find_if(depthFormats.begin(), depthFormats.end(),
            hasStencil ? fvkutils::isVkStencilFormat : fvkutils::isVkDepthFormat);
    assert_invariant(
            formatItr != depthFormats.end() && "Cannot find suitable swapchain depth format");
    return *formatItr;
}

}// anonymous namespace

VulkanPlatformSwapChainImpl::VulkanPlatformSwapChainImpl(VulkanContext const& context,
        VkDevice device, VkQueue queue)
    : mContext(context),
      mDevice(device),
      mQueue(queue) {}

VulkanPlatformSwapChainImpl::~VulkanPlatformSwapChainImpl() {
    destroy();
}

void VulkanPlatformSwapChainImpl::destroy() {
    if (mSwapChainBundle.depth) {
        vkDestroyImage(mDevice, mSwapChainBundle.depth, VKALLOC);
        if (mMemory.find(mSwapChainBundle.depth) != mMemory.end()) {
            vkFreeMemory(mDevice, mMemory.at(mSwapChainBundle.depth), VKALLOC);
            mMemory.erase(mSwapChainBundle.depth);
        }
    }
    mSwapChainBundle.depth = VK_NULL_HANDLE;

    // Note: Hardware-backed swapchain images are not owned by us and should not be destroyed.
    mSwapChainBundle.colors.clear();
}

VkImage VulkanPlatformSwapChainImpl::createImage(VkExtent2D extent, VkFormat format,
        bool isProtected) {
    auto [image, memory] = createImageAndMemory(mContext, mDevice, extent, format, isProtected);
    mMemory.insert({image, memory});
    return image;
}

VulkanPlatformSurfaceSwapChain::VulkanPlatformSurfaceSwapChain(VulkanContext const& context,
        VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkInstance instance,
        VkSurfaceKHR surface, VkExtent2D fallbackExtent, uint64_t flags)
    : VulkanPlatformSwapChainImpl(context, device, queue),
      mInstance(instance),
      mPhysicalDevice(physicalDevice),
      mSurface(surface),
      mFallbackExtent(fallbackExtent),
      mUsesRGB((flags & backend::SWAP_CHAIN_CONFIG_SRGB_COLORSPACE) != 0),
      mHasStencil((flags & backend::SWAP_CHAIN_HAS_STENCIL_BUFFER) != 0),
      mIsProtected((flags & backend::SWAP_CHAIN_CONFIG_PROTECTED_CONTENT) != 0) {
    assert_invariant(surface);
    create();
}

VulkanPlatformSurfaceSwapChain::~VulkanPlatformSurfaceSwapChain() {
    destroy();
    vkDestroySurfaceKHR(mInstance, mSurface, VKALLOC);
}

VkResult VulkanPlatformSurfaceSwapChain::create() {
    VkSurfaceFormatKHR surfaceFormat = {};
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);

    // The general advice is to require one more than the minimum swap chain length, since the
    // absolute minimum could easily require waiting for a driver or presentation layer to release
    // the previous frame's buffer. The only situation in which we'd ask for the minimum length is
    // when using a MAILBOX presentation strategy for low-latency situations where tearing is
    // acceptable.
    uint32_t const maxImageCount = caps.maxImageCount;
    uint32_t const minImageCount = caps.minImageCount;
    uint32_t desiredImageCount = minImageCount + 1;

    // According to section 30.5 of VK 1.1, maxImageCount of zero means "that there is no limit on
    // the number of images, though there may be limits related to the total amount of memory used
    // by presentable images."
    if (maxImageCount != 0 && desiredImageCount > maxImageCount) {
        FVK_LOGE << "Swap chain does not support " << desiredImageCount << " images.";
        desiredImageCount = caps.minImageCount;
    }

    // Find a suitable surface format.
    FixedCapacityVector<VkSurfaceFormatKHR> const surfaceFormats
            = fvkutils::enumerate(vkGetPhysicalDeviceSurfaceFormatsKHR, mPhysicalDevice, mSurface);
    std::array<VkFormat, 2> expectedFormats = {
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM,
    };
    if (mUsesRGB) {
        expectedFormats = {
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_FORMAT_B8G8R8A8_SRGB,
        };
    }
    for (VkSurfaceFormatKHR const& format: surfaceFormats) {
        if (std::any_of(expectedFormats.begin(), expectedFormats.end(),
                    [&format](VkFormat f) { return f == format.format; })) {
            surfaceFormat = format;
            break;
        }
    }
    FILAMENT_CHECK_POSTCONDITION(surfaceFormat.format != VK_FORMAT_UNDEFINED)
            << "Cannot find suitable swapchain format";

    // Verify that our chosen present mode is supported. In practice all devices support the FIFO
    // mode, but we check for it anyway for completeness.  (and to avoid validation warnings)
    VkPresentModeKHR const desiredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    FixedCapacityVector<VkPresentModeKHR> presentModes = fvkutils::enumerate(
            vkGetPhysicalDeviceSurfacePresentModesKHR, mPhysicalDevice, mSurface);
    bool foundSuitablePresentMode = false;
    for (VkPresentModeKHR mode: presentModes) {
        if (mode == desiredPresentMode) {
            foundSuitablePresentMode = true;
            break;
        }
    }
    FILAMENT_CHECK_POSTCONDITION(foundSuitablePresentMode)
            << "Desired present mode is not supported by this device.";

    // Create the low-level swap chain.
    if (caps.currentExtent.width == VULKAN_UNDEFINED_EXTENT
            || caps.currentExtent.height == VULKAN_UNDEFINED_EXTENT) {
        mSwapChainBundle.extent = mFallbackExtent;
    } else {
        mSwapChainBundle.extent = caps.currentExtent;
    }

    VkCompositeAlphaFlagBitsKHR const compositeAlpha
            = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
                      ? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
                      : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainCreateInfoKHR const createInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .flags = mIsProtected ? VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR :
                VkSwapchainCreateFlagsKHR(0),
            .surface = mSurface,
            .minImageCount = desiredImageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = mSwapChainBundle.extent,
            .imageArrayLayers = 1,
            .imageUsage
            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
              | VK_IMAGE_USAGE_TRANSFER_DST_BIT // Allows use as a blit destination (for copyFrame)
              | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,// Allows use as a blit source (for readPixels)

            // TODO: Setting the preTransform to IDENTITY means we are letting the Android
            // Compositor handle the rotation. In some situations it might be more efficient to
            // handle this ourselves by setting this field to be equal to the currentTransform mask
            // in the caps, but this would involve adjusting the MVP, derivatives in GLSL, and
            // possibly more.
            // https://android-developers.googleblog.com/2020/02/handling-device-orientation-efficiently.html
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,

            .compositeAlpha = compositeAlpha,
            .presentMode = desiredPresentMode,
            .clipped = VK_TRUE,

            .oldSwapchain = mSwapchain,
    };
    VkResult result = vkCreateSwapchainKHR(mDevice, &createInfo, VKALLOC, &mSwapchain);
    FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS) << "vkCreateSwapchainKHR failed."
                                                       << " error=" << static_cast<int32_t>(result);

    mSwapChainBundle.colors = fvkutils::enumerate(vkGetSwapchainImagesKHR, mDevice, mSwapchain);
    mSwapChainBundle.colorFormat = surfaceFormat.format;
    mSwapChainBundle.depthFormat =
            selectDepthFormat(mContext.getAttachmentDepthStencilFormats(), mHasStencil);
    mSwapChainBundle.depth = createImage(mSwapChainBundle.extent,
            mSwapChainBundle.depthFormat, mIsProtected);
    mSwapChainBundle.isProtected = mIsProtected;

    FVK_LOGI << "vkCreateSwapchain"
           << ": " << mSwapChainBundle.extent.width << "x" << mSwapChainBundle.extent.height << ", "
           << surfaceFormat.format << ", " << surfaceFormat.colorSpace << ", "
           << "swapchain-size=" << mSwapChainBundle.colors.size() << ", "
           << "identity-transform=" << (caps.currentTransform == 1) << ", "
           << "depth=" << mSwapChainBundle.depthFormat << ", "
           << "protected=" << mSwapChainBundle.isProtected;

    VkSemaphoreCreateInfo const semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    for (uint32_t i = 0; i < IMAGE_READY_SEMAPHORE_COUNT; ++i) {
        VkResult result = vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr,
                mImageReady + i);
        FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS)
                << "Failed to create semaphore."
                << " error=" << static_cast<int32_t>(result);
    }

    return result;
}

VkResult VulkanPlatformSurfaceSwapChain::acquire(VulkanPlatform::ImageSyncData* outImageSyncData) {
    mCurrentImageReadyIndex = (mCurrentImageReadyIndex + 1) % IMAGE_READY_SEMAPHORE_COUNT;
    outImageSyncData->imageReadySemaphore = mImageReady[mCurrentImageReadyIndex];
    VkResult result = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX,
            outImageSyncData->imageReadySemaphore, VK_NULL_HANDLE, &outImageSyncData->imageIndex);

    // Users should be notified of a suboptimal surface, but it should not cause a cascade of
    // log messages or a loop of re-creations.
    if (result == VK_SUBOPTIMAL_KHR && !mSuboptimal) {
        FVK_LOGW << "Vulkan Driver: Suboptimal swap chain.";
        mSuboptimal = true;
    }
    return result;
}

VkResult VulkanPlatformSurfaceSwapChain::present(uint32_t index, VkSemaphore finished) {
    uint32_t currentIndex = index;
    VkSemaphore finishedDrawing = finished;
    VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &finishedDrawing,
            .swapchainCount = 1,
            .pSwapchains = &mSwapchain,
            .pImageIndices = &currentIndex,
    };
    VkResult result = vkQueuePresentKHR(mQueue, &presentInfo);

    // On Android Q and above, a suboptimal surface is always reported after screen rotation:
    // https://android-developers.googleblog.com/2020/02/handling-device-orientation-efficiently.html
    if (result == VK_SUBOPTIMAL_KHR && !mSuboptimal) {
        FVK_LOGW << "Vulkan Driver: Suboptimal swap chain.";
        mSuboptimal = true;
    }
    return result;
}

bool VulkanPlatformSurfaceSwapChain::hasResized() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);
    VkExtent2D perceivedExtent = caps.currentExtent;
    // Create the low-level swap chain.
    if (perceivedExtent.width == VULKAN_UNDEFINED_EXTENT
            || perceivedExtent.height == VULKAN_UNDEFINED_EXTENT) {
        perceivedExtent = mFallbackExtent;
    }
    return !fvkutils::equivalent(mSwapChainBundle.extent, perceivedExtent);
}

bool VulkanPlatformSurfaceSwapChain::isProtected() {
    return mIsProtected;
}

// Non-virtual override
VkResult VulkanPlatformSurfaceSwapChain::recreate() {
    destroy();
    return create();
}

void VulkanPlatformSurfaceSwapChain::destroy() {
    // The next part is not ideal. We don't have a good signal on when it's ok to destroy
    // a swapchain. This is a spec oversight and mentioned as much:
    // https://github.com/KhronosGroup/Vulkan-Docs/issues/1678
    //
    // One workaround [1] is:
    // https://docs.vulkan.org/samples/latest/samples/api/swapchain_recreation/README.html
    //
    // The proper fix is to use VK_EXT_swapchain_maintenance1, but availability of this extension is
    // unknown (not yet ratified).
    //
    // Instead of adding too much mechanics, we're taking a hacksaw to the problem - just wait for
    // the queue to be idle. The hope is that this only happens on resize, where performance
    // degradation is less obvious (until, of course, people complain about lag when rotating their
    // phone). If necessary, we can revisit and implement the workaround [1].
    vkQueueWaitIdle(mQueue);

    VulkanPlatformSwapChainImpl::destroy();

    for (uint32_t i = 0; i < IMAGE_READY_SEMAPHORE_COUNT; ++i) {
        if (mImageReady[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(mDevice, mImageReady[i], VKALLOC);
            mImageReady[i] = VK_NULL_HANDLE;
        }
    }
    if (mSwapchain) {
        vkDestroySwapchainKHR(mDevice, mSwapchain, VKALLOC);
        mSwapchain = VK_NULL_HANDLE;
    }
}

VulkanPlatformHeadlessSwapChain::VulkanPlatformHeadlessSwapChain(VulkanContext const& context,
        VkDevice device, VkQueue queue, VkExtent2D extent, uint64_t flags)
    : VulkanPlatformSwapChainImpl(context, device, queue),
      mCurrentIndex(0) {
    mSwapChainBundle.extent = extent;
    mSwapChainBundle.colorFormat = (flags & backend::SWAP_CHAIN_CONFIG_SRGB_COLORSPACE) != 0
                                           ? VK_FORMAT_R8G8B8A8_SRGB
                                           : VK_FORMAT_R8G8B8A8_UNORM;

    auto& images = mSwapChainBundle.colors;
    images.reserve(HEADLESS_SWAPCHAIN_SIZE);
    images.resize(HEADLESS_SWAPCHAIN_SIZE);
    for (size_t i = 0; i < HEADLESS_SWAPCHAIN_SIZE; ++i) {
        images[i] = createImage(extent, mSwapChainBundle.colorFormat, false);
    }

    bool const hasStencil = (flags & backend::SWAP_CHAIN_HAS_STENCIL_BUFFER) != 0;
    mSwapChainBundle.depthFormat =
            selectDepthFormat(mContext.getAttachmentDepthStencilFormats(), hasStencil);
    mSwapChainBundle.depth = createImage(extent, mSwapChainBundle.depthFormat, false);
}

VulkanPlatformHeadlessSwapChain::~VulkanPlatformHeadlessSwapChain() {
    destroy();
}

VkResult VulkanPlatformHeadlessSwapChain::present(uint32_t index, VkSemaphore finished) {
    // No-op for headless swapchain.
    return VK_SUCCESS;
}

VkResult VulkanPlatformHeadlessSwapChain::acquire(VulkanPlatform::ImageSyncData* outImageSyncData) {
    outImageSyncData->imageIndex = mCurrentIndex;
    mCurrentIndex = (mCurrentIndex + 1) % HEADLESS_SWAPCHAIN_SIZE;
    return VK_SUCCESS;
}

void VulkanPlatformHeadlessSwapChain::destroy() {
    // This is only ever called from the destructor since headless does not recreate.
    for (auto image: mSwapChainBundle.colors) {
        vkDestroyImage(mDevice, image, VKALLOC);
        if (mMemory.find(image) != mMemory.end()) {
            vkFreeMemory(mDevice, mMemory.at(image), VKALLOC);
            mMemory.erase(image);
        }
    }
    mSwapChainBundle.colors.clear();
    // No need to manually call through to the super because the super's destructor will be called
}

}// namespace filament::backend
