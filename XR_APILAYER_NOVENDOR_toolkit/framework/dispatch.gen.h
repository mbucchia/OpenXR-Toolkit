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

#pragma once

#ifndef LAYER_NAMESPACE
#error Must define LAYER_NAMESPACE
#endif

namespace LAYER_NAMESPACE
{

	class OpenXrApi
	{
	private:
		XrInstance m_instance{ XR_NULL_HANDLE };
		std::string m_applicationName;
		std::vector<std::string> m_upstreamLayers;

	protected:
		OpenXrApi() = default;

		PFN_xrGetInstanceProcAddr m_xrGetInstanceProcAddr{ nullptr };

	public:
		virtual ~OpenXrApi() = default;

		XrInstance GetXrInstance() const
		{
			return m_instance;
		}

		const std::string& GetApplicationName() const
		{
			return m_applicationName;
		}

		void SetGetInstanceProcAddr(PFN_xrGetInstanceProcAddr pfn_xrGetInstanceProcAddr, XrInstance instance)
		{
			m_xrGetInstanceProcAddr = pfn_xrGetInstanceProcAddr;
			m_instance = instance;
		}

		void SetUpstreamLayers(std::vector<std::string>& upstreamLayers)
		{
			m_upstreamLayers = upstreamLayers;
		}

		const std::vector<std::string>& GetUpstreamLayers() const
		{
			return m_upstreamLayers;
		}

