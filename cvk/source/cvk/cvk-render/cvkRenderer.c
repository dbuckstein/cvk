/*
   Copyright 2020-2021 Daniel S. Buckstein

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/

/*
	cvk: Vulkan renderer from scratch in C.
	By Daniel S. Buckstein

	cvkRenderer.c
	Main renderer source.
*/

#include "cvk/cvk-render/cvkRenderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else	// !_WIN32
#endif	// _WIN32
#include "vulkan/vulkan.h"


//-----------------------------------------------------------------------------

// cvk_arrlen
//	Get number of elements in a sized array.
#define cvk_arrlen(arr) (sizeof(arr) / sizeof(*arr))

// cvk_arrlen_p
//	Get number of elements in an array of pointers (excluding null at end).
inline int cvk_arrlen_p(kptr* arr)
{
	int n = 0;
	while (*arr)
	{
		++arr;
		++n;
	}
	return n;
}

// cvk_arrlen_pl
//	Get number of elements in an array of pointers, up to a maximum length.
inline int cvk_arrlen_pl(kptr const* arr, ui32 const len)
{
	int n = 0;
	kptr const* const end = arr + len;
	while ((arr < end) && *arr)
	{
		++arr;
		++n;
	}
	return n;
}

// cvk_strfind_p
//	Find a string in an array of strings, returning index (negative if fail).
inline int cvk_strfind_p(kstr const key, kstr const* arr)
{
	int n = 0;
	while (*arr)
	{
		if (!strcmp(key, *arr))
			return n;
		++arr;
		++n;
	}
	return -1;
}

// cvk_strfind_pl
//	Find a string in an array of strings, returning index (negative if fail).
inline int cvk_strfind_pl(kstr const key, kstr const* arr, ui32 const len)
{
	int n = 0;
	kstr const* const end = arr + len;
	while ((arr < end) && *arr)
	{
		if (!strcmp(key, *arr))
			return n;
		++arr;
		++n;
	}
	return -1;
}

// cvk_strfind_store
//	Find a string in provided menus, storing in final list if found; returns 
//	non-negative if found.
inline int cvk_strfind_store(kstr const key, ui32* const count_inout, kstr* arr_out, kstr const* arr_request, kstr const* arr_require, ui32 const count_request, ui32 const count_require)
{
	ui32 count = *count_inout;
	i32 n = cvk_strfind_pl(key, arr_out, count);
	if (n < 0)
	{
		// search for name in requested list, add to final if found
		n = cvk_strfind_pl(key, arr_request, count_request);
		if (n >= 0)
			arr_out[count++] = arr_request[n];

		// search required list
		else
			n = cvk_strfind_pl(key, arr_require, count_require);

		// reassign count
		*count_inout = count;
	}
	return n;
}


//-----------------------------------------------------------------------------

// cvkRendererData_vk
//	Internal data/handle indices for renderer instance.
enum cvkRendererData_vk
{
	cvkRendererData_instance,		// instance
	cvkRendererData_logicalDevice,	// logical device

	cvkRendererData_count			// maximum number of renderer handles
};

// cvkRendererData
//	Internal data not involved with renderer.
enum cvkRendererData
{
	cvkRendererData_allocation = cvkRendererData_count,	// global memory management
	cvkRendererData_logicalDevice_queuePriority,		// priority flags for logical device
};


//-----------------------------------------------------------------------------

inline int cvkRendererInternalPrintLayer(VkLayerProperties const* const layerProp, ui32 const index, kstr const prefix)
{
	return printf("%s layerProp[%u] = { \"%s\" (%u.%u.%u; %u.%u.%u): \"%s\" } \n", prefix, index,
		layerProp->layerName,
		VK_VERSION_MAJOR(layerProp->specVersion), VK_VERSION_MINOR(layerProp->specVersion), VK_VERSION_PATCH(layerProp->specVersion),
		VK_VERSION_MAJOR(layerProp->implementationVersion), VK_VERSION_MINOR(layerProp->implementationVersion), VK_VERSION_PATCH(layerProp->implementationVersion),
		layerProp->description);
}

