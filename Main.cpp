#include <iostream>
#include <cassert>
#include <vector>

//#include <X11/Xlib.h>

#define VK_USE_PLATFORM_XLB_KHR
#include <vulkan/vulkan.h>

#define VERIFY_SUCCEEDED(VR) if(VK_SUCCESS != VR) { assert(false && VR); } 

const VkAllocationCallbacks* GetAllocationCallbacks() { return nullptr; }

int main()
{
	uint32_t APIVersion;
	vkEnumerateInstanceVersion(&APIVersion);
	const auto MajorVersion = VK_VERSION_MAJOR(APIVersion);
	const auto MinorVersion = VK_VERSION_MINOR(APIVersion);
	const auto PatchVersion = VK_VERSION_PATCH(APIVersion);
	std::cout << "Version = " << MajorVersion << "." << MinorVersion << ".(Header = " << VK_HEADER_VERSION << ")(Patch = " << PatchVersion << ")" << std::endl;

	//!< layers, extensions
	{
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
	}

	//!< instance
	VkInstance Instance;
	{
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
		VERIFY_SUCCEEDED(vkCreateInstance(&ICI, GetAllocationCallbacks(), &Instance));
	}

	//!< debug
#if 0
	VkDebugReportCallbackEXT DebugReportCallback = VK_NULL_HANDLE;
	{
		const auto Flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT
			| VK_DEBUG_REPORT_WARNING_BIT_EXT
			| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
			| VK_DEBUG_REPORT_ERROR_BIT_EXT
			| VK_DEBUG_REPORT_DEBUG_BIT_EXT;
		const VkDebugReportCallbackCreateInfoEXT DRCCI = {
		VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		nullptr,
		Flags,
		// PFN_vkDebugReportCallbackEXT
		nullptr
		};
		vkCreateDebugReportCallbackEXT(Instance, &DRCCI, GetAllocationCallback(), &DebugReportCallback);
	}
#endif

	//!< surface
	//Display* XDisplay = XOpenDisplay(nullptr);
	VkSurfaceKHR Surface = VK_NULL_HANDLE;
	{
		//const auto XWindow = XDefaultRootWindow(XDisplay);
		//const VkDisplaySurfaceCreateInfoKHR SCI = {
		//	VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		//	nullptr,
		//	0,
		//	XDisplay,
		//	XWindow,
		//};
		//VERIFY_SUCCEEDED(VkXlibSurfaceCreateInfoKHR(Instance, &SCI, GetAllocationCallbacks(), &Surface)
	}

	//!< physical device
	std::vector<VkPhysicalDevice> PhysicalDevices;
	std::vector<VkPhysicalDeviceMemoryProperties> PhysicalDeivceMemoryProperties;
	{
		uint32_t Count = 0;
		VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, nullptr));
		PhysicalDevices.resize(Count);
		VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, PhysicalDevices.data()));
		std::cout << PhysicalDevices.size() << std::endl;
		for (const auto& i : PhysicalDevices) {
			VkPhysicalDeviceProperties PDP;
			vkGetPhysicalDeviceProperties(i, &PDP);
			if (VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU == PDP.deviceType) { std::cout << "INTEGRATED_GPU" << std::endl; }
			if (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == PDP.deviceType) { std::cout << "DISCRETE_GPU" << std::endl; }
			std::cout << "maxUniformBufferRange = " << PDP.limits.maxUniformBufferRange << std::endl;
			std::cout << "maxFragmentOutputAttachments = " << PDP.limits.maxFragmentOutputAttachments << std::endl;
			std::cout << "maxColorAttachments = " << PDP.limits.maxColorAttachments << std::endl;

			VkPhysicalDeviceFeatures PDF;
			vkGetPhysicalDeviceFeatures(i, &PDF);
			if (PDF.geometryShader) { std::cout << "geometryShader" << std::endl; }
			if (PDF.tessellationShader) { std::cout << "tessellationShader" << std::endl; }
			if (PDF.multiViewport) { std::cout << "multiViewport" << std::endl; }

			VkPhysicalDeviceMemoryProperties PDMP;
			vkGetPhysicalDeviceMemoryProperties(i, &PDMP);
			for (uint32_t i = 0; i < PDMP.memoryTypeCount; ++i) {
				std::cout << "[" << i << "] HeapIndex = " << PDMP.memoryTypes[i].heapIndex << ", ";
				std::cout << "PropertyFlags(" << PDMP.memoryTypes[i].propertyFlags << ") = ";
				if (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT & PDMP.memoryTypes[i].propertyFlags) { std::cout << "DEVICE_LOCAL | "; }
				if (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & PDMP.memoryTypes[i].propertyFlags) { std::cout << "HOST_VISIBLE | "; }
				std::cout << std::endl;
			}
			for (uint32_t i = 0; i < PDMP.memoryHeapCount; ++i) {
				std::cout << "Heap[" << i << "] Size = " << PDMP.memoryHeaps[i].size << ", ";
				std::cout << "Flags = ";
				if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & PDMP.memoryHeaps[i].flags) { std::cout << "DEVICE_LOCAL | "; }
				if (VK_MEMORY_HEAP_MULTI_INSTANCE_BIT_KHR & PDMP.memoryHeaps[i].flags) { std::cout << "MULTI_INSTANCE | "; }
				std::cout << std::endl;
			}
		}

		PhysicalDeivceMemoryProperties.resize(PhysicalDevices.size());
		for (size_t i = 0; i < PhysicalDevices.size();++i) {
			vkGetPhysicalDeviceMemoryProperties(PhysicalDevices[i], &PhysicalDeivceMemoryProperties[i]);
		}
	}

	//!< device
	{
	}


	//!< destruct
	{
		vkDestroySurfaceKHR(Instance, Surface, GetAllocationCallbacks());
		//XCloseDisplay(XDisplay);

#if 0
		vkDestroyDebugReportCallback(Instance, DebugReportCallback, GetAllocationCallbacks());
#endif
		vkDestroyInstance(Instance, GetAllocationCallbacks());
	}

	return 0;
}
