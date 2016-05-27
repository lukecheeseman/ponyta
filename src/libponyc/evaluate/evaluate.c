#include "evaluate.h"
#include "../ast/astbuild.h"
#include "../pass/expr.h"
#include "../pass/pass.h"
#include "../pkg/package.h"
#include "../evaluate/evaluate_bool.h"
#include "../evaluate/evaluate_float.h"
#include "../evaluate/evaluate_int.h"
#include "../evaluate/evaluate_vector.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../expr/literal.h"
#include "../expr/operator.h"
#include "string.h"
#include <assert.h>
#include <inttypes.h>
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/ds/hash.h"

static ast_t* evaluate(pass_opt_t* opt, ast_t* expression, ast_t* scope,
                       int depth);

static bool evaluate_expression(pass_opt_t* opt, ast_t** astp);

bool evaluate_expressions(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  // FIXME: the type of expressions hasn't been resolved yet
  ast_t* type = ast_type(ast);
  if(type != NULL)
    if(!evaluate_expressions(opt, &type))
      return false;

  if(ast_id(ast) == TK_CONSTANT)
  {
    //FIXME: this hack is dues to nesting constants in constants
    // the breaking example involved a matrix where the dimensions were
    // the dims did not have an evaluated type after reification
    ast_t* expr = ast_child(ast);
    evaluate_expressions(opt, &expr);
    return evaluate_expression(opt, astp);
  }

  ast_t* child = ast_child(ast);
  while(child != NULL)
  {
    if(!evaluate_expressions(opt, &child))
      return false;

    child = ast_sibling(child);
  }

  // FIXME: We may have to reassign some expressions
  if(ast_id(ast) == TK_ASSIGN)
  {
    AST_GET_CHILDREN(ast, right, left);
    map_value(opt, left, right, true);
  }

  return true;
}

bool ast_equal(ast_t* left, ast_t* right)
{
  if(left == NULL || right == NULL)
    return left == NULL && right == NULL;

  // Look through all of the constant expression directives
  if(ast_id(left) == TK_CONSTANT)
    return ast_equal(ast_child(left), right);

  if(ast_id(right) == TK_CONSTANT)
    return ast_equal(left, ast_child(right));

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

    case TK_CONSTANT_OBJECT:
      return ast_equal(ast_childidx(left, 1), ast_childidx(right, 1));

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

static size_t ast_hash(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_ID:
    case TK_STRING:
      return ponyint_hash_str(ast_name(ast));

    case TK_INT:
    {
      lexint_t* val = ast_int(ast);
      return val->low ^ val->high;
    }

    case TK_FLOAT:
      return (size_t)ast_float(ast);

    case TK_CONSTANT_OBJECT:
      return ast_hash(ast_childidx(ast, 1));

    default:
      break;
  }

  size_t hash = ast_id(ast);
  ast_t* child = ast_child(ast);

  while(child)
  {
    hash ^= ast_hash(child);
    child = ast_sibling(child);
  }

  return hash;

}

typedef struct cache_entry_t
{
  ast_t* ast;
  ast_t* value;
} cache_entry_t;

static cache_entry_t* cache_entry_dup(cache_entry_t* entry)
{
  cache_entry_t* c = POOL_ALLOC(cache_entry_t);
  memcpy(c, entry, sizeof(cache_entry_t));
  return c;
}

static size_t cache_hash(cache_entry_t* entry)
{
  return ast_hash(entry->ast);
}

static bool cache_cmp(cache_entry_t* a, cache_entry_t* b)
{
  return a->ast == b->ast || ast_equal(a->ast, b->ast);
}

static void cache_free(cache_entry_t* cache)
{
  POOL_FREE(cache_entry_t, cache);
}

DECLARE_HASHMAP(cache, cache_t, cache_entry_t);
DEFINE_HASHMAP(cache, cache_t, cache_entry_t,
  cache_hash, cache_cmp, ponyint_pool_alloc_size, ponyint_pool_free_size,
  cache_free);

static cache_t cache;

static void obj_cache(ast_t** obj)
{
  cache_entry_t c1 = {*obj, *obj};
  cache_entry_t* c2 = cache_get(&cache, &c1);

  if(c2 != NULL)
  {
    ast_replace(obj, c2->value);
    return;
  }

  cache_put(&cache, cache_entry_dup(&c1));
}

