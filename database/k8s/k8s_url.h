//
// Copyright (c) 2020 Juniper Networks, Inc. All rights reserved.
//

#ifndef DATABASE_K8S_URL_H_
#define DATABASE_K8S_URL_H_

#include <string>

namespace k8s {
namespace client {

/**
 * @brief Break up the URL for the service into pieces for convenience.
 */
class K8sUrl
{
public:
    /**
     * Break apart serviceUrl into parts.
     */
    K8sUrl(const std::string& serviceUrl,
           const std::string& apiGroup,
           const std::string& apiVersion);
    K8sUrl() {}
    void Reset(const std::string& serviceUrl,
               const std::string& apiGroup,
               const std::string& apiVersion);
    const std::string& protocol() const { return protocol_; }
    bool encrypted() const { return protocol_ == "https"; }
    const std::string& server() const { return server_; }
    const std::string& port() const { return port_; }
    const std::string& path() const { return path_; }
    const std::string& apiPath() const { return apiPath_; }
    const std::string& apiGroup() const { return apiGroup_; }
    const std::string& apiVersion() const { return apiVersion_; }
    /**
     * Reconstitute the serviceUrl from parts.
     */
    const std::string& serverUrl() const { return serverUrl_; }
    std::string apiUrl() const { return serverUrl_ + apiPath_; };
    std::string nameUrl(const std::string& name) const { return apiUrl() + '/' + name; };
    std::string namePath(const std::string& name) const { return apiPath_ + '/' + name; };
protected:
    std::string protocol_;
    std::string server_;
    std::string port_;
    std::string path_;
    std::string apiGroup_;
    std::string apiVersion_;
    std::string serverUrl_;
    std::string apiPath_;
};

} //namespace client
} //namespace k8s
#endif
