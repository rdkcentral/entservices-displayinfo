/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Stub implementation of the displayinfo-soc library for platforms that do not
 * provide virtual/vendor-displayinfo-soc (e.g. vdevice_x86-64-mw).
 * There is no GPU / graphics plane to query, so every value is reported as
 * "unavailable" (0), which is also what the Broadcom implementation returns
 * when its /proc entries cannot be read.
 */

#include "../SoC_abstraction.h"

uint64_t SoC_GetTotalGpuRam()
{
    return 0;
}

uint64_t SoC_GetFreeGpuRam()
{
    return 0;
}

uint32_t SoC_GetGraphicsWidth()
{
    return 0;
}

uint32_t SoC_GetGraphicsHeight()
{
    return 0;
}
