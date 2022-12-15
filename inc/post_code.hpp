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
#pragma once
#include <config.h>
#include <fcntl.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <sdbusplus/timer.hpp>
#include <xyz/openbmc_project/Collection/DeleteAll/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/State/Boot/PostCode/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

const static constexpr char* CurrentBootCycleCountName =
    "CurrentBootCycleCount";
const static constexpr char* CurrentBootCycleIndexName =
    "CurrentBootCycleIndex";

const static constexpr char* PostCodePath =
    "/xyz/openbmc_project/state/boot/raw";
const static constexpr char* PostCodeListPathPrefix =
    "/var/lib/phosphor-post-code-manager/host";
const static constexpr char* HostStatePathPrefix =
    "/xyz/openbmc_project/state/host";

struct EventDeleter
{
    void operator()(sd_event* event) const
    {
        sd_event_unref(event);
    }
};
using EventPtr = std::unique_ptr<sd_event, EventDeleter>;
using primarycode_t = uint64_t;
using secondarycode_t = std::vector<uint8_t>;
using postcode_t = std::tuple<primarycode_t, secondarycode_t>;
namespace fs = std::filesystem;
namespace StateServer = sdbusplus::xyz::openbmc_project::State::server;

using post_code =
    sdbusplus::xyz::openbmc_project::State::Boot::server::PostCode;
using delete_all =
    sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll;

struct PostCode : sdbusplus::server::object_t<post_code, delete_all>
{
    PostCode(sdbusplus::bus_t& bus, const char* path, EventPtr& event,
             int nodeIndex) :
        sdbusplus::server::object_t<post_code, delete_all>(bus, path),
        bus(bus), event(event), node(nodeIndex),
        postCodeListPath(PostCodeListPathPrefix + std::to_string(node)),
        propertiesChangedSignalRaw(
            bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                PostCodePath + std::to_string(node),
                "xyz.openbmc_project.State.Boot.Raw"),
            [this](sdbusplus::message_t& msg) {
                std::string intfName;
                std::map<std::string, std::variant<postcode_t>> msgData;
                msg.read(intfName, msgData);
                // Check if it was the Value property that changed.
                auto valPropMap = msgData.find("Value");
                if (valPropMap != msgData.end())
                {
                    this->savePostCodes(
                        std::get<postcode_t>(valPropMap->second));
                }
            }),
        propertiesChangedSignalCurrentHostState(
            bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                HostStatePathPrefix + std::to_string(node),
                "xyz.openbmc_project.State.Host"),
            [this](sdbusplus::message_t& msg) {
                std::string intfName;
                std::map<std::string, std::variant<std::string>> msgData;
                msg.read(intfName, msgData);
                // Check if it was the Value property that changed.
                auto valPropMap = msgData.find("CurrentHostState");
                if (valPropMap != msgData.end())
                {
                    StateServer::Host::HostState currentHostState =
                        StateServer::Host::convertHostStateFromString(
                            std::get<std::string>(valPropMap->second));
                    if (currentHostState == StateServer::Host::HostState::Off)
                    {
                        if (this->postCodes.empty())
                        {
                            std::cerr << "HostState changed to OFF. Empty "
                                         "postcode log, keep boot cycle at "
                                      << this->currentBootCycleIndex
                                      << std::endl;
                        }
                        else
                        {
                            this->postCodes.clear();
                        }
                    }
                }
            })
    {
        phosphor::logging::log<phosphor::logging::level::INFO>(
            "PostCode is created");
        fs::create_directories(postCodeListPath);
        deserialize(postCodeListPath / CurrentBootCycleIndexName,
                    currentBootCycleIndex);
        uint16_t count = 0;
        deserialize(postCodeListPath / CurrentBootCycleCountName, count);
        currentBootCycleCount(count);
        maxBootCycleNum(MAX_BOOT_CYCLE_COUNT);
    }
    ~PostCode() {}

    std::vector<postcode_t> getPostCodes(uint16_t index) override;
    std::map<uint64_t, postcode_t>
        getPostCodesWithTimeStamp(uint16_t index) override;
    void deleteAll() override;

  private:
    void incrBootCycle();
    uint16_t getBootNum(const uint16_t index) const;

    std::unique_ptr<phosphor::Timer> timer;
    sdbusplus::bus_t& bus;
    EventPtr& event;
    int node;
    std::chrono::time_point<std::chrono::steady_clock> firstPostCodeTimeSteady;
    uint64_t firstPostCodeUsSinceEpoch;
    std::map<uint64_t, postcode_t> postCodes;
    fs::path postCodeListPath;
    uint16_t currentBootCycleIndex = 0;
    sdbusplus::bus::match_t propertiesChangedSignalRaw;
    sdbusplus::bus::match_t propertiesChangedSignalCurrentHostState;

    void savePostCodes(postcode_t code);
    fs::path serialize(const fs::path& path);
    bool deserialize(const fs::path& path, uint16_t& index);
    bool deserializePostCodes(const fs::path& path,
                              std::map<uint64_t, postcode_t>& codes);
};
