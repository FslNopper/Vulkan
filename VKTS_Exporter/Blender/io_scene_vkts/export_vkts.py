# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

"""
This script exports a scene into the VulKan ToolS (vkts) format.
"""

import bpy
import os
import math
import mathutils
import bmesh

#
# Cycles materials.
#

VKTS_BINDING_UNIFORM_SAMPLER_BSDF_FIRST = 4

fragmentGLSL = """#version 450 core

layout (location = 0) in vec4 v_f_position;
layout (location = 1) in vec3 v_f_normal;
#nextAttribute#
#nextTexture#
layout (location = 4) out vec4 ob_position;                // Position as NDC. Last element used but could be freed.
layout (location = 3) out vec4 ob_glossyNormalRoughness;   // Glossy normal and roughness.
layout (location = 2) out vec4 ob_glossyColor;             // Glossy color and alpha.
layout (location = 1) out vec4 ob_diffuseNormalRoughness;  // Diffuse normal and roughness.
layout (location = 0) out vec4 ob_diffuseColor;            // Diffuse color and alpha.

mat4 translate(vec3 t)
{
    return mat4(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, t.x, t.y, t.z, 1.0);
}

mat4 rotateRzRyRx(vec3 rotate)
{
    if (rotate.x == 0.0 && rotate.y == 0.0 && rotate.z == 0.0)
    {
        return mat4(1.0);
    }

    float rz = radians(rotate.z);
    float ry = radians(rotate.y);
    float rx = radians(rotate.x);
    float sx = sin(rx);
    float cx = cos(rx);
    float sy = sin(ry);
    float cy = cos(ry);
    float sz = sin(rz);
    float cz = cos(rz);

    return mat4(cy * cz, cy * sz, -sy, 0.0, -cx * sz + cz * sx * sy, cx * cz + sx * sy * sz, cy * sx, 0.0, sz * sx + cx * cz * sy, -cz * sx + cx * sy * sz, cx * cy, 0.0, 0.0, 0.0, 0.0, 1.0);
}

mat4 scale(vec3 s)
{
    return mat4(s.x, 0.0, 0.0, 0.0, 0.0, s.y, 0.0, 0.0, 0.0, 0.0, s.z, 0.0, 0.0, 0.0, 0.0, 1.0);
}

void main()
{
    vec4 position = v_f_position / v_f_position.w; 
    
    ob_position = position * 0.5 + 0.5;
    
    vec3 normal = normalize(v_f_normal);
    #nextTangents#
    #previousMain#
}"""

nextTangents = """vec3 bitangent = normalize(v_f_bitangent);
    vec3 tangent = normalize(v_f_tangent);
    
    mat3 objectToWorldMatrix = mat3(tangent, bitangent, normal);"""

normalMapAttribute = """layout (location = 2) in vec3 v_f_bitangent;
layout (location = 3) in vec3 v_f_tangent;
#nextAttribute#"""

texCoordAttribute = """layout (location = 4) in vec2 v_f_texCoord;
#nextAttribute#"""

texImageFunction = """layout (binding = %d) uniform sampler2D u_texture%d;
#nextTexture#"""

normalMapMain = """#previousMain#
    
    // Normal map start

    // In
    float %s = %s;
    vec3 %s = objectToWorldMatrix * normalize(%s.xyz * 2.0 - 1.0);
    
    // Out
    
    vec3 %s = mix(normal, %s, %s);
    
    // Normal map end"""

texImageMain = """#previousMain#
    
    // Image texture start

    // In
    vec3 %s = %s;
    
    // Out
    vec4 %s = texture(u_texture%d, %s.st).rgba;
    float %s = texture(u_texture%d, %s.st).a;
    
    // Image texture end"""

texCheckerMain = """#previousMain#
    
    // Checker texture start

    // In
    vec3 %s = %s;
    vec4 %s = %s;
    vec4 %s = %s;
    float %s = %s;

    bool %s = mod(floor(%s.s * %s), 2.0) == 1.0;
    bool %s = mod(floor(%s.t * %s), 2.0) == 1.0;
    
    // Out
    vec4 %s = %s;
    float %s = 0.0;
        
    if ((%s && !%s) || (!%s && %s))
    {
        %s = %s;
        %s = 1.0;
    }
    
    // Checker texture end"""

multiplyMain = """#previousMain#
    
    // Multiply start

    // In
    float %s = %s;
    vec4 %s = %s;
    vec4 %s = %s;
    
    // Out
    vec4 %s = %s%s * (1.0 - %s) + %s * %s * %s%s;
    
    // Multiply end"""

addMain = """#previousMain#
    
    // Add start

    // In
    float %s = %s;
    vec4 %s = %s;
    vec4 %s = %s;
    
    // Out
    vec4 %s = %s%s + %s * %s%s;
    
    // Add end"""

mixMain = """#previousMain#
    
    // Mix start

    // In
    float %s = %s;
    vec4 %s = %s;
    vec4 %s = %s;
    
    // Out
    vec4 %s = %smix(%s, %s, %s)%s;
    
    // Mix end"""

invertMain = """#previousMain#
    
    // Invert start

    // In
    float %s = %s;
    vec4 %s = %s;
    
    // Out
    vec4 %s = mix(%s, vec4(1.0 - %s.r, 1.0 - %s.g, 1.0 - %s.b, 1.0 - %s.a), %s);
    
    // Invert end"""

rgbToBwMain = """#previousMain#
    
    // RGB to BW start

    // In
    vec4 %s = %s;
    
    // Out
    float %s = %s.r * 0.2126 + %s.g * 0.7152 + %s.g * 0.0722;
    
    // end"""

mappingMain = """#previousMain#
    
    // Mapping start

    // In
    vec4 %s = vec4(%s, 0.0);

    %s = %s;
    
    // Out
    vec3 %s = %s%s%s.xyz;
    
    // Mapping end"""

uvMapMain = """#previousMain#
    
    // UV map start

    // Out
    vec3 %s = vec3(v_f_texCoord, 0.0);
    
    // UV map end"""

diffuseMain = """#previousMain#
    
    // Diffuse BSDF start

    // In
    vec4 %s = %s;
    float %s = %s;
    vec3 %s = %s;
    
    // Out
    vec4 %s = vec4(%s, %s);
    vec4 %s = %s;
    
    // Diffuse BSDF end"""

glossyMain = """#previousMain#
    
    // Glossy BSDF start

    // In
    vec4 %s = %s;
    float %s = %s;
    vec3 %s = %s;
    
    // Out
    vec4 %s = vec4(%s, %s);
    vec4 %s = %s;
    
    // Glossy BSDF end"""

addShaderMain = """#previousMain#
    
    // Add shader start

    // Out
    vec4 %s = %s;
    vec4 %s = %s;

    vec4 %s = %s;
    vec4 %s = %s;
    
    // Add shader end"""

discard = """if (%s %s)
    {
        discard;
    }
"""

mixShaderMain = """#previousMain#
    
    // Mix shader start
    
    // In
    float %s = %s;
    
    #discard#
    // Out
    vec4 %s = %s * %s;
    vec4 %s = %s * %s;

    vec4 %s = %s * %s;
    vec4 %s = %s * %s;

    // Mix shader end"""

materialMain = """#previousMain#
    
    // Material start

    // Out
    ob_glossyNormalRoughness = vec4(%s.xyz * 0.5 + 0.5, %s.w);
    ob_glossyColor = %s;

    ob_diffuseNormalRoughness = vec4(%s.xyz * 0.5 + 0.5, %s.w);
    ob_diffuseColor = %s;
    
    // Material end"""
        
#
#
#

def friendlyName(name):

    return name.replace(" ", "_")

def friendlyNodeName(name):

    return friendlyName(name).replace(".", "_") 

def friendlyImageName(name):

    return friendlyName(os.path.splitext(bpy.path.basename(name))[0])

def friendlyTransformName(name):

    if len(name) >= len("location") and name[-len("location"):] == "location" :
        return "TRANSLATE"
    if len(name) >= len("rotation_euler") and name[-len("rotation_euler"):] == "rotation_euler" :
        return "ROTATE"
    if len(name) >= len("rotation_quaternion") and name[-len("rotation_quaternion"):] == "rotation_quaternion" :
        return "QUATERNION_ROTATE"
    if len(name) >= len("scale") and name[-len("scale"):] == "scale" :
        return "SCALE"

    return "UNKNOWN"

def friendlyElementName(index, name, isJoint):
    
    newIndex = index

    if len(name) >= len("location") and name[-len("location"):] == "location" :
        if not isJoint:
            if index == 1:
                newIndex = 2
            if index == 2:
                newIndex = 1
    if len(name) >= len("rotation_euler") and name[-len("rotation_euler"):] == "rotation_euler" :
        if not isJoint:
            if index == 1:
                newIndex = 2
            if index == 2:
                newIndex = 1
    if len(name) >= len("rotation_quaternion") and name[-len("rotation_quaternion"):] == "rotation_quaternion" :
        if not isJoint:
            if index == 0:
                newIndex = 3
            if index == 1:
                newIndex = 0
            if index == 2:
                newIndex = 2
            if index == 3:
                newIndex = 1
        else:
            if index == 0:
                newIndex = 3
            if index == 1:
                newIndex = 0
            if index == 2:
                newIndex = 1
            if index == 3:
                newIndex = 2
    if len(name) >= len("scale") and name[-len("scale"):] == "scale" :
        if not isJoint:
            if index == 1:
                newIndex = 2
            if index == 2:
                newIndex = 1

    if newIndex == 0:
        return "X"
    if newIndex == 1:
        return "Y"
    if newIndex == 2:
        return "Z"
    if newIndex == 3:
        return "W"

    return "UNKNOWN"


