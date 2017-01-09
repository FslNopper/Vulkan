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

#include <vkts/scenegraph/vkts_scenegraph.hpp>

#include "GltfVisitor.hpp"

namespace vkts
{

static VkBool32 gltfProcessSubMesh(ISubMeshSP& subMesh, const GltfVisitor& visitor, const GltfPrimitive& gltfPrimitive, const ISceneManagerSP& sceneManager, const ISceneFactorySP& sceneFactory)
{
	if (!gltfPrimitive.position)
	{
		return VK_FALSE;
	}

	subMesh->setNumberVertices((int32_t)gltfPrimitive.position->count);

	uint32_t totalSize = 0;
	uint32_t strideInBytes = 0;

    VkTsVertexBufferType vertexBufferType = 0;

    //

    subMesh->setVertexOffset(strideInBytes);
    strideInBytes += 4 * sizeof(float);

    totalSize += 4 * sizeof(float) * subMesh->getNumberVertices();

    vertexBufferType |= VKTS_VERTEX_BUFFER_TYPE_VERTEX;

	if (gltfPrimitive.normal)
	{
		// TODO: Process normal.
	}

	if (gltfPrimitive.binormal)
	{
		// TODO: Process binormal.
	}

	if (gltfPrimitive.tangent)
	{
		// TODO: Process tangent.
	}

	if (gltfPrimitive.texCoord)
	{
		// TODO: Process texCoord.
	}

	if (gltfPrimitive.joint)
	{
		// TODO: Process joint.
	}

	if (gltfPrimitive.weight)
	{
		// TODO: Process weight.
	}

	// TODO: Create buffer and Write data.

	//
	// Indices
	//

	if (gltfPrimitive.indices)
	{
		subMesh->setNumberIndices((int32_t)gltfPrimitive.indices->count);
	}
	else
	{
		// TODO: Create own indices list.

		subMesh->setNumberIndices((int32_t)gltfPrimitive.position->count);
	}

	// TODO: Create buffer and Write data.

	switch (gltfPrimitive.mode)
	{
		case 0:
			subMesh->setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
			break;
		case 1:
			subMesh->setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
			break;
		case 2:
			return VK_FALSE;
			break;
		case 3:
			subMesh->setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
			break;
		case 4:
			subMesh->setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
			break;
		case 5:
			subMesh->setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
			break;
		case 6:
			subMesh->setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
			break;
		default:
			return VK_FALSE;
	}

	return VK_TRUE;
}

static VkBool32 gltfProcessNode(INodeSP& node, const GltfVisitor& visitor, const GltfNode& gltfNode, const ISceneManagerSP& sceneManager, const ISceneFactorySP& sceneFactory)
{
	node->setNodeRotationMode(VKTS_EULER_XYZ);

	//

	if (gltfNode.useMatrix)
	{
		// Process matrix.

		glm::mat4 matrix;

		for (uint32_t i = 0; i < 16; i++)
		{
			matrix[i / 4][i % 4] = gltfNode.matrix[i];
		}

		node->setTranslate(decomposeTranslate(matrix));
		node->setRotate(decomposeRotateRzRyRx(matrix));
		node->setScale(decomposeScale(matrix));
	}
	else
	{
		// Process translation, rotation and scale.

		if (gltfNode.useTranslation)
		{
			node->setTranslate(glm::vec3(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2]));
		}

		if (gltfNode.useRotation)
		{
			Quat quat(gltfNode.rotation[0], gltfNode.rotation[1], gltfNode.rotation[2], gltfNode.rotation[3]);

			node->setRotate(decomposeRotateRzRyRx(quat.mat4()));
		}

		if (gltfNode.useScale)
		{
			node->setScale(glm::vec3(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]));
		}
	}

    if (gltfNode.skin)
    {
    	// Process skin, so this is an armature node.

    	if (!sceneFactory->getSceneRenderFactory()->prepareJointsUniformBuffer(sceneManager, node, (int32_t)gltfNode.skin->jointNodes.size()))
    	{
            return VK_FALSE;
    	}

		glm::mat4 matrix;

		for (uint32_t i = 0; i < 16; i++)
		{
			matrix[i / 4][i % 4] = gltfNode.skin->bindShapeMatrix[i];
		}

		node->setTranslate(decomposeTranslate(matrix));
		node->setRotate(decomposeRotateRzRyRx(matrix));
		node->setScale(decomposeScale(matrix));
    }

    if (gltfNode.jointName != "")
    {
    	// Process jointName, so this is a joint node.

    	if (!node->getParentNode().get())
    	{
    		return VK_FALSE;
    	}

    	auto currentParentNode = node->getParentNode();

    	while (currentParentNode.get())
    	{
    		if (currentParentNode->isArmature())
    		{
    			break;
    		}

    		currentParentNode = currentParentNode->getParentNode();
    	}

    	if (!currentParentNode.get())
    	{
    		return VK_FALSE;
    	}

    	//

    	const auto& armatureGltfNode = visitor.getAllGltfNodes()[currentParentNode->getName()];

    	if (!armatureGltfNode.skin)
    	{
    		return VK_FALSE;
    	}

    	auto jointIndex = armatureGltfNode.skin->jointNames.index(node->getName());

    	if (jointIndex == armatureGltfNode.skin->jointNames.size())
    	{
    		return VK_FALSE;
    	}

    	node->setJointIndex((int32_t)jointIndex);

    	//

    	// Not using inverse bind matrix, as calculated by the engine.
    }

    // Process meshes.
    for (uint32_t i = 0; i < gltfNode.meshes.size(); i++)
    {
        auto mesh = sceneFactory->createMesh(sceneManager);

        if (!mesh.get())
        {
            return VK_FALSE;
        }

        mesh->setName(node->getName() + "_Mesh_" + std::to_string(i));

        sceneManager->addMesh(mesh);

        //

        for (uint32_t k = 0; k < gltfNode.meshes[i]->primitives.size(); k++)
        {
            auto subMesh = sceneFactory->createSubMesh(sceneManager);

            if (!subMesh.get())
            {
                return VK_FALSE;
            }

            subMesh->setName(node->getName() + "_SubMesh_" + std::to_string(k));

            sceneManager->addSubMesh(subMesh);

            //

            if (!gltfProcessSubMesh(subMesh, visitor, gltfNode.meshes[i]->primitives[k], sceneManager, sceneFactory))
            {
            	return VK_FALSE;
            }

            //

            auto currentSubMesh = sceneManager->useSubMesh(subMesh->getName());

            mesh->addSubMesh(currentSubMesh);
        }

        //

        auto currentMesh = sceneManager->useMesh(mesh->getName());

        if (!currentMesh.get())
        {
            return VK_FALSE;
        }

        node->addMesh(currentMesh);
    }

	// Process children.
    for (uint32_t i = 0; i < gltfNode.childrenPointer.size(); i++)
    {
        auto childNode = sceneFactory->createNode(sceneManager);

        if (!childNode.get())
        {
            return VK_FALSE;
        }

        childNode->setName(gltfNode.children[i]);

        childNode->setParentNode(node);

        //

        node->addChildNode(childNode);

        //

        if (!gltfProcessNode(childNode, visitor, *(gltfNode.childrenPointer[i]), sceneManager, sceneFactory))
        {
        	return VK_FALSE;
        }
    }

	// Process skeletons.
    for (uint32_t i = 0; i < gltfNode.skeletonsPointer.size(); i++)
    {
        auto childNode = sceneFactory->createNode(sceneManager);

        if (!childNode.get())
        {
            return VK_FALSE;
        }

        childNode->setName(gltfNode.skeletons[i]);

        childNode->setParentNode(node);

        //

        node->addChildNode(childNode);

        //

        if (!gltfProcessNode(childNode, visitor, *(gltfNode.skeletonsPointer[i]), sceneManager, sceneFactory))
        {
        	return VK_FALSE;
        }
    }

	return VK_TRUE;
}

