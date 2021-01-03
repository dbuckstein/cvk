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
inline int cvk_strfind_store(kstr const key, ui32* const count_inout, kstr* arr_out, kstr const* arr_request, ui32 const count_request)
{
	i32 n = cvk_strfind_pl(key, arr_out, *count_inout);
	if (n < 0)
	{
		// search for name in requested list, add to final if found
		if ((n = cvk_strfind_pl(key, arr_request, count_request)) >= 0)
			arr_out[(*count_inout)++] = arr_request[n];
	}
	return n;
}

// cvk_strfind_store_list
//	Copy list of strings to another if not already contained.
inline int cvk_strfind_store_list(ui32* const count_inout, kstr* arr_out, kstr const* arr_request, ui32 const count_request)
{
	ui32 i;
	for (i = 0; i < count_request; ++i)
		if (cvk_strfind_pl(arr_request[i], arr_out, *count_inout) < 0)
			arr_out[(*count_inout)++] = arr_request[i];
	return i;
}


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

#ifdef _WIN32
LRESULT CALLBACK cvkRendererInternalWindowEventProcess(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_NCCREATE:
		break;

	case WM_CREATE:
		break;

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

#define cvk_win_name TEXT("cvkRendererWindow")
int cvkRendererInternalCreateWindowClassDefault(WNDCLASSEX* windowClassPtr, HINSTANCE const instHandle)
{
	// fill in properties
	windowClassPtr->cbSize = sizeof(WNDCLASSEX);
	windowClassPtr->style = (CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS);
	windowClassPtr->lpfnWndProc = cvkRendererInternalWindowEventProcess;
	windowClassPtr->cbClsExtra = 0;
	windowClassPtr->cbWndExtra = sizeof(ptr);
	windowClassPtr->hInstance = instHandle;
	windowClassPtr->hIcon = LoadIcon(instHandle, IDI_WINLOGO);
	windowClassPtr->hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClassPtr->hbrBackground = 0;
	windowClassPtr->lpszMenuName = 0;
	windowClassPtr->lpszClassName = cvk_win_name;
	windowClassPtr->hIconSm = windowClassPtr->hIcon;

	// register
	if (RegisterClassEx(windowClassPtr))
	{
		// done
		return 0;
	}
	return -1;
}

int cvkRendererInternalCreateWindowDefault(HWND* windowPtr)
{
	HWND wndHandle = NULL;
	RECT displayArea = { 0 };
	dword style = (WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE | WS_POPUP), styleEx = (WS_EX_APPWINDOW);
	ui16 const winWidth = 1024, winHeight = 768;

	// create class if it doesn't exist
	HINSTANCE const instHandle = GetModuleHandle(NULL);
	WNDCLASSEX wndClass = { 0 };
	if (!GetClassInfoEx(instHandle, cvk_win_name, &wndClass))
		cvkRendererInternalCreateWindowClassDefault(&wndClass, instHandle);

	// create window
	displayArea.right = displayArea.left + winWidth;
	displayArea.bottom = displayArea.top + winHeight;
	styleEx |= WS_EX_WINDOWEDGE;
	style |= WS_OVERLAPPEDWINDOW;
	AdjustWindowRectEx(&displayArea, style, FALSE, styleEx);
	wndHandle = CreateWindowEx(styleEx, cvk_win_name, cvk_win_name, style,
		0, 0, (displayArea.right - displayArea.left), (displayArea.bottom - displayArea.top),
		NULL, NULL, instHandle, NULL);
	if (wndHandle)
	{
		ShowWindow(wndHandle, SW_SHOW);
		UpdateWindow(wndHandle);

		// done
		*windowPtr = wndHandle;
		return 0;
	}
	return -1;
}

int cvkRendererInternalWindowMainLoop()
{
	MSG message[1] = { 0 };
	BOOL result;
	while (result = GetMessage(message, NULL, 0, 0))
	{
		// message
		if (result > 0)
		{
			TranslateMessage(message);
			DispatchMessage(message);
		}
		// error
		else if (result < 0)
		{
		}
		// quit
		else
			break;
	}

	// unregister window class
	if (UnregisterClass(cvk_win_name, GetModuleHandle(NULL)))
	{
		// done
		return (int)message->wParam;
	}
	return -1;
}
#else	// !_WIN32
#endif	// _WIN32


//-----------------------------------------------------------------------------

//PFN_vkDebugReportCallbackEXT
VkBool32 cvkRendererInternalDebugCallback(
	VkDebugReportFlagsEXT const flags, VkDebugReportObjectTypeEXT const objectType,
	ui64 const object, size const location,
	i32 const messageCode, kstr const pLayerPrefix, kstr const pMessage,
	ptr const pUserData)
{
	// print debug info
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		printf("cvk Debug INFO (%s, #%d): %s \n", pLayerPrefix, messageCode, pMessage);
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		printf("cvk Debug WARNING (%s, #%d): %s \n", pLayerPrefix, messageCode, pMessage);
	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		printf("cvk Debug PERFORMANCE WARNING (%s, #%d): %s \n", pLayerPrefix, messageCode, pMessage);
	else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		printf("cvk Debug ERROR (%s, #%d): %s \n", pLayerPrefix, messageCode, pMessage);
	else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
		printf("cvk Debug DEBUG (%s, #%d): %s \n", pLayerPrefix, messageCode, pMessage);
	// not handled
	else
		return VK_FALSE;

	// handled, but should still return false according to spec
	return VK_SUCCESS;
}


//-----------------------------------------------------------------------------

int cvkRendererInternalCreate(VkAllocationCallbacks const* const alloc,
	VkInstance* const instPtr, VkDevice* const logicalDevicePtr, VkSurfaceKHR* const presSurfacePtr,
	VkDebugReportCallbackEXT* const debugReportPtr)
{
	int n = -1;
	ui32 i = 0, j = 0, k = 0;
	kstr name = NULL;
	kstr const pf1 = "\t  ", pf1s = "\t->", pf2 = "\t\t  ", pf2s = "\t\t->", pf3 = "\t\t\t  ", pf3s = "\t\t\t->";
	VkResult result = VK_SUCCESS;


	//---------------------------------------------------------------------
	// instance data

	// instance handle
	VkInstance inst = NULL;
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
		"VK_LAYER_LUNARG_standard_validation",
	/*	// included in standard validation (7): 
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_LUNARG_parameter_validation",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_LUNARG_image",
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_GOOGLE_unique_objects",	*/
		"VK_LAYER_LUNARG_api_dump",
		//"VK_LAYER_LUNARG_monitor",
		//"VK_LAYER_LUNARG_screenshot",
#endif	// _DEBUG
		NULL
	};
	// required instance layers
	kstr const layerInfo_inst_req[] = {
		NULL
	};
	// number of layers in original array
	ui32 const layerInfoLen_inst = cvk_arrlen_pl(layerInfo_inst, cvk_arrlen(layerInfo_inst));
	// number of required instance layers
	ui32 const layerInfoLen_inst_req = cvk_arrlen_pl(layerInfo_inst_req, cvk_arrlen(layerInfo_inst_req));
	// final layers to be used with instance
	kstr layerInfoFinal_inst[cvk_arrlen(layerInfo_inst) + cvk_arrlen(layerInfo_inst_req)] = { 0 };
	// maximum final layers
	ui32 const layerInfoLenFinal_inst = cvk_arrlen(layerInfoFinal_inst);

	// extensions to be searched and enabled for instance
	kstr const extInfo_inst[] = {
#ifdef _DEBUG
		// included with KHR validation layer (3): 
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,
#endif	// _DEBUG
		NULL
	};
	// required instance extensions
	kstr const extInfo_inst_req[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else	// !_WIN32
#endif	// _WIN32
		NULL
	};
	// number of extensions in original array
	ui32 const extInfoLen_inst = cvk_arrlen_pl(extInfo_inst, cvk_arrlen(extInfo_inst));
	// number of required instance extensions
	ui32 const extInfoLen_inst_req = cvk_arrlen_pl(extInfo_inst_req, cvk_arrlen(extInfo_inst_req));
	// final extensions to be used with instance
	kstr extInfoFinal_inst[cvk_arrlen(extInfo_inst) + cvk_arrlen(extInfo_inst_req)] = { 0 };
	// maximum final extensions
	ui32 const extInfoLenFinal_inst = cvk_arrlen(extInfoFinal_inst);

	// application info for instance
	VkApplicationInfo const appInfo = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
		"cvkTest", VK_MAKE_VERSION(0, 0, 1), "cvk", VK_MAKE_VERSION(0, 0, 1), VK_API_VERSION_1_0,
	};