def extractNode(name):
    if name is None:
        return None

    index = name.find("[\"")
    if (index == -1):
        return None

    subName = name[(index + 2):]

    index = subName.find("\"")
    if (index == -1):
        return None

    return subName[:(index)]


def convertLocation(location):

    location = tuple(location)

    return (location[0], location[2], -location[1])

def convertRotation(rotation):

    rotation = tuple(rotation)

    return (math.degrees(rotation[0]), math.degrees(rotation[2]), -math.degrees(rotation[1]))

def convertScale(scale):

    scale = tuple(scale)

    return (scale[0], scale[2], scale[1])

def convertLocationNoAdjust(location):

    location = tuple(location)

    return (location[0], location[1], location[2])

def convertRotationNoAdjust(rotation):

    rotation = tuple(rotation)

    return (math.degrees(rotation[0]), math.degrees(rotation[1]), math.degrees(rotation[2]))

def convertScaleNoAdjust(scale):

    scale = tuple(scale)

    return (scale[0], scale[1], scale[2])


def saveTextures(context, filepath, imagesLibraryName, materials):

    imagesLibraryFilepath = os.path.dirname(filepath) + "/" + imagesLibraryName

    file_image = open(imagesLibraryFilepath, "w", encoding="utf8", newline="\n")
    fw_image = file_image.write
    fw_image("#\n")
    fw_image("# VulKan ToolS images.\n")
    fw_image("#\n")
    fw_image("\n")

    #

    file = open(filepath, "w", encoding="utf8", newline="\n")
    fw = file.write
    fw("#\n")
    fw("# VulKan ToolS textures.\n")
    fw("#\n")
    fw("\n")
    fw("image_library %s\n" % friendlyName(imagesLibraryName))
    fw("\n")

    textures = {}
    envTextures = {}
    cyclesTextures = {}
    cyclesEnvTextures = {}
    images = {}
    mipMappedImages = []
    environmentImages = []
    preFilteredImages = []
    
    #

    if context.scene.world.use_nodes:    
        for currentNode in context.scene.world.node_tree.nodes:
            if isinstance(currentNode, bpy.types.ShaderNodeTexEnvironment):
                storeTexture = False
                if currentNode.image is not None:
                    images.setdefault(friendlyImageName(currentNode.image.filepath), currentNode.image)
                    storeTexture = True

                if storeTexture:
                    cyclesEnvTextures.setdefault(friendlyImageName(currentNode.name), currentNode)
    else:
        if context.scene.world.light_settings.environment_color == 'SKY_TEXTURE':
            for materialName in materials:

                material = materials[materialName]
        
                for currentTextureSlot in material.texture_slots:
                    if currentTextureSlot and currentTextureSlot.texture and currentTextureSlot.texture.type == 'ENVIRONMENT_MAP':
                        storeTexture = False
                        if currentTextureSlot.texture.image is not None:
                            images.setdefault(friendlyImageName(currentTextureSlot.texture.image.filepath), currentTextureSlot.texture.image)
                            storeTexture = True

                        if storeTexture:
                            envTextures.setdefault(friendlyImageName(currentTextureSlot.texture.name), currentTextureSlot.texture)

    #

    for materialName in materials:

        material = materials[materialName]
        
        for currentTextureSlot in material.texture_slots:
            if currentTextureSlot and currentTextureSlot.texture and currentTextureSlot.texture.type == 'IMAGE':
                image = currentTextureSlot.texture.image
                if image and image.has_data:
                    storeTexture = False
                    if currentTextureSlot.use_map_emit:
                        images.setdefault(friendlyImageName(image.filepath), image)
                        storeTexture = True
                    if currentTextureSlot.use_map_alpha:
                        images.setdefault(friendlyImageName(image.filepath), image)
                        storeTexture = True
                    if currentTextureSlot.use_map_displacement:
                        images.setdefault(friendlyImageName(image.filepath), image)
                        storeTexture = True
                    if currentTextureSlot.use_map_normal:
                        images.setdefault(friendlyImageName(image.filepath), image)
                        storeTexture = True
                    if currentTextureSlot.use_map_ambient:
                        images.setdefault(friendlyImageName(image.filepath), image)
                        storeTexture = True
                    if currentTextureSlot.use_map_color_diffuse:
                        images.setdefault(friendlyImageName(image.filepath), image)
                        storeTexture = True
                    if currentTextureSlot.use_map_color_spec:
                        images.setdefault(friendlyImageName(image.filepath), image)
                        storeTexture = True
                    if currentTextureSlot.use_map_hardness:
                        images.setdefault(friendlyImageName(image.filepath), image)
                        storeTexture = True
                        
                    if storeTexture:
                        textures.setdefault(friendlyImageName(currentTextureSlot.texture.name), currentTextureSlot.texture)
                                           
        # Cycles
        if material.use_nodes == True:
            for currentNode in material.node_tree.nodes:
                if isinstance(currentNode, bpy.types.ShaderNodeTexImage):
                    storeTexture = False
                    if currentNode.image is not None:
                        images.setdefault(friendlyImageName(currentNode.image.filepath), currentNode.image)
                        storeTexture = True

                    if storeTexture:
                        cyclesTextures.setdefault(friendlyName(material.name) + "_" + friendlyNodeName(currentNode.name), currentNode)

    for nameOfTexture in textures:

        texture = textures[nameOfTexture]
        
        nameOfImage = friendlyImageName(texture.image.filepath)
        
        if texture.use_mipmap:
            if not nameOfImage in mipMappedImages:
                mipMappedImages.append(nameOfImage)

        fw("#\n")
        fw("# Texture.\n")
        fw("#\n")
        fw("\n")
        fw("name %s\n" % (nameOfTexture + "_texture"))
        fw("\n")
        if nameOfImage in mipMappedImages:
            fw("mipmap true\n")
        fw("image %s\n" % (nameOfImage + "_image"))
        fw("\n")

    for nameOfTexture in cyclesTextures:

        node = cyclesTextures[nameOfTexture]
        
        nameOfImage = friendlyImageName(node.image.filepath)
        
        if node.interpolation == 'Cubic':
            if not nameOfImage in mipMappedImages:
                mipMappedImages.append(nameOfImage)

        fw("#\n")
        fw("# Texture.\n")
        fw("#\n")
        fw("\n")
        fw("name %s\n" % (nameOfTexture + "_texture"))
        fw("\n")
        if nameOfImage in mipMappedImages:
            fw("mipmap true\n")
        fw("image %s\n" % (nameOfImage + "_image"))
        fw("\n")

    allEnvironmentTextures = []

    for nameOfTexture in envTextures:

        texture = envTextures[nameOfTexture]
        
        nameOfImage = friendlyImageName(texture.image.filepath)

        if not nameOfImage in environmentImages:
            environmentImages.append(nameOfImage)

        fw("#\n")
        fw("# Environment Texture.\n")
        fw("#\n")
        fw("\n")
        fw("name %s\n" % (nameOfTexture + "_texture"))
        fw("\n")
        fw("environment true\n")
        fw("image %s\n" % (nameOfImage + "_image"))
        fw("\n")

        allEnvironmentTextures.append(nameOfTexture + "_texture")

    for nameOfTexture in cyclesEnvTextures:

        node = cyclesEnvTextures[nameOfTexture]
        
        nameOfImage = friendlyImageName(node.image.filepath)
        
        if not nameOfImage in environmentImages:
            environmentImages.append(nameOfImage)
        if not nameOfImage in preFilteredImages:
            preFilteredImages.append(nameOfImage)

        fw("#\n")
        fw("# Environment Texture.\n")
        fw("#\n")
        fw("\n")
        fw("name %s\n" % (nameOfTexture + "_texture"))
        fw("\n")
        fw("environment true\n")
        fw("pre_filtered true\n")
        fw("image %s\n" % (nameOfImage + "_image"))
        fw("\n")

        allEnvironmentTextures.append(nameOfTexture + "_texture")

    for nameOfImage in images:

        image = images[nameOfImage]

        # Always save as TARGA ...
        context.scene.render.image_settings.file_format = 'TARGA'
        context.scene.render.image_settings.color_depth = '8'        
        extension = ".tga"
        
        # ... and if image uses floats, as HDR:
        if image.is_float:
            context.scene.render.image_settings.file_format = 'HDR'
            context.scene.render.image_settings.color_depth = '32'
            extension = ".hdr"
        
        image.save_render((os.path.dirname(filepath) + "/" + nameOfImage + extension), context.scene)

        fw_image("#\n")
        fw_image("# Image.\n")
        fw_image("#\n")
        fw_image("\n")
        fw_image("name %s\n" % (nameOfImage + "_image"))
        fw_image("\n")
        if nameOfImage in mipMappedImages:
            fw_image("mipmap true\n")
        else:
            if nameOfImage in environmentImages:
                fw_image("environment true\n")
            if nameOfImage in preFilteredImages:
                fw_image("pre_filtered true\n")
        fw_image("image_data %s\n" % (nameOfImage + extension))
        fw_image("\n")
    
    file.close()
    
    file_image.close()

    return allEnvironmentTextures

def getFloat(value):
    return "%.3f"%(value)

def getVec2(value):
    return "vec2(%.3f, %.3f)"%(value[0], value[1])

def getVec3(value):
    return "vec3(%.3f, %.3f, %.3f)"%(value[0], value[1], value[2])

def getVec4(value):
    return "vec4(%.3f, %.3f, %.3f, %.3f)"%(value[0], value[1], value[2], value[3])


