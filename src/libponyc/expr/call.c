#include "call.h"
#include "postfix.h"
#include "control.h"
#include "literal.h"
#include "reference.h"
#include "../ast/astbuild.h"
#include "../pkg/package.h"
#include "../pass/expr.h"
#include "../pass/sugar.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/reify.h"
#include "../type/safeto.h"
#include "../type/sanitise.h"
#include "../type/subtype.h"
#include "../type/viewpoint.h"
#include <assert.h>

static bool insert_apply(pass_opt_t* opt, ast_t** astp)
{
  // Sugar .apply()
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  ast_t* dot = ast_from(ast, TK_DOT);
  ast_add(dot, ast_from_string(ast, "apply"));
  ast_swap(lhs, dot);
  ast_add(dot, lhs);

  if(!expr_dot(opt, &dot))
    return false;

  return expr_call(opt, astp);
}

bool is_this_incomplete(typecheck_t* t, ast_t* ast)
{
  // If we're in a default argument, we're incomplete by definition.
  if(t->frame->method == NULL)
    return true;

  // If we're not in a constructor, we're complete by definition.
  if(ast_id(t->frame->method) != TK_NEW)
    return false;

  // Check if all fields have been marked as defined.
  ast_t* members = ast_childidx(t->frame->type, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FLET:
      case TK_FVAR:
      case TK_EMBED:
      {
        sym_status_t status;
        ast_t* id = ast_child(member);
        ast_get(ast, ast_name(id), &status);

        if(status != SYM_DEFINED)
          return true;

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return false;
}

static bool check_type_params(ast_t** astp)
{
  ast_t* lhs = *astp;
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  ast_t* typeparams = ast_childidx(type, 1);
  assert(ast_id(type) == TK_FUNTYPE);

  if(ast_id(typeparams) == TK_NONE)
    return true;

  BUILD(typeargs, typeparams, NODE(TK_TYPEARGS));

  if(!reify_defaults(typeparams, typeargs, true))
  {
    ast_free_unattached(typeargs);
    return false;
  }

  if(!check_constraints(lhs, typeparams, typeargs, true))
  {
    ast_free_unattached(typeargs);
    return false;
  }

  type = reify(type, typeparams, typeargs);
  typeparams = ast_childidx(type, 1);
  ast_replace(&typeparams, ast_from(typeparams, TK_NONE));

  REPLACE(astp, NODE(ast_id(lhs), TREE(lhs) TREE(typeargs)));
  ast_settype(*astp, type);

  return true;
}

static bool extend_positional_args(ast_t* params, ast_t* positional)
{
  // Fill out the positional args to be as long as the param list.
  size_t param_len = ast_childcount(params);
  size_t arg_len = ast_childcount(positional);

  if(arg_len > param_len)
  {
    ast_error(positional, "too many arguments");
    return false;
  }

  while(arg_len < param_len)
  {
    ast_setid(positional, TK_POSITIONALARGS);
    ast_append(positional, ast_from(positional, TK_NONE));
    arg_len++;
  }

  return true;
}

static bool apply_named_args(ast_t* params, ast_t* positional,
  ast_t* namedargs)
{
  ast_t* namedarg = ast_pop(namedargs);

  while(namedarg != NULL)
  {
    AST_GET_CHILDREN(namedarg, arg_id, arg);

    ast_t* param = ast_child(params);
    size_t param_index = 0;

    while(param != NULL)
    {
      AST_GET_CHILDREN(param, param_id);

      if(ast_name(arg_id) == ast_name(param_id))
        break;

      param = ast_sibling(param);
      param_index++;
    }

    if(param == NULL)
    {
      if(ast_id(namedarg) == TK_UPDATEARG)
      {
        ast_error(arg_id,
          "cannot use sugar, update() has no parameter named \"value\"");
        return false;
      }

      ast_error(arg_id, "not a parameter name");
      return false;
    }

    ast_t* arg_replace = ast_childidx(positional, param_index);

    if(ast_id(arg_replace) != TK_NONE)
    {
      ast_error(arg_id, "named argument is already supplied");
      ast_error(arg_replace, "supplied argument is here");
      return false;
    }

    // Extract named argument expression to avoid copying it
    ast_free(ast_pop(namedarg));  // ID
    arg = ast_pop(namedarg);  // Expression

    ast_replace(&arg_replace, arg);
    namedarg = ast_pop(namedargs);
  }

  ast_setid(namedargs, TK_NONE);
  return true;
}

static bool apply_default_arg(pass_opt_t* opt, ast_t* param, ast_t* arg)
{
  // Pick up a default argument.
  ast_t* def_arg = ast_childidx(param, 2);

  if(ast_id(def_arg) == TK_NONE)
  {
    ast_error(arg, "not enough arguments");
    return false;
  }

  if(ast_id(def_arg) == TK_LOCATION)
  {
    // Default argument is __loc. Expand call location.
    ast_t* location = expand_location(arg);
    ast_add(arg, location);

    if(!ast_passes_subtree(&location, opt, PASS_EXPR))
      return false;
  }
  else
  {
    // Just use default argument.
    ast_add(arg, def_arg);
  }

  ast_setid(arg, TK_SEQ);

  if(!expr_seq(opt, arg))
    return false;

  return true;
}

static bool check_arg_types(pass_opt_t* opt, ast_t* params, ast_t* positional,
  bool incomplete, bool partial)
{
  // Check positional args vs params.
  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positional);

  while(arg != NULL)
  {
    if(ast_id(arg) == TK_NONE)
    {
      if(partial)
      {
        // Don't check missing arguments for partial application.
        arg = ast_sibling(arg);
        param = ast_sibling(param);
        continue;
      } else {
        // Pick up a default argument if we can.
        if(!apply_default_arg(opt, param, arg))
          return false;
      }
    }

    ast_t* p_type = ast_childidx(param, 1);

    if(!coerce_literals(&arg, p_type, opt))
      return false;

    ast_t* arg_type = ast_type(arg);

    if(is_typecheck_error(arg_type))
      return false;

    if(is_control_type(arg_type))
    {
      ast_error(arg, "can't use a control expression in an argument");
      return false;
    }

    ast_t* a_type = alias(arg_type);

    if(incomplete)
    {
      ast_t* expr = arg;

      if(ast_id(arg) == TK_SEQ)
        expr = ast_child(arg);

      // If 'this' is incomplete and the arg is 'this', change the type to tag.
      if((ast_id(expr) == TK_THIS) && (ast_sibling(expr) == NULL))
      {
        ast_t* tag_type = set_cap_and_ephemeral(a_type, TK_TAG, TK_NONE);
        ast_free_unattached(a_type);
        a_type = tag_type;
      }
    }

    errorframe_t info = NULL;
    if(!is_subtype(a_type, p_type, &info))
    {
      errorframe_t frame = NULL;
      ast_error_frame(&frame, arg, "argument not a subtype of parameter");
      ast_error_frame(&frame, param, "parameter type: %s",
        ast_print_type(p_type));
      ast_error_frame(&frame, arg, "argument type: %s", ast_print_type(a_type));
      errorframe_append(&frame, &info);
      errorframe_report(&frame);

      ast_free_unattached(a_type);
      return false;
    }

    ast_free_unattached(a_type);
    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  return true;
}

static bool auto_recover_call(ast_t* ast, ast_t* receiver_type,
  ast_t* positional, ast_t* result)
{
  // We can recover the receiver (ie not alias the receiver type) if all
  // arguments are safe and the result is either safe or unused.
  if(is_result_needed(ast) && !safe_to_autorecover(receiver_type, result))
    return false;

  ast_t* arg = ast_child(positional);

  while(arg != NULL)
  {
    if(ast_id(arg) != TK_NONE)
    {
      ast_t* arg_type = ast_type(arg);

      if(is_typecheck_error(arg_type))
        return false;

      ast_t* a_type = alias(arg_type);
      bool ok = safe_to_autorecover(receiver_type, a_type);
      ast_free_unattached(a_type);

      if(!ok)
        return false;
    }

    arg = ast_sibling(arg);
  }

  return true;
}

static bool check_receiver_cap(ast_t* ast, bool incomplete)
{
  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, typeparams, params, result);

  // Check receiver cap.
  ast_t* receiver = ast_child(lhs);

  // Dig through function qualification.
  if(ast_id(receiver) == TK_FUNREF || ast_id(receiver) == TK_FUNAPP)
    receiver = ast_child(receiver);

  // Receiver type, alias of receiver type, and target type.
  ast_t* r_type = ast_type(receiver);

  if(is_typecheck_error(r_type))
    return false;

  ast_t* t_type = set_cap_and_ephemeral(r_type, ast_id(cap), TK_NONE);
  ast_t* a_type;

  // If we can recover the receiver, we don't alias it here.
  bool can_recover = auto_recover_call(ast, r_type, positional, result);
  bool cap_recover = false;

  switch(ast_id(cap))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_VAL:
    case TK_TAG:
      break;

    case TK_REF:
    case TK_BOX:
      cap_recover = true;
      break;

    default:
      assert(0);
  }

  if(can_recover && cap_recover)
    a_type = r_type;
  else
    a_type = alias(r_type);

  if(incomplete && (ast_id(receiver) == TK_THIS))
  {
    // If 'this' is incomplete and the arg is 'this', change the type to tag.
    ast_t* tag_type = set_cap_and_ephemeral(a_type, TK_TAG, TK_NONE);

    if(a_type != r_type)
      ast_free_unattached(a_type);

    a_type = tag_type;
  } else {
    incomplete = false;
  }

  errorframe_t info = NULL;
  bool ok = is_subtype(a_type, t_type, &info);

  if(!ok)
  {
    errorframe_t frame = NULL;

    ast_error_frame(&frame, ast,
      "receiver type is not a subtype of target type");
    ast_error_frame(&frame, receiver,
      "receiver type: %s", ast_print_type(a_type));
    ast_error_frame(&frame, cap,
      "target type: %s", ast_print_type(t_type));

    if(!can_recover && cap_recover && is_subtype(r_type, t_type, NULL))
    {
      ast_error_frame(&frame, ast,
        "this would be possible if the arguments and return value "
        "were all sendable");
    }

    if(incomplete && is_subtype(r_type, t_type, NULL))
    {
      ast_error_frame(&frame, ast,
        "this would be possible if all the fields of 'this' were assigned to "
        "at this point");
    }

    errorframe_append(&frame, &info);
    errorframe_report(&frame);
  }

  if(a_type != r_type)
    ast_free_unattached(a_type);

  ast_free_unattached(r_type);
  ast_free_unattached(t_type);
  return ok;
}

