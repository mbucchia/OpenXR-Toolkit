// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri
// Copyright(c) 2021-2022 Jean-Luc Dupiot - Reality XP
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

// Helper to detour a function.
template <typename TMethod>
void DetourFunctionAttach(TMethod target, TMethod hooked, TMethod& original) {
    if (original) {
        // Already hooked.
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    original = target;
    DetourAttach((PVOID*)&original, hooked);

    CHECK_MSG(DetourTransactionCommit() == NO_ERROR, "Detour failed");
}

template <typename TMethod>
void DetourFunctionDetach(TMethod target, TMethod hooked, TMethod& original) {
    if (!original) {
        // Not hooked.
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourDetach((PVOID*)&original, hooked);

    CHECK_MSG(DetourTransactionCommit() == NO_ERROR, "Detour failed");

    original = nullptr;
}

// Helper to detour a function in a DLL.
template <typename TMethod>
void DetourDllAttach(const char* dll, const char* target, TMethod hooked, TMethod& original) {
    if (original) {
        // Already hooked.
        return;
    }

    HMODULE handle;
    CHECK_MSG(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, dll, &handle), "Failed to get DLL handle");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    original = (TMethod)GetProcAddress(handle, target);
    CHECK_MSG(original, "Failed to resolve symbol");
    DetourAttach((PVOID*)&original, hooked);

    CHECK_MSG(DetourTransactionCommit() == NO_ERROR, "Detour failed");
}

template <typename TMethod>
void DetourDllDetach(const char* dll, const char* target, TMethod hooked, TMethod& original) {
    if (!original) {
        // Not hooked.
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourDetach((PVOID*)&original, hooked);

    CHECK_MSG(DetourTransactionCommit() == NO_ERROR, "Detour failed");

    original = nullptr;
}

// Helper to detour a class method.
template <class T, typename TMethod>
void DetourMethodAttach(T* instance, unsigned int methodOffset, TMethod hooked, TMethod& original) {
    if (original) {
        // Already hooked.
        return;
    }

    LPVOID* vtable = *((LPVOID**)instance);
    LPVOID target = vtable[methodOffset];

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    original = (TMethod)target;
    DetourAttach((PVOID*)&original, hooked);

    CHECK_MSG(DetourTransactionCommit() == NO_ERROR, "Detour failed");
}

template <class T, typename TMethod>
void DetourMethodDetach(T* instance, unsigned int methodOffset, TMethod hooked, TMethod& original) {
    if (!original) {
        // Not hooked.
        return;
    }

    LPVOID* vtable = *((LPVOID**)instance);
    LPVOID target = vtable[methodOffset];

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourDetach((PVOID*)&original, hooked);

    CHECK_MSG(DetourTransactionCommit() == NO_ERROR, "Detour failed");

    original = nullptr;
}

#define DECLARE_DETOUR_FUNCTION(ReturnType, Callconv, FunctionName, ...)                                               \
    inline ReturnType(Callconv* g_original_##FunctionName)(##__VA_ARGS__) = nullptr;                                   \
    ReturnType Callconv hooked_##FunctionName(##__VA_ARGS__)
