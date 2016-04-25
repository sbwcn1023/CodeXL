//==============================================================================
// Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  This class interfaces with GPA to retrieve PMC and write the output file
//==============================================================================

#include <iostream>
#include <algorithm>

#include <hsa_ext_amd.h>
#include <amd_hsa_kernel_code.h>

#include <AMDTOSWrappers/Include/osThread.h>

#include "DeviceInfoUtils.h"
#include "GPUPerfAPI-HSA.h"

#include "HSAGPAProfiler.h"
#include "HSAModule.h"
#include "HSARTModuleLoader.h"
#include "AutoGenerated/HSAPMCInterception.h"

#include "FinalizerInfoManager.h"

#include "../Common/Logger.h"
#include "../Common/FileUtils.h"
#include "../Common/KernelProfileResultManager.h"

#include "../CLOccupancyAgent/CLOccupancyInfoManager.h"

using namespace std;

HSAGPAProfiler::HSAGPAProfiler(void) :
    m_uiCurKernelCount(0),
    m_uiMaxKernelCount(DEFAULT_MAX_KERNELS),
    m_uiOutputLineCount(0),
    m_isProfilingEnabled(true),
    m_isProfilerInitialized(false)
{
}

HSAGPAProfiler::~HSAGPAProfiler(void)
{
}

bool HSAGPAProfiler::WaitForCompletedSession(uint64_t queueId, uint32_t timeoutSeconds)
{
    bool retVal = true;

    // to avoid an infinite loop, bail after spinning for the specified number of seconds
    static const uint32_t SLEEP_TIME_MILLISECONDS = 10;
    uint64_t waitCount = (timeoutSeconds * 1000) / SLEEP_TIME_MILLISECONDS;
    uint64_t safetyNet = 0;

    bool queueFound = m_activeSessionMap.find(queueId) != m_activeSessionMap.end();

    if (!queueFound)
    {
        Log(logERROR, "Unknown queue specified\n");
        retVal = false;
    }

    while (queueFound && (safetyNet < waitCount))
    {
        safetyNet++;

        if (!CheckForCompletedSession(queueId))
        {
            OSUtils::Instance()->SleepMillisecond(SLEEP_TIME_MILLISECONDS);
        }

        queueFound = m_activeSessionMap.find(queueId) != m_activeSessionMap.end();
    }

    if (queueFound)
    {
        // previous session never completed
        Log(logERROR, "Session never completed after waiting %d seconds\n", timeoutSeconds);
        retVal = false;
    }

    return retVal;
}

void HSAGPAProfiler::WaitForCompletedSessions(uint32_t timeoutSeconds)
{
    bool waitForSessionCompletedSucceeded = true;
    auto it = m_activeSessionMap.begin();

    while (waitForSessionCompletedSucceeded && it != m_activeSessionMap.end())
    {
        waitForSessionCompletedSucceeded = WaitForCompletedSession(it->first, timeoutSeconds);

        // reinitialize the iterator -- WaitForCompletedSession can remove an item from the map
        it = m_activeSessionMap.begin();
    }
}

bool HSAGPAProfiler::CheckForCompletedSession(uint64_t queueId)
{
    bool retVal = false;
    QueueSessionMap::iterator it = m_activeSessionMap.find(queueId);

    if (m_activeSessionMap.end() != it)
    {
        bool isSessionReady = false;
        GPA_Status sessionStatus = m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_IsSessionReady(&isSessionReady, it->second.m_sessionID));

        if (GPA_STATUS_OK == sessionStatus && isSessionReady)
        {
            WriteSessionResult(it->second);
            m_gpaUtils.Close(); // TODO: this might not be right in the multi-queue case
            retVal = true;
        }
    }

    if (retVal)
    {
        m_activeSessionMap.erase(queueId);
    }

    return retVal;
}

