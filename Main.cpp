#include <iostream>
#include <cassert>
#include <vector>
#include <array>
#include <fstream>
#include <numeric>

#include <xcb/xcb.h>

//#define VK_USE_PLATFORM_XLB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#define VERIFY_SUCCEEDED(VR) if(VK_SUCCESS != VR) { std::cerr << "VkResult = " << VR << std::endl; assert(false); } 

const VkAllocationCallbacks* GetAllocationCallbacks() { return nullptr; }
static bool IsAligned(const size_t Size, const size_t Align) { return !(Size & ~Align); }
static size_t RoundDown(const size_t Size, const size_t Align) {
	if (IsAligned(Size, Align)) { return Size; }
	return (Size / Align) * Align;
}
static size_t RoundUp(const size_t Size, const size_t Align) {
	if (IsAligned(Size, Align)) { return Size; }
	return RoundDown(Size, Align) + Align;
}

static void CreateBuffer(VkBuffer* Buffer, const VkDevice Device, const VkBufferUsageFlags BUF, const VkDeviceSize Size)
{
	const VkBufferCreateInfo BCI = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		nullptr,
		0,
		Size,
		BUF,
		VK_SHARING_MODE_EXCLUSIVE,
		0, nullptr
	};
	VERIFY_SUCCEEDED(vkCreateBuffer(Device, &BCI, GetAllocationCallbacks(), Buffer));
}

static uint32_t GetMemoryTypeIndex(const VkPhysicalDevice PD, const VkMemoryRequirements& MR, const VkMemoryPropertyFlags MPF) {
	VkPhysicalDeviceMemoryProperties PDMP;
	vkGetPhysicalDeviceMemoryProperties(PD, &PDMP);
	for (uint32_t i = 0; i < PDMP.memoryTypeCount; ++i) {
		if (MR.memoryTypeBits & (1 << i)) {
			if ((PDMP.memoryTypes[i].propertyFlags & MPF) == MPF) {
				return i;
			}
		}
	}
	return static_cast<uint32_t>(0xffff);
};

static void CreateDeviceMemory(std::vector<VkDeviceMemory>& DMs, const VkDevice Device, const std::vector<std::vector<VkMemoryRequirements>>& MRs)
{
	DMs.assign(MRs.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < DMs.size(); ++i) {
		if (!MRs[i].empty()) {
			const VkMemoryAllocateInfo MAI = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				nullptr,
				std::accumulate(MRs[i].cbegin(), MRs[i].cend(), static_cast<VkDeviceSize>(0), [](const VkDeviceSize lhs, const VkMemoryRequirements& rhs) { return RoundUp(lhs, rhs.alignment) + rhs.size; }),
				i
			};
			VERIFY_SUCCEEDED(vkAllocateMemory(Device, &MAI, GetAllocationCallbacks(), &DMs[i]));
		}
	}
}