		// Specially-handled by the auto-generated code.
		virtual XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);
		virtual XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo);


		// Auto-generated entries for the requested APIs.

	public:
		virtual XrResult xrDestroyInstance(XrInstance instance)
		{
			return m_xrDestroyInstance(instance);
		}
	private:
		PFN_xrDestroyInstance m_xrDestroyInstance{ nullptr };

	public:
		virtual XrResult xrGetInstanceProperties(XrInstance instance, XrInstanceProperties* instanceProperties)
		{
			return m_xrGetInstanceProperties(instance, instanceProperties);
		}
	private:
		PFN_xrGetInstanceProperties m_xrGetInstanceProperties{ nullptr };

	public:
		virtual XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData)
		{
			return m_xrPollEvent(instance, eventData);
		}
	private:
		PFN_xrPollEvent m_xrPollEvent{ nullptr };

	public:
		virtual XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId)
		{
			return m_xrGetSystem(instance, getInfo, systemId);
		}
	private:
		PFN_xrGetSystem m_xrGetSystem{ nullptr };

	public:
		virtual XrResult xrGetSystemProperties(XrInstance instance, XrSystemId systemId, XrSystemProperties* properties)
		{
			return m_xrGetSystemProperties(instance, systemId, properties);
		}
	private:
		PFN_xrGetSystemProperties m_xrGetSystemProperties{ nullptr };

	public:
		virtual XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
		{
			return m_xrCreateSession(instance, createInfo, session);
		}
	private:
		PFN_xrCreateSession m_xrCreateSession{ nullptr };

	public:
		virtual XrResult xrDestroySession(XrSession session)
		{
			return m_xrDestroySession(session);
		}
	private:
		PFN_xrDestroySession m_xrDestroySession{ nullptr };

	public:
		virtual XrResult xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
		{
			return m_xrCreateReferenceSpace(session, createInfo, space);
		}
	private:
		PFN_xrCreateReferenceSpace m_xrCreateReferenceSpace{ nullptr };

	public:
		virtual XrResult xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
		{
			return m_xrCreateActionSpace(session, createInfo, space);
		}
	private:
		PFN_xrCreateActionSpace m_xrCreateActionSpace{ nullptr };

	public:
		virtual XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
		{
			return m_xrLocateSpace(space, baseSpace, time, location);
		}
	private:
		PFN_xrLocateSpace m_xrLocateSpace{ nullptr };

	public:
		virtual XrResult xrDestroySpace(XrSpace space)
		{
			return m_xrDestroySpace(space);
		}
	private:
		PFN_xrDestroySpace m_xrDestroySpace{ nullptr };

	public:
		virtual XrResult xrEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrViewConfigurationView* views)
		{
			return m_xrEnumerateViewConfigurationViews(instance, systemId, viewConfigurationType, viewCapacityInput, viewCountOutput, views);
		}
	private:
		PFN_xrEnumerateViewConfigurationViews m_xrEnumerateViewConfigurationViews{ nullptr };

	public:
		virtual XrResult xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
		{
			return m_xrCreateSwapchain(session, createInfo, swapchain);
		}
	private:
		PFN_xrCreateSwapchain m_xrCreateSwapchain{ nullptr };

	public:
		virtual XrResult xrDestroySwapchain(XrSwapchain swapchain)
		{
			return m_xrDestroySwapchain(swapchain);
		}
	private:
		PFN_xrDestroySwapchain m_xrDestroySwapchain{ nullptr };

	public:
		virtual XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images)
		{
			return m_xrEnumerateSwapchainImages(swapchain, imageCapacityInput, imageCountOutput, images);
		}
	private:
		PFN_xrEnumerateSwapchainImages m_xrEnumerateSwapchainImages{ nullptr };

	public:
		virtual XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index)
		{
			return m_xrAcquireSwapchainImage(swapchain, acquireInfo, index);
		}
	private:
		PFN_xrAcquireSwapchainImage m_xrAcquireSwapchainImage{ nullptr };

	public:
		virtual XrResult xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo)
		{
			return m_xrReleaseSwapchainImage(swapchain, releaseInfo);
		}
	private:
		PFN_xrReleaseSwapchainImage m_xrReleaseSwapchainImage{ nullptr };

	public:
		virtual XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState)
		{
			return m_xrWaitFrame(session, frameWaitInfo, frameState);
		}
	private:
		PFN_xrWaitFrame m_xrWaitFrame{ nullptr };

	public:
		virtual XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo)
		{
			return m_xrBeginFrame(session, frameBeginInfo);
		}
	private:
		PFN_xrBeginFrame m_xrBeginFrame{ nullptr };

	public:
		virtual XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo)
		{
			return m_xrEndFrame(session, frameEndInfo);
		}
	private:
		PFN_xrEndFrame m_xrEndFrame{ nullptr };

	public:
		virtual XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
		{
			return m_xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
		}
	private:
		PFN_xrLocateViews m_xrLocateViews{ nullptr };

	public:
		virtual XrResult xrStringToPath(XrInstance instance, const char* pathString, XrPath* path)
		{
			return m_xrStringToPath(instance, pathString, path);
		}
	private:
		PFN_xrStringToPath m_xrStringToPath{ nullptr };

	public:
		virtual XrResult xrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
		{
			return m_xrPathToString(instance, path, bufferCapacityInput, bufferCountOutput, buffer);
		}
	private:
		PFN_xrPathToString m_xrPathToString{ nullptr };

	public:
		virtual XrResult xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet)
		{
			return m_xrCreateActionSet(instance, createInfo, actionSet);
		}
	private:
		PFN_xrCreateActionSet m_xrCreateActionSet{ nullptr };

	public:
		virtual XrResult xrDestroyActionSet(XrActionSet actionSet)
		{
			return m_xrDestroyActionSet(actionSet);
		}
	private:
		PFN_xrDestroyActionSet m_xrDestroyActionSet{ nullptr };

	public:
		virtual XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action)
		{
			return m_xrCreateAction(actionSet, createInfo, action);
		}
	private:
		PFN_xrCreateAction m_xrCreateAction{ nullptr };

	public:
		virtual XrResult xrDestroyAction(XrAction action)
		{
			return m_xrDestroyAction(action);
		}
	private:
		PFN_xrDestroyAction m_xrDestroyAction{ nullptr };

	public:
		virtual XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings)
		{
			return m_xrSuggestInteractionProfileBindings(instance, suggestedBindings);
		}
	private:
		PFN_xrSuggestInteractionProfileBindings m_xrSuggestInteractionProfileBindings{ nullptr };

	public:
		virtual XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
		{
			return m_xrAttachSessionActionSets(session, attachInfo);
		}
	private:
		PFN_xrAttachSessionActionSets m_xrAttachSessionActionSets{ nullptr };

	public:
		virtual XrResult xrGetCurrentInteractionProfile(XrSession session, XrPath topLevelUserPath, XrInteractionProfileState* interactionProfile)
		{
			return m_xrGetCurrentInteractionProfile(session, topLevelUserPath, interactionProfile);
		}
	private:
		PFN_xrGetCurrentInteractionProfile m_xrGetCurrentInteractionProfile{ nullptr };

	public:
		virtual XrResult xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state)
		{
			return m_xrGetActionStateBoolean(session, getInfo, state);
		}
	private:
		PFN_xrGetActionStateBoolean m_xrGetActionStateBoolean{ nullptr };

	public:
		virtual XrResult xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state)
		{
			return m_xrGetActionStateFloat(session, getInfo, state);
		}
	private:
		PFN_xrGetActionStateFloat m_xrGetActionStateFloat{ nullptr };

	public:
		virtual XrResult xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state)
		{
			return m_xrGetActionStatePose(session, getInfo, state);
		}
	private:
		PFN_xrGetActionStatePose m_xrGetActionStatePose{ nullptr };

	public:
		virtual XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
		{
			return m_xrSyncActions(session, syncInfo);
		}
	private:
		PFN_xrSyncActions m_xrSyncActions{ nullptr };

	public:
		virtual XrResult xrApplyHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback)
		{
			return m_xrApplyHapticFeedback(session, hapticActionInfo, hapticFeedback);
		}
	private:
		PFN_xrApplyHapticFeedback m_xrApplyHapticFeedback{ nullptr };

	public:
		virtual XrResult xrStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo)
		{
			return m_xrStopHapticFeedback(session, hapticActionInfo);
		}
	private:
		PFN_xrStopHapticFeedback m_xrStopHapticFeedback{ nullptr };



	};

} // namespace LAYER_NAMESPACE

