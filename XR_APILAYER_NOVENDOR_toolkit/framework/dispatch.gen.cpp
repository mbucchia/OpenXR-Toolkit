// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********
// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
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
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrPollEvent");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrPollEvent(instance, eventData);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrPollEvent_Error", TLArg(exc.what(), "Error"));
			Log("xrPollEvent: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrPollEvent", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrGetSystem");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetSystem(instance, getInfo, systemId);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrGetSystem_Error", TLArg(exc.what(), "Error"));
			Log("xrGetSystem: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrGetSystem", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrCreateSession");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrCreateSession(instance, createInfo, session);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrCreateSession_Error", TLArg(exc.what(), "Error"));
			Log("xrCreateSession: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrCreateSession", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrDestroySession(XrSession session)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrDestroySession");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrDestroySession(session);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrDestroySession_Error", TLArg(exc.what(), "Error"));
			Log("xrDestroySession: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrDestroySession", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrCreateActionSpace");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrCreateActionSpace(session, createInfo, space);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrCreateActionSpace_Error", TLArg(exc.what(), "Error"));
			Log("xrCreateActionSpace: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrCreateActionSpace", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrLocateSpace");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrLocateSpace(space, baseSpace, time, location);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrLocateSpace_Error", TLArg(exc.what(), "Error"));
			Log("xrLocateSpace: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrLocateSpace", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrDestroySpace(XrSpace space)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrDestroySpace");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrDestroySpace(space);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrDestroySpace_Error", TLArg(exc.what(), "Error"));
			Log("xrDestroySpace: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrDestroySpace", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrEnumerateViewConfigurationViews");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrEnumerateViewConfigurationViews(instance, systemId, viewConfigurationType, viewCapacityInput, viewCountOutput, views);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrEnumerateViewConfigurationViews_Error", TLArg(exc.what(), "Error"));
			Log("xrEnumerateViewConfigurationViews: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrEnumerateViewConfigurationViews", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrCreateSwapchain");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrCreateSwapchain(session, createInfo, swapchain);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrCreateSwapchain_Error", TLArg(exc.what(), "Error"));
			Log("xrCreateSwapchain: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrCreateSwapchain", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrDestroySwapchain(XrSwapchain swapchain)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrDestroySwapchain");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrDestroySwapchain(swapchain);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrDestroySwapchain_Error", TLArg(exc.what(), "Error"));
			Log("xrDestroySwapchain: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrDestroySwapchain", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrEnumerateSwapchainImages");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrEnumerateSwapchainImages(swapchain, imageCapacityInput, imageCountOutput, images);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrEnumerateSwapchainImages_Error", TLArg(exc.what(), "Error"));
			Log("xrEnumerateSwapchainImages: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrEnumerateSwapchainImages", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrAcquireSwapchainImage");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrAcquireSwapchainImage(swapchain, acquireInfo, index);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrAcquireSwapchainImage_Error", TLArg(exc.what(), "Error"));
			Log("xrAcquireSwapchainImage: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrAcquireSwapchainImage", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrReleaseSwapchainImage");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrReleaseSwapchainImage(swapchain, releaseInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrReleaseSwapchainImage_Error", TLArg(exc.what(), "Error"));
			Log("xrReleaseSwapchainImage: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrReleaseSwapchainImage", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrBeginSession");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrBeginSession(session, beginInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrBeginSession_Error", TLArg(exc.what(), "Error"));
			Log("xrBeginSession: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrBeginSession", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrEndSession(XrSession session)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrEndSession");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrEndSession(session);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrEndSession_Error", TLArg(exc.what(), "Error"));
			Log("xrEndSession: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrEndSession", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrWaitFrame");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrWaitFrame(session, frameWaitInfo, frameState);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrWaitFrame_Error", TLArg(exc.what(), "Error"));
			Log("xrWaitFrame: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrWaitFrame", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrBeginFrame");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrBeginFrame(session, frameBeginInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrBeginFrame_Error", TLArg(exc.what(), "Error"));
			Log("xrBeginFrame: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrBeginFrame", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrEndFrame");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrEndFrame(session, frameEndInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrEndFrame_Error", TLArg(exc.what(), "Error"));
			Log("xrEndFrame: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrEndFrame", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrLocateViews");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrLocateViews_Error", TLArg(exc.what(), "Error"));
			Log("xrLocateViews: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrLocateViews", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrCreateAction");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrCreateAction(actionSet, createInfo, action);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrCreateAction_Error", TLArg(exc.what(), "Error"));
			Log("xrCreateAction: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrCreateAction", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrDestroyAction(XrAction action)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrDestroyAction");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrDestroyAction(action);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrDestroyAction_Error", TLArg(exc.what(), "Error"));
			Log("xrDestroyAction: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrDestroyAction", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrSuggestInteractionProfileBindings");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrSuggestInteractionProfileBindings(instance, suggestedBindings);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrSuggestInteractionProfileBindings_Error", TLArg(exc.what(), "Error"));
			Log("xrSuggestInteractionProfileBindings: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrSuggestInteractionProfileBindings", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrAttachSessionActionSets");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrAttachSessionActionSets(session, attachInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrAttachSessionActionSets_Error", TLArg(exc.what(), "Error"));
			Log("xrAttachSessionActionSets: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrAttachSessionActionSets", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrGetCurrentInteractionProfile");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrGetCurrentInteractionProfile_Error", TLArg(exc.what(), "Error"));
			Log("xrGetCurrentInteractionProfile: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrGetCurrentInteractionProfile", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrGetActionStateBoolean");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetActionStateBoolean(session, getInfo, state);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrGetActionStateBoolean_Error", TLArg(exc.what(), "Error"));
			Log("xrGetActionStateBoolean: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrGetActionStateBoolean", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrGetActionStateFloat");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetActionStateFloat(session, getInfo, state);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrGetActionStateFloat_Error", TLArg(exc.what(), "Error"));
			Log("xrGetActionStateFloat: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrGetActionStateFloat", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrGetActionStatePose");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetActionStatePose(session, getInfo, state);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrGetActionStatePose_Error", TLArg(exc.what(), "Error"));
			Log("xrGetActionStatePose: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrGetActionStatePose", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrSyncActions");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrSyncActions(session, syncInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrSyncActions_Error", TLArg(exc.what(), "Error"));
			Log("xrSyncActions: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrSyncActions", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrApplyHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrApplyHapticFeedback");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrApplyHapticFeedback(session, hapticActionInfo, hapticFeedback);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrApplyHapticFeedback_Error", TLArg(exc.what(), "Error"));
			Log("xrApplyHapticFeedback: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrApplyHapticFeedback", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrStopHapticFeedback");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrStopHapticFeedback(session, hapticActionInfo);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrStopHapticFeedback_Error", TLArg(exc.what(), "Error"));
			Log("xrStopHapticFeedback: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrStopHapticFeedback", TLArg((int)result, "Result"));

		return result;
	}

	XrResult xrGetVisibilityMaskKHR(XrSession session, XrViewConfigurationType viewConfigurationType, uint32_t viewIndex, XrVisibilityMaskTypeKHR visibilityMaskType, XrVisibilityMaskKHR* visibilityMask)
	{
		TraceLocalActivity(local);
		TraceLoggingWriteStart(local, "xrGetVisibilityMaskKHR");

		XrResult result;
		try
		{
			result = LAYER_NAMESPACE::GetInstance()->xrGetVisibilityMaskKHR(session, viewConfigurationType, viewIndex, visibilityMaskType, visibilityMask);
		}
		catch (std::exception& exc)
		{
			TraceLoggingWriteTagged(local, "xrGetVisibilityMaskKHR_Error", TLArg(exc.what(), "Error"));
			Log("xrGetVisibilityMaskKHR: %s\n", exc.what());
			result = XR_ERROR_RUNTIME_FAILURE;
		}

		TraceLoggingWriteStop(local, "xrGetVisibilityMaskKHR", TLArg((int)result, "Result"));

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
			else if (apiName == "xrReleaseSwapchainImage")
			{
				m_xrReleaseSwapchainImage = reinterpret_cast<PFN_xrReleaseSwapchainImage>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrReleaseSwapchainImage);
			}
			else if (apiName == "xrBeginSession")
			{
				m_xrBeginSession = reinterpret_cast<PFN_xrBeginSession>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrBeginSession);
			}
			else if (apiName == "xrEndSession")
			{
				m_xrEndSession = reinterpret_cast<PFN_xrEndSession>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrEndSession);
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
			else if (apiName == "xrAttachSessionActionSets")
			{
				m_xrAttachSessionActionSets = reinterpret_cast<PFN_xrAttachSessionActionSets>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrAttachSessionActionSets);
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
			else if (apiName == "xrApplyHapticFeedback")
			{
				m_xrApplyHapticFeedback = reinterpret_cast<PFN_xrApplyHapticFeedback>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrApplyHapticFeedback);
			}
			else if (apiName == "xrStopHapticFeedback")
			{
				m_xrStopHapticFeedback = reinterpret_cast<PFN_xrStopHapticFeedback>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrStopHapticFeedback);
			}
			else if (apiName == "xrGetVisibilityMaskKHR")
			{
				m_xrGetVisibilityMaskKHR = reinterpret_cast<PFN_xrGetVisibilityMaskKHR>(*function);
				*function = reinterpret_cast<PFN_xrVoidFunction>(LAYER_NAMESPACE::xrGetVisibilityMaskKHR);
			}

		}

		return result;
	}

	// Auto-generated create instance handler.
	XrResult OpenXrApi::xrCreateInstance(const XrInstanceCreateInfo* createInfo)
    {
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetInstanceProperties", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetInstanceProperties))))
		{
			throw std::runtime_error("Failed to resolve xrGetInstanceProperties");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetSystem", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetSystem))))
		{
			throw std::runtime_error("Failed to resolve xrGetSystem");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetSystemProperties", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetSystemProperties))))
		{
			throw std::runtime_error("Failed to resolve xrGetSystemProperties");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateReferenceSpace", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateReferenceSpace))))
		{
			throw std::runtime_error("Failed to resolve xrCreateReferenceSpace");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateActionSpace", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateActionSpace))))
		{
			throw std::runtime_error("Failed to resolve xrCreateActionSpace");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrLocateSpace", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrLocateSpace))))
		{
			throw std::runtime_error("Failed to resolve xrLocateSpace");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrDestroySpace", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroySpace))))
		{
			throw std::runtime_error("Failed to resolve xrDestroySpace");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrEnumerateViewConfigurationViews", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrEnumerateViewConfigurationViews))))
		{
			throw std::runtime_error("Failed to resolve xrEnumerateViewConfigurationViews");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrEnumerateSwapchainFormats", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrEnumerateSwapchainFormats))))
		{
			throw std::runtime_error("Failed to resolve xrEnumerateSwapchainFormats");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateSwapchain", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateSwapchain))))
		{
			throw std::runtime_error("Failed to resolve xrCreateSwapchain");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrEnumerateSwapchainImages", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrEnumerateSwapchainImages))))
		{
			throw std::runtime_error("Failed to resolve xrEnumerateSwapchainImages");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrAcquireSwapchainImage", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrAcquireSwapchainImage))))
		{
			throw std::runtime_error("Failed to resolve xrAcquireSwapchainImage");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrWaitSwapchainImage", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrWaitSwapchainImage))))
		{
			throw std::runtime_error("Failed to resolve xrWaitSwapchainImage");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrReleaseSwapchainImage", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrReleaseSwapchainImage))))
		{
			throw std::runtime_error("Failed to resolve xrReleaseSwapchainImage");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrStringToPath", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrStringToPath))))
		{
			throw std::runtime_error("Failed to resolve xrStringToPath");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrPathToString", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrPathToString))))
		{
			throw std::runtime_error("Failed to resolve xrPathToString");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateActionSet", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateActionSet))))
		{
			throw std::runtime_error("Failed to resolve xrCreateActionSet");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrDestroyActionSet", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroyActionSet))))
		{
			throw std::runtime_error("Failed to resolve xrDestroyActionSet");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrCreateAction", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateAction))))
		{
			throw std::runtime_error("Failed to resolve xrCreateAction");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrDestroyAction", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroyAction))))
		{
			throw std::runtime_error("Failed to resolve xrDestroyAction");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrGetActionStatePose", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrGetActionStatePose))))
		{
			throw std::runtime_error("Failed to resolve xrGetActionStatePose");
		}
		if (XR_FAILED(m_xrGetInstanceProcAddr(m_instance, "xrSyncActions", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrSyncActions))))
		{
			throw std::runtime_error("Failed to resolve xrSyncActions");
		}
		m_xrGetInstanceProcAddr(m_instance, "xrConvertWin32PerformanceCounterToTimeKHR", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrConvertWin32PerformanceCounterToTimeKHR));
		m_xrGetInstanceProcAddr(m_instance, "xrCreateHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrCreateHandTrackerEXT));
		m_xrGetInstanceProcAddr(m_instance, "xrDestroyHandTrackerEXT", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrDestroyHandTrackerEXT));
		m_xrGetInstanceProcAddr(m_instance, "xrLocateHandJointsEXT", reinterpret_cast<PFN_xrVoidFunction*>(&m_xrLocateHandJointsEXT));
		m_applicationName = createInfo->applicationInfo.applicationName;
		return XR_SUCCESS;
	}

} // namespace LAYER_NAMESPACE

