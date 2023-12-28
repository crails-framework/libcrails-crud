#ifdef WITH_ODB
#ifndef  CRUD_ODB_CONTROLLER_HPP
# define CRUD_ODB_CONTROLLER_HPP

# include "../../crud/base_controller.hpp"
# include <crails/odb/connection.hpp>
# include <crails/odb/to_vector.hpp>

namespace Crud
{
  namespace Odb
  {
    template<typename BASE, typename MODEL, typename QUERY_OBJECT = MODEL>
    class Controller : public Crud::BaseController<BASE, MODEL>
    {
      typedef Crud::BaseController<BASE, MODEL> Super;
    public:
      typedef odb::query<QUERY_OBJECT> Query;

      CRUD_BASE_CONTROLLER_IMPL(Super)

      Controller(Crails::Context& context) : Super(context)
      {
      }

      void index() override
      {
        find_models();
        Super::paginator.decorate_view(Super::vars, [this]() {
          return Super::database.template count<QUERY_OBJECT>(get_index_query());
        });
        Super::vars["models"] = &(reinterpret_cast<std::vector<MODEL>&>(Super::models));
        Super::render(get_index_view());
      }

      void show() override
      {
        Super::vars["model"] = Super::model.get();
        Super::render(get_show_view());
      }

      void create() override
      {
        Data model_params = get_model_params();

        if (initialize_model(model_params) && edit_model(model_params))
        {
          Super::database.save(*Super::model);
          after_create();
          Super::vars["model"] = Super::model.get();
          Super::render(get_show_view());
        }
      }

      void update() override
      {
        if (edit_model(get_model_params()))
        {
          Super::database.save(*Super::model);
          after_update();
          Super::vars["model"] = Super::model.get();
          Super::render(get_show_view());
        }
      }

      void destroy() override
      {
        Super::database.destroy(*Super::model);
        after_destroy();
        Super::respond_with(Crails::HttpStatus::ok);
      }

    protected:
      virtual void after_update() {}
      virtual void after_create() {}
      virtual void after_destroy() {}

      virtual Query get_index_query() const
      {
        return Query(true);
      }

      virtual bool find_model() override
      {
        auto param_name = get_id_param_name();
        auto id = Super::params[param_name].template as<Crails::Odb::id_type>();

        return Super::database.find_one(Super::model, odb::query<MODEL>::id == id);
      }

      virtual bool find_models() override
      {
        Query query = get_index_query();
        odb::result<QUERY_OBJECT> results;

        Super::paginator.decorate_query(query);
        Super::database.find(results, query);
        Super::models = Crails::Odb::to_vector<MODEL, QUERY_OBJECT>(results);
        return Super::models.size() > 0;
      }
    };
  }
}

#endif
#endif
