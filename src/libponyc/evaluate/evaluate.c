#include "evaluate.h"
#include "../pass/expr.h"
#include "../pass/pass.h"
#include "../evaluate/evaluate_bool.h"
#include "../evaluate/evaluate_float.h"
#include "../evaluate/evaluate_int.h"
#include "../type/subtype.h"
#include "string.h"
#include <assert.h>

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

bool expr_constant(ast_t** astp) {
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

  ast_t* evaluated = evaluate(expression);
  if (evaluated == NULL)
  {
    ast_settype(ast, ast_from(ast_type(expression), TK_ERRORTYPE));
    return false;
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

static method_ptr_t lookup_method(ast_t* type, const char* operation) {
  const char* type_name =
    is_integer(type) || ast_id(ast_parent(type)) == TK_INT ? "integer" : 
    is_float(type) || ast_id(ast_parent(type)) == TK_FLOAT ? "float" :
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

// This is essentially the evaluate TK_FUN case however, we require
// more information regarding the arguments and receiver to evaluate
// this

// TODO: what happens with field assignments on classes
// should we dup the whole class ... may be massive
static ast_t* evaluate_method(ast_t* def, ast_t* receiver, ast_t* args)
{
  assert(receiver);
  AST_GET_CHILDREN(def, cap, id, typeparams, params, result, error, body);
  assert(ast_id(args) == TK_POSITIONALARGS || ast_id(args) == TK_NONE);

  ast_t* argument = ast_child(args);
  ast_t* parameter = ast_child(params);

  while(argument != NULL)
  {
    const char* param_name = ast_name(ast_child(parameter));
    ast_set_value(parameter, param_name, argument);
    argument = ast_sibling(argument);
    parameter = ast_sibling(parameter);
  }

  return evaluate(body);
}

static const char* get_lvalue_name(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
      return ast_name(ast_child(ast));
    default:
      return NULL;

    case TK_FLETREF:
      return ast_name(ast_childidx(ast, 1));
  }
}

ast_t* evaluate(ast_t* expression) {
  switch(ast_id(expression)) {
    case TK_NONE:
    case TK_TRUE:
    case TK_FALSE:
    case TK_INT:
    case TK_FLOAT:
    case TK_TYPEREF:
    case TK_THIS:
      return expression;

    case TK_VARREF:
    case TK_FVARREF:
      ast_error(expression, "Compile time expression can only use read-only variables");
      return NULL;

    case TK_PARAMREF:
    case TK_LETREF:
    {
      ast_t *type = ast_type(expression);
      AST_GET_CHILDREN(type, package, id, typeargs, cap, ephemeral);
      if (ast_id(cap) != TK_VAL && ast_id(cap) != TK_BOX)
      {
        ast_error(expression, "Compile time expression can only use read-only variables");
        return NULL;
      }
      return ast_get_value(expression, ast_name(ast_child(expression)));
    }

    case TK_FLETREF:
    {
      // this will need to know the object
      ast_t *type = ast_type(expression);
      AST_GET_CHILDREN(type, package, id, typeargs, cap, ephemeral);
      if (ast_id(cap) != TK_VAL && ast_id(cap) != TK_BOX)
      {
        ast_error(expression, "Compile time expression can only use read-only variables");
        return NULL;
      }

      AST_GET_CHILDREN(expression, object, field_id);
      ast_t* evaluated_object = evaluate(object);
      if(!evaluated_object)
        return NULL;

      return ast_get_value(evaluated_object, ast_name(field_id));
    }

    case TK_SEQ:
    {
      ast_t * evaluated;
      for(ast_t* p = ast_child(expression); p != NULL; p = ast_sibling(p))
      {
        evaluated = evaluate(p);
        if(!evaluated)
          return NULL;
      }
      return evaluated;
    }

    case TK_CALL:
    {
      AST_GET_CHILDREN(expression, positional, named, function);
      AST_GET_CHILDREN(function, receiver, id);

      // named arguments have already been converted to positional
      assert(ast_id(named) == TK_NONE);

      ast_t* evaluated_receiver = evaluate(receiver);

      ast_t* evaluated_positional_args = ast_dup(positional);
      ast_t* argument = ast_child(evaluated_positional_args);
      while(argument != NULL)
      {
        ast_t* evaluated_argument = evaluate(argument);
        ast_replace(&argument, evaluated_argument);
        argument = ast_sibling(argument);
      }

      // TODO: should probably check here if this is a builtin type
      ast_t* type = ast_get_base_type(evaluated_receiver);
      method_ptr_t builtin_method = lookup_method(type, ast_name(id));
      if(builtin_method)
        return builtin_method(evaluated_receiver,
                              evaluated_positional_args);

      // the method lookup should probabyl be on the evaluated receiver node
      // this way fields will have been assigned to

      const char* type_name = ast_name(ast_childidx(type, 1));
      ast_t* type_def = ast_get(evaluated_receiver, type_name, NULL);
      ast_t* method_def = ast_get(type_def, ast_name(id), NULL);
      assert(method_def);

      // FIXME: duplicating this each time is going to be expensive ;_;
      return evaluate_method(ast_dup(method_def),
                             evaluated_receiver,
                             evaluated_positional_args);
    }

    case TK_IF:
    case TK_ELSEIF:
    {
      AST_GET_CHILDREN(expression, condition, then_branch, else_branch);
      ast_t* condition_evaluated = evaluate(condition);

      return ast_id(condition_evaluated) == TK_TRUE ?
             evaluate(then_branch):
             evaluate(else_branch);
    }

    case TK_ASSIGN:
    {
      AST_GET_CHILDREN(expression, right, left);

      const char* name = get_lvalue_name(left);
      if(name)
        ast_set_value(left, name, evaluate(right));

      return right;
    }

    default:
      ast_error(expression, "Cannot evaluate compile time expression");
      return NULL;
  }
  return NULL;
}