bool HSAGPAProfiler::IsGPUDevice(hsa_agent_t agent)
{
    bool retVal = false;

    hsa_device_type_t deviceType;
    hsa_status_t status = g_realHSAFunctions->hsa_agent_get_info_fn(agent, HSA_AGENT_INFO_DEVICE, &deviceType);

    if (HSA_STATUS_SUCCESS == status && HSA_DEVICE_TYPE_GPU == deviceType)
    {
        retVal = true;
    }

    return retVal;
}

hsa_status_t HSAGPAProfiler::GetGPUDeviceIDs(hsa_agent_t agent, void* pData)
{
    hsa_status_t status = HSA_STATUS_SUCCESS;

    if (NULL == pData)
    {
        status = HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        if (IsGPUDevice(agent))
        {
            uint32_t deviceId;
            status = g_realHSAFunctions->hsa_agent_get_info_fn(agent, static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_CHIP_ID), &deviceId);

            if (HSA_STATUS_SUCCESS == status)
            {
                static_cast<vector<uint32_t>*>(pData)->push_back(deviceId);
            }
        }
    }

    return status;
}

bool HSAGPAProfiler::Init(const Parameters& params, std::string& strErrorOut)
{
    if (!m_isProfilerInitialized)
    {
        Log(logMESSAGE, "Initializing HSAGPAProfiler\n");
        const size_t nMaxPass = 1;

        m_isProfilingEnabled = !params.m_bStartDisabled;

        m_uiMaxKernelCount = params.m_uiMaxKernels;

        if (params.m_strCounterFile.empty())
        {
            cout << "No counter file specified. Only counters that will fit into a single pass will be enabled.\n";
        }

        // Set output file
        SetOutputFile(params.m_strOutputFile);

        // Set list separator
        KernelProfileResultManager::Instance()->SetListSeparator(params.m_cOutputSeparator);

        // Init CSV file header and column row
        InitHeader();

        CounterList enabledCounters;
        m_gpaUtils.InitGPA(GPA_API_HSA,
                           params.m_strDLLPath,
                           strErrorOut,
                           params.m_strCounterFile.empty() ? NULL : params.m_strCounterFile.c_str(),
                           &enabledCounters,
                           nMaxPass); // only allow a single pass

        SP_TODO("support enforcing single pass when there are multiple HSA devices");

        // Enable all counters if no counter file is specified or counter file is empty.
        if (enabledCounters.empty())
        {
#ifdef GDT_INTERNAL
            // Internal mode must have a counter file specified.
            cout << "Please specify a counter file using -c. No counter is enabled." << endl;
            return false;
#else
            vector<uint32_t> gpuDeviceIds;
            hsa_status_t status = g_realHSAFunctions->hsa_iterate_agents_fn(GetGPUDeviceIDs, &gpuDeviceIds);

            if (HSA_STATUS_SUCCESS == status)
            {
                set<string> counterSet;
                CounterList tempCounters;

                for (auto deviceId : gpuDeviceIds)
                {
                    CounterList counterNames;
                    // TODO: need to get revision id from HSA runtime (SWDEV-79571)
                    m_gpaUtils.GetAvailableCountersForDevice(deviceId, 0, nMaxPass, counterNames);
                    tempCounters.insert(tempCounters.end(), counterNames.begin(), counterNames.end());
                }

                // remove duplicated counter
                for (CounterList::iterator it = tempCounters.begin(); it != tempCounters.end(); ++it)
                {
                    if (counterSet.find(*it) == counterSet.end())
                    {
                        counterSet.insert(*it);
                        enabledCounters.push_back(*it);
                    }
                }

                m_gpaUtils.SetEnabledCounters(enabledCounters);
            }

#endif //GDT_INTERNAL
        }

        for (CounterList::iterator it = enabledCounters.begin(); it != enabledCounters.end(); ++it)
        {
            KernelProfileResultManager::Instance()->AddProfileResultItem(*it);
        }

        m_isProfilerInitialized = true;
    }

    return true;
}

