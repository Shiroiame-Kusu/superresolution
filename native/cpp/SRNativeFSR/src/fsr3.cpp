#include "sr/sr_api.h"
#include "sr/fsr/fsr3.h"
#include "FidelityFX/host/backends/vk/ffx_vk.h"
#include "FidelityFX/host/ffx_fsr3upscaler.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "sr/fsr/sr_provider.h"
struct SRFsr3PrivateData
{
    FfxInterface *ffxInterface;
    FfxFsr3UpscalerContext *context;
    void *scratchBuffer;
};
#ifdef __cplusplus
extern "C"
{
#endif

    SR_API SRReturnCode srFfxFsr3InitUpscaleContext(SRUpscaleContext *context)
    {
        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3InitUpscaleContext: Enter\n"); fflush(stderr);
        const SRCreateUpscaleContextDesc *desc = &context->desc;
        SRFsr3PrivateData *privateData = (SRFsr3PrivateData *)context->userContext;

        if (!privateData) {
            fprintf(stderr, "[SRNative-FSR3] srFfxFsr3InitUpscaleContext: ERROR privateData is NULL\n"); fflush(stderr);
            return (SRReturnCode)SR_RETURN_CODE_ERROR;
        }
        if (!privateData->context) {
            fprintf(stderr, "[SRNative-FSR3] srFfxFsr3InitUpscaleContext: ERROR privateData->context is NULL\n"); fflush(stderr);
            return (SRReturnCode)SR_RETURN_CODE_ERROR;
        }
        if (!privateData->ffxInterface) {
            fprintf(stderr, "[SRNative-FSR3] srFfxFsr3InitUpscaleContext: ERROR privateData->ffxInterface is NULL\n"); fflush(stderr);
            return (SRReturnCode)SR_RETURN_CODE_ERROR;
        }

        FfxFsr3UpscalerContextDescription fsrContexDesc = {};
        fsrContexDesc.flags = 0;
        if (desc->flags & SR_UPSCALE_CONTEXT_CREATE_FLAG_ENABLE_DEBUG)
        {
            fsrContexDesc.flags |= FFX_FSR3UPSCALER_ENABLE_DEBUG_CHECKING;
        }
        if (desc->flags & SR_UPSCALE_CONTEXT_CREATE_FLAG_ENABLE_AUTO_EXPOSURE)
        {
            fsrContexDesc.flags |= FFX_FSR3UPSCALER_ENABLE_AUTO_EXPOSURE;
        }
        if (desc->flags & SR_UPSCALE_CONTEXT_CREATE_FLAG_ENABLE_DEPTH_INVERTED)
        {
            fsrContexDesc.flags |= FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED;
        }
        if (desc->flags & SR_UPSCALE_CONTEXT_CREATE_FLAG_ENABLE_MOTION_VECTORS_JITTERED)
        {
            fsrContexDesc.flags |= FFX_FSR3UPSCALER_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION;
        }
        fsrContexDesc.backendInterface = *(privateData->ffxInterface);
        fsrContexDesc.maxRenderSize = {desc->renderSize.x, desc->renderSize.y};
        fsrContexDesc.maxUpscaleSize = {desc->upscaledSize.x, desc->upscaledSize.y};
        fsrContexDesc.fpMessage = desc->messageCallback ? reinterpret_cast<FfxFsr3UpscalerMessage>(desc->messageCallback) : nullptr;

        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3InitUpscaleContext: flags=0x%x maxRenderSize=%ux%u maxUpscaleSize=%ux%u\n",
                fsrContexDesc.flags, desc->renderSize.x, desc->renderSize.y, desc->upscaledSize.x, desc->upscaledSize.y);
        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3InitUpscaleContext: Calling ffxFsr3UpscalerContextCreate...\n"); fflush(stderr);

        FfxErrorCode code = ffxFsr3UpscalerContextCreate(privateData->context, &fsrContexDesc);

        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3InitUpscaleContext: ffxFsr3UpscalerContextCreate returned %d\n", (int)code); fflush(stderr);

        if (code != FFX_OK)
        {
            if (desc->messageCallback)
            {
                desc->messageCallback(SR_MESSAGE_TYPE_ERROR, L"FSR3 Context init failed");
                desc->messageCallback(SR_MESSAGE_TYPE_ERROR, std::to_wstring(code).c_str());
            }
            fprintf(stderr, "[SRNative-FSR3] srFfxFsr3InitUpscaleContext: FAILED code=%d\n", (int)code); fflush(stderr);
            return (SRReturnCode)SR_RETURN_CODE_ERROR;
        }
        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3InitUpscaleContext: Exit OK\n"); fflush(stderr);
        return (SRReturnCode)SR_RETURN_CODE_OK;
    }

    SR_API SRReturnCode srFfxFsr3CreateUpscaleContext(SRUpscaleContext *context, const SRCreateUpscaleContextDesc *desc)
    {
        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: Enter\n"); fflush(stderr);

        if (desc->renderApiType != SR_RENDER_API_TYPE_VULKAN)
        {
            if (desc->messageCallback)
            {
                desc->messageCallback(SR_MESSAGE_TYPE_ERROR, L"FSR3 only supports Vulkan");
            }
            fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: ERROR not Vulkan\n"); fflush(stderr);
            return SR_RETURN_CODE_UNSUPPORTED_RENDER_API;
        }

        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: device=%p physicalDevice=%p deviceProcAddr=%p\n",
                (void*)desc->renderDeviceInfo.vulkan.device,
                (void*)desc->renderDeviceInfo.vulkan.physicalDevice,
                (void*)desc->renderDeviceInfo.vulkan.deviceProcAddr);
        fflush(stderr);

        VkDeviceContext deviceContext = {
            (VkDevice)(desc->renderDeviceInfo.vulkan.device),
            (VkPhysicalDevice)(desc->renderDeviceInfo.vulkan.physicalDevice),
            (PFN_vkGetDeviceProcAddr)(desc->renderDeviceInfo.vulkan.deviceProcAddr),
        };

        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: Calling ffxGetDeviceVK...\n"); fflush(stderr);
        FfxDevice device = ffxGetDeviceVK(&deviceContext);
        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: ffxGetDeviceVK returned device=%p\n", (void*)device); fflush(stderr);

        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: Calling ffxGetScratchMemorySizeVK...\n"); fflush(stderr);
        size_t scratchBufferSize = ffxGetScratchMemorySizeVK((VkPhysicalDevice)(desc->renderDeviceInfo.vulkan.physicalDevice), 1);
        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: scratchBufferSize=%zu\n", scratchBufferSize); fflush(stderr);

        void *scratchBuffer = calloc(1, scratchBufferSize);
        if (!scratchBuffer) {
            fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: ERROR calloc failed for scratchBuffer\n"); fflush(stderr);
            return (SRReturnCode)SR_RETURN_CODE_ERROR;
        }

        FfxInterface *ffxInterface = new FfxInterface();
        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: Calling ffxGetInterfaceVK (scratchBuffer=%p, size=%zu)...\n", scratchBuffer, scratchBufferSize); fflush(stderr);
        if (FfxErrorCode _rc = ffxGetInterfaceVK(ffxInterface, device, scratchBuffer, scratchBufferSize, 1); _rc != FFX_OK)
        {
            fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: ERROR ffxGetInterfaceVK failed code=%d\n", (int)_rc); fflush(stderr);
            free(scratchBuffer);
            delete ffxInterface;
            return (SRReturnCode)SR_RETURN_CODE_ERROR;
        }
        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: ffxGetInterfaceVK OK\n"); fflush(stderr);

        FfxFsr3UpscalerContext *fsr3Context = new FfxFsr3UpscalerContext();

        SRFsr3PrivateData *privateData = new SRFsr3PrivateData();
        privateData->context = fsr3Context;
        privateData->ffxInterface = ffxInterface;
        privateData->scratchBuffer = scratchBuffer;

        context->desc = *const_cast<SRCreateUpscaleContextDesc *>(desc);
        context->userContext = privateData;
        fprintf(stderr, "[SRNative-FSR3] srFfxFsr3CreateUpscaleContext: Exit OK\n"); fflush(stderr);
        return (SRReturnCode)SR_RETURN_CODE_OK;
    }

    SR_API SRReturnCode srFfxFsr3DestroyUpscaleContext(SRUpscaleContext *context)
    {
        if (!context || !context->userContext)
        {
            return SR_RETURN_CODE_NULL_POINTER;
        }

        SRFsr3PrivateData *privateData = (SRFsr3PrivateData *)context->userContext;
        
        if (!privateData->context)
        {
            if (privateData->scratchBuffer)
            {
                free(privateData->scratchBuffer);
                privateData->scratchBuffer = nullptr;
            }
            delete privateData->ffxInterface;
            delete privateData;
            context->userContext = nullptr;
            return SR_RETURN_CODE_ERROR;
        }

        FfxErrorCode errorCode = ffxFsr3UpscalerContextDestroy(privateData->context);
        
        if (errorCode != FFX_OK)
        {
            if (context->desc.messageCallback)
            {
                context->desc.messageCallback(SR_MESSAGE_TYPE_ERROR, L"FSR3 Context destroy failed");
                context->desc.messageCallback(SR_MESSAGE_TYPE_ERROR, std::to_wstring(errorCode).c_str());
            }
        }
        
        if (privateData->scratchBuffer)
        {
            free(privateData->scratchBuffer);
            privateData->scratchBuffer = nullptr;
        }
        
        delete privateData->context;
        privateData->context = nullptr;
        
        delete privateData->ffxInterface;
        privateData->ffxInterface = nullptr;
        
        delete privateData;
        context->userContext = nullptr;
        
        return (errorCode != FFX_OK) ? (SRReturnCode)SR_RETURN_CODE_ERROR : (SRReturnCode)SR_RETURN_CODE_OK;
    }

    SR_API SRReturnCode srFfxFsr3QueryUpscale(SRUpscaleContext *context, SRUpscaleContextQueryResult *result, SRUpscaleContextQueryType queryType)
    {
        switch (queryType)
        {
        case SR_UPSCALE_CONTEXT_QUERY_VERSION_INFO:
        {
            static SRQueryVersionResult outResult = {};
            outResult.versionId = SR_MAKE_VERSION(FFX_FSR3UPSCALER_VERSION_MAJOR, FFX_FSR3UPSCALER_VERSION_MINOR, FFX_FSR3UPSCALER_VERSION_PATCH);
            outResult.versionNumber = SR_MAKE_VERSION(FFX_FSR3UPSCALER_VERSION_MAJOR, FFX_FSR3UPSCALER_VERSION_MINOR, FFX_FSR3UPSCALER_VERSION_PATCH);
            result->data = &outResult;
            break;
        }
        case SR_UPSCALE_CONTEXT_QUERY_GPU_MEMORY_INFO:
        {
            FfxEffectMemoryUsage usage = {};
            ffxFsr3UpscalerContextGetGpuMemoryUsage(((SRFsr3PrivateData *)context->userContext)->context, &usage);
            static SRQueryGpuMemoryResult outResult = {};
            outResult.gpuMemory = usage.totalUsageInBytes;
            result->data = &outResult;
            break;
        }
        case SR_UPSCALE_CONTEXT_QUERY_AVAILABLE:
        {
            static SRQueryAvailabilityResult outResult = {};
            outResult.isAvailable = true;
            result->data = &outResult;
            break;
        }
        default:
            break;
        }
        return (SRReturnCode)SR_RETURN_CODE_OK;
    }

    SR_API SRReturnCode srFfxFsr3DispatchUpscale(SRUpscaleContext *context, const SRDispatchUpscaleDesc *desc)
    {
        FfxFsr3UpscalerContext *fsr3Context = ((SRFsr3PrivateData *)context->userContext)->context;

        FfxFsr3UpscalerDispatchDescription dispatchDesc = {};
        dispatchDesc.commandList = ffxGetCommandListVK(desc->commandList.apiCommandBuffer.vulkan.commandBuffer);

        if (desc->color.exist)
            dispatchDesc.color = srTextureResourceToFfxResource(&desc->color);
        if (desc->depth.exist)
            dispatchDesc.depth = srTextureResourceToFfxResource(&desc->depth);
        if (desc->motionVectors.exist)
            dispatchDesc.motionVectors = srTextureResourceToFfxResource(&desc->motionVectors);
        if (desc->exposure.exist)
            dispatchDesc.exposure = srTextureResourceToFfxResource(&desc->exposure);
        if (desc->reactive.exist)
            dispatchDesc.reactive = srTextureResourceToFfxResource(&desc->reactive);
        if (desc->transparencyAndComposition.exist)
            dispatchDesc.transparencyAndComposition = srTextureResourceToFfxResource(&desc->transparencyAndComposition);
        if (desc->output.exist)
            dispatchDesc.output = srTextureResourceToFfxResource(&desc->output);
        //return (SRReturnCode)SR_RETURN_CODE_OK;
        dispatchDesc.jitterOffset = {desc->jitterOffset.x, desc->jitterOffset.y};
        dispatchDesc.motionVectorScale = {desc->motionVectorScale.x, desc->motionVectorScale.y};
        dispatchDesc.renderSize = {desc->renderSize.x, desc->renderSize.y};

        dispatchDesc.enableSharpening = desc->enableSharpening;
        dispatchDesc.sharpness = desc->sharpness;
        dispatchDesc.frameTimeDelta = desc->frameTimeDelta;
        dispatchDesc.preExposure = desc->preExposure;
        dispatchDesc.reset = desc->reset;
        dispatchDesc.cameraNear = desc->cameraNear;
        dispatchDesc.cameraFar = desc->cameraFar;
        dispatchDesc.cameraFovAngleVertical = desc->cameraFovAngleVertical;
        dispatchDesc.viewSpaceToMetersFactor = desc->viewSpaceToMetersFactor;
        SRFSR_CHECK(ffxFsr3UpscalerContextDispatch(fsr3Context, &dispatchDesc));
        return (SRReturnCode)SR_RETURN_CODE_OK;
    }
    SR_API SRUpscaleContextCallbacks srGetFfxFSR3UpscaleCallbacks()
    {
        static SRUpscaleContextCallbacks callbacks = {
            .pCreate = (SRCreateFunc)srFfxFsr3CreateUpscaleContext,
            .pInit = (SRInitFunc)srFfxFsr3InitUpscaleContext,
            .pDestroy = (SRDestroyFunc)srFfxFsr3DestroyUpscaleContext,
            .pQuery = (SRQueryFunc)srFfxFsr3QueryUpscale,
            .pDispatchUpscale = (SRDispatchUpscaleFunc)srFfxFsr3DispatchUpscale,
        };
        return callbacks;
    }

#ifdef __cplusplus
}
#endif