void eval_cache_init()
{
  cache_init(&cache, 50);
}

void eval_cache_done()
{
  cache_destroy(&cache);
  memset(&cache, 0, sizeof(cache_t));
}

bool contains_valueparamref(ast_t* ast) {
  while(ast != NULL) {
    if(ast_id(ast) == TK_VALUEFORMALPARAMREF || contains_valueparamref(ast_child(ast)))
      return true;
    ast = ast_sibling(ast);
  }
  return false;
}

static bool eval_error(ast_t* ast)
{
  return (ast == NULL || ast_id(ast) == TK_ERROR ||
          ast_id(ast) == TK_VALUEFORMALPARAMREF);
}

bool expr_constant(pass_opt_t* opt, ast_t** astp) {
  // If we see a compile time expression
  // we first evaluate it then replace this node with the result
  ast_t* ast = *astp;

  // We set this here as we may not yet be able to evaluate the expressions
  // due to references, but we need to know for assignment what we believe
  // to be a compile tim expression.
  assert(ast_id(ast) == TK_CONSTANT);
  ast_setconstant(ast);

  ast_t* expression = ast_child(ast);
  ast_t* expr_type = ast_type(expression);

  if(is_typecheck_error(ast_type(expression)))
    return false;

  // See if we can recover the expression to val capability
  if(!is_type_literal(expr_type))
  {
    expr_type = recover_type(expr_type, TK_VAL);
    if(expr_type == NULL)
    {
      ast_error(opt->check.errors, expression,
        "can't recover compile-time object to val capability");
      return false;
    }
  }
  ast_settype(ast, expr_type);

  return true;
}

static bool evaluate_expression(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  assert(ast_id(ast) == TK_CONSTANT);
  ast_setconstant(ast);

  ast_t* expression = ast_child(ast);

  if(contains_valueparamref(expression))
    return true;

  // TODO: explaing the types of error
  // evaluate the expression passing NULL as this as we aren't
  // evaluate a method on an object
  ast_t* evaluated = evaluate(opt, expression, NULL, 0);
  if(eval_error(evaluated) && evaluated == NULL)
  {
    ast_settype(ast, ast_from(ast_type(expression), TK_ERRORTYPE));
    ast_error(opt->check.errors, expression,
              "could not evaluate compile time expression");
    return false;
  }

  if(eval_error(evaluated) && ast_id(evaluated) == TK_ERROR)
  {
    ast_settype(ast, ast_from(ast_type(expression), TK_ERRORTYPE));
    ast_error(opt->check.errors, expression,
              "unresolved error occurred during evaluation");
    ast_error_continue(opt->check.errors, evaluated,
              "error originated from here");
    return false;
  }

  ast_setconstant(evaluated);

  //FIXME: bro plz don't check again, there has to be a nicer way of handling
  //this, the issue is that in templates which use value parms in constant
  //expressions, we won't know anything at the template stage to evaluate things
  //like types and so on
  if(contains_valueparamref(evaluated))
    return true;

  ast_t* type = ast_type(evaluated);
  if(ast_id(type) == TK_NOMINAL)
  {
    ast_t* cap = ast_childidx(type, 3);
    if(ast_id(cap) != TK_VAL)
    {
      ast_error(opt->check.errors, expression,
                "result of compile time expression must be val");
      return false;
    }
  }

  ast_replace(astp, evaluated);
  return true;
}

typedef ast_t* (*method_ptr_t)(ast_t*, ast_t*, pass_opt_t* opt);

typedef struct method_entry_t
{
  const char* type;
  const char* name;
  const method_ptr_t method;
} method_entry_t;

static method_entry_t* method_dup(method_entry_t* method)
{
  method_entry_t* m = POOL_ALLOC(method_entry_t);
  memcpy(m, method, sizeof(method_entry_t));
  return m;
}

static size_t method_hash(method_entry_t* method)
{
  return ponyint_hash_ptr(method->name) ^ ponyint_hash_ptr(method->name);
}

static bool method_cmp(method_entry_t* a, method_entry_t* b)
{
  return a->name == b->name && a->type == b->type;
}

static void method_free(method_entry_t* method)
{
  POOL_FREE(method_entry_t, method);
}

DECLARE_HASHMAP(method_table, method_table_t, method_entry_t);
DEFINE_HASHMAP(method_table, method_table_t, method_entry_t,
  method_hash, method_cmp, ponyint_pool_alloc_size, ponyint_pool_free_size,
  method_free);