void HSAGPAProfiler::InitHeader()
{
    KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("ProfileFileVersion=%d.%d", GPUPROFILER_BACKEND_MAJOR_VERSION, GPUPROFILER_BACKEND_MINOR_VERSION));
    KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("ProfilerVersion=%d.%d.%d", GPUPROFILER_BACKEND_MAJOR_VERSION, GPUPROFILER_BACKEND_MINOR_VERSION, GPUPROFILER_BACKEND_BUILD_NUMBER));
    KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("API=HSA"));
    KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("Application=%s", FileUtils::GetExeFullPath().c_str()));
    KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("ApplicationArgs=%s", GlobalSettings::GetInstance()->m_params.m_strCmdArgs.asUTF8CharArray()));
    KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("WorkingDirectory=%s", GlobalSettings::GetInstance()->m_params.m_strWorkingDir.asUTF8CharArray()));

    EnvVarMap envVarMap = GlobalSettings::GetInstance()->m_params.m_mapEnvVars;

    if (envVarMap.size() > 0)
    {
        KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("FullEnvironment=%d", GlobalSettings::GetInstance()->m_params.m_bFullEnvBlock));

        for (EnvVarMap::const_iterator it = envVarMap.begin(); it != envVarMap.end(); ++it)
        {
            KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("EnvVar=%s=%s", it->first.asUTF8CharArray(), it->second.asUTF8CharArray()));
        }
    }

    // TODO: determine what device info to include in file header for HSA
    //for (CLPlatformSet::iterator idxPlatform = m_platformList.begin(); idxPlatform != m_platformList.end(); idxPlatform++)
    //{
    //   KernelProfileResultManager::Instance()->AddHeader( StringUtils::FormatString("Device %s Platform Vendor=%s", idxPlatform->strDeviceName.c_str(), idxPlatform->strPlatformVendor.c_str()));
    //   KernelProfileResultManager::Instance()->AddHeader( StringUtils::FormatString("Device %s Platform Name=%s", idxPlatform->strDeviceName.c_str(), idxPlatform->strPlatformName.c_str()));
    //   KernelProfileResultManager::Instance()->AddHeader( StringUtils::FormatString("Device %s Platform Version=%s", idxPlatform->strDeviceName.c_str(), idxPlatform->strPlatformVersion.c_str()));
    //   KernelProfileResultManager::Instance()->AddHeader( StringUtils::FormatString("Device %s CLDriver Version=%s", idxPlatform->strDeviceName.c_str(), idxPlatform->strDriverVersion.c_str()));
    //   KernelProfileResultManager::Instance()->AddHeader( StringUtils::FormatString("Device %s CLRuntime Version=%s", idxPlatform->strDeviceName.c_str(), idxPlatform->strCLRuntime.c_str()));
    //   KernelProfileResultManager::Instance()->AddHeader( StringUtils::FormatString("Device %s NumberAppAddressBits=%d", idxPlatform->strDeviceName.c_str(), idxPlatform->uiNbrAddressBits));
    //}

    std::string strOSVersion = OSUtils::Instance()->GetOSInfo();
    KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("OS Version=%s", OSUtils::Instance()->GetOSInfo().c_str()));
    KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("DisplayName=%s", GlobalSettings::GetInstance()->m_params.m_strSessionName.c_str()));
    KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("ListSeparator=%c", GlobalSettings::GetInstance()->m_params.m_cOutputSeparator));

    if (m_uiMaxKernelCount != DEFAULT_MAX_KERNELS)
    {
        KernelProfileResultManager::Instance()->AddHeader(StringUtils::FormatString("MaxNumberOfKernelsToProfile=%d", m_uiMaxKernelCount));
    }

    // TODO: add back in columns when kernel stats are added
    KernelProfileResultManager::Instance()->AddProfileResultItem("Method");
    KernelProfileResultManager::Instance()->AddProfileResultItem("ExecutionOrder");
    KernelProfileResultManager::Instance()->AddProfileResultItem("ThreadID");
    KernelProfileResultManager::Instance()->AddProfileResultItem("GlobalWorkSize");
    KernelProfileResultManager::Instance()->AddProfileResultItem("WorkGroupSize");
    //KernelProfileResultManager::Instance()->AddProfileResultItem("Time");
    KernelProfileResultManager::Instance()->AddProfileResultItem("LocalMemSize");
    KernelProfileResultManager::Instance()->AddProfileResultItem("VGPRs");
    KernelProfileResultManager::Instance()->AddProfileResultItem("SGPRs");
    //KernelProfileResultManager::Instance()->AddProfileResultItem("ScratchRegs");
}

