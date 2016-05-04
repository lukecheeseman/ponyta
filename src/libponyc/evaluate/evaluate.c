#include "evaluate.h"
#include "../ast/astbuild.h"
#include "../pass/expr.h"
#include "../pass/pass.h"
#include "../evaluate/evaluate_bool.h"
#include "../evaluate/evaluate_float.h"
#include "../evaluate/evaluate_int.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "string.h"
#include <assert.h>
#include <inttypes.h>

bool ast_equal(ast_t* left, ast_t* right)
{
  if(left == NULL || right == NULL)
    return left == NULL && right == NULL;

  if(ast_id(left) != ast_id(right))
    return false;

  switch(ast_id(left))
  {
    case TK_ID:
    case TK_STRING:
      return ast_name(left) == ast_name(right);

    case TK_INT:
      return lexint_cmp(ast_int(left), ast_int(right)) == 0;

    case TK_FLOAT:
      return ast_float(left) == ast_float(right);

    default:
      break;
  }

  ast_t* l_child = ast_child(left);
  ast_t* r_child = ast_child(right);

  while(l_child != NULL && r_child != NULL)
  {
    if(!ast_equal(l_child, r_child))
      return false;

    l_child = ast_sibling(l_child);
    r_child = ast_sibling(r_child);
  }

  return l_child == NULL && r_child == NULL;
}

bool contains_valueparamref(ast_t* ast) {
  while(ast != NULL) {
    if(ast_id(ast) == TK_VALUEFORMALPARAMREF || contains_valueparamref(ast_child(ast)))
      return true;
    ast = ast_sibling(ast);
  }
  return false;
}

bool expr_constant(pass_opt_t* opt, ast_t** astp) {
  // If we see a compile time expression
  // we first evaluate it then replace this node with the result
  ast_t *ast = *astp;
  assert(ast_id(ast) == TK_CONSTANT);

  ast_t* expression = ast_child(ast);
  ast_settype(ast, ast_type(expression));

  if(is_typecheck_error(ast_type(expression)))
    return false;

  if(contains_valueparamref(expression))
    return true;

  // evaluate the expression passing NULL as this as we aren't
  // evaluate a method on an object
  ast_t* evaluated = evaluate(opt, expression, NULL);
  if (evaluated == NULL)
  {
    ast_settype(ast, ast_from(ast_type(expression), TK_ERRORTYPE));
    ast_error(expression, "could not evaluate compile time expression");
    return false;
  }
  ast_setconstant(evaluated);

  ast_t* type = ast_type(evaluated);
  if(ast_id(type) == TK_NOMINAL)
  {
    ast_t* cap = ast_childidx(type, 3);
    if(ast_id(cap) != TK_VAL)
    {
      ast_error(expression, "result of compile time expression must be val");
      return false;
    }
  }

  ast_replace(astp, evaluated);
  return true;
}

typedef ast_t* (*method_ptr_t)(ast_t*, ast_t*);

typedef struct method_entry {
  const char* type;
  const char* name;
  const method_ptr_t method;
} method_entry_t;

