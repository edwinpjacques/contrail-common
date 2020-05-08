//
// Copyright (c) 2020 Juniper Networks, Inc. All rights reserved.
//

#include "k8s_util.h"

std::string k8s::client::CertType(const std::string& caCertPath)
{
    std::string type;
    auto extension = caCertPath.rfind('.');
    type = caCertPath.substr(extension + 1);
    for (auto i = type.begin(); i != type.end(); i++)
    {
        *i = toupper(*i);
    }
    return type;
}