void HSAGPAProfiler::SetOutputFile(const std::string& strOutputFile)
{
    if (strOutputFile.empty())
    {
        // If output file is not set, we use exe name as file name
        m_strOutputFile = FileUtils::GetDefaultOutputPath() + FileUtils::GetExeName() + ".hsa." + PERF_COUNTER_EXT;
    }
    else
    {
        std::string strExtension("");

        strExtension = FileUtils::GetFileExtension(strOutputFile);

        if (strExtension != PERF_COUNTER_EXT)
        {
            if ((strExtension == TRACE_EXT) || (strExtension == OCCUPANCY_EXT))
            {
                std::string strBaseFileName = FileUtils::GetBaseFileName(strOutputFile);
                m_strOutputFile = strBaseFileName + ".hsa." + PERF_COUNTER_EXT;
            }
            else
            {
                m_strOutputFile = strOutputFile + ".hsa." + PERF_COUNTER_EXT;
            }
        }
        else
        {
            m_strOutputFile = strOutputFile;
        }
    }

    KernelProfileResultManager::Instance()->SetOutputFile(m_strOutputFile);
}

bool HSAGPAProfiler::PopulateKernelStatsFromDispatchPacket(hsa_kernel_dispatch_packet_t* pAqlPacket, const std::string& strAgentName,  KernelStats& kernelStats)
{
    bool retVal = true;

    if (nullptr == pAqlPacket || 0 == pAqlPacket->kernel_object)
    {
        Log(logERROR, "Unable to get Kernel Stats from dispatch packet.\n");
        retVal = false;
    }
    else
    {
        FinalizerInfoManager* pFinalizerInfoMan = FinalizerInfoManager::Instance();

#ifdef _DEBUG
        Log(logMESSAGE, "Lookup %llu\n", pAqlPacket->kernel_object);

        Log(logMESSAGE, "Dump m_codeHandleToSymbolHandleMap\n");

        for (auto mapItem : pFinalizerInfoMan->m_codeHandleToSymbolHandleMap)
        {
            Log(logMESSAGE, "  Item: %llu == %llu\n", mapItem.first, mapItem.second);

            if (pAqlPacket->kernel_object == mapItem.first)
            {
                Log(logMESSAGE, "  Match found!\n");
            }
        }

        Log(logMESSAGE, "End Dump m_codeHandleToSymbolHandleMap\n");
#endif

        std::string symName;

        if (pFinalizerInfoMan->m_codeHandleToSymbolHandleMap.count(pAqlPacket->kernel_object) > 0)
        {
            uint64_t symHandle = pFinalizerInfoMan->m_codeHandleToSymbolHandleMap[pAqlPacket->kernel_object];

            if (pFinalizerInfoMan->m_symbolHandleToNameMap.count(symHandle) > 0)
            {
                symName = pFinalizerInfoMan->m_symbolHandleToNameMap[symHandle];
                Log(logMESSAGE, "Lookup: CodeHandle: %llu, SymHandle: %llu, symName: %s\n", pAqlPacket->kernel_object, symHandle, symName.c_str());
            }
        }

        if (symName.empty())
        {
            symName = "<UnknownKernelName>";
        }

        kernelStats.m_strName = symName + "_" + strAgentName;

        const amd_kernel_code_t* pKernelCode = reinterpret_cast<const amd_kernel_code_t*>(pAqlPacket->kernel_object);

        HSAModule* pHsaModule = HSARTModuleLoader<HSAModule>::Instance()->GetHSARTModule();

        if (nullptr == pHsaModule)
        {
            Log(logERROR, "Unable to load HSA RT Module\n");
            retVal = false;
        }
        else
        {
            const void* pKernelHostAddress = nullptr;

            if (nullptr != pHsaModule->ven_amd_loaded_code_object_query_host_address)
            {
                hsa_status_t status = pHsaModule->ven_amd_loaded_code_object_query_host_address(reinterpret_cast<const void*>(pAqlPacket->kernel_object), &pKernelHostAddress);

                if (HSA_STATUS_SUCCESS == status)
                {
                    pKernelCode = reinterpret_cast<const amd_kernel_code_t*>(pKernelHostAddress);
                }
            }
        }

        if (nullptr == pKernelCode)
        {
            Log(logERROR, "Unable to get Kernel Stats from dispatch packet: kernel code object is null.\n");
            retVal = false;
        }
        else
        {
            kernelStats.m_kernelInfo.m_nUsedGPRs = pKernelCode->workitem_vgpr_count;
            kernelStats.m_kernelInfo.m_nUsedScalarGPRs = pKernelCode->wavefront_sgpr_count;
            kernelStats.m_kernelInfo.m_nUsedLDSSize = pKernelCode->workgroup_group_segment_byte_size;

            kernelStats.m_kernelInfo.m_nAvailableGPRs = 256; // TODO: get value from runtime when available
            kernelStats.m_kernelInfo.m_nAvailableScalarGPRs = 104; // TODO: get value from runtime when available
            kernelStats.m_kernelInfo.m_nAvailableLDSSize = 65536; // TODO: get value from runtime when available

            // extract the number of dimensions from the setup field in the packet
            kernelStats.m_uWorkDim = pAqlPacket->setup & ((1 << HSA_KERNEL_DISPATCH_PACKET_SETUP_WIDTH_DIMENSIONS) - 1);

            kernelStats.m_workGroupSize[0] = pAqlPacket->workgroup_size_x;
            kernelStats.m_workGroupSize[1] = pAqlPacket->workgroup_size_y;
            kernelStats.m_workGroupSize[2] = pAqlPacket->workgroup_size_z;

            kernelStats.m_globalWorkSize[0] = pAqlPacket->grid_size_x;
            kernelStats.m_globalWorkSize[1] = pAqlPacket->grid_size_y;
            kernelStats.m_globalWorkSize[2] = pAqlPacket->grid_size_z;

            // per the HSA RT team, the thread we get the pre-dispatch callback from is
            // the same thread that dispatched the kernel.
            kernelStats.m_threadId = osGetUniqueCurrentThreadId();
        }
    }

    return retVal;
}

