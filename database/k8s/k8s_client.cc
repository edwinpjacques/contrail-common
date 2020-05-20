//
// Copyright (c) 2020 Juniper Networks, Inc. All rights reserved.
//

#include "k8s_client.h"
#include "k8s_util.h"

#include "k8s_types.h"
#include "k8s_client_log.h"
#include "schema/vnc_cfg_types.h"

#include <boost/regex.hpp>
#include <sstream>

using namespace boost;
using namespace k8s::client;
using namespace contrail_rapidjson;

using tcp = boost::asio::ip::tcp;

SandeshTraceBufferPtr K8sClientTraceBuf(SandeshTraceBufferCreate(
     K8S_CLIENT_TRACE_BUF, 10000));


K8sClient::K8sClient(const ::K8sUrl &k8sUrl,
                     const std::string &caCertFile,
                     size_t fetchLimit)
  : k8sUrl_(k8sUrl),
    caCertFile_(caCertFile),
    fetchLimit_(fetchLimit)
{
    std::ostringstream o;
    o << fetchLimit_;
    fetchLimitString_ = o.str();

    RestClient::init();
}

K8sClient::~K8sClient ()
{
    StopWatchAll();
    RestClient::disable();
}

int K8sClient::Init()
{
    if (!kindInfoMap_.empty())
    {
        K8S_CLIENT_DEBUG(K8sDebug, "K8S CLIENT: Already initialized.");
        return EXIT_FAILURE;
    }

    // Resolve the address of the configuration service
    boost::asio::io_service ioService;
    tcp::resolver resolver(ioService);
    // Form the query of the service by name (or IP) and port
    tcp::resolver::query query(
        k8sUrl_.server(), k8sUrl_.port(),
        tcp::resolver::query::numeric_service);
    // Get a list of endpoints corresponding to the server name
    boost::system::error_code ec;
    tcp::resolver::iterator endpointIterator = resolver.resolve(query, ec);
    // Make sure we can resolve the DNS name
    if (ec != 0)
    {
        K8S_CLIENT_DEBUG(
            K8sDebug, "K8S CLIENT: Could not resolve address for server " 
                      << k8sUrl_.server() << ", " << ec.message());
        return EXIT_FAILURE;
    }
    endpoints_.push_back(*endpointIterator);

    // Create connection context
    cx_.reset(new RestClient::Connection(k8sUrl_.serverUrl()));

    // Set SSL options if enabled
    if (k8sUrl_.encrypted())
    {
        cx_->SetCertPath(caCertFile_);
        // Convert file extension to cert type.
        // e.g. -- cert.pem => "PEM", cert.p12 => "P12"
        cx_->SetCertType(k8s::client::CertType(caCertFile_));
    }

    // Use the connection context to get API metadata
    RestClient::Response response = cx_->get(k8sUrl_.apiPath());
    if (response.code != 200) {
        K8S_CLIENT_DEBUG(
            K8sDebug, "K8S CLIENT: Unexpected reponse from API server: " << 
                      response.code << ", body: " << response.body);
        return EXIT_FAILURE;
    }

    try
    {
        // Parse the response received.
        Document apiMeta;
        apiMeta.Parse<0>(response.body.c_str());

        auto resources = apiMeta.FindMember("resources")->value.GetArray();
        for (auto resource = resources.Begin(); resource != resources.End(); resource++)
        {
            KindInfo kindInfo;

            // skip the "/status" entries
            kindInfo.name = resource->FindMember("name")->value.GetString();
            if (kindInfo.name.rfind("/status") != string::npos) {
                continue;
            }

            kindInfo.kind = resource->FindMember("kind")->value.GetString();
            kindInfo.singularName = resource->FindMember("singularName")->value.GetString();
            kindInfo.namespaced = resource->FindMember("namespaced")->value.GetBool();

            // Populate the object metadata
            kindInfoMap_[kindInfo.kind] = kindInfo;        
        }
    }
    catch(const std::exception& e)
    {
        K8S_CLIENT_DEBUG(K8sDebug, "K8S CLIENT: Error parsing API metadata: " << e.what());
        return EXIT_FAILURE;
    }

    K8S_CLIENT_DEBUG(K8sDebug, "K8S CLIENT: Initialization Done.");
    return EXIT_SUCCESS;
}