// This table will have to be updated to be in the relevant classes
static method_entry_t method_table[] = {
  // integer operations
  { "integer" , "create"  , &evaluate_create_int },
  { "integer" , "add"     , &evaluate_add_int    },
  { "integer" , "sub"     , &evaluate_sub_int    },
  { "integer" , "mul"     , &evaluate_mul_int    },
  { "integer" , "div"     , &evaluate_div_int    },

  { "integer" , "neg"     , &evaluate_neg_int    },
  { "integer" , "eq"      , &evaluate_eq_int     },
  { "integer" , "ne"      , &evaluate_ne_int     },
  { "integer" , "lt"      , &evaluate_lt_int     },
  { "integer" , "le"      , &evaluate_le_int     },
  { "integer" , "gt"      , &evaluate_gt_int     },
  { "integer" , "ge"      , &evaluate_ge_int     },

  { "integer" , "min"     , &evaluate_min_int    },
  { "integer" , "max"     , &evaluate_max_int    },

  { "integer" , "hash"    , &evaluate_hash_int   },

  { "integer" , "op_and"  , &evaluate_and_int    },
  { "integer" , "op_or"   , &evaluate_or_int     },
  { "integer" , "op_xor"  , &evaluate_xor_int    },
  { "integer" , "op_not"  , &evaluate_not_int    },
  { "integer" , "shl"     , &evaluate_shl_int    },
  { "integer" , "shr"     , &evaluate_shr_int    },

  // integer casting methods
  { "integer" , "i8"      , &evaluate_i8_int     },
  { "integer" , "i16"     , &evaluate_i16_int    },
  { "integer" , "i32"     , &evaluate_i32_int    },
  { "integer" , "i64"     , &evaluate_i64_int    },
  { "integer" , "i128"    , &evaluate_i128_int   },
  { "integer" , "ilong"   , &evaluate_ilong_int  },
  { "integer" , "isize"   , &evaluate_isize_int  },
  { "integer" , "u8"      , &evaluate_u8_int     },
  { "integer" , "u16"     , &evaluate_u16_int    },
  { "integer" , "u32"     , &evaluate_u32_int    },
  { "integer" , "u64"     , &evaluate_u64_int    },
  { "integer" , "u128"    , &evaluate_u128_int   },
  { "integer" , "ulong"   , &evaluate_ulong_int  },
  { "integer" , "usize"   , &evaluate_usize_int  },
  { "integer" , "f32"     , &evaluate_f32_int    },
  { "integer" , "f64"     , &evaluate_f64_int    },

  //float operations
  { "float"   , "add"     , &evaluate_add_float  },
  { "float"   , "sub"     , &evaluate_sub_float  },
  { "float"   , "mul"     , &evaluate_mul_float  },
  { "float"   , "div"     , &evaluate_div_float  },

  { "float"   , "neg"     , &evaluate_neg_float  },
  { "float"   , "eq"      , &evaluate_eq_float   },
  { "float"   , "ne"      , &evaluate_ne_float   },
  { "float"   , "lt"      , &evaluate_lt_float   },
  { "float"   , "le"      , &evaluate_le_float   },
  { "float"   , "gt"      , &evaluate_gt_float   },
  { "float"   , "ge"      , &evaluate_ge_float   },

  // boolean operations
  { "Bool"    , "op_and"  , &evaluate_and_bool   },
  { "Bool"    , "op_or"   , &evaluate_or_bool    },

  // no entry in method table
  { NULL, NULL, NULL }
};

static method_ptr_t lookup_method(ast_t* receiver, ast_t* type,
  const char* operation)
{
  const char* type_name =
    is_integer(type) || ast_id(receiver) == TK_INT ? "integer" :
    is_float(type) || ast_id(receiver) == TK_FLOAT ? "float" :
    ast_name(ast_childidx(type, 1));

  for (int i = 0; method_table[i].name != NULL; ++i) {
    if (!strcmp(type_name, method_table[i].type) && !strcmp(operation, method_table[i].name))
      return method_table[i].method;
  }
  return NULL;
}

// TODO: is this wrong?
static ast_t* ast_get_base_type(ast_t* ast)
{
  ast_t* type = ast_type(ast);
  switch(ast_id(type))
  {
    case TK_NOMINAL:
    case TK_LITERAL:
      return type;

    case TK_ARROW:
      return ast_childidx(type, 1);

    default: assert(0);
  }
  return NULL;
}

static const char* get_lvalue_name(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
    case TK_REFERENCE:
      return ast_name(ast_child(ast));

    case TK_EMBEDREF:
    case TK_FLETREF:
      return ast_name(ast_childidx(ast, 1));

    default:
      assert(0);
  }
  return NULL;
}

// FIXME: a per type object count would be nicer
static uint64_t count = 0;

