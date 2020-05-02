#ifndef DATABASE_K8S_CLIENT_H_
#define DATABASE_K8S_CLIENT_H_

#include "cpprest/http_client.h"
#include "rapidjson/document.h"
#include <string>
#include <boost/function.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace k8s {
namespace client {

/**
 * K8sClient is the Kubernetes client that is used to create and maintain connection
 * to the Kubernetes server.
 * The methods of the client can be used to perform Kubernetes operations.
 * Control node is only interested in the reading data from or watching
 * changes on a specific key or directory in Kubernetes and hence only those
 * operations are implemented here.
 */
class K8sClient {
public:
    /**
     * Types (kinds)
     */
    typedef std::shared_ptr<contrail_rapidjson::Document> DomPtr;
    
    typedef boost::function<void (DomPtr object)> GetCb;
    typedef boost::function<void (std::string type, DomPtr object)> WatchCb;
    
    struct KindInfo {
        std::string         name;
        std::string         singularName;
        bool                namespaced;
        std::string         kind;
        std::string         resourceVersion;
        pplx::task<void>    watchTask;
    };
 
    typedef std::map<const std::string, KindInfo> KindInfoMap;

    typedef boost::asio::ip::tcp::endpoint Endpoint;

    /**
     * Constructor that creates a Kubernetes client object.
     * @param host The IP address of the Kubernetes server
     * @param port The port to connect to
     */
    K8sClient(const web::http::uri &host,
              const web::http::client::http_client_config &config,
              const std::string &apiGroup,
              const std::string &apiVersion,
              size_t fetchLimit=defaultFetchLimit);

    /**
     * Destructor
     */
    virtual ~K8sClient();

    /**
     * Initialize the client by getting information on the types supported
     * by this apiGroup.  Returns "true" if initialization is successful,
     * otherwise returns false.
     */
    virtual bool Init();

    /**
     * Default maximum number of items to fetch at a time.
     */
    static const size_t defaultFetchLimit=500;

    /**
     * Get all Kubernetes objects of a particular type.
     * Blocks until all the data is retrieved.
     * @param kind Type name to get (e.g.-- project)
     * @param getCb Callback to invoke for each object that is retrieved.
     *              Takes as an argument a DomPtr.
     */
    virtual web::http::http_response BulkGet(const std::string &kind,
                                             GetCb getCb);

    /**
     * Watch for changes for a particular type since the last BulkGet.
     * Processing will continue in the background.  
     * @param kind Type to watch
     * @param watchCb Callback to invoke for each object that is retrieved.
     *                Takes as arguments the type and (ADDED/MODIFIED/DELETED)
     *                and the DomPtr for the obect.
     */
    virtual void Watch(const std::string &kind,
                       WatchCb watchCb);

    /**
     * Watch for changes for all types since the last BulkGet (for that type).
     * @param watchCb Callback to invoke for each object that is retrieved.
     *                Takes as arguments the type and (ADDED/MODIFIED/DELETED)
     *                and the DomPtr for the obect.
     */
    virtual void WatchAll(WatchCb watchCb);

    /**
     * Stop the watch request if scheduled
     */
    virtual void StopWatch();

    /**
     * Getters
     */
    web::http::uri uri() const { 
        return uri_; 
    }
    std::vector<Endpoint> endpoints() { 
        return endpoints_; 
    }
    web::http::client::http_client_config httpClientConfig() const { 
        return httpClientConfig_; 
    }
    std::string apiGroup() const { 
        return apiGroup_; 
    }
    std::string apiVersion() const { 
        return apiVersion_; 
    }
    size_t fetchLimint() const { 
        return fetchLimit_; 
    }

    const KindInfoMap& kindInfoMap() const { 
        return kindInfoMap_;
    }

    // Get UUID from DOM of an object.
    static std::string UidFromObject(const contrail_rapidjson::Document& dom);

protected:
    // Information needed to connect
    web::http::uri uri_;
    std::vector<Endpoint> endpoints_;
    web::http::client::http_client_config httpClientConfig_;
    const std::string apiGroup_;
    const std::string apiVersion_;
    size_t fetchLimit_;

    // Type information, indexed by kind
    KindInfoMap kindInfoMap_;

    // Map kinds to asynchronous tasks 
    std::map<const std::string, pplx::task<void> > watchTaskMap_;
};

} //namespace client
} //namespace k8s
#endif