static bool method_application(pass_opt_t* opt, ast_t* ast, bool partial)
{
  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  if(!check_type_params(&lhs))
    return false;

  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, typeparams, params, result);

  if(!extend_positional_args(params, positional))
    return false;

  if(!apply_named_args(params, positional, namedargs))
    return false;

  bool incomplete = is_this_incomplete(&opt->check, ast);

  if(!check_arg_types(opt, params, positional, incomplete, partial))
    return false;

  switch(ast_id(lhs))
  {
    case TK_FUNREF:
    case TK_FUNAPP:
      if(!check_receiver_cap(ast, incomplete))
        return false;
      break;

    default: {}
  }

  return true;
}

static bool method_call(pass_opt_t* opt, ast_t* ast)
{
  if(!method_application(opt, ast, false))
    return false;

  AST_GET_CHILDREN(ast, positional, namedargs, lhs);
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  AST_GET_CHILDREN(type, cap, typeparams, params, result);
  ast_settype(ast, result);
  ast_inheritflags(ast);

  switch(ast_id(lhs))
  {
    case TK_NEWBEREF:
    case TK_BEREF:
      ast_setsend(ast);
      return true;

    case TK_NEWREF:
    case TK_FUNREF:
      ast_setmightsend(ast);
      return true;

    default: {}
  }

  assert(0);
  return false;
}