def enqueueNode(openNodes, currentNode):
    # Every node apears only once in the list.
    if currentNode in openNodes:
        return

    insertIndexList = []

    # Create a list of insert indices, where the current node links to.
    for currentSocket in currentNode.outputs:
        for currentLink in currentSocket.links: 
            insertIndex = 0
            for checkNode in openNodes:
                if currentLink.to_node == checkNode:
                    insertIndexList.append(insertIndex + 1) 
                insertIndex += 1

    if len(insertIndexList) == 0 or len(openNodes) == 0:
        openNodes.append(currentNode)
    else:
        # Insert at highest index, that all dependent nodes are before.    
        openNodes.insert(max(insertIndexList), currentNode)


def createOpenNodeList(openNodes, rootNode):

    todoList = [rootNode]

    # Gather all linked nodes and enque them on their dependency.
    while len(todoList) > 0:
        currentNode = todoList.pop(0)

        enqueueNode(openNodes, currentNode) 

        for currentSocket in currentNode.inputs:
            if len(currentSocket.links) > 0:
                todoList.append(currentSocket.links[0].from_node)


def replaceParameters(currentNode, openNodes, processedNodes, currentMain):
    for currentSocket in currentNode.inputs:
        currentParameter = currentSocket.name + "_Dummy"
            
        if len(currentSocket.links) == 0:
            currentValue = ""

            # Convert value into GLSL expression.
            if isinstance(currentSocket, bpy.types.NodeSocketColor):
                currentValue = getVec4(currentSocket.default_value)
            elif isinstance(currentSocket, bpy.types.NodeSocketFloatFactor):
                currentValue = getFloat(currentSocket.default_value)
            elif isinstance(currentSocket, bpy.types.NodeSocketFloat):
                currentValue = getFloat(currentSocket.default_value)
            elif isinstance(currentSocket, bpy.types.NodeSocketVector):
                if currentSocket.name == "Normal":
                    currentValue = "normal"
                else:
                    currentValue = getVec3(currentSocket.default_value)
            
            # Replace parameter with value.
            currentMain = currentMain.replace(currentParameter, currentValue)
        else:
            # Replace parameter with variable.
            currentMain = currentMain.replace(currentParameter, friendlyNodeName(currentSocket.links[0].from_node.name) + "_" + friendlyNodeName(currentSocket.links[0].from_socket.name))
        
    return currentMain

def replaceShaderParameters(currentNode, openNodes, processedNodes, currentMain):
    index = 0
    for currentSocket in currentNode.inputs:

        if len(currentSocket.links) == 0:
            # Convert value into GLSL expression.
            if isinstance(currentSocket, bpy.types.NodeSocketFloatFactor):
                currentParameter = currentSocket.name + "_Dummy"
                currentValue = getFloat(currentSocket.default_value)
                currentMain = currentMain.replace(currentParameter, currentValue)
                
        else:
            if isinstance(currentSocket, bpy.types.NodeSocketFloatFactor):
                currentParameter = currentSocket.name + "_Dummy"
                currentMain = currentMain.replace(currentParameter, friendlyNodeName(currentSocket.links[0].from_node.name) + "_" + friendlyNodeName(currentSocket.links[0].from_socket.name))

            # Replace parameter with variable.
            linkedNode = currentSocket.links[0].from_node
            if isinstance(linkedNode, bpy.types.ShaderNodeAddShader): 
                currentMain = currentMain.replace("GlossyNormalRoughness_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_GlossyNormalRoughness")
                currentMain = currentMain.replace("GlossyColor_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_GlossyColor")
                currentMain = currentMain.replace("DiffuseNormalRoughness_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_DiffuseNormalRoughness")
                currentMain = currentMain.replace("DiffuseColor_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_DiffuseColor")
            if isinstance(linkedNode, bpy.types.ShaderNodeBsdfGlossy): 
                currentMain = currentMain.replace("GlossyNormalRoughness_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_GlossyNormalRoughness")
                currentMain = currentMain.replace("GlossyColor_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_GlossyColor")
            if isinstance(linkedNode, bpy.types.ShaderNodeBsdfDiffuse):
                currentMain = currentMain.replace("DiffuseNormalRoughness_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_DiffuseNormalRoughness")
                currentMain = currentMain.replace("DiffuseColor_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_DiffuseColor")
                
        index += 1

    parameterNames = ["GlossyNormalRoughness", "GlossyColor", "DiffuseNormalRoughness", "DiffuseColor"]
        
    for currentParameter in parameterNames:
        currentMain = currentMain.replace(currentParameter + "_Dummy", "vec4(0.0, 0.0, 0.0, 0.0)")
        
    return currentMain

def replaceMaterialParameters(currentNode, openNodes, processedNodes, currentMain):
    currentSocket = currentNode.inputs["Surface"]

    if len(currentSocket.links) == 0:
        parameterNames = ["GlossyNormalRoughness", "GlossyColor", "DiffuseNormalRoughness", "DiffuseColor"]
    
        for currentParameter in parameterNames:
            currentMain = currentMain.replace(currentParameter + "_Dummy", "vec4(0.0, 0.0, 0.0, 0.0)")
    else:
        # Replace parameter with variable.
        linkedNode = currentSocket.links[0].from_node
        if isinstance(linkedNode, bpy.types.ShaderNodeAddShader) or isinstance(linkedNode, bpy.types.ShaderNodeMixShader): 
            currentMain = currentMain.replace("GlossyNormalRoughness_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_GlossyNormalRoughness")
            currentMain = currentMain.replace("GlossyColor_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_GlossyColor")
            currentMain = currentMain.replace("DiffuseNormalRoughness_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_DiffuseNormalRoughness")
            currentMain = currentMain.replace("DiffuseColor_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_DiffuseColor")
        if isinstance(linkedNode, bpy.types.ShaderNodeBsdfGlossy): 
            currentMain = currentMain.replace("GlossyNormalRoughness_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_GlossyNormalRoughness")
            currentMain = currentMain.replace("GlossyColor_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_GlossyColor")
            currentMain = currentMain.replace("DiffuseNormalRoughness_Dummy", "vec4(0.0, 0.0, 0.0, 0.0)")
            currentMain = currentMain.replace("DiffuseColor_Dummy", "vec4(0.0, 0.0, 0.0, 0.0)")
        if isinstance(linkedNode, bpy.types.ShaderNodeBsdfDiffuse):
            currentMain = currentMain.replace("GlossyNormalRoughness_Dummy", "vec4(0.0, 0.0, 0.0, 0.0)")
            currentMain = currentMain.replace("GlossyColor_Dummy", "vec4(0.0, 0.0, 0.0, 0.0)")
            currentMain = currentMain.replace("DiffuseNormalRoughness_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_DiffuseNormalRoughness")
            currentMain = currentMain.replace("DiffuseColor_Dummy", friendlyNodeName(currentSocket.links[0].from_node.name) + "_DiffuseColor")
        
    return currentMain

