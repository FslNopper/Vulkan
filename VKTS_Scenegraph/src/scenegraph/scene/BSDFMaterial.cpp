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

#include "../scene/BSDFMaterial.hpp"

namespace vkts
{

BSDFMaterial::BSDFMaterial(const VkBool32 forwardRendering) :
    IBSDFMaterial(), Material(), forwardRendering(forwardRendering), name(), fragmentShader(nullptr), attributes(VKTS_VERTEX_BUFFER_TYPE_VERTEX | VKTS_VERTEX_BUFFER_TYPE_NORMAL), allTextureObjects()
{
}

BSDFMaterial::BSDFMaterial(const BSDFMaterial& other) :
    IBSDFMaterial(), Material(other), forwardRendering(other.forwardRendering), name(other.name + "_clone"), fragmentShader(other.fragmentShader), attributes(other.attributes), allTextureObjects()
{
	for (size_t i = 0; i < other.allTextureObjects.size(); i++)
	{
		addTextureObject(other.allTextureObjects[i]);
	}
}

BSDFMaterial::~BSDFMaterial()
{
    destroy();
}

//
// IBSDFMaterial
//

VkBool32 BSDFMaterial::getForwardRendering() const
{
    return forwardRendering;
}

const std::string& BSDFMaterial::getName() const
{
    return name;
}

void BSDFMaterial::setName(const std::string& name)
{
    this->name = name;
}

IShaderModuleSP BSDFMaterial::getFragmentShader() const
{
	return fragmentShader;
}

void BSDFMaterial::setFragmentShader(const IShaderModuleSP& fragmentShader)
{
	this->fragmentShader = fragmentShader;
}


VkTsVertexBufferType BSDFMaterial::getAttributes() const
{
	return attributes;
}

void BSDFMaterial::setAttributes(const VkTsVertexBufferType attributes)
{
	this->attributes = attributes;
}

void BSDFMaterial::addTextureObject(const ITextureObjectSP& textureObject)
{
    if (textureObject.get())
    {
    	uint32_t offset = forwardRendering ? VKTS_BINDING_UNIFORM_SAMPLER_BSDF_FORWARD_FIRST : VKTS_BINDING_UNIFORM_SAMPLER_BSDF_DEFERRED_FIRST;
        updateDescriptorImageInfo((uint32_t)allTextureObjects.size(), offset, textureObject->getSampler()->getSampler(), textureObject->getImageObject()->getImageView()->getImageView(), textureObject->getImageObject()->getImage()->getImageLayout());
    }

    allTextureObjects.append(textureObject);
}

VkBool32 BSDFMaterial::removeTextureObject(const ITextureObjectSP& textureObject)
{
    return allTextureObjects.remove(textureObject);
}

size_t BSDFMaterial::getNumberTextureObjects() const
{
    return allTextureObjects.size();
}

const SmartPointerVector<ITextureObjectSP>& BSDFMaterial::getTextureObjects() const
{
    return allTextureObjects;
}

const IDescriptorPoolSP& BSDFMaterial::getDescriptorPool() const
{
    return descriptorPool;
}

void BSDFMaterial::setDescriptorPool(const IDescriptorPoolSP& descriptorPool)
{
    this->descriptorPool = descriptorPool;
}

IDescriptorSetsSP BSDFMaterial::getDescriptorSets() const
{
    return descriptorSets;
}

void BSDFMaterial::setDescriptorSets(const IDescriptorSetsSP& descriptorSets)
{
    this->descriptorSets = descriptorSets;
}

void BSDFMaterial::updateDescriptorSetsRecursive(const std::string& nodeName, const uint32_t allWriteDescriptorSetsCount, VkWriteDescriptorSet* allWriteDescriptorSets)
{
    auto currentDescriptorSets = createDescriptorSetsByName(nodeName);

    if (!currentDescriptorSets.get())
    {
        return;
    }

    //

    VkWriteDescriptorSet finalWriteDescriptorSets[VKTS_BINDING_UNIFORM_MATERIAL_TOTAL_BINDING_COUNT];
    uint32_t finalWriteDescriptorSetsCount = 0;

    uint32_t currentTextureObject = 0;

    for (uint32_t i = 0; i < allWriteDescriptorSetsCount; i++)
    {
        // Gather valid descriptor sets.
    	if (allWriteDescriptorSets[i].sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET && allWriteDescriptorSets[i].descriptorCount > 0)
    	{
    		finalWriteDescriptorSets[finalWriteDescriptorSetsCount] = allWriteDescriptorSets[i];

    		finalWriteDescriptorSets[finalWriteDescriptorSetsCount].dstSet = currentDescriptorSets->getDescriptorSets()[0];

			finalWriteDescriptorSetsCount++;
    	}
    	else
    	{
        	if ((size_t)currentTextureObject == allTextureObjects.size())
        	{
        		break;
        	}

    		finalWriteDescriptorSets[finalWriteDescriptorSetsCount] = writeDescriptorSets[currentTextureObject];
    		finalWriteDescriptorSets[finalWriteDescriptorSetsCount].dstSet = currentDescriptorSets->getDescriptorSets()[0];

    		currentTextureObject++;
			finalWriteDescriptorSetsCount++;
    	}
    }

    currentDescriptorSets->updateDescriptorSets(finalWriteDescriptorSetsCount, finalWriteDescriptorSets, 0, nullptr);
}

void BSDFMaterial::bindDescriptorSets(const std::string& nodeName, const ICommandBuffersSP& cmdBuffer, const VkPipelineLayout layout, const uint32_t bufferIndex) const
{
    if (!cmdBuffer.get())
    {
        return;
    }

    auto currentDescriptorSets = getDescriptorSetsByName(nodeName);

    if (!currentDescriptorSets.get())
    {
        return;
    }

    vkCmdBindDescriptorSets(cmdBuffer->getCommandBuffer(bufferIndex), VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &currentDescriptorSets->getDescriptorSets()[0], 0, nullptr);
}

void BSDFMaterial::bindDrawIndexedRecursive(const std::string& nodeName, const ICommandBuffersSP& cmdBuffer, const IGraphicsPipelineSP& graphicsPipeline, const Overwrite* renderOverwrite, const uint32_t bufferIndex) const
{
    const Overwrite* currentOverwrite = renderOverwrite;
    while (currentOverwrite)
    {
    	if (!currentOverwrite->materialBindDrawIndexedRecursive(*this, cmdBuffer, graphicsPipeline, bufferIndex))
    	{
    		return;
    	}

    	currentOverwrite = currentOverwrite->getNextOverwrite();
    }

	if (!graphicsPipeline.get())
	{
		return;
	}

	vkCmdBindPipeline(cmdBuffer->getCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipeline());

    bindDescriptorSets(nodeName, cmdBuffer, graphicsPipeline->getLayout(), bufferIndex);
}

//
// ICloneable
//

IBSDFMaterialSP BSDFMaterial::clone() const
{
	auto result = IBSDFMaterialSP(new BSDFMaterial(*this));

	if (result && result->getNumberTextureObjects() != getNumberTextureObjects())
	{
		return IBSDFMaterialSP();
	}

    return result;
}


//
// IDestroyable
//

void BSDFMaterial::destroy()
{
	if (fragmentShader.get())
	{
		fragmentShader->destroy();
	}

	for (size_t i = 0; i < allTextureObjects.size(); i++)
	{
		allTextureObjects[i]->destroy();
	}

	//

	Material::destroy();
}

} /* namespace vkts */
