/**
 * VKTS - VulKan ToolS.
 *
 * The MIT License (MIT)
 *
 * Copyright (c) since 2014 Norbert Nopper
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef VKTS_IDESCRIPTORSETLAYOUT_HPP_
#define VKTS_IDESCRIPTORSETLAYOUT_HPP_

#include <vkts/vulkan/wrapper/vkts_wrapper.hpp>

namespace vkts
{

class IDescriptorSetLayout: public IDestroyable
{

public:

    IDescriptorSetLayout() :
        IDestroyable()
    {
    }

    virtual ~IDescriptorSetLayout()
    {
    }

    virtual const VkDevice getDevice() const = 0;

    virtual const VkDescriptorSetLayoutCreateInfo& getDescriptorSetLayoutCreateInfo() const = 0;

    virtual VkDescriptorSetLayoutCreateFlags getFlags() const = 0;

    virtual uint32_t getBindingCount() const = 0;

    virtual const VkDescriptorSetLayoutBinding* getBindings() const = 0;

    virtual const VkDescriptorSetLayout getDescriptorSetLayout() const = 0;

    //
    // IDestroyable
    //

    virtual void destroy() = 0;

};

typedef std::shared_ptr<IDescriptorSetLayout> IDescriptorSetLayoutSP;

} /* namespace vkts */

#endif /* VKTS_IDESCRIPTORSETLAYOUT_HPP_ */