bool HSAGPAProfiler::Begin(const hsa_agent_t             device,
                           const hsa_queue_t*            pQueue,
                           hsa_kernel_dispatch_packet_t* pAqlPacket,
                           void*                         pAqlTranslationHandle)
{
    SpAssertRet(NULL != pQueue) false;

    bool retVal = true;

    uint32_t deviceId;
    hsa_status_t status = g_realHSAFunctions->hsa_agent_get_info_fn(device, static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_CHIP_ID), &deviceId);

    std::string strAgentName = "<UnknownDeviceName>";

    if (HSA_STATUS_SUCCESS == status)
    {
        GDT_GfxCardInfo cardInfo;

        // TODO: need to get revision id from HSA runtime (SWDEV-79571)
        if (AMDTDeviceInfoUtils::Instance()->GetDeviceInfo(deviceId, 0, cardInfo))
        {
            strAgentName = std::string(cardInfo.m_szCALName);
        }
    }

    KernelStats kernelStats;
    PopulateKernelStatsFromDispatchPacket(pAqlPacket, strAgentName, kernelStats);

    if (IsGPUDevice(device))
    {
        if (!m_gpaUtils.IsInitialized())
        {
            return false;
        }

        // make sure any previous sessions are completed before starting a new one.
        WaitForCompletedSession(pQueue->id);

        m_mtx.Lock();
        SpAssertRet(pQueue != NULL) false;

        GPA_HSA_Context gpaContext;
        gpaContext.m_pAgent = &device;
        gpaContext.m_pQueue = pQueue;
        gpaContext.m_pAqlTranslationHandle = pAqlTranslationHandle;
        //TODO: do we need to force synchronization so that we ensure only one context open at a time?
        bool bRet = m_gpaUtils.Open(&gpaContext);
        SpAssertRet(bRet) false;
        bRet = m_gpaUtils.EnableCounters();
        SpAssertRet(bRet) false;

        gpa_uint32 currentSessionId;
        int stat = m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_BeginSession(&currentSessionId));
        gpa_uint32 GPA_pCountTemp;
        stat += m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_GetPassCount(&GPA_pCountTemp));

        SpAssertRet(GPA_pCountTemp == 1) false;

        stat += m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_BeginPass());
        stat += m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_BeginSample(0));

        retVal = stat == static_cast<int>(GPA_STATUS_OK);

        if (retVal)
        {
            SessionInfo sessionInfo = { currentSessionId, kernelStats, strAgentName };
            m_activeSessionMap[pQueue->id] = sessionInfo;

            if (GlobalSettings::GetInstance()->m_params.m_bKernelOccupancy)
            {
                if (!AddOccupancyEntry(kernelStats, strAgentName, device))
                {
                    Log(logERROR, "Unable to add Occupancy data\n");
                }
            }
        }
    }

    return retVal;
}