static token_id partial_application_cap(ast_t* ftype, ast_t* receiver,
  ast_t* positional)
{
  // Check if the apply method in the generated object literal can accept a box
  // receiver. If not, it must be a ref receiver. It can accept a box receiver
  // if box->receiver <: lhs->receiver and box->arg <: lhs->param.
  AST_GET_CHILDREN(ftype, cap, typeparams, params, result);

  ast_t* type = ast_type(receiver);
  ast_t* view_type = viewpoint_type(ast_from(type, TK_BOX), type);
  ast_t* need_type = set_cap_and_ephemeral(type, ast_id(cap), TK_NONE);

  bool ok = is_subtype(view_type, need_type, NULL);
  ast_free_unattached(view_type);
  ast_free_unattached(need_type);

  if(!ok)
    return TK_REF;

  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positional);

  while(arg != NULL)
  {
    if(ast_id(arg) != TK_NONE)
    {
      type = ast_type(arg);
      view_type = viewpoint_type(ast_from(type, TK_BOX), type);
      need_type = ast_childidx(param, 1);

      ok = is_subtype(view_type, need_type, NULL);
      ast_free_unattached(view_type);
      ast_free_unattached(need_type);

      if(!ok)
        return TK_REF;
    }

    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  return TK_BOX;
}

