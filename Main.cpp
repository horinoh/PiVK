#include <iostream>
#include <cassert>
#include <vector>
#include <array>

#include <xcb/xcb.h>

//#define VK_USE_PLATFORM_XLB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>

#define VERIFY_SUCCEEDED(VR) if(VK_SUCCESS != VR) { std::cerr << "VkResult = " << VR << std::endl; assert(false); } 

const VkAllocationCallbacks* GetAllocationCallbacks() { return nullptr; }

int main()
{
	//!< X-Window
	xcb_connection_t* Connection;
	xcb_window_t Window;
	xcb_screen_t* Screen;
	{
		Connection = xcb_connect(nullptr, nullptr);
		assert(0 == xcb_connection_has_error(Connection) && "");

		Screen = xcb_setup_roots_iterator(xcb_get_setup(Connection)).data;

		Window = xcb_generate_id(Connection);
		const std::array<uint32_t, 2> Values = { Screen->white_pixel, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS };
		xcb_create_window(Connection, Screen->root_depth, Window, Screen->root,
			0, 0, 1280, 720, 1,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, Screen->root_visual,
			XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, Values.data());

		xcb_map_window(Connection, Window);
		xcb_flush(Connection);
	}

	//!< Version
	uint32_t APIVersion;
	{
		vkEnumerateInstanceVersion(&APIVersion);
		const auto MajorVersion = VK_VERSION_MAJOR(APIVersion);
		const auto MinorVersion = VK_VERSION_MINOR(APIVersion);
		const auto PatchVersion = VK_VERSION_PATCH(APIVersion);
		std::cout << "Version = " << MajorVersion << "." << MinorVersion << ".(Header = " << VK_HEADER_VERSION << ")(Patch = " << PatchVersion << ")" << std::endl;
	}

	//!< Layers, Extensions
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

	//!< Instance
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
		const std::vector<const char*> InstanceExtensions = { VK_KHR_XCB_SURFACE_EXTENSION_NAME };
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

	//!< Debug
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
	VkSurfaceKHR Surface = VK_NULL_HANDLE;
	{
		const VkXcbSurfaceCreateInfoKHR SCI = {
			VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			Connection,
			Window
		};
		VERIFY_SUCCEEDED(vkCreateXcbSurfaceKHR(Instance, &SCI, GetAllocationCallbacks(), &Surface));
	}

	//!< Physical device
	std::vector<VkPhysicalDevice> PhysicalDevices;
	std::vector<VkPhysicalDeviceMemoryProperties> PhysicalDeivceMemoryProperties;
	{
		uint32_t Count = 0;
		VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, nullptr));
		PhysicalDevices.resize(Count);
		VERIFY_SUCCEEDED(vkEnumeratePhysicalDevices(Instance, &Count, PhysicalDevices.data()));
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

	//!< Device
#if 0
	VkDevice Device;
	{
		const auto& PD = PhysicalDevices[0];
		std::vector<VkQueueFamilyProperties> QFPs;
		uint32_t Count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(PD, &Count, nullptr);
		QFPs.resize(Count);
		vkGetPhysicalDeviceQueueFamilyProperties(PD, &Count, QFPs.data());
		for (size_t i = 0; i < QFPs.size();++i) {
			if (VK_QUEUE_GRAPHICS_BIT & QFPs[i].queueFlags) { std::cout << "GRAPHICS | "; }
			if (VK_QUEUE_COMPUTE_BIT & QFPs[i].queueFlags) { std::cout << "COMPUTE | "; }
			if (VK_QUEUE_TRANSFER_BIT & QFPs[i].queueFlags) { std::cout << "TRANSFER | "; }
			std::cout << std::endl;

			VkBool32 b = VK_FALSE;
			VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceSupportKHR(PD, i, Surface, &b));
			if (b) { std::cout << "SurfaceSupported" << std::endl; }
		}

		std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos = {}; //TODO
		const std::array<const char*, 2> DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_VALIDATION_CACHE_EXTENSION_NAME, };
		VkPhysicalDeviceFeatures PDF;
		vkGetPhysicalDeviceFeatures(PD, &PDF);
		const VkDeviceCreateInfo DCI = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			nullptr,
			0,
			static_cast<uint32_t>(QueueCreateInfos.size()), QueueCreateInfos.data(),
			0, nullptr,
			static_cast<uint32_t>(DeviceExtensions.size()), DeviceExtensions.data(),
			&PDF
		};
		VERIFY_SUCCEEDED(vkCreateDevice(PD, &DCI, GetAllocationCallbacks(), &Device));
	}
#endif

	//!< Loop
	{
		auto LoopEnd = false;
		xcb_generic_event_t* Event;
		while (!LoopEnd && (Event = xcb_wait_for_event(Connection))) {
			switch (Event->response_type & ~0x80) {
			case XCB_KEY_PRESS: LoopEnd = true; break;
			}
			free(Event);
		}
	}

	//!< Destruct
	{
		//vkDestroyDevice(Device, GetAllocationCallbacks());
		vkDestroySurfaceKHR(Instance, Surface, GetAllocationCallbacks());
#if 0
		vkDestroyDebugReportCallback(Instance, DebugReportCallback, GetAllocationCallbacks());
#endif
		vkDestroyInstance(Instance, GetAllocationCallbacks());

		xcb_disconnect(Connection);
	}

	return 0;
}