def saveMaterials(context, filepath, texturesLibraryName, imagesLibraryName):
    
    file = open(filepath, "w", encoding="utf8", newline="\n")
    fw = file.write
    fw("#\n")
    fw("# VulKan ToolS materials.\n")
    fw("#\n")
    fw("\n")

    if texturesLibraryName is not None:
        fw("texture_library %s\n" % friendlyName(texturesLibraryName))
        fw("\n")

    meshes = {}

    # Gather all meshes.
    for currentObject in context.scene.objects:

        if currentObject.type == 'MESH':

            meshes.setdefault(currentObject.data)

    materials = {}

    # Gather all materials.
    for mesh in meshes:

        for face in mesh.polygons:

            # Create default material if none is set.        
            if len(mesh.materials) == 0:
            
                if "DefaultMaterial" not in materials:
                    materials.setdefault("DefaultMaterial", bpy.data.materials.new("DefaultMaterial"))
                    
            else:
            
                if mesh.materials[face.material_index].name not in materials:
            
                    materials.setdefault(mesh.materials[face.material_index].name, mesh.materials[face.material_index])

    # Save textures.

    texturesLibraryFilepath = os.path.dirname(filepath) + "/" + texturesLibraryName
    allEnvironmentTextures = saveTextures(context, texturesLibraryFilepath, imagesLibraryName, materials)

    # Write materials.
    for materialName in materials:

        fw("#\n")
        fw("# Material.\n")
        fw("#\n")
        fw("\n")
        
        material = materials[materialName]
        
        if material.use_nodes == True:
            
            normalMapUsed = False
            texCoordUsed = False
            
            vertexAttributes = 0x00000001 | 0x00000002 

            #
            # Cycles material.
            #
        
            currentFragmentGLSL = fragmentGLSL

            #            
        
            fw("shading BSDF\n")
            fw("\n")        
            fw("name %s\n" % friendlyName(materialName))
            fw("\n")
            fw("fragment_shader %s\n" % friendlyName(materialName + ".frag.spv"))
            fw("\n")
            
            #
            
            nodes = material.node_tree.nodes
            
            for currentNode in nodes:
                if isinstance(currentNode, bpy.types.ShaderNodeOutputMaterial):
                    materialOutput = currentNode
                    break  
            
            if materialOutput is None:
                continue

            #
            
            addMixCounter = 0
            alphaCounter = 0
            colorCounter = 0
            diffuseCounter = 0
            facCounter = 0
            glossyCounter = 0
            normalCounter = 0
            roughnessCounter = 0
            scaleCounter = 0
            strengthCounter = 0
            tempCounter = 0
            vectorCounter = 0
            
            openNodes = []
            processedNodes = []
            
            nodes = []

            createOpenNodeList(openNodes, materialOutput)
            
            while len(openNodes) > 0:
            
                currentNode = openNodes.pop(0)

                if isinstance(currentNode, bpy.types.ShaderNodeNormalMap):
                    # Normal map.
                    
                    # Inputs
                    
                    strengthInputName = "Strength_%d" % (strengthCounter)
                    colorInputName = "Color_%d" % (colorCounter)

                    strengthCounter += 1
                    colorCounter += 1

                    strengthInputParameterName = "Strength_Dummy"
                    colorInputParameterName = "Color_Dummy"
                    
                    # Outputs
                    
                    normalOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["Normal"].name)
                    
                    #
                    
                    currentMain = normalMapMain % (strengthInputName, strengthInputParameterName, colorInputName, colorInputParameterName, normalOutputName, colorInputName, strengthInputName) 
                    
                    #
                    
                    currentMain = replaceParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    #
                    
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)
                    
                    #
                
                    normalMapUsed = True
                    
                    vertexAttributes = vertexAttributes | 0x00000004 | 0x00000008

                elif isinstance(currentNode, bpy.types.ShaderNodeTexImage):
                    # Image texture.
                    
                    if currentNode not in nodes:
                        nodes.append(currentNode)

                    textureIndex = nodes.index(currentNode)
                        
                    # Inputs.
                    
                    vectorInputName = "Vector_%d" % (vectorCounter)

                    vectorCounter += 1
                    
                    vectorInputParameterName = "Vector_Dummy"
                    
                    # Outputs
                    
                    colorOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["Color"].name) 
                    alphaOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["Alpha"].name)
                    
                    #
                    
                    currentMain = texImageMain % (vectorInputName, vectorInputParameterName, colorOutputName, textureIndex, vectorInputName, alphaOutputName, textureIndex, vectorInputName)
                    
                    #
                    
                    currentMain = replaceParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    #
                    
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)

                elif isinstance(currentNode, bpy.types.ShaderNodeTexChecker):
                    # Checker texture.
                    
                    # Inputs.
                    
                    vectorInputName = "Vector_%d" % (vectorCounter)
                    
                    color1InputName = "Color1_%d" % (colorCounter)
                    colorCounter += 1
                    color2InputName = "Color2_%d" % (colorCounter)
                    
                    scaleInputName = "Scale_%d" % (scaleCounter)
                    
                    vectorCounter += 1
                    colorCounter += 1
                    scaleCounter += 1

                    vectorInputParameterName = "Vector_Dummy"
                    color1InputParameterName = "Color1_Dummy"
                    color2InputParameterName = "Color2_Dummy"
                    scaleInputParameterName = "Scale_Dummy"
                    
                    # Temporary

                    bool1TempName = "TempBool_%d" % (tempCounter)
                    
                    tempCounter += 1;

                    bool2TempName = "TempBool_%d" % (tempCounter)

                    tempCounter += 1;
                    
                    # Outputs
                    
                    colorOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["Color"].name) 
                    facOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["Fac"].name) 

                    #
                    
                    currentMain = texCheckerMain % (vectorInputName, vectorInputParameterName, color1InputName, color1InputParameterName, color2InputName, color2InputParameterName, scaleInputName, scaleInputParameterName, bool1TempName, vectorInputName, scaleInputName, bool2TempName, vectorInputName, scaleInputName, colorOutputName, color2InputName, facOutputName, bool1TempName, bool2TempName, bool1TempName, bool2TempName, colorOutputName, color1InputName, facOutputName)
                    
                    # 
                    
                    currentMain = replaceParameters(currentNode, openNodes, processedNodes, currentMain)
                     
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)
                
                elif isinstance(currentNode, bpy.types.ShaderNodeMixRGB):
                    # Mix color.
                    
                    # Inputs.
                    
                    facInputName = "Fac_%d" % (facCounter)
                    color1InputName = "Color1_%d" % (colorCounter)
                    colorCounter += 1
                    color2InputName = "Color2_%d" % (colorCounter)
                    
                    facCounter += 1
                    colorCounter += 1

                    facInputParameterName = "Fac_Dummy"
                    color1InputParameterName = "Color1_Dummy"
                    color2InputParameterName = "Color2_Dummy"
                    
                    # Outputs
                    
                    colorOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["Color"].name) 
                    
                    #
                                        
                    preClamp = ""
                    postClamp = ""
                    
                    if currentNode.use_clamp:
                        preClamp = "clamp("
                        postClamp = ", vec4(0.0, 0.0, 0.0, 0.0), vec4(1.0, 1.0, 1.0, 1.0))"
                    
                    #
                    
                    if currentNode.blend_type == 'MIX':    
                        currentMain = mixMain % (facInputName, facInputParameterName, color1InputName, color1InputParameterName, color2InputName, color2InputParameterName, colorOutputName, preClamp, color1InputName, color2InputName, facInputName, postClamp)
                    elif currentNode.blend_type == 'ADD':
                        currentMain = addMain % (facInputName, facInputParameterName, color1InputName, color1InputParameterName, color2InputName, color2InputParameterName, colorOutputName, preClamp, color1InputName, color2InputName, facInputName, postClamp)
                    elif currentNode.blend_type == 'MULTIPLY':
                        currentMain = multiplyMain % (facInputName, facInputParameterName, color1InputName, color1InputParameterName, color2InputName, color2InputParameterName, colorOutputName, preClamp, color1InputName, facInputName, color1InputName, color2InputName, facInputName, postClamp)
                    else:
                        currentMain = ""
                    #
                    
                    currentMain = replaceParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)
                    
                elif isinstance(currentNode, bpy.types.ShaderNodeMapping):
                    # Mapping.
                        
                    # Inputs.
                    
                    vectorInputName = "Vector_%d" % (vectorCounter)

                    vectorCounter += 1
                    
                    vectorInputParameterName = "Vector_Dummy"

                    # Outputs
                    
                    vectorOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["Vector"].name) 
                    
                    #

                    preItem = ""
                    postItem = ""
                    
                    if currentNode.use_min:
                        preItem = "min("
                        postItem = ", " + getVec3(currentNode.min) + ")"

                    if currentNode.use_max:
                        preItem = "max(" + preItem 
                        postItem = postItem + ", " + getVec3(currentNode.max) + ")"

                    #

                    finalValue = "rotateRzRyRx(" + getVec3(currentNode.rotation) + ") * scale(" + getVec3(currentNode.scale) + ")"

                    if currentNode.vector_type == 'TEXTURE' or currentNode.vector_type == 'POINT':
                        finalValue = "translate(" + getVec3(currentNode.translation) + ") * "  + finalValue

                        if currentNode.vector_type == 'TEXTURE':
                            finalValue = "inverse(" + finalValue + ")"

                    finalValue = finalValue + " * " + vectorInputName    

                    if currentNode.vector_type == 'NORMAL':
                        finalValue = "normalize(" + finalValue + ")"

                    #                    
                    
                    currentMain = mappingMain % (vectorInputName, vectorInputParameterName, vectorInputName, finalValue, vectorOutputName, preItem, vectorInputName, postItem)
                    
                    # 
                    
                    currentMain = replaceParameters(currentNode, openNodes, processedNodes, currentMain)
                     
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)
                    
                elif isinstance(currentNode, bpy.types.ShaderNodeUVMap):
                    # UV map.

                    # Outputs
                    
                    uvOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["UV"].name) 
                    
                    #
                    
                    currentMain = uvMapMain % (uvOutputName) 
                    
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)
                    
                    texCoordUsed = True
                    
                    vertexAttributes = vertexAttributes | 0x00000010 

                elif isinstance(currentNode, bpy.types.ShaderNodeInvert):
                    # Invert color.

                    # Inputs
                    
                    facInputName = "Fac_%d" % (facCounter)
                    colorInputName = "Color_%d" % (colorCounter)

                    facCounter += 1
                    colorCounter += 1

                    facInputParameterName = "Fac_Dummy"
                    colorInputParameterName = "Color_Dummy"
                    
                    # Outputs
                    
                    colorOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["Color"].name) 
                    
                    #
                    
                    currentMain = invertMain % (facInputName, facInputParameterName, colorInputName, colorInputParameterName, colorOutputName, colorInputName, colorInputName, colorInputName, colorInputName, colorInputName, facInputName) 
                    
                    #
                    
                    currentMain = replaceParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)

                elif isinstance(currentNode, bpy.types.ShaderNodeRGBToBW):
                    # RGB to BW color.

                    # Inputs
                    
                    colorInputName = "Color_%d" % (colorCounter)

                    colorCounter += 1

                    colorInputParameterName = "Color_Dummy"
                    
                    # Outputs
                    
                    valOutputName = friendlyNodeName(currentNode.name) + "_" + friendlyNodeName(currentNode.outputs["Val"].name) 
                    
                    #
                    
                    currentMain = rgbToBwMain % (colorInputName, colorInputParameterName, valOutputName, colorInputName, colorInputName, colorInputName) 
                    
                    #
                    
                    currentMain = replaceParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)

                elif isinstance(currentNode, bpy.types.ShaderNodeBsdfDiffuse) and diffuseCounter == 0:
                    # Diffuse BSDF shader.
                    
                    # Inputs.

                    colorInputName = "Color_%d" % colorCounter
                    roughnessInputName = "Roughness_%d" % roughnessCounter
                    normalInputName = "Normal_%d" % normalCounter

                    colorCounter += 1
                    roughnessCounter += 1
                    normalCounter += 1
                    
                    colorInputParameterName = "Color_Dummy"
                    roughnessInputParameterName = "Roughness_Dummy"
                    normalInputParameterName =  "Normal_Dummy"
                    
                    # Outputs.
                    
                    diffuseNormalRoughnessOutputName = friendlyNodeName(currentNode.name) + "_DiffuseNormalRoughness" 
                    diffuseColorOutputName = friendlyNodeName(currentNode.name) + "_DiffuseColor"
                    
                    #
                    
                    currentMain = diffuseMain % (colorInputName, colorInputParameterName, roughnessInputName, roughnessInputParameterName, normalInputName, normalInputParameterName, diffuseNormalRoughnessOutputName, normalInputName, roughnessInputName, diffuseColorOutputName, colorInputName)
                    
                    #
                    
                    currentMain = replaceParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)
                    
                elif isinstance(currentNode, bpy.types.ShaderNodeBsdfGlossy) and glossyCounter == 0:
                    # Glossy BSDF shader.

                    # Inputs.

                    colorInputName = "Color_%d" % colorCounter
                    roughnessInputName = "Roughness_%d" % roughnessCounter
                    normalInputName = "Normal_%d" % normalCounter

                    colorCounter += 1
                    roughnessCounter += 1
                    normalCounter += 1
                    
                    colorInputParameterName = "Color_Dummy"
                    roughnessInputParameterName = "Roughness_Dummy"
                    normalInputParameterName =  "Normal_Dummy"
                    
                    # Outputs.
                    
                    glossyNormalRoughnessOutputName = friendlyNodeName(currentNode.name) + "_GlossyNormalRoughness" 
                    glossyColorOutputName = friendlyNodeName(currentNode.name) + "_GlossyColor"
                    
                    #
                    
                    currentMain = glossyMain % (colorInputName, colorInputParameterName, roughnessInputName, roughnessInputParameterName, normalInputName, normalInputParameterName, glossyNormalRoughnessOutputName, normalInputName, roughnessInputName, glossyColorOutputName, colorInputName)
                    
                    #
                    
                    currentMain = replaceParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)
                    
                elif isinstance(currentNode, bpy.types.ShaderNodeAddShader) and addMixCounter == 0:
                    # Material Add shader.

                    # Inputs.
                    
                    # No more inputs, as only diffuse and glossy can be added.

                    glossyNormalRoughnessInputName = "GlossyNormalRoughness_%d" % addMixCounter
                    glossyColorInputName = "GlossyColor_%d" % addMixCounter

                    diffuseNormalRoughnessInputName = "DiffuseNormalRoughness_%d" % addMixCounter
                    diffuseColorInputName = "DiffuseColor_%d" % addMixCounter

                    addMixCounter += 1

                    glossyNormalRoughnessInputParameterName = "GlossyNormalRoughness_Dummy"
                    glossyColorInputParameterName = "GlossyColor_Dummy"

                    diffuseNormalRoughnessInputParameterName = "DiffuseNormalRoughness_Dummy"
                    diffuseColorInputParameterName = "DiffuseColor_Dummy"
                    
                    # Outputs.
                    
                    glossyNormalRoughnessOutputName = friendlyNodeName(currentNode.name) + "_GlossyNormalRoughness" 
                    glossyColorOutputName = friendlyNodeName(currentNode.name) + "_GlossyColor"
                    
                    diffuseNormalRoughnessOutputName = friendlyNodeName(currentNode.name) + "_DiffuseNormalRoughness" 
                    diffuseColorOutputName = friendlyNodeName(currentNode.name) + "_DiffuseColor"
                    
                    #
                    
                    currentMain = addShaderMain % (glossyNormalRoughnessOutputName, glossyNormalRoughnessInputParameterName, glossyColorOutputName, glossyColorInputParameterName, diffuseNormalRoughnessOutputName, diffuseNormalRoughnessInputParameterName, diffuseColorOutputName, diffuseColorInputParameterName)
                    
                    #
                    
                    currentMain = replaceShaderParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)

                elif isinstance(currentNode, bpy.types.ShaderNodeMixShader) and addMixCounter == 0:
                    # Material Mix shader.

                    # Inputs.
                    
                    facInputName = "Fac_%d" % (facCounter)
                    
                    glossyNormalRoughnessInputName = "GlossyNormalRoughness_%d" % addMixCounter
                    glossyColorInputName = "GlossyColor_%d" % addMixCounter

                    diffuseNormalRoughnessInputName = "DiffuseNormalRoughness_%d" % addMixCounter
                    diffuseColorInputName = "DiffuseColor_%d" % addMixCounter

                    facCounter += 1
                    addMixCounter += 1

                    facInputParameterName = "Fac_Dummy"

                    glossyNormalRoughnessInputParameterName = "GlossyNormalRoughness_Dummy"
                    glossyColorInputParameterName = "GlossyColor_Dummy"

                    diffuseNormalRoughnessInputParameterName = "DiffuseNormalRoughness_Dummy"
                    diffuseColorInputParameterName = "DiffuseColor_Dummy"
                    
                    # Outputs.
                    
                    glossyNormalRoughnessOutputName = friendlyNodeName(currentNode.name) + "_GlossyNormalRoughness" 
                    glossyColorOutputName = friendlyNodeName(currentNode.name) + "_GlossyColor"
                    
                    diffuseNormalRoughnessOutputName = friendlyNodeName(currentNode.name) + "_DiffuseNormalRoughness" 
                    diffuseColorOutputName = friendlyNodeName(currentNode.name) + "_DiffuseColor"
                    
                    #
                    
                    factorOneMinusFac = "(1.0 - %s)" % (facInputName)
                    factorFac = facInputName
                    
                    factorOne = factorOneMinusFac
                    factorTwo = factorFac
                    
                    counter = 0
                    for currentSocket in currentNode.inputs:
                        if len(currentSocket.links) != 0:
                            linkedNode = currentSocket.links[0].from_node
                            if isinstance(linkedNode, bpy.types.ShaderNodeBsdfDiffuse):
                                if (counter == 1):
                                    factorOne = factorFac
                                    factorTwo = factorOneMinusFac
                                else:
                                    factorOne = factorOneMinusFac
                                    factorTwo = factorFac
                                break
                            if isinstance(linkedNode, bpy.types.ShaderNodeBsdfGlossy):
                                if (counter == 1):
                                    factorOne = factorOneMinusFac
                                    factorTwo = factorFac
                                else:
                                    factorOne = factorFac
                                    factorTwo = factorOneMinusFac
                                break
                        counter += 1
                    
                    currentMain = mixShaderMain % (facInputName, facInputParameterName, glossyNormalRoughnessOutputName, glossyNormalRoughnessInputParameterName, factorOne, glossyColorOutputName, glossyColorInputParameterName, factorOne, diffuseNormalRoughnessOutputName, diffuseNormalRoughnessInputParameterName, factorTwo, diffuseColorOutputName, diffuseColorInputParameterName, factorTwo)

                    #
                    
                    currentMain = replaceShaderParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    # Special case, if a mask is available.
                    counter = 0
                    for currentSocket in currentNode.inputs:
                        if len(currentSocket.links) != 0:
                            linkedNode = currentSocket.links[0].from_node
                            if isinstance(linkedNode, bpy.types.ShaderNodeBsdfTransparent):
                                if (counter == 1):
                                    currentDiscard = discard % (facInputName, "< 1.0")                
                                else:
                                    currentDiscard = discard % (facInputName, "> 0.0")
                                
                                currentMain = currentMain.replace("#discard#", currentDiscard)
                                    
                                break
                        counter += 1
                    
                    currentMain = currentMain.replace("#discard#", "")
                    
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)

                elif isinstance(currentNode, bpy.types.ShaderNodeOutputMaterial):
                    # Material output.

                    # Inputs.
                    
                    # No inputs, as only one diffuse and glossy are possible.

                    glossyNormalRoughnessInputParameterName = "GlossyNormalRoughness_Dummy"
                    glossyColorInputParameterName = "GlossyColor_Dummy"

                    diffuseNormalRoughnessInputParameterName = "DiffuseNormalRoughness_Dummy"
                    diffuseColorInputParameterName = "DiffuseColor_Dummy"
                    
                    # Outputs.
                    
                    # No outputs, as written to texture.
                    
                    #
                    
                    currentMain = materialMain % (glossyNormalRoughnessInputParameterName, glossyNormalRoughnessInputParameterName, glossyColorInputParameterName, diffuseNormalRoughnessInputParameterName, diffuseNormalRoughnessInputParameterName, diffuseColorInputParameterName)
                    
                    #
                    
                    currentMain = replaceMaterialParameters(currentNode, openNodes, processedNodes, currentMain)
                    
                    #
                        
                    currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", currentMain)
                    
                if currentNode not in processedNodes:
                    processedNodes.append(currentNode)

            if normalMapUsed:
                currentFragmentGLSL = currentFragmentGLSL.replace("#nextTangents#", nextTangents)
                currentFragmentGLSL = currentFragmentGLSL.replace("#nextAttribute#", normalMapAttribute)
                
            if texCoordUsed:
                currentFragmentGLSL = currentFragmentGLSL.replace("#nextAttribute#", texCoordAttribute)
            
            for binding in range (0, len(nodes)):
                currentTexImage = texImageFunction % ((binding + VKTS_BINDING_UNIFORM_SAMPLER_BSDF_FIRST), binding)
                currentFragmentGLSL = currentFragmentGLSL.replace("#nextTexture#", currentTexImage)
                
                fw("add_texture %s\n" % (friendlyName(material.name) + "_" + friendlyNodeName(nodes[binding].name) + "_texture" ))    
                fw("\n")
                
            fw("attributes %x\n" % (vertexAttributes))
            fw("\n")
                
            currentFragmentGLSL = currentFragmentGLSL.replace("#nextAttribute#", "")

            currentFragmentGLSL = currentFragmentGLSL.replace("#nextTexture#", "")

            currentFragmentGLSL = currentFragmentGLSL.replace("#nextTangents#", "")                

            currentFragmentGLSL = currentFragmentGLSL.replace("#previousMain#", "")
                                
            #

            fragmentShaderFilepath = os.path.dirname(filepath) + "/" + friendlyName(materialName) + ".frag"
        
            file_fragmentShader = open(fragmentShaderFilepath, "w", encoding="utf8", newline="\n")
            
            file_fragmentShader.write("%s\n" % currentFragmentGLSL)
            
            file_fragmentShader.close()
        
        else:
        
            #
            # Blender internal material.
            #
        
            fw("shading Phong\n")
            fw("\n")        
            fw("name %s\n" % friendlyName(materialName))
            fw("\n")

            emissiveWritten = False
            alphaWritten = False
            displacementWritten = False
            normalWritten = False
            
            for currentTextureSlot in material.texture_slots:
                if currentTextureSlot and currentTextureSlot.texture and currentTextureSlot.texture.type == 'IMAGE':
                    image = currentTextureSlot.texture.image
                    if image and image.has_data:
                        if currentTextureSlot.use_map_emit:
                            fw("emissive_texture %s\n" % (friendlyImageName(currentTextureSlot.texture.name) + "_texture"))
                            emissiveWritten = True
                        if currentTextureSlot.use_map_alpha:
                            fw("alpha_texture %s\n" % (friendlyImageName(currentTextureSlot.texture.name) + "_texture"))
                            alphaWritten = True
                        if currentTextureSlot.use_map_displacement:
                            fw("displacement_texture %s\n" % (friendlyImageName(currentTextureSlot.texture.name) + "_texture"))
                            displacementWritten = True
                        if currentTextureSlot.use_map_normal:
                            fw("normal_texture %s\n" % (friendlyImageName(currentTextureSlot.texture.name) + "_texture"))
                            normalWritten = True

            if not emissiveWritten:
                fw("emissive_color %f %f %f\n" % (material.emit * material.diffuse_color[0], material.emit * material.diffuse_color[1], material.emit * material.diffuse_color[2]))
            if not alphaWritten:
                fw("alpha_value %f\n" % material.alpha)
            if not displacementWritten:
                fw("displacement_value 0.0\n")
            if not normalWritten:
                fw("normal_vector 0.0 0.0 1.0\n")

            phongAmbientWritten = False
            phongDiffuseWritten = False
            phongSpecularWritten = False
            phongSpecularShininessWritten = False

            for currentTextureSlot in material.texture_slots:
                if currentTextureSlot and currentTextureSlot.texture and currentTextureSlot.texture.type == 'IMAGE':
                    image = currentTextureSlot.texture.image
                    if image:
                        if currentTextureSlot.use_map_ambient:
                            fw("phong_ambient_texture %s\n" % (friendlyImageName(currentTextureSlot.texture.name) + "_texture"))
                            phongAmbientWritten = True
                        if currentTextureSlot.use_map_color_diffuse:
                            fw("phong_diffuse_texture %s\n" % (friendlyImageName(currentTextureSlot.texture.name) + "_texture"))
                            phongDiffuseWritten = True
                        if currentTextureSlot.use_map_color_spec:
                            fw("phong_specular_texture %s\n" % (friendlyImageName(currentTextureSlot.texture.name) + "_texture"))
                            phongSpecularWritten = True
                        if currentTextureSlot.use_map_hardness:
                            fw("phong_specular_shininess_texture %s\n" % (friendlyImageName(currentTextureSlot.texture.name) + "_texture"))
                            phongSpecularShininessWritten = True

            if not phongAmbientWritten:
                fw("phong_ambient_color %f %f %f\n" % (material.ambient * context.scene.world.ambient_color[0], material.ambient * context.scene.world.ambient_color[1], material.ambient * context.scene.world.ambient_color[2]))
            if not phongDiffuseWritten:
                fw("phong_diffuse_color %f %f %f\n" % (material.diffuse_intensity * material.diffuse_color[0], material.diffuse_intensity * material.diffuse_color[1], material.diffuse_intensity * material.diffuse_color[2]))
            if not phongSpecularWritten:
                fw("phong_specular_color %f %f %f\n" % (material.specular_intensity * material.specular_color[0], material.specular_intensity * material.specular_color[1], material.specular_intensity * material.specular_color[2]))
            if not phongSpecularShininessWritten:
                fw("phong_specular_shininess_value %f\n" % ((float(material.specular_hardness) - 1.0) / 510.0))
                
            if material.use_transparency:
                fw("\n")    
                fw("transparent true\n")

            fw("\n")
    
    file.close()

    return allEnvironmentTextures

