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

#include "iomanip"

PostCodeDataHolder* PostCodeDataHolder::instance = 0;

void PostCode::deleteAll()
{
    auto dir = fs::path(postcodeDataHolderObj->PostCodeListPathPrefix +
                        std::to_string(postcodeDataHolderObj->node));
    std::uintmax_t n = fs::remove_all(dir);
    std::cerr << "clearPostCodes deleted " << n << " files in "
              << postcodeDataHolderObj->PostCodeListPathPrefix +
                     std::to_string(postcodeDataHolderObj->node)
              << std::endl;
    fs::create_directories(dir);
    postCodes.clear();
    currentBootCycleIndex = 0;
    currentBootCycleCount(0);
}

std::vector<postcode_t> PostCode::getPostCodes(uint16_t index)
{
    std::vector<postcode_t> codesVec;
    if (1 == index && !postCodes.empty())
    {
        for (auto& code : postCodes)
            codesVec.push_back(code.second);
    }
    else
    {
        uint16_t bootNum = getBootNum(index);

        decltype(postCodes) codes;
        deserializePostCodes(
            fs::path(strPostCodeListPath + std::to_string(bootNum)), codes);
        for (std::pair<uint64_t, postcode_t> code : codes)
            codesVec.push_back(code.second);
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
    deserializePostCodes(
        fs::path(strPostCodeListPath + std::to_string(bootNum)), codes);
    return codes;
}

void PostCode::savePostCodes(postcode_t code)
{
    uint64_t usTimeOffset = 0;
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
        usTimeOffset = std::chrono::duration_cast<std::chrono::microseconds>(
                           postCodeTimeSteady - firstPostCodeTimeSteady)
                           .count();
        tsUS = usTimeOffset + firstPostCodeUsSinceEpoch;
    }

    postCodes.insert(std::make_pair(tsUS, code));
    serialize(fs::path(strPostCodeListPath));

#ifdef ENABLE_BIOS_POST_CODE_LOG
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
                                 "OpenBMC.0.1.BIOSPOSTCode"),
        phosphor::logging::entry(
            "REDFISH_MESSAGE_ARGS=%d,%s,%s", currentBootCycleIndex,
            timeOffsetStr.str().c_str(), hexCode.str().c_str()));
#endif

    return;
}

fs::path PostCode::serialize(const std::string& path)
{
    try
    {
        fs::path idxPath(path + strCurrentBootCycleIndexName);
        std::ofstream osIdx(idxPath.c_str(), std::ios::binary);
        cereal::JSONOutputArchive idxArchive(osIdx);
        idxArchive(currentBootCycleIndex);

        uint16_t count = currentBootCycleCount();
        fs::path cntPath(path + strCurrentBootCycleCountName);
        std::ofstream osCnt(cntPath.c_str(), std::ios::binary);
        cereal::JSONOutputArchive cntArchive(osCnt);
        cntArchive(count);

        std::ofstream osPostCodes(
            (path + std::to_string(currentBootCycleIndex)));
        cereal::JSONOutputArchive oarchivePostCodes(osPostCodes);
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
            std::ifstream is(path.c_str(), std::ios::in | std::ios::binary);
            cereal::JSONInputArchive iarchive(is);
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
            std::ifstream is(path.c_str(), std::ios::in | std::ios::binary);
            cereal::JSONInputArchive iarchive(is);
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
