#pragma once
#include <crails/params.hpp>
#include <crails/safe_ptr.hpp>
#include <crails/controller.hpp>
#include <crails/paginator.hpp>

#define CRUD_BASE_CONTROLLER_IMPL(SUPER) \
    virtual std::string get_id_param_name() const override { return SUPER::get_id_param_name(); } \
    virtual Data get_model_params() override { return SUPER::get_model_params(); } \
    virtual bool edit_model(Data data) override { return SUPER::edit_model(data); } \
    virtual bool initialize_model(Data data) override { return SUPER::initialize_model(data); } \
    virtual std::string get_view_path() const override { return SUPER::get_view_path(); } \
    virtual std::string get_show_view()  const override { return SUPER::get_show_view(); } \
    virtual std::string get_index_view() const override { return SUPER::get_index_view(); } \
    virtual std::vector<std::string> get_actions_with_model() const override { return SUPER::get_actions_with_model(); } \
    virtual void initialize_crud() override { return SUPER::initialize_crud(); }

namespace Crud
{
  template<typename BASE, typename MODEL>
  class BaseController : public BASE
  {
    template<typename CONTROLLER>
    friend class ActionRoute;
  protected:
    BaseController(Crails::Context& context) :
      BASE(context),
      paginator(BASE::params.as_data())
    {
    }

    virtual void index() = 0;
    virtual void show() = 0;
    virtual void create() = 0;
    virtual void update() = 0;
    virtual void destroy() = 0;

    virtual bool find_model() = 0;
    virtual bool find_models() = 0;

    void initialize() override
    {
      BASE::initialize();
      initialize_crud();
    }

    virtual void initialize_crud()
    {
      for (auto action : get_actions_with_model())
      {
        if (action == BASE::params["controller-data"]["action"].template as<std::string>())
        {
          require_model();
          break ;
        }
      }
    }
  
    virtual std::string get_view_path() const
    {
      if (MODEL::scope != "")
        return MODEL::scope + "/";
      return "";
    }

    virtual std::string get_show_view()  const { return get_view_path() + "show"; }
    virtual std::string get_index_view() const { return get_view_path() + "index"; }

    virtual std::vector<std::string> get_actions_with_model() const
    {
      return { "show", "update", "destroy" };
    }

    virtual bool initialize_model(Data data)
    {
      model = std::make_shared<MODEL>();
      return true;
    }

    virtual bool edit_model(Data data)
    {
      model->edit(data);
      model->on_change();
      return validate();
    }

    virtual bool validate()
    {
      if (!model->is_valid())
      {
        Crails::Controller::render(Crails::Controller::JSON,
          model->errors.as_data()
        );
        BASE::response.set_status_code(Crails::HttpStatus::bad_request);
        return false;
      }
      return true;
    }

    virtual Data get_model_params()
    {
      if (MODEL::scope == "")
        return BASE::params.as_data();
      return BASE::params[MODEL::scope];
    }

    void require_model()
    {
      if (!find_model())
        BASE::response.set_status_code(Crails::HttpStatus::not_found);
    }

    virtual std::string get_id_param_name() const
    {
      return "id";
    }

    Crails::Paginator  paginator;
    safe_ptr<MODEL>    model;
  public:
    std::vector<MODEL> models;
  };
}
