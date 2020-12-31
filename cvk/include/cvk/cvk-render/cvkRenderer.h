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

	cvkRenderer.h
	Main renderer header.
*/

#ifndef _CVK_RENDERER_H_
#define _CVK_RENDERER_H_

#include "cvk/cvk/cvk-types.h"

#ifdef __cplusplus
extern "C" {
#endif	// __cplusplus


//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------

// cvkRenderer
//	Renderer organizational structure.
//		member init: initialized flag
//		member data: internal data
typedef struct cvkRenderer
{
	bool init;		// Initialized flag.
	ptr data[4];	// Internal data.
} cvkRenderer;


//-----------------------------------------------------------------------------

// cvkRendererCreate
//	Create and initialize renderer.
//		param renderer: pointer to renderer object
//			valid: non-null, uninitialized
//		return SUCCESS: 0 if renderer initialized
//		return FAILURE: -1 if invalid parameters
//		return FAILURE: -2 if renderer not initialized
int cvkRendererCreate(cvkRenderer* const renderer);

// cvkRendererRelease
//	Terminate and release renderer.
//		param renderer: pointer to renderer object
//			valid: non-null, initialized
//		return SUCCESS: 0 if renderer terminated
//		return FAILURE: -1 if invalid parameters
//		return FAILURE: -2 if renderer not terminated
int cvkRendererRelease(cvkRenderer* const renderer);

// cvkRendererTest
//	Complete testing program before breaking down interface.
//		param renderer: pointer to renderer object
//			valid: non-null, initialized
//		return SUCCESS: 0 if renderer terminated
//		return FAILURE: -1 if invalid parameters
//		return FAILURE: -2 if renderer not terminated
int cvkRendererTest(cvkRenderer* const renderer);


//-----------------------------------------------------------------------------


#ifdef __cplusplus
}
#endif	// __cplusplus


#endif	// !_CVK_RENDERER_H_