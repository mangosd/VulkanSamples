/*
 *
 * Copyright (C) 2015 Valve Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Chia-I Wu <olvaffe@gmail.com>
 * Author: Courtney Goeltzenleuchter <courtney@LunarG.com>
 * Author: Jon Ashburn <jon@lunarg.com>
 *
 */

#include "dev.h"
#include "gpu.h"
#include "mem.h"
#include "obj.h"
#include "vk_debug_marker_lunarg.h"

VkResult intel_base_get_memory_requirements(struct intel_base *base, VkMemoryRequirements* pRequirements)
{
    memset(pRequirements, 0, sizeof(VkMemoryRequirements));
    pRequirements->memoryTypeBits = (1<< INTEL_MEMORY_TYPE_COUNT) - 1;

    return VK_SUCCESS;
}

static bool base_dbg_copy_create_info(const struct intel_handle *handle,
                                      struct intel_base_dbg *dbg,
                                      const void *create_info)
{
    const union {
        const void *ptr;
        const struct {
            VkStructureType struct_type;
            void *next;
        } *header;
    } info = { .ptr = create_info };
    size_t shallow_copy = 0;

    if (!create_info)
        return true;

    switch (dbg->type) {
    case VK_OBJECT_TYPE_DEVICE:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        break;
    case VK_OBJECT_TYPE_DEVICE_MEMORY:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO);
        break;
    case VK_OBJECT_TYPE_EVENT:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
        shallow_copy = sizeof(VkEventCreateInfo);
        break;
    case VK_OBJECT_TYPE_FENCE:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        shallow_copy = sizeof(VkFenceCreateInfo);
        break;
    case VK_OBJECT_TYPE_QUERY_POOL:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
        shallow_copy = sizeof(VkQueryPoolCreateInfo);
        break;
    case VK_OBJECT_TYPE_BUFFER:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        shallow_copy = sizeof(VkBufferCreateInfo);
        break;
    case VK_OBJECT_TYPE_BUFFER_VIEW:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
        shallow_copy = sizeof(VkBufferViewCreateInfo);
        break;
    case VK_OBJECT_TYPE_IMAGE:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        shallow_copy = sizeof(VkImageCreateInfo);
        break;
    case VK_OBJECT_TYPE_IMAGE_VIEW:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        shallow_copy = sizeof(VkImageViewCreateInfo);
        break;
    case VK_OBJECT_TYPE_SAMPLER:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
        shallow_copy = sizeof(VkSamplerCreateInfo);
        break;
    case VK_OBJECT_TYPE_DESCRIPTOR_SET:
        /* no create info */
        break;
    case VK_OBJECT_TYPE_COMMAND_POOL:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        shallow_copy = sizeof(VkCommandPoolCreateInfo);
        break;
    case VK_OBJECT_TYPE_COMMAND_BUFFER:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOC_INFO);
        shallow_copy = sizeof(VkCommandBufferAllocateInfo);
        break;
    case VK_OBJECT_TYPE_PIPELINE:
        assert(info.header->struct_type == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
        break;
    case VK_OBJECT_TYPE_FRAMEBUFFER:
        assert(info.header->struct_type ==  VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
        shallow_copy = sizeof(VkFramebufferCreateInfo);
        break;
    case VK_OBJECT_TYPE_RENDER_PASS:
        assert(info.header->struct_type ==  VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
        shallow_copy = sizeof(VkRenderPassCreateInfo);
        break;
    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
        assert(info.header->struct_type ==  VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        /* TODO */
        shallow_copy = sizeof(VkDescriptorSetLayoutCreateInfo) * 0;
        break;
    case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
        assert(info.header->struct_type ==  VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        shallow_copy = sizeof(VkDescriptorPoolCreateInfo);
        break;
    default:
        assert(!"unknown dbg object type");
        return false;
        break;
    }

    if (shallow_copy) {
        dbg->create_info = intel_alloc(handle, shallow_copy, 0,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        if (!dbg->create_info)
            return false;

        memcpy(dbg->create_info, create_info, shallow_copy);
        dbg->create_info_size = shallow_copy;
    } else if (info.header->struct_type ==
            VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO) {
        size_t size;
        const VkMemoryAllocateInfo *src = info.ptr;
        VkMemoryAllocateInfo *dst;
        uint8_t *d;
        size = sizeof(*src);

        dbg->create_info_size = size;
        dst = intel_alloc(handle, size, 0, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        if (!dst)
            return false;
        memcpy(dst, src, sizeof(*src));

        d = (uint8_t *) dst;
        d += sizeof(*src);

        dbg->create_info = dst;
    } else if (info.header->struct_type ==
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO) {
        const VkDeviceCreateInfo *src = info.ptr;
        VkDeviceCreateInfo *dst;
        uint8_t *d;
        size_t size;

        size = sizeof(*src);
        dbg->create_info_size = size;

        size += sizeof(src->pRequestedQueues[0]) *
            src->requestedQueueCount;
        for (uint32_t i = 0; i < src->requestedQueueCount; i++) {
            size += src->pRequestedQueues[i].queuePriorityCount * sizeof(float);
        }
        size += sizeof(src->ppEnabledExtensionNames[0]) *
            src->enabledExtensionNameCount;
        for (uint32_t i = 0; i < src->enabledExtensionNameCount; i++) {
            size += strlen(src->ppEnabledExtensionNames[i]) + 1;
        }

        dst = intel_alloc(handle, size, 0, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        if (!dst)
            return false;

        memcpy(dst, src, sizeof(*src));

        d = (uint8_t *) dst;
        d += sizeof(*src);

        size = sizeof(src->pRequestedQueues[0]) * src->requestedQueueCount;
        memcpy(d, src->pRequestedQueues, size);
        dst->pRequestedQueues = (const VkDeviceQueueCreateInfo *) d;
        d += size;
        for (uint32_t i = 0; i < src->requestedQueueCount; i++) {
            size = sizeof(float) *
                dst->pRequestedQueues[i].queuePriorityCount;
            memcpy(d, src->pRequestedQueues[i].pQueuePriorities, size);
            *((float **) &dst->pRequestedQueues[i].pQueuePriorities) = (float *) d;
            d += size;
        }

        size = sizeof(src->ppEnabledExtensionNames[0]) *
            src->enabledExtensionNameCount;
        dst->ppEnabledExtensionNames = (const char **) d;
        memcpy(d, src->ppEnabledExtensionNames, size);
        d += size;
        for (uint32_t i = 0; i < src->enabledExtensionNameCount; i++) {
            char **ptr = (char **) &dst->ppEnabledExtensionNames[i];
            strcpy((char *) d, src->ppEnabledExtensionNames[i]);
            *ptr = (char *) d;
            d += strlen(src->ppEnabledExtensionNames[i]) + 1;
        }

        dbg->create_info = dst;
    } else if (info.header->struct_type == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO) {
        // TODO: What do we want to copy here?
    }

    return true;
}

/**
 * Create an intel_base_dbg.  When dbg_size is non-zero, a buffer of that
 * size is allocated and zeroed.
 */
struct intel_base_dbg *intel_base_dbg_create(const struct intel_handle *handle,
                                             VkDbgObjectType type,
                                             const void *create_info,
                                             size_t dbg_size)
{
    struct intel_base_dbg *dbg;

    if (!dbg_size)
        dbg_size = sizeof(*dbg);

    assert(dbg_size >= sizeof(*dbg));

    dbg = intel_alloc(handle, dbg_size, 0, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (!dbg)
        return NULL;

    memset(dbg, 0, dbg_size);

    dbg->type = type;

    if (!base_dbg_copy_create_info(handle, dbg, create_info)) {
        intel_free(handle, dbg);
        return NULL;
    }

    return dbg;
}

void intel_base_dbg_destroy(const struct intel_handle *handle,
                            struct intel_base_dbg *dbg)
{
    if (dbg->tag)
        intel_free(handle, dbg->tag);

    if (dbg->create_info)
        intel_free(handle, dbg->create_info);

    intel_free(handle, dbg);
}

/**
 * Create an intel_base.  obj_size and dbg_size specify the real sizes of the
 * object and the debug metadata.  Memories are zeroed.
 */
struct intel_base *intel_base_create(const struct intel_handle *handle,
                                     size_t obj_size, bool debug,
                                     VkDbgObjectType type,
                                     const void *create_info,
                                     size_t dbg_size)
{
    struct intel_base *base;

    if (!obj_size)
        obj_size = sizeof(*base);

    assert(obj_size >= sizeof(*base));

    base = intel_alloc(handle, obj_size, 0, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (!base)
        return NULL;

    memset(base, 0, obj_size);
    intel_handle_init(&base->handle, type, handle->instance);

    if (debug) {
        base->dbg = intel_base_dbg_create(&base->handle,
                type, create_info, dbg_size);
        if (!base->dbg) {
            intel_free(handle, base);
            return NULL;
        }
    }

    base->get_memory_requirements = intel_base_get_memory_requirements;

    return base;
}

void intel_base_destroy(struct intel_base *base)
{
    if (base->dbg)
        intel_base_dbg_destroy(&base->handle, base->dbg);
    intel_free(base, base);
}

ICD_EXPORT void VKAPI vkGetBufferMemoryRequirements(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkMemoryRequirements*                       pRequirements)
{
    struct intel_base *base = intel_base(buffer);

    base->get_memory_requirements(base, pRequirements);
}

ICD_EXPORT void VKAPI vkGetImageMemoryRequirements(
    VkDevice                                    device,
    VkImage                                     image,
    VkMemoryRequirements*                       pRequirements)
{
    struct intel_base *base = intel_base(image);

    base->get_memory_requirements(base, pRequirements);
}

ICD_EXPORT VkResult VKAPI vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              mem_,
    VkDeviceSize                                memoryOffset)
{
    struct intel_obj *obj = intel_obj(buffer);
    struct intel_mem *mem = intel_mem(mem_);

    intel_obj_bind_mem(obj, mem, memoryOffset);

    return VK_SUCCESS;
}

ICD_EXPORT VkResult VKAPI vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem_,
    VkDeviceSize                                memoryOffset)
{
    struct intel_obj *obj = intel_obj(image);
    struct intel_mem *mem = intel_mem(mem_);

    intel_obj_bind_mem(obj, mem, memoryOffset);

    return VK_SUCCESS;
}

ICD_EXPORT VkResult VKAPI vkQueueBindSparse(
    VkQueue                                     queue,
    uint32_t                                    bindInfoCount,
    const VkBindSparseInfo*                     pBindInfo,
    VkFence                                     fence)
{
    assert(0 && "vkQueueBindSparse not supported");
    return VK_SUCCESS;
}
