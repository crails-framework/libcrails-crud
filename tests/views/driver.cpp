#include <crails/environment.hpp>
#include "../support.hpp"

#undef NDEBUG
#include <cassert>

using namespace Support;
using namespace std;

struct Article : public Model
{
};

struct Comment : public Model
{
  static inline const string scope = "comments";
};

struct Reply : public Model
{
  static inline const string scope = "blog/replies";
};

typedef TestController<Base, Article> ArticleController;
typedef TestController<Base, Comment> CommentController;
typedef TestController<Base, Reply>   ReplyController;

struct CustomizedController : public ArticleController
{
  CustomizedController(Crails::Context& context) : ArticleController(context) {}

  string get_view_path() const override { return "admin/"; }
  string get_id_param_name() const override { return "article_id"; }
  vector<string> get_actions_with_model() const override { return {"show", "edit"}; }
};

// Crud::Odb::Controller and Crud::MongoDB::Controller expose the hooks of
// Crud::BaseController using this macro. It has to stay in sync with it.
struct MacroController : public Crud::BaseController<Base, Comment>
{
  typedef Crud::BaseController<Base, Comment> Super;

  CRUD_BASE_CONTROLLER_IMPL(Super)

  MacroController(Crails::Context& context) : Super(context) {}

  void index() override {}
  void show() override {}
  void create() override {}
  void update() override {}
  void destroy() override {}
  bool find_model() override { return false; }
  bool find_models() override { return false; }
};

int main()
{
  Crails::environment = Crails::Test;
  Setup setup;

  // Models without a scope are rendered by the views at the root
  {
    Fixture fixture;
    ArticleController controller(*fixture);

    assert(controller.get_view_path() == "");
    assert(controller.get_show_view() == "show");
    assert(controller.get_index_view() == "index");
  }

  // Models with a scope are rendered by the views of that scope
  {
    Fixture fixture;
    CommentController controller(*fixture);

    assert(controller.get_view_path() == "comments/");
    assert(controller.get_show_view() == "comments/show");
    assert(controller.get_index_view() == "comments/index");
  }

  // Scopes may be nested
  {
    Fixture fixture;
    ReplyController controller(*fixture);

    assert(controller.get_view_path() == "blog/replies/");
    assert(controller.get_show_view() == "blog/replies/show");
    assert(controller.get_index_view() == "blog/replies/index");
  }

  // By default, models are looked up by the `id` parameter...
  {
    Fixture fixture;
    ArticleController controller(*fixture);

    assert(controller.get_id_param_name() == "id");
  }

  // ...and only the actions targeting a single model require it
  {
    Fixture fixture;
    ArticleController controller(*fixture);
    auto actions = controller.get_actions_with_model();

    assert((actions == vector<string>{"show", "update", "destroy"}));
  }

  // The hooks are virtual: view names follow a customized view path...
  {
    Fixture fixture;
    CustomizedController controller(*fixture);

    assert(controller.get_view_path() == "admin/");
    assert(controller.get_show_view() == "admin/show");
    assert(controller.get_index_view() == "admin/index");
    assert(controller.get_id_param_name() == "article_id");
    assert((controller.get_actions_with_model() == vector<string>{"show", "edit"}));
  }

  // ...even when called through the base class
  {
    Fixture fixture;
    CustomizedController controller(*fixture);
    ArticleController& base = controller;

    assert(base.get_show_view() == "admin/show");
    assert(base.get_index_view() == "admin/index");
  }

  // The macro forwards every hook to Crud::BaseController
  {
    Fixture fixture;
    MacroController controller(*fixture);

    assert(controller.get_view_path() == "comments/");
    assert(controller.get_show_view() == "comments/show");
    assert(controller.get_index_view() == "comments/index");
    assert(controller.get_id_param_name() == "id");
    assert((controller.get_actions_with_model() == vector<string>{"show", "update", "destroy"}));
    assert(controller.initialize_model(controller.get_model_params()));
    assert(controller.model != nullptr);
  }

  // Unscoped models read their attributes from the root of the parameters
  {
    Fixture fixture;
    fixture.params()["title"] = "Hello";
    fixture.params()["author"]["name"] = "Michael";
    ArticleController controller(*fixture);
    Data model_params = controller.get_model_params();

    assert(model_params["title"].as<string>() == "Hello");
    assert(model_params["author"]["name"].as<string>() == "Michael");
  }

  // Scoped models read their attributes from their scope only
  {
    Fixture fixture;
    fixture.params()["title"] = "Not for the model";
    fixture.params()["comments"]["body"] = "Hello";
    CommentController controller(*fixture);
    Data model_params = controller.get_model_params();

    assert(model_params["body"].as<string>() == "Hello");
    assert(!model_params["title"].exists());
  }

  // A nested scope is used verbatim as the key of the attributes
  {
    Fixture fixture;
    fixture.params()["blog/replies"]["body"] = "Hello";
    ReplyController controller(*fixture);

    assert(controller.get_model_params()["body"].as<string>() == "Hello");
  }

  // A scope missing from the request produces empty parameters
  {
    Fixture fixture;
    fixture.params()["title"] = "Hello";
    CommentController controller(*fixture);

    assert(!controller.get_model_params().exists());
  }

  return 0;
}