inline int cvkRendererInternalPrintExtension(VkExtensionProperties const* const extensionProp, ui32 const index, kstr const prefix)
{
	return printf("%s extensionProp[%u] = { \"%s\" (%u.%u.%u) } \n", prefix, index,
		extensionProp->extensionName,
		VK_VERSION_MAJOR(extensionProp->specVersion), VK_VERSION_MINOR(extensionProp->specVersion), VK_VERSION_PATCH(extensionProp->specVersion));
}

inline int cvkRendererInternalSelectDeviceID(ui32 const id)
{
	// https://www.reddit.com/r/vulkan/comments/4ta9nj/is_there_a_comprehensive_list_of_the_names_and/ 
	// 0x1002 - AMD
	// 0x1010 - ImgTec
	// 0x10DE - NVIDIA
	// 0x13B5 - ARM
	// 0x5143 - Qualcomm
	// 0x8086 - INTEL

	switch (id)
	{
	case 0x1002: return 1;
	case 0x1010: return 2;
	case 0x10DE: return 3;
	case 0x13B5: return 4;
	case 0x5143: return 5;
	case 0x8086: return 6;

	case 0x10000: return 8;
	case VK_VENDOR_ID_VIV: return 9;
	case VK_VENDOR_ID_VSI: return 10;
	case VK_VENDOR_ID_KAZAN: return 11;
	case VK_VENDOR_ID_CODEPLAY: return 12;
	case VK_VENDOR_ID_MESA: return 13;
	case 0x10006: return 14;
	}
	return 0;
}

inline int cvkRendererInternalPrintPhysicalDevice(VkPhysicalDeviceProperties const* const physicalDeviceProp, ui32 const index, kstr const prefix)
{
	kstr const deviceType[] = { "other", "integrated gpu", "discrete gpu", "virtual gpu", "cpu" };
	kstr const deviceID[] = { 
		"other", "amd", "imgtec", "nvidia", "arm", "qualcomm", "intel", 0,
		"khr", "viv", "vsi", "kazan", "codeplay", "mesa", "pocl", 0,
	};
	ui32 const vID = cvkRendererInternalSelectDeviceID(physicalDeviceProp->vendorID);
	ui32 const dID = cvkRendererInternalSelectDeviceID(physicalDeviceProp->deviceID);

	return printf("%s physicalDeviceProp[%u] = { \"%s\" [%s] (%u.%u.%u; %u.%u.%u; %s; %s) } \n", prefix, index,
		physicalDeviceProp->deviceName, deviceType[physicalDeviceProp->deviceType],
		VK_VERSION_MAJOR(physicalDeviceProp->apiVersion), VK_VERSION_MINOR(physicalDeviceProp->apiVersion), VK_VERSION_PATCH(physicalDeviceProp->apiVersion),
		VK_VERSION_MAJOR(physicalDeviceProp->driverVersion), VK_VERSION_MINOR(physicalDeviceProp->driverVersion), VK_VERSION_PATCH(physicalDeviceProp->driverVersion),
		deviceID[vID], deviceID[dID]);
}

inline int cvkRendererInternalPrintQueueFamily(VkQueueFamilyProperties const* const queueFamilyProp, ui32 const index, kstr const prefix)
{
	kstr const queueFlag[] = { "[graphics]", "[compute]", "[transfer]", "[sparsebind]", "[protected]" };
	return printf("%s queueFamilyProp[%u] = { [%s%s%s%s%s] (%u; %u; %u,%u,%u) } \n", prefix, index,
		(queueFamilyProp->queueFlags & VK_QUEUE_GRAPHICS_BIT) ? queueFlag[0] : "",
		(queueFamilyProp->queueFlags & VK_QUEUE_COMPUTE_BIT) ? queueFlag[1] : "",
		(queueFamilyProp->queueFlags & VK_QUEUE_TRANSFER_BIT) ? queueFlag[2] : "",
		(queueFamilyProp->queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) ? queueFlag[3] : "",
		(queueFamilyProp->queueFlags & VK_QUEUE_PROTECTED_BIT) ? queueFlag[4] : "",
		queueFamilyProp->queueCount, queueFamilyProp->timestampValidBits,
		queueFamilyProp->minImageTransferGranularity.width, queueFamilyProp->minImageTransferGranularity.height, queueFamilyProp->minImageTransferGranularity.depth);
}

