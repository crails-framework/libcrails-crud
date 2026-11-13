#include <crails/environment.hpp>
#include "../support.hpp"

#undef NDEBUG
#include <cassert>

// Assertions wrapped in `#ifdef PATCHED` describe the behavior we expect, but
// which the controller does not honor yet. Build with
// `config.cxx.poptions += -DPATCHED` to enable them.

#include <algorithm>

using namespace Support;
using namespace std;

// The JSON output is indented: compare it regardless of its layout
static string compact(string json)
{
  json.erase(remove_if(json.begin(), json.end(), [](unsigned char c) { return isspace(c); }), json.end());
  return json;
}

// A model which does not need the database
struct Article : public Model
{
  static inline vector<string> log;

  void edit(Data data)
  {
    log.push_back("edit");
    title = data["title"].defaults_to<string>("");
  }

  void on_change() { log.push_back("on_change"); }

  bool is_valid()
  {
    log.push_back("is_valid");
    if (title.length() > 0)
      return true;
    errors["title"] = string("blank");
    return false;
  }

  string title;
};

// A model which does
struct Account : public Model
{
  static inline const FakeDatabase* seen[3] = {nullptr, nullptr, nullptr};

  void edit(FakeDatabase& db, Data) { seen[0] = &db; }
  void on_change(FakeDatabase& db)  { seen[1] = &db; }
  bool is_valid(FakeDatabase& db)   { seen[2] = &db; return true; }
};

typedef TestController<Base, Article>             ArticleController;
typedef TestController<BaseWithDatabase, Account> AccountController;

int main()
{
  Crails::environment = Crails::Test;
  Setup setup;

  // A valid model: edited, notified, then validated, in that order
  {
    Article::log.clear();
    Fixture fixture(Crails::HttpVerb::post, "create");
    fixture.params()["title"] = "Hello";
    ArticleController controller(*fixture);
    int closed = 0;

    Crails::ActionRoute<ArticleController>::attach(controller, [&closed]() { ++closed; });
    assert(controller.initialize_model(controller.get_model_params()));
    assert(controller.edit_model(controller.get_model_params()));
    assert(controller.model->title == "Hello");
    assert((Article::log == vector<string>{"edit", "on_change", "is_valid"}));
    assert(fixture.status() == 200);
    assert(fixture.body().empty());
    assert(!controller.is_closed());
    assert(closed == 0);
  }

  // An invalid model is reported as a bad request, errors included
  {
    Article::log.clear();
    Fixture fixture(Crails::HttpVerb::post, "create");
    ArticleController controller(*fixture);
    int closed = 0;

    Crails::ActionRoute<ArticleController>::attach(controller, [&closed]() { ++closed; });
    assert(controller.initialize_model(controller.get_model_params()));
    assert(!controller.edit_model(controller.get_model_params()));
    assert(fixture.status() == 400);
    assert(fixture.header("Content-Type") == "application/json");
    assert(compact(fixture.body()) == "{\"title\":\"blank\"}");
    assert(controller.is_closed());
    assert(closed == 1);
  }

  // The response is sent when the controller closes: it must carry the 400.
  // Today the status is set after rendering the errors, so it is too late and
  // the client receives a 200.
  {
    Fixture fixture(Crails::HttpVerb::post, "create");
    ArticleController controller(*fixture);
    unsigned status_when_sent = 0;

    Crails::ActionRoute<ArticleController>::attach(controller, [&]() { status_when_sent = fixture.status(); });
    controller.initialize_model(controller.get_model_params());
    controller.edit_model(controller.get_model_params());
#ifdef PATCHED
    assert(status_when_sent == 400);
#endif
  }

  // validate() can be called on its own
  {
    Fixture fixture(Crails::HttpVerb::post, "create");
    ArticleController controller(*fixture);

    controller.initialize_model(controller.get_model_params());
    assert(!controller.is_model_valid());
    assert(!controller.validate());
    assert(fixture.status() == 400);
  }
  {
    Fixture fixture(Crails::HttpVerb::post, "create");
    fixture.params()["title"] = "Hello";
    ArticleController controller(*fixture);

    controller.initialize_model(controller.get_model_params());
    controller.edit_model(controller.get_model_params());
    assert(controller.is_model_valid());
    assert(controller.validate());
    assert(fixture.status() == 200);
  }

  // Models which need the database are given the controller's
  {
    Fixture fixture(Crails::HttpVerb::post, "create");
    AccountController controller(*fixture);
    const FakeDatabase* database = &controller.database;

    assert(controller.initialize_model(controller.get_model_params()));
    assert(controller.edit_model(controller.get_model_params()));
    assert(Account::seen[0] == database);
    assert(Account::seen[1] == database);
    assert(Account::seen[2] == database);
  }

  // Editing requires a model
  {
    Fixture fixture(Crails::HttpVerb::post, "create");
    ArticleController controller(*fixture);
    bool thrown = false;

    try { controller.edit_model(controller.get_model_params()); }
    catch (const NullPointerException&) { thrown = true; }
    assert(thrown);
  }

  return 0;
}
