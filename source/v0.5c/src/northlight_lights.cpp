#include "northlight_lights.h"

#include <windows.h>
#include <imgui.h>

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

namespace ctl::northlight_lights
{
    struct Matrix4x3
    {
        float m[12]{};
    };

    enum class LightType { Spot, Point };

    struct SpawnedSpot
    {
        LightType type{LightType::Spot};
        void* genericContent{};
        void* genericState{};
        void* spotContent{};
        void* spotState{};
        void* manager{};
        void* gameObject{};
        void* runtime{};
        void* wrapper{};
        void* traveller{};
        uint64_t genericId{};
        uint64_t spotId{};

        float intensity{20.0f};
        float color[3]{1.0f, 1.0f, 1.0f};
        float fov{45.0f};
        float radius{0.5f};
        float pointRange{10.0f};
        float pointDrawDistance{100.0f};
        float pointDecay{1.0f};
        float pointDiameter{0.0f};
        bool pointShadows{false};
        bool pointBentNormals{false};
        bool editPointRange{false};
        bool editPointDrawDistance{false};
        bool editPointDecay{false};
        bool editPointDiameter{false};
        bool editPointShadows{false};
        bool editPointBentNormals{false};
        float aspectRatio{0.0f};
        float directionality{0.0f};
        float nearPlane{0.3f};
        float farPlane{1.0f};
        int projectionType{0}; // 0 Perspective, 1 Stereographic
        bool bentNormals{true};
        bool directLighting{true};
        bool farClipping{false};
        bool volumetric{true};
        float scatter{1.0f};
        float volumetricMultiplier{1.0f};
        float phase0{0.65f};
        float phase1{0.0f};
        float phaseBlend{0.5f};
        int volumetricQuality{2}; // Low, Medium, High
        int volumetricShape{0};   // Cone, Square Frustum
        float depthBias{0.001f};
        float depthSlopeBias{2.0f};
        float shadowFilterSize{0.004f};
        int shadowmapType{1}; // Off, PCF, VSM
        int shadowmapSize{3}; // 128, 256, 512, 1024
        bool enabled{true};   // toolkit master switch

        Matrix4x3 transform{};
        bool hasTransform{false};
    };

    struct Api
    {
        HMODULE rl{};
        HMODULE core{};
        HMODULE renderer{};
        HMODULE coreshared{};

        using ConstructClassFn = void* (__fastcall*)(const char*);
        using ProcessorCtorFn = void(__fastcall*)(void*, int, const char*);
        using ProcessorConstructFn = void(__fastcall*)(void*);
        using GlobalIDSetFn = void(__fastcall*)(uint64_t*, uint16_t, const char*, int);
        using SetNetOwnershipFn = void(__fastcall*)(uint64_t*, int);
        using RegisterIDFn = void(__fastcall*)(void*, const uint64_t*, void*);
        using ReserveComponentsFn = void(__fastcall*)(void*, unsigned int);
        using AddComponentFn = void(__fastcall*)(void*, void*);
        using OnComponentsAddedFn = void(__fastcall*)(void*);
        using GetManagerFn = void* (__fastcall*)(int);
        using InstantiateFn = void* (__fastcall*)(void*, void*);
        using InstantiateComponentsFn = void(__fastcall*)(void*);
        using SetContentPointerFn = void(__fastcall*)(void*, const void*);
        using RequestUpdateFn = void(__fastcall*)(void*);
        using VariableUpdateFn = void(__fastcall*)(void*);

        using StateEnableFn = void(__fastcall*)(void*, bool);
        using StateSetColorFn = void(__fastcall*)(void*, float, float, float, float);
        using StateSetIntensityFn = void(__fastcall*)(void*, float, float);
        using StateSetConeFn = void(__fastcall*)(void*, float, float);
        using StateSetNearFn = void(__fastcall*)(void*, float);

        using WrapperMatrixFn = void(__fastcall*)(void*, const Matrix4x3&);
        using WrapperFloatFn = void(__fastcall*)(void*, float);
        using WrapperVector3Fn = void(__fastcall*)(void*, const float*);
        using WrapperBoolFn = void(__fastcall*)(void*, bool);
        using WrapperPhaseFn = void(__fastcall*)(void*, float, float, float);

        ConstructClassFn constructClass{};
        ProcessorCtorFn processorCtor{};
        ProcessorConstructFn processorConstruct{};
        GlobalIDSetFn globalIDSet{};
        SetNetOwnershipFn setNetOwnership{};
        void** globalIDMapExport{};
        RegisterIDFn registerID{};
        ReserveComponentsFn reserveComponents{};
        AddComponentFn addComponent{};
        OnComponentsAddedFn onComponentsAdded{};
        GetManagerFn getManager{};
        InstantiateFn instantiate{};
        InstantiateComponentsFn instantiateComponents{};
        SetContentPointerFn setContentPointer{};
        RequestUpdateFn requestUpdate{};
        VariableUpdateFn variableUpdate{};

        StateEnableFn stateEnable{};
        StateSetColorFn stateSetColor{};
        StateSetIntensityFn stateSetIntensity{};
        StateSetConeFn stateSetCone{};
        StateSetNearFn stateSetNear{};

        WrapperMatrixFn setLightToWorld{};
        WrapperFloatFn setRadius{};
        WrapperFloatFn setConeShape{};
        WrapperBoolFn setVolumetric{};
        WrapperBoolFn setVolumetricOnly{};
        WrapperPhaseFn setPhase{};
        StateEnableFn pointStateEnable{};
        StateSetColorFn pointSetColor{};
        StateSetIntensityFn pointSetIntensity{};
        RequestUpdateFn pointRequestUpdate{};
        VariableUpdateFn pointVariableUpdate{};
        WrapperMatrixFn pointSetMatrix{};
        WrapperFloatFn pointSetRadius{};
        WrapperVector3Fn pointWrapperSetIntensity{};
        WrapperFloatFn pointSetScatter{};
        WrapperFloatFn pointSetRangeClip{};
        WrapperFloatFn pointSetMaxDrawDistance{};
        WrapperFloatFn pointSetDecay{};
        WrapperFloatFn pointSetDiameter{};
        WrapperBoolFn pointSetCastShadows{};
        WrapperBoolFn pointSetBentNormals{};
        WrapperFloatFn setVolumetricMultiplier{};
        WrapperFloatFn setDirectionality{};
        WrapperFloatFn setScatter{};

