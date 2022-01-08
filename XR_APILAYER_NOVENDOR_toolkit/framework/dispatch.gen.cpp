// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********
// MIT License
//
// Copyright(c) 2021 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pch.h"

#include <layer.h>

#include "dispatch.h"
#include "log.h"

#ifndef LAYER_NAMESPACE
#error Must define LAYER_NAMESPACE
#endif

using namespace LAYER_NAMESPACE::log;

namespace LAYER_NAMESPACE
{

	// Auto-generated wrappers for the requested APIs.

	XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
	{
		DebugLog("--> xrPollEvent\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrPollEvent(instance, eventData);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrPollEvent %d\n", result);

		return result;
	}

	XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId)
	{
		DebugLog("--> xrGetSystem\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetSystem(instance, getInfo, systemId);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrGetSystem %d\n", result);

		return result;
	}

	XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
	{
		DebugLog("--> xrCreateSession\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrCreateSession(instance, createInfo, session);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrCreateSession %d\n", result);

		return result;
	}

	XrResult xrDestroySession(XrSession session)
	{
		DebugLog("--> xrDestroySession\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrDestroySession(session);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrDestroySession %d\n", result);

		return result;
	}

	XrResult xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
	{
		DebugLog("--> xrCreateActionSpace\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrCreateActionSpace(session, createInfo, space);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrCreateActionSpace %d\n", result);

		return result;
	}

	XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
	{
		DebugLog("--> xrLocateSpace\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrLocateSpace(space, baseSpace, time, location);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrLocateSpace %d\n", result);

		return result;
	}

	XrResult xrDestroySpace(XrSpace space)
	{
		DebugLog("--> xrDestroySpace\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrDestroySpace(space);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrDestroySpace %d\n", result);

		return result;
	}

	XrResult xrEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views)
	{
		DebugLog("--> xrEnumerateViewConfigurationViews\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrEnumerateViewConfigurationViews(instance, systemId, viewConfigurationType, viewCapacityInput, viewCountOutput, views);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrEnumerateViewConfigurationViews %d\n", result);

		return result;
	}

	XrResult xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
	{
		DebugLog("--> xrCreateSwapchain\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrCreateSwapchain(session, createInfo, swapchain);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrCreateSwapchain %d\n", result);

		return result;
	}

	XrResult xrDestroySwapchain(XrSwapchain swapchain)
	{
		DebugLog("--> xrDestroySwapchain\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrDestroySwapchain(swapchain);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrDestroySwapchain %d\n", result);

		return result;
	}

	XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images)
	{
		DebugLog("--> xrEnumerateSwapchainImages\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrEnumerateSwapchainImages(swapchain, imageCapacityInput, imageCountOutput, images);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrEnumerateSwapchainImages %d\n", result);

		return result;
	}

	XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index)
	{
		DebugLog("--> xrAcquireSwapchainImage\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrAcquireSwapchainImage(swapchain, acquireInfo, index);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrAcquireSwapchainImage %d\n", result);

		return result;
	}

	XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState)
	{
		DebugLog("--> xrWaitFrame\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrWaitFrame(session, frameWaitInfo, frameState);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrWaitFrame %d\n", result);

		return result;
	}

	XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo)
	{
		DebugLog("--> xrBeginFrame\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrBeginFrame(session, frameBeginInfo);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrBeginFrame %d\n", result);

		return result;
	}

	XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
	{
		DebugLog("--> xrEndFrame\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrEndFrame(session, frameEndInfo);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrEndFrame %d\n", result);

		return result;
	}

	XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
	{
		DebugLog("--> xrLocateViews\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrLocateViews %d\n", result);

		return result;
	}

	XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action)
	{
		DebugLog("--> xrCreateAction\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrCreateAction(actionSet, createInfo, action);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrCreateAction %d\n", result);

		return result;
	}

	XrResult xrDestroyAction(XrAction action)
	{
		DebugLog("--> xrDestroyAction\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrDestroyAction(action);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrDestroyAction %d\n", result);

		return result;
	}

	XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings)
	{
		DebugLog("--> xrSuggestInteractionProfileBindings\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrSuggestInteractionProfileBindings(instance, suggestedBindings);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrSuggestInteractionProfileBindings %d\n", result);

		return result;
	}

	XrResult xrGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile)
	{
		DebugLog("--> xrGetCurrentInteractionProfile\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrGetCurrentInteractionProfile %d\n", result);

		return result;
	}

	XrResult xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state)
	{
		DebugLog("--> xrGetActionStateBoolean\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetActionStateBoolean(session, getInfo, state);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrGetActionStateBoolean %d\n", result);

		return result;
	}

	XrResult xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state)
	{
		DebugLog("--> xrGetActionStateFloat\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetActionStateFloat(session, getInfo, state);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrGetActionStateFloat %d\n", result);

		return result;
	}

	XrResult xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state)
	{
		DebugLog("--> xrGetActionStatePose\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetActionStatePose(session, getInfo, state);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrGetActionStatePose %d\n", result);

		return result;
	}

	XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
	{
		DebugLog("--> xrSyncActions\n");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrSyncActions(session, syncInfo);
		}
		catch (std::exception exc)
		{
			Log("%s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		DebugLog("<-- xrSyncActions %d\n", result);

		return result;
	}


	// Auto-generated dispatcher handler.
	XrResult OpenXrApi::xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function)
	{
		XrResult result = m_xrGetInstanceProcAddr(instance, name, function);

		if (XR_SUCCEEDED(result))
		{
			const std::string apiName(name);

			if (apiName == "xrDestroyInstance")
			{
				m_xrDestroyInstance = reinterpret_cast<PFN_xrDestroyInstance>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrDestroyInstance);
			}
			else if (apiName == "xrPollEvent")
			{
				m_xrPollEvent = reinterpret_cast<PFN_xrPollEvent>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrPollEvent);
			}
			else if (apiName == "xrGetSystem")
			{
				m_xrGetSystem = reinterpret_cast<PFN_xrGetSystem>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrGetSystem);
			}
			else if (apiName == "xrCreateSession")
			{
				m_xrCreateSession = reinterpret_cast<PFN_xrCreateSession>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrCreateSession);
			}
			else if (apiName == "xrDestroySession")
			{
				m_xrDestroySession = reinterpret_cast<PFN_xrDestroySession>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrDestroySession);
			}
			else if (apiName == "xrCreateActionSpace")
			{
				m_xrCreateActionSpace = reinterpret_cast<PFN_xrCreateActionSpace>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrCreateActionSpace);
			}
			else if (apiName == "xrLocateSpace")
			{
				m_xrLocateSpace = reinterpret_cast<PFN_xrLocateSpace>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrLocateSpace);
			}
			else if (apiName == "xrDestroySpace")
			{
				m_xrDestroySpace = reinterpret_cast<PFN_xrDestroySpace>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrDestroySpace);
			}
			else if (apiName == "xrEnumerateViewConfigurationViews")
			{
				m_xrEnumerateViewConfigurationViews = reinterpret_cast<PFN_xrEnumerateViewConfigurationViews>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrEnumerateViewConfigurationViews);
			}
			else if (apiName == "xrCreateSwapchain")
			{
				m_xrCreateSwapchain = reinterpret_cast<PFN_xrCreateSwapchain>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrCreateSwapchain);
			}
			else if (apiName == "xrDestroySwapchain")
			{
				m_xrDestroySwapchain = reinterpret_cast<PFN_xrDestroySwapchain>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrDestroySwapchain);
			}
			else if (apiName == "xrEnumerateSwapchainImages")
			{
				m_xrEnumerateSwapchainImages = reinterpret_cast<PFN_xrEnumerateSwapchainImages>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrEnumerateSwapchainImages);
			}
			else if (apiName == "xrAcquireSwapchainImage")
			{
				m_xrAcquireSwapchainImage = reinterpret_cast<PFN_xrAcquireSwapchainImage>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrAcquireSwapchainImage);
			}
			else if (apiName == "xrWaitFrame")
			{
				m_xrWaitFrame = reinterpret_cast<PFN_xrWaitFrame>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrWaitFrame);
			}
			else if (apiName == "xrBeginFrame")
			{
				m_xrBeginFrame = reinterpret_cast<PFN_xrBeginFrame>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrBeginFrame);
			}
			else if (apiName == "xrEndFrame")
			{
				m_xrEndFrame = reinterpret_cast<PFN_xrEndFrame>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrEndFrame);
			}
			else if (apiName == "xrLocateViews")
			{
				m_xrLocateViews = reinterpret_cast<PFN_xrLocateViews>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrLocateViews);
			}
			else if (apiName == "xrCreateAction")
			{
				m_xrCreateAction = reinterpret_cast<PFN_xrCreateAction>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrCreateAction);
			}
			else if (apiName == "xrDestroyAction")
			{
				m_xrDestroyAction = reinterpret_cast<PFN_xrDestroyAction>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrDestroyAction);
			}
			else if (apiName == "xrSuggestInteractionProfileBindings")
			{
				m_xrSuggestInteractionProfileBindings = reinterpret_cast<PFN_xrSuggestInteractionProfileBindings>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrSuggestInteractionProfileBindings);
			}
			else if (apiName == "xrGetCurrentInteractionProfile")
			{
				m_xrGetCurrentInteractionProfile = reinterpret_cast<PFN_xrGetCurrentInteractionProfile>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrGetCurrentInteractionProfile);
			}
			else if (apiName == "xrGetActionStateBoolean")
			{
				m_xrGetActionStateBoolean = reinterpret_cast<PFN_xrGetActionStateBoolean>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrGetActionStateBoolean);
			}
			else if (apiName == "xrGetActionStateFloat")
			{
				m_xrGetActionStateFloat = reinterpret_cast<PFN_xrGetActionStateFloat>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrGetActionStateFloat);
			}
			else if (apiName == "xrGetActionStatePose")
			{
				m_xrGetActionStatePose = reinterpret_cast<PFN_xrGetActionStatePose>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrGetActionStatePose);
			}
			else if (apiName == "xrSyncActions")
			{
				m_xrSyncActions = reinterpret_cast<PFN_xrSyncActions>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrSyncActions);
			}

		}

		return result;
	}

	// Auto-generated create instance handler.
	XrResult OpenXrApi::xrCreateInstance(const XrInstanceCreateInfo* createInfo)
    {
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetInstanceProperties", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetInstanceProperties))))
		{
			throw new std::runtime_error("Failed to resolve xrGetInstanceProperties");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetSystemProperties", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetSystemProperties))))
		{
			throw new std::runtime_error("Failed to resolve xrGetSystemProperties");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateReferenceSpace", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateReferenceSpace))))
		{
			throw new std::runtime_error("Failed to resolve xrCreateReferenceSpace");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrEnumerateViewConfigurationViews", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrEnumerateViewConfigurationViews))))
		{
			throw new std::runtime_error("Failed to resolve xrEnumerateViewConfigurationViews");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrEnumerateSwapchainImages", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrEnumerateSwapchainImages))))
		{
			throw new std::runtime_error("Failed to resolve xrEnumerateSwapchainImages");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrStringToPath", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrStringToPath))))
		{
			throw new std::runtime_error("Failed to resolve xrStringToPath");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrPathToString", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrPathToString))))
		{
			throw new std::runtime_error("Failed to resolve xrPathToString");
		}
		return XR_SUCCESS;
	}

} // namespace LAYER_NAMESPACE

