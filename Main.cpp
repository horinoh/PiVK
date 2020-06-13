#include <iostream>
#include <cassert>
#include <vector>

#define VK_USE_PLATFORM_XLB_KHR
#include <vulkan/vulkan.h>

#define VERIFY_SUCCEEDED(VR) if(VK_SUCCESS != VR) { assert(false && VR); } 

int main()
{ 
  uint32_t APIVersion;
  vkEnumerateInstanceVersion(&APIVersion);
  const auto MajorVersion = VK_VERSION_MAJOR(APIVersion);
  const auto MinorVersion = VK_VERSION_MINOR(APIVersion);
  const auto PatchVersion = VK_VERSION_PATCH(APIVersion);
  std::cout << "Version = " << MajorVersion << "." << MinorVersion << ".(Header = " << VK_HEADER_VERSION << ")(Patch = " << PatchVersion << ")" << std::endl;

  uint32_t Count = 0;
  VERIFY_SUCCEEDED(vkEnumerateInstanceLayerProperties(&Count, nullptr));
  if (Count) {
    std::vector<VkLayerProperties> LPs(Count);
    VERIFY_SUCCEEDED(vkEnumerateInstanceLayerProperties(&Count, LPs.data()));
    for (const auto& i : LPs) {
      std::cout << i.layerName << ", " << i.description << std::endl;

      VERIFY_SUCCEEDED(vkEnumerateInstanceExtensionProperties(i.layerName, &Count, nullptr));
      if (Count) {
	std::vector<VkExtensionProperties> EPs(Count);
	VERIFY_SUCCEEDED(vkEnumerateInstanceExtensionProperties(i.layerName, &Count, EPs.data()));
	for (const auto& j : EPs) {
	  std::cout << "\t" << j.extensionName << std::endl;
	}
      }
    }
  }
  
  const VkApplicationInfo AI = {
   VK_STRUCTURE_TYPE_APPLICATION_INFO,
   nullptr,
   "AppName", 0,
   "EngineName", 0,
   APIVersion
  };
  const std::vector<const char*> InstanceLayers = {};
  const std::vector<const char*> InstanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, };
  const VkInstanceCreateInfo ICI = {
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    nullptr,
    0,
    &AI,
    static_cast<uint32_t>(InstanceLayers.size()), InstanceLayers.data(),
    static_cast<uint32_t>(InstanceExtensions.size()), InstanceExtensions.data(),
  };
  VkInstance Instance;
  VERIFY_SUCCEEDED(vkCreateInstance(&ICI, nullptr, &Instance));

  return 0;
}
