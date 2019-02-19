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

#include <xyz/openbmc_project/State/Boot/PostCode/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <phosphor-logging/elog-errors.hpp>

struct EventDeleter
{
    void operator()(sd_event *event) const
    {
        event = sd_event_unref(event);
    }
};
using EventPtr = std::unique_ptr<sd_event, EventDeleter>;

using post_code =
    sdbusplus::xyz::openbmc_project::State::Boot::server::PostCode;

struct PostCode : sdbusplus::server::object_t<post_code>
{
    PostCode(sdbusplus::bus::bus& bus, const char* path,
                 EventPtr &event) :
        sdbusplus::server::object_t<post_code>(bus, path),
        bus(bus)
    {
        phosphor::logging::log<phosphor::logging::level::INFO>(
                    "PostCode is created");
    }
    ~PostCode()
    {

    }

    std::vector<uint64_t> getPostCodes(uint16_t index) override; 

  private:
    sdbusplus::bus::bus& bus;
};