inline int cvkRendererInternalPrintMemoryType(VkMemoryType const* const memoryType, ui32 const index, kstr const prefix)
{
	kstr const memoryTypeFlag[] = { "[device local]", "[host visible]", "[host coherent]", "[host cached]", "[lazy alloc]", "[protected]", "[device coherent AMD]", "[device uncached AMD]" };
	return printf("%s memoryType[%u] = { [%s%s%s%s%s%s%s%s] (%u) } \n", prefix, index,
		(memoryType->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? memoryTypeFlag[0] : "",
		(memoryType->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? memoryTypeFlag[1] : "",
		(memoryType->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? memoryTypeFlag[2] : "",
		(memoryType->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? memoryTypeFlag[3] : "",
		(memoryType->propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) ? memoryTypeFlag[4] : "",
		(memoryType->propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) ? memoryTypeFlag[5] : "",
		(memoryType->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) ? memoryTypeFlag[6] : "",
		(memoryType->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD) ? memoryTypeFlag[7] : "",
		memoryType->heapIndex);
}

inline int cvkRendererInternalPrintMemoryHeap(VkMemoryHeap const* const memoryHeap, ui32 const index, kstr const prefix)
{
	kstr const memoryHeapFlag[] = { "[device local]", "[multi-instance]", "[multi-instance KHR]" };
	return printf("%s memoryHeap[%u] = { [%s%s%s] (%llu) } \n", prefix, index,
		(memoryHeap->flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? memoryHeapFlag[0] : "",
		(memoryHeap->flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) ? memoryHeapFlag[1] : "",
		(memoryHeap->flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT_KHR) ? memoryHeapFlag[2] : "",
		memoryHeap->size);
}


//-----------------------------------------------------------------------------

int cvkRendererCreate(cvkRenderer* const renderer)
{
	if (renderer && !renderer->init)
	{
		int result = -2;
		int n = -1;
		ui32 i = 0, j = 0, k = 0;
		kstr name = NULL;
		kstr const pf1 = "\t  ", pf1s = "\t->", pf2 = "\t\t  ", pf2s = "\t\t->", pf3 = "\t\t\t  ", pf3s = "\t\t\t->";


		//---------------------------------------------------------------------
		// persistent data

		// instance
		VkInstance inst = NULL;

		// logical device
		VkDevice logicalDevice = NULL;

		// global memory management
		VkAllocationCallbacks* alloc = NULL;

		// logical device queue priority
		f32* logicalDeviceQueuePriority = NULL;


		//---------------------------------------------------------------------
		// instance data

		// data for instance, layers and extensions
		ui32 instVersion = 0, nLayer = 0, nExtension = 0;
		// array of layer info
		VkLayerProperties* layerProp = NULL;
		// array of extension info for each layer
		VkExtensionProperties* extensionProp = NULL;

		// layers to be searched and enabled for instance
		kstr const layerInfo_inst[] = {
#ifdef _DEBUG
			"VK_LAYER_KHRONOS_validation",
			"VK_LAYER_LUNARG_api_dump",
			"VK_LAYER_LUNARG_standard_validation",
			//"VK_LAYER_LUNARG_monitor",
			//"VK_LAYER_LUNARG_object_tracker",
#else	// !_DEBUG
			NULL
#endif	// _DEBUG
		};
		// extensions to be searched and enabled for instance
		kstr const extInfo_inst[] = {
#ifdef _DEBUG
			"VK_EXT_debug_report",
			"VK_EXT_debug_utils",
			"VK_EXT_validation_features",
#else	// !_DEBUG
			NULL
#endif	// _DEBUG
		};
		// required layers
		kstr const layerInfo_inst_req[] = {
			NULL
		};
		// required extensions
		kstr const extInfo_inst_req[] = {
			VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else	// !_WIN32
#endif	// _WIN32
		};
		// number of layers in original array
		ui32 const layerInfoLen_inst = cvk_arrlen(layerInfo_inst);
		// number of required layers in original array
		ui32 const layerInfoLen_inst_req = cvk_arrlen(layerInfo_inst_req);
		// number of extensions in original array
		ui32 const extInfoLen_inst = cvk_arrlen(extInfo_inst);
		// number of required extensions in original array
		ui32 const extInfoLen_inst_req = cvk_arrlen(extInfo_inst_req);
		// final layers to be used with instance
		kstr layerInfoFinal_inst[cvk_arrlen(layerInfo_inst) + cvk_arrlen(layerInfo_inst_req)] = { 0 };
		// final extensions to be used with instance
		kstr extInfoFinal_inst[cvk_arrlen(extInfo_inst) + cvk_arrlen(extInfo_inst_req)] = { 0 };
		// maximum final layers
		ui32 const layerInfoLenFinal_inst = cvk_arrlen(layerInfoFinal_inst);
		// maximum final extensions
		ui32 const extInfoLenFinal_inst = cvk_arrlen(extInfoFinal_inst);

		// application info for instance
		VkApplicationInfo const appInfo = {
			VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
			"cvkTest", VK_MAKE_VERSION(0, 0, 1), "cvk", VK_MAKE_VERSION(0, 0, 1), VK_API_VERSION_1_0,
		};

		// instance initialization info
		VkInstanceCreateInfo instInfo = {
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &appInfo,
			cvk_arrlen_pl(layerInfoFinal_inst, layerInfoLenFinal_inst), layerInfoFinal_inst,
			cvk_arrlen_pl(extInfoFinal_inst, extInfoLenFinal_inst), extInfoFinal_inst,
		};


		//---------------------------------------------------------------------
		// device data

		// info for physical devices
		ui32 nPhysicalDevice = 0, nQueueFamily = 0;
		// physical device used to create logical device
		VkPhysicalDevice physicalDevice = NULL;
		// array of physical device descriptors
		VkPhysicalDevice* physicalDeviceList = NULL;
		// properties for each physical device
		VkPhysicalDeviceProperties physicalDeviceProp = { 0 };
		// device features (available and required for instance)
		VkPhysicalDeviceFeatures physicalDeviceFeat = { 0 }, physicalDeviceFeatReq = { 0 };
		// device memory properties
		VkPhysicalDeviceMemoryProperties physicalDeviceMemProp = { 0 };
		// device queue family properties
		VkQueueFamilyProperties* queueFamilyProp = NULL;

		// layers to be searched and enabled for device
		kstr const layerInfo_device[] = {
#ifdef _DEBUG
			"VK_LAYER_KHRONOS_validation",
			"VK_LAYER_LUNARG_api_dump",
			"VK_LAYER_LUNARG_standard_validation",
#else	// !_DEBUG
			NULL
#endif	// _DEBUG
		};
		// extensions to be searched and enabled for device
		kstr const extInfo_device[] = {
#ifdef _DEBUG
			"VK_EXT_validation_cache",
			"VK_EXT_debug_marker",
			"VK_EXT_tooling_info",
#else	// !_DEBUG
			NULL
#endif	// _DEBUG
		};
		// required layers
		kstr const layerInfo_device_req[] = {
			NULL
		};
		// required extensions
		kstr const extInfo_device_req[] = {
			NULL
		};
		// number of layers in original array
		ui32 const layerInfoLen_device = cvk_arrlen(layerInfo_device);
		// number of required layers in original array
		ui32 const layerInfoLen_device_req = cvk_arrlen(layerInfo_device_req);
		// number of extensions in original array
		ui32 const extInfoLen_device = cvk_arrlen(extInfo_device);
		// number of required extensions in original array
		ui32 const extInfoLen_device_req = cvk_arrlen(extInfo_device_req);
		// final layers to be used with device
		kstr layerInfoFinal_device[cvk_arrlen(layerInfo_device) + cvk_arrlen(layerInfo_device_req)] = { 0 };
		// final extensions to be used with device
		kstr extInfoFinal_device[cvk_arrlen(extInfo_device) + cvk_arrlen(extInfo_device_req)] = { 0 };
		// maximum final layers
		ui32 const layerInfoLenFinal_device = cvk_arrlen(layerInfoFinal_device);
		// maximum final extensions
		ui32 const extInfoLenFinal_device = cvk_arrlen(extInfoFinal_device);

		// physical device to use for graphics
		i32 physicalDeviceIndex = -1;
		// queue family index to use for graphics and presentation
		i32 queueFamilyIndex = -1;
		// queue count in selected family
		ui32 queueCount = 0;

		// queue creation info for logical device
		VkDeviceQueueCreateInfo queueInfo = {
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0,
			queueFamilyIndex, queueCount, NULL,
		};

		// logical device creation info
		VkDeviceCreateInfo logicalDeviceInfo = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0,
			1, &queueInfo,
			cvk_arrlen_pl(layerInfoFinal_device, layerInfoLenFinal_device), layerInfoFinal_device,
			cvk_arrlen_pl(extInfoFinal_device, extInfoLenFinal_device), extInfoFinal_device,
			&physicalDeviceFeatReq,
		};


		//---------------------------------------------------------------------
		// initialize

		// begin setup
		printf("cvkRendererCreate \n");

		// allocator setup
		//alloc = (VkAllocationCallbacks*)malloc(sizeof(VkAllocationCallbacks));
		//alloc->pUserData = malloc(0);


		//---------------------------------------------------------------------

		// instance setup
		printf(" Vulkan instance... \n");

		// enumerate instance version
		vkEnumerateInstanceVersion(&instVersion);
		printf("\t instVer = %u.%u.%u \n",
			VK_VERSION_MAJOR(instVersion), VK_VERSION_MINOR(instVersion), VK_VERSION_PATCH(instVersion));

		// enumerate instance layers
		vkEnumerateInstanceLayerProperties(&nLayer, NULL);
		if (nLayer)
		{
			// iterate through layers and enumerate extensions
			layerProp = (VkLayerProperties*)malloc(nLayer * sizeof(VkLayerProperties));
			vkEnumerateInstanceLayerProperties(&nLayer, layerProp);
			printf("\t nLayer = %u: { \"layerName\" (specVer; implVer) \"description\" } \n", nLayer);
			for (i = 0; i < nLayer; ++i)
			{
				// search layer in final list, requested list and required list
				name = layerProp[i].layerName;
				n = cvk_strfind_store(name, &instInfo.enabledLayerCount, layerInfoFinal_inst,
					layerInfo_inst, layerInfo_inst_req, layerInfoLen_inst, layerInfoLen_inst_req);

				// print layer info, indicating whether it is requested
				cvkRendererInternalPrintLayer(layerProp + i, i, (n >= 0 ? pf1s : pf1));

				// enumerate extensions for each layer
				vkEnumerateInstanceExtensionProperties(name, &nExtension, NULL);
				if (nExtension)
				{
					// iterate through extensions
					extensionProp = (VkExtensionProperties*)malloc(nExtension * sizeof(VkExtensionProperties));
					vkEnumerateInstanceExtensionProperties(name, &nExtension, extensionProp);
					printf("\t\t nExtension = %u: { \"extensionName\" (specVer) } \n", nExtension);
					for (j = 0; j < nExtension; ++j)
					{
						// search extension
						name = extensionProp[j].extensionName;
						n = cvk_strfind_store(name, &instInfo.enabledExtensionCount, extInfoFinal_inst,
							extInfo_inst, extInfo_inst_req, extInfoLen_inst, extInfoLen_inst_req);

						// print extension info, indicating whether it is requested
						cvkRendererInternalPrintExtension(extensionProp + j, j, (n >= 0 ? pf2s : pf2));
					}

					// release extension list
					free(extensionProp);
					extensionProp = NULL;
				}
			}

			// release layer list
			free(layerProp);
			layerProp = NULL;
		}

		// add required layers and extensions
		for (i = 0; i < layerInfoLen_inst_req; ++i)
			if (layerInfo_inst_req[i] && layerInfo_inst_req[i][0])
				layerInfoFinal_inst[instInfo.enabledLayerCount++] = layerInfo_inst_req[i];
		for (i = 0; i < extInfoLen_inst_req; ++i)
			if (extInfo_inst_req[i] && extInfo_inst_req[i][0])
				extInfoFinal_inst[instInfo.enabledExtensionCount++] = extInfo_inst_req[i];

		// confirm counts are equal
		n = (instInfo.enabledLayerCount == cvk_arrlen_pl(layerInfoFinal_inst, layerInfoLenFinal_inst));
		assert(n);
		n = (instInfo.enabledExtensionCount == cvk_arrlen_pl(extInfoFinal_inst, extInfoLenFinal_inst));
		assert(n);

		// create instance
		vkCreateInstance(&instInfo, alloc, &inst);
		if (inst)
		{
			printf(" Vulkan instance created. \n");

			// set up logical device
			printf(" Vulkan logical device... \n");

			// set up devices
			vkEnumeratePhysicalDevices(inst, &nPhysicalDevice, NULL);
			if (nPhysicalDevice)
			{
				// alloc device info
				physicalDeviceList = (VkPhysicalDevice*)malloc(nLayer * sizeof(VkPhysicalDevice));
				vkEnumeratePhysicalDevices(inst, &nPhysicalDevice, physicalDeviceList);
				printf("\t nPhysicalDevice = %u: { \"name\" [type] (apiVer; driverVer; vendorID; deviceID) } \n", nPhysicalDevice);
				for (i = 0; i < nPhysicalDevice; ++i)
				{
					// query device properties
					vkGetPhysicalDeviceProperties(physicalDeviceList[i], &physicalDeviceProp);

					// save index of capable device
					n = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
					if (physicalDeviceProp.deviceType == n)
						cvkRendererInternalPrintPhysicalDevice(&physicalDeviceProp, (physicalDeviceIndex = i), pf1s);
					else
						cvkRendererInternalPrintPhysicalDevice(&physicalDeviceProp, i, pf1);
				}

				// select physical device and properties
				if (physicalDeviceIndex >= 0)
				{
					physicalDevice = physicalDeviceList[physicalDeviceIndex];
					vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProp);
				}

				// release device list
				free(physicalDeviceList);
				physicalDeviceList = NULL;
			}

			// set up logical device from selected physical device
			if (physicalDevice)
			{
				// enumerate device layers
				vkEnumerateDeviceLayerProperties(physicalDevice, &nLayer, NULL);
				if (nLayer)
				{
					// iterate through layers and enumerate extensions
					layerProp = (VkLayerProperties*)malloc(nLayer * sizeof(VkLayerProperties));
					vkEnumerateDeviceLayerProperties(physicalDevice, &nLayer, layerProp);
					printf("\t\t nLayer = %u: \n", nLayer);
					for (j = 0; j < nLayer; ++j)
					{
						// search layer
						name = layerProp[j].layerName;
						n = cvk_strfind_store(name, &logicalDeviceInfo.enabledLayerCount, layerInfoFinal_device,
							layerInfo_device, layerInfo_device_req, layerInfoLen_device, layerInfoLen_device_req);

						// print layer info
						cvkRendererInternalPrintLayer(layerProp + j, j, (n >= 0 ? pf2s : pf2));

						// enumerate extensions for each layer
						vkEnumerateDeviceExtensionProperties(physicalDevice, name, &nExtension, NULL);
						if (nExtension)
						{
							// iterate through extensions
							extensionProp = (VkExtensionProperties*)malloc(nExtension * sizeof(VkExtensionProperties));
							vkEnumerateDeviceExtensionProperties(physicalDevice, name, &nExtension, extensionProp);
							printf("\t\t\t nExtension = %u: \n", nExtension);
							for (k = 0; k < nExtension; ++k)
							{
								// search extension
								name = extensionProp[k].extensionName;
								n = cvk_strfind_store(name, &logicalDeviceInfo.enabledExtensionCount, extInfoFinal_device,
									extInfo_device, extInfo_device_req, extInfoLen_device, extInfoLen_device_req);

								// print extension info
								cvkRendererInternalPrintExtension(extensionProp + k, k, (n >= 0 ? pf3s : pf3));
							}

							// release extension list
							free(extensionProp);
							extensionProp = NULL;
						}
					}

					// release layer list
					free(layerProp);
					layerProp = NULL;
				}

				// set up queue family info
				vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &nQueueFamily, NULL);
				if (nQueueFamily)
				{
					// allocate and enumerate queue family properties
					queueFamilyProp = (VkQueueFamilyProperties*)malloc(nQueueFamily * sizeof(VkQueueFamilyProperties));
					vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &nQueueFamily, queueFamilyProp);
					printf("\t\t nQueueFamily = %u: { [flags] (count; timestamp valid bits; min image transfer gran) } \n", nQueueFamily);
					for (j = 0; j < nQueueFamily; ++j)
					{
						// save family index if it meets the requirements
						// must also support graphics functionality and presentation
						n = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
						if (((queueFamilyProp[j].queueFlags & n) == n) &&
#ifdef _WIN32
							vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, j)
#else	// !_WIN32
#endif	// _WIN32
							)
							cvkRendererInternalPrintQueueFamily(queueFamilyProp + j, (queueFamilyIndex = j), pf2s);
						else
							cvkRendererInternalPrintQueueFamily(queueFamilyProp + j, j, pf2);
					}

					// set queue properties
					if (queueFamilyIndex >= 0)
					{
						// get queue family data
						queueCount = queueFamilyProp[queueFamilyIndex].queueCount;
						n = (queueCount * sizeof(f32));
						logicalDeviceQueuePriority = (f32*)malloc(n);
						memset(logicalDeviceQueuePriority, 0, n);

						// update queue info for device initialization
						queueInfo.queueFamilyIndex = queueFamilyIndex;
						queueInfo.queueCount = queueCount;
						queueInfo.pQueuePriorities = logicalDeviceQueuePriority;
					}

					// release queue family list
					free(queueFamilyProp);
					queueFamilyProp = NULL;
				}

				// get features of device
				vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeat);

				// specify device features to be enabled and disabled
				physicalDeviceFeatReq.geometryShader = VK_TRUE;
				physicalDeviceFeatReq.tessellationShader = VK_TRUE;
				physicalDeviceFeatReq.multiDrawIndirect = physicalDeviceFeat.multiDrawIndirect;
				//physicalDeviceFeatReq.multiViewport = physicalDeviceFeat.multiViewport;

				// get memory properties of device
				vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemProp);
				printf("\t nMemoryType = %u: { [flags] (heap index) } \n", physicalDeviceMemProp.memoryTypeCount);
				for (i = 0; i < physicalDeviceMemProp.memoryTypeCount; ++i)
				{
					cvkRendererInternalPrintMemoryType(physicalDeviceMemProp.memoryTypes + i, i, pf1);
				}
				printf("\t nMemoryHeap = %u: { [flags] (device size) } \n", physicalDeviceMemProp.memoryHeapCount);
				for (i = 0; i < physicalDeviceMemProp.memoryHeapCount; ++i)
				{
					cvkRendererInternalPrintMemoryHeap(physicalDeviceMemProp.memoryHeaps + i, i, pf1);
				}

				// add required layers and extensions
				for (i = 0; i < layerInfoLen_device_req; ++i)
					if (layerInfo_device_req[i] && layerInfo_device_req[i][0])
						layerInfoFinal_device[logicalDeviceInfo.enabledLayerCount++] = layerInfo_device_req[i];
				for (i = 0; i < extInfoLen_device_req; ++i)
					if (extInfo_device_req[i] && extInfo_device_req[i][0])
						extInfoFinal_device[logicalDeviceInfo.enabledExtensionCount++] = extInfo_device_req[i];

				// confirm counts are equal
				n = (logicalDeviceInfo.enabledLayerCount == cvk_arrlen_pl(layerInfoFinal_device, layerInfoLenFinal_device));
				assert(n);
				n = (logicalDeviceInfo.enabledExtensionCount == cvk_arrlen_pl(extInfoFinal_device, extInfoLenFinal_device));
				assert(n);

				// create logical device
				vkCreateDevice(physicalDevice, &logicalDeviceInfo, alloc, &logicalDevice);
				if (logicalDevice)
				{
					printf(" Vulkan logical device created. \n");

					// done
					result = 0;
				}
			}

			// device failure
			if (result < 0)
			{
				printf(" Vulkan logical device creation failed. \n");
				vkDestroyInstance(inst, alloc);
				inst = NULL;
			}
		}
		else
		{
			printf(" Vulkan instance creation failed. \n");
		}


		//---------------------------------------------------------------------

		// done
		if (!result)
		{
			// raise init flag
			renderer->init = true;

			// set persistent renderer data
			renderer->data[cvkRendererData_instance] = inst;
			renderer->data[cvkRendererData_logicalDevice] = logicalDevice;

			// set persistent non-renderer data
			renderer->data[cvkRendererData_allocation] = alloc;
			renderer->data[cvkRendererData_logicalDevice_queuePriority] = logicalDeviceQueuePriority;
		}
		return result;
	}
	return -1;
}