int K8sClient::BulkGet(const std::string &kind,
                       GetCb getCb)
{
    auto kindInfoFound = kindInfoMap_.find(kind);
    if (kindInfoFound == kindInfoMap_.end())
    {
        K8S_CLIENT_DEBUG(K8sDebug, "K8S CLIENT: Kind not supported: " << kind);
        return 400;
    }

    std::string bulkGetPath = k8sUrl_.namePath(kindInfoFound->second.name) + 
        "?limit=" + fetchLimitString_;
    try
    {
        std::string continueToken;
        do
        {
            RestClient::Response response = cx_->get(bulkGetPath + 
                (continueToken.empty() ? "" : "&continue=" + continueToken));
            if (response.code != 200) {
                K8S_CLIENT_DEBUG(
                    K8sDebug, "K8S CLIENT: Unexpected reponse from API server: " << 
                            response.code << ", body: " << response.body);
                return response.code;
            }
            // Parse the response received.
            Document bulkData;
            bulkData.Parse<0>(response.body.c_str());

            // Find all the items.  Create DOM and pass data to callback.
            auto items = bulkData.FindMember("items")->value.GetArray();
            for (auto item = items.Begin(); item != items.End(); item++)
            {
                DomPtr itemDomPtr(new Document);
                itemDomPtr->CopyFrom(*item, itemDomPtr->GetAllocator());
                getCb(itemDomPtr);
            }

            // get revision for subsequent watch call
            auto metadata = bulkData.FindMember("metadata");
            kindInfoMap_[kind].resourceVersion = metadata->value.FindMember("resourceVersion")->value.GetString();
            // get continue token (if any)
            auto continueValue = metadata->value.FindMember("continue");
            continueToken = (continueValue == metadata->value.MemberEnd() ? "" : continueValue->value.GetString());
        } while (!continueToken.empty());
    }
    catch(const std::exception& e)
    {
        K8S_CLIENT_DEBUG(K8sDebug, "K8S CLIENT: Error parsing bulk data: " << e.what());
        return 400;
    }

    return 200;
}

void K8sClient::StartWatch(
    const std::string &kind, WatchCb watchCb, size_t retryDelay)
{
    // Create thread watch for changes on one kind of object.
    KindInfoMap::iterator kindInfo = kindInfoMap_.find(kind);
    if (kindInfo == kindInfoMap_.end()) {
        K8S_CLIENT_DEBUG(K8sDebug, 
            "K8S CLIENT: Ignoring request to watch unsupported kind: " << kind);
        return;
    }

    // We look up the type to watch with the kind, but watcher needs the name
    kindInfo->second.watcher.reset(
        new K8sWatcher(k8sUrl_, kindInfo->second.name, watchCb));
    kindInfo->second.watcher->StartWatch(
        kindInfo->second.resourceVersion, retryDelay);
}

void K8sClient::StartWatchAll(WatchCb watchCb, size_t retryDelay)
{
    for(auto kindInfo = kindInfoMap_.begin(); 
        kindInfo != kindInfoMap_.end();
        kindInfo++)
    {
        StartWatch(kindInfo->first, watchCb, retryDelay);
    }
}

void K8sClient::StopWatch(const std::string &kind)
{
    // Stop the watch if its running for this kind.
    KindInfoMap::iterator kindInfo = kindInfoMap_.find(kind);
    if (kindInfo == kindInfoMap_.end()) {
        K8S_CLIENT_DEBUG(K8sDebug, 
            "K8S CLIENT: Ignoring request stop watch unsupported kind: " << kind);
        return;
    }
    if (!kindInfo->second.watcher)
    {
        K8S_CLIENT_DEBUG(K8sDebug, 
            "K8S CLIENT: Watcher not running, ignoring stop request for kind: " << kind);
        return;
    }

    kindInfo->second.watcher->StopWatch();
}

void K8sClient::StopWatchAll()
{
    for(auto kindInfo = kindInfoMap_.begin(); 
        kindInfo != kindInfoMap_.end();
        kindInfo++)
    {
        StopWatch(kindInfo->first);
    }
}

std::string K8sClient::UidFromObject(const Document& dom)
{
    Value::ConstMemberIterator metadata = dom.FindMember("metadata");
    if (metadata != dom.MemberEnd()) {
        Value::ConstMemberIterator uid = metadata->value.FindMember("uid");
        if (uid != metadata->value.MemberEnd() &&
            uid->value.IsString()) {
            return uid->value.GetString();
        }
    }
    return "";
}