static method_table_t method_table;

static void add_method(const char* name, const char* type, method_ptr_t method)
{
  method_entry_t m = {name, type, method};
  method_table_put(&method_table, method_dup(&m));
}

void methodtab_init()
{
  method_table_init(&method_table, 20);

  // integer operations
  add_method(stringtab("integer"), stringtab("create"), &evaluate_create_int);
  add_method(stringtab("integer"), stringtab("create"), &evaluate_create_int);
  add_method(stringtab("integer"), stringtab("add"), &evaluate_add_int);
  add_method(stringtab("integer"), stringtab("sub"), &evaluate_sub_int);
  add_method(stringtab("integer"), stringtab("mul"), &evaluate_mul_int);
  add_method(stringtab("integer"), stringtab("div"), &evaluate_div_int);

  add_method(stringtab("integer"), stringtab("neg"), &evaluate_neg_int);
  add_method(stringtab("integer"), stringtab("eq"), &evaluate_eq_int);
  add_method(stringtab("integer"), stringtab("ne"), &evaluate_ne_int);
  add_method(stringtab("integer"), stringtab("lt"), &evaluate_lt_int);
  add_method(stringtab("integer"), stringtab("le"), &evaluate_le_int);
  add_method(stringtab("integer"), stringtab("gt"), &evaluate_gt_int);
  add_method(stringtab("integer"), stringtab("ge"), &evaluate_ge_int);

  add_method(stringtab("integer"), stringtab("min"), &evaluate_min_int);
  add_method(stringtab("integer"), stringtab("max"), &evaluate_max_int);

  add_method(stringtab("integer"), stringtab("hash"), &evaluate_hash_int);

  add_method(stringtab("integer"), stringtab("op_and"), &evaluate_and_int);
  add_method(stringtab("integer"), stringtab("op_or"), &evaluate_or_int);
  add_method(stringtab("integer"), stringtab("op_xor"), &evaluate_xor_int);
  add_method(stringtab("integer"), stringtab("op_not"), &evaluate_not_int);
  add_method(stringtab("integer"), stringtab("shl"), &evaluate_shl_int);
  add_method(stringtab("integer"), stringtab("shr"), &evaluate_shr_int);

  // integer casting methods
  add_method(stringtab("integer"), stringtab("i8"), &evaluate_i8_int);
  add_method(stringtab("integer"), stringtab("i16"), &evaluate_i16_int);
  add_method(stringtab("integer"), stringtab("i32"), &evaluate_i32_int);
  add_method(stringtab("integer"), stringtab("i64"), &evaluate_i64_int);
  add_method(stringtab("integer"), stringtab("i128"), &evaluate_i128_int);
  add_method(stringtab("integer"), stringtab("ilong"), &evaluate_ilong_int);
  add_method(stringtab("integer"), stringtab("isize"), &evaluate_isize_int);
  add_method(stringtab("integer"), stringtab("u8"), &evaluate_u8_int);
  add_method(stringtab("integer"), stringtab("u16"), &evaluate_u16_int);
  add_method(stringtab("integer"), stringtab("u32"), &evaluate_u32_int);
  add_method(stringtab("integer"), stringtab("u64"), &evaluate_u64_int);
  add_method(stringtab("integer"), stringtab("u128"), &evaluate_u128_int);
  add_method(stringtab("integer"), stringtab("ulong"), &evaluate_ulong_int);
  add_method(stringtab("integer"), stringtab("usize"), &evaluate_usize_int);
  add_method(stringtab("integer"), stringtab("f32"), &evaluate_f32_int);
  add_method(stringtab("integer"), stringtab("f64"), &evaluate_f64_int);

  //float operations
  add_method(stringtab("float"), stringtab("add"), &evaluate_add_float);
  add_method(stringtab("float"), stringtab("sub"), &evaluate_sub_float);
  add_method(stringtab("float"), stringtab("mul"), &evaluate_mul_float);
  add_method(stringtab("float"), stringtab("div"), &evaluate_div_float);

  add_method(stringtab("float"), stringtab("neg"), &evaluate_neg_float);
  add_method(stringtab("float"), stringtab("eq"), &evaluate_eq_float);
  add_method(stringtab("float"), stringtab("ne"), &evaluate_ne_float);
  add_method(stringtab("float"), stringtab("lt"), &evaluate_lt_float);
  add_method(stringtab("float"), stringtab("le"), &evaluate_le_float);
  add_method(stringtab("float"), stringtab("gt"), &evaluate_gt_float);
  add_method(stringtab("float"), stringtab("ge"), &evaluate_ge_float);

  // boolean operations
  add_method(stringtab("Bool"), stringtab("op_and"), &evaluate_and_bool);
  add_method(stringtab("Bool"), stringtab("op_or"), &evaluate_or_bool);
  add_method(stringtab("Bool"), stringtab("op_not"), &evaluate_not_bool);

  // vector operations
  add_method(stringtab("Vector"), stringtab("_apply"), &evaluate_apply_vector);
}

