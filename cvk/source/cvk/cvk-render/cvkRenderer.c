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

int cvkRendererCreate(cvkRenderer* const renderer)
{
	if (renderer && !renderer->init)
	{
		int result = 0;
		ui32 i = 0, j = 0;

		// data for layers and extensions
		ui32 instVersion = 0, nInstLayer = 0, nInstExt = 0;
		VkLayerProperties* instLayer = NULL;
		VkExtensionProperties** instExt = NULL;

		// data for instance
		VkInstance inst = { 0 };
		VkApplicationInfo appInfo = {
			VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
			"cvkTest", 0, "cvk", 0, 0,
		};
		kstr layerInfo[] = {
			"VK_LAYER_KHRONOS_validation",
			//"VK_LAYER_LUNARG_standard_validation",
			//"VK_LAYER_LUNARG_object_tracker",
		};
		kstr extInfo[] = {
			VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else	// !_WIN32
#endif	// _WIN32
			"VK_EXT_debug_report",
			"VK_EXT_debug_utils",
			"VK_EXT_validation_features",
		};
		VkInstanceCreateInfo instInfo = {
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &appInfo,
			(sizeof(layerInfo) / sizeof(*layerInfo)), layerInfo,
			(sizeof(extInfo) / sizeof(*extInfo)), extInfo,
		};
		VkAllocationCallbacks instAlloc = { 0 }, * instAllocPtr = NULL;

		printf("cvkRendererCreate \n");

		// enumerate instance version
		vkEnumerateInstanceVersion(&instVersion);
		printf("  instVersion = %u \n", instVersion);

		// enumerate instance layers
		vkEnumerateInstanceLayerProperties(&nInstLayer, NULL);
		printf("  nInstLayer = %u \n", nInstLayer);
		if (nInstLayer)
		{
			// allocate space for layer info
			printf("  \"layerName\" (specVersion.implementationVersion) \"description\" \n");
			instLayer = (VkLayerProperties*)malloc(nInstLayer * sizeof(VkLayerProperties));
			instExt = (VkExtensionProperties**)malloc(nInstLayer * sizeof(VkExtensionProperties*));
			vkEnumerateInstanceLayerProperties(&nInstLayer, instLayer);
			for (i = 0; i < nInstLayer; ++i)
			{
				// enumerate instance extensions for each instance layer
				vkEnumerateInstanceExtensionProperties(instLayer[i].layerName, &nInstExt, NULL);
				printf("  instLayer[%u] = \"%s\" (%u.%u): \"%s\" \n", i,
					instLayer[i].layerName,
					instLayer[i].specVersion, instLayer[i].implementationVersion,
					instLayer[i].description);
				printf("    nInstExt = %u \n", nInstExt);
				if (nInstExt)
				{
					// allocate space for extension info
					printf("    \"extensionName\" (specVersion) \n");
					instExt[i] = (VkExtensionProperties*)malloc(nInstExt * sizeof(VkExtensionProperties));
					vkEnumerateInstanceExtensionProperties(instLayer[i].layerName, &nInstExt, instExt[i]);
					for (j = 0; j < nInstExt; ++j)
					{
						printf("    instExt[%u][%u] = \"%s\" (%u) \n", i, j,
							instExt[i][j].extensionName, instExt[i][j].specVersion);
					}
					free(instExt[i]);
				}
			}
			free(instExt);
			free(instLayer);
			instExt = NULL;
			instLayer = NULL;
		}

		// create instance
		vkCreateInstance(&instInfo, instAllocPtr, &inst);
		if (inst)
		{

			vkDestroyInstance(inst, instAllocPtr);
		}

		// done
		if (!result)
			renderer->init = true;
		return result;
	}
	return -1;
}


int cvkRendererRelease(cvkRenderer* const renderer)
{
	if (renderer && renderer->init)
	{
		int result = 0;

		// done
		if (!result)
			renderer->init = false;
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
