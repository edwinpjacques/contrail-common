//
// Copyright (c) 2018 Juniper Networks, Inc. All rights reserved.
//

#include <iostream>
#include <sandesh/sandesh.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>
#include <boost/unordered_map.hpp>
#include <boost/system/error_code.hpp>
#include <k8s_types.h>
#include "schema/vnc_cfg_types.h"
#include "k8s_client.h"
#include "k8s_client_log.h"
#include "base/address_util.h"

using namespace std;
using namespace k8s::client;
using namespace contrail_rapidjson;

SandeshTraceBufferPtr K8sClientTraceBuf(SandeshTraceBufferCreate(
     K8S_CLIENT_TRACE_BUF, 10000));

K8sClient::K8sClient(const web::http::uri &uri,
                     const web::http::client::http_client_config &httpClientConfig,
                     const std::string &apiGroup,
                     const std::string &apiVersion,
                     size_t fetchLimit)
  : uri_(uri),
    httpClientConfig_(httpClientConfig), 
    apiGroup_(apiGroup), 
    apiVersion_(apiVersion),
    fetchLimit_(fetchLimit)
{
    boost::asio::ip::address addr;
    boost::system::error_code ec;
    addr = AddressFromString(uri_.host(), &ec);
    assert(!ec);
    Endpoint endpoint(addr, uri.port());
    endpoints_.push_back(endpoint);
}

K8sClient::~K8sClient ()
{
}

bool K8sClient::Init()
{
    return true;
}

web::http::http_response K8sClient::BulkGet(const std::string &kind,
                                            GetCb getCb)
{
    web::http::http_response resp;
    return resp;
}

void K8sClient::Watch(const std::string &kind,
                      WatchCb watchCb)
{
}

void K8sClient::WatchAll(WatchCb watchCb)
{
}

void K8sClient::StopWatch()
{
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

//                HandleK8sConnectionStatus(false);


    // K8S_CLIENT_DEBUG(K8sDebug, os << "Watch Request - Key: " << key);
    //         K8S_CLIENT_TRACE(K8sErrorTrace,
    //                   "Watch Response: Error",
    //                   resp.err_code(),
    //                   resp.err_msg());
    //                 K8S_CLIENT_DEBUG(K8sDebug, os << "Watch Response: "
    //                                               << "Success"
    //                                               << " revision: "
    //                                               << resp.revision()
    //                                               << " action: "
    //                                               << resp.action()
    //                                               << " Key: "
    //                                               << resp.key()
    //                                               << " Value: "
    //                                               << resp.value()
    //                                               << " PrevKey: "
    //                                               << resp.prev_key()
    //                                               << " PrevValue: "
    //                                               << resp.prev_value());
