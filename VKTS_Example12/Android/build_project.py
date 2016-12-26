import glob
import os
import shutil
import subprocess  
import sys
  
from distutils.dir_util import copy_tree

####################
#
# Functions
#
####################

def copy(src, dst):
    # Create directory if needed
    if not os.path.exists(dst):
        os.makedirs(dst)

    # Copy exact file or given by wildcard 
    for filename in glob.glob(src):
        print("Copying asset '%s'" % (os.path.basename(filename)))
        shutil.copy(filename, dst)        
        
def validate():
        
    os.chdir("libs")    
        
    allArchs = os.listdir()

    for arch in allArchs:
        copy_tree(os.environ['ANDROID_NDK_HOME'] + "/sources/third_party/vulkan/src/build-android/jniLibs/" + arch, arch)
    
    os.chdir("..")
        
####################
#
# Main
#
####################

print("Copying project assets")

copy("../../VKTS_Binaries/shader/SPIR/V/font.vert.spv", "./assets/shader/SPIR/V/")
copy("../../VKTS_Binaries/shader/SPIR/V/font.frag.spv", "./assets/shader/SPIR/V/")
copy("../../VKTS_Binaries/shader/SPIR/V/font_df.vert.spv", "./assets/shader/SPIR/V/")
copy("../../VKTS_Binaries/shader/SPIR/V/font_df.frag.spv", "./assets/shader/SPIR/V/")

copy("../../VKTS_Binaries/shader/SPIR/V/bsdf.vert.spv", "./assets/shader/SPIR/V/")
copy("../../VKTS_Binaries/shader/SPIR/V/bsdf_no_texcoord.vert.spv", "./assets/shader/SPIR/V/")
copy("../../VKTS_Binaries/shader/SPIR/V/bsdf_tangents.vert.spv", "./assets/shader/SPIR/V/")
copy("../../VKTS_Binaries/shader/SPIR/V/bsdf_skinning.vert.spv", "./assets/shader/SPIR/V/")

copy("../../VKTS_Binaries/shader/SPIR/V/environment.vert.spv", "./assets/shader/SPIR/V/")
copy("../../VKTS_Binaries/shader/SPIR/V/environment.frag.spv", "./assets/shader/SPIR/V/")

copy("../../VKTS_Binaries/shader/SPIR/V/texture_ndc.vert.spv", "./assets/shader/SPIR/V/")
copy("../../VKTS_Binaries/shader/SPIR/V/resolve_bsdf.frag.spv", "./assets/shader/SPIR/V/")

copy("../../VKTS_Binaries/font/*.fnt", "./assets/font/")
copy("../../VKTS_Binaries/font/*.tga", "./assets/font/")

copy("../../VKTS_Binaries/buster_drone/*.spv", "./assets/buster_drone/")
copy("../../VKTS_Binaries/buster_drone/*.vkts", "./assets/buster_drone/")
copy("../../VKTS_Binaries/buster_drone/*.tga", "./assets/buster_drone/")

copy("../../VKTS_Binaries/scifi_helmet/*.spv", "./assets/scifi_helmet/")
copy("../../VKTS_Binaries/scifi_helmet/*.vkts", "./assets/scifi_helmet/")
copy("../../VKTS_Binaries/scifi_helmet/*.tga", "./assets/scifi_helmet/")
copy("../../VKTS_Binaries/scifi_helmet/*.hdr", "./assets/scifi_helmet/")

copy("../../VKTS_Binaries/primitives/*.vkts", "./assets/primitives/")

copy("../../VKTS_Binaries/texture/BSDF_LUT_512_8.data", "./assets/texture/")

copy("../../VKTS_Binaries/cache/buster_drone/*.tga", "./assets/cache/buster_drone/")

copy("../../VKTS_Binaries/cache/scifi_helmet/*.tga", "./assets/cache/scifi_helmet/")
copy("../../VKTS_Binaries/cache/scifi_helmet/*.hdr", "./assets/cache/scifi_helmet/")

print("Building project")

os.chdir("jni")

subprocess.call("ndk-build", shell=True)

os.chdir("..")

for x in range(1, len(sys.argv)):
    if sys.argv[x] == "validate":
        validate()
        break
        
subprocess.call("ant debug", shell=True)  
