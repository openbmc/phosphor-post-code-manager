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
#include <phosphor-logging/commit.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>

#include <iomanip>

using nlohmann::json;

const static constexpr auto timeoutMicroSeconds = 1000000;
/* systemd service to kick start a target. */
constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_ROOT = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";

void PostCodeEvent::raise() const
{
    json j = {{name, args}};
    try
    {
        sdbusplus::exception::throw_via_json(j);
    }
    catch (sdbusplus::exception::generated_event_base& e)
    {
        auto path = lg2::commit(std::move(e));
        std::cout << path.str << std::endl;
    }
}

void from_json(const json& j, PostCodeEvent& event)
{
    j.at("name").get_to(event.name);
    for (const auto& entry : j.at("arguments").items())
    {
        if (entry.value().is_string())
        {
            event.args[entry.key()] = entry.value().get<std::string>();
        }
        else if (entry.value().is_number_integer())
        {
            event.args[entry.key()] = entry.value().get<int>();
        }
    }
}

void from_json(const json& j, PostCodeHandler& handler)
{
    j.at("primary").get_to(handler.primary);
    if (j.contains("secondary"))
    {
        secondarycode_t secondary;
        j.at("secondary").get_to(secondary);
        handler.secondary = secondary;
    }
    if (j.contains("targets"))
    {
        j.at("targets").get_to(handler.targets);
    }
    if (j.contains("event"))
    {
        PostCodeEvent event;
        j.at("event").get_to(event);
        handler.event = event;
    }
}

const PostCodeHandler* PostCodeHandlers::find(postcode_t code)
{
    for (const auto& handler : handlers)
    {
        if (handler.primary == std::get<0>(code) &&
            (!handler.secondary || *handler.secondary == std::get<1>(code)))
        {
            return &handler;
        }
    }
    return nullptr;
}

void PostCodeHandlers::handle(postcode_t code)
{
    const PostCodeHandler* handler = find(code);
    if (!handler)
    {
        return;
    }
    for (const auto& target : handler->targets)
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_ROOT,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(target);
        method.append("replace");
        bus.call_noreply(method);
    }
    if (handler->event)
    {
        (*(handler->event)).raise();
    }
}

void PostCodeHandlers::load(const std::string& path)
{
    std::ifstream ifs(path);
    handlers = json::parse(ifs);
    ifs.close();
}

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

std::map<uint64_t, postcode_t> PostCode::getPostCodesWithTimeStamp(
    uint16_t index)
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
        postCodeDisplayFile << "0x" << std::setfill('0') << std::hex;
        for (const auto& byte : std::get<0>(code))
        {
            postCodeDisplayFile << std::setw(2) << static_cast<int>(byte);
        }
        postCodeDisplayFile.close();
    }

#ifdef ENABLE_BIOS_POST_CODE_LOG
    uint64_t usTimeOffset = tsUS - firstPostCodeUsSinceEpoch;
    std::ostringstream hexCode;
    hexCode << "0x" << std::setfill('0') << std::hex;
    for (const auto& byte : std::get<0>(code))
    {
        hexCode << std::setw(2) << static_cast<int>(byte);
    }

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
    postCodeHandlers.handle(code);

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
