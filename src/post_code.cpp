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
std::vector<uint64_t> PostCode::getPostCodes(uint16_t index)
{
    std::vector<uint64_t> codes;

    phosphor::logging::log<phosphor::logging::level::INFO>(
                    "getPostCodes is called");
    if (currentBootCycleIndex() == index)
        return postCodes;
    deserializePostCodes(fs::path(strPostCodeListPath + std::to_string(index)), codes);
    return codes;
}
void PostCode::savePostCodes(uint64_t code)
{
    phosphor::logging::log<phosphor::logging::level::INFO>(
                    "saveToPostCodeList is called");
    postCodes.push_back(code);
    serialize(fs::path(PostCodeListPath));
    return;
}

fs::path PostCode::serialize(const std::string& path)
{
    fs::path fullPath(path + strCurrentBootCycleIndexName);
    std::ofstream os(fullPath.c_str(), std::ios::binary);
    cereal::JSONOutputArchive oarchive(os);
    oarchive(currentBootCycleIndex());

    std::ofstream osPostCodes((path + std::to_string(currentBootCycleIndex())).c_str(), std::ios::binary);
    cereal::JSONOutputArchive oarchivePostCodes(osPostCodes);
    oarchivePostCodes(postCodes);
    return path;
}

bool PostCode::deserialize(const fs::path& path)
{
    try
    {
        if (fs::exists(path))
        {
            std::ifstream is(path.c_str(), std::ios::in | std::ios::binary);
            cereal::JSONInputArchive iarchive(is);
            iarchive(currentBootCycleIndex());
            return true;
        }
        return false;
    }
    catch (cereal::Exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
        fs::remove(path);
        return false;
    }
    catch (const fs::filesystem_error& e)
    {
        return false;
    }

    return false;
}

bool PostCode::deserializePostCodes(const fs::path& path, std::vector<uint64_t> &codes)
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
        fs::remove(path);
        return false;
    }
    catch (const fs::filesystem_error& e)
    {
        return false;
    }

    return false;
}

