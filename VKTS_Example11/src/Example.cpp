/**
 * VKTS Examples - Examples for Vulkan using VulKan ToolS.
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

#include "Example.hpp"

Example::Example(const vkts::IInitialResourcesSP& initialResources, const int32_t windowIndex, const vkts::ISurfaceSP& surface) :
		IUpdateThread(), initialResources(initialResources), windowIndex(windowIndex), surface(surface), depthFormat(VK_FORMAT_D32_SFLOAT), camera(nullptr), inputController(nullptr), allUpdateables(), commandPool(nullptr), imageAcquiredSemaphore(nullptr), renderingCompleteSemaphore(nullptr), descriptorSetLayout(nullptr), vertexViewProjectionUniformBuffer(nullptr), fragmentUniformBuffer(nullptr), shadowUniformBuffer(nullptr), voxelizeViewProjectionUniformBuffer(nullptr), voxelizeModelNormalUniformBuffer(nullptr), standardVertexShaderModule(nullptr), standardFragmentShaderModule(nullptr), standardShadowFragmentShaderModule(nullptr), voxelizeVertexShaderModule(nullptr), voxelizeGeometryShaderModule(nullptr), voxelizeFragmentShaderModule(nullptr), pipelineLayout(nullptr), loadTask(), sceneLoaded(VK_FALSE), sceneContext(nullptr), scene(nullptr), swapchain(nullptr), renderPass(nullptr), shadowRenderPass(nullptr), voxelRenderPass(nullptr), allOpaqueGraphicsPipelines(), allBlendGraphicsPipelines(), allBlendCwGraphicsPipelines(), allShadowGraphicsPipelines(), shadowTexture(nullptr), msaaColorTexture(nullptr), msaaDepthTexture(nullptr), depthTexture(nullptr), voxelTexture{nullptr, nullptr, nullptr}, shadowImageView(nullptr), msaaColorImageView(nullptr), msaaDepthStencilImageView(nullptr), depthStencilImageView(nullptr), shadowSampler(nullptr), swapchainImagesCount(0), swapchainImageView(), framebuffer(), shadowFramebuffer(), fences(), cmdBuffer(), shadowCmdBuffer()
{
}

Example::~Example()
{
}

VkBool32 Example::buildCmdBuffer(const int32_t usedBuffer)
{
	VkResult result;

    //
    // Depth pass.
    //

	shadowCmdBuffer[usedBuffer] = vkts::commandBuffersCreate(initialResources->getDevice()->getDevice(), commandPool->getCmdPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

	if (!shadowCmdBuffer[usedBuffer].get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create command buffer.");

		return VK_FALSE;
	}

	result = shadowCmdBuffer[usedBuffer]->beginCommandBuffer(0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, VK_FALSE, 0, 0);

	if (result != VK_SUCCESS)
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not begin command buffer.");

		return VK_FALSE;
	}

	//

	VkClearDepthStencilValue shadowClearDepthStencilValue{};

	shadowClearDepthStencilValue.depth = 1.0f;
	shadowClearDepthStencilValue.stencil = 0;

	VkClearValue shadowClearValues[1]{};

	shadowClearValues[0].depthStencil = shadowClearDepthStencilValue;

	VkRenderPassBeginInfo shadowRenderPassBeginInfo{};

	shadowRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

	shadowRenderPassBeginInfo.renderPass = shadowRenderPass->getRenderPass();
	shadowRenderPassBeginInfo.framebuffer = shadowFramebuffer[usedBuffer]->getFramebuffer();
	shadowRenderPassBeginInfo.renderArea.offset.x = 0;
	shadowRenderPassBeginInfo.renderArea.offset.y = 0;
	shadowRenderPassBeginInfo.renderArea.extent = {VKTS_SHADOW_MAP_SIZE, VKTS_SHADOW_MAP_SIZE};
	shadowRenderPassBeginInfo.clearValueCount = 1;
	shadowRenderPassBeginInfo.pClearValues = shadowClearValues;

	shadowCmdBuffer[usedBuffer]->cmdBeginRenderPass(&shadowRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport shadowViewport{};

	shadowViewport.x = 0.0f;
	shadowViewport.y = 0.0f;
	shadowViewport.width = (float)VKTS_SHADOW_MAP_SIZE;
	shadowViewport.height = (float)VKTS_SHADOW_MAP_SIZE;
	shadowViewport.minDepth = 0.0f;
	shadowViewport.maxDepth = 1.0f;

	vkCmdSetViewport(shadowCmdBuffer[usedBuffer]->getCommandBuffer(), 0, 1, &shadowViewport);

	VkRect2D shadowScissor{};

	shadowScissor.offset.x = 0;
	shadowScissor.offset.y = 0;
	shadowScissor.extent = {VKTS_SHADOW_MAP_SIZE, VKTS_SHADOW_MAP_SIZE};

	vkCmdSetScissor(shadowCmdBuffer[usedBuffer]->getCommandBuffer(), 0, 1, &shadowScissor);

	if (scene.get())
	{
		scene->bindDrawIndexedRecursive(shadowCmdBuffer[usedBuffer], allShadowGraphicsPipelines);
	}

	shadowCmdBuffer[usedBuffer]->cmdEndRenderPass();

    //

	result = shadowCmdBuffer[usedBuffer]->endCommandBuffer();

	if (result != VK_SUCCESS)
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not end command buffer.");

		return VK_FALSE;
	}

    //
    // Color pass.
    //

	cmdBuffer[usedBuffer] = vkts::commandBuffersCreate(initialResources->getDevice()->getDevice(), commandPool->getCmdPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

	if (!cmdBuffer[usedBuffer].get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create command buffer.");

		return VK_FALSE;
	}

	result = cmdBuffer[usedBuffer]->beginCommandBuffer(0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, VK_FALSE, 0, 0);

	if (result != VK_SUCCESS)
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not begin command buffer.");

		return VK_FALSE;
	}

    //

    swapchain->cmdPipelineBarrier(cmdBuffer[usedBuffer]->getCommandBuffer(), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, usedBuffer);

	//
	// Barrier, that we can read from the shadow map.
	//

    VkImageSubresourceRange depthSubresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

	shadowTexture->getImage()->cmdPipelineBarrier(cmdBuffer[usedBuffer]->getCommandBuffer(), VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, depthSubresourceRange);

	//

	VkClearColorValue clearColorValue{};

	clearColorValue.float32[0] = 0.7f;
	clearColorValue.float32[1] = 0.9f;
	clearColorValue.float32[2] = 1.0f;
	clearColorValue.float32[3] = 1.0f;

	VkClearDepthStencilValue clearDepthStencilValue{};

	clearDepthStencilValue.depth = 1.0f;
	clearDepthStencilValue.stencil = 0;

	VkClearValue clearValues[4]{};

	clearValues[0].color = clearColorValue;
	clearValues[1].depthStencil = clearDepthStencilValue;
	clearValues[2].color = clearColorValue;
	clearValues[3].depthStencil = clearDepthStencilValue;

	VkRenderPassBeginInfo renderPassBeginInfo{};

	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

	renderPassBeginInfo.renderPass = renderPass->getRenderPass();
	renderPassBeginInfo.framebuffer = framebuffer[usedBuffer]->getFramebuffer();
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent = swapchain->getImageExtent();
	renderPassBeginInfo.clearValueCount = 4;
	renderPassBeginInfo.pClearValues = clearValues;

	cmdBuffer[usedBuffer]->cmdBeginRenderPass(&renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) swapchain->getImageExtent().width;
	viewport.height = (float) swapchain->getImageExtent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vkCmdSetViewport(cmdBuffer[usedBuffer]->getCommandBuffer(), 0, 1, &viewport);

	VkRect2D scissor{};

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = swapchain->getImageExtent();

	vkCmdSetScissor(cmdBuffer[usedBuffer]->getCommandBuffer(), 0, 1, &scissor);

	//

	if (scene.get())
	{
		vkts::Blend blend;

		// First all opaque elements.
		blend.setPassTransparent(VK_FALSE);
		scene->bindDrawIndexedRecursive(cmdBuffer[usedBuffer], allOpaqueGraphicsPipelines, &blend);

		// Then, transparent elements.
		blend.setPassTransparent(VK_TRUE);
		// Transparent elements are one sided, so render clockwise ...
		scene->bindDrawIndexedRecursive(cmdBuffer[usedBuffer], allBlendCwGraphicsPipelines, &blend);
		// ... and counter clockwise.
		scene->bindDrawIndexedRecursive(cmdBuffer[usedBuffer], allBlendGraphicsPipelines, &blend);
	}

	cmdBuffer[usedBuffer]->cmdEndRenderPass();

    //

	//
	// Barrier, that we can write to the shadow map.
	//

	shadowTexture->getImage()->cmdPipelineBarrier(cmdBuffer[usedBuffer]->getCommandBuffer(), VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depthSubresourceRange);

	//

    swapchain->cmdPipelineBarrier(cmdBuffer[usedBuffer]->getCommandBuffer(), VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, usedBuffer);

    //

	result = cmdBuffer[usedBuffer]->endCommandBuffer();

	if (result != VK_SUCCESS)
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not end command buffer.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildFences(const int32_t usedBuffer)
{
	fences[usedBuffer] = vkts::fenceCreate(initialResources->getDevice()->getDevice(), 0);

	if (!fences[usedBuffer].get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create fences.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildFramebuffer(const int32_t usedBuffer)
{
	VkImageView imageViews[4];

	imageViews[0] = swapchainImageView[usedBuffer]->getImageView();
	imageViews[1] = depthStencilImageView->getImageView();
	imageViews[2] = msaaColorImageView->getImageView();
	imageViews[3] = msaaDepthStencilImageView->getImageView();

	framebuffer[usedBuffer] = vkts::framebufferCreate(initialResources->getDevice()->getDevice(), 0, renderPass->getRenderPass(), 4, imageViews, swapchain->getImageExtent().width, swapchain->getImageExtent().height, 1);

	if (!framebuffer[usedBuffer].get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create frame buffer.");

		return VK_FALSE;
	}

	// Build shadow frame buffer.

	imageViews[0] = shadowImageView->getImageView();

	shadowFramebuffer[usedBuffer] = vkts::framebufferCreate(initialResources->getDevice()->getDevice(), 0, shadowRenderPass->getRenderPass(), 1, imageViews, VKTS_SHADOW_MAP_SIZE, VKTS_SHADOW_MAP_SIZE, 1);

	if (!shadowFramebuffer[usedBuffer].get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create frame buffer.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildSwapchainImageView(const int32_t usedBuffer)
{
	VkComponentMapping componentMapping = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	swapchainImageView[usedBuffer] = vkts::imageViewCreate(initialResources->getDevice()->getDevice(), 0, swapchain->getAllSwapchainImages()[usedBuffer], VK_IMAGE_VIEW_TYPE_2D, swapchain->getImageFormat(), componentMapping, imageSubresourceRange);

	if (!swapchainImageView[usedBuffer].get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create color attachment view.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::updateDescriptorSets()
{
	memset(descriptorBufferInfos, 0, sizeof(descriptorBufferInfos));

	descriptorBufferInfos[0].buffer = vertexViewProjectionUniformBuffer->getBuffer()->getBuffer();
	descriptorBufferInfos[0].offset = 0;
	descriptorBufferInfos[0].range = vertexViewProjectionUniformBuffer->getBuffer()->getSize();

	descriptorBufferInfos[1].buffer = fragmentUniformBuffer->getBuffer()->getBuffer();
	descriptorBufferInfos[1].offset = 0;
	descriptorBufferInfos[1].range = fragmentUniformBuffer->getBuffer()->getSize();

	descriptorBufferInfos[2].buffer = shadowUniformBuffer->getBuffer()->getBuffer();
	descriptorBufferInfos[2].offset = 0;
	descriptorBufferInfos[2].range = shadowUniformBuffer->getBuffer()->getSize();

    memset(descriptorImageInfos, 0, sizeof(descriptorImageInfos));

    descriptorImageInfos[0].sampler = shadowSampler->getSampler();
    descriptorImageInfos[0].imageView = shadowImageView->getImageView();
    descriptorImageInfos[0].imageLayout = shadowTexture->getImage()->getImageLayout();

	memset(writeDescriptorSets, 0, sizeof(writeDescriptorSets));

	writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	writeDescriptorSets[0].dstSet = VK_NULL_HANDLE;	// Defined later.
	writeDescriptorSets[0].dstBinding = VKTS_BINDING_UNIFORM_BUFFER_VIEWPROJECTION;
	writeDescriptorSets[0].dstArrayElement = 0;
	writeDescriptorSets[0].descriptorCount = 1;
	writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSets[0].pImageInfo = nullptr;
	writeDescriptorSets[0].pBufferInfo = &descriptorBufferInfos[0];
	writeDescriptorSets[0].pTexelBufferView = nullptr;

	writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	writeDescriptorSets[1].dstSet = VK_NULL_HANDLE;	// Defined later.
	writeDescriptorSets[1].dstBinding = VKTS_BINDING_UNIFORM_BUFFER_LIGHT;
	writeDescriptorSets[1].dstArrayElement = 0;
	writeDescriptorSets[1].descriptorCount = 1;
	writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSets[1].pImageInfo = nullptr;
	writeDescriptorSets[1].pBufferInfo = &descriptorBufferInfos[1];
	writeDescriptorSets[1].pTexelBufferView = nullptr;

	writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	writeDescriptorSets[2].dstSet = VK_NULL_HANDLE;	// Defined later.
	writeDescriptorSets[2].dstBinding = VKTS_BINDING_UNIFORM_SAMPLER_SHADOW;
	writeDescriptorSets[2].dstArrayElement = 0;
	writeDescriptorSets[2].descriptorCount = 1;
	writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSets[2].pImageInfo = &descriptorImageInfos[0];
	writeDescriptorSets[2].pBufferInfo = nullptr;
	writeDescriptorSets[2].pTexelBufferView = nullptr;

	writeDescriptorSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	writeDescriptorSets[3].dstSet = VK_NULL_HANDLE;	// Defined later.
	writeDescriptorSets[3].dstBinding = VKTS_BINDING_UNIFORM_BUFFER_SHADOW;
	writeDescriptorSets[3].dstArrayElement = 0;
	writeDescriptorSets[3].descriptorCount = 1;
	writeDescriptorSets[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSets[3].pImageInfo = nullptr;
	writeDescriptorSets[3].pBufferInfo = &descriptorBufferInfos[2];
	writeDescriptorSets[3].pTexelBufferView = nullptr;

	//

	writeDescriptorSets[4].dstBinding = VKTS_BINDING_UNIFORM_BUFFER_TRANSFORM;

	for (uint32_t i = VKTS_BINDING_UNIFORM_SAMPLER_PHONG_FIRST; i <= VKTS_BINDING_UNIFORM_SAMPLER_PHONG_LAST; i++)
	{
		writeDescriptorSets[5 + i - VKTS_BINDING_UNIFORM_SAMPLER_PHONG_FIRST].dstBinding = i;
	}

	return VK_TRUE;
}

VkBool32 Example::buildShadowSampler()
{
	// Enabled texture compare.
	shadowSampler = vkts::samplerCreate(initialResources->getDevice()->getDevice(), 0, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, VK_FALSE, 1.0f, VK_FALSE, VK_COMPARE_OP_NEVER, 0.0f, 0.0f, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, VK_FALSE);

	if (!shadowSampler.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create sampler.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildDepthStencilImageView()
{
	VkComponentMapping componentMapping = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

	depthStencilImageView = vkts::imageViewCreate(initialResources->getDevice()->getDevice(), 0, depthTexture->getImage()->getImage(), VK_IMAGE_VIEW_TYPE_2D, depthTexture->getImage()->getFormat(), componentMapping, imageSubresourceRange);

	if (!depthStencilImageView.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create depth attachment view.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildMSAADepthStencilImageView()
{
	VkComponentMapping componentMapping = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

	msaaDepthStencilImageView = vkts::imageViewCreate(initialResources->getDevice()->getDevice(), 0, msaaDepthTexture->getImage()->getImage(), VK_IMAGE_VIEW_TYPE_2D, msaaDepthTexture->getImage()->getFormat(), componentMapping, imageSubresourceRange);

	if (!msaaDepthStencilImageView.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create MSAA depth attachment view.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildMSAAColorImageView()
{
	VkComponentMapping componentMapping = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	msaaColorImageView = vkts::imageViewCreate(initialResources->getDevice()->getDevice(), 0, msaaColorTexture->getImage()->getImage(), VK_IMAGE_VIEW_TYPE_2D, msaaColorTexture->getImage()->getFormat(), componentMapping, imageSubresourceRange);

	if (!msaaColorImageView.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create MSAA color attachment view.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildShadowImageView()
{
	VkComponentMapping componentMapping = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

	shadowImageView = vkts::imageViewCreate(initialResources->getDevice()->getDevice(), 0, shadowTexture->getImage()->getImage(), VK_IMAGE_VIEW_TYPE_2D, shadowTexture->getImage()->getFormat(), componentMapping, imageSubresourceRange);

	if (!shadowImageView.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create shadow attachment view.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildVoxelTexture(const vkts::ICommandBuffersSP& cmdBuffer)
{
	if (voxelTexture[0].get() && voxelTexture[1].get() && voxelTexture[2].get())
	{
		return VK_TRUE;
	}
	else if (voxelTexture[0].get() || voxelTexture[1].get() || voxelTexture[2].get())
	{
		return VK_FALSE;
	}

	//

	VkImageCreateInfo imageCreateInfo{};

	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
	imageCreateInfo.format = VK_FORMAT_R32_UINT;
	imageCreateInfo.extent = {VKTS_VOXEL_CUBE_SIZE, VKTS_VOXEL_CUBE_SIZE, VKTS_VOXEL_CUBE_SIZE};
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.pQueueFamilyIndices = nullptr;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	for (uint32_t i = 0; i < 3; i++)
	{
		voxelTexture[i] = vkts::memoryImageCreate(initialResources, cmdBuffer, "VoxelTexture_" + std::to_string(i), imageCreateInfo, 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, subresourceRange, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (!voxelTexture[i].get())
		{
			vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create voxel texture.");

			return VK_FALSE;
		}
	}

	return VK_TRUE;
}

VkBool32 Example::buildDepthTexture(const vkts::ICommandBuffersSP& cmdBuffer)
{
	VkImageCreateInfo imageCreateInfo{};

	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = VK_FORMAT_D16_UNORM;
	imageCreateInfo.extent = {swapchain->getImageExtent().width, swapchain->getImageExtent().height, 1};
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.pQueueFamilyIndices = nullptr;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

	depthTexture = vkts::memoryImageCreate(initialResources, cmdBuffer, "DepthTexture", imageCreateInfo, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, subresourceRange, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (!depthTexture.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create depth texture.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildMSAADepthTexture(const vkts::ICommandBuffersSP& cmdBuffer)
{
	VkImageCreateInfo imageCreateInfo{};

	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = VK_FORMAT_D16_UNORM;
	imageCreateInfo.extent = {swapchain->getImageExtent().width, swapchain->getImageExtent().height, 1};
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VKTS_SAMPLE_COUNT_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.pQueueFamilyIndices = nullptr;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

	msaaDepthTexture = vkts::memoryImageCreate(initialResources, cmdBuffer, "MSAADepthTexture", imageCreateInfo, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, subresourceRange, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (!msaaDepthTexture.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create depth texture.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildMSAAColorTexture(const vkts::ICommandBuffersSP& cmdBuffer)
{
	VkImageCreateInfo imageCreateInfo{};

	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = swapchain->getImageFormat();
	imageCreateInfo.extent = {swapchain->getImageExtent().width, swapchain->getImageExtent().height, 1};
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VKTS_SAMPLE_COUNT_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.pQueueFamilyIndices = nullptr;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	msaaColorTexture = vkts::memoryImageCreate(initialResources, cmdBuffer, "MSAAColorTexture", imageCreateInfo, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, subresourceRange, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (!msaaColorTexture.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create msaa color texture.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildShadowTexture(const vkts::ICommandBuffersSP& cmdBuffer)
{
	VkImageCreateInfo imageCreateInfo{};

	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = depthFormat;
	imageCreateInfo.extent = {VKTS_SHADOW_MAP_SIZE, VKTS_SHADOW_MAP_SIZE, 1};
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.pQueueFamilyIndices = nullptr;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

	shadowTexture = vkts::memoryImageCreate(initialResources, cmdBuffer, "ShadowTexture", imageCreateInfo, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, subresourceRange, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (!shadowTexture.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create shadow texture.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildPipeline()
{
	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo[VKTS_SHADER_STAGE_COUNT]{};

	pipelineShaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

	pipelineShaderStageCreateInfo[0].flags = 0;
	pipelineShaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineShaderStageCreateInfo[0].module = standardVertexShaderModule->getShaderModule();
	pipelineShaderStageCreateInfo[0].pName = "main";
	pipelineShaderStageCreateInfo[0].pSpecializationInfo = nullptr;

	pipelineShaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

	pipelineShaderStageCreateInfo[1].flags = 0;
	pipelineShaderStageCreateInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineShaderStageCreateInfo[1].module = standardFragmentShaderModule->getShaderModule();
	pipelineShaderStageCreateInfo[1].pName = "main";
	pipelineShaderStageCreateInfo[1].pSpecializationInfo = nullptr;

	//

	VkTsVertexBufferType vertexBufferType = VKTS_VERTEX_BUFFER_TYPE_VERTEX | VKTS_VERTEX_BUFFER_TYPE_TANGENTS | VKTS_VERTEX_BUFFER_TYPE_TEXCOORD;

	VkVertexInputBindingDescription vertexInputBindingDescription{};

	vertexInputBindingDescription.binding = VKTS_BINDING_VERTEX_BUFFER;
	vertexInputBindingDescription.stride = vkts::commonGetStrideInBytes(vertexBufferType);
	vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertexInputAttributeDescription[5]{};

	vertexInputAttributeDescription[0].location = 0;
	vertexInputAttributeDescription[0].binding = VKTS_BINDING_VERTEX_BUFFER;
	vertexInputAttributeDescription[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexInputAttributeDescription[0].offset = vkts::commonGetOffsetInBytes(VKTS_VERTEX_BUFFER_TYPE_VERTEX, vertexBufferType);

	vertexInputAttributeDescription[1].location = 1;
	vertexInputAttributeDescription[1].binding = VKTS_BINDING_VERTEX_BUFFER;
	vertexInputAttributeDescription[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexInputAttributeDescription[1].offset = vkts::commonGetOffsetInBytes(VKTS_VERTEX_BUFFER_TYPE_NORMAL, vertexBufferType);

	vertexInputAttributeDescription[2].location = 2;
	vertexInputAttributeDescription[2].binding = VKTS_BINDING_VERTEX_BUFFER;
	vertexInputAttributeDescription[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexInputAttributeDescription[2].offset = vkts::commonGetOffsetInBytes(VKTS_VERTEX_BUFFER_TYPE_BITANGENT, vertexBufferType);

	vertexInputAttributeDescription[3].location = 3;
	vertexInputAttributeDescription[3].binding = VKTS_BINDING_VERTEX_BUFFER;
	vertexInputAttributeDescription[3].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexInputAttributeDescription[3].offset = vkts::commonGetOffsetInBytes(VKTS_VERTEX_BUFFER_TYPE_TANGENT, vertexBufferType);

	vertexInputAttributeDescription[4].location = 4;
	vertexInputAttributeDescription[4].binding = VKTS_BINDING_VERTEX_BUFFER;
	vertexInputAttributeDescription[4].format = VK_FORMAT_R32G32_SFLOAT;
	vertexInputAttributeDescription[4].offset = vkts::commonGetOffsetInBytes(VKTS_VERTEX_BUFFER_TYPE_TEXCOORD, vertexBufferType);


	VkPipelineVertexInputStateCreateInfo pipelineVertexInputCreateInfo{};

	pipelineVertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	pipelineVertexInputCreateInfo.flags = 0;
	pipelineVertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	pipelineVertexInputCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
	pipelineVertexInputCreateInfo.vertexAttributeDescriptionCount = 5;
	pipelineVertexInputCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescription;

	//

	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};

	pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

	pipelineInputAssemblyStateCreateInfo.flags = 0;
	pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;


	//

	VkViewport viewport{};

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) swapchain->getImageExtent().width;
	viewport.height = (float) swapchain->getImageExtent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};

	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = swapchain->getImageExtent();

	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};

	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	pipelineViewportStateCreateInfo.flags = 0;
	pipelineViewportStateCreateInfo.viewportCount = 1;
	pipelineViewportStateCreateInfo.pViewports = &viewport;
	pipelineViewportStateCreateInfo.scissorCount = 1;
	pipelineViewportStateCreateInfo.pScissors = &scissor;

	//

	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};

	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

	pipelineRasterizationStateCreateInfo.flags = 0;
	pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

	//

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};

	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

	pipelineMultisampleStateCreateInfo.flags = 0;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = VKTS_SAMPLE_COUNT_BIT;
	pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	pipelineMultisampleStateCreateInfo.minSampleShading = 0.0f;
	pipelineMultisampleStateCreateInfo.pSampleMask = nullptr;
	pipelineMultisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	pipelineMultisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	//

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};

	pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	pipelineDepthStencilStateCreateInfo.flags = 0;
	pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	pipelineDepthStencilStateCreateInfo.front.failOp = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.front.passOp = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.front.compareOp = VK_COMPARE_OP_NEVER;
	pipelineDepthStencilStateCreateInfo.front.compareMask = 0;
	pipelineDepthStencilStateCreateInfo.front.writeMask = 0;
	pipelineDepthStencilStateCreateInfo.front.reference = 0;
	pipelineDepthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.back.depthFailOp = VK_STENCIL_OP_KEEP;
	pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_NEVER;
	pipelineDepthStencilStateCreateInfo.back.compareMask = 0;
	pipelineDepthStencilStateCreateInfo.back.writeMask = 0;
	pipelineDepthStencilStateCreateInfo.back.reference = 0;
	pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.0f;

	//

	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};

	pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
	pipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	pipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	pipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	pipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	pipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	pipelineColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};

	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

	pipelineColorBlendStateCreateInfo.flags = 0;
	pipelineColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	pipelineColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	pipelineColorBlendStateCreateInfo.attachmentCount = 1;
	pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;
	pipelineColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[3] = 0.0f;

	//

	VkDynamicState dynamicState[VKTS_NUMBER_DYNAMIC_STATES] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};

	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

	pipelineDynamicStateCreateInfo.flags = 0;
	pipelineDynamicStateCreateInfo.dynamicStateCount = VKTS_NUMBER_DYNAMIC_STATES;
	pipelineDynamicStateCreateInfo.pDynamicStates = dynamicState;

	//

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};

	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = VKTS_SHADER_STAGE_COUNT;
	graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfo;
	graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = pipelineLayout->getPipelineLayout();
	graphicsPipelineCreateInfo.renderPass = renderPass->getRenderPass();
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;

	//

	auto pipeline = vkts::pipelineCreateGraphics(initialResources->getDevice()->getDevice(), VK_NULL_HANDLE, graphicsPipelineCreateInfo, vertexBufferType);

	if (!pipeline.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create graphics pipeline.");

		return VK_FALSE;
	}

	allOpaqueGraphicsPipelines.append(pipeline);

	//
	// Same as above without writing color.
	//

	pipelineShaderStageCreateInfo[1].module = standardShadowFragmentShaderModule->getShaderModule();


	VkViewport shadowViewport{};

	shadowViewport.x = 0.0f;
	shadowViewport.y = 0.0f;
	shadowViewport.width = (float)VKTS_SHADOW_MAP_SIZE;
	shadowViewport.height = (float)VKTS_SHADOW_MAP_SIZE;
	shadowViewport.minDepth = 0.0f;
	shadowViewport.maxDepth = 1.0f;

	VkRect2D shadowScissor{};

	shadowScissor.offset.x = 0;
	shadowScissor.offset.y = 0;
	shadowScissor.extent = {VKTS_SHADOW_MAP_SIZE, VKTS_SHADOW_MAP_SIZE};

	pipelineViewportStateCreateInfo.pViewports = &shadowViewport;
	pipelineViewportStateCreateInfo.pScissors = &shadowScissor;

	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	graphicsPipelineCreateInfo.pColorBlendState = nullptr;
	graphicsPipelineCreateInfo.renderPass = shadowRenderPass->getRenderPass();

	pipeline = vkts::pipelineCreateGraphics(initialResources->getDevice()->getDevice(), VK_NULL_HANDLE, graphicsPipelineCreateInfo, vertexBufferType);

	if (!pipeline.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create graphics pipeline.");

		return VK_FALSE;
	}

	allShadowGraphicsPipelines.append(pipeline);

	// Revert
	pipelineShaderStageCreateInfo[1].module = standardFragmentShaderModule->getShaderModule();

	pipelineViewportStateCreateInfo.pViewports = &viewport;
	pipelineViewportStateCreateInfo.pScissors = &scissor;

	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	pipelineMultisampleStateCreateInfo.rasterizationSamples = VKTS_SAMPLE_COUNT_BIT;

	graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.renderPass = renderPass->getRenderPass();

	//
	//
	//

	// Same as above with blending.
	pipelineColorBlendAttachmentState.blendEnable = VK_TRUE;

	pipeline = vkts::pipelineCreateGraphics(initialResources->getDevice()->getDevice(), VK_NULL_HANDLE, graphicsPipelineCreateInfo, vertexBufferType);

	if (!pipeline.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create graphics pipeline.");

		return VK_FALSE;
	}

	allBlendGraphicsPipelines.append(pipeline);

	//

	// Same as above with clockwise as front face.
	pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

	pipeline = vkts::pipelineCreateGraphics(initialResources->getDevice()->getDevice(), VK_NULL_HANDLE, graphicsPipelineCreateInfo, vertexBufferType);

	if (!pipeline.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create graphics pipeline.");

		return VK_FALSE;
	}

	allBlendCwGraphicsPipelines.append(pipeline);

	return VK_TRUE;
}

VkBool32 Example::buildRenderPass()
{
	VkAttachmentDescription attachmentDescription[4]{};

	attachmentDescription[0].flags = 0;
	attachmentDescription[0].format = swapchain->getImageFormat();
	attachmentDescription[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentDescription[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	attachmentDescription[1].flags = 0;
	attachmentDescription[1].format = VK_FORMAT_D16_UNORM;
	attachmentDescription[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentDescription[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	attachmentDescription[2].flags = 0;
	attachmentDescription[2].format = swapchain->getImageFormat();	// Later request same format for MSAA image.
	attachmentDescription[2].samples = VKTS_SAMPLE_COUNT_BIT;
	attachmentDescription[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription[2].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachmentDescription[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	attachmentDescription[3].flags = 0;
	attachmentDescription[3].format = VK_FORMAT_D16_UNORM;
	attachmentDescription[3].samples = VKTS_SAMPLE_COUNT_BIT;
	attachmentDescription[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription[3].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentDescription[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference resolveAttachmentReference[2];

	resolveAttachmentReference[0].attachment = 0;
	resolveAttachmentReference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	resolveAttachmentReference[1].attachment = 1;
	resolveAttachmentReference[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentReference;

	colorAttachmentReference.attachment = 2;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference deptStencilAttachmentReference;

	deptStencilAttachmentReference.attachment = 3;
	deptStencilAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//

	VkSubpassDescription subpassDescription[1]{};

	subpassDescription[0].flags = 0;
	subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription[0].inputAttachmentCount = 0;
	subpassDescription[0].pInputAttachments = nullptr;
	subpassDescription[0].colorAttachmentCount = 1;
	subpassDescription[0].pColorAttachments = &colorAttachmentReference;
	subpassDescription[0].pResolveAttachments = resolveAttachmentReference;
	subpassDescription[0].pDepthStencilAttachment = &deptStencilAttachmentReference;
	subpassDescription[0].preserveAttachmentCount = 0;
	subpassDescription[0].pPreserveAttachments = nullptr;

    //

	renderPass = vkts::renderPassCreate( initialResources->getDevice()->getDevice(), 0, 4, attachmentDescription, 1, subpassDescription, 0, nullptr);

	if (!renderPass.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create render pass.");

		return VK_FALSE;
	}

	//
	// Create shadow render pass.
	//

	attachmentDescription[0].format = depthFormat;
	attachmentDescription[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription[0].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentDescription[0].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	deptStencilAttachmentReference.attachment = 0;

	subpassDescription[0].colorAttachmentCount = 0;
	subpassDescription[0].pColorAttachments = nullptr;
	subpassDescription[0].pResolveAttachments = nullptr;

    //

	shadowRenderPass = vkts::renderPassCreate( initialResources->getDevice()->getDevice(), 0, 1, attachmentDescription, 1, subpassDescription, 0, nullptr);

	if (!shadowRenderPass.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create shadow render pass.");

		return VK_FALSE;
	}

	//
	// Create voxel render pass.
	//

	attachmentDescription[0].format = VK_FORMAT_R32_UINT;
	attachmentDescription[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription[0].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	attachmentDescription[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;

	subpassDescription[0].colorAttachmentCount = 0;
	subpassDescription[0].pColorAttachments = nullptr;
	subpassDescription[0].pResolveAttachments = nullptr;
	subpassDescription[0].pDepthStencilAttachment = nullptr;

    //

	voxelRenderPass = vkts::renderPassCreate(initialResources->getDevice()->getDevice(), 0, 1, attachmentDescription, 1, subpassDescription, 0, nullptr);

	if (!voxelRenderPass.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create voxel render pass.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildPipelineLayout()
{
	VkDescriptorSetLayout setLayouts[1];

	setLayouts[0] = descriptorSetLayout->getDescriptorSetLayout();

	pipelineLayout = vkts::pipelineCreateLayout(initialResources->getDevice()->getDevice(), 0, 1, setLayouts, 0, nullptr);

	if (!pipelineLayout.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create pipeline layout.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[VKTS_DESCRIPTOR_SET_COUNT]{};

	descriptorSetLayoutBinding[0].binding = VKTS_BINDING_UNIFORM_BUFFER_VIEWPROJECTION;
	descriptorSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBinding[0].descriptorCount = 1;
	descriptorSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	descriptorSetLayoutBinding[0].pImmutableSamplers = nullptr;

	descriptorSetLayoutBinding[1].binding = VKTS_BINDING_UNIFORM_BUFFER_LIGHT;
	descriptorSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBinding[1].descriptorCount = 1;
	descriptorSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	descriptorSetLayoutBinding[1].pImmutableSamplers = nullptr;

	descriptorSetLayoutBinding[2].binding = VKTS_BINDING_UNIFORM_SAMPLER_SHADOW;
	descriptorSetLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorSetLayoutBinding[2].descriptorCount = 1;
	descriptorSetLayoutBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	descriptorSetLayoutBinding[2].pImmutableSamplers = nullptr;

	descriptorSetLayoutBinding[3].binding = VKTS_BINDING_UNIFORM_BUFFER_SHADOW;
	descriptorSetLayoutBinding[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBinding[3].descriptorCount = 1;
	descriptorSetLayoutBinding[3].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	descriptorSetLayoutBinding[3].pImmutableSamplers = nullptr;

	descriptorSetLayoutBinding[4].binding = VKTS_BINDING_UNIFORM_BUFFER_TRANSFORM;
	descriptorSetLayoutBinding[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBinding[4].descriptorCount = 1;
	descriptorSetLayoutBinding[4].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	descriptorSetLayoutBinding[4].pImmutableSamplers = nullptr;

    for (int32_t i = VKTS_BINDING_UNIFORM_SAMPLER_PHONG_FIRST; i <= VKTS_BINDING_UNIFORM_SAMPLER_PHONG_LAST; i++)
    {
		descriptorSetLayoutBinding[5 + i - VKTS_BINDING_UNIFORM_SAMPLER_PHONG_FIRST].binding = i;
		descriptorSetLayoutBinding[5 + i - VKTS_BINDING_UNIFORM_SAMPLER_PHONG_FIRST].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorSetLayoutBinding[5 + i - VKTS_BINDING_UNIFORM_SAMPLER_PHONG_FIRST].descriptorCount = 1;
		descriptorSetLayoutBinding[5 + i - VKTS_BINDING_UNIFORM_SAMPLER_PHONG_FIRST].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptorSetLayoutBinding[5 + i - VKTS_BINDING_UNIFORM_SAMPLER_PHONG_FIRST].pImmutableSamplers = nullptr;
    }

    //

	descriptorSetLayout = vkts::descriptorSetLayoutCreate(initialResources->getDevice()->getDevice(), 0, VKTS_DESCRIPTOR_SET_COUNT, descriptorSetLayoutBinding);

	if (!descriptorSetLayout.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create descriptor set layout.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildShader()
{
	auto vertexShaderBinary = vkts::fileLoadBinary(VKTS_STANDARD_VERTEX_SHADER_NAME);

	if (!vertexShaderBinary.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not load vertex shader: '%s'", VKTS_STANDARD_VERTEX_SHADER_NAME);

		return VK_FALSE;
	}

	auto fragmentShaderBinary = vkts::fileLoadBinary(VKTS_STANDARD_FRAGMENT_SHADER_NAME);

	if (!fragmentShaderBinary.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not load fragment shader: '%s'", VKTS_STANDARD_FRAGMENT_SHADER_NAME);

		return VK_FALSE;
	}

	//

	standardVertexShaderModule = vkts::shaderModuleCreate(VKTS_STANDARD_VERTEX_SHADER_NAME, initialResources->getDevice()->getDevice(), 0, vertexShaderBinary->getSize(), (uint32_t*)vertexShaderBinary->getData());

	if (!standardVertexShaderModule.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create vertex shader module.");

		return VK_FALSE;
	}

	standardFragmentShaderModule = vkts::shaderModuleCreate(VKTS_STANDARD_FRAGMENT_SHADER_NAME, initialResources->getDevice()->getDevice(), 0, fragmentShaderBinary->getSize(), (uint32_t*)fragmentShaderBinary->getData());

	if (!standardFragmentShaderModule.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create fragment shader module.");

		return VK_FALSE;
	}

	fragmentShaderBinary = vkts::fileLoadBinary(VKTS_STANDARD_SHADOW_FRAGMENT_SHADER_NAME);

	if (!fragmentShaderBinary.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not load fragment shader: '%s'", VKTS_STANDARD_SHADOW_FRAGMENT_SHADER_NAME);

		return VK_FALSE;
	}

	standardShadowFragmentShaderModule = vkts::shaderModuleCreate(VKTS_STANDARD_SHADOW_FRAGMENT_SHADER_NAME, initialResources->getDevice()->getDevice(), 0, fragmentShaderBinary->getSize(), (uint32_t*)fragmentShaderBinary->getData());

	if (!standardShadowFragmentShaderModule.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create fragment shader module.");

		return VK_FALSE;
	}

	//
	// Shader modules for voxelisation.
	//

	vertexShaderBinary = vkts::fileLoadBinary(VKTS_VOXELIZE_VERTEX_SHADER_NAME);

	if (!vertexShaderBinary.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not load vertex shader: '%s'", VKTS_VOXELIZE_VERTEX_SHADER_NAME);

		return VK_FALSE;
	}


	auto geometryShaderBinary = vkts::fileLoadBinary(VKTS_VOXELIZE_GEOMETRY_SHADER_NAME);

	if (!geometryShaderBinary.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not load geometry shader: '%s'", VKTS_VOXELIZE_GEOMETRY_SHADER_NAME);

		return VK_FALSE;
	}

	fragmentShaderBinary = vkts::fileLoadBinary(VKTS_VOXELIZE_FRAGMENT_SHADER_NAME);

	if (!fragmentShaderBinary.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not load fragment shader: '%s'", VKTS_VOXELIZE_FRAGMENT_SHADER_NAME);

		return VK_FALSE;
	}

	//

	voxelizeVertexShaderModule = vkts::shaderModuleCreate(VKTS_VOXELIZE_VERTEX_SHADER_NAME, initialResources->getDevice()->getDevice(), 0, vertexShaderBinary->getSize(), (uint32_t*)vertexShaderBinary->getData());

	if (!voxelizeVertexShaderModule.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create vertex shader module.");

		return VK_FALSE;
	}

	voxelizeGeometryShaderModule = vkts::shaderModuleCreate(VKTS_VOXELIZE_GEOMETRY_SHADER_NAME, initialResources->getDevice()->getDevice(), 0, geometryShaderBinary->getSize(), (uint32_t*)geometryShaderBinary->getData());

	if (!voxelizeGeometryShaderModule.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create geometry shader module.");

		return VK_FALSE;
	}

	voxelizeFragmentShaderModule = vkts::shaderModuleCreate(VKTS_VOXELIZE_FRAGMENT_SHADER_NAME, initialResources->getDevice()->getDevice(), 0, fragmentShaderBinary->getSize(), (uint32_t*)fragmentShaderBinary->getData());

	if (!voxelizeFragmentShaderModule.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create fragment shader module.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildUniformBuffers()
{
	VkBufferCreateInfo bufferCreateInfo{};

	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = vkts::commonGetDeviceSize(16 * sizeof(float) * 2, 16);
	bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;
	bufferCreateInfo.pQueueFamilyIndices = nullptr;

	vertexViewProjectionUniformBuffer = vkts::bufferObjectCreate(initialResources, bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (!vertexViewProjectionUniformBuffer.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create vertex uniform buffer.");

		return VK_FALSE;
	}

	memset(&bufferCreateInfo, 0, sizeof(VkBufferCreateInfo));

	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = vkts::commonGetDeviceSize(3 * sizeof(float), 16);
	bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;
	bufferCreateInfo.pQueueFamilyIndices = nullptr;

	fragmentUniformBuffer = vkts::bufferObjectCreate(initialResources, bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (!fragmentUniformBuffer.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create fragment uniform buffer.");

		return VK_FALSE;
	}

	memset(&bufferCreateInfo, 0, sizeof(VkBufferCreateInfo));

	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = vkts::commonGetDeviceSize(16 * sizeof(float), 16);
	bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;
	bufferCreateInfo.pQueueFamilyIndices = nullptr;

	shadowUniformBuffer = vkts::bufferObjectCreate(initialResources, bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (!shadowUniformBuffer.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create shadow uniform buffer.");

		return VK_FALSE;
	}

	//
	// Uniform buffers for voxelization.
	//

	memset(&bufferCreateInfo, 0, sizeof(VkBufferCreateInfo));

	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = vkts::commonGetDeviceSize(16 * sizeof(float) * 2, 16);
	bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;
	bufferCreateInfo.pQueueFamilyIndices = nullptr;

	voxelizeViewProjectionUniformBuffer = vkts::bufferObjectCreate(initialResources, bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (!voxelizeViewProjectionUniformBuffer.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create first uniform buffer for voxelization.");

		return VK_FALSE;
	}

	memset(&bufferCreateInfo, 0, sizeof(VkBufferCreateInfo));

	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = vkts::commonGetDeviceSize(16 * sizeof(float) + 12 * sizeof(float), 16);
	bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;
	bufferCreateInfo.pQueueFamilyIndices = nullptr;

	voxelizeModelNormalUniformBuffer = vkts::bufferObjectCreate(initialResources, bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (!voxelizeModelNormalUniformBuffer.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create second uniform buffer for voxelization.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 Example::buildResources(const vkts::IUpdateThreadContext& updateContext)
{
	VkResult result;

	//

	auto lastSwapchain = swapchain;

	VkSwapchainKHR oldSwapchain = lastSwapchain.get() ? lastSwapchain->getSwapchain() : VK_NULL_HANDLE;

	swapchain = vkts::wsiSwapchainCreate(initialResources->getPhysicalDevice()->getPhysicalDevice(), initialResources->getDevice()->getDevice(), 0, surface->getSurface(), VKTS_NUMBER_BUFFERS, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_TRUE, oldSwapchain);

	if (!swapchain.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create swap chain.");

		return VK_FALSE;
	}

    //

    swapchainImagesCount = (uint32_t)swapchain->getAllSwapchainImages().size();

    if (swapchainImagesCount == 0)
    {
        vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not get swap chain images count.");

        return VK_FALSE;
    }

    swapchainImageView = vkts::SmartPointerVector<vkts::IImageViewSP>(swapchainImagesCount);
    framebuffer = vkts::SmartPointerVector<vkts::IFramebufferSP>(swapchainImagesCount);
    fences = vkts::SmartPointerVector<vkts::IFenceSP>(swapchainImagesCount);
    shadowFramebuffer = vkts::SmartPointerVector<vkts::IFramebufferSP>(swapchainImagesCount);
    cmdBuffer = vkts::SmartPointerVector<vkts::ICommandBuffersSP>(swapchainImagesCount);

    //

	if (lastSwapchain.get())
	{
		lastSwapchain->destroy();
	}

	//

	if (!buildRenderPass())
	{
		return VK_FALSE;
	}

	if (!buildPipeline())
	{
		return VK_FALSE;
	}

	//

	vkts::ICommandBuffersSP updateCmdBuffer = vkts::commandBuffersCreate(initialResources->getDevice()->getDevice(), commandPool->getCmdPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

	if (!updateCmdBuffer.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create command buffer.");

		return VK_FALSE;
	}


	result = updateCmdBuffer->beginCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, VK_FALSE, 0, 0);

	if (result != VK_SUCCESS)
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not begin command buffer.");

		return VK_FALSE;
	}

	if (!buildShadowTexture(updateCmdBuffer))
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build MSAA color texture.");

		return VK_FALSE;
	}

	if (!buildMSAAColorTexture(updateCmdBuffer))
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build MSAA color texture.");

		return VK_FALSE;
	}

	if (!buildMSAADepthTexture(updateCmdBuffer))
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build MSAA depth texture.");

		return VK_FALSE;
	}

	if (!buildDepthTexture(updateCmdBuffer))
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build depth texture.");

		return VK_FALSE;
	}

	if (!buildVoxelTexture(updateCmdBuffer))
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build voxel texture.");

		return VK_FALSE;
	}

	result = updateCmdBuffer->endCommandBuffer();

	if (result != VK_SUCCESS)
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not end command buffer.");

		return VK_FALSE;
	}


	VkSubmitInfo submitInfo{};

	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.commandBufferCount = updateCmdBuffer->getCommandBufferCount();
	submitInfo.pCommandBuffers = updateCmdBuffer->getCommandBuffers();
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	result = initialResources->getQueue()->submit(1, &submitInfo, VK_NULL_HANDLE);

	if (result != VK_SUCCESS)
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not submit queue.");

		return VK_FALSE;
	}

	result = initialResources->getQueue()->waitIdle();

	if (result != VK_SUCCESS)
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not wait for idle queue.");

		return VK_FALSE;
	}

	updateCmdBuffer->destroy();

	//

	if (!buildShadowImageView())
	{
		return VK_FALSE;
	}

	if (!buildMSAAColorImageView())
	{
		return VK_FALSE;
	}

	if (!buildMSAADepthStencilImageView())
	{
		return VK_FALSE;
	}

	if (!buildDepthStencilImageView())
	{
		return VK_FALSE;
	}


	if (!buildShadowSampler())
	{
		return VK_FALSE;
	}

	//

	if (sceneLoaded)
	{
		if (!updateDescriptorSets())
		{
			return VK_FALSE;
		}

		if (scene.get())
		{
			scene->updateDescriptorSetsRecursive(VKTS_DESCRIPTOR_SET_COUNT, writeDescriptorSets);
		}
	}

	for (int32_t i = 0; i < (int32_t)swapchainImagesCount; i++)
	{
		if (!buildSwapchainImageView(i))
		{
			return VK_FALSE;
		}

		if (!buildFramebuffer(i))
		{
			return VK_FALSE;
		}

		if (!buildFences(i))
		{
			return VK_FALSE;
		}

		if (sceneLoaded)
		{
			if (!buildCmdBuffer(i))
			{
				return VK_FALSE;
			}
		}
	}

	return VK_TRUE;
}

void Example::terminateResources(const vkts::IUpdateThreadContext& updateContext)
{
	if (initialResources.get())
	{
		if (initialResources->getDevice().get())
		{
			for (int32_t i = 0; i < (int32_t)swapchainImagesCount; i++)
			{
				if (cmdBuffer[i].get())
				{
					cmdBuffer[i]->destroy();
				}

				if (shadowCmdBuffer[i].get())
				{
					shadowCmdBuffer[i]->destroy();
				}

				if (fences[i].get())
				{
					fences[i]->destroy();
				}

				if (shadowFramebuffer[i].get())
				{
					shadowFramebuffer[i]->destroy();
				}

				if (framebuffer[i].get())
				{
					framebuffer[i]->destroy();
				}

				if (swapchainImageView[i].get())
				{
					swapchainImageView[i]->destroy();
				}
			}

			if (shadowSampler.get())
			{
				shadowSampler->destroy();
			}

			if (depthStencilImageView.get())
			{
				depthStencilImageView->destroy();
			}

			if (msaaDepthStencilImageView.get())
			{
				msaaDepthStencilImageView->destroy();
			}

			if (msaaColorImageView.get())
			{
				msaaColorImageView->destroy();
			}

			if (shadowImageView.get())
			{
				shadowImageView->destroy();
			}

			if (depthTexture.get())
			{
				depthTexture->destroy();
			}

			if (msaaDepthTexture.get())
			{
				msaaDepthTexture->destroy();
			}

			if (msaaColorTexture.get())
			{
				msaaColorTexture->destroy();
			}

			if (shadowTexture.get())
			{
				shadowTexture->destroy();
			}

			for (size_t i = 0; i < allShadowGraphicsPipelines.size(); i++)
			{
				allShadowGraphicsPipelines[i]->destroy();
			}
			allShadowGraphicsPipelines.clear();

			for (size_t i = 0; i < allBlendGraphicsPipelines.size(); i++)
			{
				allBlendGraphicsPipelines[i]->destroy();
			}
			allBlendGraphicsPipelines.clear();

			for (size_t i = 0; i < allBlendCwGraphicsPipelines.size(); i++)
			{
				allBlendCwGraphicsPipelines[i]->destroy();
			}
			allBlendCwGraphicsPipelines.clear();

			for (size_t i = 0; i < allOpaqueGraphicsPipelines.size(); i++)
			{
				allOpaqueGraphicsPipelines[i]->destroy();
			}
			allOpaqueGraphicsPipelines.clear();

			if (voxelRenderPass.get())
			{
				voxelRenderPass->destroy();
			}

			if (shadowRenderPass.get())
			{
				shadowRenderPass->destroy();
			}

			if (renderPass.get())
			{
				renderPass->destroy();
			}
		}
	}
}

//
// Vulkan initialization.
//
VkBool32 Example::init(const vkts::IUpdateThreadContext& updateContext)
{
	if (!updateContext.isWindowAttached(windowIndex))
	{
		return VK_FALSE;
	}

	//

	VkFormatProperties formatProperties{};

	initialResources->getPhysicalDevice()->getGetPhysicalDeviceFormatProperties(formatProperties, depthFormat);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
	{
		depthFormat = VK_FORMAT_D16_UNORM;
	}

	//

	surface->hasCurrentExtentChanged(initialResources->getPhysicalDevice()->getPhysicalDevice());

	//

	camera = vkts::userCameraCreate(glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

	if (!camera.get())
	{
		return VK_FALSE;
	}

	allUpdateables.append(camera);

	inputController = vkts::inputControllerCreate(updateContext, windowIndex, 0, camera);

	if (!inputController.get())
	{
		return VK_FALSE;
	}

	allUpdateables.insert(0, inputController);

	//

	commandPool = vkts::commandPoolCreate(initialResources->getDevice()->getDevice(), 0, initialResources->getQueue()->getQueueFamilyIndex());

	if (!commandPool.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not get command pool.");

		return VK_FALSE;
	}

	//

    imageAcquiredSemaphore = vkts::semaphoreCreate(initialResources->getDevice()->getDevice(), 0);

    if (!imageAcquiredSemaphore.get())
    {
        vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create semaphore.");

        return VK_FALSE;
    }

    renderingCompleteSemaphore = vkts::semaphoreCreate(initialResources->getDevice()->getDevice(), 0);

    if (!renderingCompleteSemaphore.get())
    {
        vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create semaphore.");

        return VK_FALSE;
    }

	//

	if (!buildUniformBuffers())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build uniform buffers.");

		return VK_FALSE;
	}

	if (!buildShader())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build shader.");

		return VK_FALSE;
	}

	if (!buildDescriptorSetLayout())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build descriptor set layout.");

		return VK_FALSE;
	}

	if (!buildPipelineLayout())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build pipeline cache.");

		return VK_FALSE;
	}

	//

	loadTask = ILoadTaskSP(new LoadTask(initialResources, descriptorSetLayout, sceneContext, scene));

	if (!loadTask.get())
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not create load task.");

		return VK_FALSE;
	}

	if (!updateContext.sendTask(loadTask))
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not send load task.");

		return VK_FALSE;
	}

	//

	if (!buildResources(updateContext))
	{
		vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build resources.");

		return VK_FALSE;
	}

	return VK_TRUE;
}

//
// Vulkan update.
//
VkBool32 Example::update(const vkts::IUpdateThreadContext& updateContext)
{
	if (sceneLoaded)
	{
		for (size_t i = 0; i < allUpdateables.size(); i++)
		{
			allUpdateables[i]->update(updateContext.getDeltaTime(), updateContext.getDeltaTicks());
		}

		//

		VkResult result = VK_SUCCESS;

		//

		if (surface->hasCurrentExtentChanged(initialResources->getPhysicalDevice()->getPhysicalDevice()))
		{
			const auto& currentExtent = surface->getCurrentExtent(initialResources->getPhysicalDevice()->getPhysicalDevice(), VK_FALSE);

			if (currentExtent.width == 0 || currentExtent.height == 0)
			{
				return VK_TRUE;
			}

			result = VK_ERROR_OUT_OF_DATE_KHR;
		}

		//

		uint32_t currentBuffer;

		if (result == VK_SUCCESS)
		{
			result = swapchain->acquireNextImage(UINT64_MAX, imageAcquiredSemaphore->getSemaphore(), VK_NULL_HANDLE, currentBuffer);
		}

		if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
		{
			glm::mat4 projectionMatrix(1.0f);
			glm::mat4 viewMatrix(1.0f);
			glm::mat4 shadowMatrix(1.0f);

			glm::vec3 worldLightDirection = vkts::rotateRx(10.0f) * glm::vec3(0.0f, 1.0f, 0.0f);

			//
			// Shadow
			//

			projectionMatrix = vkts::orthoMat4(-VKTS_SHADOW_MAP_SIZE * 0.5f * VKTS_SHADOW_CAMERA_SCALE, VKTS_SHADOW_MAP_SIZE * 0.5f * VKTS_SHADOW_CAMERA_SCALE, -VKTS_SHADOW_MAP_SIZE * 0.5f * VKTS_SHADOW_CAMERA_SCALE, VKTS_SHADOW_MAP_SIZE * 0.5f * VKTS_SHADOW_CAMERA_SCALE, 0.0f, VKTS_SHADOW_CAMERA_ORTHO_FAR);

			// Get view matrix from light.
			viewMatrix = vkts::lookAtMat4(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) + glm::vec4(glm::normalize(worldLightDirection) * VKTS_SHADOW_CAMERA_DISTANCE, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			// Bias matrix to convert to window space.
			glm::mat4 biasMatrix = vkts::translateMat4(0.5f, 0.5f, 0.0f) * vkts::scaleMat4(0.5f, 0.5f, 1.0f);

			shadowMatrix = biasMatrix * projectionMatrix * viewMatrix;

			if (!vertexViewProjectionUniformBuffer->upload(0 * sizeof(float) * 16, 0, projectionMatrix))
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not upload matrices.");

				return VK_FALSE;
			}
			if (!vertexViewProjectionUniformBuffer->upload(1 * sizeof(float) * 16, 0, viewMatrix))
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not upload matrices.");

				return VK_FALSE;
			}

			if (scene.get())
			{
				scene->updateRecursive(updateContext);
			}

			//

			VkSemaphore waitSemaphores = imageAcquiredSemaphore->getSemaphore();

			VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

			VkSubmitInfo submitInfo{};

			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &waitSemaphores;
			submitInfo.pWaitDstStageMask = &waitDstStageMask;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = shadowCmdBuffer[currentBuffer]->getCommandBuffers();
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;

			// Added fence for later waiting.
			result = initialResources->getQueue()->submit(1, &submitInfo, fences[currentBuffer]->getFence());

			if (result != VK_SUCCESS)
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not submit queue.");

				return VK_FALSE;
			}

			//
			// Wait for fence, as view projection buffer is used by both commands.
			//

			result = fences[currentBuffer]->waitForFence(UINT64_MAX);

			if (result != VK_SUCCESS)
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not wait for fence.");

				return VK_FALSE;
			}

			result = fences[currentBuffer]->reset();

			if (result != VK_SUCCESS)
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not reset fence.");

				return VK_FALSE;
			}

			//
			// Color
			//

			const auto& currentExtent = surface->getCurrentExtent(initialResources->getPhysicalDevice()->getPhysicalDevice(), VK_FALSE);

			projectionMatrix = vkts::perspectiveMat4(45.0f, (float)currentExtent.width / (float)currentExtent.height, 1.0f, 1000.0f);

			viewMatrix = camera->getViewMatrix();

			glm::vec3 lightDirection = glm::mat3(viewMatrix) * worldLightDirection;

			lightDirection = glm::normalize(lightDirection);

			if (!fragmentUniformBuffer->upload(0, 0, lightDirection))
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not upload light direction.");

				return VK_FALSE;
			}
			if (!vertexViewProjectionUniformBuffer->upload(0 * sizeof(float) * 16, 0, projectionMatrix))
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not upload matrices.");

				return VK_FALSE;
			}
			if (!vertexViewProjectionUniformBuffer->upload(1 * sizeof(float) * 16, 0, viewMatrix))
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not upload matrices.");

				return VK_FALSE;
			}
			if (!shadowUniformBuffer->upload(0, 0, shadowMatrix))
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not upload shadow matrix.");

				return VK_FALSE;
			}

			// Scene already updated.

			//

			VkSemaphore signalSemaphores = renderingCompleteSemaphore->getSemaphore();


			memset(&submitInfo, 0, sizeof(VkSubmitInfo));

			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.pWaitDstStageMask = nullptr;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = cmdBuffer[currentBuffer]->getCommandBuffers();
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &signalSemaphores;

			result = initialResources->getQueue()->submit(1, &submitInfo, VK_NULL_HANDLE);

			if (result != VK_SUCCESS)
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not submit queue.");

				return VK_FALSE;
			}

			waitSemaphores = renderingCompleteSemaphore->getSemaphore();

			VkSwapchainKHR swapchains = swapchain->getSwapchain();

			result = swapchain->queuePresent(initialResources->getQueue()->getQueue(), 1, &waitSemaphores, 1, &swapchains, &currentBuffer, nullptr);

			if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
			{
				result = initialResources->getQueue()->waitIdle();

				if (result != VK_SUCCESS)
				{
					vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not wait for idle queue.");

					return VK_FALSE;
				}
			}
			else
			{
				if (result == VK_ERROR_OUT_OF_DATE_KHR)
				{
					terminateResources(updateContext);

					if (!buildResources(updateContext))
					{
						vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build resources.");

						return VK_FALSE;
					}
				}
				else
				{
					vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not present queue.");

					return VK_FALSE;
				}
			}
		}
		else
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				terminateResources(updateContext);

				if (!buildResources(updateContext))
				{
					vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not build resources.");

					return VK_FALSE;
				}
			}
			else
			{
				vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not acquire next image.");

				return VK_FALSE;
			}
		}
	}
	else
	{
		vkts::ITaskSP executedTask;

		// Do not wait.
		if (!updateContext.receiveExecutedTask(executedTask, VK_FALSE))
		{
			return VK_TRUE;
		}

		//

		vkts::logPrint(VKTS_LOG_INFO, __FILE__, __LINE__, "Scene loaded");

		sceneLoaded = VK_TRUE;

		//

		VkCommandBuffer updateCommandBuffer = loadTask->getCommandBuffer();

		//

		VkSubmitInfo submitInfo{};

		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &updateCommandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		auto result = initialResources->getQueue()->submit(1, &submitInfo, VK_NULL_HANDLE);

		if (result != VK_SUCCESS)
		{
			vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not submit queue.");

			return VK_FALSE;
		}

		result = initialResources->getQueue()->waitIdle();

		if (result != VK_SUCCESS)
		{
			vkts::logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Could not wait for idle queue.");

			return VK_FALSE;
		}

		//


		// Destroys the load task.
		loadTask = ILoadTaskSP();

		//

		if (!updateDescriptorSets())
		{
			return VK_FALSE;
		}

		if (scene.get())
		{
			scene->updateDescriptorSetsRecursive(VKTS_DESCRIPTOR_SET_COUNT, writeDescriptorSets);
		}

		for (int32_t i = 0; i < (int32_t)swapchainImagesCount; i++)
		{
			if (!buildCmdBuffer(i))
			{
				return VK_FALSE;
			}
		}
	}

	return VK_TRUE;
}

//
// Vulkan termination.
//
void Example::terminate(const vkts::IUpdateThreadContext& updateContext)
{
	if (loadTask.get() && !sceneLoaded)
	{
		vkts::ITaskSP executedTask;

		// Wait, until finished.
		updateContext.receiveExecutedTask(executedTask);

		loadTask = ILoadTaskSP();
	}

	if (initialResources.get())
	{
		if (initialResources->getDevice().get())
		{
			terminateResources(updateContext);

			//

			for (uint32_t i = 0; i < 3; i++)
			{
				if (voxelTexture[i].get())
				{
					voxelTexture[i]->destroy();
				}
			}

			//

			if (sceneContext.get())
			{
				sceneContext->destroy();

				sceneContext.reset();
			}

			if (scene.get())
			{
				scene->destroy();
			}

			if (swapchain.get())
			{
				swapchain->destroy();
			}

			if (pipelineLayout.get())
			{
				pipelineLayout->destroy();
			}

			if (voxelizeVertexShaderModule.get())
			{
				voxelizeVertexShaderModule->destroy();
			}

			if (voxelizeGeometryShaderModule.get())
			{
				voxelizeGeometryShaderModule->destroy();
			}

			if (voxelizeFragmentShaderModule.get())
			{
				voxelizeFragmentShaderModule->destroy();
			}

			if (standardVertexShaderModule.get())
			{
				standardVertexShaderModule->destroy();
			}

			if (standardFragmentShaderModule.get())
			{
				standardFragmentShaderModule->destroy();
			}

			if (standardShadowFragmentShaderModule.get())
			{
				standardShadowFragmentShaderModule->destroy();
			}

			if (voxelizeViewProjectionUniformBuffer.get())
			{
				voxelizeViewProjectionUniformBuffer->destroy();
			}

			if (voxelizeModelNormalUniformBuffer.get())
			{
				voxelizeModelNormalUniformBuffer->destroy();
			}

			if (vertexViewProjectionUniformBuffer.get())
			{
				vertexViewProjectionUniformBuffer->destroy();
			}

			if (fragmentUniformBuffer.get())
			{
				fragmentUniformBuffer->destroy();
			}

			if (shadowUniformBuffer.get())
			{
				shadowUniformBuffer->destroy();
			}

			if (descriptorSetLayout.get())
			{
				descriptorSetLayout->destroy();
			}

            if (renderingCompleteSemaphore.get())
            {
                renderingCompleteSemaphore->destroy();
            }

            if (imageAcquiredSemaphore.get())
            {
                imageAcquiredSemaphore->destroy();
            }

			if (commandPool.get())
			{
				commandPool->destroy();
			}
		}
	}
}
