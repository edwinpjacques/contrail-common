//
// Copyright (c) 2020 Juniper Networks, Inc. All rights reserved.
//

#include "k8s_watcher.h"
#include "k8s_types.h"
#include "k8s_client_log.h"
#include "k8s_util.h"
#include <unistd.h>
#include <sstream>
#include <algorithm>

using namespace boost;
using namespace k8s::client;
using namespace contrail_rapidjson;

size_t k8s::client::K8sWatcherWriteCallback(
    void *data, size_t size, size_t nmemb, K8sWatcherResponse *userdata)
{
    size_t bytes = size * nmemb;
    try
    {
        // Add to the buffer
        userdata->body.append(static_cast<char*>(data), bytes);
        // If the last character is not a new line, wait for the rest.
        if ('\n' != *(userdata->body.end() - 1))
        {
            return bytes;
        }
        // We have at least one event to process.
        std::stringstream stream(userdata->body);
        std::string eventString;
        while (std::getline(stream, eventString))
        {
            Document eventDom;
            eventDom.Parse<0>(eventString.c_str());
            if (!eventDom.IsObject())
            {
                K8S_CLIENT_WARN(K8sDebug, 
                    std::string("K8S CLIENT: Invalid JSON: ") + 
                    eventString.c_str());
                return CURLE_WRITE_ERROR;
            }
            auto type = eventDom.FindMember("type");
            if (type == eventDom.MemberEnd())
            {
                K8S_CLIENT_WARN(K8sDebug, 
                    std::string("K8S CLIENT: Error Watch Response: ") + 
                    eventString.c_str());
                return CURLE_WRITE_ERROR;
            }
            std::string typeStr = eventDom.FindMember("type")->value.GetString();
            DomPtr objectDomPtr(new Document);
            objectDomPtr->CopyFrom(eventDom.FindMember("object")->value, objectDomPtr->GetAllocator());

            // Pass type and object payload to callback
            userdata->watcher->watchCb()(typeStr, objectDomPtr);
        }
        // Done with the string
        userdata->body.clear();
    }
    catch(std::exception e)
    {
        K8S_CLIENT_WARN(K8sDebug, std::string("K8S CLIENT: Unhandled exception: ") + e.what());
        return CURLE_WRITE_ERROR;
    }
    return CURLE_OK;
}

K8sWatcher::K8sWatcher(
    const K8sUrl& k8sUrl, const std::string& name, 
    k8s::client::WatchCb watchCb, const std::string& caCertFile)
: k8sUrl_(k8sUrl), name_(name), watchCb_(watchCb), caCertFile_(caCertFile)
{}

void K8sWatcher::Watch(const std::string& version, size_t retryDelay)
{
    // Save the version (of the last bulk get).
    // Used to formulate the watch query.
    version_ = version;

    // Create connection context
    k8s::client::InitConnection(cx_, k8sUrl_, caCertFile_);

    // Set the callback used to process response
    cx_->SetWriteFunction(
        reinterpret_cast<RestClient::WriteCallback>(K8sWatcherWriteCallback));
    // Create the response context to handle receiving streamed data.
    // Provide it with a back-pointer to this watch object.
    response_.reset(new K8sWatcherResponse(this));

    while(true)
    {
        try
        {
            // Initiate the watch.
            // The response structure will receive watch data via callback.
            cx_->get(watchPath(), *response_);

            // Watch stopped, log it.
            K8S_CLIENT_DEBUG(
                K8sDebug, "K8S CLIENT: " << name_ << 
                " watch stopped with code " << response_->code);

            // On success, restart watch immediately.  Otherwise, sleep.
            if (response_->code == 200)
            {
                continue;
            }
            sleep(retryDelay);
        }
        catch(const std::runtime_error& e)
        {
            // Log termination request and exit
            K8S_CLIENT_DEBUG(
                K8sDebug, "K8S CLIENT: " << name_ << 
                " watch runtime error: " << e.what());
            break;
        }
    }
}

void K8sWatcher::Terminate()
{
    if (cx_) 
    {
        cx_->Terminate();
        if (threadPtr_)
        {
            K8S_CLIENT_DEBUG(
                K8sDebug, "K8S CLIENT: " << name_ << " watch terminated.");
            threadPtr_->join();
            threadPtr_.reset();
        }
    }
}

void K8sWatcher::StartWatch(const std::string& version, size_t retryDelay)
{
    if (threadPtr_)
    {
        K8S_CLIENT_DEBUG(
            K8sDebug, "K8S CLIENT: " << name_ << " watch thread already running.");
        return;
    }

    threadPtr_.reset(new boost::thread(boost::bind(&K8sWatcher::Watch, 
                    this, version, retryDelay)));
    K8S_CLIENT_DEBUG(
        K8sDebug, "K8S CLIENT: " << name_ << " watch thread started.");
}

void K8sWatcher::StopWatch()
{
    Terminate();
}

K8sWatcher::~K8sWatcher()
{
    Terminate();
}