        const void** renderViewGlobal{};
        bool ready{};
    };

    static Api g_api{};
    static std::vector<SpawnedSpot> g_spots{};
    static int g_selected{-1};
    static std::atomic<uint64_t> g_serial{1};
    static std::string g_status{"Light API not resolved"};

    static void* Export(HMODULE module, const char* name)
    {
        return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
    }

    template <typename T>
    static bool SafeRead(const void* address, T& out)
    {
        if (!address)
            return false;
        __try
        {
            out = *reinterpret_cast<const T*>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool ResolveApi()
    {
        if (g_api.ready)
            return true;

        g_api.rl = GetModuleHandleW(L"rl_rmdwin10_f.dll");
        g_api.core = GetModuleHandleW(L"coregame_rmdwin10_f.dll");
        g_api.renderer = GetModuleHandleW(L"renderer_rmdwin10_f.dll");
        g_api.coreshared = GetModuleHandleW(L"coreshared_rmdwin10_f.dll");

        if (!g_api.rl || !g_api.core || !g_api.renderer || !g_api.coreshared)
        {
            g_status = "Northlight modules are not loaded";
            return false;
        }

#define NL_RESOLVE(field, moduleField, symbol) \
        g_api.field = reinterpret_cast<decltype(g_api.field)>(Export(g_api.moduleField, symbol))

        NL_RESOLVE(constructClass, rl, "?constructClass@ClassFactory@r@@SAPEAXPEBD@Z");
        NL_RESOLVE(processorCtor, rl, "??0ObjectStreamProcessorBase@r@@QEAA@W4Mode@ObjectStreamProcessor@1@PEBD@Z");
        NL_RESOLVE(processorConstruct, rl, "?construct@ObjectStreamProcessorBase@r@@IEAAXXZ");
        NL_RESOLVE(globalIDSet, rl, "?set@GlobalID@r@@QEAAXGPEBDW4Owner@12@@Z");
        NL_RESOLVE(setNetOwnership, rl, "?setGlobalIDNetOwnership@net@@YAXAEAVGlobalID@r@@W4NetworkRole@1@@Z");
        NL_RESOLVE(globalIDMapExport, rl, "?sm_pInstance@GlobalIDMap@r@@0PEAV12@EA");
        NL_RESOLVE(registerID, rl, "?registerID@GlobalIDMap@r@@QEAAXAEBVGlobalID@2@PEAX@Z");
        NL_RESOLVE(reserveComponents, rl, "?reserveComponents@GameObjectState@r@@QEAAXI@Z");
        NL_RESOLVE(addComponent, rl, "?addComponent@GameObjectState@r@@QEAAXPEAVComponentStateBase@2@@Z");
        NL_RESOLVE(onComponentsAdded, rl, "?onComponentsAdded@GameObjectState@r@@QEAAXXZ");
        NL_RESOLVE(getManager, rl, "?getInstance@GameObjectManager@r@@SAPEAV12@W4NetworkRole@net@@@Z");
        NL_RESOLVE(instantiate, rl, "?instantiate@GameObjectManager@r@@QEAAPEAVGameObject@2@PEAVGameObjectState@2@@Z");
        NL_RESOLVE(instantiateComponents, rl, "?instantiateComponents@GameObject@r@@AEAAXXZ");

        NL_RESOLVE(setContentPointer, core, "?setContentPointer@EntityComponentState@coregame@@QEAAXPEBVGenericComponent@content@@@Z");
        NL_RESOLVE(requestUpdate, core, "?requestUpdate@SpotLightComponentState@coregame@@AEAAXXZ");
        NL_RESOLVE(variableUpdate, core, "?updateVariableUpdate@SpotLightComponent@coregame@@QEAAXXZ");
        NL_RESOLVE(stateEnable, core, "?enable@SpotLightComponentState@coregame@@QEAAX_N@Z");
        NL_RESOLVE(stateSetColor, core, "?setColor@SpotLightComponentState@coregame@@QEAAXMMMM@Z");
        NL_RESOLVE(stateSetIntensity, core, "?setIntensity@SpotLightComponentState@coregame@@QEAAXMM@Z");
        NL_RESOLVE(stateSetCone, core, "?setConeAngle@SpotLightComponentState@coregame@@QEAAXMM@Z");
        NL_RESOLVE(stateSetNear, core, "?setNear@SpotLightComponentState@coregame@@QEAAXM@Z");

        NL_RESOLVE(setLightToWorld, renderer, "?setLightToWorld@SpotLightWrapper@rend@@QEAAXAEBV?$Matrix4x3Template@M@m@@@Z");
        NL_RESOLVE(setRadius, renderer, "?setRadius@SpotLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(setConeShape, renderer, "?setConeShape@SpotLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(setVolumetric, renderer, "?setVolumetricLight@SpotLightWrapper@rend@@QEAAX_N@Z");
        NL_RESOLVE(setVolumetricOnly, renderer, "?setVolumetricLightOnly@SpotLightWrapper@rend@@QEAAX_N@Z");
        NL_RESOLVE(setPhase, renderer, "?setVolumetricLightPhase@SpotLightWrapper@rend@@QEAAXMMM@Z");
        NL_RESOLVE(setVolumetricMultiplier, renderer, "?setVolumetricLightIntensityMultiplier@SpotLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(setDirectionality, renderer, "?setDirectionality@SpotLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(setScatter, renderer, "?setScatterIntensityMultiplier@SpotLightWrapper@rend@@QEAAXM@Z");

        NL_RESOLVE(pointStateEnable, core, "?enable@PointLightComponentState@coregame@@QEAAX_N@Z");
        NL_RESOLVE(pointSetColor, core, "?setColor@PointLightComponentState@coregame@@QEAAXMMMM@Z");
        NL_RESOLVE(pointSetIntensity, core, "?setIntensity@PointLightComponentState@coregame@@QEAAXMM@Z");
        NL_RESOLVE(pointRequestUpdate, core, "?requestUpdate@PointLightComponentState@coregame@@AEAAXXZ");
        NL_RESOLVE(pointVariableUpdate, core, "?updateVariableUpdate@PointLightComponent@coregame@@QEAAXXZ");
        NL_RESOLVE(pointSetMatrix, renderer, "?setMatrix@PointLightWrapper@rend@@QEAAXAEBV?$Matrix4x3Template@M@m@@@Z");
        NL_RESOLVE(pointSetRadius, renderer, "?setRadius@PointLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(pointWrapperSetIntensity, renderer, "?setIntensity@PointLightWrapper@rend@@QEAAXAEBV?$Vector3Template@M@m@@@Z");
        NL_RESOLVE(pointSetScatter, renderer, "?setScatterIntensityMultiplier@PointLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(pointSetRangeClip, renderer, "?setRangeClip@PointLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(pointSetMaxDrawDistance, renderer, "?setMaxDrawDistance@PointLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(pointSetDecay, renderer, "?setDecay@PointLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(pointSetDiameter, renderer, "?setDiameter@PointLightWrapper@rend@@QEAAXM@Z");
        NL_RESOLVE(pointSetCastShadows, renderer, "?setCastShadows@PointLightWrapper@rend@@QEAAX_N@Z");
        NL_RESOLVE(pointSetBentNormals, renderer, "?setBentNormals@PointLightWrapper@rend@@QEAAX_N@Z");

        g_api.renderViewGlobal = reinterpret_cast<const void**>(
            Export(g_api.coreshared, "?g_pRenderView@coreshared@@3PEBVView@m@@EB")
        );

#undef NL_RESOLVE

        const bool essential =
            g_api.constructClass &&
            g_api.processorCtor &&
            g_api.processorConstruct &&
            g_api.globalIDSet &&
            g_api.setNetOwnership &&
            g_api.globalIDMapExport &&
            g_api.registerID &&
            g_api.reserveComponents &&
            g_api.addComponent &&
            g_api.onComponentsAdded &&
            g_api.getManager &&
            g_api.instantiate &&
            g_api.instantiateComponents &&
            g_api.setContentPointer &&
            g_api.requestUpdate &&
            g_api.variableUpdate &&
            g_api.setLightToWorld &&
            g_api.renderViewGlobal;

        g_api.ready = essential;
        g_status = essential ? "Northlight light API ready" : "Northlight light API: missing export(s)";
        return essential;
    }

    static bool ReadCurrentCameraMatrix(Matrix4x3& out)
    {
        if (!ResolveApi() || !g_api.renderViewGlobal)
            return false;

        const void* viewPtr{};
        if (!SafeRead(g_api.renderViewGlobal, viewPtr) || !viewPtr)
            return false;

        const auto* view = reinterpret_cast<const uint8_t*>(viewPtr);
        for (int i = 0; i < 12; ++i)
        {
            double d{};
            if (!SafeRead(view + 0x70 + i * 8, d))
                return false;
            out.m[i] = static_cast<float>(d);
        }
        return true;
    }

    static bool ProcessGenericContent(void* content)
    {
        if (!content || !ResolveApi())
            return false;

        void* processor = VirtualAlloc(nullptr, 0x200, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!processor)
            return false;

        g_api.processorCtor(processor, 1, nullptr);
        g_api.processorConstruct(processor);

        uintptr_t* vt{};
        if (!SafeRead(content, vt) || !vt)
            return false;

        uintptr_t helperFnAddress{};
        if (!SafeRead(vt + (0xA0 / sizeof(uintptr_t)), helperFnAddress) || !helperFnAddress)
            return false;

        using GetHelperFn = void* (__fastcall*)(void*);
        auto getHelper = reinterpret_cast<GetHelperFn>(helperFnAddress);
        void* helper = getHelper(content);
        if (!helper)
            return false;

        uintptr_t* helperVT{};
        if (!SafeRead(helper, helperVT) || !helperVT)
            return false;

        uintptr_t processAddress{};
        if (!SafeRead(helperVT + (0xF0 / sizeof(uintptr_t)), processAddress) || !processAddress)
            return false;

        using ProcessFn = void(__fastcall*)(void*, void*, void*, int);
        reinterpret_cast<ProcessFn>(processAddress)(helper, content, processor, 0);
        return true;
    }

    static bool ProcessSpotContent(void* content)
    {
        if (!content || !ResolveApi())
            return false;

        void* processor = VirtualAlloc(nullptr, 0x200, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!processor)
            return false;

        g_api.processorCtor(processor, 1, nullptr);
        g_api.processorConstruct(processor);

        uintptr_t* vt{};
        if (!SafeRead(content, vt) || !vt)
            return false;

        uintptr_t processAddress{};
        if (!SafeRead(vt + (0xF8 / sizeof(uintptr_t)), processAddress) || !processAddress)
            return false;

        using ProcessFn = void(__fastcall*)(void*, void*, int);
        reinterpret_cast<ProcessFn>(processAddress)(content, processor, 0);
        return true;
    }

    static void ApplyContentDefaults(void* content)
    {
        if (!content)
            return;

        auto* p = reinterpret_cast<uint8_t*>(content);
        *reinterpret_cast<uint32_t*>(p + 0x18) = 1;
        *reinterpret_cast<uint32_t*>(p + 0x24) = 3;
        *reinterpret_cast<uint32_t*>(p + 0x28) = 2;
        *reinterpret_cast<uint32_t*>(p + 0x2C) = 1;
        *reinterpret_cast<uint32_t*>(p + 0x30) = 0;
        *reinterpret_cast<float*>(p + 0x38) = 20.0f;
        *reinterpret_cast<float*>(p + 0x3C) = 45.0f;
        *reinterpret_cast<float*>(p + 0x40) = 0.300f;
        *reinterpret_cast<float*>(p + 0x44) = 0.001f;
        *reinterpret_cast<float*>(p + 0x48) = 2.000f;
        *reinterpret_cast<float*>(p + 0x4C) = 0.500f;
        *reinterpret_cast<float*>(p + 0x50) = 0.000f;
        *reinterpret_cast<float*>(p + 0x54) = 1.000f;
        *reinterpret_cast<float*>(p + 0x58) = 0.000f;
        *reinterpret_cast<float*>(p + 0x5C) = 1.000f;
        *reinterpret_cast<float*>(p + 0x60) = 1.000f;
        *reinterpret_cast<float*>(p + 0x64) = 0.650f;
        *reinterpret_cast<float*>(p + 0x68) = 0.000f;
        *reinterpret_cast<float*>(p + 0x6C) = 0.500f;
        *reinterpret_cast<float*>(p + 0x7C) = 0.004f;
        p[0x80] = 0;
        p[0x82] = 1;
        p[0x83] = 1;
        p[0x84] = 1;
    }

    static bool BuildGlobalID(void* object, const std::string& name, uint64_t& outId)
    {
        if (!object || !ResolveApi())
            return false;

        uint64_t id{};
        g_api.globalIDSet(&id, 0xE3, name.c_str(), 0);
        g_api.setNetOwnership(&id, 1);
        outId = id;

        *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(object) + 0x18) = id;

        void* map{};
        if (!SafeRead(g_api.globalIDMapExport, map) || !map)
            return false;

        g_api.registerID(map, reinterpret_cast<const uint64_t*>(reinterpret_cast<uint8_t*>(object) + 0x18), object);
        return true;
    }

    static uint32_t PackColor(const float color[3])
    {
        auto cv = [](float v) -> uint32_t
        {
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            return static_cast<uint32_t>(v * 255.0f + 0.5f);
        };

        const uint32_t r = cv(color[0]);
        const uint32_t g = cv(color[1]);
        const uint32_t b = cv(color[2]);
        return 0xFF000000u | (r << 16) | (g << 8) | b;
    }

    static bool CallSpotGameLoad(void* spotState)
    {
        if (!spotState)
            return false;

        uintptr_t* vt{};
        if (!SafeRead(spotState, vt) || !vt)
            return false;

        uintptr_t fn{};
        if (!SafeRead(vt + (0x270 / sizeof(uintptr_t)), fn) || !fn)
            return false;

        using VoidMethod = void(__fastcall*)(void*);
        reinterpret_cast<VoidMethod>(fn)(spotState);
        return true;
    }

    static void WriteContentValues(SpawnedSpot& s)
    {
        if (!s.spotContent)
            return;

        auto* p = reinterpret_cast<uint8_t*>(s.spotContent);

        // This is the native content layout used by the spotlight editor.
        *reinterpret_cast<uint32_t*>(p + 0x18) = static_cast<uint32_t>(s.shadowmapType);
        *reinterpret_cast<uint32_t*>(p + 0x24) = static_cast<uint32_t>(s.shadowmapSize);
        *reinterpret_cast<uint32_t*>(p + 0x28) = static_cast<uint32_t>(s.volumetricQuality);
        *reinterpret_cast<uint32_t*>(p + 0x2C) = static_cast<uint32_t>(s.volumetricShape);
        *reinterpret_cast<uint32_t*>(p + 0x30) = static_cast<uint32_t>(s.projectionType);
        *reinterpret_cast<uint32_t*>(p + 0x34) = PackColor(s.color);

        // Master Enabled is implemented by forcing effective intensity to zero.
        // This is immediate while Photo Mode is paused, unlike State::enable().
        *reinterpret_cast<float*>(p + 0x38) = s.enabled ? s.intensity : 0.0f;
        *reinterpret_cast<float*>(p + 0x3C) = s.fov;
        *reinterpret_cast<float*>(p + 0x40) = s.nearPlane;
        *reinterpret_cast<float*>(p + 0x44) = s.depthBias;
        *reinterpret_cast<float*>(p + 0x48) = s.depthSlopeBias;
        *reinterpret_cast<float*>(p + 0x4C) = s.radius;
        *reinterpret_cast<float*>(p + 0x50) = s.aspectRatio;
        *reinterpret_cast<float*>(p + 0x54) = s.farPlane;
        *reinterpret_cast<float*>(p + 0x58) = s.directionality;
        *reinterpret_cast<float*>(p + 0x5C) = s.scatter;
        *reinterpret_cast<float*>(p + 0x60) = s.volumetricMultiplier;
        *reinterpret_cast<float*>(p + 0x64) = s.phase0;
        *reinterpret_cast<float*>(p + 0x68) = s.phase1;
        *reinterpret_cast<float*>(p + 0x6C) = s.phaseBlend;
        *reinterpret_cast<float*>(p + 0x7C) = s.shadowFilterSize;

        p[0x80] = s.farClipping ? 1 : 0;
        p[0x82] = s.bentNormals ? 1 : 0;
        p[0x83] = (s.enabled && s.volumetric) ? 1 : 0;
        p[0x84] = (s.enabled && s.directLighting) ? 1 : 0;
    }

    static void ApplyRuntimeValues(SpawnedSpot& s)
    {
        if (!s.spotContent || !s.spotState || !s.runtime || !s.wrapper || !ResolveApi())
            return;

        if (s.type == LightType::Point)
        {
            if (!g_api.pointSetIntensity || !g_api.pointSetColor || !g_api.pointWrapperSetIntensity || !g_api.pointRequestUpdate ||
                !g_api.pointVariableUpdate || !g_api.pointSetMatrix || !g_api.pointSetRadius)
            { g_status = "Point Light API incomplete"; return; }
            // Do NOT call PointLightComponentState::enable(false) here: it can
            // remove a light from the renderer and re-enabling may require a
            // separate registration lifecycle. Keep state alive at its authored
            // intensity and mute/unmute only the renderer wrapper below.
            const float intensity = s.intensity;
            g_api.pointSetColor(s.spotState, s.color[0], s.color[1], s.color[2], 1.0f);
            g_api.pointSetIntensity(s.spotState, intensity, intensity);
            g_api.pointRequestUpdate(s.spotState);
            g_api.pointVariableUpdate(s.runtime);
            g_api.pointSetRadius(s.wrapper, s.radius);
            if (g_api.pointSetScatter) g_api.pointSetScatter(s.wrapper, s.scatter);
            if (s.editPointRange && g_api.pointSetRangeClip) g_api.pointSetRangeClip(s.wrapper, s.pointRange);
            if (s.editPointDrawDistance && g_api.pointSetMaxDrawDistance) g_api.pointSetMaxDrawDistance(s.wrapper, s.pointDrawDistance);
            if (s.editPointDecay && g_api.pointSetDecay) g_api.pointSetDecay(s.wrapper, s.pointDecay);
            if (s.editPointDiameter && g_api.pointSetDiameter) g_api.pointSetDiameter(s.wrapper, s.pointDiameter);
            if (s.editPointShadows && g_api.pointSetCastShadows) g_api.pointSetCastShadows(s.wrapper, s.pointShadows);
            if (s.editPointBentNormals && g_api.pointSetBentNormals) g_api.pointSetBentNormals(s.wrapper, s.pointBentNormals);
            if (s.hasTransform) g_api.pointSetMatrix(s.wrapper, s.transform);
            // Apply color again after the renderer update: the state-to-wrapper
            // synchronization may replace colors assigned before variableUpdate.
            g_api.pointSetColor(s.spotState, s.color[0], s.color[1], s.color[2], 1.0f);
            // Renderer-side master mute. Vector3 is three contiguous floats;
            // use per-channel intensity so an OFF->ON transition does not
            // require re-creating/re-registering the PointLight component.
            if (g_api.pointWrapperSetIntensity)
            {
                const float rgb[3] = {
                    s.enabled ? s.intensity * s.color[0] : 0.0f,
                    s.enabled ? s.intensity * s.color[1] : 0.0f,
                    s.enabled ? s.intensity * s.color[2] : 0.0f
                };
                g_api.pointWrapperSetIntensity(s.wrapper, rgb);
            }
            g_status = "Selected Point Light updated";
            return;
        }

        // The native editor does not drive these values through the individual State setters.
        // It edits content, calls gameLoad(), requestUpdate(), updateVariableUpdate(),
        // then refreshes the renderer-only fields below. Mirroring that path also
        // fixes FOV updates while the game is paused.
        WriteContentValues(s);

        CallSpotGameLoad(s.spotState);
        g_api.requestUpdate(s.spotState);
        g_api.variableUpdate(s.runtime);

        if (g_api.setConeShape)
            g_api.setConeShape(s.wrapper, s.aspectRatio);
        if (g_api.setDirectionality)
            g_api.setDirectionality(s.wrapper, s.directionality);
        if (g_api.setRadius)
            g_api.setRadius(s.wrapper, s.radius);
        if (g_api.setVolumetric)
            g_api.setVolumetric(s.wrapper, s.enabled && s.volumetric);
        if (g_api.setVolumetricOnly)
            g_api.setVolumetricOnly(s.wrapper, s.enabled ? !s.directLighting : false);
        if (g_api.setPhase)
            g_api.setPhase(s.wrapper, s.phase0, s.phase1, s.phaseBlend);
        if (g_api.setVolumetricMultiplier)
            g_api.setVolumetricMultiplier(s.wrapper, s.volumetricMultiplier);
        if (g_api.setScatter)
            g_api.setScatter(s.wrapper, s.scatter);

        if (s.hasTransform && g_api.setLightToWorld)
            g_api.setLightToWorld(s.wrapper, s.transform);

        g_status = "Selected light updated";
    }

    static bool PlaceAtCamera(SpawnedSpot& s)
    {
        if (!s.wrapper || !ResolveApi())
            return false;

        Matrix4x3 matrix{};
        if (!ReadCurrentCameraMatrix(matrix))
        {
            g_status = "RenderView unavailable";
            return false;
        }

        s.transform = matrix;
        s.hasTransform = true;

        if (s.type == LightType::Point)
        {
            if (!g_api.pointSetMatrix || !g_api.pointRequestUpdate || !g_api.pointVariableUpdate)
                return false;
            g_api.pointSetMatrix(s.wrapper, s.transform);
            g_api.pointRequestUpdate(s.spotState);
            g_api.pointVariableUpdate(s.runtime);
            g_api.pointSetMatrix(s.wrapper, s.transform);
        }
        else
        {
            g_api.setLightToWorld(s.wrapper, s.transform);
            g_api.requestUpdate(s.spotState);
            g_api.variableUpdate(s.runtime);
            g_api.setLightToWorld(s.wrapper, s.transform);
        }
        // Match the successful intensity-slider path after every placement.
        // PointLightWrapper::setMatrix alone updates the transform but can leave
        // the rendered light stale until setIntensity(Vector3) runs.
        ApplyRuntimeValues(s);
        g_status = "Placed selected light at current camera (renderer updated)";
        return true;
    }

    // Northlight Matrix4x3: three basis rows followed by XYZ translation.
    // Rotation is edited as an incremental delta, keeping the original camera matrix
    // conventions instead of attempting to reconstruct an unverified absolute Euler basis.
    static void RotateBasisLocal(Matrix4x3& m, int axis, float degrees)
    {
        const float r = degrees * 0.01745329251994329577f;
        const float c = std::cos(r), sn = std::sin(r);
        const int a = (axis == 0) ? 1 : 0;
        const int b = (axis == 2) ? 1 : 2;
        // pitch: Y/Z, yaw: X/Z, roll: X/Y
        for (int k = 0; k < 3; ++k)
        {
            const float x = m.m[a * 3 + k], y = m.m[b * 3 + k];
            m.m[a * 3 + k] = c*x - sn*y;
            m.m[b * 3 + k] = sn*x + c*y;
        }
    }

    static void DrawTransformEditor(SpawnedSpot& s)
    {
        if (!ImGui::CollapsingHeader("Transform (XYZ / PYR)", ImGuiTreeNodeFlags_DefaultOpen)) return;
        if (!s.hasTransform) { ImGui::TextDisabled("No transform recorded"); return; }

        // Use one line per vector; native matrix and CONTROL's Y/Z mapping stay unchanged.
        const float fieldWidth = (ImGui::GetContentRegionAvail().x - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
        bool positionChanged = false;
        ImGui::PushItemWidth(fieldWidth);
        positionChanged |= ImGui::DragFloat("X##LightXYZ", &s.transform.m[9], 0.05f, 0.0f, 0.0f, "X %.3f");
        ImGui::SameLine();
        positionChanged |= ImGui::DragFloat("Y##LightXYZ", &s.transform.m[11], 0.05f, 0.0f, 0.0f, "Y %.3f");
        ImGui::SameLine();
        positionChanged |= ImGui::DragFloat("Z##LightXYZ", &s.transform.m[10], 0.05f, 0.0f, 0.0f, "Z %.3f");
        if (positionChanged) ApplyRuntimeValues(s);

        // PYR edits remain incremental local-axis rotations, as in v0.4t.
        static float pitchStep = 0.0f, yawStep = 0.0f, rollStep = 0.0f;
        bool rot = false;
        if (ImGui::DragFloat("##PitchDeltaLight", &pitchStep, 0.25f, -180.f, 180.f, "P %.2f")) { RotateBasisLocal(s.transform, 0, pitchStep); pitchStep=0; rot=true; }
        ImGui::SameLine();
        if (ImGui::DragFloat("##YawDeltaLight", &yawStep, 0.25f, -180.f, 180.f, "Y %.2f")) { RotateBasisLocal(s.transform, 1, yawStep); yawStep=0; rot=true; }
        ImGui::SameLine();
        if (ImGui::DragFloat("##RollDeltaLight", &rollStep, 0.25f, -180.f, 180.f, "R %.2f")) { RotateBasisLocal(s.transform, 2, rollStep); rollStep=0; rot=true; }
        ImGui::PopItemWidth();
        if (rot) ApplyRuntimeValues(s);
    }

    static bool SpawnAtCamera(LightType type)
    {
        if (!ResolveApi())
            return false;

        SpawnedSpot s{};
        s.type = type;
        if (type == LightType::Point &&
            (!g_api.pointSetIntensity || !g_api.pointSetColor || !g_api.pointRequestUpdate ||
             !g_api.pointVariableUpdate || !g_api.pointSetMatrix || !g_api.pointSetRadius))
        { g_status = "Point Light native exports missing"; return false; }

        s.genericContent = g_api.constructClass("content::GenericEntity");
        if (!s.genericContent || !ProcessGenericContent(s.genericContent))
        {
            g_status = "GenericEntity processing failed";
            return false;
        }

        s.spotContent = g_api.constructClass(type == LightType::Point ? "content::PointLightComponent" : "content::SpotLightComponent");
        if (!s.spotContent || !ProcessSpotContent(s.spotContent))
        {
            g_status = type == LightType::Point ? "PointLight content processing failed" : "SpotLight content processing failed";
            return false;
        }

        if (type == LightType::Spot) ApplyContentDefaults(s.spotContent);

        s.genericState = g_api.constructClass("GenericEntityState");
        if (!s.genericState)
        {
            g_status = "GenericEntityState construction failed";
            return false;
        }

        using OwnerFn = void* (__fastcall*)(void*);
        using OwnerInitFn = void(__fastcall*)(void*, void*);
        using AttachContentFn = void(__fastcall*)(void*, void*);
        using VoidMethod = void(__fastcall*)(void*);

        uintptr_t* genericVT{};
        if (!SafeRead(s.genericState, genericVT) || !genericVT)
        {
            g_status = "GenericState vtable missing";
            return false;
        }

        uintptr_t fn{};
        if (!SafeRead(genericVT + (0xA8 / sizeof(uintptr_t)), fn) || !fn)
        {
            g_status = "Generic owner resolver missing";
            return false;
        }
        void* genericOwner = reinterpret_cast<OwnerFn>(fn)(s.genericState);
        if (!genericOwner)
        {
            g_status = "Generic owner missing";
            return false;
        }

        uintptr_t* genericOwnerVT{};
        if (!SafeRead(genericOwner, genericOwnerVT) || !genericOwnerVT)
            return false;
        uintptr_t ownerInitAddress{};
        uintptr_t attachAddress{};
        if (!SafeRead(genericOwnerVT + (0x60 / sizeof(uintptr_t)), ownerInitAddress) || !ownerInitAddress)
            return false;
        if (!SafeRead(genericVT + (0x300 / sizeof(uintptr_t)), attachAddress) || !attachAddress)
            return false;
        reinterpret_cast<OwnerInitFn>(ownerInitAddress)(genericOwner, s.genericState);
        reinterpret_cast<AttachContentFn>(attachAddress)(s.genericState, s.genericContent);

        const uint64_t serial = g_serial.fetch_add(1);
        const std::string genericName = "IGCSDOF_Generic_" + std::to_string(GetTickCount64()) + "_" + std::to_string(serial);
        if (!BuildGlobalID(s.genericState, genericName, s.genericId))
        {
            g_status = "Generic GlobalID failed";
            return false;
        }

        g_api.reserveComponents(s.genericState, 1);

        s.spotState = g_api.constructClass(type == LightType::Point ? "coregame::PointLightComponentState" : "coregame::SpotLightComponentState");
        if (!s.spotState)
        {
            g_status = type == LightType::Point ? "PointLightComponentState construction failed" : "SpotLightComponentState construction failed";
            return false;
        }

        uintptr_t* spotVT{};
        if (!SafeRead(s.spotState, spotVT) || !spotVT)
            return false;
        if (!SafeRead(spotVT + (0xA8 / sizeof(uintptr_t)), fn) || !fn)
            return false;
        void* spotOwner = reinterpret_cast<OwnerFn>(fn)(s.spotState);
        if (!spotOwner)
        {
            g_status = "Spot owner missing";
            return false;
        }

        uintptr_t* spotOwnerVT{};
        if (!SafeRead(spotOwner, spotOwnerVT) || !spotOwnerVT)
            return false;
        if (!SafeRead(spotOwnerVT + (0x60 / sizeof(uintptr_t)), ownerInitAddress) || !ownerInitAddress)
            return false;
        reinterpret_cast<OwnerInitFn>(ownerInitAddress)(spotOwner, s.spotState);
        g_api.setContentPointer(s.spotState, s.spotContent);

        const std::string spotName = (type == LightType::Point ? "IGCSDOF_Point_" : "IGCSDOF_Spot_") + std::to_string(GetTickCount64()) + "_" + std::to_string(serial);
        if (!BuildGlobalID(s.spotState, spotName, s.spotId))
        {
            g_status = "Spot GlobalID failed";
            return false;
        }

        g_api.addComponent(s.genericState, s.spotState);
        g_api.onComponentsAdded(s.genericState);

        s.manager = g_api.getManager(1);
        if (!s.manager)
        {
            g_status = "GameObjectManager missing";
            return false;
        }

        s.gameObject = g_api.instantiate(s.manager, s.genericState);
        if (!s.gameObject)
        {
            g_status = "GameObject instantiate failed";
            return false;
        }

        g_api.instantiateComponents(s.gameObject);
        if (!SafeRead(reinterpret_cast<uint8_t*>(s.spotState) + 0x20, s.runtime) || !s.runtime)
        {
            g_status = "Runtime SpotLightComponent missing";
            return false;
        }

        // Spot lifecycle is validated; Point uses corresponding native virtual lifecycle.
        if (!SafeRead(spotVT + (0x270 / sizeof(uintptr_t)), fn) || !fn)
            return false;
        reinterpret_cast<VoidMethod>(fn)(s.spotState); // SpotLightComponentState::gameLoad

        uintptr_t* gameObjectVT{};
        if (!SafeRead(s.gameObject, gameObjectVT) || !gameObjectVT)
            return false;
        if (!SafeRead(gameObjectVT + (0xC0 / sizeof(uintptr_t)), fn) || !fn)
            return false;
        reinterpret_cast<VoidMethod>(fn)(s.gameObject);
        if (!SafeRead(gameObjectVT + (0xD8 / sizeof(uintptr_t)), fn) || !fn)
            return false;
        reinterpret_cast<VoidMethod>(fn)(s.gameObject);

        if (!SafeRead(s.genericState, genericVT) || !genericVT)
            return false;
        if (!SafeRead(genericVT + (0x2C8 / sizeof(uintptr_t)), fn) || !fn)
            return false;
        reinterpret_cast<VoidMethod>(fn)(s.genericState);

        if (!SafeRead(gameObjectVT + (0x110 / sizeof(uintptr_t)), fn) || !fn)
            return false;
        reinterpret_cast<VoidMethod>(fn)(s.gameObject);

        if (!SafeRead(s.spotState, spotVT) || !spotVT)
            return false;
        if (!SafeRead(spotVT + (0x270 / sizeof(uintptr_t)), fn) || !fn)
            return false;
        reinterpret_cast<VoidMethod>(fn)(s.spotState);

        if (!SafeRead(reinterpret_cast<uint8_t*>(s.runtime) + 0x20, s.wrapper) || !s.wrapper)
        {
            g_status = "SpotLightWrapper missing";
            return false;
        }
        SafeRead(reinterpret_cast<uint8_t*>(s.runtime) + 0x28, s.traveller);

        ApplyRuntimeValues(s);
        PlaceAtCamera(s);

        uint32_t goIndex{0xFFFFFFFFu};
        uint32_t runtimeIndex{0xFFFFFFFFu};
        SafeRead(reinterpret_cast<uint8_t*>(s.gameObject) + 0x10, goIndex);
        SafeRead(reinterpret_cast<uint8_t*>(s.runtime) + 0x10, runtimeIndex);
        if (goIndex == 0xFFFFFFFFu || runtimeIndex == 0xFFFFFFFFu)
        {
            g_status = "Spawned, but lifecycle registration indices are invalid";
            return false;
        }

        g_spots.push_back(s);
        g_selected = static_cast<int>(g_spots.size()) - 1;

        char b[192]{};
        sprintf_s(b, "Created %s Light at camera (runtime=%u, gameObject=%u)", type == LightType::Point ? "Point" : "Spot", runtimeIndex, goIndex);
        g_status = b;
        return true;
    }


    void DrawUI()
    {
        ImGui::TextWrapped("%s", g_status.c_str());
        ImGui::Separator();

        static int createType = 0;
        ImGui::SetNextItemWidth(110.0f);
        ImGui::Combo("##NewLightType", &createType, "Spot\0Point / Omni\0");
        ImGui::SameLine();
        if (ImGui::Button("Create Light"))
            SpawnAtCamera(createType == 1 ? LightType::Point : LightType::Spot);
        ImGui::SameLine();
        if (ImGui::Button("Resolve Light API"))
            ResolveApi();

        if (g_spots.empty())
        {
            ImGui::Spacing();
            ImGui::TextDisabled("No spawned lights yet.");
            ImGui::TextDisabled("Nothing is resolved or spawned during DLL injection; the light path only runs when you press Create.");
            return;
        }

        if (g_selected < 0 || g_selected >= static_cast<int>(g_spots.size()))
            g_selected = static_cast<int>(g_spots.size()) - 1;

        ImGui::Spacing();
        ImGui::Text("Spawned lights: %d", static_cast<int>(g_spots.size()));
        for (int i = 0; i < static_cast<int>(g_spots.size()); ++i)
        {
            char label[64]{};
            sprintf_s(label, "%s Light %d", g_spots[i].type == LightType::Point ? "Point" : "Spot", i + 1);
            if (ImGui::Selectable(label, g_selected == i))
                g_selected = i;
        }

        auto& s = g_spots[g_selected];
        ImGui::Separator();

        DrawTransformEditor(s);
        ImGui::Spacing();
        bool changed = false;

        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed |= ImGui::Checkbox("Enabled (master)", &s.enabled);
            changed |= ImGui::DragFloat("Intensity", &s.intensity, 0.10f, 0.0f, 0.0f, "%.2f");
            changed |= ImGui::ColorEdit3("Color", s.color);
            if (s.type == LightType::Spot)
            {
            changed |= ImGui::DragFloat("Field of view", &s.fov, 0.10f, 0.1f, 179.0f, "%.2f");
            }
            changed |= ImGui::DragFloat(s.type == LightType::Point ? "Source radius (area)" : "Area light radius", &s.radius, 0.01f, 0.0f, 10.0f, "%.3f");
            if (s.type == LightType::Spot)
            {
            changed |= ImGui::DragFloat("Aspect ratio", &s.aspectRatio, 0.01f, 0.0f, 2.0f, "%.3f");
            changed |= ImGui::DragFloat("Directionality", &s.directionality, 0.01f, 0.0f, 1.0f, "%.3f");
            changed |= ImGui::DragFloat("Near plane", &s.nearPlane, 0.01f, 0.1f, 100.0f, "%.3f");
            changed |= ImGui::DragFloat("Far plane", &s.farPlane, 0.01f, 0.1f, 100.0f, "%.3f");

            static const char* projectionItems[] = { "Perspective", "Stereographic" };
            changed |= ImGui::Combo("Projection type", &s.projectionType, projectionItems, IM_ARRAYSIZE(projectionItems));

            changed |= ImGui::Checkbox("Bent Normals", &s.bentNormals);
            changed |= ImGui::Checkbox("Enable direct lighting", &s.directLighting);
            changed |= ImGui::Checkbox("Enable far clipping", &s.farClipping);
            }
        }

        if (s.type == LightType::Point && ImGui::CollapsingHeader("Point Light Range", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("Source radius controls light size, not reach.");
            if (g_api.pointSetRangeClip && ImGui::DragFloat("Range clip", &s.pointRange, 0.1f, 0.0f, 0.0f, "%.2f")) { s.editPointRange = true; changed = true; }
            if (g_api.pointSetMaxDrawDistance && ImGui::DragFloat("Max draw distance", &s.pointDrawDistance, 1.0f, 0.0f, 0.0f, "%.2f")) { s.editPointDrawDistance = true; changed = true; }
            if (g_api.pointSetDecay && ImGui::DragFloat("Decay", &s.pointDecay, 0.01f, 0.0f, 0.0f, "%.3f")) { s.editPointDecay = true; changed = true; }
            if (g_api.pointSetDiameter && ImGui::DragFloat("Diameter", &s.pointDiameter, 0.01f, 0.0f, 0.0f, "%.3f")) { s.editPointDiameter = true; changed = true; }
        }

        if (s.type == LightType::Point && ImGui::CollapsingHeader("Volumetric", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (g_api.pointSetScatter)
                changed |= ImGui::DragFloat("Point scatter intensity multiplier", &s.scatter, 0.05f, 0.0f, 100.0f, "%.3f");
        }

        if (s.type == LightType::Point && ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (g_api.pointSetCastShadows && ImGui::Checkbox("Cast shadows", &s.pointShadows)) { s.editPointShadows = true; changed = true; }
            if (g_api.pointSetBentNormals && ImGui::Checkbox("Bent Normals", &s.pointBentNormals)) { s.editPointBentNormals = true; changed = true; }
        }

        if (s.type == LightType::Spot && ImGui::CollapsingHeader("Volumetric", ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed |= ImGui::Checkbox("Enable volumetric lighting", &s.volumetric);
            changed |= ImGui::DragFloat("Scattered light multiplier", &s.scatter, 0.01f, 0.0f, 10.0f, "%.3f");
            changed |= ImGui::DragFloat("Volumetric effect multiplier", &s.volumetricMultiplier, 0.01f, 0.0f, 10.0f, "%.3f");
            changed |= ImGui::DragFloat("Scattering lobe #1", &s.phase0, 0.01f, 0.0f, 10.0f, "%.3f");
            changed |= ImGui::DragFloat("Scattering lobe #2", &s.phase1, 0.01f, 0.0f, 10.0f, "%.3f");
            changed |= ImGui::DragFloat("Scattering lobe blend", &s.phaseBlend, 0.01f, 0.0f, 1.0f, "%.3f");

            static const char* qualityItems[] = { "Low", "Medium", "High" };
            changed |= ImGui::Combo("Volumetric quality", &s.volumetricQuality, qualityItems, IM_ARRAYSIZE(qualityItems));

            static const char* shapeItems[] = { "Cone", "Square Frustum" };
            changed |= ImGui::Combo("Volumetric shape", &s.volumetricShape, shapeItems, IM_ARRAYSIZE(shapeItems));
        }

        if (s.type == LightType::Spot && ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed |= ImGui::DragFloat("Depth bias", &s.depthBias, 0.00005f, 0.0f, 0.01f, "%.6f");
            changed |= ImGui::DragFloat("Depth slope bias", &s.depthSlopeBias, 0.01f, 0.1f, 16.0f, "%.3f");
            changed |= ImGui::DragFloat("Shadow filter size", &s.shadowFilterSize, 0.0001f, 0.0f, 10.0f, "%.4f");

            static const char* shadowTypeItems[] = { "Off", "PCF", "VSM" };
            changed |= ImGui::Combo("Shadowmap type", &s.shadowmapType, shadowTypeItems, IM_ARRAYSIZE(shadowTypeItems));

            static const char* shadowSizeItems[] = { "128", "256", "512", "1024" };
            changed |= ImGui::Combo("Shadowmap size", &s.shadowmapSize, shadowSizeItems, IM_ARRAYSIZE(shadowSizeItems));
        }

        if (changed)
            ApplyRuntimeValues(s);

        if (ImGui::Button("Place Selected at Current Camera"))
            PlaceAtCamera(s);

        uint32_t goIndex{0xFFFFFFFFu};
        uint32_t runtimeIndex{0xFFFFFFFFu};
        if (s.gameObject)
            SafeRead(reinterpret_cast<uint8_t*>(s.gameObject) + 0x10, goIndex);
        if (s.runtime)
            SafeRead(reinterpret_cast<uint8_t*>(s.runtime) + 0x10, runtimeIndex);

        ImGui::Spacing();
        ImGui::TextDisabled("GameObject=%p  index=%u", s.gameObject, goIndex);
        ImGui::TextDisabled("Runtime=%p  index=%u", s.runtime, runtimeIndex);
        ImGui::TextDisabled("Wrapper=%p  Traveller=%p", s.wrapper, s.traveller);
        ImGui::TextDisabled("Native Delete unavailable: safe unregister/destruction sequence unverified.");
    }
}