bool HSAGPAProfiler::End(const hsa_agent_t  device,
                         const hsa_queue_t* pQueue,
                         void*              pAqlTranslationHandle,
                         hsa_signal_t       signal)
{
    SP_UNREFERENCED_PARAMETER(device);
    SP_UNREFERENCED_PARAMETER(pQueue);
    SP_UNREFERENCED_PARAMETER(pAqlTranslationHandle);
    SP_UNREFERENCED_PARAMETER(signal);

    bool retVal = true;

    if (IsGPUDevice(device))
    {
        if (!m_gpaUtils.IsInitialized())
        {
            return false;
        }

        int stat = m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_EndSample());
        stat += m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_EndPass());
        stat += m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_EndSession());

        m_mtx.Unlock();
        retVal = (stat == static_cast<int>(GPA_STATUS_OK));
    }

    return retVal;
}

bool HSAGPAProfiler::WriteSessionResult(const SessionInfo& sessionInfo)
{
    if (!m_gpaUtils.IsInitialized())
    {
        return false;
    }

    ++m_uiOutputLineCount;

    KernelProfileResultManager::Instance()->BeginKernelInfo();

    KernelProfileResultManager::Instance()->WriteKernelInfo("Method", sessionInfo.m_kernelStats.m_strName);
    KernelProfileResultManager::Instance()->WriteKernelInfo("ExecutionOrder", m_uiOutputLineCount);
    KernelProfileResultManager::Instance()->WriteKernelInfo("ThreadID", sessionInfo.m_kernelStats.m_threadId);

    KernelProfileResultManager::Instance()->WriteKernelInfo("GlobalWorkSize",
                                                            StringUtils::FormatString("{%7lu %7lu %7lu}", sessionInfo.m_kernelStats.m_globalWorkSize[0], sessionInfo.m_kernelStats.m_globalWorkSize[1], sessionInfo.m_kernelStats.m_globalWorkSize[2]));
    KernelProfileResultManager::Instance()->WriteKernelInfo("WorkGroupSize",
                                                            StringUtils::FormatString("{%5lu %5lu %5lu}", sessionInfo.m_kernelStats.m_workGroupSize[0], sessionInfo.m_kernelStats.m_workGroupSize[1], sessionInfo.m_kernelStats.m_workGroupSize[2]));

    KernelProfileResultManager::Instance()->WriteKernelInfo("LocalMemSize", sessionInfo.m_kernelStats.m_kernelInfo.m_nUsedLDSSize == KERNELINFO_NONE ? "NA" : StringUtils::ToString(sessionInfo.m_kernelStats.m_kernelInfo.m_nUsedLDSSize));
    KernelProfileResultManager::Instance()->WriteKernelInfo("VGPRs", sessionInfo.m_kernelStats.m_kernelInfo.m_nUsedGPRs == KERNELINFO_NONE ? "NA" : StringUtils::ToString(sessionInfo.m_kernelStats.m_kernelInfo.m_nUsedGPRs));
    KernelProfileResultManager::Instance()->WriteKernelInfo("SGPRs", sessionInfo.m_kernelStats.m_kernelInfo.m_nUsedScalarGPRs == KERNELINFO_NONE ? "NA" : StringUtils::ToString(sessionInfo.m_kernelStats.m_kernelInfo.m_nUsedScalarGPRs));

    gpa_uint32 sampleCount;
    m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_GetSampleCount(sessionInfo.m_sessionID, &sampleCount));

    for (gpa_uint32 sample = 0; sample < sampleCount; sample++)
    {
        // TODO: add call to write kernel stats here

        gpa_uint32 nEnabledCounters = 0;
        m_gpaUtils.GetGPALoader().GPA_GetEnabledCount(&nEnabledCounters);

        for (gpa_uint32 counter = 0 ; counter < nEnabledCounters ; counter++)
        {
            gpa_uint32 enabledCounterIndex;

            if (GPA_STATUS_OK != m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_GetEnabledIndex(counter, &enabledCounterIndex)))
            {
                continue;
            }

            GPA_Type type;

            if (m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_GetCounterDataType(enabledCounterIndex, &type)))
            {
                continue;
            }

            const char* pName = NULL;

            if (m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_GetCounterName(enabledCounterIndex, &pName)))
            {
                continue;
            }

            string strName;

            if (pName != NULL)
            {
                strName = pName;
            }
            else
            {
                SpBreak("Failed to retrieve counter name.");
                continue;
            }

            if (type == GPA_TYPE_UINT32)
            {
                gpa_uint32 value;

                if (GPA_STATUS_OK == m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_GetSampleUInt32(sessionInfo.m_sessionID, sample, enabledCounterIndex, &value)))
                {
                    KernelProfileResultManager::Instance()->WriteKernelInfo(strName, StringUtils::FormatString("%8u", value));
                }
            }
            else if (type == GPA_TYPE_UINT64)
            {
                gpa_uint64 value;

                if (GPA_STATUS_OK == m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_GetSampleUInt64(sessionInfo.m_sessionID, sample, enabledCounterIndex, &value)))
                {
#ifdef _WIN32
                    KernelProfileResultManager::Instance()->WriteKernelInfo(strName, StringUtils::FormatString("%8I64u", value));
#else
                    KernelProfileResultManager::Instance()->WriteKernelInfo(strName, StringUtils::FormatString("%lu", value));
#endif
                }
            }
            else if (type == GPA_TYPE_FLOAT32)
            {
                gpa_float32 value;

                if (GPA_STATUS_OK == m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_GetSampleFloat32(sessionInfo.m_sessionID, sample, enabledCounterIndex, &value)))
                {
                    KernelProfileResultManager::Instance()->WriteKernelInfo(strName, StringUtils::FormatString("%12.2f", value));
                }
            }
            else if (type == GPA_TYPE_FLOAT64)
            {
                gpa_float64 value;

                if (GPA_STATUS_OK == m_gpaUtils.StatusCheck(m_gpaUtils.GetGPALoader().GPA_GetSampleFloat64(sessionInfo.m_sessionID, sample, enabledCounterIndex, &value)))
                {
                    KernelProfileResultManager::Instance()->WriteKernelInfo(strName, StringUtils::FormatString("%12.2f", value));
                }
            }
            else
            {
                assert(false);
                return false;
            }
        }
    }

    KernelProfileResultManager::Instance()->EndKernelInfo();

    return true;
}

