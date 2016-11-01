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

#ifndef VKTS_COPYCONSTRAINT_HPP_
#define VKTS_COPYCONSTRAINT_HPP_

#include <vkts/vkts.hpp>

namespace vkts
{

enum CopyConstraintType {COPY_LOCATION, COPY_ROTATION, COPY_SCALE};

class CopyConstraint : public IConstraint
{

private:

	enum CopyConstraintType type;

	INodeSP target;
	glm::bvec3 use;
	glm::bvec3 invert;
	VkBool32 offset;
	float influence;

public:

	CopyConstraint() = delete;
	CopyConstraint(const enum CopyConstraintType type);
	CopyConstraint(const CopyConstraint& other);
	CopyConstraint(CopyConstraint&& other) = delete;
    virtual ~CopyConstraint();

    CopyConstraint& operator =(const CopyConstraint& other) = delete;
    CopyConstraint& operator =(CopyConstraint && other) = delete;

    enum CopyConstraintType getType() const;

	const INodeSP& getTarget() const;
	void setTarget(const INodeSP& target);

	const glm::bvec3& getUse() const;
	void setUse(const glm::bvec3& use);

	const glm::bvec3& getInvert() const;
	void setInvert(const glm::bvec3& invert);

	VkBool32 getOffset() const;
	void setOffset(const VkBool32 offset);

	float getInfluence() const;
	void setInfluence(const float influence);

    //
    // IConstraint
    //

    virtual VkBool32 applyConstraint(INode& node) override;

    //
    // ICloneable
    //

    virtual IConstraintSP clone() const override;

};

} /* namespace vkts */

#endif /* VKTS_COPYCONSTRAINT_HPP_ */