// This is essentially the evaluate TK_FUN/TK_NEW case however, we require
// more information regarding the arguments and receiver to evaluate
// this
static ast_t* evaluate_method(pass_opt_t* opt, ast_t* function, ast_t* args,
  ast_t* this)
{
  AST_GET_CHILDREN(function, receiver, func_id);
  ast_t* evaluated_receiver = evaluate(opt, receiver, this);
  if(evaluated_receiver == NULL)
    return NULL;

  // First lookup to see if we have a special method to evaluate the expression
  ast_t* type = ast_get_base_type(evaluated_receiver);
  method_ptr_t builtin_method
    = lookup_method(evaluated_receiver, type, ast_name(func_id));
  if(builtin_method != NULL)
    return builtin_method(evaluated_receiver, args);

  // We ensure that we have type checked the method before we attempt to
  // evaluate it, this is so that we do not attempt to evaluate erroneuos
  // functions and also as we require that expressions have been correctly
  // desugared and types assigned.
  // TODO: this can go absolutely insane on .string()
  switch(ast_id(function))
  {
    case TK_NEWREF:
    {
      // Here we typecheck the whole class as the constructor may rely on
      // fields etc. it is also likely that we would then go on to use some
      // of the methods in this class.
      ast_t* def = ast_get(function, ast_name(ast_childidx(type, 1)), NULL);
      if(ast_visit(&def, pass_pre_expr, pass_expr, opt, PASS_EXPR) != AST_OK)
        return NULL;
      break;
    }

    case TK_FUNREF:
    {
      // find the method and type check it
      ast_t* type_def = ast_get(function, ast_name(ast_childidx(type, 1)), NULL);
      ast_t* def = ast_get(type_def, ast_name(func_id), NULL);
      if(ast_visit(&def, pass_pre_expr, pass_expr, opt, PASS_EXPR) != AST_OK)
        return NULL;
      break;
    }

    case TK_BEREF:
      ast_error(function, "cannot evaluate compile-time behaviours");
      return NULL;

    default: break;
  }

  // lookup the reified defintion of the function
  ast_t* function_def = lookup(opt, receiver, type, ast_name(func_id));
  if(function_def == NULL)
    return NULL;

  // map each parameter to its argument value in the symbol table
  ast_t* params = ast_childidx(function_def, 3);
  ast_t* argument = ast_child(args);
  ast_t* parameter = ast_child(params);
  while(argument != NULL)
  {
    const char* param_name = ast_name(ast_child(parameter));
    ast_set_value(parameter, param_name, argument);
    argument = ast_sibling(argument);
    parameter = ast_sibling(parameter);
  }

  // look up the body of the method so that we can evaluate it
  ast_t* body = ast_childidx(function_def, 6);

  // push the receiver and evaluate the body
  ast_t* evaluated = evaluate(opt, body, evaluated_receiver);

  if(evaluated == NULL)
  {
    ast_error(function, "function is not a compile time expression");
    return NULL;
  }

  // If the method is a constructor then we need to build a compile time object
  // adding the values of the fields as the children and setting the type
  // to be the return type of the function body
  if(ast_id(function_def) == TK_NEW)
  {
    // TODO: we need to create some name or means by which to indicate that
    // we are refering to the same object
    const char* type_name = ast_name(ast_childidx(type, 1));
    char obj_name[strlen(type_name) + 11 + 21];
    sprintf(obj_name, "%s_$instance_%" PRIu64, type_name, count++);

    BUILD(obj, receiver,
      NODE(TK_CONSTANT_OBJECT, ID(obj_name) NODE(TK_MEMBERS)))
    ast_set_symtab(obj, ast_get_symtab(function_def));

    // get the return type
    ast_t* ret_type = ast_childidx(ast_type(function), 3);
    ast_settype(obj, ast_dup(ret_type));

    // find the class definition and add the members of the object as child
    // nodes
    ast_t* class_def = ast_get(receiver, type_name, NULL);
    ast_t* obj_members = ast_childidx(obj, 1);
    ast_t* members = ast_childidx(class_def, 4);
    ast_t* member = ast_child(members);
    while(member != NULL)
    {
      switch(ast_id(member))
      {
        case TK_FVAR:
          ast_error(member, "compile time objects may only have read-only fields");
          return NULL;

        case TK_EMBED:
        case TK_FLET:
        {
          const char* field_name = ast_name(ast_child(member));
          ast_append(obj_members, ast_get_value(obj, field_name));
        }
        default:
          break;
      }
      member = ast_sibling(member);
    }
    return obj;
  }

  return evaluated;
}