static void CreateShaderModule(VkShaderModule* ShaderModule, const VkDevice Device, const std::string& Path)
{
	std::ifstream In(Path.c_str(), std::ios::in | std::ios::binary);
	if (!In.fail()) {
		In.seekg(0, std::ios_base::end);
		const auto Size = In.tellg();
		In.seekg(0, std::ios_base::beg);
		if (Size) {
			auto Data = new char[Size];
			In.read(Data, Size);
			const VkShaderModuleCreateInfo SMCI = {
				VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				nullptr,
				0,
				static_cast<size_t>(Size), reinterpret_cast<uint32_t*>(Data)
			};
			VERIFY_SUCCEEDED(vkCreateShaderModule(Device, &SMCI, GetAllocationCallbacks(), ShaderModule));
			delete[] Data;
		}
		In.close();
	}
}

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
	//std::vector<VkPhysicalDeviceMemoryProperties> PhysicalDeivceMemoryProperties;
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

		//PhysicalDeivceMemoryProperties.resize(PhysicalDevices.size());
		//for (size_t i = 0; i < PhysicalDevices.size();++i) {
		//	vkGetPhysicalDeviceMemoryProperties(PhysicalDevices[i], &PhysicalDeivceMemoryProperties[i]);
		//}
	}

	//!< Device
	uint32_t GraphicsQueueFamilyIndex = 0xffff;
	uint32_t PresentQueueFamilyIndex = 0xffff;
	VkDevice Device;
	VkQueue GraphicsQueue;
	//VkQueue PresentQueue;
	{
		const auto& PD = PhysicalDevices[0];

		std::vector<VkQueueFamilyProperties> QFPs;
		uint32_t Count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(PD, &Count, nullptr);
		QFPs.resize(Count);
		vkGetPhysicalDeviceQueueFamilyProperties(PD, &Count, QFPs.data());
		for (size_t i = 0; i < QFPs.size();++i) {
			if (VK_QUEUE_GRAPHICS_BIT & QFPs[i].queueFlags) { 
				GraphicsQueueFamilyIndex = i;
			}
			VkBool32 b = VK_FALSE;
			VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceSupportKHR(PD, i, Surface, &b));
			if (b) {
				PresentQueueFamilyIndex = i;
			}
		}
		std::cout << "GraphicsQueueFamilyIndex = " << GraphicsQueueFamilyIndex << std::endl;
		std::cout << "PresentQueueFamilyIndex = " << PresentQueueFamilyIndex << std::endl;
		assert(0xffff != GraphicsQueueFamilyIndex && "");
		//assert(0xffff != PresentQueueFamilyIndex && "");

		std::vector<std::vector<float>> Priorites;
		Priorites.resize(QFPs.size());
		const uint32_t GraphicsQueueIndexInFamily = static_cast<uint32_t>(Priorites[GraphicsQueueFamilyIndex].size()); Priorites[GraphicsQueueFamilyIndex].push_back(0.5f);
		//const uint32_t PresentQueueIndexInFamily = static_cast<uint32_t>(Priorites[PresentQueueFamilyIndex].size()); Priorites[PresentQueueFamilyIndex].push_back(0.5f);
		std::cout << "\tGraphicsQueueIndexInFamily = " << GraphicsQueueIndexInFamily << std::endl;
		//std::cout << "\tPresentQueueIndexInFamily = " << PresentQueueIndexInFamily << std::endl;

		std::vector<VkDeviceQueueCreateInfo> DQCIs;
		DQCIs.reserve(Priorites.size());
		for (size_t i = 0; i < Priorites.size(); ++i) {
			if (!Priorites[i].empty()) {
				DQCIs.push_back(
					{
						VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
						nullptr,
						0,
						static_cast<uint32_t>(i),
						static_cast<uint32_t>(Priorites[i].size()), Priorites[i].data()
					}
				);
			}
		}

		const std::array<const char*, 1> Extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		VkPhysicalDeviceFeatures PDF;
		vkGetPhysicalDeviceFeatures(PD, &PDF);
		const VkDeviceCreateInfo DCI = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			nullptr,
			0,
			static_cast<uint32_t>(DQCIs.size()), DQCIs.data(),
			0, nullptr,
			static_cast<uint32_t>(Extensions.size()), Extensions.data(),
			&PDF
		};
		VERIFY_SUCCEEDED(vkCreateDevice(PD, &DCI, GetAllocationCallbacks(), &Device));

		vkGetDeviceQueue(Device, GraphicsQueueFamilyIndex, GraphicsQueueIndexInFamily, &GraphicsQueue);
		//vkGetDeviceQueue(Device, PresentQueueFamilyIndex, PresentQueueIndexInFamily, &PresentQueue);
	}

	//!< Fence
	VkFence Fence;
	{
		const VkFenceCreateInfo FCI = {
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			nullptr,
			VK_FENCE_CREATE_SIGNALED_BIT
		};
		VERIFY_SUCCEEDED(vkCreateFence(Device, &FCI, GetAllocationCallbacks(), &Fence));
	}

	//!< Semaphore
	VkSemaphore NextImageAcquiredSemaphore;
	VkSemaphore RenderFinishedSemaphore;
	{
		const VkSemaphoreCreateInfo SCI = {
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			nullptr,
			0
		};
		VERIFY_SUCCEEDED(vkCreateSemaphore(Device, &SCI, GetAllocationCallbacks(), &NextImageAcquiredSemaphore));
		VERIFY_SUCCEEDED(vkCreateSemaphore(Device, &SCI, GetAllocationCallbacks(), &RenderFinishedSemaphore));
	}

	//!< Swapchain
	VkSwapchainKHR Swapchain;
	std::vector<VkImage> SwapchainImages;
	std::vector<VkImageView> SwapchainImageViews;
	{
		//const auto& PD = PhysicalDevices[0];
		uint32_t Count = 0;
#if 0
		VkSurfaceCapabilitiesKHR SC;
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PD, Surface, &SC));
		const auto ImageCount = (std::min)(SC.minImageCount + 1, 0 == SC.maxImageCount ? 0xffff : SC.maxImageCount);
		std::cout << SC.currentTransform << std::endl;
