#include <crails/environment.hpp>
#include "../support.hpp"

#undef NDEBUG
#include <cassert>

using namespace Support;
using namespace std;

struct Article : public Model {};

typedef TestController<Base, Article> ArticleController;

struct SmallPagesController : public ArticleController
{
  SmallPagesController(Crails::Context& context) : ArticleController(context)
  {
    paginator.set_max_items_per_page(10);
    paginator.set_default_items_per_page(5);
  }
};

int main()
{
  Crails::environment = Crails::Test;
  Setup setup;

  // Controllers paginate their index using the request's parameters
  {
    Fixture fixture(Crails::HttpVerb::get, "index");
    fixture.params()["page"] = "3";
    fixture.params()["count"] = "20";
    ArticleController controller(*fixture);

    assert(controller.paginator.is_enabled());
    assert(controller.paginator.get_current_page() == 3);
    assert(controller.paginator.get_items_per_page() == 20);
    assert(controller.paginator.get_current_offset() == 40);
  }

  // Without parameters, the first page is served
  {
    Fixture fixture(Crails::HttpVerb::get, "index");
    ArticleController controller(*fixture);

    assert(controller.paginator.get_current_page() == 1);
    assert(controller.paginator.get_current_offset() == 0);
  }

  // The paginator follows the request's parameters, which may change after the
  // controller was built
  {
    Fixture fixture(Crails::HttpVerb::get, "index");
    ArticleController controller(*fixture);

    fixture.params()["page"] = "2";
    fixture.params()["count"] = "10";
    assert(controller.paginator.get_current_offset() == 10);
  }

  // Controllers can configure their paginator
  {
    Fixture fixture(Crails::HttpVerb::get, "index");
    SmallPagesController controller(*fixture);

    assert(controller.paginator.get_items_per_page() == 5);
    fixture.params()["count"] = "100";
    assert(controller.paginator.get_items_per_page() == 10);
  }

  // The paginator decorates the views the controller renders
  {
    Fixture fixture(Crails::HttpVerb::get, "index");
    fixture.params()["page"] = "2";
    fixture.params()["count"] = "10";
    ArticleController controller(*fixture);
    auto& vars = controller.get_shared_vars();

    controller.paginator.decorate_view(vars, []() -> unsigned long { return 42; });
    assert(any_cast<bool>(vars["with_paginator"]) == true);
    assert(any_cast<unsigned int>(vars["page"]) == 2);
    assert(any_cast<unsigned int>(vars["items_per_page"]) == 10);
    assert(any_cast<unsigned long>(vars["items_count"]) == 42);
  }

  // Disabling pagination reaches the views as well
  {
    Fixture fixture(Crails::HttpVerb::get, "index");
    ArticleController controller(*fixture);
    auto& vars = controller.get_shared_vars();
    bool counted = false;

    controller.paginator.set_enabled(false);
    controller.paginator.decorate_view(vars, [&counted]() -> unsigned long { counted = true; return 0; });
    assert(any_cast<bool>(vars["with_paginator"]) == false);
    assert(!counted);
  }

  // Each request has its own paginator
  {
    Fixture first(Crails::HttpVerb::get, "index");
    Fixture second(Crails::HttpVerb::get, "index");
    first.params()["page"] = "2";
    second.params()["page"] = "5";
    ArticleController one(*first);
    ArticleController two(*second);

    assert(one.paginator.get_current_page() == 2);
    assert(two.paginator.get_current_page() == 5);
  }

  return 0;
}
