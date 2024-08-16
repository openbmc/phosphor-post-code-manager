/*
// Copyright (c) 2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#include "post_code.hpp"

#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/vector.hpp>

#include <iomanip>

const static constexpr auto timeoutMicroSeconds = 1000000;

void PostCode::deleteAll()
{
    std::uintmax_t n = fs::remove_all(postCodeListPath);
    std::cerr << "clearPostCodes deleted " << n << " files in "
              << postCodeListPath << std::endl;
    fs::create_directories(postCodeListPath);
    postCodes.clear();
    currentBootCycleIndex = 0;
    currentBootCycleCount(0);
}

std::vector<postcode_t> PostCode::getPostCodes(uint16_t index)
{
    std::vector<postcode_t> codesVec;
    if (1 == index && !postCodes.empty())
    {
        std::transform(postCodes.begin(), postCodes.end(),
                       std::back_inserter(codesVec),
                       [](const auto& kv) { return kv.second; });
    }
    else
    {
        uint16_t bootNum = getBootNum(index);

        decltype(postCodes) codes;
        deserializePostCodes(postCodeListPath / std::to_string(bootNum), codes);
        std::transform(codes.begin(), codes.end(), std::back_inserter(codesVec),
                       [](const auto& kv) { return kv.second; });
    }
    return codesVec;
}

std::map<uint64_t, postcode_t>
    PostCode::getPostCodesWithTimeStamp(uint16_t index)
{
    if (1 == index && !postCodes.empty())
    {
        return postCodes;
    }

    uint16_t bootNum = getBootNum(index);
    decltype(postCodes) codes;
    deserializePostCodes(postCodeListPath / std::to_string(bootNum), codes);
    return codes;
}

void PostCode::savePostCodes(postcode_t code)
{
    if (!timer)
    {
        timer = std::make_unique<sdbusplus::Timer>(event.get(), [this]() {
            serialize(postCodeListPath);
        });
    }

    // steady_clock is a monotonic clock that is guaranteed to never be adjusted
    auto postCodeTimeSteady = std::chrono::steady_clock::now();
    uint64_t tsUS = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

    if (postCodes.empty())
    {
        firstPostCodeTimeSteady = postCodeTimeSteady;
        firstPostCodeUsSinceEpoch = tsUS; // uS since epoch for 1st post code
        incrBootCycle();
    }
    else
    {
        // calculating tsUS so it is monotonic within the same boot
        tsUS = firstPostCodeUsSinceEpoch +
               std::chrono::duration_cast<std::chrono::microseconds>(
                   postCodeTimeSteady - firstPostCodeTimeSteady)
                   .count();
    }

    postCodes.insert(std::make_pair(tsUS, code));
    if (postCodes.size() > MAX_POST_CODE_SIZE_PER_CYCLE)
    {
        postCodes.erase(postCodes.begin());
    }

    if (!timer->isRunning())
    {
        timer->start(std::chrono::microseconds(timeoutMicroSeconds));
    }

    if (strlen(POSTCODE_DISPLAY_PATH) > 0)
    {
        std::string postCodeDisplayPath =
            POSTCODE_DISPLAY_PATH + std::to_string(node);

        std::ofstream postCodeDisplayFile(postCodeDisplayPath);
        postCodeDisplayFile << "0x" << std::setfill('0') << std::setw(2)
                            << std::hex << std::get<0>(code);
        postCodeDisplayFile.close();
    }

#ifdef ENABLE_BIOS_POST_CODE_LOG
    uint64_t usTimeOffset = tsUS - firstPostCodeUsSinceEpoch;
    std::ostringstream hexCode;
    hexCode << "0x" << std::setfill('0') << std::setw(2) << std::hex
            << std::get<0>(code);

    std::ostringstream timeOffsetStr;
    // Set Fixed-Point Notation
    timeOffsetStr << std::fixed;
    // Set precision to 4 digits
    timeOffsetStr << std::setprecision(4);
    // Add double to stream
    timeOffsetStr << static_cast<double>(usTimeOffset) / 1000 / 1000;

    phosphor::logging::log<phosphor::logging::level::INFO>(
        "BIOS POST Code",
        phosphor::logging::entry("REDFISH_MESSAGE_ID=%s",
                                 "OpenBMC.0.2.BIOSPOSTCode"),
        phosphor::logging::entry(
            "REDFISH_MESSAGE_ARGS=%d,%s,%s", currentBootCycleIndex,
            timeOffsetStr.str().c_str(), hexCode.str().c_str()));
#endif

    return;
}

fs::path PostCode::serialize(const fs::path& path)
{
    try
    {
        std::ofstream osIdx(path / CurrentBootCycleIndexName, std::ios::binary);
        cereal::BinaryOutputArchive idxArchive(osIdx);
        idxArchive(currentBootCycleIndex);

        uint16_t count = currentBootCycleCount();
        std::ofstream osCnt(path / CurrentBootCycleCountName, std::ios::binary);
        cereal::BinaryOutputArchive cntArchive(osCnt);
        cntArchive(count);

        std::ofstream osPostCodes(path / std::to_string(currentBootCycleIndex));
        cereal::BinaryOutputArchive oarchivePostCodes(osPostCodes);
        oarchivePostCodes(postCodes);
    }
    catch (const cereal::Exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
        return "";
    }
    catch (const fs::filesystem_error& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
        return "";
    }
    return path;
}

bool PostCode::deserialize(const fs::path& path, uint16_t& index)
{
    try
    {
        if (fs::exists(path))
        {
            std::ifstream is(path, std::ios::in | std::ios::binary);
            cereal::BinaryInputArchive iarchive(is);
            iarchive(index);
            return true;
        }
        return false;
    }
    catch (const cereal::Exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
        return false;
    }
    catch (const fs::filesystem_error& e)
    {
        return false;
    }

    return false;
}

bool PostCode::deserializePostCodes(const fs::path& path,
                                    std::map<uint64_t, postcode_t>& codes)
{
    try
    {
        if (fs::exists(path))
        {
            std::ifstream is(path, std::ios::in | std::ios::binary);
            cereal::BinaryInputArchive iarchive(is);
            iarchive(codes);
            return true;
        }
        return false;
    }
    catch (const cereal::Exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
        return false;
    }
    catch (const fs::filesystem_error& e)
    {
        return false;
    }
    return false;
}

void PostCode::incrBootCycle()
{
    if (currentBootCycleIndex >= maxBootCycleNum())
    {
        currentBootCycleIndex = 1;
    }
    else
    {
        currentBootCycleIndex++;
    }
    currentBootCycleCount(std::min(
        maxBootCycleNum(), static_cast<uint16_t>(currentBootCycleCount() + 1)));
}

uint16_t PostCode::getBootNum(const uint16_t index) const
{
    // bootNum assumes the oldest archive is boot number 1
    // and the current boot number equals bootCycleCount
    // map bootNum back to bootIndex that was used to archive postcode
    uint16_t bootNum = currentBootCycleIndex;
    if (index > bootNum) // need to wrap around
    {
        bootNum = (maxBootCycleNum() + currentBootCycleIndex) - index + 1;
    }
    else
    {
        bootNum = currentBootCycleIndex - index + 1;
    }
    return bootNum;
}
