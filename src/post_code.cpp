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

void PostCode::deleteAll()
{
    auto dir = fs::path(PostCodeListPath);
    std::uintmax_t n = fs::remove_all(dir);
    std::cerr << "clearPostCodes deleted " << n << " files in "
              << PostCodeListPath << std::endl;
    fs::create_directories(dir);
    postCodes.clear();
    currentBootCycleIndex(1);
}

std::vector<uint64_t> PostCode::getPostCodes(uint16_t index)
{
    std::map<std::string, uint64_t> codes;
    std::vector<uint64_t> codesVec;

    if (currentBootCycleIndex() == index)
    {
        for (auto code : postCodes)
            codesVec.push_back(code.second);
    }
    else
    {
        deserializePostCodes(
            fs::path(strPostCodeListPath + std::to_string(index)), codes);
        for (auto code : codes)
            codesVec.push_back(code.second);
    }
    return codesVec;
}

std::map<std::string, uint64_t> PostCode::getPostCodesTS(uint16_t index)
{
    std::map<std::string, uint64_t> codes;

    if (currentBootCycleIndex() == index)
        return postCodes;
    deserializePostCodes(fs::path(strPostCodeListPath + std::to_string(index)),
                         codes);
    return codes;
}

void PostCode::savePostCodes(uint64_t code)
{
    // for calculating monotonic time offset is microsecond
    auto postCodeTimeSteady = std::chrono::steady_clock::now();
    // for display human readable realtime/wall time
    std::chrono::time_point<std::chrono::system_clock,
                            std::chrono::microseconds>
        now = std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now());

    if (postCodes.empty())
    {
        firstPostCodeTimeSteady = postCodeTimeSteady;
    }
    uint64_t timeOffsetUS =
        std::chrono::duration_cast<std::chrono::microseconds>(
            postCodeTimeSteady - firstPostCodeTimeSteady)
            .count();

    // Exploit now_t (time_t): resolution is only 1 second - get microseconds
    auto now_t = std::chrono::system_clock::to_time_t(now);
    auto dur = now - std::chrono::system_clock::from_time_t(now_t);
    auto micros =
        std::chrono::duration_cast<std::chrono::microseconds>(dur).count();

    // Print the date and time as yyyy-mm-ddThh::mm::ss.xxxxxx, oooooo
    // where oooooo is montonic time offset in micorsecond since first post code
    std::tm& now_tm = *(std::gmtime(&now_t));
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%FT%T") << "." << std::setfill('0')
       << std::setw(6) << micros << std::put_time(&now_tm, "%z") << ", "
       << std::to_string(static_cast<double>(timeOffsetUS) / 1000000.0);

    postCodes.insert(std::make_pair(ss.str(), code));
    serialize(fs::path(PostCodeListPath));

    return;
}

fs::path PostCode::serialize(const std::string& path)
{
    try
    {
        uint16_t index = currentBootCycleIndex();
        fs::path fullPath(path + strCurrentBootCycleIndexName);
        std::ofstream os(fullPath.c_str(), std::ios::binary);
        cereal::JSONOutputArchive oarchive(os);
        oarchive(index);

        std::ofstream osPostCodes(
            (path + std::to_string(currentBootCycleIndex())).c_str(),
            std::ios::binary);
        cereal::JSONOutputArchive oarchivePostCodes(osPostCodes);
        oarchivePostCodes(postCodes);
    }
    catch (cereal::Exception& e)
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
    catch (cereal::Exception& e)
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
                                    std::map<std::string, uint64_t>& codes)
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
    catch (cereal::Exception& e)
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