bool HSAGPAProfiler::AddOccupancyEntry(const KernelStats& kernelStats, const std::string& deviceName, hsa_agent_t agent)
{
    bool retVal = true;
    OccupancyInfoEntry* pEntry = new(std::nothrow) OccupancyInfoEntry();
    SpAssertRet(pEntry != NULL) false;

    pEntry->m_tid = kernelStats.m_threadId;
    pEntry->m_strKernelName = kernelStats.m_strName;
    pEntry->m_strDeviceName = deviceName;
    pEntry->m_nWorkGroupItemCount = kernelStats.m_workGroupSize[0];

    for (unsigned int i = 1; i < kernelStats.m_uWorkDim; i++)
    {
        pEntry->m_nWorkGroupItemCount *= kernelStats.m_workGroupSize[i];
    }

    pEntry->m_nWorkGroupItemCountMax = 256; // TODO: get value from runtime when available

    pEntry->m_nGlobalItemCount = kernelStats.m_globalWorkSize[0];

    for (unsigned int i = 1; i < kernelStats.m_uWorkDim; i++)
    {
        pEntry->m_nGlobalItemCount *= kernelStats.m_globalWorkSize[i];
    }

    pEntry->m_nGlobalItemCountMax = 0x1000000; // TODO: get value from runtime when available

    uint32_t numComputeUnit;
    hsa_status_t status = g_realHSAFunctions->hsa_agent_get_info_fn(agent, static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT), &numComputeUnit);

    if (HSA_STATUS_SUCCESS == status)
    {
        pEntry->m_nNumberOfComputeUnits = numComputeUnit;
    }
    else
    {
        Log(logERROR, "Unable to get Compute Unit Count from hsa_agent_get_info\n");
        retVal = false;
    }

    GDT_HW_GENERATION gen;

    if (!AMDTDeviceInfoUtils::Instance()->GetHardwareGeneration(pEntry->m_strDeviceName.c_str(), gen))
    {
        Log(logERROR, "Unable to query the hw generation\n");
        SAFE_DELETE(pEntry);
        return false;
    }

    if (gen >= GDT_HW_GENERATION_VOLCANICISLAND && gen < GDT_HW_GENERATION_LAST)
    {
        pEntry->m_pCLCUInfo = new(std::nothrow) CLCUInfoVI();
    }
    else if (gen == GDT_HW_GENERATION_SEAISLAND)
    {
        pEntry->m_pCLCUInfo = new(std::nothrow) CLCUInfoSI();
    }
    else
    {
        // don't use the EG/NI/SI occupancy calculation since HSA is supported on CI and newer
        Log(logERROR, "Unsupported hw generation\n");
        SAFE_DELETE(pEntry);
        return false;
    }

    SpAssertRet(pEntry->m_pCLCUInfo != NULL) false;

    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_VECTOR_GPRS_MAX, kernelStats.m_kernelInfo.m_nAvailableGPRs);
    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_SCALAR_GPRS_MAX, kernelStats.m_kernelInfo.m_nAvailableScalarGPRs);
    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_LDS_MAX, kernelStats.m_kernelInfo.m_nAvailableLDSSize);

    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_VECTOR_GPRS_USED, kernelStats.m_kernelInfo.m_nUsedGPRs);
    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_SCALAR_GPRS_USED, kernelStats.m_kernelInfo.m_nUsedScalarGPRs);
    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_LDS_USED, kernelStats.m_kernelInfo.m_nUsedLDSSize);

    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_KERNEL_WG_SIZE, pEntry->m_nWorkGroupItemCount);
    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_WG_SIZE_MAX, pEntry->m_nWorkGroupItemCountMax);
    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_KERNEL_GLOBAL_SIZE, pEntry->m_nGlobalItemCount);
    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_GLOBAL_SIZE_MAX, pEntry->m_nGlobalItemCountMax);

    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_NBR_COMPUTE_UNITS, pEntry->m_nNumberOfComputeUnits);

    pEntry->m_pCLCUInfo->SetCUParam(CU_PARAMS_DEVICE_NAME, pEntry->m_strDeviceName);

    pEntry->m_pCLCUInfo->ComputeCUOccupancy((unsigned int)pEntry->m_nWorkGroupItemCount);

    OccupancyInfoManager::Instance()->AddTraceInfoEntry(pEntry);

    return retVal;
}