def saveMeshes(context, filepath, materialsLibraryName, subMeshLibraryName):
    
    subMeshLibraryFilepath = os.path.dirname(filepath) + "/" + subMeshLibraryName

    file_subMesh = open(subMeshLibraryFilepath, "w", encoding="utf8", newline="\n")
    fw_subMesh = file_subMesh.write
    fw_subMesh("#\n")
    fw_subMesh("# VulKan ToolS sub meshes.\n")
    fw_subMesh("#\n")
    fw_subMesh("\n")

    if materialsLibraryName is not None:
        fw_subMesh("material_library %s\n" % friendlyName(materialsLibraryName))
        fw_subMesh("\n")

    file = open(filepath, "w", encoding="utf8", newline="\n")
    fw = file.write
    fw("#\n")
    fw("# VulKan ToolS meshes.\n")
    fw("#\n")
    fw("\n")

    if subMeshLibraryName is not None:
        fw("submesh_library %s\n" % friendlyName(subMeshLibraryName))
        fw("\n")

    meshes = {}

    meshToObject = {}
    meshToVertexGroups = {}
    groupNameToIndex = {}

    # Gather all meshes.
    for currentObject in context.scene.objects:

        if currentObject.type == 'MESH':

            meshes.setdefault(currentObject.data)
            
            meshToVertexGroups[currentObject.data] = currentObject.vertex_groups
            meshToObject[currentObject.data] = currentObject

        if currentObject.type == 'ARMATURE':
            jointIndex = 0
            for currentPoseBone in currentObject.pose.bones:
                groupNameToIndex[currentPoseBone.name] = jointIndex
                jointIndex += 1

    # Save meshes.
    for mesh in meshes:

        currentGroups = meshToVertexGroups[mesh]

        #

        fw("#\n")
        fw("# Mesh.\n")
        fw("#\n")
        fw("\n")

        fw("name %s\n" % friendlyName(mesh.name))
        fw("\n")

        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.triangulate(bm, faces=bm.faces)

        materialIndex = 0

        maxMaterialIndex = 0
        for face in bm.faces:
            if face.material_index > maxMaterialIndex:
                maxMaterialIndex = face.material_index

        while materialIndex <= maxMaterialIndex:

            vertexIndex = 0
            
            doWrite = False

            indices = []
            indexToVertex = {}
            indexToNormal = {}
            hasUVs = bm.loops.layers.uv.active
            indexToUV = {}
            hasBones = bm.verts.layers.deform.active
            indexToBones = {}

            # Search for faces with same material.
            for face in bm.faces:
                
                if face.material_index == materialIndex:

                    doWrite = True
                    currentVertex = 0

                    for vert in face.verts:
                        
                        currentFaceVertex = 0
                        
                        if vertexIndex not in indices:
                                
                            indices.append(vertexIndex)
                            
                            indexToVertex.setdefault(vertexIndex, mathutils.Vector((vert.co.x, vert.co.z, -vert.co.y)))
                            
                            if face.smooth:
                                indexToNormal.setdefault(vertexIndex, mathutils.Vector((vert.normal.x, vert.normal.z, -vert.normal.y)))
                            else:
                                indexToNormal.setdefault(vertexIndex, mathutils.Vector((face.normal.x, face.normal.z, -face.normal.y)))
                                    
                            if hasUVs:
                                for loop in face.loops:
                                    if loop.vert == vert:
                                        indexToUV.setdefault(vertexIndex, mathutils.Vector((loop[hasUVs].uv.x, loop[hasUVs].uv.y)))

                            if hasBones:
                                for searchVertex in bm.verts:
                                    if searchVertex.index == vert.index:
                                        deformVertex = searchVertex[hasBones]
                                        indexToBones.setdefault(vertexIndex, deformVertex)
                                            
                            currentFaceVertex += 1
                                                                
                            currentVertex += 1

                            vertexIndex += 1

            if doWrite:
                subMeshName = mesh.name + "_" + str(materialIndex)

                fw("submesh %s\n" % friendlyName(subMeshName))

                fw_subMesh("#\n")
                fw_subMesh("# Sub mesh.\n")
                fw_subMesh("#\n")
                fw_subMesh("\n")
                
                fw_subMesh("name %s\n" % friendlyName(subMeshName))
                fw_subMesh("\n")

                # Store only the vertices used by this material and faces.
                for vertIndex in indices:
                    vert = indexToVertex[vertIndex]
                    fw_subMesh("vertex %f %f %f 1.0\n" % (vert.x, vert.y, vert.z))
                fw_subMesh("\n")
                for vertIndex in indices:
                    normal = indexToNormal[vertIndex]
                    fw_subMesh("normal %f %f %f\n" % (normal.x, normal.y, normal.z))
                fw_subMesh("\n")
                            
                if hasUVs:

                    indexToBitangent = {}
                    indexToTangent = {}

                    vertexIndex = 0

                    for face in bm.faces:
                        if face.material_index == materialIndex:
                            
                            tempIndices = []
                            P = []
                            UV = []

                            for vert in face.verts:
                                
                                tempIndices.append(vertexIndex)
                                P.append(indexToVertex[vertexIndex])
                                UV.append(indexToUV[vertexIndex])

                                vertexIndex += 1                                    

                            for index in range(0, 3):
                                Q1 = P[(index + 1) % 3] - P[index]
                                Q2 = P[(index + 2) % 3] - P[index]
                                    
                                UV1 = UV[(index + 1) % 3] - UV[index]
                                UV2 = UV[(index + 2) % 3] - UV[index]
                                    
                                s1 = UV1.x
                                t1 = UV1.y
                                s2 = UV2.x
                                t2 = UV2.y

                                divisor = s1*t2-s2*t1
                                if divisor == 0.0:
                                    divisor = 1.0

                                factor = 1.0/divisor

                                bitangent = mathutils.Vector((Q1.x * -s2 + Q2.x * s1, Q1.y * -s2 + Q2.y * s1, Q1.z * -s2 + Q2.z * s1)) * factor                        
                                tangent = mathutils.Vector((Q1.x * t2 + Q2.x * -t1, Q1.y * t2 + Q2.y * -t1, Q1.z * t2 + Q2.z * -t1)) * factor

                                bitangent.normalize()
                                tangent.normalize()

                                indexToBitangent.setdefault(tempIndices[index], bitangent)
                                indexToTangent.setdefault(tempIndices[index], tangent)

                    for vertIndex in indices:
                        bitangent = indexToBitangent[vertIndex]
                        fw_subMesh("bitangent %f %f %f\n" % (bitangent.x, bitangent.y, bitangent.z))
                    fw_subMesh("\n")
                                
                    for vertIndex in indices:
                        tangent = indexToTangent[vertIndex]
                        fw_subMesh("tangent %f %f %f\n" % (tangent.x, tangent.y, tangent.z))
                    fw_subMesh("\n")

                    for vertIndex in indices:
                        uv = indexToUV[vertIndex]
                        fw_subMesh("texcoord %f %f\n" % (uv.x, uv.y))
                    fw_subMesh("\n")
                    
                else:
                    # Save default texture coordinates.
                    for vertIndex in indices:
                        fw_subMesh("texcoord 0.0 0.0\n")
                    fw_subMesh("\n")

                # Save bones when available.
                if hasBones:

                    allBoneIndices = {}

                    allBoneWeights = {}

                    allNumberBones = []
                    
                    for vertIndex in indices:

                        tempBoneIndices = []

                        tempBoneWeights = []
                        
                        deformVertex = indexToBones[vertIndex]

                        numberBones = 0.0
                        
                        for item in deformVertex.items():
                            
                            group = currentGroups[item[0]]
                            
                            tempBoneIndices.append(groupNameToIndex[group.name])
                            
                            tempBoneWeights.append(item[1])
                            numberBones += 1.0

                            if numberBones == 8.0:
                                break

                        for iterate in range(int(numberBones), 8):
                            tempBoneIndices.append(-1.0)
                            tempBoneWeights.append(0.0)

                        allBoneIndices.setdefault(vertIndex, tempBoneIndices)
                        allBoneWeights.setdefault(vertIndex, tempBoneWeights)

                        allNumberBones.append(numberBones)

                    for vertIndex in indices:
                        tempBoneIndices = allBoneIndices[vertIndex]

                        fw_subMesh("boneIndex")
                        for currentIndex in tempBoneIndices:
                            fw_subMesh(" %.1f" % currentIndex)
                        fw_subMesh("\n")
                    fw_subMesh("\n")

                    for vertIndex in indices:
                        tempBoneWeights = allBoneWeights[vertIndex]

                        fw_subMesh("boneWeight")
                        for currentWeight in tempBoneWeights:
                            fw_subMesh(" %f" % currentWeight)
                        fw_subMesh("\n")
                    fw_subMesh("\n")

                    for currentNumberBones in allNumberBones:
                        fw_subMesh("numberBones %.1f\n" % currentNumberBones)
                    fw_subMesh("\n")

                # Save face and adjust face index, if needed.
                for index in indices:
                    # Indices go from 0 to maximum vertices.
                    if index % 3 == 0:
                        fw_subMesh("face")
                    fw_subMesh(" %d" % index)
                    if index % 3 == 2:
                        fw_subMesh("\n")
                    
                fw_subMesh("\n")

                if len(mesh.materials) > 0:
                    fw_subMesh("material %s\n" % friendlyName(mesh.materials[materialIndex].name))
                else:
                    fw_subMesh("material %s\n" % friendlyName("DefaultMaterial"))
                fw_subMesh("\n")
            
            materialIndex += 1

        fw("\n")

        currentObject = meshToObject[mesh]

        #
        # Save displacement, if available.
        #

        if "Displace" in currentObject.modifiers:
            fw("displace %f %f\n" % (currentObject.modifiers["Displace"].mid_level, currentObject.modifiers["Displace"].strength))
            fw("\n")

        #
        # Save AABB.
        #

        center = mathutils.Vector(convertLocation((currentObject.bound_box[0][0], currentObject.bound_box[0][1], currentObject.bound_box[0][2])))

        for corner in range(1, 8):
            center = center + mathutils.Vector(convertLocation((currentObject.bound_box[corner][0], currentObject.bound_box[corner][1], currentObject.bound_box[corner][2])))

        center = center / 8.0

        scale = center - mathutils.Vector(convertLocation((currentObject.bound_box[0][0], currentObject.bound_box[0][1], currentObject.bound_box[0][2])))

        fw("aabb %f %f %f %f %f %f\n" % (center.x, center.y, center.z, math.fabs(scale.x), math.fabs(scale.y), math.fabs(scale.z)))
        fw("\n")

    file.close()

    file_subMesh.close()

    return

