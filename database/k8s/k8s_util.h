//
// Copyright (c) 2020 Juniper Networks, Inc. All rights reserved.
//

#ifndef DATABASE_K8S_UTIL_H_
#define DATABASE_K8S_UTIL_H_

#include <string>

namespace k8s {
namespace client {

/**
 * Convert the file extension into a certificate type string.
 * e.g.-- cert.p12 ==> "P12", cert.pem ==> "PEM"
 * Returns empty string on failure.
 */
std::string CertType(const std::string& caCertPath);

} //namespace client
} //namespace k8s
#endif