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

#include "CopyConstraint.hpp"

namespace vkts
{

CopyConstraint::CopyConstraint(const enum CopyConstraintType type) :
	IConstraint(), type(type), target(nullptr), use(VK_FALSE, VK_FALSE, VK_FALSE), invert(VK_FALSE, VK_FALSE, VK_FALSE), offset(VK_FALSE), influence(0.0f)
{
}

CopyConstraint::CopyConstraint(const CopyConstraint& other) :
	IConstraint(), type(other.type), target(other.target), use(other.use), invert(other.invert), offset(other.offset), influence(other.influence)
{
}

CopyConstraint::~CopyConstraint()
{
}

enum CopyConstraintType CopyConstraint::getType() const
{
	return type;
}

const INodeSP& CopyConstraint::getTarget() const
{
	return target;
}

void CopyConstraint::setTarget(const INodeSP& target)
{
	this->target = target;
}

const glm::bvec3& CopyConstraint::getUse() const
{
	return use;
}

void CopyConstraint::setUse(const glm::bvec3& use)
{
	this->use = use;
}

VkBool32 CopyConstraint::getOffset() const
{
	return offset;
}

void CopyConstraint::setOffset(const VkBool32 offset)
{
	this->offset = offset;
}

const glm::bvec3& CopyConstraint::getInvert() const
{
	return invert;
}

void CopyConstraint::setInvert(const glm::bvec3& invert)
{
	this->invert = invert;
}

float CopyConstraint::getInfluence() const
{
	return influence;
}

void CopyConstraint::setInfluence(const float influence)
{
	this->influence = influence;
}

//
// ICopyLocationConstraint
//

VkBool32 CopyConstraint::applyConstraint(INode& node)
{
	if (!target.get())
	{
		return VK_FALSE;
	}

	if (influence == 0.0f)
	{
		return VK_TRUE;
	}

	//

	glm::vec3 transform;
	glm::vec3 targetTransform;

	switch (type)
	{
		case COPY_LOCATION:
			transform = node.getFinalTranslate();
			targetTransform  = decomposeTranslate(target->getTransformMatrix());
			break;
		case COPY_ROTATION:
			transform = node.getFinalRotate();
			targetTransform  = decomposeRotateRzRyRx(target->getTransformMatrix());
			break;
		case COPY_SCALE:
			transform = node.getFinalScale();
			targetTransform  = decomposeScale(target->getTransformMatrix());
			break;
	}

	//

	for (int32_t i = 0; i < 3; i++)
	{
		if (getUse()[i])
		{
			float value = targetTransform[i] * getInfluence();

			if (getInvert()[i])
			{
				value = -value;
			}

			if (getOffset())
			{
				transform[i] += value;
			}
			else
			{
				transform[i] = value;
			}
		}
	}

	//

	switch (type)
	{
		case COPY_LOCATION:
			node.setFinalTranslate(transform);
			break;
		case COPY_ROTATION:
			node.setFinalRotate(transform);
			break;
		case COPY_SCALE:
			node.setFinalScale(transform);
			break;
	}

	return VK_TRUE;
}

//
// ICloneable
//

IConstraintSP CopyConstraint::clone() const
{
    return IConstraintSP(new CopyConstraint(*this));
}

} /* namespace vkts */