// Sugar for partial application, which we convert to a lambda.
static bool partial_application(pass_opt_t* opt, ast_t** astp)
{
  /* Example that we refer to throughout this function.
   * ```pony
   * class C
   *   fun f[T](a: A, b: B = b_default): R
   *
   * let recv: T = ...
   * recv~f[T2](foo)
   * ```
   *
   * Partial call is converted to:
   * ```pony
   * lambda(b: B = b_default)($0 = recv, a = foo): R => $0.f[T2](a, consume b)
   * ```
   */

  ast_t* ast = *astp;
  typecheck_t* t = &opt->check;

  if(!method_application(opt, ast, true))
    return false;

  AST_GET_CHILDREN(ast, positional, namedargs, lhs);
  assert(ast_id(lhs) == TK_FUNAPP || ast_id(lhs) == TK_BEAPP ||
    ast_id(lhs) == TK_NEWAPP);

  // LHS must be a TK_TILDE, possibly contained in a TK_QUALIFY.
  AST_GET_CHILDREN(lhs, receiver, method);
  ast_t* type_args = NULL;

  switch(ast_id(receiver))
  {
    case TK_NEWAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
      type_args = method;
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  // The TK_FUNTYPE of the LHS.
  ast_t* type = ast_type(lhs);

  if(is_typecheck_error(type))
    return false;

  token_id apply_cap = partial_application_cap(type, receiver, positional);
  AST_GET_CHILDREN(type, cap, type_params, target_params, result);

  token_id can_error = ast_canerror(lhs) ? TK_QUESTION : TK_NONE;
  const char* recv_name = package_hygienic_id(t);

  // Build captures. We always have at least one capture, for receiver.
  // Capture: `$0 = recv`
  BUILD(captures, receiver,
    NODE(TK_LAMBDACAPTURES,
      NODE(TK_LAMBDACAPTURE,
        ID(recv_name)
        NONE  // Infer type.
        TREE(receiver))));

  // Process arguments.
  ast_t* given_arg = ast_child(positional);
  ast_t* target_param = ast_child(target_params);
  ast_t* lambda_params = ast_from(target_params, TK_NONE);
  ast_t* lambda_call_args = ast_from(positional, TK_NONE);

  while(given_arg != NULL)
  {
    assert(target_param != NULL);
    const char* target_p_name = ast_name(ast_child(target_param));

    if(ast_id(given_arg) == TK_NONE)
    {
      // This argument is not supplied already, must be a lambda parameter.
      // Like `b` in example above.
      // Build a new a new TK_PARAM node rather than copying the target one,
      // since the target has already been processed to expr pass, and we need
      // a clean one.
      AST_GET_CHILDREN(target_param, p_id, p_type, p_default);

      // Parameter: `b: B = b_default`
      BUILD(lambda_param, target_param,
        NODE(TK_PARAM,
          TREE(p_id)
          TREE(sanitise_type(p_type))
          TREE(p_default)));

      ast_append(lambda_params, lambda_param);
      ast_setid(lambda_params, TK_PARAMS);

      // Argument: `consume b`
      BUILD(target_arg, lambda_param,
        NODE(TK_SEQ,
          NODE(TK_CONSUME,
            NONE
            NODE(TK_REFERENCE, ID(target_p_name)))));

      ast_append(lambda_call_args, target_arg);
      ast_setid(lambda_call_args, TK_POSITIONALARGS);
    }
    else
    {
      // This argument is supplied to the partial, capture it.
      // Like `a` in example above.
      // Capture: `a = foo`
      BUILD(capture, given_arg,
        NODE(TK_LAMBDACAPTURE,
          ID(target_p_name)
          NONE
          TREE(given_arg)));

      ast_append(captures, capture);

      // Argument: `a`
      BUILD(target_arg, given_arg,
        NODE(TK_SEQ,
          NODE(TK_REFERENCE, ID(target_p_name))));

      ast_append(lambda_call_args, target_arg);
      ast_setid(lambda_call_args, TK_POSITIONALARGS);
    }

    given_arg = ast_sibling(given_arg);
    target_param = ast_sibling(target_param);
  }

  assert(target_param == NULL);

  // Build lambda expression.
  // `$0.f`
  BUILD(call_receiver, ast,
    NODE(TK_DOT,
      NODE(TK_REFERENCE, ID(recv_name))
      TREE(method)));

  if(type_args != NULL)
  {
    // The partial call has type args, add them to the actual call in apply().
    // `$0.f[T2]`
    BUILD(qualified, type_args,
      NODE(TK_QUALIFY,
        TREE(call_receiver)
        TREE(type_args)));
    call_receiver = qualified;
  }

  REPLACE(astp,
    NODE(TK_LAMBDA,
      NODE(apply_cap)
      NONE  // Lambda function name.
      NONE  // Lambda type params.
      TREE(lambda_params)
      TREE(captures)
      TREE(sanitise_type(result))
      NODE(can_error)
      NODE(TK_SEQ,
        NODE(TK_CALL,
          TREE(lambda_call_args)
          NONE  // Named args.
          TREE(call_receiver)))));

  // Need to preserve various lambda children.
  ast_setflag(ast_childidx(*astp, 2), AST_FLAG_PRESERVE); // Type params.
  ast_setflag(ast_childidx(*astp, 3), AST_FLAG_PRESERVE); // Parameters.
  ast_setflag(ast_childidx(*astp, 5), AST_FLAG_PRESERVE); // Return type.
  ast_setflag(ast_childidx(*astp, 7), AST_FLAG_PRESERVE); // Body.

  // Catch up to this pass.
  return ast_passes_subtree(astp, opt, PASS_EXPR);
}

bool expr_call(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  if(!literal_call(ast, opt))
    return false;

  // Type already set by literal handler. Check for infertype, which is a
  // marker for typechecking default arguments.
  ast_t* type = ast_type(ast);

  if((type != NULL) && (ast_id(type) != TK_INFERTYPE))
    return true;

  AST_GET_CHILDREN(ast, positional, namedargs, lhs);

  switch(ast_id(lhs))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
      return method_call(opt, ast);

    case TK_NEWAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
      return partial_application(opt, astp);

    default: {}
  }

  return insert_apply(opt, astp);
}