def saveAnimation(context, fw, fw_animation, fw_channel, name, currentAnimation, filterName, isJoint):

    hasData = False

    # Check, animation data for a specific filter is available. 

    for currentCurve in currentAnimation.action.fcurves:
        if filterName == extractNode(currentCurve.data_path):
            hasData = True
            break

    if not hasData:
        return

    fw("animation %s\n" % friendlyName("animation_" + name))
    fw("\n")

    fw_animation("#\n")
    fw_animation("# Animation.\n")
    fw_animation("#\n")
    fw_animation("\n")
    fw_animation("name %s\n" % friendlyName("animation_" + name))
    fw_animation("\n")
    fw_animation("start %f\n" % (context.scene.frame_start / context.scene.render.fps))
    fw_animation("stop %f\n" % (context.scene.frame_end / context.scene.render.fps))
    fw_animation("\n")

    # Loop over curves several times to achieve sorting.
    for usedTransform in ["TRANSLATE", "ROTATE", "QUATERNION_ROTATE", "SCALE"]:

        dataWritten = False

        for usedElement in ["X", "Y", "Z", "W"]:

            for currentCurve in currentAnimation.action.fcurves:

                currentFilterName = extractNode(currentCurve.data_path)

                if currentFilterName != filterName:
                    continue


                transform = friendlyTransformName(currentCurve.data_path)
                element = friendlyElementName(currentCurve.array_index, currentCurve.data_path, isJoint)

                if transform != usedTransform or element != usedElement:
                    continue

                if not dataWritten:
                    fw_animation("# %s\n" % (transform.lower().title() + "."))
                    fw_animation("\n")

                dataWritten = True
                
                fw_animation("channel %s\n" % friendlyName("channel_" + transform + "_" + element + "_" + name))

                # Save the channel.

                fw_channel("#\n")
                fw_channel("# Channel.\n")
                fw_channel("#\n")
                fw_channel("\n")
                fw_channel("name %s\n" % friendlyName("channel_" + transform + "_" + element + "_" + name))
                fw_channel("\n")
                fw_channel("target_transform %s\n" % transform)
                fw_channel("target_element %s\n" % element)
                fw_channel("\n")

                for currentKeyframe in currentCurve.keyframe_points:

                    value = currentKeyframe.co[1]
                    leftValue = currentKeyframe.handle_left[1]
                    rightValue = currentKeyframe.handle_right[1]

                    if element == "Z" and transform != "SCALE" and not isJoint:
                        value = -value
                        leftValue = -leftValue
                        rightValue = -rightValue
                        
                    if transform == "ROTATE":
                        value = math.degrees(value)
                        leftValue = math.degrees(leftValue)
                        rightValue = math.degrees(rightValue)

                    if currentKeyframe.interpolation == 'BEZIER':
                        fw_channel("keyframe %f %f BEZIER %f %f %f %f\n" % (currentKeyframe.co[0] / context.scene.render.fps, value, currentKeyframe.handle_left[0] / context.scene.render.fps, leftValue, currentKeyframe.handle_right[0] / context.scene.render.fps, rightValue))        
                    elif currentKeyframe.interpolation == 'LINEAR':
                        fw_channel("keyframe %f %f LINEAR\n" % (currentKeyframe.co[0] / context.scene.render.fps, value))    
                    elif currentKeyframe.interpolation == 'CONSTANT':
                        fw_channel("keyframe %f %f CONSTANT\n" % (currentKeyframe.co[0] / context.scene.render.fps, value))    
                    
                fw_channel("\n")

        if dataWritten:    
            fw_animation("\n")

    return