int cvkRendererRelease(cvkRenderer* const renderer)
{
	if (renderer && renderer->init)
	{
		int result = -2;

		// retrieve instance
		VkInstance inst = (VkInstance)renderer->data[cvkRendererData_instance];

		// retrieve logical device
		VkDevice logicalDevice = (VkDevice)renderer->data[cvkRendererData_logicalDevice];

		// retrieve global memory management
		VkAllocationCallbacks* alloc = (VkAllocationCallbacks*)renderer->data[cvkRendererData_allocation];

		// retrieve logical device queue priority
		f32* logicalDeviceQueuePriority = (f32*)renderer->data[cvkRendererData_logicalDevice_queuePriority];

		// destroy data in reverse order
		if (vkDeviceWaitIdle(logicalDevice) == VK_SUCCESS)
		{
			// device
			vkDestroyDevice(logicalDevice, alloc);

			// instance
			vkDestroyInstance(inst, alloc);
			
			// done
			result = 0;
		}

		// done
		if (!result)
		{
			// other data deallocations
			if (alloc)
				free(alloc->pUserData);
			free(alloc);
			free(logicalDeviceQueuePriority);

			// reset persistent data
			memset(renderer->data, 0, sizeof(renderer->data));

			// lower init flag
			renderer->init = false;
		}
		return result;
	}
	return -1;
}


int cvkRendererTest(cvkRenderer* const renderer)
{
	if (renderer && renderer->init)
	{
		int result = 0;

		// done
		return result;
	}
	return -1;
}


//-----------------------------------------------------------------------------