#ifdef _DEBUG
	// create debug report callback function pointer
	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = NULL;
	// debug report callback info
	VkDebugReportCallbackCreateInfoEXT const debugInfo = {
		VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT, NULL,
		(VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT),
		cvkRendererInternalDebugCallback, NULL,
	}, * const debugInfoPtr = NULL;// &debugInfo;
#else	// !_DEBUG
	ptr const debugInfoPtr = NULL;
#endif	// _DEBUG

	// instance initialization info
	//	-> need to update layer and extension counts
	VkInstanceCreateInfo instInfo = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, debugInfoPtr, 0, &appInfo,
		cvk_arrlen_pl(layerInfoFinal_inst, layerInfoLenFinal_inst), layerInfoFinal_inst,
		cvk_arrlen_pl(extInfoFinal_inst, extInfoLenFinal_inst), extInfoFinal_inst,
	};


	//---------------------------------------------------------------------
	// logical device data

	// logical device
	VkDevice logicalDevice = NULL;
	// info for physical devices
	ui32 nPhysicalDevice = 0;
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

/*	// layers to be searched and enabled for device
	kstr const layerInfo_device[] = {
#ifdef _DEBUG
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_LUNARG_standard_validation",
		"VK_LAYER_LUNARG_api_dump",
#endif	// _DEBUG
		NULL
	};
	// required device layers
	kstr const layerInfo_device_req[] = {
		NULL
	};
	// number of layers in original array
	ui32 const layerInfoLen_device = cvk_arrlen_pl(layerInfo_device, cvk_arrlen(layerInfo_device));
	// number of required device layers
	ui32 const layerInfoLen_device_req = cvk_arrlen_pl(layerInfo_device_req, cvk_arrlen(layerInfo_device_req));
	// final layers to be used with device
	kstr layerInfoFinal_device[cvk_arrlen(layerInfo_device) + cvk_arrlen(layerInfo_device_req)] = { 0 };
	// maximum final layers
	ui32 const layerInfoLenFinal_device = cvk_arrlen(layerInfoFinal_device);
*/
	// extensions to be searched and enabled for device
	kstr const extInfo_device[] = {
#ifdef _DEBUG
		// included with KHR validation layer (3): 
		VK_EXT_VALIDATION_CACHE_EXTENSION_NAME,
		VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
		VK_EXT_TOOLING_INFO_EXTENSION_NAME,
#endif	// _DEBUG
		NULL
	};
	// required device extensions
	kstr const extInfo_device_req[] = {
		NULL
	};
	// number of extensions in original array
	ui32 const extInfoLen_device = cvk_arrlen_pl(extInfo_device, cvk_arrlen(extInfo_device));
	// number of required device extensions
	ui32 const extInfoLen_device_req = cvk_arrlen_pl(extInfo_device_req, cvk_arrlen(extInfo_device_req));
	// final extensions to be used with device
	kstr extInfoFinal_device[cvk_arrlen(extInfo_device) + cvk_arrlen(extInfo_device_req)] = { 0 };
	// maximum final extensions
	ui32 const extInfoLenFinal_device = cvk_arrlen(extInfoFinal_device);

	// number of queue families on selected device
	ui32 nQueueFamily = 0;
	// queue family index to use for graphics and presentation
	i32 queueFamilyIndex = -1;
	// queue count in selected family and across all families
	ui32 queueCount = 0, queueCountTotal = 0;
	// priorities list
	f32* queuePriority = NULL;
	// queue creation info for logical device
	//	-> copy for each queue family
	//	-> need to update queue family index, count and priorities
	VkDeviceQueueCreateInfo const queueInfoTmp = {
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0,
		queueFamilyIndex, queueCount, queuePriority, // update
	};
	// queue creation info array
	VkDeviceQueueCreateInfo* queueInfoArr = NULL;
	// device queue family properties
	VkQueueFamilyProperties* queueFamilyProp = NULL;

	// physical device to use for graphics
	i32 physicalDeviceIndex = -1;
	// logical device creation info
	//	-> need to update queue info
	//	-> need to update layer and extension counts
	VkDeviceCreateInfo logicalDeviceInfo = {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0,
		nQueueFamily, queueInfoArr, // update
		//cvk_arrlen_pl(layerInfoFinal_device, layerInfoLenFinal_device), layerInfoFinal_device,
		0, NULL, // device layers deprecated
		cvk_arrlen_pl(extInfoFinal_device, extInfoLenFinal_device), extInfoFinal_device,
		&physicalDeviceFeatReq,
	};


	//---------------------------------------------------------------------
	// presentation surface data

	// presentation surface
	VkSurfaceKHR presSurface = NULL;