ast_t* evaluate(pass_opt_t* opt, ast_t* expression, ast_t* this) {
  switch(ast_id(expression)) {
    // Literal cases where we can return the value
    case TK_NONE:
    case TK_TRUE:
    case TK_FALSE:
    case TK_INT:
    case TK_FLOAT:
    case TK_CONSTANT_OBJECT:
      return expression;

    case TK_FUNREF:
    case TK_TYPEREF:
      return expression;

    // If we're evaluating a method on an object then we have a this node
    // representing the object. Otherwise we just return the current this node.
    case TK_THIS:
      return this == NULL ? expression : this;

    // We do not allow var references to be used in compile time expressions
    case TK_VARREF:
    case TK_FVARREF:
      ast_error(expression, "compile time expression can only use read-only variables");
      return NULL;

    case TK_PARAMREF:
    case TK_LETREF:
    {
      ast_t *type = ast_type(expression);
      ast_t* cap = ast_childidx(type, 3);
      if (ast_id(cap) != TK_VAL && ast_id(cap) != TK_BOX)
      {
        ast_error(expression, "compile time expression can only use read-only variables");
        return NULL;
      }

      ast_t* value = ast_get_value(expression, ast_name(ast_child(expression)));
      if(value == NULL)
      {
        ast_error(expression, "variable is not a compile time expression");
        return false;
      }
      return value;
    }

    case TK_EMBEDREF:
    case TK_FLETREF:
    {
      // this will need to know the object
      ast_t *type = ast_type(expression);
      ast_t* cap = ast_childidx(type, 3);
      if (ast_id(cap) != TK_VAL && ast_id(cap) != TK_BOX)
      {
        ast_error(expression, "compile time expression can only use read-only variables");
        return NULL;
      }

      AST_GET_CHILDREN(expression, receiver, id);
      ast_t* evaluated_receiver = evaluate(opt, receiver, this);
      if(evaluated_receiver == NULL)
      {
        ast_error(receiver, "could not evaluate receiver");
        return NULL;
      }

      ast_t* field = ast_get_value(evaluated_receiver, ast_name(id));
      if(field == NULL)
      {
        ast_error(expression, "could not find field");
        return NULL;
      }
      return field;
    }

    case TK_SEQ:
    {
      ast_t * evaluated;
      for(ast_t* p = ast_child(expression); p != NULL; p = ast_sibling(p))
      {
        evaluated = evaluate(opt, p, this);
        if(evaluated == NULL)
        {
          ast_error(p, "could not evaluate compile time expression");
          return NULL;
        }
      }
      return evaluated;
    }

    case TK_CALL:
    {
      AST_GET_CHILDREN(expression, positional, named, function);

      // named arguments have already been converted to positional
      assert(ast_id(named) == TK_NONE);

      // build up the evaluated arguments
      ast_t* evaluated_positional_args = ast_from(positional, ast_id(positional));
      ast_t* argument = ast_child(positional);
      while(argument != NULL)
      {
        ast_t* evaluated_argument = evaluate(opt, argument, this);
        if(evaluated_argument == NULL)
          return NULL;
        ast_append(evaluated_positional_args, evaluated_argument);
        argument = ast_sibling(argument);
      }

      return evaluate_method(opt, function, evaluated_positional_args, this);
    }

    case TK_IF:
    case TK_ELSEIF:
    {
      AST_GET_CHILDREN(expression, condition, then_branch, else_branch);
      ast_t* condition_evaluated = evaluate(opt, condition, this);

      return ast_id(condition_evaluated) == TK_TRUE ?
             evaluate(opt, then_branch, this):
             evaluate(opt, else_branch, this);
    }

    case TK_ASSIGN:
    {
      AST_GET_CHILDREN(expression, right, left);

      const char* name = get_lvalue_name(left);
      assert(name != NULL);
      ast_set_value(left, name, evaluate(opt, right, this));

      return right;
    }

    default:
      assert(0);
      ast_error(expression, "Cannot evaluate compile time expression");
      return NULL;
  }
  return NULL;
}
