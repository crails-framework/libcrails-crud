#include <crails/environment.hpp>
#include "../support.hpp"

#undef NDEBUG
#include <cassert>

// Assertions wrapped in `#ifdef PATCHED` describe the behavior we expect, but
// which the controller does not honor yet. Build with
// `config.cxx.poptions += -DPATCHED` to enable them.

using namespace Support;
using namespace std;

struct Article : public Model {};

typedef TestController<Base, Article> ArticleController;

struct EditableController : public ArticleController
{
  EditableController(Crails::Context& context) : ArticleController(context) {}

  vector<string> get_actions_with_model() const override { return {"edit"}; }
};

int main()
{
  Crails::environment = Crails::Test;
  Setup setup;

  // The actions targeting a single model look it up when initializing...
  for (const char* action : {"show", "update", "destroy"})
  {
    Base::reset();
    Fixture fixture(Crails::HttpVerb::get, action);
    ArticleController controller(*fixture);

    controller.initialize();
    assert(controller.find_model_calls == 1);
    assert(controller.model != nullptr);
    assert(fixture.status() == 200);
    assert(!controller.is_closed());
  }

  // ...the others do not
  for (const char* action : {"index", "create", "new_"})
  {
    Base::reset();
    Fixture fixture(Crails::HttpVerb::get, action);
    ArticleController controller(*fixture);

    controller.initialize();
    assert(controller.find_model_calls == 0);
    assert(controller.model == nullptr);
    assert(fixture.status() == 200);
  }

  // initialize() runs the base class's initialization first
  {
    Base::reset();
    Fixture fixture(Crails::HttpVerb::get, "show");
    ArticleController controller(*fixture);

    controller.initialize();
    assert(Base::log.size() == 1 && Base::log[0] == "base");
  }

  // Which actions look their model up is up to the controller
  {
    Base::reset();
    Fixture edit(Crails::HttpVerb::get, "edit");
    EditableController editing(*edit);
    Fixture show(Crails::HttpVerb::get, "show");
    EditableController showing(*show);

    editing.initialize();
    showing.initialize();
    assert(editing.find_model_calls == 1);
    assert(showing.find_model_calls == 0);
  }

  // initialize_crud() and require_model() can be called on their own
  {
    Base::reset();
    Fixture fixture(Crails::HttpVerb::get, "show");
    ArticleController controller(*fixture);

    controller.initialize_crud();
    assert(controller.find_model_calls == 1);
    controller.require_model();
    assert(controller.find_model_calls == 2);
    assert(fixture.status() == 200);
  }

  // A model which cannot be found is a 404
  for (const char* action : {"show", "update", "destroy"})
  {
    Base::reset();
    Fixture fixture(Crails::HttpVerb::get, action);
    ArticleController controller(*fixture);

    controller.model_exists = false;
    controller.initialize();
    assert(fixture.status() == 404);
    assert(controller.model == nullptr);
  }

  // ... and since the router runs the action unless the controller closed, the
  // action must not run. Otherwise `destroy` dereferences a null model (which
  // throws, and turns the 404 into a 500), and `show` renders one.
  {
    Base::reset();
    Fixture fixture(Crails::HttpVerb::get, "destroy");
    ArticleController controller(*fixture);
    int closed = 0;

    controller.model_exists = false;
    run(controller, &ArticleController::destroy, [&closed]() { ++closed; });
    assert(fixture.status() == 404);
#ifdef PATCHED
    assert(controller.is_closed());
    assert(closed == 1);
    assert(controller.actions.empty());
#endif
  }

  // When the model is found, the action runs
  {
    Base::reset();
    Fixture fixture(Crails::HttpVerb::get, "show");
    ArticleController controller(*fixture);

    run(controller, &ArticleController::show);
    assert(controller.actions.size() == 1 && controller.actions[0] == "show");
    assert(!controller.is_closed());
  }

  // initialize_model() builds a new model, whatever the parameters are
  {
    Base::reset();
    Fixture fixture(Crails::HttpVerb::post, "create");
    ArticleController controller(*fixture);

    assert(controller.model == nullptr);
    assert(controller.initialize_model(controller.get_model_params()));
    auto first = controller.model;
    assert(first != nullptr);
    assert(controller.initialize_model(controller.get_model_params()));
    assert(controller.model != first);
  }

  // The base class's checks come first: a forged request does not reach the
  // database
  {
    Base::reset();
    Base::csrf = true;
    Fixture fixture(Crails::HttpVerb::patch, "update");
    ArticleController controller(*fixture);
    bool forged = false;

    try { controller.initialize(); }
    catch (const Crails::ExceptionCSRF&) { forged = true; }
    assert(forged);
    assert(controller.find_model_calls == 0);
  }

  // Requests which pass the check carry on to the model
  {
    Base::reset();
    Base::csrf = true;
    Fixture fixture(Crails::HttpVerb::get, "show");
    ArticleController controller(*fixture);

    controller.initialize();
    assert(controller.find_model_calls == 1);
  }

  return 0;
}