#ifdef _WIN32
	// presentation surface creation info for Windows
	//	-> need to update window handle
	VkWin32SurfaceCreateInfoKHR presSurfaceInfo = {
		VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, NULL, 0,
		GetModuleHandle(NULL), NULL,
	};
#else	// !_WIN32
#endif	// _WIN32


	//---------------------------------------------------------------------

	// instance setup
	printf(" Vulkan instance... \n");

	// enumerate instance version
	vkEnumerateInstanceVersion(&instVersion);
	printf("\t instVer = %u.%u.%u \n",
		VK_VERSION_MAJOR(instVersion), VK_VERSION_MINOR(instVersion), VK_VERSION_PATCH(instVersion));

	// enumerate instance layers
	result = vkEnumerateInstanceLayerProperties(&nLayer, NULL);
	if ((result == VK_SUCCESS) && nLayer)
	{
		// iterate through layers and enumerate extensions
		layerProp = (VkLayerProperties*)malloc(nLayer * sizeof(VkLayerProperties));
		result = vkEnumerateInstanceLayerProperties(&nLayer, layerProp);
		printf("\t nLayer = %u: { \"layerName\" (specVer; implVer) \"description\" } \n", nLayer);
		for (i = 0; i < nLayer; ++i)
		{
			// search and add layer from requested list
			name = layerProp[i].layerName;
			n = cvk_strfind_store(name, &instInfo.enabledLayerCount,
				layerInfoFinal_inst, layerInfo_inst, layerInfoLen_inst);

			// print layer info, indicating whether it is requested
			cvkRendererInternalPrintLayer(layerProp + i, i, (n >= 0 ? pf1s : pf1));

			// enumerate extensions for each layer
			result = vkEnumerateInstanceExtensionProperties(name, &nExtension, NULL);
			if ((result == VK_SUCCESS) && nExtension)
			{
				// iterate through extensions
				extensionProp = (VkExtensionProperties*)malloc(nExtension * sizeof(VkExtensionProperties));
				result = vkEnumerateInstanceExtensionProperties(name, &nExtension, extensionProp);
				printf("\t\t nExtension = %u: { \"extensionName\" (specVer) } \n", nExtension);
				for (j = 0; j < nExtension; ++j)
				{
					// search extension
					name = extensionProp[j].extensionName;
					n = cvk_strfind_store(name, &instInfo.enabledExtensionCount,
						extInfoFinal_inst, extInfo_inst, extInfoLen_inst);

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

	// add remaining instance layers and extensions, confirm counts are equal
	cvk_strfind_store_list(&instInfo.enabledLayerCount,
		layerInfoFinal_inst, layerInfo_inst_req, layerInfoLen_inst_req);
	assert(instInfo.enabledLayerCount == cvk_arrlen_pl(layerInfoFinal_inst, layerInfoLenFinal_inst));
	cvk_strfind_store_list(&instInfo.enabledExtensionCount,
		extInfoFinal_inst, extInfo_inst_req, extInfoLen_inst_req);
	assert(instInfo.enabledExtensionCount == cvk_arrlen_pl(extInfoFinal_inst, extInfoLenFinal_inst));

	// create instance
	result = vkCreateInstance(&instInfo, alloc, &inst);
	if ((result == VK_SUCCESS) && inst)
	{
		printf(" Vulkan instance created. \n");

		// set up logical device
		printf(" Vulkan logical device... \n");

		// retrieve devices
		result = vkEnumeratePhysicalDevices(inst, &nPhysicalDevice, NULL);
		if (nPhysicalDevice)
		{
			// alloc device info
			physicalDeviceList = (VkPhysicalDevice*)malloc(nLayer * sizeof(VkPhysicalDevice));
			result = vkEnumeratePhysicalDevices(inst, &nPhysicalDevice, physicalDeviceList);
			printf("\t nPhysicalDevice = %u: { \"name\" [type] (apiVer; driverVer; vendorID; deviceID) } \n", nPhysicalDevice);
			for (i = 0; i < nPhysicalDevice; ++i)
			{
				// query device properties
				vkGetPhysicalDeviceProperties(physicalDeviceList[i], &physicalDeviceProp);

				// save index of most capable device
				n = (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
				if ((physicalDeviceProp.deviceType & n) && physicalDeviceIndex < 0)
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
			result = vkEnumerateDeviceLayerProperties(physicalDevice, &nLayer, NULL);
			if ((result == VK_SUCCESS) && nLayer)
			{
				// iterate through layers and enumerate extensions
				layerProp = (VkLayerProperties*)malloc(nLayer * sizeof(VkLayerProperties));
				result = vkEnumerateDeviceLayerProperties(physicalDevice, &nLayer, layerProp);
				printf("\t\t nLayer = %u: \n", nLayer);
				for (j = 0; j < nLayer; ++j)
				{
					// search layer
					name = layerProp[j].layerName;
					//n = cvk_strfind_store(name, &logicalDeviceInfo.enabledLayerCount,
					//	layerInfoFinal_device, layerInfo_device, layerInfoLen_device);

					// print layer info
					cvkRendererInternalPrintLayer(layerProp + j, j, pf2);//(n >= 0 ? pf2s : pf2));

					// enumerate extensions for each layer
					result = vkEnumerateDeviceExtensionProperties(physicalDevice, name, &nExtension, NULL);
					if ((result == VK_SUCCESS) && nExtension)
					{
						// iterate through extensions
						extensionProp = (VkExtensionProperties*)malloc(nExtension * sizeof(VkExtensionProperties));
						result = vkEnumerateDeviceExtensionProperties(physicalDevice, name, &nExtension, extensionProp);
						printf("\t\t\t nExtension = %u: \n", nExtension);
						for (k = 0; k < nExtension; ++k)
						{
							// search extension
							name = extensionProp[k].extensionName;
							n = cvk_strfind_store(name, &logicalDeviceInfo.enabledExtensionCount,
								extInfoFinal_device, extInfo_device, extInfoLen_device);

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
				queueInfoArr = (VkDeviceQueueCreateInfo*)malloc(nQueueFamily * sizeof(VkDeviceQueueCreateInfo));
				vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &nQueueFamily, queueFamilyProp);
				printf("\t\t nQueueFamily = %u: { [flags] (count; timestamp valid bits; min image transfer gran) } \n", nQueueFamily);
				for (j = 0; j < nQueueFamily; ++j)
				{
					// copy create info template
					queueInfoArr[j] = queueInfoTmp;
					queueInfoArr[j].queueFamilyIndex = j;
					queueCount = queueFamilyProp[j].queueCount;
					queueCountTotal += queueCount;
					queueInfoArr[j].queueCount = queueCount;

					// save family index if it meets the requirements
					// must also support graphics functionality and presentation
					n = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
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

				// allocate and set priorities
				if (queueCountTotal)
				{
					n = (queueCountTotal * sizeof(f32));
					queuePriority = (f32*)memset(malloc(n), 0, n);
					for (n = j = 0; j < nQueueFamily; n += queueInfoArr[j++].queueCount)
						queueInfoArr[j].pQueuePriorities = (queuePriority + n);
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

			// adjust and set queue info
			if (queueFamilyIndex >= 0)
			{
				logicalDeviceInfo.queueCreateInfoCount = 1;// nQueueFamily;
				logicalDeviceInfo.pQueueCreateInfos = queueInfoArr + queueFamilyIndex;// queueInfoArr;
				queueInfoArr[queueFamilyIndex].queueCount = 1;
			}

			// get memory properties of device
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemProp);
			printf("\t nMemoryType = %u: { [flags] (heap index) } \n", physicalDeviceMemProp.memoryTypeCount);
			for (i = 0; i < physicalDeviceMemProp.memoryTypeCount; ++i)
				cvkRendererInternalPrintMemoryType(physicalDeviceMemProp.memoryTypes + i, i, pf1);
			printf("\t nMemoryHeap = %u: { [flags] (device size) } \n", physicalDeviceMemProp.memoryHeapCount);
			for (i = 0; i < physicalDeviceMemProp.memoryHeapCount; ++i)
				cvkRendererInternalPrintMemoryHeap(physicalDeviceMemProp.memoryHeaps + i, i, pf1);

			// add remaining device layers and extensions
			//cvk_strfind_store_list(&logicalDeviceInfo.enabledLayerCount,
			//	layerInfoFinal_device, layerInfo_device_req, layerInfoLen_device_req);
			//assert(logicalDeviceInfo.enabledLayerCount == cvk_arrlen_pl(layerInfoFinal_device, layerInfoLenFinal_device));
			cvk_strfind_store_list(&logicalDeviceInfo.enabledExtensionCount,
				extInfoFinal_device, extInfo_device_req, extInfoLen_device_req);
			assert(logicalDeviceInfo.enabledExtensionCount == cvk_arrlen_pl(extInfoFinal_device, extInfoLenFinal_device));

			// create logical device
			result = vkCreateDevice(physicalDevice, &logicalDeviceInfo, alloc, &logicalDevice);
			free(queueInfoArr);
			queueInfoArr = NULL;
			free(queuePriority);
			queuePriority = NULL;
			if ((result == VK_SUCCESS) && logicalDevice)
			{
				printf(" Vulkan logical device created. \n");

				// set up presentation surface
				printf(" Vulkan presentation surface... \n");

				// create window
#ifdef _WIN32
				cvkRendererInternalCreateWindowDefault(&presSurfaceInfo.hwnd);
				if (presSurfaceInfo.hwnd)
#else	// !_WIN32
#endif	// _WIN32
				{
#ifdef _WIN32
					// create presentation surface
					result = vkCreateWin32SurfaceKHR(inst, &presSurfaceInfo, alloc, &presSurface);
#else	// !_WIN32
#endif	// _WIN32
					if ((result == VK_SUCCESS && presSurface))
					{
						printf(" Vulkan presentation surface created. \n");

#ifdef _DEBUG
						if (debugReportPtr)
						{
							// set up debugging
							printf(" Vulkan debug report callback... \n");
							result = VK_TRUE;
							vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(inst, "vkCreateDebugReportCallbackEXT");
							if (vkCreateDebugReportCallbackEXT)
								result = vkCreateDebugReportCallbackEXT(inst, &debugInfo, alloc, debugReportPtr);
							if ((result == VK_SUCCESS) && *debugReportPtr)
								printf(" Vulkan debug report callback created. \n");
							else
								printf(" Vulkan debug report callback creation failed. \n");
						}
#endif	// _DEBUG

						// assign final values
						*instPtr = inst;
						*logicalDevicePtr = logicalDevice;
						*presSurfacePtr = presSurface;

						// done
						return 0;
					}
				}
				
				// surface failure
				printf(" Vulkan presentation surface creation failed. \n");
				return -3;
			}
		}

		// device failure
		printf(" Vulkan logical device creation failed. \n");
		return -2;
	}

	// instance failure
	printf(" Vulkan instance creation failed. \n");
	return -1;
}

int cvkRendererInternalRelease(VkAllocationCallbacks const* const alloc,
	VkInstance const inst, VkDevice const logicalDevice, VkSurfaceKHR const presSurface,
	VkDebugReportCallbackEXT const debugReport)
{
#ifdef _DEBUG
	// destroy debug report callback function pointer
	PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = NULL;
#endif	// _DEBUG

#ifdef _DEBUG
	if (debugReport)
	{
		// cleanup debugging
		vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(inst, "vkDestroyDebugReportCallbackEXT");
		if (vkDestroyDebugReportCallbackEXT)
			vkDestroyDebugReportCallbackEXT(inst, debugReport, alloc);
	}
#endif	// _DEBUG

	// presentation surface
	if (presSurface)
		vkDestroySurfaceKHR(inst, presSurface, alloc);

	// logical device
	// wait for device to finish work
	if (logicalDevice)
		if (vkDeviceWaitIdle(logicalDevice) == VK_SUCCESS)
			vkDestroyDevice(logicalDevice, alloc);

	// instance
	if (inst)
		vkDestroyInstance(inst, alloc);

	// done
	return 0;
}

int cvkRendererInternalAllocSetup(VkAllocationCallbacks** const allocPtr)
{
	int result = -1;
	VkAllocationCallbacks* alloc = NULL;

	// create instance and data
	//alloc = (VkAllocationCallbacks*)malloc(sizeof(VkAllocationCallbacks));
	if (alloc)
	{
		//alloc->pUserData = malloc(0);

		// success
		result = 0;
	}

	// done
	*allocPtr = alloc;
	return result;
}

int cvkRendererInternalAllocCleanup(VkAllocationCallbacks* const alloc)
{
	int result = -1;

	// delete data and instance
	if (alloc)
	{
		free(alloc->pUserData);
		free(alloc);

		// success
		result = 0;
	}

	// done
	return result;
}


//-----------------------------------------------------------------------------

// cvkRendererData_vk
//	Internal data/handle indices for renderer instance.
enum cvkRendererData_vk
{
	cvkRendererData_instance,		// instance
	cvkRendererData_logicalDevice,	// logical device
	cvkRendererData_presSurface,	// presentation surface
	cvkRendererData_debugReport,			// debug callback

	// maximum number of renderer handles, start counting non-handle data here
	cvkRendererData_count
};

// cvkRendererData
//	Internal data not involved with renderer.
enum cvkRendererData
{
	cvkRendererData_allocation = cvkRendererData_count,	// global memory management
};


//-----------------------------------------------------------------------------

int cvkRendererCreate(cvkRenderer* const renderer)
{
	if (renderer && !renderer->init)
	{
		int result = 0;

		// instance
		VkInstance inst = NULL;

		// logical device
		VkDevice logicalDevice = NULL;

		// presentation surface
		VkSurfaceKHR presSurface = NULL;

		// debug report
		VkDebugReportCallbackEXT debugReport = NULL;

		// global memory management
		VkAllocationCallbacks* alloc = NULL;

		// begin setup
		printf("cvkRendererCreate \n");

		// allocator setup
		cvkRendererInternalAllocSetup(&alloc);

		// internal create
		result = cvkRendererInternalCreate(alloc, &inst, &logicalDevice, &presSurface, &debugReport);

		// success
		if (!result)
		{
			// raise init flag
			renderer->init = true;

			// set persistent renderer data
			renderer->data[cvkRendererData_instance] = inst;
			renderer->data[cvkRendererData_logicalDevice] = logicalDevice;
			renderer->data[cvkRendererData_presSurface] = presSurface;
			renderer->data[cvkRendererData_debugReport] = debugReport;

			// set persistent non-renderer data
			renderer->data[cvkRendererData_allocation] = alloc;

			// done
			return result;
		}

		// failure
		result = cvkRendererInternalRelease(alloc, inst, logicalDevice, presSurface, debugReport);
		cvkRendererInternalAllocCleanup(alloc);
		memset(renderer->data, 0, sizeof(renderer->data));
		return -2;
	}
	return -1;
}


int cvkRendererRelease(cvkRenderer* const renderer)
{
	if (renderer && renderer->init)
	{
		int result = 0;

		// retrieve instance
		VkInstance inst = (VkInstance)renderer->data[cvkRendererData_instance];

		// retrieve logical device
		VkDevice logicalDevice = (VkDevice)renderer->data[cvkRendererData_logicalDevice];

		// retrieve presentation surface
		VkSurfaceKHR presSurface = (VkSurfaceKHR)renderer->data[cvkRendererData_presSurface];

		// retrieve debug report callback
		VkDebugReportCallbackEXT debugReport = (VkDebugReportCallbackEXT)renderer->data[cvkRendererData_debugReport];

		// retrieve global memory management
		VkAllocationCallbacks* alloc = (VkAllocationCallbacks*)renderer->data[cvkRendererData_allocation];

		// begin termination
		printf("cvkRendererRelease \n");

		// destroy data in reverse order
		result = cvkRendererInternalRelease(alloc, inst, logicalDevice, presSurface, debugReport);

		// success
		if (!result)
		{
			// other data deallocations
			cvkRendererInternalAllocCleanup(alloc);

			// reset persistent data
			memset(renderer->data, 0, sizeof(renderer->data));

			// lower init flag
			renderer->init = false;

			// done
			return result;
		}

		// failure
		return -2;
	}
	return -1;
}


int cvkRendererTest(cvkRenderer* const renderer)
{
	if (renderer && renderer->init)
	{
		int result = 0;

		// begin testing
		printf("cvkRendererTest \n");

		// window loop
		result = cvkRendererInternalWindowMainLoop();

		// success
		if (!result)
		{

			// done
			return result;
		}

		// failure
		return -2;
	}
	return -1;
}


//-----------------------------------------------------------------------------