def saveBone(context, fw, fw_animation, fw_channel, currentPoseBone, armatureName, jointIndex, animation_data):

    parentPoseBone = currentPoseBone.parent
    if parentPoseBone is None:
        parentName = armatureName
    else:
        parentName = parentPoseBone.name


    fw("# Node.\n")
    fw("\n")

    # This matrix, as it is from the pose, already has the wanted coordinate system.
    # Switch to "Local" transformation orientation in Blender to see it.

    location, rotation, scale = currentPoseBone.matrix_basis.decompose()        

    fw("node %s %s\n" % (friendlyName(currentPoseBone.name), friendlyName(parentName)))
    fw("\n")
    fw("translate %f %f %f\n" % (convertLocationNoAdjust(location)))
    fw("rotate %f %f %f\n" % (convertRotationNoAdjust(rotation.to_euler())))
    fw("scale %f %f %f\n" % (convertScaleNoAdjust(scale)))
    fw("\n")
    fw("jointIndex %d\n" % jointIndex)
    fw("\n")

    # This matrix, as it is from the armature, already has the wanted coordinate system.
    # Also, even if noted as local, the matrix is the result of the matrix multiplication of the bone hierarchy.
    # To get the "real" local matrix, the inverted parent matrix has to be multiplied.
    # As the root bone is relative to the Blender original coordinate system, the root bone has to be converted.
    # This has not to be done for the child bones, as this is canceled out through the inverted parent matrix.

    convertMatrix = mathutils.Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, -1, 0, 0) , (0, 0, 0, 1)))

    bindMatrix = currentPoseBone.bone.matrix_local

    parentBone = currentPoseBone.bone.parent
    if parentBone:
        bindMatrix = parentBone.matrix_local.inverted() * bindMatrix
    else:
        bindMatrix = convertMatrix * bindMatrix
    
    location, rotation, scale = bindMatrix.decompose()        

    location = convertLocationNoAdjust(location)
    rotation = convertRotationNoAdjust(rotation.to_euler())
    scale = convertScaleNoAdjust(scale)

    fw("bind_translate %f %f %f\n" % (location))
    fw("bind_rotate %f %f %f\n" % (rotation))
    fw("bind_scale %f %f %f\n" % (scale))
    fw("\n")
    
    if animation_data is not None:
        saveAnimation(context, fw, fw_animation, fw_channel, currentPoseBone.name, animation_data, currentPoseBone.name, True)
    
    return

