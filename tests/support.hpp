#pragma once
#include <crails/server.hpp>
#include <crails/context.hpp>
#include <crails/params.hpp>
#include <crails/http_response.hpp>
#include <crails/session_store.hpp>
#include <crails/controller.hpp>
#include <crails/crud/base_controller.hpp>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Crails
{
  template<typename CONTROLLER, bool WITH_ASYNC = true>
  class ActionRoute
  {
  public:
    static void attach(CONTROLLER& controller, std::function<void()> callback)
    {
      controller.ActionController::callback = callback;
    }
  };
}

namespace Support
{
  struct FakeDatabase { int id = 0; };

  struct Model
  {
    static inline const std::string scope = "";

    void edit(Data) {}
    void on_change() {}
    bool is_valid() { return true; }

    DataTree errors;
  };

  struct TestServer : public Crails::Server
  {
    SINGLETON_IMPLEMENTATION(TestServer, Crails::Server)
  public:
    TestServer() {}
  };

  struct MemorySessionStore : public Crails::SessionStore
  {
    SESSION_STORE_IMPLEMENTATION(MemorySessionStore)
  public:
    static inline DataTree cookie;
    static void reset() { cookie = DataTree(); }
    void        load(const Crails::HttpRequest&) override {}
    void        finalize(Crails::BuildingResponse&) override {}
    Data        to_data() override { return cookie.as_data(); }
    const Data  to_data() const override { return cookie.as_data(); }
  };

  struct Setup
  {
    SingletonInstantiator<TestServer>                  server;
    SingletonInstantiator<MemorySessionStore::Factory> sessions;
    Setup() { MemorySessionStore::reset(); }
  };

  // A request and the context it is processed in, without any network
  struct Fixture
  {
    std::shared_ptr<Crails::Connection> connection;
    std::shared_ptr<Crails::Context>    context;

    Fixture(Crails::HttpVerb verb = Crails::HttpVerb::get, const std::string& action = "", std::string target = "/")
    {
      Crails::HttpRequest request;

      request.method(verb);
      request.target(target);
      connection = std::make_shared<Crails::Connection>(Crails::Server::singleton::require(), request);
      context    = std::make_shared<Crails::Context>(Crails::Server::singleton::require(), *connection);
      if (action.length() > 0)
      {
        params()["controller-data"]["name"]   = "Articles";
        params()["controller-data"]["action"] = action;
      }
    }

    Crails::Context& operator*() { return *context; }
    Crails::Params&  params()    { return context->params; }
    unsigned         status() const { return context->response.get_raw_response().result_int(); }
    std::string      body() const   { return context->response.get_raw_response().body(); }
    std::string      header(const std::string& name) const
    {
      auto it = context->response.get_raw_response().find(name);
      return it == context->response.get_raw_response().end() ? std::string() : std::string(it->value());
    }
  };

  template<typename CONTROLLER>
  void run(CONTROLLER& controller, void (CONTROLLER::*action)(), std::function<void()> on_close = []() {})
  {
    Crails::ActionRoute<CONTROLLER>::attach(controller, on_close);
    controller.initialize();
    if (!controller.is_closed())
      (controller.*action)();
  }

  struct Base : public Crails::Controller
  {
    static inline bool                     csrf = false;
    static inline std::vector<std::string> log;
    static void reset() { csrf = false; log.clear(); }

    Base(Crails::Context& context) : Crails::Controller(context) {}

    void initialize() override
    {
      log.push_back("base");
      Crails::Controller::initialize();
    }

    bool must_protect_from_forgery() const override { return csrf; }
  };

  struct BaseWithDatabase : public Base
  {
    BaseWithDatabase(Crails::Context& context) : Base(context) {}

    FakeDatabase database;
  };

  template<typename BASE, typename MODEL>
  class TestController : public Crud::BaseController<BASE, MODEL>
  {
    typedef Crud::BaseController<BASE, MODEL> Super;
  public:
    TestController(Crails::Context& context) : Super(context) {}

    using Crails::ActionController::is_closed;

    void index()   override { actions.push_back("index"); }
    void show()    override { actions.push_back("show"); }
    void create()  override { actions.push_back("create"); }
    void update()  override { actions.push_back("update"); }
    void destroy() override { actions.push_back("destroy"); }

    bool find_model() override
    {
      ++find_model_calls;
      if (!model_exists)
        return false;
      this->model = std::make_shared<MODEL>();
      return true;
    }

    bool find_models() override
    {
      ++find_models_calls;
      return true;
    }

    Crails::Params&     get_params()      { return this->params; }
    Crails::SharedVars& get_shared_vars() { return this->vars; }

    bool                     model_exists = true;
    int                      find_model_calls = 0;
    int                      find_models_calls = 0;
    std::vector<std::string> actions;
  };
}
