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
#include <fcntl.h>
#include <unistd.h>
#include <cereal/cereal.hpp>
#include <experimental/filesystem>
#include <cereal/access.hpp>
#include <cereal/archives/json.hpp>
#include <iostream>
#include <fstream>
#include <cereal/types/vector.hpp>

#include <xyz/openbmc_project/State/Boot/PostCode/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <phosphor-logging/elog-errors.hpp>

#define MaxPostCodeCycles 100

const static constexpr char *PostCodePath =
    "/xyz/openbmc_project/State/Boot/Raw";
const static constexpr char *PostCodeIntf =
    "xyz.openbmc_project.State.Boot.Raw";
const static constexpr char *PropertiesIntf =
    "org.freedesktop.DBus.Properties";
const static constexpr char *PostCodeListPath =
    "/var/lib/phosphor-post-code-manager/";
const static constexpr char *CurrentBootCycleIndexName =
    "CurrentBootCycleIndex";


struct EventDeleter
{
    void operator()(sd_event *event) const
    {
        event = sd_event_unref(event);
    }
};
using EventPtr = std::unique_ptr<sd_event, EventDeleter>;
namespace fs = std::experimental::filesystem;

using post_code =
    sdbusplus::xyz::openbmc_project::State::Boot::server::PostCode;

struct PostCode : sdbusplus::server::object_t<post_code>
{
    PostCode(sdbusplus::bus::bus& bus, const char* path,
                 EventPtr &event) :
        sdbusplus::server::object_t<post_code>(bus, path),
        bus(bus),
        propertiesChangedSignal(
            bus,
            sdbusplus::bus::match::rules::type::signal() +
                sdbusplus::bus::match::rules::member("PropertiesChanged") +
                sdbusplus::bus::match::rules::path(
                    PostCodePath) +
                sdbusplus::bus::match::rules::interface(PropertiesIntf),
            [this](sdbusplus::message::message &msg) {
                phosphor::logging::log<phosphor::logging::level::INFO>(
                    "PostCode propertiesChangedSignal callback function is "
                    "called...");
                std::string objectName;
                std::map<std::string, sdbusplus::message::variant<uint64_t>> msgData;
                msg.read(objectName, msgData);
                // Check if it was the Value property that changed.
                auto valPropMap = msgData.find("Value");
                {
                    if (valPropMap != msgData.end())
                    {
                        phosphor::logging::log<phosphor::logging::level::INFO>(
                            "find value");
                        this->savePostCodes(sdbusplus::message::variant_ns::get<uint64_t>(valPropMap->second));
                    }
                }
        })
    {
        phosphor::logging::log<phosphor::logging::level::INFO>(
                    "PostCode is created");
        currentBootCycleIndex(0);
        strPostCodeListPath = PostCodeListPath;
        strCurrentBootCycleIndexName = CurrentBootCycleIndexName;
        deserialize(fs::path((strPostCodeListPath + strCurrentBootCycleIndexName).c_str()));
        maxBootCycleNum(MaxPostCodeCycles);
        if (currentBootCycleIndex() >= maxBootCycleNum())
        {
            currentBootCycleIndex(1);
        } else{
            currentBootCycleIndex(currentBootCycleIndex() + 1);
        }
    }
    ~PostCode()
    {

    }

    std::vector<uint64_t> getPostCodes(uint16_t index) override; 

  private:
    sdbusplus::bus::bus& bus;
    std::vector<uint64_t> postCodes;
    std::string strPostCodeListPath;
    std::string strCurrentBootCycleIndexName;
    void savePostCodes(uint64_t code);
    sdbusplus::bus::match_t propertiesChangedSignal;
    fs::path serialize(const std::string& path);
    bool deserialize(const fs::path& path);
    bool deserializePostCodes(const fs::path& path, std::vector<uint64_t> &codes);
};