static VkBool32 gltfProcessObject(IObjectSP& object, const GltfVisitor& visitor, const GltfScene& gltfScene, const ISceneManagerSP& sceneManager, const ISceneFactorySP& sceneFactory)
{
	// Process root node.

    for (uint32_t i = 0; i < gltfScene.nodes.size(); i++)
    {
    	auto* gltfNode = gltfScene.nodes[i];

    	if (gltfNode->name == object->getName())
    	{
            auto node = sceneFactory->createNode(sceneManager);

            if (!node.get())
            {
                return VK_FALSE;
            }

            node->setName(gltfNode->name);

            //

            if (!gltfProcessNode(node, visitor, *gltfNode, sceneManager, sceneFactory))
            {
            	return VK_FALSE;
            }

            //

            object->setRootNode(node);
    	}
    }

	return VK_TRUE;
}

ISceneSP VKTS_APIENTRY gltfLoad(const char* filename, const ISceneManagerSP& sceneManager, const ISceneFactorySP& sceneFactory, const VkBool32 freeHostMemory)
{
    if (!filename || !sceneManager.get() || !sceneFactory.get())
    {
        return ISceneSP();
    }

	auto textFile = fileLoadText(filename);

	if (!textFile.get())
	{
		return ISceneSP();
	}

	auto json = jsonDecode(textFile->getString());

	if (!json.get())
	{
		logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Parsing JSON failed");

		return ISceneSP();
	}

    char directory[VKTS_MAX_BUFFER_CHARS] = "";

    fileGetDirectory(directory, filename);

	GltfVisitor visitor(directory);

	json->visit(visitor);

	if (visitor.getState() != GltfState_End)
	{
		logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "Processing glTF failed");

		return ISceneSP();
	}

	logPrint(VKTS_LOG_INFO, __FILE__, __LINE__, "Processing glTF succeeded");

	//
	// Scene.
	//

	const GltfScene* gltfScene = visitor.getDefaultScene();

	if (!gltfScene && visitor.getAllGltfScenes().size() > 0)
	{
		gltfScene = &(visitor.getAllGltfScenes().valueAt(0));
	}

	if (!gltfScene)
	{
		logPrint(VKTS_LOG_ERROR, __FILE__, __LINE__, "No glTF scene found");

		return ISceneSP();
	}

    auto scene = sceneFactory->createScene(sceneManager);

    if (!scene.get())
    {
        return ISceneSP();
    }

    scene->setName(gltfScene->name);

    //
    // Objects: Create out of every root node an object, that it can be moved around independently.
    //

    for (uint32_t i = 0; i < gltfScene->nodes.size(); i++)
    {
    	auto* gltfNode = gltfScene->nodes[i];

    	VkBool32 isRoot = VK_TRUE;

        for (uint32_t k = 0; k < gltfScene->nodes.size(); k++)
        {
        	if (k == i)
        	{
        		continue;
        	}

        	auto* testGltfNode = gltfScene->nodes[k];

            for (uint32_t m = 0; m < testGltfNode->childrenPointer.size(); m++)
            {
            	if (gltfNode == testGltfNode->childrenPointer[m])
            	{
            		isRoot = VK_FALSE;

            		break;
            	}
            }

            if (isRoot)
            {
                for (uint32_t m = 0; m < testGltfNode->skeletonsPointer.size(); m++)
                {
                	if (gltfNode == testGltfNode->skeletonsPointer[m])
                	{
                		isRoot = VK_FALSE;

                		break;
                	}
                }
            }

            if (!isRoot)
            {
            	break;
            }
        }

        if (!isRoot)
        {
        	continue;
        }

    	//

        auto object = sceneFactory->createObject(sceneManager);

        if (!object.get())
        {
        	return ISceneSP();
        }

        object->setName(gltfNode->name);

        sceneManager->addObject(object);

        //

        if (!gltfProcessObject(object, visitor, *gltfScene, sceneManager, sceneFactory))
        {
        	return ISceneSP();
        }

        //

        auto currentObject = sceneManager->useObject(gltfNode->name);

        if (!currentObject.get())
        {
            return ISceneSP();
        }

        scene->addObject(currentObject);
    }

    // TODO: Process animations.

    return scene;
}

}