void methodtab_done()
{
  method_table_destroy(&method_table);
  memset(&method_table, 0, sizeof(method_table_t));
}

static method_ptr_t lookup_method(ast_t* receiver, ast_t* type,
  const char* operation)
{
  const char* type_name =
    is_integer(type) || ast_id(receiver) == TK_INT ? "integer" :
    is_float(type) || ast_id(receiver) == TK_FLOAT ? "float" :
    ast_name(ast_childidx(type, 1));

  method_entry_t m1 = {stringtab(type_name), stringtab(operation), NULL};
  method_entry_t* m2 = method_table_get(&method_table, &m1);

  if(m2 == NULL)
    return NULL;

  return m2->method;
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

static int count = 0;
static const char* object_hygienic_name(pass_opt_t* opt, const char* type_name)
{
  // FIXME: use hygienic naming
//  const char* s = package_hygienic_id(&opt->check);
  (void) opt;
  size_t buf_size = strlen(type_name) + 21 + 2;
  char* buffer = (char*)ponyint_pool_alloc_size(buf_size);
  snprintf(buffer, buf_size, "%s_%d", type_name, count++);
  return stringtab_consume(buffer, buf_size);
}

// This is essentially the evaluate TK_FUN/TK_NEW case however, we require
// more information regarding the arguments and receiver to evaluate
// this
static ast_t* evaluate_method(pass_opt_t* opt, ast_t* function, ast_t* args,
  ast_t* this, int depth)
{
  ast_t* typeargs = NULL;
  if(ast_id(ast_childidx(function, 1)) == TK_TYPEARGS)
  {
    typeargs = ast_childidx(function, 1);
    function = ast_child(function);
  }

  AST_GET_CHILDREN(function, receiver, func_id);
  ast_t* evaluated_receiver = evaluate(opt, receiver, this, depth + 1);
  if(eval_error(evaluated_receiver))
    return evaluated_receiver;

  // First lookup to see if we have a special method to evaluate the expression
  ast_t* type = ast_get_base_type(evaluated_receiver);
  method_ptr_t builtin_method
    = lookup_method(evaluated_receiver, type, ast_name(func_id));
  if(builtin_method != NULL)
    return builtin_method(evaluated_receiver, args, opt);

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
      if(ast_visit_scope(&def, pass_pre_expr, pass_expr, opt, PASS_EXPR) != AST_OK)
        return NULL;
      break;
    }

    case TK_FUNREF:
    {
      // find the method and type check it
      //ast_t* type_def = ast_get(function, ast_name(ast_childidx(type, 1)), NULL);
      //ast_t* def = ast_get(type_def, ast_name(func_id), NULL);
      //if(ast_visit_scope(&def, pass_pre_expr, pass_expr, opt, PASS_EXPR) != AST_OK)
      //  return NULL;
      break;
    }

    case TK_BEREF:
      ast_error(opt->check.errors, function,
                "cannot evaluate compile-time behaviours");
      return NULL;

    default: break;
  }

  // lookup the reified defintion of the function
  ast_t* fun = lookup(opt, receiver, type, ast_name(func_id));
  if(fun == NULL)
    return NULL;

  if(typeargs != NULL)
  {
    ast_t* typeparams = ast_childidx(fun, 2);
    ast_t* r_fun = reify(fun, typeparams, typeargs, opt);
    ast_free_unattached(fun);
    fun = r_fun;
    assert(fun != NULL);
  }

  // map each parameter to its argument value in the symbol table
  ast_t* params = ast_childidx(fun, 3);
  ast_t* argument = ast_child(args);
  ast_t* parameter = ast_child(params);
  while(argument != NULL)
  {
    map_value(opt, parameter, argument, false);
    argument = ast_sibling(argument);
    parameter = ast_sibling(parameter);
  }

  // look up the body of the method so that we can evaluate it
  ast_t* body = ast_childidx(fun, 6);

  // push the receiver and evaluate the body
  ast_t* evaluated = evaluate(opt, body, evaluated_receiver, depth + 1);
  if(eval_error(evaluated))
  {
    if(evaluated == NULL)
      ast_error(opt->check.errors, function,
                "function is not a compile time expression");
    return evaluated;
  }

  // If the method is a constructor then we need to build a compile time object
  // adding the values of the fields as the children and setting the type
  // to be the return type of the function body
  if(ast_id(fun) == TK_NEW)
  {
    const char* type_name = ast_name(ast_childidx(type, 1));
    const char* obj_name = object_hygienic_name(opt, type_name);

    // get the return type
    ast_t* ret_type = ast_dup(ast_childidx(ast_type(function), 3));

    // TODO: move this to later so that we don't require things to be
    // val until the leave they leave the compile time expression
    // See if we can recover the constructed object to a val
    ret_type = recover_type(ret_type, TK_VAL);
    if(type == NULL)
    {
      ast_error(opt->check.errors, function,
                "can't recover compile-time object to val capability");
      return NULL;
    }

    BUILD(obj, receiver,
      NODE(TK_CONSTANT_OBJECT, ID(obj_name) NODE(TK_MEMBERS)))
    ast_set_symtab(obj, ast_get_symtab(fun));

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
          ast_error(opt->check.errors, member,
                    "compile time objects fields must be read-only");
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
    obj_cache(&obj);
    return obj;
  }

  return evaluated;
}

