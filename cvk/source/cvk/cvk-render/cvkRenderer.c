/*
   Copyright 2020 Daniel S. Buckstein

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

#include "vulkan/vulkan.h"
#ifdef _WIN32
#include <Windows.h>
#include "vulkan/vulkan_win32.h"
#else	// !_WIN32
#endif	// _WIN32


//-----------------------------------------------------------------------------

// cvkRendererData
//	Internal renderer data indices.
enum cvkRendererData
{
	cvkRendererData_instance,
	cvkRendererData_logicalDevice,

	cvkRendererData_count
};


//-----------------------------------------------------------------------------

int cvkRendererCreate(cvkRenderer* const renderer)
{
	if (renderer && !renderer->init)
	{
		int result = -2;
		ui32 i = 0, j = 0, k = 0;
		kstr const deviceType[] = { "other", "integrated gpu", "discrete gpu", "virtual gpu", "cpu" };


		//---------------------------------------------------------------------
		// persistent data

		// instance
		VkInstance inst = NULL;
		VkAllocationCallbacks const instAlloc = { 0 }, * instAllocPtr = NULL;

		// logical device
		VkDevice logicalDevice = NULL;
		VkAllocationCallbacks const logicalDeviceAlloc = { 0 }, * logicalDeviceAllocPtr = NULL;


		//---------------------------------------------------------------------
		// instance data

		// data for instance, layers and extensions
		ui32 instVersion = 0, nLayer = 0, nExtension = 0;
		// array of layer info
		VkLayerProperties* layerProp = NULL;
		// array of extension info for each layer
		VkExtensionProperties* extensionProp = NULL;
		// application info for instance
		VkApplicationInfo const appInfo = {
			VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
			"cvkTest", 0, "cvk", 0, 0,
		};
		// layers to be enabled for instance
		kstr const layerInfo[] = {
			//"VK_LAYER_KHRONOS_validation",
			"VK_LAYER_LUNARG_standard_validation",
			//"VK_LAYER_LUNARG_object_tracker",
		};
		// extensions to be enabled for instance
		kstr const extInfo[] = {
			VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else	// !_WIN32
#endif	// _WIN32
			//"VK_EXT_debug_report",
			//"VK_EXT_debug_utils",
			//"VK_EXT_validation_features",
		};
		// instance initialization info
		VkInstanceCreateInfo const instInfo = {
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &appInfo,
			(sizeof(layerInfo) / sizeof(*layerInfo)), layerInfo,
			(sizeof(extInfo) / sizeof(*extInfo)), extInfo,
		}, * instInfoPtr = &instInfo;


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
		// queue creation info
		VkDeviceQueueCreateInfo const queueInfo = {
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL,
			0, 0, 1, NULL,
		}, * queueInfoPtr = &queueInfo;
		// logical device creation info
		VkDeviceCreateInfo const logicalDeviceInfo = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL,
			0, 1, queueInfoPtr, 0, NULL, 0, NULL, &physicalDeviceFeatReq,
		}, * logicalDeviceInfoPtr = &logicalDeviceInfo;


		//---------------------------------------------------------------------
		// initialize

		// begin setup
		printf("cvkRendererCreate \n");

		// instance setup
		printf(" Vulkan instance... \n");

		// enumerate instance version
		vkEnumerateInstanceVersion(&instVersion);
		printf("\t instVersion = %u \n", instVersion);

		// enumerate instance layers
		vkEnumerateInstanceLayerProperties(&nLayer, NULL);
		if (nLayer)
		{
			printf("\t nLayer = %u: { \"layerName\" (specVersion.implementationVersion) \"description\" } \n", nLayer);
			
			// iterate through layers and enumerate extensions
			layerProp = (VkLayerProperties*)malloc(nLayer * sizeof(VkLayerProperties));
			vkEnumerateInstanceLayerProperties(&nLayer, layerProp);
			for (i = 0; i < nLayer; ++i)
			{
				printf("\t layerProp[%u] = { \"%s\" (%u.%u): \"%s\" } \n", i,
					layerProp[i].layerName,
					layerProp[i].specVersion, layerProp[i].implementationVersion,
					layerProp[i].description);

				// enumerate extensions for each layer
				vkEnumerateInstanceExtensionProperties(layerProp[i].layerName, &nExtension, NULL);
				if (nExtension)
				{
					printf("\t\t nExtension = %u: { \"extensionName\" (specVersion) } \n", nExtension);

					// iterate through extensions
					extensionProp = (VkExtensionProperties*)malloc(nExtension * sizeof(VkExtensionProperties));
					vkEnumerateInstanceExtensionProperties(layerProp[i].layerName, &nExtension, extensionProp);
					for (j = 0; j < nExtension; ++j)
					{
						printf("\t\t extensionProp[%u] = { \"%s\" (%u) } \n", j,
							extensionProp[j].extensionName, extensionProp[j].specVersion);
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

		// create instance
		vkCreateInstance(instInfoPtr, instAllocPtr, &inst);
		if (inst)
		{
			printf(" Vulkan instance created. \n");

			// set up logical device
			printf(" Vulkan logical device... \n");

			// set up devices
			vkEnumeratePhysicalDevices(inst, &nPhysicalDevice, NULL);
			if (nPhysicalDevice)
			{
				printf("\t nPhysicalDevice = %u: { \"name\": \"type\" (apiVer.driverVer.vendorID.deviceID) } \n", nPhysicalDevice);

				// alloc device info
				physicalDeviceList = (VkPhysicalDevice*)malloc(nLayer * sizeof(VkPhysicalDevice));
				vkEnumeratePhysicalDevices(inst, &nPhysicalDevice, physicalDeviceList);
				for (i = 0; i < nPhysicalDevice; ++i)
				{
					// query device properties
					vkGetPhysicalDeviceProperties(physicalDeviceList[i], &physicalDeviceProp);
					printf("\t physicalDeviceList[%u] = { \"%s\": \"%s\" (%u.%u.%u.%u) } \n", i,
						physicalDeviceProp.deviceName, deviceType[physicalDeviceProp.deviceType],
						physicalDeviceProp.apiVersion, physicalDeviceProp.driverVersion,
						physicalDeviceProp.vendorID, physicalDeviceProp.deviceID);

					// enumerate device layers
					vkEnumerateDeviceLayerProperties(physicalDeviceList[i], &nLayer, NULL);
					if (nLayer)
					{
						printf("\t\t nLayer = %u: { \"layerName\" (specVersion.implementationVersion) \"description\" } \n", nLayer);

						// iterate through layers and enumerate extensions
						layerProp = (VkLayerProperties*)malloc(nLayer * sizeof(VkLayerProperties));
						vkEnumerateDeviceLayerProperties(physicalDeviceList[i], &nLayer, layerProp);
						for (j = 0; j < nLayer; ++j)
						{
							printf("\t\t layerProp[%u] = { \"%s\" (%u.%u): \"%s\" } \n", j,
								layerProp[j].layerName,
								layerProp[j].specVersion, layerProp[j].implementationVersion,
								layerProp[j].description);

							// enumerate extensions for each layer
							vkEnumerateDeviceExtensionProperties(physicalDeviceList[i], layerProp[j].layerName, &nExtension, NULL);
							if (nExtension)
							{
								printf("\t\t\t nExtension = %u: { \"extensionName\" (specVersion) } \n", nExtension);

								// iterate through extensions
								extensionProp = (VkExtensionProperties*)malloc(nExtension * sizeof(VkExtensionProperties));
								vkEnumerateDeviceExtensionProperties(physicalDeviceList[i], layerProp[j].layerName, &nExtension, extensionProp);
								for (k = 0; k < nExtension; ++k)
								{
									printf("\t\t\t extensionProp[%u] = { \"%s\" (%u) } \n", k,
										extensionProp[k].extensionName, extensionProp[k].specVersion);
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
					vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceList[i], &nQueueFamily, NULL);
					if (nQueueFamily)
					{
						printf("\t\t nQueueFamily = %u: { [flags] (count) } \n", nQueueFamily);

						// allocate and query queue family properties
						queueFamilyProp = (VkQueueFamilyProperties*)malloc(nQueueFamily * sizeof(VkQueueFamilyProperties));
						vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceList[i], &nQueueFamily, queueFamilyProp);
						for (j = 0; j < nQueueFamily; ++j)
						{
							printf("\t\t queueFamilyProp[%u] = { %s%s%s%s%s (%u) } \n", j,
								(queueFamilyProp[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) ? "[graphics]" : "",
								(queueFamilyProp[j].queueFlags & VK_QUEUE_COMPUTE_BIT) ? "[compute]" : "",
								(queueFamilyProp[j].queueFlags & VK_QUEUE_TRANSFER_BIT) ? "[transfer]" : "",
								(queueFamilyProp[j].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) ? "[sparsebind]" : "",
								(queueFamilyProp[j].queueFlags & VK_QUEUE_PROTECTED_BIT) ? "[protected]" : "",
								queueFamilyProp[j].queueCount);
						}

						// release queue family list
						free(queueFamilyProp);
						queueFamilyProp = NULL;
					}
				}

				// select physical device and properties
				physicalDevice = physicalDeviceList[0];
				vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeat);
				vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemProp);

				// release device list
				free(physicalDeviceList);
				physicalDeviceList = NULL;
			}

			// specify device features required given some from physical device's features
			physicalDeviceFeatReq.multiDrawIndirect = physicalDeviceFeat.multiDrawIndirect;
			physicalDeviceFeatReq.multiViewport = physicalDeviceFeat.multiViewport;
			physicalDeviceFeatReq.geometryShader = VK_TRUE;
			physicalDeviceFeatReq.tessellationShader = VK_TRUE;

			// create logical device
			vkCreateDevice(physicalDevice, logicalDeviceInfoPtr, logicalDeviceAllocPtr, &logicalDevice);
			if (logicalDevice)
			{
				printf(" Vulkan logical device created. \n");

				// done
				result = 0;
			}
			else
			{
				printf(" Logical device creation failed. \n");
				vkDestroyInstance(inst, instAllocPtr);
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

			// set persistent data
			renderer->data[cvkRendererData_instance] = inst;
			renderer->data[cvkRendererData_logicalDevice] = logicalDevice;

			// set persistent management functions
			renderer->func = malloc(cvkRendererData_count * sizeof(VkAllocationCallbacks));
			*((VkAllocationCallbacks*)renderer->func + cvkRendererData_instance) = instAlloc;
			*((VkAllocationCallbacks*)renderer->func + cvkRendererData_logicalDevice) = logicalDeviceAlloc;
		}
		return result;
	}
	return -1;
}


int cvkRendererRelease(cvkRenderer* const renderer)
{
	if (renderer && renderer->init)
	{
		int result = 0;

		// retrieve logical device
		VkDevice logicalDevice = (VkDevice)renderer->data[cvkRendererData_logicalDevice];
		VkAllocationCallbacks const* logicalDeviceAlloc = NULL;// (VkAllocationCallbacks*)renderer->func + cvkRendererData_logicalDevice;

		// retrieve instance
		VkInstance inst = (VkInstance)renderer->data[cvkRendererData_instance];
		VkAllocationCallbacks const* instAlloc = NULL;// (VkAllocationCallbacks*)renderer->func + cvkRendererData_instance;

		// destroy data
		vkDestroyDevice(logicalDevice, logicalDeviceAlloc);
		vkDestroyInstance(inst, instAlloc);

		// done
		if (!result)
		{
			// reset persistent management functions
			free(renderer->func);
			renderer->func = NULL;

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