def saveNode(context, fw, fw_animation, fw_channel, currentObject):

    #convertMatrix = mathutils.Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, -1, 0, 0) , (0, 0, 0, 1)))

    #location, rotation, scale = (convertMatrix * currentObject.matrix_local).decompose()
    location, rotation, scale = currentObject.matrix_local.decompose()

    location = convertLocation(location)
    rotation = convertRotation(rotation.to_euler())
    scale = convertScale(scale)

    parentObject = currentObject.parent
    if parentObject is None:
        parentName = "-"
    else:
        parentName = parentObject.name

    fw("# Node.\n")
    fw("\n")

    fw("node %s %s\n" % (friendlyName(currentObject.name), friendlyName(parentName)))
    fw("\n")
    fw("translate %f %f %f\n" % location)
    fw("rotate %f %f %f\n" % rotation)
    fw("scale %f %f %f\n" % scale)
    fw("\n")

    if currentObject.type == 'MESH':
        fw("mesh %s\n" % friendlyName(currentObject.data.name))
        fw("\n")

    if currentObject.animation_data is not None:
        saveAnimation(context, fw, fw_animation, fw_channel, currentObject.name, currentObject.animation_data, None, False)

    if currentObject.type == 'ARMATURE':
        fw("joints %d\n" % len(currentObject.pose.bones.values()))
        fw("\n")

        jointIndex = 0
        for currentPoseBone in currentObject.pose.bones:
            saveBone(context, fw, fw_animation, fw_channel, currentPoseBone, currentObject.name, jointIndex, currentObject.animation_data)
            jointIndex += 1

    for childObject in currentObject.children:
        saveNode(context, fw, fw_animation, fw_channel, childObject)
    
    return

def saveObjects(context, filepath, meshLibraryName, animationLibraryName, channelLibraryName):

    channelLibraryFilepath = os.path.dirname(filepath) + "/" + channelLibraryName

    file_channel = open(channelLibraryFilepath, "w", encoding="utf8", newline="\n")
    fw_channel = file_channel.write
    fw_channel("#\n")
    fw_channel("# VulKan ToolS channels.\n")
    fw_channel("#\n")
    fw_channel("\n")


    animationLibraryFilepath = os.path.dirname(filepath) + "/" + animationLibraryName

    file_animation = open(animationLibraryFilepath, "w", encoding="utf8", newline="\n")
    fw_animation = file_animation.write
    fw_animation("#\n")
    fw_animation("# VulKan ToolS animations.\n")
    fw_animation("#\n")
    fw_animation("\n")

    if channelLibraryName is not None:
        fw_animation("channel_library %s\n" % friendlyName(channelLibraryName))
        fw_animation("\n")


    file = open(filepath, "w", encoding="utf8", newline="\n")
    fw = file.write
    fw("#\n")
    fw("# VulKan ToolS objects.\n")
    fw("#\n")
    fw("\n")

    if meshLibraryName is not None:
        fw("mesh_library %s\n" % friendlyName(meshLibraryName))
        fw("\n")

    if animationLibraryName is not None:
        fw("animation_library %s\n" % friendlyName(animationLibraryName))
        fw("\n")

    for currentObject in context.scene.objects:

        if currentObject.type == 'CAMERA':
            continue

        if currentObject.type == 'CURVE':
            continue

        if currentObject.type == 'FONT':
            continue

        if currentObject.type == 'LAMP':
            continue

        if currentObject.type == 'LATTICE':
            continue

        if currentObject.type == 'META':
            continue

        if currentObject.type == 'PATH':
            continue

        if currentObject.type == 'SPEAKER':
            continue

        if currentObject.type == 'SURFACE':
            continue

        if currentObject.parent is not None:
            continue;
        
        fw("#\n")
        fw("# Object.\n")
        fw("#\n")
        fw("\n")

        fw("name %s\n" % friendlyName(currentObject.name))
        fw("\n")
                    
        saveNode(context, fw, fw_animation, fw_channel, currentObject)
    
    file.close()

    file_animation.close()

    file_channel.close()

    return

def save(operator,
         context,
         filepath="",
         global_matrix=None
         ):

    if global_matrix is None:
        from mathutils import Matrix
        global_matrix = Matrix()

    sceneFilepath = filepath
        
    file = open(sceneFilepath, "w", encoding="utf8", newline="\n")
    fw = file.write
    fw("#\n")
    fw("# VulKan ToolS scene.\n")
    fw("#\n")
    fw("\n")

    fw("scene_name %s\n" % friendlyName(context.scene.name))
    fw("\n")
    
    imagesLibraryName = bpy.path.basename(sceneFilepath).replace(".vkts", "_images.vkts")

    texturesLibraryName = bpy.path.basename(sceneFilepath).replace(".vkts", "_textures.vkts")

    #

    materialsLibraryName = bpy.path.basename(sceneFilepath).replace(".vkts", "_materials.vkts")

    materialsLibraryFilepath = os.path.dirname(sceneFilepath) + "/" + materialsLibraryName

    allEnvironmentTextures = saveMaterials(context, materialsLibraryFilepath, texturesLibraryName, imagesLibraryName)

    #

    subMeshLibraryName = bpy.path.basename(sceneFilepath).replace(".vkts", "_submeshes.vkts")

    meshLibraryName = bpy.path.basename(sceneFilepath).replace(".vkts", "_meshes.vkts")
    
    meshLibraryFilepath = os.path.dirname(sceneFilepath) + "/" + meshLibraryName

    saveMeshes(context, meshLibraryFilepath, materialsLibraryName, subMeshLibraryName)

    #

    animationLibraryName = bpy.path.basename(sceneFilepath).replace(".vkts", "_animations.vkts")

    channelLibraryName = bpy.path.basename(sceneFilepath).replace(".vkts", "_channels.vkts")

    #

    objectLibraryName = bpy.path.basename(sceneFilepath).replace(".vkts", "_objects.vkts")

    fw("object_library %s\n" % friendlyName(objectLibraryName))
    fw("\n")
    
    objectLibraryFilepath = os.path.dirname(sceneFilepath) + "/" + objectLibraryName

    saveObjects(context, objectLibraryFilepath, meshLibraryName, animationLibraryName, channelLibraryName)

    #
    
    for currentObject in context.scene.objects:

        if currentObject.type == 'CAMERA':
            continue

        if currentObject.type == 'CURVE':
            continue

        if currentObject.type == 'FONT':
            continue

        if currentObject.type == 'LAMP':
            continue

        if currentObject.type == 'LATTICE':
            continue

        if currentObject.type == 'META':
            continue

        if currentObject.type == 'PATH':
            continue

        if currentObject.type == 'SPEAKER':
            continue

        if currentObject.type == 'SURFACE':
            continue

        if currentObject.parent is not None:
            continue;
        
        fw("#\n")
        fw("# Object instance.\n")
        fw("#\n")
        fw("\n")

        fw("object %s\n" % friendlyName(currentObject.name))
        fw("\n")
        fw("name %s\n" % friendlyName(currentObject.name))
        fw("\n")
        fw("translate 0.0 0.0 0.0\n")
        fw("rotate 0.0 0.0 0.0\n")
        fw("scale 1.0 1.0 1.0\n")
        fw("\n")
        
    for currentEnvironmentTexture in allEnvironmentTextures:
        fw("#\n")
        fw("# Environment texture.\n")
        fw("#\n")
        fw("\n")

        fw("environment %s\n" % (currentEnvironmentTexture))
        fw("\n")
        fw("texture %s\n" % (currentEnvironmentTexture))
        fw("\n")
    
    file.close()

    return {'FINISHED'}