#endif
#if 0
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceFormatsKHR(PD, Surface, &Count, nullptr));
		std::vector<VkSurfaceFormatKHR> SFs(Count);
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfaceFormatsKHR(PD, Surface, &Count, SFs.data()));
		for (auto& i : SFs) { std::cout << i.format << ", " << i.colorSpace << std::endl; }
		//!< 要素が 1 つのみで UNDEFINED の場合、制限は無く好きなものを選択できる
		if (1 == SFs.size() && VK_FORMAT_UNDEFINED == SFs[0].format) { std::cout << "Any format" << std::endl; }
#endif
#if 0
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfacePresentModesKHR(PD, Surface, &Count, nullptr));
		std::vector<VkPresentModeKHR> PMs(Count);
		VERIFY_SUCCEEDED(vkGetPhysicalDeviceSurfacePresentModesKHR(PD, Surface, &Count, PMs.data()));
#endif
		const VkSwapchainCreateInfoKHR SCI = {
			VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			nullptr,
			0,
			Surface,
			2/*3*/,
			VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
			{ 1280, 720 },
			1,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
			VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_PRESENT_MODE_FIFO_KHR/*VK_PRESENT_MODE_MAILBOX_KHR*/,
			VK_TRUE,
			VK_NULL_HANDLE
		};
		//!< エラーが出力されている
		VERIFY_SUCCEEDED(vkCreateSwapchainKHR(Device, &SCI, GetAllocationCallbacks(), &Swapchain));

		//!< Swapchain image
		VERIFY_SUCCEEDED(vkGetSwapchainImagesKHR(Device, Swapchain, &Count, nullptr));
		SwapchainImages.resize(Count);
		VERIFY_SUCCEEDED(vkGetSwapchainImagesKHR(Device, Swapchain, &Count, SwapchainImages.data()));

		//!< Swapchain image view
		SwapchainImageViews.resize(SwapchainImages.size());
		for (size_t i = 0; i < SwapchainImageViews.size();++i) {
			const VkImageViewCreateInfo IVCI = {	
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				nullptr,
				0,
				SwapchainImages[i],
				VK_IMAGE_VIEW_TYPE_2D,
				SCI.imageFormat,
				{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, },
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			VERIFY_SUCCEEDED(vkCreateImageView(Device, &IVCI, GetAllocationCallbacks(), &SwapchainImageViews[i]));
		}
	}
	
	//!< Command
	VkCommandPool CommandPool;
	std::vector<VkCommandBuffer> CommandBuffers;
	{
		const VkCommandPoolCreateInfo CPCI = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			nullptr,
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			GraphicsQueueFamilyIndex
		};
		VERIFY_SUCCEEDED(vkCreateCommandPool(Device, &CPCI, GetAllocationCallbacks(), &CommandPool));

		CommandBuffers.resize(SwapchainImages.size());
		const VkCommandBufferAllocateInfo CBAI = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			nullptr,
			CommandPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			static_cast<uint32_t>(CommandBuffers.size())
		};
		VERIFY_SUCCEEDED(vkAllocateCommandBuffers(Device, &CBAI, CommandBuffers.data()));
	}

	//!< Vertex data
	using Vertex_PositionColor = struct Vertex_PositionColor { glm::vec3 Position; glm::vec4 Color; };
	const std::array<Vertex_PositionColor, 3> Vertices = { {
		{ { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }, //!< CT
		{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, //!< LB
		{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }, //!< RB
	} };

	//!< Index data
	const std::array<uint32_t, 3> Indices = { 0, 1, 2 };
	const uint32_t IndexCount = static_cast<uint32_t>(Indices.size());
	
	//!< Indirect data
	const VkDrawIndexedIndirectCommand DrawIndexedIndirectCommand = { IndexCount, 1, 0, 0, 0 };

	//!< Buffers
	std::vector<VkBuffer> Buffers;
	{
		Buffers.resize(3);
		{
			const auto Stride = sizeof(Vertices[0]);
			const auto Size = static_cast<VkDeviceSize>(Stride * Vertices.size());
			CreateBuffer(&Buffers[0], Device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, Size);
		}
		{
			const auto Stride = sizeof(Indices[0]);
			const auto Size = static_cast<VkDeviceSize>(Stride * IndexCount);
			CreateBuffer(&Buffers[1], Device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, Size);
		}
		{
			const auto Stride = sizeof(DrawIndexedIndirectCommand);
			const auto Size = static_cast<VkDeviceSize>(Stride * 1);
			CreateBuffer(&Buffers[2], Device, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, Size);
		}
	}
	//!< Device memory
	std::vector<std::vector<VkMemoryRequirements>> MemoryRequirements;
	std::vector<VkDeviceMemory> DeviceMemories;
	{
		const auto& PD = PhysicalDevices[0];
		VkPhysicalDeviceMemoryProperties PDMP;
		vkGetPhysicalDeviceMemoryProperties(PD, &PDMP);
		MemoryRequirements.resize(PDMP.memoryTypeCount);
		for (auto i : Buffers) {
			VkMemoryRequirements MR;
			vkGetBufferMemoryRequirements(Device, i, &MR);
			MemoryRequirements[GetMemoryTypeIndex(PD, MR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)].push_back(MR);
		}
		CreateDeviceMemory(DeviceMemories, Device, MemoryRequirements);
	}

	//!< TODO
	//!< Staging copy (create host visible memory, map and write to host visible memory, submit copy command to device local memory
	if(0){
		std::vector<VkBuffer> HostVisibleBuffers;
		{
			HostVisibleBuffers.resize(3);
			{
				const auto Stride = sizeof(Vertices[0]);
				const auto Size = static_cast<VkDeviceSize>(Stride * Vertices.size());
				CreateBuffer(&HostVisibleBuffers[0], Device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, Size);
			}
			{
				const auto Stride = sizeof(Indices[0]);
				const auto Size = static_cast<VkDeviceSize>(Stride * IndexCount);
				CreateBuffer(&HostVisibleBuffers[1], Device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, Size);
			}
			{
				const auto Stride = sizeof(DrawIndexedIndirectCommand);
				const auto Size = static_cast<VkDeviceSize>(Stride * 1);
				CreateBuffer(&HostVisibleBuffers[2], Device, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, Size);
			}
		}
		std::vector<VkDeviceMemory> HostVisibleDeviceMemories;
		std::vector<std::vector<VkMemoryRequirements>> HostVisibleMemoryRequirements;
		{
			const auto& PD = PhysicalDevices[0];
			VkPhysicalDeviceMemoryProperties PDMP;
			vkGetPhysicalDeviceMemoryProperties(PD, &PDMP);
			HostVisibleMemoryRequirements.resize(PDMP.memoryTypeCount);
			for (auto i : Buffers) {
				VkMemoryRequirements MR;
				vkGetBufferMemoryRequirements(Device, i, &MR);
				HostVisibleMemoryRequirements[GetMemoryTypeIndex(PD, MR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)].push_back(MR);
			}
			CreateDeviceMemory(HostVisibleDeviceMemories, Device, HostVisibleMemoryRequirements);
		}
		
		//void* Data;
		//VERIFY_SUCCEEDED(vkMapMemory(Device, DM, 0, Size, static_cast<VkMemoryMapFlags>(0), &Data)); {
		//	memcpy(Data, Source, Size);
		//	const std::array<VkMappedMemoryRange, 1> MMRs = {
		//		{
		//			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		//			nullptr,
		//			DM,
		//			0,
		//			VK_WHOLE_SIZE
		//		}
		//	};
		//	VERIFY_SUCCEEDED(vkFlushMappedMemoryRanges(Device, static_cast<uint32_t>(MMRs.size()), MMRs.data()));
		//} vkUnmapMemory(Device, DM);

		for (auto i : HostVisibleDeviceMemories) {
			if (VK_NULL_HANDLE != i) {
				vkFreeMemory(Device, i, GetAllocationCallbacks());
			}
		}
		for (auto i : HostVisibleBuffers) {
			vkDestroyBuffer(Device, i, GetAllocationCallbacks());
		}
	}

	//!< Bind buffers
	{
		const auto& PD = PhysicalDevices[0];
		std::vector<uint32_t> IndexInMemoryType; IndexInMemoryType.assign(MemoryRequirements.size(), 0);
		VkDeviceSize MemoryOffset = 0;
		for (auto i : Buffers) {
			VkMemoryRequirements MR;
			vkGetBufferMemoryRequirements(Device, i, &MR);
			const auto MemoryTypeIndex = GetMemoryTypeIndex(PD, MR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			std::cout << "Bind : TypeIndex = " << MemoryTypeIndex << ", Offset = " << MemoryOffset << ", IndexInMemotryType = " << IndexInMemoryType[MemoryTypeIndex] << std::endl;
			VERIFY_SUCCEEDED(vkBindBufferMemory(Device, i, DeviceMemories[MemoryTypeIndex], MemoryOffset)); MemoryOffset += MemoryRequirements[MemoryTypeIndex][IndexInMemoryType[MemoryTypeIndex]].size; ++IndexInMemoryType[MemoryTypeIndex];
		}
	}

	//!< Pipeline layout
	VkPipelineLayout PipelineLayout;
	{
		const std::array<VkDescriptorSetLayout, 0> DSLs = {};
		const std::array<VkPushConstantRange, 0> PCRs = {};
		const VkPipelineLayoutCreateInfo PLCI = {
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			nullptr,
			0,
			static_cast<uint32_t>(DSLs.size()), DSLs.data(),
			static_cast<uint32_t>(PCRs.size()), PCRs.data()
		};
		VERIFY_SUCCEEDED(vkCreatePipelineLayout(Device, &PLCI, GetAllocationCallbacks(), &PipelineLayout));
	}

	//!< Render pass
	VkRenderPass RenderPass;
	{
		const std::array<VkAttachmentDescription, 1> ADs = {
			0,
			VK_FORMAT_B8G8R8A8_UNORM,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};
		const std::array<VkAttachmentReference, 0> InputARs = {};
		const std::array<VkAttachmentReference, 1> ColorARs = { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }, };
		const std::array<VkAttachmentReference, 0> ResolveARs = {};
		const std::array<uint32_t, 0> Preserve = {};
		const std::array<VkSubpassDescription, 1> SDs = {
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			static_cast<uint32_t>(InputARs.size()), InputARs.data(),
			static_cast<uint32_t>(ColorARs.size()), ColorARs.data(), ResolveARs.data(),
			nullptr,
			static_cast<uint32_t>(Preserve.size()), Preserve.data()
		};
		const std::array<VkSubpassDependency, 0> SDeps = {};
		const VkRenderPassCreateInfo RPCI = {
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			nullptr,
			0,
			static_cast<uint32_t>(ADs.size()), ADs.data(),
			static_cast<uint32_t>(SDs.size()), SDs.data(),
			static_cast<uint32_t>(SDeps.size()), SDeps.data()
		};
		VERIFY_SUCCEEDED(vkCreateRenderPass(Device, &RPCI, GetAllocationCallbacks(), &RenderPass));
	}

	//!< Shader moudles
	std::vector<VkShaderModule> ShaderModules;
	{
		ShaderModules.resize(2);
		CreateShaderModule(&ShaderModules[0], Device, "VS.spv");
		CreateShaderModule(&ShaderModules[1], Device, "FS.spv");
	}

	//!< Pipeline
	VkPipeline Pipeline;
	{
		const std::array<VkPipelineShaderStageCreateInfo, 2> PSSCIs = {
			VkPipelineShaderStageCreateInfo({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, ShaderModules[0], "main", nullptr }),
			VkPipelineShaderStageCreateInfo({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, ShaderModules[1], "main", nullptr }),
		};

		const uint32_t Binding = 0;
		const std::array<VkVertexInputBindingDescription, 1> VIBDs = { {
			{ Binding, sizeof(Vertex_PositionColor), VK_VERTEX_INPUT_RATE_VERTEX },
		} };
		const std::array<VkVertexInputAttributeDescription, 2> VIADs = { {
			{ 0, Binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_PositionColor, Position) },
			{ 1, Binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex_PositionColor, Color) },
		} };
		const VkPipelineVertexInputStateCreateInfo PVISCI = {
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			nullptr,
			0,
			static_cast<uint32_t>(VIBDs.size()), VIBDs.data(),
			static_cast<uint32_t>(VIADs.size()), VIADs.data()
		};

		const VkPipelineInputAssemblyStateCreateInfo PIASCI = {
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			VK_FALSE
		};

		const VkPipelineTessellationStateCreateInfo PTSCI = {
			VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
			nullptr,
			0,
			0
		};

		const VkPipelineViewportStateCreateInfo PVSCI = {
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			nullptr,
			0,
			1, nullptr,
			1, nullptr
		};

		const VkPipelineRasterizationStateCreateInfo PRSCI = {
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE,
			VK_FALSE,
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE, 0.0f, 0.0f, 0.0f,
			1.0f
		};

		const VkPipelineMultisampleStateCreateInfo PMSCI = {
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FALSE, 0.0f,
			nullptr,
			VK_FALSE, VK_FALSE
		};

		const VkPipelineDepthStencilStateCreateInfo PDSSCI = {
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL,
			VK_FALSE,
			VK_FALSE, { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0, 0, 0 }, { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
			0.0f, 1.0f
		};

		const std::array<VkPipelineColorBlendAttachmentState, 1> PCBASs = {
			{
				VK_FALSE,
				VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
				VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
				VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			},
		};
		const VkPipelineColorBlendStateCreateInfo PCBSCI = {
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_FALSE, VK_LOGIC_OP_COPY,
			static_cast<uint32_t>(PCBASs.size()), PCBASs.data(),
			{ 1.0f, 1.0f, 1.0f, 1.0f }
		};

		const std::array<VkDynamicState, 2> DSs = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};
		const VkPipelineDynamicStateCreateInfo PDSCI = {
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			nullptr,
			0,
			static_cast<uint32_t>(DSs.size()), DSs.data()
		};

		const std::array<VkGraphicsPipelineCreateInfo, 1> GPCIs = {
			{
				VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				nullptr,
				0,
				static_cast<uint32_t>(PSSCIs.size()), PSSCIs.data(),
				&PVISCI,
				&PIASCI,
				&PTSCI,
				&PVSCI,
				&PRSCI,
				&PMSCI,
				&PDSSCI,
				&PCBSCI,
				&PDSCI,
				PipelineLayout,
				RenderPass, 0, 
				VK_NULL_HANDLE, -1
			}
		};
		VERIFY_SUCCEEDED(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, static_cast<uint32_t>(GPCIs.size()), GPCIs.data(), GetAllocationCallbacks(), &Pipeline));
	}

	//!< Framebuffer
	std::vector<VkFramebuffer> Framebuffers(SwapchainImageViews.size());
	{
		for (size_t i = 0; i < SwapchainImageViews.size(); ++i) {
			Framebuffers.push_back(VkFramebuffer());
			const std::array<VkImageView, 1> IVs = { SwapchainImageViews[i] };
			const VkFramebufferCreateInfo FCI = {
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				nullptr,
				0,
				RenderPass,
				static_cast<uint32_t>(IVs.size()), IVs.data(),
				1280, 720,
				1
			};
			VERIFY_SUCCEEDED(vkCreateFramebuffer(Device, &FCI, GetAllocationCallbacks(), &Framebuffers[i]));
		}
	}

	//!< Populate command
	{
		for (size_t i = 0; i < CommandBuffers.size(); ++i) {
			const auto CB = CommandBuffers[i];
			const VkCommandBufferBeginInfo CBBI = {
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				nullptr,
				0,
				nullptr
			};
			VERIFY_SUCCEEDED(vkBeginCommandBuffer(CB, &CBBI)); {
				const std::array<VkClearValue, 1> CVs = { { 0.529411793f, 0.807843208f, 0.921568692f, 1.0f } };
				const VkRect2D RenderArea = { { 0, 0 }, { 1280, 720 } };
				const VkRenderPassBeginInfo RPBI = {
					VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					nullptr,
					RenderPass,
					Framebuffers[i],
					RenderArea,
					static_cast<uint32_t>(CVs.size()), CVs.data()
				};
				vkCmdBeginRenderPass(CB, &RPBI, VK_SUBPASS_CONTENTS_INLINE); {
					const std::array< VkViewport, 1> Viewports = { { 0.0f, 720.0f, 1280.0f, -720.0f, 0.0f, 1.0f } };
					const std::array<VkRect2D, 1> ScissorRects = { {{{ 0, 0 }, { 1280, 720 }}} };
					vkCmdSetViewport(CB, 0, static_cast<uint32_t>(Viewports.size()), Viewports.data());
					vkCmdSetScissor(CB, 0, static_cast<uint32_t>(ScissorRects.size()), ScissorRects.data());

					vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);

					const std::array<VkBuffer, 1> VBs = { Buffers[0] };
					const std::array<VkDeviceSize, 1> Offsets = { 0 };
					vkCmdBindVertexBuffers(CB, 0, static_cast<uint32_t>(VBs.size()), VBs.data(), Offsets.data());
					const auto IB = Buffers[1];
					vkCmdBindIndexBuffer(CB, IB, 0, VK_INDEX_TYPE_UINT32);
					const auto IDB = Buffers[2];
					vkCmdDrawIndexedIndirect(CB, IDB, 0, 1, 0);
				} vkCmdEndRenderPass(CB);
			} VERIFY_SUCCEEDED(vkEndCommandBuffer(CB));
		}
	}

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
		for (auto i : Framebuffers) {
			vkDestroyFramebuffer(Device, i, GetAllocationCallbacks());
		}
		vkDestroyPipeline(Device, Pipeline, GetAllocationCallbacks());
		for (auto i : ShaderModules) {
			vkDestroyShaderModule(Device, i, GetAllocationCallbacks());
		}
		vkDestroyRenderPass(Device, RenderPass, GetAllocationCallbacks());
		vkDestroyPipelineLayout(Device, PipelineLayout, GetAllocationCallbacks());
		for (auto i : DeviceMemories) {
			if (VK_NULL_HANDLE != i) {
				vkFreeMemory(Device, i, GetAllocationCallbacks());
			}
		}
		for (auto i : Buffers) {
			vkDestroyBuffer(Device, i, GetAllocationCallbacks());
		}
		vkFreeCommandBuffers(Device, CommandPool, static_cast<uint32_t>(CommandBuffers.size()), CommandBuffers.data());
		vkDestroyCommandPool(Device, CommandPool, GetAllocationCallbacks());
		vkDestroySwapchainKHR(Device, Swapchain, GetAllocationCallbacks());
		vkDestroySemaphore(Device, NextImageAcquiredSemaphore, GetAllocationCallbacks());
		vkDestroySemaphore(Device, RenderFinishedSemaphore, GetAllocationCallbacks());
		vkDestroyFence(Device, Fence, GetAllocationCallbacks());
		vkDestroyDevice(Device, GetAllocationCallbacks());
		vkDestroySurfaceKHR(Instance, Surface, GetAllocationCallbacks());
#if 0
		vkDestroyDebugReportCallback(Instance, DebugReportCallback, GetAllocationCallbacks());
#endif
		vkDestroyInstance(Instance, GetAllocationCallbacks());

		xcb_disconnect(Connection);
	}

	return 0;
}
