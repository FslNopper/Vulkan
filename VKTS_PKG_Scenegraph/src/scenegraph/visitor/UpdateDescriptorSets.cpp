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

#include "UpdateDescriptorSets.hpp"

namespace vkts
{

void UpdateDescriptorSets::updateMaterial(Material& material)
{
    auto currentDescriptorSets = material.createDescriptorSetsByName(nodeName);

    if (!currentDescriptorSets.get())
    {
        return;
    }

    //

    VkWriteDescriptorSet finalWriteDescriptorSets[VKTS_BINDING_UNIFORM_MATERIAL_TOTAL_BINDING_COUNT];
    uint32_t finalWriteDescriptorSetsCount = 0;

    for (uint32_t i = 0; i < allWriteDescriptorSetsCount; i++)
    {
        for (uint32_t k = 0; k < VKTS_BINDING_UNIFORM_PHONG_BINDING_COUNT; k++)
        {
        	// Assign used descriptor set.
			if (allWriteDescriptorSets[i].dstBinding == material.writeDescriptorSets[k].dstBinding && material.writeDescriptorSets[k].sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)
			{
				allWriteDescriptorSets[i] = material.writeDescriptorSets[k];
			}
        }

        // Gather valid descriptor sets.
    	if (allWriteDescriptorSets[i].sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)
    	{
    		finalWriteDescriptorSets[finalWriteDescriptorSetsCount] = allWriteDescriptorSets[i];

    		finalWriteDescriptorSets[finalWriteDescriptorSetsCount].dstSet = currentDescriptorSets->getDescriptorSets()[0];

			finalWriteDescriptorSetsCount++;
    	}
    }

    currentDescriptorSets->updateDescriptorSets(finalWriteDescriptorSetsCount, finalWriteDescriptorSets, 0, nullptr);
}

UpdateDescriptorSets::UpdateDescriptorSets(const uint32_t allWriteDescriptorSetsCount, VkWriteDescriptorSet* allWriteDescriptorSets) :
	SceneVisitor(), allWriteDescriptorSetsCount(allWriteDescriptorSetsCount), allWriteDescriptorSets(allWriteDescriptorSets), nodeName("")
{
}

UpdateDescriptorSets::~UpdateDescriptorSets()
{
}

//

VkBool32 UpdateDescriptorSets::visit(Node& node)
{
	for (uint32_t i = 0; i < allWriteDescriptorSetsCount; i++)
	{
		if (node.transformWriteDescriptorSet.dstBinding == VKTS_BINDING_UNIFORM_BUFFER_TRANSFORM && node.transformWriteDescriptorSet.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)
		{
			if (allWriteDescriptorSets[i].dstBinding == VKTS_BINDING_UNIFORM_BUFFER_TRANSFORM)
			{
				allWriteDescriptorSets[i] = node.transformWriteDescriptorSet;
			}
		}

		if (node.jointWriteDescriptorSet.dstBinding == VKTS_BINDING_UNIFORM_BUFFER_BONE_TRANSFORM && node.jointWriteDescriptorSet.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)
		{
			if (allWriteDescriptorSets[i].dstBinding == VKTS_BINDING_UNIFORM_BUFFER_BONE_TRANSFORM)
			{
				allWriteDescriptorSets[i] = node.jointWriteDescriptorSet;
			}

		}
	}

	nodeName = node.name;

	for (size_t i = 0; i < node.allMeshes.size(); i++)
	{
		static_cast<Mesh*>(node.allMeshes[i].get())->visitRecursive(this);
	}

    for (size_t i = 0; i < node.allChildNodes.size(); i++)
    {
    	static_cast<Node*>(node.allChildNodes[i].get())->visitRecursive(this);
    }

	return VK_FALSE;
}

VkBool32 UpdateDescriptorSets::visit(PhongMaterial& material)
{
	updateMaterial(material);

	return VK_TRUE;
}

VkBool32 UpdateDescriptorSets::visit(BSDFMaterial& material)
{
	updateMaterial(material);

	return VK_TRUE;
}

} /* namespace vkts */