static ast_t* evaluate(pass_opt_t* opt, ast_t* expression, ast_t* this,
  int depth)
{
  if(depth >= opt->evaluation_depth)
  {
    ast_error(opt->check.errors, expression,
      "compile-time expression evaluation depth exceeds maximum of %d",
       opt->evaluation_depth);
    return NULL;
  }

  switch(ast_id(expression)) {
    // Literal cases where we can return the value
    case TK_NONE:
    case TK_TRUE:
    case TK_FALSE:
    case TK_INT:
    case TK_FLOAT:
    case TK_CONSTANT_OBJECT:
    case TK_ERROR:
      return expression;

    case TK_FUNREF:
    case TK_TYPEREF:
      return expression;

    // If we're evaluating a method on an object then we have a this node
    // representing the object. Otherwise we just return the current this node.
    case TK_THIS:
      return this == NULL ? expression : this;

    case TK_VARREF:
    case TK_PARAMREF:
    case TK_LETREF:
    {
      ast_t *type = ast_type(expression);
      ast_t* cap = ast_childidx(type, 3);
      if (ast_id(cap) != TK_VAL && ast_id(cap) != TK_BOX)
      {
        ast_error(opt->check.errors, expression,
                  "compile time expression can only use read-only variables");
        return NULL;
      }

      ast_t* value = ast_get_value(expression, ast_name(ast_child(expression)));
      if(eval_error(value))
      {
        if(value == NULL)
          ast_error(opt->check.errors, expression,
                    "variable is not a compile-time expression");
        return value;
      }

      return value;
    }

    case TK_FVARREF:
    case TK_EMBEDREF:
    case TK_FLETREF:
    {
      ast_t *type = ast_type(expression);
      ast_t* cap = ast_childidx(type, 3);
      if (ast_id(cap) != TK_VAL && ast_id(cap) != TK_BOX)
      {
        ast_error(opt->check.errors, expression,
                  "compile time expression can only use read-only variables");
        return NULL;
      }

      AST_GET_CHILDREN(expression, receiver, id);
      ast_t* evaluated_receiver = evaluate(opt, receiver, this, depth + 1);
      if(eval_error(evaluated_receiver))
        return evaluated_receiver;

      ast_t* field = ast_get_value(evaluated_receiver, ast_name(id));
      if(eval_error(field))
      {
        if(field == NULL)
          ast_error(opt->check.errors, expression,
                    "field is not a compile-time expression");
        return field;
      }
      return field;
    }

    case TK_SEQ:
    {
      ast_t * evaluated;
      for(ast_t* p = ast_child(expression); p != NULL; p = ast_sibling(p))
      {
        evaluated = evaluate(opt, p, this, depth + 1);
        if(eval_error(evaluated))
          return evaluated;
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
        ast_t* evaluated_argument = evaluate(opt, argument, this, depth + 1);
        if(eval_error(evaluated_argument))
          return evaluated_argument;

        ast_append(evaluated_positional_args, evaluated_argument);
        argument = ast_sibling(argument);
      }

      return evaluate_method(opt, function, evaluated_positional_args, this,
                             depth);
    }

    case TK_IF:
    case TK_ELSEIF:
    {
      AST_GET_CHILDREN(expression, condition, then_branch, else_branch);
      ast_t* condition_evaluated = evaluate(opt, condition, this, depth + 1);
      if(eval_error(condition_evaluated))
        return condition_evaluated;

      return ast_id(condition_evaluated) == TK_TRUE ?
            evaluate(opt, then_branch, this, depth + 1):
            evaluate(opt, else_branch, this, depth + 1);
    }

    case TK_ASSIGN:
    {
      AST_GET_CHILDREN(expression, right, left);

      ast_t* evaluated_right = evaluate(opt, right, this, depth + 1);
      if(eval_error(evaluated_right))
        return evaluated_right;

      ast_t* old = map_value(opt, left, evaluated_right, false);
      return old == NULL ? right: old;
    }

    case TK_CONSTANT:
      return evaluate(opt, ast_child(expression), this, depth + 1);

    case TK_VECTOR:
    {
      // get the vector type
      ast_t* type = ast_type(expression);

      // first ensure that the vector class has been type checked
      ast_t* def = ast_get(expression, stringtab("Vector"), NULL);
      if(ast_visit_scope(&def, pass_pre_expr, pass_expr, opt, PASS_EXPR) != AST_OK)
        return NULL;

      // See if we can recover the constructed vector to a val
      if(!is_type_literal(type))
      {
        type = recover_type(type, TK_VAL);
        if(type == NULL)
        {
          ast_error(opt->check.errors, expression,
            "can't recover compile-time object to val capability");
          return NULL;
        }
      }

      const char* vec_name = object_hygienic_name(opt, stringtab("Vector"));
      BUILD(obj, expression,
        NODE(TK_CONSTANT_OBJECT, ID(vec_name) NODE(TK_MEMBERS)))
      ast_settype(obj, type);

      ast_t* obj_members = ast_childidx(obj, 1);
      ast_t* elem = ast_childidx(expression, 1);
      while(elem != NULL)
      {
        ast_t* evaluated_elem = evaluate(opt, elem, this, depth + 1);
        if(eval_error(evaluated_elem))
          return evaluated_elem;

        ast_append(obj_members, evaluated_elem);
        elem = ast_sibling(elem);
      }
      obj_cache(&obj);
      return obj;
    }

    case TK_VALUEFORMALPARAMREF:
      return expression;

    case TK_WHILE:
    {
      AST_GET_CHILDREN(expression, cond, thenbody, elsebody);
      //FIXME: currently not supporting the else branch
      ast_t* evaluated_cond = evaluate(opt, cond, this, depth + 1);
      if(eval_error(evaluated_cond))
        return evaluated_cond;

      assert(ast_id(evaluated_cond) == TK_TRUE ||
             ast_id(evaluated_cond) == TK_FALSE);

      while(ast_id(evaluated_cond) == TK_TRUE)
      {
        ast_t* evaluated_then = evaluate(opt, thenbody, this, depth + 1);
        if(eval_error(evaluated_then))
          return evaluated_then;

        evaluated_cond = evaluate(opt, cond, this, depth + 1);
        if(eval_error(evaluated_cond))
          return evaluated_cond;
      }

      //FIXME: return the correct value
      return evaluated_cond;
    }

    case TK_TRY:
    {
      AST_GET_CHILDREN(expression, trybody, elsebody);
      ast_t* evaluated_try = evaluate(opt, trybody, this, depth + 1);
      if(eval_error(evaluated_try))
      {
        if(evaluated_try == NULL || ast_id(evaluated_try) != TK_ERROR)
          return evaluated_try;

        return evaluate(opt, elsebody, this, depth + 1);
      }
      return evaluated_try;
    }

    default:
      assert(0);
      ast_error(opt->check.errors, expression,
        "cannot evaluate compile time expression");
      return NULL;
  }
  return NULL;
}
